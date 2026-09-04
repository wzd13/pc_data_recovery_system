#pragma once

#include "utils/Types.hpp"
#include <string>
#include <vector>

namespace pcdr {

struct AppSettings {
    std::wstring defaultRecoveryFolder;
    std::wstring theme = L"dark";
    bool enablePreview = true;
    bool previewImages = true;
    bool previewPdf = true;
    bool previewText = true;
    std::uint64_t maxFileSize = 4ULL * 1024 * 1024 * 1024;
    bool warnBeforeRecovery = true;
    bool warnSourceOverwrite = true;
    bool includeHidden = true;
    bool includeSystem = false;
    ScanMode defaultScanMode = ScanMode::Quick;
    std::vector<std::wstring> enabledExtensions; // empty = all supported
};

class Settings {
public:
    static Settings& Instance();

    bool Load();
    bool Save() const;
    AppSettings& Get();
    const AppSettings& Get() const;

private:
    Settings() = default;
    AppSettings settings_;
    std::wstring settingsPath() const;
};

} // namespace pcdr
