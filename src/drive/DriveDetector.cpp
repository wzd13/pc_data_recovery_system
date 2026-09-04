#include "drive/DriveDetector.hpp"
#include "drive/PhysicalDisk.hpp"
#include "utils/StringUtil.hpp"
#include "utils/Logger.hpp"

#include <windows.h>
#include <winioctl.h>

namespace pcdr {

std::vector<DriveInfo> DriveDetector::EnumerateDrives() const {
    std::vector<DriveInfo> drives;
    const DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if ((mask & (1u << i)) == 0) continue;
        const wchar_t letter = static_cast<wchar_t>(L'A' + i);
        auto info = BuildDriveInfo(letter);
        if (info.isReady) drives.push_back(std::move(info));
    }
    return drives;
}

FileSystemType DriveDetector::ParseFileSystem(const std::wstring& name) const {
    if (str::EqualsIgnoreCase(name, L"NTFS")) return FileSystemType::Ntfs;
    if (str::EqualsIgnoreCase(name, L"FAT32")) return FileSystemType::Fat32;
    if (str::EqualsIgnoreCase(name, L"exFAT")) return FileSystemType::ExFat;
    if (str::EqualsIgnoreCase(name, L"FAT16") || str::EqualsIgnoreCase(name, L"FAT")) return FileSystemType::Fat16;
    if (!name.empty()) return FileSystemType::Other;
    return FileSystemType::Unknown;
}

DriveMediaType DriveDetector::ClassifyMedia(wchar_t /*letter*/, UINT driveType, bool isSsd, bool isUsb) const {
    if (driveType == DRIVE_CDROM) return DriveMediaType::Optical;
    if (driveType == DRIVE_REMOTE) return DriveMediaType::Network;

    if (driveType == DRIVE_REMOVABLE) {
        if (isUsb) return isSsd ? DriveMediaType::UsbExternalSsd : DriveMediaType::UsbFlash;
        return DriveMediaType::MemoryCard;
    }

    if (driveType == DRIVE_FIXED) {
        if (isUsb) return isSsd ? DriveMediaType::UsbExternalSsd : DriveMediaType::ExternalHdd;
        return isSsd ? DriveMediaType::InternalSsd : DriveMediaType::InternalHdd;
    }

    return DriveMediaType::Unknown;
}

bool DriveDetector::IsSsdDisk(DWORD diskNumber) const {
    const std::wstring path = L"\\\\.\\PhysicalDrive" + std::to_wstring(diskNumber);
    HANDLE h = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageDeviceSeekPenaltyProperty;
    query.QueryType = PropertyStandardQuery;
    DEVICE_SEEK_PENALTY_DESCRIPTOR desc{};
    DWORD bytes = 0;
    BOOL ok = DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query),
                              &desc, sizeof(desc), &bytes, nullptr);
    CloseHandle(h);
    if (!ok) return false;
    // No seek penalty typically means SSD
    return desc.IncursSeekPenalty == FALSE;
}

bool DriveDetector::QueryPhysicalDisk(wchar_t letter, DriveInfo& info) const {
    DWORD diskNumber = 0;
    if (!PhysicalDisk::GetDiskNumberForVolume(letter, diskNumber)) return false;
    info.physicalDiskNumber = diskNumber;
    PhysicalDisk::GetDiskModelSerial(diskNumber, info.physicalDiskModel, info.physicalDiskSerial);
    return true;
}

DriveInfo DriveDetector::BuildDriveInfo(wchar_t letter) const {
    DriveInfo info;
    info.letter = std::wstring(1, letter) + L":";
    const std::wstring root = info.letter + L"\\";

    const UINT driveType = GetDriveTypeW(root.c_str());
    if (driveType == DRIVE_NO_ROOT_DIR || driveType == DRIVE_UNKNOWN) {
        info.isReady = false;
        return info;
    }

    wchar_t volumeName[MAX_PATH]{};
    wchar_t fsName[MAX_PATH]{};
    DWORD serial = 0, maxComp = 0, flags = 0;
    if (!GetVolumeInformationW(root.c_str(), volumeName, MAX_PATH, &serial, &maxComp, &flags, fsName, MAX_PATH)) {
        // Media may not be ready (empty card reader, etc.)
        info.isReady = false;
        return info;
    }

    info.isReady = true;
    info.volumeName = volumeName;
    info.fileSystemName = fsName;
    info.fileSystem = ParseFileSystem(fsName);
    info.isReadOnly = (flags & FILE_READ_ONLY_VOLUME) != 0;

    ULARGE_INTEGER freeBytesAvailable{}, totalBytes{}, totalFreeBytes{};
    if (GetDiskFreeSpaceExW(root.c_str(), &freeBytesAvailable, &totalBytes, &totalFreeBytes)) {
        info.totalBytes = totalBytes.QuadPart;
        info.freeBytes = totalFreeBytes.QuadPart;
        info.usedBytes = info.totalBytes > info.freeBytes ? info.totalBytes - info.freeBytes : 0;
    }

    DWORD sectorsPerCluster = 0, bytesPerSector = 0, freeClusters = 0, totalClusters = 0;
    if (GetDiskFreeSpaceW(root.c_str(), &sectorsPerCluster, &bytesPerSector, &freeClusters, &totalClusters)) {
        info.sectorsPerCluster = sectorsPerCluster;
        info.bytesPerSector = bytesPerSector;
    }

    wchar_t windowsDir[MAX_PATH]{};
    GetWindowsDirectoryW(windowsDir, MAX_PATH);
    if (towupper(windowsDir[0]) == towupper(letter)) {
        info.isSystemDrive = true;
    }

    bool isSsd = false;
    bool isUsb = false;
    if (QueryPhysicalDisk(letter, info)) {
        isSsd = IsSsdDisk(info.physicalDiskNumber);
        isUsb = PhysicalDisk::IsUsbBus(info.physicalDiskNumber);
    }

    info.mediaType = ClassifyMedia(letter, driveType, isSsd, isUsb);
    return info;
}

} // namespace pcdr
