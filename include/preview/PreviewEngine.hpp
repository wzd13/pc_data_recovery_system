#pragma once

#include "utils/Types.hpp"
#include <windows.h>
#include <string>
#include <vector>
#include <optional>

namespace pcdr {

struct PreviewData {
    std::wstring fileName;
    std::wstring path;
    std::wstring typeLabel;
    std::wstring confidence;
    std::uint64_t sizeBytes = 0;
    int imageWidth = 0;
    int imageHeight = 0;
    enum class Kind { None, Image, Text, PdfInfo, Unsupported } kind = Kind::None;
    HBITMAP imageBitmap = nullptr;
    std::wstring textContent;
    std::wstring infoMessage;
};

class PreviewEngine {
public:
    PreviewEngine();
    ~PreviewEngine();

    PreviewEngine(const PreviewEngine&) = delete;
    PreviewEngine& operator=(const PreviewEngine&) = delete;

    void Clear();
    bool BuildPreview(const RecoveredFile& file, class VolumeReader* reader, PreviewData& out);
    static bool IsPreviewableExtension(const std::wstring& ext);

private:
    bool LoadImageFromMemory(const std::vector<std::uint8_t>& bytes, PreviewData& out);
    bool LoadTextFromMemory(const std::vector<std::uint8_t>& bytes, PreviewData& out);
    bool ReadFileBytes(const RecoveredFile& file, VolumeReader* reader, std::vector<std::uint8_t>& out, std::size_t maxBytes);
    ULONG_PTR gdiplusToken_ = 0;
};

} // namespace pcdr
