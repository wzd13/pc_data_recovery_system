#pragma once

#include "filesystem/IFileSystemScanner.hpp"

namespace pcdr {

class ExFatScanner : public IFileSystemScanner {
public:
    FileSystemType Type() const override { return FileSystemType::ExFat; }
    bool QuickScan(VolumeReader& reader, const DriveInfo& drive, const ScanOptions& options,
                   ScanProgress& progress, const FileFoundCallback& onFile) override;
    bool DeepScan(VolumeReader& reader, const DriveInfo& drive, const ScanOptions& options,
                  ScanProgress& progress, const FileFoundCallback& onFile) override;

private:
    struct ExFatBoot {
        std::uint16_t bytesPerSector = 512;
        std::uint8_t sectorsPerClusterShift = 0;
        std::uint32_t fatOffsetSectors = 0;
        std::uint32_t fatLengthSectors = 0;
        std::uint32_t clusterHeapOffset = 0;
        std::uint32_t clusterCount = 0;
        std::uint32_t rootCluster = 0;
        std::uint32_t clusterSize = 0;
        std::uint64_t volumeLength = 0;
    };

    bool ParseBoot(VolumeReader& reader, ExFatBoot& boot) const;
    bool ScanRoot(VolumeReader& reader, const DriveInfo& drive, const ExFatBoot& boot,
                  const ScanOptions& options, ScanProgress& progress,
                  const FileFoundCallback& onFile, bool deep);
};

} // namespace pcdr
