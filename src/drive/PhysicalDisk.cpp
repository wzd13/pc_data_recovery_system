#include "drive/PhysicalDisk.hpp"
#include "utils/Logger.hpp"

#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <string>
#include <cstring>

#pragma comment(lib, "setupapi.lib")

namespace pcdr {

bool PhysicalDisk::GetDiskNumberForVolume(wchar_t letter, DWORD& diskNumber) {
    const std::wstring path = std::wstring(L"\\\\.\\") + letter + L":";
    HANDLE h = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    VOLUME_DISK_EXTENTS extents{};
    DWORD bytes = 0;
    const BOOL ok = DeviceIoControl(h, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, nullptr, 0,
                                    &extents, sizeof(extents), &bytes, nullptr);
    CloseHandle(h);
    if (!ok || extents.NumberOfDiskExtents == 0) return false;
    diskNumber = extents.Extents[0].DiskNumber;
    return true;
}

bool PhysicalDisk::GetDiskModelSerial(DWORD diskNumber, std::wstring& model, std::wstring& serial) {
    const std::wstring path = L"\\\\.\\PhysicalDrive" + std::to_wstring(diskNumber);
    HANDLE h = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;

    BYTE buffer[4096]{};
    DWORD bytes = 0;
    if (!DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query),
                         buffer, sizeof(buffer), &bytes, nullptr)) {
        CloseHandle(h);
        return false;
    }
    CloseHandle(h);

    auto* desc = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer);
    if (desc->ProductIdOffset) {
        model = std::wstring(reinterpret_cast<char*>(buffer + desc->ProductIdOffset),
                             reinterpret_cast<char*>(buffer + desc->ProductIdOffset) +
                             strnlen_s(reinterpret_cast<char*>(buffer + desc->ProductIdOffset), 256));
    }
    if (desc->SerialNumberOffset) {
        serial = std::wstring(reinterpret_cast<char*>(buffer + desc->SerialNumberOffset),
                              reinterpret_cast<char*>(buffer + desc->SerialNumberOffset) +
                              strnlen_s(reinterpret_cast<char*>(buffer + desc->SerialNumberOffset), 256));
    }
    // trim
    while (!model.empty() && iswspace(model.back())) model.pop_back();
    while (!serial.empty() && iswspace(serial.back())) serial.pop_back();
    return true;
}

bool PhysicalDisk::IsRemovableOrHotplug(DWORD diskNumber) {
    const std::wstring path = L"\\\\.\\PhysicalDrive" + std::to_wstring(diskNumber);
    HANDLE h = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;
    BYTE buffer[1024]{};
    DWORD bytes = 0;
    BOOL ok = DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query),
                              buffer, sizeof(buffer), &bytes, nullptr);
    CloseHandle(h);
    if (!ok) return false;
    auto* desc = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer);
    return desc->RemovableMedia != FALSE;
}

bool PhysicalDisk::IsUsbBus(DWORD diskNumber) {
    const std::wstring path = L"\\\\.\\PhysicalDrive" + std::to_wstring(diskNumber);
    HANDLE h = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageAdapterProperty;
    query.QueryType = PropertyStandardQuery;
    BYTE buffer[1024]{};
    DWORD bytes = 0;
    BOOL ok = DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query),
                              buffer, sizeof(buffer), &bytes, nullptr);
    CloseHandle(h);
    if (!ok) return false;
    auto* desc = reinterpret_cast<STORAGE_ADAPTER_DESCRIPTOR*>(buffer);
    return desc->BusType == BusTypeUsb;
}

} // namespace pcdr
