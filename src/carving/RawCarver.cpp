#include "carving/RawCarver.hpp"
#include "utils/FileUtil.hpp"
#include "utils/Logger.hpp"
#include "utils/StringUtil.hpp"

#include <algorithm>
#include <cstring>

namespace pcdr {

RawCarver::RawCarver(SignatureDatabase db) : db_(std::move(db)) {}

bool RawCarver::MatchAt(const std::uint8_t* data, std::size_t size, const FileSignature& sig) {
    if (sig.header.empty()) return false;
    if (sig.headerOffset + sig.header.size() > size) return false;
    return std::memcmp(data + sig.headerOffset, sig.header.data(), sig.header.size()) == 0;
}

std::uint64_t RawCarver::EstimateLength(const std::uint8_t* data, std::size_t size,
                                        const FileSignature& sig, std::uint64_t maxSize) {
    const std::uint64_t limit = static_cast<std::uint64_t>(std::min<std::size_t>(size, static_cast<std::size_t>(maxSize)));

    if (sig.extension == L"bmp" && size >= 6) {
        const std::uint32_t bfSize = *reinterpret_cast<const std::uint32_t*>(data + 2);
        if (bfSize >= 54 && bfSize <= maxSize) return bfSize;
    }

    if ((sig.extension == L"avi" || sig.extension == L"wav") && size >= 12) {
        if (std::memcmp(data, "RIFF", 4) == 0) {
            const std::uint32_t chunk = *reinterpret_cast<const std::uint32_t*>(data + 4);
            const std::uint64_t total = static_cast<std::uint64_t>(chunk) + 8;
            if (total >= 12 && total <= maxSize) return total;
        }
    }

    if (!sig.footer.empty()) {
        // Cap footer search aggressively — byte scans are expensive.
        const std::size_t searchLimit = static_cast<std::size_t>(std::min(limit, 4ULL << 20));
        const std::size_t step = (sig.footer.size() >= 4) ? 1 : 1;
        for (std::size_t i = sig.header.size(); i + sig.footer.size() <= searchLimit; i += step) {
            if (std::memcmp(data + i, sig.footer.data(), sig.footer.size()) == 0) {
                return static_cast<std::uint64_t>(i + sig.footer.size());
            }
        }
    }

    if (sig.maxCarveSize) return std::min(*sig.maxCarveSize, std::min(limit, 8ULL << 20));
    return std::min<std::uint64_t>(limit, 1ULL << 20);
}

bool RawCarver::Carve(VolumeReader& reader, const DriveInfo& drive, const ScanOptions& options,
                      ScanProgress& progress, const FileFoundCallback& onFile) {
    auto signatures = db_.FilterByExtensions(options.enabledExtensions);
    signatures.erase(std::remove_if(signatures.begin(), signatures.end(),
                                    [](const FileSignature& s) { return s.header.empty(); }),
                     signatures.end());

    if (signatures.empty()) {
        progress.SetStatus(L"No file signatures enabled for RAW recovery");
        return false;
    }

    const std::uint64_t volumeSize = reader.Size() ? reader.Size() : drive.totalBytes;
    const std::uint32_t chunkSize = options.carveChunkSize ? options.carveChunkSize : (4u << 20);
    const std::uint32_t overlap = 64 * 1024;
    // File headers are almost always sector-aligned; scanning every byte freezes the UI.
    constexpr std::size_t kAlign = 512;

    // Preserve elapsed/files if continuing a Deep/other phase.
    if (progress.GetState() == ScanState::Running || progress.GetState() == ScanState::Paused) {
        progress.BeginPhase(volumeSize ? volumeSize : 1, L"RAW carving for known file signatures (read-only)...");
    } else {
        progress.Reset(volumeSize ? volumeSize : 1, L"RAW carving for known file signatures (read-only)...");
    }
    progress.SetState(ScanState::Running);

    std::vector<std::uint8_t> buffer;
    std::uint64_t offset = 0;
    std::uint64_t fileCounter = 0;
    std::vector<std::uint8_t> prevTail;

    while (offset < volumeSize) {
        progress.WaitWhilePaused();
        if (progress.ShouldStop()) break;

        const std::size_t lead = prevTail.size();
        const std::uint64_t remaining = volumeSize - offset;
        const std::size_t toRead = static_cast<std::size_t>(std::min<std::uint64_t>(chunkSize, remaining));

        std::vector<std::uint8_t> chunk;
        if (!reader.Read(offset, chunk, toRead)) {
            offset += toRead;
            progress.SetBytesScanned(offset);
            prevTail.clear();
            continue;
        }

        buffer.resize(lead + chunk.size());
        if (lead) std::memcpy(buffer.data(), prevTail.data(), lead);
        std::memcpy(buffer.data() + lead, chunk.data(), chunk.size());

        // Sector-aligned signature scan
        for (std::size_t i = 0; i + 8 < buffer.size(); i += kAlign) {
            for (const auto& sig : signatures) {
                // Skip signatures that require non-aligned offsets unless within sector
                if (sig.headerOffset != 0 && (i % kAlign) != 0) continue;
                if (sig.headerOffset > 0) {
                    // e.g. ISO at 0x8001 — check absolute positions separately below
                    continue;
                }
                if (!MatchAt(buffer.data() + i, buffer.size() - i, sig)) continue;

                if ((sig.extension == L"mp4" || sig.extension == L"mov") && buffer.size() >= i + 8) {
                    if (std::memcmp(buffer.data() + i + 4, "ftyp", 4) != 0) continue;
                }
                if (sig.extension == L"avi" && buffer.size() >= i + 12) {
                    if (std::memcmp(buffer.data() + i + 8, "AVI ", 4) != 0) continue;
                }
                if (sig.extension == L"wav" && buffer.size() >= i + 12) {
                    if (std::memcmp(buffer.data() + i + 8, "WAVE", 4) != 0) continue;
                }
                if ((sig.extension == L"docx" || sig.extension == L"xlsx" || sig.extension == L"pptx")) continue;
                if ((sig.extension == L"xls" || sig.extension == L"ppt")) continue;

                const std::uint64_t maxSize = std::min(options.maxFileSize,
                    sig.maxCarveSize.value_or(options.maxFileSize));
                const std::uint64_t length = EstimateLength(buffer.data() + i, buffer.size() - i, sig, maxSize);
                if (length == 0) continue;

                const std::uint64_t absolute = offset - lead + i;
                ++fileCounter;

                RecoveredFile file;
                file.displayName = L"Carved_" + std::to_wstring(fileCounter) + L"." + sig.extension;
                file.extension = sig.extension;
                file.originalPath = drive.letter + L"\\[RAW]\\" + file.displayName;
                file.sizeBytes = length;
                file.dataLength = length;
                file.method = DiscoveryMethod::RawCarve;
                file.sourceFs = drive.fileSystem;
                file.sourceDrive = drive.letter;
                file.typeLabel = sig.typeLabel;
                file.previewable = sig.previewable;
                file.startOffset = absolute;
                file.runs.emplace_back(absolute, length);
                file.confidence = sig.footer.empty() ? Confidence::Partial : Confidence::Good;
                if (length < 64) file.confidence = Confidence::Corrupted;

                onFile(file);
                progress.IncrementFiles();

                const std::size_t skip = static_cast<std::size_t>(std::min(length, static_cast<std::uint64_t>(kAlign * 8)));
                i += (skip / kAlign) * kAlign;
                if (i > 0) i -= kAlign; // loop will add kAlign
                break;
            }
        }

        // ISO primary volume descriptor sits at offset 0x8001 within some images;
        // check once per chunk at absolute-aligned positions.
        for (const auto& sig : signatures) {
            if (sig.headerOffset == 0) continue;
            if (sig.headerOffset >= buffer.size()) continue;
            // Only check when absolute chunk may contain the offset pattern near start of ISO sessions
            for (std::size_t base = 0; base + sig.headerOffset + sig.header.size() < buffer.size(); base += 0x8000) {
                if (MatchAt(buffer.data() + base, buffer.size() - base, sig)) {
                    ++fileCounter;
                    RecoveredFile file;
                    file.displayName = L"Carved_" + std::to_wstring(fileCounter) + L"." + sig.extension;
                    file.extension = sig.extension;
                    file.originalPath = drive.letter + L"\\[RAW]\\" + file.displayName;
                    file.sizeBytes = sig.maxCarveSize.value_or(8ULL << 20);
                    file.dataLength = file.sizeBytes;
                    file.method = DiscoveryMethod::RawCarve;
                    file.sourceFs = drive.fileSystem;
                    file.sourceDrive = drive.letter;
                    file.typeLabel = sig.typeLabel;
                    file.startOffset = offset - lead + base;
                    file.runs.emplace_back(file.startOffset, file.sizeBytes);
                    file.confidence = Confidence::Partial;
                    onFile(file);
                    progress.IncrementFiles();
                    break;
                }
            }
        }

        if (buffer.size() > overlap) {
            prevTail.assign(buffer.end() - overlap, buffer.end());
        } else {
            prevTail = buffer;
        }

        offset += toRead;
        progress.SetBytesScanned(offset);
    }

    progress.SetStatus(L"RAW carving finished");
    return true;
}

} // namespace pcdr
