#include "preview/PreviewEngine.hpp"
#include "drive/VolumeReader.hpp"
#include "utils/StringUtil.hpp"
#include "utils/FileUtil.hpp"
#include "settings/Settings.hpp"

#include <objidl.h>
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")

namespace pcdr {

PreviewEngine::PreviewEngine() {
    Gdiplus::GdiplusStartupInput input;
    Gdiplus::GdiplusStartup(&gdiplusToken_, &input, nullptr);
}

PreviewEngine::~PreviewEngine() {
    Clear();
    if (gdiplusToken_) Gdiplus::GdiplusShutdown(gdiplusToken_);
}

void PreviewEngine::Clear() {
    // Caller owns PreviewData bitmaps; nothing cached here.
}

bool PreviewEngine::IsPreviewableExtension(const std::wstring& ext) {
    return ext == L"jpg" || ext == L"jpeg" || ext == L"png" || ext == L"gif" || ext == L"bmp" ||
           ext == L"tif" || ext == L"tiff" || ext == L"txt" || ext == L"csv" || ext == L"log" ||
           ext == L"pdf";
}

bool PreviewEngine::ReadFileBytes(const RecoveredFile& file, VolumeReader* reader,
                                  std::vector<std::uint8_t>& out, std::size_t maxBytes) {
    out.clear();
    if (!reader || !reader->IsOpen()) return false;
    std::uint64_t remaining = std::min(file.sizeBytes, static_cast<std::uint64_t>(maxBytes));
    if (!file.runs.empty()) {
        for (const auto& run : file.runs) {
            if (remaining == 0) break;
            const auto take = static_cast<std::size_t>(std::min(remaining, run.second));
            std::vector<std::uint8_t> chunk;
            if (!reader->Read(run.first, chunk, take)) return false;
            out.insert(out.end(), chunk.begin(), chunk.end());
            remaining -= take;
        }
    } else {
        return reader->Read(file.startOffset, out, static_cast<std::size_t>(remaining));
    }
    return !out.empty();
}

bool PreviewEngine::LoadImageFromMemory(const std::vector<std::uint8_t>& bytes, PreviewData& out) {
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes.size());
    if (!hMem) return false;
    void* p = GlobalLock(hMem);
    memcpy(p, bytes.data(), bytes.size());
    GlobalUnlock(hMem);

    IStream* stream = nullptr;
    if (CreateStreamOnHGlobal(hMem, TRUE, &stream) != S_OK) {
        GlobalFree(hMem);
        return false;
    }

    Gdiplus::Bitmap* bmp = Gdiplus::Bitmap::FromStream(stream);
    stream->Release();
    if (!bmp || bmp->GetLastStatus() != Gdiplus::Ok) {
        delete bmp;
        return false;
    }

    out.imageWidth = static_cast<int>(bmp->GetWidth());
    out.imageHeight = static_cast<int>(bmp->GetHeight());
    HBITMAP hbmp = nullptr;
    if (bmp->GetHBITMAP(Gdiplus::Color(30, 30, 30), &hbmp) == Gdiplus::Ok) {
        out.imageBitmap = hbmp;
        out.kind = PreviewData::Kind::Image;
    }
    delete bmp;
    return out.imageBitmap != nullptr;
}

bool PreviewEngine::LoadTextFromMemory(const std::vector<std::uint8_t>& bytes, PreviewData& out) {
    // Detect UTF-8 / UTF-16 LE
    std::wstring text;
    if (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) {
        text.assign(reinterpret_cast<const wchar_t*>(bytes.data() + 2),
                    (bytes.size() - 2) / 2);
    } else {
        text = str::Utf8ToWide(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    }
    if (text.size() > 20000) text.resize(20000);
    out.textContent = std::move(text);
    out.kind = PreviewData::Kind::Text;
    return true;
}

bool PreviewEngine::BuildPreview(const RecoveredFile& file, VolumeReader* reader, PreviewData& out) {
    if (out.imageBitmap) {
        DeleteObject(out.imageBitmap);
        out.imageBitmap = nullptr;
    }
    out = {};
    out.fileName = file.displayName;
    out.path = file.originalPath;
    out.typeLabel = file.typeLabel;
    out.confidence = ToString(file.confidence);
    out.sizeBytes = file.sizeBytes;

    const auto& settings = Settings::Instance().Get();
    if (!settings.enablePreview) {
        out.kind = PreviewData::Kind::Unsupported;
        out.infoMessage = L"Preview disabled in settings.";
        return false;
    }

    const auto ext = str::ToLower(file.extension);
    std::vector<std::uint8_t> bytes;

    if ((ext == L"jpg" || ext == L"jpeg" || ext == L"png" || ext == L"gif" || ext == L"bmp" ||
         ext == L"tif" || ext == L"tiff") && settings.previewImages) {
        if (!ReadFileBytes(file, reader, bytes, 16 * 1024 * 1024)) {
            out.kind = PreviewData::Kind::Unsupported;
            out.infoMessage = L"Unable to read image data for preview.";
            return false;
        }
        if (!LoadImageFromMemory(bytes, out)) {
            out.kind = PreviewData::Kind::Unsupported;
            out.infoMessage = L"Image preview failed (file may be partial/corrupted).";
            return false;
        }
        return true;
    }

    if ((ext == L"txt" || ext == L"csv" || ext == L"log") && settings.previewText) {
        if (!ReadFileBytes(file, reader, bytes, 256 * 1024)) {
            out.kind = PreviewData::Kind::Unsupported;
            out.infoMessage = L"Unable to read text data for preview.";
            return false;
        }
        return LoadTextFromMemory(bytes, out);
    }

    if (ext == L"pdf" && settings.previewPdf) {
        if (!ReadFileBytes(file, reader, bytes, 64 * 1024)) {
            out.kind = PreviewData::Kind::Unsupported;
            out.infoMessage = L"Unable to read PDF header.";
            return false;
        }
        out.kind = PreviewData::Kind::PdfInfo;
        std::wstring header = str::Utf8ToWide(std::string(reinterpret_cast<const char*>(bytes.data()),
            std::min<std::size_t>(bytes.size(), 32)));
        out.infoMessage = L"PDF detected (" + header + L"). Embedded PDF rendering is limited in portable mode; "
                          L"recover the file to open it in a PDF viewer.";
        return true;
    }

    out.kind = PreviewData::Kind::Unsupported;
    out.infoMessage = L"Preview not available for this file type.";
    return false;
}

} // namespace pcdr
