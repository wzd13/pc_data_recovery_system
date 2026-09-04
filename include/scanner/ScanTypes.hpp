#pragma once

#include "utils/Types.hpp"

namespace pcdr {

// Shared scan mode / option helpers
struct ScanModeInfo {
    ScanMode mode;
    const wchar_t* title;
    const wchar_t* description;
};

} // namespace pcdr
