#include "filesystem/NtfsScanner.hpp"
#include "utils/FileUtil.hpp"
#include "utils/Logger.hpp"
#include "utils/StringUtil.hpp"
#include "utils/TimeUtil.hpp"

#include <cstring>
#include <algorithm>

namespace pcdr {
namespace {

#pragma pack(push, 1)
struct NtfsBootRaw {
    std::uint8_t jump[3];
    char oem[8];
    std::uint16_t bytesPerSector;
    std::uint8_t sectorsPerCluster;
    std::uint16_t reservedSectors;
    std::uint8_t unused1[5];
    std::uint8_t media;
    std::uint16_t unused2;
    std::uint16_t sectorsPerTrack;
    std::uint16_t heads;
    std::uint32_t hiddenSectors;
    std::uint32_t unused3;
    std::uint32_t unused4;
    std::uint64_t totalSectors;
    std::uint64_t mftLcn;
    std::uint64_t mftMirrLcn;
    std::int8_t clustersPerMftRecord;
    std::uint8_t unused5[3];
    std::int8_t clustersPerIndexRecord;
    std::uint8_t unused6[3];
    std::uint64_t serial;
    std::uint32_t checksum;
};

struct MftHeader {
    char signature[4];
    std::uint16_t usaOffset;
    std::uint16_t usaCount;
    std::uint64_t lsn;
    std::uint16_t sequenceNumber;
    std::uint16_t linkCount;
    std::uint16_t attrOffset;
    std::uint16_t flags; // 0x01 in-use, 0x02 directory
    std::uint32_t bytesInUse;
    std::uint32_t bytesAllocated;
    std::uint64_t baseFileRecord;
    std::uint16_t nextAttrId;
};

struct AttrHeader {
    std::uint32_t type;
    std::uint32_t length;
    std::uint8_t nonResident;
    std::uint8_t nameLength;
    std::uint16_t nameOffset;
    std::uint16_t flags;
    std::uint16_t attrId;
};
#pragma pack(pop)

constexpr std::uint32_t kAttrStandardInfo = 0x10;
constexpr std::uint32_t kAttrFileName = 0x30;
constexpr std::uint32_t kAttrData = 0x80;
constexpr std::uint32_t kAttrEnd = 0xFFFFFFFF;

bool FixupMftRecord(std::uint8_t* record, std::size_t recordSize, std::uint32_t bytesPerSector) {
    auto* hdr = reinterpret_cast<MftHeader*>(record);
    if (hdr->usaOffset == 0 || hdr->usaCount < 2) return true;
    if (hdr->usaOffset + hdr->usaCount * 2 > recordSize) return false;
    const std::uint16_t* usa = reinterpret_cast<const std::uint16_t*>(record + hdr->usaOffset);
    for (std::uint16_t i = 1; i < hdr->usaCount; ++i) {
        const std::size_t sectorEnd = static_cast<std::size_t>(i) * bytesPerSector;
        if (sectorEnd < 2 || sectorEnd > recordSize) return false;
        *reinterpret_cast<std::uint16_t*>(record + sectorEnd - 2) = usa[i];
    }
    return true;
}

std::wstring ReadUtf16(const std::uint8_t* data, std::size_t charCount) {
    std::wstring out;
    out.resize(charCount);
    for (std::size_t i = 0; i < charCount; ++i) {
        out[i] = static_cast<wchar_t>(data[i * 2] | (data[i * 2 + 1] << 8));
    }
    return out;
}

bool ExtensionAllowed(const ScanOptions& options, const std::wstring& ext) {
    if (options.enabledExtensions.empty()) return true;
    for (const auto& e : options.enabledExtensions) {
        if (str::EqualsIgnoreCase(e, ext)) return true;
    }
    return false;
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> DecodeRunList(
    const std::uint8_t* runList, std::size_t maxLen,
    std::uint32_t bytesPerCluster, std::uint64_t& outDataSize) {

    std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;
    std::int64_t offsetLcn = 0;
    outDataSize = 0;
    std::size_t i = 0;
    while (i < maxLen) {
        const std::uint8_t header = runList[i++];
        if (header == 0) break;
        const std::uint8_t lengthSize = header & 0x0F;
        const std::uint8_t offsetSize = (header >> 4) & 0x0F;
        if (i + lengthSize + offsetSize > maxLen) break;

        std::uint64_t runLength = 0;
        for (std::uint8_t b = 0; b < lengthSize; ++b) {
            runLength |= static_cast<std::uint64_t>(runList[i++]) << (8 * b);
        }

        std::int64_t runOffset = 0;
        for (std::uint8_t b = 0; b < offsetSize; ++b) {
            runOffset |= static_cast<std::int64_t>(runList[i++]) << (8 * b);
        }
        if (offsetSize > 0 && (runList[i - 1] & 0x80)) {
            for (std::uint8_t b = offsetSize; b < 8; ++b) {
                runOffset |= static_cast<std::int64_t>(0xFF) << (8 * b);
            }
        }

        if (offsetSize == 0) {
            // sparse
            offsetLcn += 0;
            outDataSize += runLength * bytesPerCluster;
            continue;
        }

        offsetLcn += runOffset;
        const std::uint64_t byteOffset = static_cast<std::uint64_t>(offsetLcn) * bytesPerCluster;
        const std::uint64_t byteLength = runLength * bytesPerCluster;
        runs.emplace_back(byteOffset, byteLength);
        outDataSize += byteLength;
    }
    return runs;
}

} // namespace

bool NtfsScanner::ParseBoot(VolumeReader& reader, BootSector& boot) const {
    std::vector<std::uint8_t> sector;
    if (!reader.Read(0, sector, 512)) return false;
    if (sector.size() < sizeof(NtfsBootRaw)) return false;

    const auto* raw = reinterpret_cast<const NtfsBootRaw*>(sector.data());
    if (std::memcmp(raw->oem, "NTFS    ", 8) != 0) {
        Logger::Instance().Warning(L"Volume OEM ID is not NTFS");
        // continue if BPB looks sane
    }

    boot.bytesPerSector = raw->bytesPerSector ? raw->bytesPerSector : 512;
    boot.sectorsPerCluster = raw->sectorsPerCluster ? raw->sectorsPerCluster : 8;
    boot.mftLcn = raw->mftLcn;
    boot.totalSectors = raw->totalSectors;

    if (raw->clustersPerMftRecord > 0) {
        boot.mftRecordSize = static_cast<std::uint32_t>(raw->clustersPerMftRecord) *
                             boot.bytesPerSector * boot.sectorsPerCluster;
    } else {
        boot.mftRecordSize = 1u << static_cast<std::uint32_t>(-raw->clustersPerMftRecord);
    }
    if (boot.mftRecordSize == 0 || boot.mftRecordSize > 4096) boot.mftRecordSize = 1024;
    return boot.mftLcn != 0;
}

bool NtfsScanner::ScanMft(VolumeReader& reader, const DriveInfo& drive, const BootSector& boot,
                          const ScanOptions& options, ScanProgress& progress,
                          const FileFoundCallback& onFile, bool deepMode) {
    const std::uint32_t clusterSize = boot.bytesPerSector * boot.sectorsPerCluster;
    const std::uint64_t mftStart = boot.mftLcn * static_cast<std::uint64_t>(clusterSize);

    // Resolve $MFT data runs from record 0 (MFT is often fragmented).
    std::vector<std::uint8_t> record(boot.mftRecordSize);
    if (!reader.Read(mftStart, record.data(), boot.mftRecordSize) ||
        std::memcmp(record.data(), "FILE", 4) != 0) {
        progress.SetStatus(L"Cannot read NTFS $MFT record 0");
        return false;
    }
    FixupMftRecord(record.data(), boot.mftRecordSize, boot.bytesPerSector);

    std::vector<std::pair<std::uint64_t, std::uint64_t>> mftRuns;
    {
        auto* hdr = reinterpret_cast<MftHeader*>(record.data());
        std::size_t attrPos = hdr->attrOffset;
        while (attrPos + sizeof(AttrHeader) < boot.mftRecordSize) {
            auto* attr = reinterpret_cast<AttrHeader*>(record.data() + attrPos);
            if (attr->type == kAttrEnd || attr->length == 0) break;
            if (attrPos + attr->length > boot.mftRecordSize) break;
            if (attr->type == kAttrData && attr->nameLength == 0 && attr->nonResident != 0 && attr->length >= 0x40) {
                const std::uint16_t runOffset = *reinterpret_cast<std::uint16_t*>(record.data() + attrPos + 0x20);
                if (runOffset < attr->length) {
                    std::uint64_t runBytes = 0;
                    mftRuns = DecodeRunList(record.data() + attrPos + runOffset,
                                           attr->length - runOffset, clusterSize, runBytes);
                }
                break;
            }
            attrPos += attr->length;
        }
    }
    if (mftRuns.empty()) {
        // Fallback: assume contiguous MFT starting at boot.mftLcn
        const std::uint64_t fallback = deepMode ? (256ULL << 20) : (64ULL << 20);
        mftRuns.emplace_back(mftStart, fallback);
    }

    std::uint64_t totalMftBytes = 0;
    for (const auto& r : mftRuns) totalMftBytes += r.second;
    const std::uint64_t cap = deepMode ? (512ULL << 20) : (96ULL << 20);
    if (totalMftBytes > cap) totalMftBytes = cap;

    progress.Reset(totalMftBytes ? totalMftBytes : 1, deepMode
        ? L"Deep Scan: walking NTFS $MFT runs (deleted records)..."
        : L"Quick Scan: walking NTFS $MFT runs (deleted records)...");
    progress.SetState(ScanState::Running);

    std::uint64_t scanned = 0;
    std::uint64_t deletedSeen = 0;
    std::uint64_t fileRecords = 0;

    for (const auto& run : mftRuns) {
        if (scanned >= totalMftBytes) break;
        std::uint64_t runOff = run.first;
        std::uint64_t runLen = run.second;
        while (runLen >= boot.mftRecordSize && scanned + boot.mftRecordSize <= totalMftBytes) {
            progress.WaitWhilePaused();
            if (progress.ShouldStop()) break;

            const std::uint64_t offset = runOff;
            if (!reader.Read(offset, record.data(), boot.mftRecordSize)) {
                runOff += boot.mftRecordSize;
                runLen -= boot.mftRecordSize;
                scanned += boot.mftRecordSize;
                progress.SetBytesScanned(scanned);
                continue;
            }

            runOff += boot.mftRecordSize;
            runLen -= boot.mftRecordSize;
            scanned += boot.mftRecordSize;
            progress.SetBytesScanned(scanned);

            if (std::memcmp(record.data(), "FILE", 4) != 0) continue;
            ++fileRecords;
            FixupMftRecord(record.data(), boot.mftRecordSize, boot.bytesPerSector);
            auto* hdr = reinterpret_cast<MftHeader*>(record.data());
            const bool inUse = (hdr->flags & 0x01) != 0;
            const bool isDir = (hdr->flags & 0x02) != 0;
            if (isDir || inUse) continue;
            ++deletedSeen;

            std::wstring bestName;
            std::uint64_t fileSize = 0;
            std::optional<std::int64_t> modified;
            std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;
            bool hasData = false;
            bool resident = false;

            std::size_t attrPos = hdr->attrOffset;
            while (attrPos + sizeof(AttrHeader) < boot.mftRecordSize) {
                auto* attr = reinterpret_cast<AttrHeader*>(record.data() + attrPos);
                if (attr->type == kAttrEnd || attr->length == 0) break;
                if (attrPos + attr->length > boot.mftRecordSize) break;

                if (attr->type == kAttrStandardInfo && attr->nonResident == 0 && attr->length >= 0x30) {
                    const std::uint16_t valueOffset = *reinterpret_cast<std::uint16_t*>(record.data() + attrPos + 0x14);
                    if (static_cast<std::uint32_t>(valueOffset) + 24u <= attr->length) {
                        const auto* si = record.data() + attrPos + valueOffset;
                        modified = timeutil::FileTimeToUnix(*reinterpret_cast<const std::uint64_t*>(si + 8));
                    }
                } else if (attr->type == kAttrFileName && attr->nonResident == 0 && attr->nameLength == 0) {
                    const std::uint16_t valueOffset = *reinterpret_cast<std::uint16_t*>(record.data() + attrPos + 0x14);
                    if (static_cast<std::uint32_t>(valueOffset) + 0x42u <= attr->length) {
                        const auto* fn = record.data() + attrPos + valueOffset;
                        const std::uint8_t nameLen = fn[0x40];
                        const std::uint8_t nameType = fn[0x41];
                        if (static_cast<std::uint32_t>(valueOffset) + 0x42u + nameLen * 2u <= attr->length) {
                            auto name = ReadUtf16(fn + 0x42, nameLen);
                            if (bestName.empty() || nameType == 1 || nameType == 3) bestName = std::move(name);
                        }
                    }
                } else if (attr->type == kAttrData && attr->nameLength == 0) {
                    hasData = true;
                    if (attr->nonResident == 0) {
                        resident = true;
                        fileSize = *reinterpret_cast<std::uint32_t*>(record.data() + attrPos + 0x10);
                    } else if (attr->length >= 0x40) {
                        fileSize = *reinterpret_cast<std::uint64_t*>(record.data() + attrPos + 0x30);
                        const std::uint16_t runOffset = *reinterpret_cast<std::uint16_t*>(record.data() + attrPos + 0x20);
                        if (runOffset < attr->length) {
                            std::uint64_t runBytes = 0;
                            runs = DecodeRunList(record.data() + attrPos + runOffset,
                                                 attr->length - runOffset, clusterSize, runBytes);
                        }
                    }
                }
                attrPos += attr->length;
            }

            if (!hasData || fileSize == 0 || fileSize > options.maxFileSize) continue;
            if (bestName.empty()) {
                bestName = L"Deleted_" + std::to_wstring(deletedSeen) + L".bin";
            }
            const auto ext = fileutil::GetExtension(bestName);
            if (!ExtensionAllowed(options, ext)) continue;

            RecoveredFile file;
            file.displayName = bestName;
            file.extension = ext;
            file.originalPath = drive.letter + L"\\[Deleted]\\" + bestName;
            file.sizeBytes = fileSize;
            file.dataLength = fileSize;
            file.method = DiscoveryMethod::FileSystem;
            file.sourceFs = FileSystemType::Ntfs;
            file.sourceDrive = drive.letter;
            file.modifiedUtc = modified;
            file.typeLabel = ext.empty() ? L"File" : str::ToLower(ext);
            file.previewable = (ext == L"jpg" || ext == L"jpeg" || ext == L"png" || ext == L"gif" ||
                                ext == L"bmp" || ext == L"txt" || ext == L"csv" || ext == L"pdf");

            if (resident) {
                file.startOffset = offset;
                file.confidence = Confidence::Good; // resident payload needs attribute re-parse on recover
                file.status = RecoveryStatus::Pending;
            } else if (!runs.empty()) {
                file.runs = std::move(runs);
                file.startOffset = file.runs.front().first;
                file.isFragmented = file.runs.size() > 1;
                file.confidence = file.isFragmented ? Confidence::Good : Confidence::Excellent;
            } else {
                file.confidence = Confidence::Partial;
                file.status = RecoveryStatus::Unsupported;
            }

            onFile(file);
            progress.IncrementFiles();
        }
        if (progress.ShouldStop()) break;
    }

    Logger::Instance().Info(L"NTFS scan stats: FILE records=" + std::to_wstring(fileRecords) +
                            L" deletedCandidates=" + std::to_wstring(deletedSeen) +
                            L" emitted=" + std::to_wstring(progress.Snapshot().filesFound));
    progress.SetStatus(deepMode
        ? (L"Deep Scan finished. Deleted candidates: " + std::to_wstring(deletedSeen))
        : (L"Quick Scan finished. Deleted candidates: " + std::to_wstring(deletedSeen)));
    return true;
}

bool NtfsScanner::QuickScan(VolumeReader& reader, const DriveInfo& drive, const ScanOptions& options,
                            ScanProgress& progress, const FileFoundCallback& onFile) {
    BootSector boot;
    if (!ParseBoot(reader, boot)) {
        progress.SetStatus(L"Unsupported or unreadable NTFS boot sector");
        return false;
    }
    return ScanMft(reader, drive, boot, options, progress, onFile, false);
}

bool NtfsScanner::DeepScan(VolumeReader& reader, const DriveInfo& drive, const ScanOptions& options,
                           ScanProgress& progress, const FileFoundCallback& onFile) {
    BootSector boot;
    if (!ParseBoot(reader, boot)) {
        progress.SetStatus(L"Unsupported or unreadable NTFS boot sector");
        return false;
    }
    return ScanMft(reader, drive, boot, options, progress, onFile, true);
}

} // namespace pcdr
