#pragma once

#include "utils/Types.hpp"
#include <windows.h>
#include <vector>

namespace pcdr {

class DriveDetector {
public:
    std::vector<DriveInfo> EnumerateDrives() const;

private:
    DriveInfo BuildDriveInfo(wchar_t letter) const;
    DriveMediaType ClassifyMedia(wchar_t letter, UINT driveType, bool isSsd, bool isUsb) const;
    FileSystemType ParseFileSystem(const std::wstring& name) const;
    bool QueryPhysicalDisk(wchar_t letter, DriveInfo& info) const;
    bool IsSsdDisk(DWORD diskNumber) const;
};

} // namespace pcdr
