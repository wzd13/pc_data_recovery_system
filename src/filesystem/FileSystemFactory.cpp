#include "filesystem/FileSystemFactory.hpp"
#include "filesystem/NtfsScanner.hpp"
#include "filesystem/Fat32Scanner.hpp"
#include "filesystem/ExFatScanner.hpp"

namespace pcdr {

std::unique_ptr<IFileSystemScanner> FileSystemFactory::Create(FileSystemType type) {
    switch (type) {
    case FileSystemType::Ntfs:
        return std::make_unique<NtfsScanner>();
    case FileSystemType::Fat32:
    case FileSystemType::Fat16:
        return std::make_unique<Fat32Scanner>();
    case FileSystemType::ExFat:
        return std::make_unique<ExFatScanner>();
    default:
        return nullptr;
    }
}

} // namespace pcdr
