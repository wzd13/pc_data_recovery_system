#pragma once

#include "utils/Types.hpp"
#include "drive/VolumeReader.hpp"
#include "scanner/ScanProgress.hpp"
#include <vector>
#include <atomic>
#include <functional>

namespace pcdr {

class IFileSystemScanner {
public:
    virtual ~IFileSystemScanner() = default;
    virtual FileSystemType Type() const = 0;
    virtual bool QuickScan(VolumeReader& reader,
                           const DriveInfo& drive,
                           const ScanOptions& options,
                           ScanProgress& progress,
                           const FileFoundCallback& onFile) = 0;
    virtual bool DeepScan(VolumeReader& reader,
                          const DriveInfo& drive,
                          const ScanOptions& options,
                          ScanProgress& progress,
                          const FileFoundCallback& onFile) = 0;
};

} // namespace pcdr
