#include "filesystem/Fat32Scanner.hpp"
#include "utils/FileUtil.hpp"
#include "utils/Logger.hpp"
#include "utils/StringUtil.hpp"
#include "utils/TimeUtil.hpp"

#include <cstring>
#include <cctype>
#include <set>

namespace pcdr {
namespace {

#pragma pack(push, 1)
struct Fat32BootRaw {
    std::uint8_t jump[3];
    char oem[8];
    std::uint16_t bytesPerSector;
    std::uint8_t sectorsPerCluster;
    std::uint16_t reservedSectors;
    std::uint8_t numFats;
    std::uint16_t rootEntryCount;
    std::uint16_t totalSectors16;
    std::uint8_t media;
    std::uint16_t fatSize16;
    std::uint16_t sectorsPerTrack;
    std::uint16_t heads;
    std::uint32_t hiddenSectors;
    std::uint32_t totalSectors32;
    std::uint32_t fatSize32;
    std::uint16_t extFlags;
    std::uint16_t fsVersion;
    std::uint32_t rootCluster;
};
#pragma pack(pop)

bool ExtensionAllowed(const ScanOptions& options, const std::wstring& ext) {
    if (options.enabledExtensions.empty()) return true;
    for (const auto& e : options.enabledExtensions) {
        if (str::EqualsIgnoreCase(e, ext)) return true;
    }
    return false;
}

std::wstring DosNameToWide(const char name[11], bool deleted) {
    char base[9]{};
    char ext[4]{};
    std::memcpy(base, name, 8);
    std::memcpy(ext, name + 8, 3);
    for (int i = 7; i >= 0 && base[i] == ' '; --i) base[i] = 0;
    for (int i = 2; i >= 0 && ext[i] == ' '; --i) ext[i] = 0;
    if (deleted && base[0] != 0) base[0] = '_';

    std::string s = base;
    if (ext[0]) {
        s.push_back('.');
        s += ext;
    }
    return str::Utf8ToWide(s);
}

} // namespace

bool Fat32Scanner::ParseBoot(VolumeReader& reader, FatBoot& boot) const {
    std::vector<std::uint8_t> sector;
    if (!reader.Read(0, sector, 512)) return false;
    const auto* raw = reinterpret_cast<const Fat32BootRaw*>(sector.data());
    if (raw->bytesPerSector == 0 || raw->sectorsPerCluster == 0) return false;
    if (raw->fatSize32 == 0) return false; // not FAT32

    boot.bytesPerSector = raw->bytesPerSector;
    boot.sectorsPerCluster = raw->sectorsPerCluster;
    boot.reservedSectors = raw->reservedSectors;
    boot.numFats = raw->numFats ? raw->numFats : 2;
    boot.sectorsPerFat = raw->fatSize32;
    boot.rootCluster = raw->rootCluster ? raw->rootCluster : 2;
    boot.totalSectors = raw->totalSectors16 ? raw->totalSectors16 : raw->totalSectors32;
    boot.fatOffset = static_cast<std::uint64_t>(boot.reservedSectors) * boot.bytesPerSector;
    boot.dataOffset = boot.fatOffset +
                      static_cast<std::uint64_t>(boot.numFats) * boot.sectorsPerFat * boot.bytesPerSector;
    boot.clusterSize = static_cast<std::uint32_t>(boot.bytesPerSector) * boot.sectorsPerCluster;
    return true;
}

std::uint32_t Fat32Scanner::NextCluster(VolumeReader& reader, const FatBoot& boot, std::uint32_t cluster) const {
    const std::uint64_t offset = boot.fatOffset + static_cast<std::uint64_t>(cluster) * 4;
    std::uint32_t value = 0;
    if (!reader.Read(offset, &value, sizeof(value))) return 0x0FFFFFFF;
    return value & 0x0FFFFFFF;
}

bool Fat32Scanner::ScanDirectoryTree(VolumeReader& reader, const DriveInfo& drive, const FatBoot& boot,
                                     const ScanOptions& options, ScanProgress& progress,
                                     const FileFoundCallback& onFile, bool deep) {
    const std::uint64_t total = boot.totalSectors
        ? static_cast<std::uint64_t>(boot.totalSectors) * boot.bytesPerSector
        : reader.Size();
    progress.Reset(total ? total : 1, L"Scanning FAT32 directory entries (read-only)...");
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

        std::uint32_t current = cluster;
        int guard = 0;
        while (current >= 2 && current < 0x0FFFFFF8 && guard++ < 4096) {
            progress.WaitWhilePaused();
            if (progress.ShouldStop()) break;

            const std::uint64_t offset = boot.dataOffset +
                static_cast<std::uint64_t>(current - 2) * boot.clusterSize;
            std::vector<std::uint8_t> data;
            if (!reader.Read(offset, data, boot.clusterSize)) break;
            bytesDone += boot.clusterSize;
            progress.SetBytesScanned(bytesDone);

            for (std::size_t i = 0; i + 32 <= data.size(); i += 32) {
                const auto* ent = data.data() + i;
                if (ent[0] == 0x00) { current = 0x0FFFFFFF; break; } // end of dir

                const bool deleted = ent[0] == 0xE5;
                const std::uint8_t attr = ent[11];
                if (attr == 0x0F) continue; // LFN
                if ((attr & 0x08) != 0) continue; // volume label

                const bool isDir = (attr & 0x10) != 0;
                const std::uint16_t clusterHigh = *reinterpret_cast<const std::uint16_t*>(ent + 20);
                const std::uint16_t clusterLow = *reinterpret_cast<const std::uint16_t*>(ent + 26);
                const std::uint32_t firstCluster = (static_cast<std::uint32_t>(clusterHigh) << 16) | clusterLow;
                const std::uint32_t size = *reinterpret_cast<const std::uint32_t*>(ent + 28);

                if (isDir) {
                    if (firstCluster >= 2 && (deep || !deleted)) {
                        // Always traverse existing dirs; also try deleted dir clusters in deep mode
                        queue.push_back(firstCluster);
                    }
                    continue;
                }

                if (!deleted && !deep) continue; // Quick: deleted only
                if (size == 0 || size > options.maxFileSize) continue;

                char dosName[11];
                std::memcpy(dosName, ent, 11);
                auto name = DosNameToWide(dosName, deleted);
                auto ext = fileutil::GetExtension(name);
                if (!ExtensionAllowed(options, ext)) continue;

                RecoveredFile file;
                file.displayName = name;
                file.extension = ext;
                file.originalPath = drive.letter + L"\\[Deleted]\\" + name;
                file.sizeBytes = size;
                file.dataLength = size;
                file.method = DiscoveryMethod::FileSystem;
                file.sourceFs = FileSystemType::Fat32;
                file.sourceDrive = drive.letter;
                file.typeLabel = ext.empty() ? L"File" : ext;
                file.previewable = (ext == L"jpg" || ext == L"png" || ext == L"gif" || ext == L"bmp" ||
                                    ext == L"txt" || ext == L"csv" || ext == L"pdf");
                file.confidence = deleted ? Confidence::Good : Confidence::Partial;

                // Reconstruct cluster chain (best effort; deleted chains may be reused)
                std::uint32_t c = firstCluster;
                std::uint32_t remaining = size;
                int chainGuard = 0;
                bool chainOk = firstCluster >= 2;
                while (chainOk && remaining > 0 && c >= 2 && c < 0x0FFFFFF8 && chainGuard++ < 100000) {
                    const std::uint64_t off = boot.dataOffset + static_cast<std::uint64_t>(c - 2) * boot.clusterSize;
                    const std::uint32_t take = std::min<std::uint32_t>(remaining, boot.clusterSize);
                    file.runs.emplace_back(off, take);
                    remaining -= take;
                    const auto next = NextCluster(reader, boot, c);
                    if (next == 0 || next == 0x0FFFFFF7) {
                        // Deleted/reclaimed FAT entry — stop; mark partial if incomplete
                        break;
                    }
                    c = next;
                }
                if (remaining > 0) {
                    file.confidence = Confidence::Partial;
                    if (file.runs.empty()) {
                        file.status = RecoveryStatus::Unsupported;
                        file.confidence = Confidence::Corrupted;
                    }
                } else {
                    file.startOffset = file.runs.empty() ? 0 : file.runs.front().first;
                }
                file.isFragmented = file.runs.size() > 1;

                onFile(file);
                progress.IncrementFiles();
            }

            const auto next = NextCluster(reader, boot, current);
            if (next < 2 || next >= 0x0FFFFFF8) break;
            current = next;
        }
    }

    progress.SetStatus(L"FAT32 directory scan finished");
    return true;
}

bool Fat32Scanner::QuickScan(VolumeReader& reader, const DriveInfo& drive, const ScanOptions& options,
                             ScanProgress& progress, const FileFoundCallback& onFile) {
    FatBoot boot;
    if (!ParseBoot(reader, boot)) {
        progress.SetStatus(L"Unsupported or unreadable FAT32 boot sector");
        return false;
    }
    return ScanDirectoryTree(reader, drive, boot, options, progress, onFile, false);
}

bool Fat32Scanner::DeepScan(VolumeReader& reader, const DriveInfo& drive, const ScanOptions& options,
                            ScanProgress& progress, const FileFoundCallback& onFile) {
    FatBoot boot;
    if (!ParseBoot(reader, boot)) {
        progress.SetStatus(L"Unsupported or unreadable FAT32 boot sector");
        return false;
    }
    return ScanDirectoryTree(reader, drive, boot, options, progress, onFile, true);
}

} // namespace pcdr
