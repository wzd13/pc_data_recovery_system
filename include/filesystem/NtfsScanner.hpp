#pragma once

#include "filesystem/IFileSystemScanner.hpp"

namespace pcdr {

class NtfsScanner : public IFileSystemScanner {
public:
    FileSystemType Type() const override { return FileSystemType::Ntfs; }
    bool QuickScan(VolumeReader& reader, const DriveInfo& drive, const ScanOptions& options,
                   ScanProgress& progress, const FileFoundCallback& onFile) override;
    bool DeepScan(VolumeReader& reader, const DriveInfo& drive, const ScanOptions& options,
                  ScanProgress& progress, const FileFoundCallback& onFile) override;

private:
    struct BootSector {
        std::uint16_t bytesPerSector = 512;
        std::uint8_t sectorsPerCluster = 8;
        std::uint64_t mftLcn = 0;
        std::uint32_t clustersPerMftRecord = 0;
        std::uint32_t mftRecordSize = 1024;
        std::uint64_t totalSectors = 0;
    };

    bool ParseBoot(VolumeReader& reader, BootSector& boot) const;
    bool ScanMft(VolumeReader& reader, const DriveInfo& drive, const BootSector& boot,
                 const ScanOptions& options, ScanProgress& progress,
                 const FileFoundCallback& onFile, bool deepMode);
};

} // namespace pcdr
