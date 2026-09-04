#include "drive/DriveDetector.hpp"
#include "drive/VolumeReader.hpp"
#include "filesystem/NtfsScanner.hpp"
#include "filesystem/Fat32Scanner.hpp"
#include "filesystem/ExFatScanner.hpp"
#include "filesystem/FileSystemFactory.hpp"
#include "scanner/ScanProgress.hpp"
#include "utils/Logger.hpp"
#include "utils/PathUtil.hpp"
#include "utils/StringUtil.hpp"

#include <iostream>
#include <atomic>

int wmain() {
    using namespace pcdr;
    Logger::Instance().SetLogDirectory(pathutil::GetLogsDirectory());

    DriveDetector det;
    auto drives = det.EnumerateDrives();
    if (drives.empty()) {
        std::wcout << L"No drives.\n";
        return 1;
    }

    // Prefer C: if present
    DriveInfo drive = drives.front();
    for (const auto& d : drives) {
        if (d.letter == L"C:") { drive = d; break; }
    }

    std::wcout << L"Testing " << drive.letter << L" FS=" << drive.fileSystemName << L"\n";

    VolumeReader reader;
    if (!reader.Open(drive.letter)) {
        std::wcout << L"FAIL open volume (need Administrator)\n";
        return 2;
    }

    auto scanner = FileSystemFactory::Create(drive.fileSystem);
    if (!scanner) {
        std::wcout << L"No FS scanner for this filesystem.\n";
        return 3;
    }

    ScanOptions opt;
    opt.mode = ScanMode::Quick;
    opt.maxFileSize = 4ULL << 30;

    ScanProgress progress;
    std::atomic<std::uint64_t> count{0};
    std::uint64_t sampleShown = 0;

    auto onFile = [&](const RecoveredFile& f) {
        ++count;
        if (sampleShown < 8) {
            std::wcout << L"  found: " << f.displayName
                       << L" size=" << str::FormatBytes(f.sizeBytes)
                       << L" conf=" << ToString(f.confidence) << L"\n";
            ++sampleShown;
        }
    };

    std::wcout << L"--- QuickScan ---\n";
    bool okQ = scanner->QuickScan(reader, drive, opt, progress, onFile);
    std::wcout << L"Quick done ok=" << (okQ ? L"1" : L"0")
               << L" files=" << count.load() << L"\n";

    count = 0;
    sampleShown = 0;
    opt.mode = ScanMode::Deep;
    std::wcout << L"--- DeepScan ---\n";
    bool okD = scanner->DeepScan(reader, drive, opt, progress, onFile);
    std::wcout << L"Deep done ok=" << (okD ? L"1" : L"0")
               << L" files=" << count.load() << L"\n";

    reader.Close();
    std::wcout << L"NOTE: Quick/Deep only list DELETED filesystem records, not existing files.\n";
    return 0;
}
