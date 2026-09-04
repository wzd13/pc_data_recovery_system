#pragma once

#include "carving/FileSignature.hpp"
#include <vector>
#include <string>

namespace pcdr {

class SignatureDatabase {
public:
    SignatureDatabase();
    const std::vector<FileSignature>& All() const { return signatures_; }
    void Add(FileSignature sig);
    std::vector<FileSignature> FilterByExtensions(const std::vector<std::wstring>& exts) const;

private:
    void LoadBuiltIn();
    std::vector<FileSignature> signatures_;
};

} // namespace pcdr
