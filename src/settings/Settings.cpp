#include "settings/Settings.hpp"
#include "utils/PathUtil.hpp"
#include "utils/FileUtil.hpp"
#include "utils/StringUtil.hpp"
#include "utils/Logger.hpp"

#include <sstream>

namespace pcdr {
namespace {

std::wstring Escape(const std::wstring& s) {
    return str::ReplaceAll(str::ReplaceAll(s, L"\\", L"\\\\"), L"\n", L"\\n");
}

std::wstring Unescape(const std::wstring& s) {
    std::wstring out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\\' && i + 1 < s.size()) {
            if (s[i + 1] == L'n') { out.push_back(L'\n'); ++i; continue; }
            if (s[i + 1] == L'\\') { out.push_back(L'\\'); ++i; continue; }
        }
        out.push_back(s[i]);
    }
    return out;
}

} // namespace

Settings& Settings::Instance() {
    static Settings instance;
    return instance;
}

std::wstring Settings::settingsPath() const {
    return fileutil::Combine(pathutil::GetConfigDirectory(), L"settings.ini");
}

AppSettings& Settings::Get() { return settings_; }
const AppSettings& Settings::Get() const { return settings_; }

bool Settings::Load() {
    const auto path = settingsPath();
    if (!fileutil::FileExists(path)) {
        settings_.defaultRecoveryFolder = fileutil::Combine(pathutil::GetDataDirectory(), L"Recovered");
        fileutil::EnsureDirectory(settings_.defaultRecoveryFolder);
        return Save();
    }

    const auto text = fileutil::ReadTextFile(path);
    for (const auto& lineRaw : str::Split(text, L'\n')) {
        auto line = str::Trim(str::ReplaceAll(lineRaw, L"\r", L""));
        if (line.empty() || line[0] == L'#' || line[0] == L';') continue;
        const auto eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        const auto key = str::Trim(line.substr(0, eq));
        const auto value = Unescape(str::Trim(line.substr(eq + 1)));

        if (key == L"defaultRecoveryFolder") settings_.defaultRecoveryFolder = value;
        else if (key == L"theme") settings_.theme = value;
        else if (key == L"enablePreview") settings_.enablePreview = value == L"1";
        else if (key == L"previewImages") settings_.previewImages = value == L"1";
        else if (key == L"previewPdf") settings_.previewPdf = value == L"1";
        else if (key == L"previewText") settings_.previewText = value == L"1";
        else if (key == L"maxFileSize") settings_.maxFileSize = std::stoull(str::WideToUtf8(value));
        else if (key == L"warnBeforeRecovery") settings_.warnBeforeRecovery = value == L"1";
        else if (key == L"warnSourceOverwrite") settings_.warnSourceOverwrite = value == L"1";
        else if (key == L"includeHidden") settings_.includeHidden = value == L"1";
        else if (key == L"includeSystem") settings_.includeSystem = value == L"1";
        else if (key == L"defaultScanMode") {
            if (value == L"deep") settings_.defaultScanMode = ScanMode::Deep;
            else if (value == L"raw") settings_.defaultScanMode = ScanMode::Raw;
            else settings_.defaultScanMode = ScanMode::Quick;
        }
        else if (key == L"enabledExtensions") {
            settings_.enabledExtensions.clear();
            for (auto part : str::Split(value, L',')) {
                part = str::ToLower(str::Trim(part));
                if (!part.empty()) settings_.enabledExtensions.push_back(part);
            }
        }
    }

    if (settings_.defaultRecoveryFolder.empty()) {
        settings_.defaultRecoveryFolder = fileutil::Combine(pathutil::GetDataDirectory(), L"Recovered");
    }
    fileutil::EnsureDirectory(settings_.defaultRecoveryFolder);
    return true;
}

bool Settings::Save() const {
    std::wstringstream ss;
    ss << L"# PC Data Recovery settings\n";
    ss << L"defaultRecoveryFolder=" << Escape(settings_.defaultRecoveryFolder) << L"\n";
    ss << L"theme=" << settings_.theme << L"\n";
    ss << L"enablePreview=" << (settings_.enablePreview ? L"1" : L"0") << L"\n";
    ss << L"previewImages=" << (settings_.previewImages ? L"1" : L"0") << L"\n";
    ss << L"previewPdf=" << (settings_.previewPdf ? L"1" : L"0") << L"\n";
    ss << L"previewText=" << (settings_.previewText ? L"1" : L"0") << L"\n";
    ss << L"maxFileSize=" << settings_.maxFileSize << L"\n";
    ss << L"warnBeforeRecovery=" << (settings_.warnBeforeRecovery ? L"1" : L"0") << L"\n";
    ss << L"warnSourceOverwrite=" << (settings_.warnSourceOverwrite ? L"1" : L"0") << L"\n";
    ss << L"includeHidden=" << (settings_.includeHidden ? L"1" : L"0") << L"\n";
    ss << L"includeSystem=" << (settings_.includeSystem ? L"1" : L"0") << L"\n";
    ss << L"defaultScanMode=";
    switch (settings_.defaultScanMode) {
    case ScanMode::Deep: ss << L"deep"; break;
    case ScanMode::Raw: ss << L"raw"; break;
    default: ss << L"quick"; break;
    }
    ss << L"\n";
    ss << L"enabledExtensions=" << str::Join(settings_.enabledExtensions, L",") << L"\n";
    return fileutil::WriteTextFile(settingsPath(), ss.str());
}

} // namespace pcdr
