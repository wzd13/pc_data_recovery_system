#pragma once

#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace pcdr {

// Read-only access to a volume (\\.\X:). Never writes to the source.
class VolumeReader {
public:
    VolumeReader() = default;
    ~VolumeReader();

    VolumeReader(const VolumeReader&) = delete;
    VolumeReader& operator=(const VolumeReader&) = delete;

    bool Open(const std::wstring& driveLetter); // e.g. L"C:"
    void Close();
    bool IsOpen() const { return handle_ != INVALID_HANDLE_VALUE; }

    bool Read(std::uint64_t offset, void* buffer, std::size_t size) const;
    bool Read(std::uint64_t offset, std::vector<std::uint8_t>& buffer, std::size_t size) const;

    std::uint64_t Size() const { return size_; }
    std::uint32_t BytesPerSector() const { return bytesPerSector_; }
    std::wstring DriveLetter() const { return driveLetter_; }

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    std::wstring driveLetter_;
    std::uint64_t size_ = 0;
    std::uint32_t bytesPerSector_ = 512;
};

} // namespace pcdr
