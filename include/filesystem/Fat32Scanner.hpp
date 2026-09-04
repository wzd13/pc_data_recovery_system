#pragma once

#include "filesystem/IFileSystemScanner.hpp"

namespace pcdr {

class Fat32Scanner : public IFileSystemScanner {
public:
    FileSystemType Type() const override { return FileSystemType::Fat32; }
    bool QuickScan(VolumeReader& reader, const DriveInfo& drive, const ScanOptions& options,
                   ScanProgress& progress, const FileFoundCallback& onFile) override;
    bool DeepScan(VolumeReader& reader, const DriveInfo& drive, const ScanOptions& options,
                  ScanProgress& progress, const FileFoundCallback& onFile) override;

private:
    struct FatBoot {
        std::uint16_t bytesPerSector = 512;
        std::uint8_t sectorsPerCluster = 0;
        std::uint16_t reservedSectors = 0;
        std::uint8_t numFats = 2;
        std::uint32_t sectorsPerFat = 0;
        std::uint32_t rootCluster = 2;
        std::uint32_t totalSectors = 0;
        std::uint64_t fatOffset = 0;
        std::uint64_t dataOffset = 0;
        std::uint32_t clusterSize = 0;
    };

    bool ParseBoot(VolumeReader& reader, FatBoot& boot) const;
    bool ScanDirectoryTree(VolumeReader& reader, const DriveInfo& drive, const FatBoot& boot,
                           const ScanOptions& options, ScanProgress& progress,
                           const FileFoundCallback& onFile, bool deep);
    std::uint32_t NextCluster(VolumeReader& reader, const FatBoot& boot, std::uint32_t cluster) const;
};

} // namespace pcdr
