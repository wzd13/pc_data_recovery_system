#include "drive/VolumeReader.hpp"
#include "utils/Logger.hpp"
#include "utils/StringUtil.hpp"

#include <winioctl.h>

namespace pcdr {

VolumeReader::~VolumeReader() {
    Close();
}

bool VolumeReader::Open(const std::wstring& driveLetter) {
    Close();
    if (driveLetter.size() < 2) return false;

    driveLetter_ = std::wstring(1, towupper(driveLetter[0])) + L":";
    const std::wstring path = L"\\\\.\\" + driveLetter_;

    // Read-only open. Sharing allows Windows to keep using the volume.
    handle_ = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_RANDOM_ACCESS,
        nullptr);

    if (handle_ == INVALID_HANDLE_VALUE) {
        // Fallback without NO_BUFFERING (some volumes reject it).
        handle_ = CreateFileW(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
    }

    if (handle_ == INVALID_HANDLE_VALUE) {
        Logger::Instance().Error(L"Failed to open volume " + driveLetter_ +
                                 L" (error " + std::to_wstring(GetLastError()) + L"). "
                                 L"Administrator privileges may be required for raw volume access.");
        return false;
    }

    DISK_GEOMETRY_EX geo{};
    DWORD bytes = 0;
    if (DeviceIoControl(handle_, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, nullptr, 0,
                        &geo, sizeof(geo), &bytes, nullptr)) {
        bytesPerSector_ = geo.Geometry.BytesPerSector ? geo.Geometry.BytesPerSector : 512;
        size_ = static_cast<std::uint64_t>(geo.DiskSize.QuadPart);
    } else {
        GET_LENGTH_INFORMATION len{};
        if (DeviceIoControl(handle_, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0,
                            &len, sizeof(len), &bytes, nullptr)) {
            size_ = static_cast<std::uint64_t>(len.Length.QuadPart);
        }
    }

    if (size_ == 0) {
        ULARGE_INTEGER freeBytes{}, totalBytes{}, totalFree{};
        const std::wstring root = driveLetter_ + L"\\";
        if (GetDiskFreeSpaceExW(root.c_str(), &freeBytes, &totalBytes, &totalFree)) {
            size_ = totalBytes.QuadPart;
        }
    }

    Logger::Instance().Info(L"Opened volume " + driveLetter_ + L" read-only, size=" +
                            str::FormatBytes(size_));
    return true;
}

void VolumeReader::Close() {
    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
}

bool VolumeReader::Read(std::uint64_t offset, void* buffer, std::size_t size) const {
    if (!IsOpen() || !buffer || size == 0) return false;

    // Align reads to sector boundaries when FILE_FLAG_NO_BUFFERING may be active.
    const std::uint32_t bps = bytesPerSector_ ? bytesPerSector_ : 512;
    const std::uint64_t alignedOffset = offset - (offset % bps);
    const std::size_t lead = static_cast<std::size_t>(offset - alignedOffset);
    const std::size_t alignedSize = ((lead + size + bps - 1) / bps) * bps;

    std::vector<std::uint8_t> temp(alignedSize);
    OVERLAPPED ov{};
    ov.Offset = static_cast<DWORD>(alignedOffset & 0xFFFFFFFFu);
    ov.OffsetHigh = static_cast<DWORD>((alignedOffset >> 32) & 0xFFFFFFFFu);

    DWORD read = 0;
    if (!ReadFile(handle_, temp.data(), static_cast<DWORD>(alignedSize), &read, &ov)) {
        const DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING) return false;
        if (!GetOverlappedResult(handle_, &ov, &read, TRUE)) return false;
    }

    if (read < lead + size) {
        // Partial read at end of volume
        if (read <= lead) return false;
        memcpy(buffer, temp.data() + lead, read - lead);
        memset(static_cast<std::uint8_t*>(buffer) + (read - lead), 0, size - (read - lead));
        return true;
    }

    memcpy(buffer, temp.data() + lead, size);
    return true;
}

bool VolumeReader::Read(std::uint64_t offset, std::vector<std::uint8_t>& buffer, std::size_t size) const {
    buffer.resize(size);
    return Read(offset, buffer.data(), size);
}

} // namespace pcdr
