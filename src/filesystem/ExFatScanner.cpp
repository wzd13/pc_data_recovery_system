#include "filesystem/ExFatScanner.hpp"
#include "utils/FileUtil.hpp"
#include "utils/Logger.hpp"
#include "utils/StringUtil.hpp"

#include <cstring>
#include <set>

namespace pcdr {
namespace {

bool ExtensionAllowed(const ScanOptions& options, const std::wstring& ext) {
    if (options.enabledExtensions.empty()) return true;
    for (const auto& e : options.enabledExtensions) {
        if (str::EqualsIgnoreCase(e, ext)) return true;
    }
    return false;
}

} // namespace

bool ExFatScanner::ParseBoot(VolumeReader& reader, ExFatBoot& boot) const {
    std::vector<std::uint8_t> sector;
    if (!reader.Read(0, sector, 512)) return false;
    if (std::memcmp(sector.data() + 3, "EXFAT   ", 8) != 0) {
        Logger::Instance().Warning(L"OEM ID is not exFAT");
    }

    boot.volumeLength = *reinterpret_cast<const std::uint64_t*>(sector.data() + 72);
    boot.fatOffsetSectors = *reinterpret_cast<const std::uint32_t*>(sector.data() + 80);
    boot.fatLengthSectors = *reinterpret_cast<const std::uint32_t*>(sector.data() + 84);
    boot.clusterHeapOffset = *reinterpret_cast<const std::uint32_t*>(sector.data() + 88);
    boot.clusterCount = *reinterpret_cast<const std::uint32_t*>(sector.data() + 92);
    boot.rootCluster = *reinterpret_cast<const std::uint32_t*>(sector.data() + 96);
    const std::uint8_t bytesPerSectorShift = sector[108];
    boot.sectorsPerClusterShift = sector[109];
    boot.bytesPerSector = static_cast<std::uint16_t>(1u << bytesPerSectorShift);
    boot.clusterSize = boot.bytesPerSector * (1u << boot.sectorsPerClusterShift);
    return boot.rootCluster >= 2 && boot.clusterSize > 0;
}

bool ExFatScanner::ScanRoot(VolumeReader& reader, const DriveInfo& drive, const ExFatBoot& boot,
                            const ScanOptions& options, ScanProgress& progress,
                            const FileFoundCallback& onFile, bool deep) {
    const std::uint64_t heapOffset =
        static_cast<std::uint64_t>(boot.clusterHeapOffset) * boot.bytesPerSector;
    progress.Reset(static_cast<std::uint64_t>(boot.clusterCount) * boot.clusterSize,
                   L"Scanning exFAT directory entries (read-only)...");
    progress.SetState(ScanState::Running);

    std::vector<std::uint32_t> queue = {boot.rootCluster};
    std::set<std::uint32_t> visited;
    std::uint64_t bytesDone = 0;

    while (!queue.empty()) {
        progress.WaitWhilePaused();
        if (progress.ShouldStop()) break;

        const std::uint32_t cluster = queue.back();
        queue.pop_back();
        if (!visited.insert(cluster).second) continue;
        if (cluster < 2) continue;

        const std::uint64_t offset = heapOffset + static_cast<std::uint64_t>(cluster - 2) * boot.clusterSize;
        std::vector<std::uint8_t> data;
        if (!reader.Read(offset, data, boot.clusterSize)) continue;
        bytesDone += boot.clusterSize;
        progress.SetBytesScanned(bytesDone);

        // Minimal exFAT entry walk: File Directory Entry (0x85) + Stream (0xC0) + File Name (0xC1)
        for (std::size_t i = 0; i + 32 <= data.size(); i += 32) {
            const auto type = data[i];
            if (type == 0x00) break;
            const bool inUse = (type & 0x80) != 0;
            const std::uint8_t entryType = type & 0x7F;

            if (entryType == 0x05 && i + 96 <= data.size()) { // File entry family
                // Secondary count
                const std::uint8_t secondary = data[i + 1];
                if (i + 32u * (1 + secondary) > data.size()) continue;

                const auto* stream = data.data() + i + 32;
                if ((stream[0] & 0x7F) != 0x40) continue;

                const bool deleted = !inUse;
                if (!deleted && !deep) continue;

                const std::uint64_t validDataLength = *reinterpret_cast<const std::uint64_t*>(stream + 8);
                const std::uint32_t firstCluster = *reinterpret_cast<const std::uint32_t*>(stream + 20);
                const std::uint8_t nameLen = stream[3];

                std::wstring name;
                std::size_t namePos = i + 64;
                while (name.size() < nameLen && namePos + 32 <= i + 32u * (1 + secondary) && namePos + 32 <= data.size()) {
                    if ((data[namePos] & 0x7F) != 0x41) break;
                    for (int c = 0; c < 15 && name.size() < nameLen; ++c) {
                        const wchar_t ch = *reinterpret_cast<const wchar_t*>(data.data() + namePos + 2 + c * 2);
                        if (ch == 0) break;
                        name.push_back(ch);
                    }
                    namePos += 32;
                }

                if (name.empty() || validDataLength == 0 || validDataLength > options.maxFileSize) continue;
                auto ext = fileutil::GetExtension(name);
                if (!ExtensionAllowed(options, ext)) continue;

                // Directory flag in file attributes at offset 4 of file entry
                const std::uint16_t attrs = *reinterpret_cast<const std::uint16_t*>(data.data() + i + 4);
                if (attrs & 0x10) {
                    if (firstCluster >= 2) queue.push_back(firstCluster);
                    continue;
                }

                RecoveredFile file;
                file.displayName = name;
                file.extension = ext;
                file.originalPath = drive.letter + L"\\[Deleted]\\" + name;
                file.sizeBytes = validDataLength;
                file.dataLength = validDataLength;
                file.method = DiscoveryMethod::FileSystem;
                file.sourceFs = FileSystemType::ExFat;
                file.sourceDrive = drive.letter;
                file.typeLabel = ext.empty() ? L"File" : ext;
                file.confidence = deleted ? Confidence::Good : Confidence::Partial;
                file.previewable = (ext == L"jpg" || ext == L"png" || ext == L"gif" || ext == L"bmp" ||
                                    ext == L"txt" || ext == L"csv" || ext == L"pdf");

                if (firstCluster >= 2) {
                    // Contiguous allocation is common on exFAT; without full FAT walk mark confidence accordingly.
                    const std::uint64_t dataOff = heapOffset + static_cast<std::uint64_t>(firstCluster - 2) * boot.clusterSize;
                    file.runs.emplace_back(dataOff, validDataLength);
                    file.startOffset = dataOff;
                    // Without verifying FAT chain continuity, avoid claiming Excellent.
                    if (deleted) file.confidence = Confidence::Good;
                    else file.confidence = Confidence::Partial;
                } else {
                    file.status = RecoveryStatus::Unsupported;
                    file.confidence = Confidence::Corrupted;
                }

                onFile(file);
                progress.IncrementFiles();
            }
        }
    }

    progress.SetStatus(L"exFAT directory scan finished");
    return true;
}

bool ExFatScanner::QuickScan(VolumeReader& reader, const DriveInfo& drive, const ScanOptions& options,
                             ScanProgress& progress, const FileFoundCallback& onFile) {
    ExFatBoot boot;
    if (!ParseBoot(reader, boot)) {
        progress.SetStatus(L"Unsupported or unreadable exFAT boot sector");
        return false;
    }
    return ScanRoot(reader, drive, boot, options, progress, onFile, false);
}

bool ExFatScanner::DeepScan(VolumeReader& reader, const DriveInfo& drive, const ScanOptions& options,
                            ScanProgress& progress, const FileFoundCallback& onFile) {
    ExFatBoot boot;
    if (!ParseBoot(reader, boot)) {
        progress.SetStatus(L"Unsupported or unreadable exFAT boot sector");
        return false;
    }
    return ScanRoot(reader, drive, boot, options, progress, onFile, true);
}

} // namespace pcdr
