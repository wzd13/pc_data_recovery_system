#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace pcdr {

struct FileSignature {
    std::wstring extension;
    std::wstring typeLabel;
    std::vector<std::uint8_t> header;
    std::size_t headerOffset = 0;
    std::vector<std::uint8_t> footer; // optional
    std::optional<std::uint64_t> maxCarveSize;
    bool previewable = false;
};

} // namespace pcdr
