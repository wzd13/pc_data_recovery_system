#include "carving/SignatureDatabase.hpp"
#include "utils/StringUtil.hpp"

namespace pcdr {
namespace {

FileSignature Sig(std::wstring ext, std::wstring label, std::vector<std::uint8_t> header,
                  bool preview = false, std::optional<std::uint64_t> maxSize = std::nullopt,
                  std::vector<std::uint8_t> footer = {}, std::size_t headerOffset = 0) {
    FileSignature s;
    s.extension = std::move(ext);
    s.typeLabel = std::move(label);
    s.header = std::move(header);
    s.footer = std::move(footer);
    s.headerOffset = headerOffset;
    s.maxCarveSize = maxSize;
    s.previewable = preview;
    return s;
}

} // namespace

SignatureDatabase::SignatureDatabase() {
    LoadBuiltIn();
}

void SignatureDatabase::Add(FileSignature sig) {
    signatures_.push_back(std::move(sig));
}

std::vector<FileSignature> SignatureDatabase::FilterByExtensions(const std::vector<std::wstring>& exts) const {
    if (exts.empty()) return signatures_;
    std::vector<FileSignature> out;
    for (const auto& sig : signatures_) {
        for (const auto& e : exts) {
            if (str::EqualsIgnoreCase(e, sig.extension)) {
                out.push_back(sig);
                break;
            }
        }
    }
    return out;
}

void SignatureDatabase::LoadBuiltIn() {
    // Images
    signatures_.push_back(Sig(L"jpg", L"JPEG Image", {0xFF, 0xD8, 0xFF}, true, 100ULL << 20, {0xFF, 0xD9}));
    signatures_.push_back(Sig(L"png", L"PNG Image", {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A}, true, 100ULL << 20,
                              {0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82}));
    signatures_.push_back(Sig(L"gif", L"GIF Image", {'G', 'I', 'F', '8'}, true, 50ULL << 20, {0x00, 0x3B}));
    signatures_.push_back(Sig(L"bmp", L"BMP Image", {'B', 'M'}, true, 100ULL << 20));
    signatures_.push_back(Sig(L"tif", L"TIFF Image", {'I', 'I', 0x2A, 0x00}, true, 200ULL << 20));
    signatures_.push_back(Sig(L"tif", L"TIFF Image", {'M', 'M', 0x00, 0x2A}, true, 200ULL << 20));

    // Video
    signatures_.push_back(Sig(L"mp4", L"MP4 Video", {0x00, 0x00, 0x00}, false, 4ULL << 30)); // ftyp often at +4
    // Better MP4: search for ftyp box — handled specially in carver via secondary check
    signatures_.push_back(Sig(L"mov", L"QuickTime Movie", {0x00, 0x00, 0x00}, false, 4ULL << 30));
    signatures_.push_back(Sig(L"avi", L"AVI Video", {'R', 'I', 'F', 'F'}, false, 4ULL << 30));
    signatures_.push_back(Sig(L"mkv", L"Matroska Video", {0x1A, 0x45, 0xDF, 0xA3}, false, 4ULL << 30));

    // Audio
    signatures_.push_back(Sig(L"mp3", L"MP3 Audio", {0xFF, 0xFB}, false, 100ULL << 20));
    signatures_.push_back(Sig(L"mp3", L"MP3 Audio", {'I', 'D', '3'}, false, 100ULL << 20));
    signatures_.push_back(Sig(L"wav", L"WAV Audio", {'R', 'I', 'F', 'F'}, false, 500ULL << 20));

    // Documents
    signatures_.push_back(Sig(L"pdf", L"PDF Document", {'%', 'P', 'D', 'F', '-'}, true, 200ULL << 20, {'%', '%', 'E', 'O', 'F'}));
    signatures_.push_back(Sig(L"doc", L"Word Document", {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1}, false, 100ULL << 20));
    signatures_.push_back(Sig(L"xls", L"Excel Spreadsheet", {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1}, false, 100ULL << 20));
    signatures_.push_back(Sig(L"ppt", L"PowerPoint", {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1}, false, 100ULL << 20));
    // OOXML (ZIP-based)
    signatures_.push_back(Sig(L"docx", L"Word Document (OOXML)", {'P', 'K', 0x03, 0x04}, false, 100ULL << 20));
    signatures_.push_back(Sig(L"xlsx", L"Excel Spreadsheet (OOXML)", {'P', 'K', 0x03, 0x04}, false, 100ULL << 20));
    signatures_.push_back(Sig(L"pptx", L"PowerPoint (OOXML)", {'P', 'K', 0x03, 0x04}, false, 100ULL << 20));

    // Archives / disk
    signatures_.push_back(Sig(L"zip", L"ZIP Archive", {'P', 'K', 0x03, 0x04}, false, 4ULL << 30, {'P', 'K', 0x05, 0x06}));
    signatures_.push_back(Sig(L"rar", L"RAR Archive", {'R', 'a', 'r', '!', 0x1A, 0x07}, false, 4ULL << 30));
    signatures_.push_back(Sig(L"7z", L"7-Zip Archive", {'7', 'z', 0xBC, 0xAF, 0x27, 0x1C}, false, 4ULL << 30));
    signatures_.push_back(Sig(L"iso", L"ISO Image", {0x01, 'C', 'D', '0', '0', '1'}, false, 8ULL << 30, {}, 0x8001));

    // DB / text
    signatures_.push_back(Sig(L"sqlite", L"SQLite Database", {'S', 'Q', 'L', 'i', 't', 'e', ' ', 'f', 'o', 'r', 'm', 'a', 't', ' ', '3', 0x00}, false, 4ULL << 30));
    signatures_.push_back(Sig(L"txt", L"Text File", {}, true, 10ULL << 20)); // not used for carve headerless
    signatures_.push_back(Sig(L"csv", L"CSV File", {}, true, 10ULL << 20));
}

} // namespace pcdr
