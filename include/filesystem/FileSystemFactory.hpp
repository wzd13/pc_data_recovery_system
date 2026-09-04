#pragma once

#include "filesystem/IFileSystemScanner.hpp"
#include <memory>

namespace pcdr {

class FileSystemFactory {
public:
    static std::unique_ptr<IFileSystemScanner> Create(FileSystemType type);
};

} // namespace pcdr
