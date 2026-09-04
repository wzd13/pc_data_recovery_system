// Quick console smoke test for core recovery plumbing (not shipped to end users).
#include "drive/DriveDetector.hpp"
#include "drive/VolumeReader.hpp"
#include "utils/StringUtil.hpp"
#include "utils/Logger.hpp"
#include "utils/PathUtil.hpp"
#include "carving/SignatureDatabase.hpp"

#include <iostream>
#include <vector>

int wmain() {
    using namespace pcdr;
    Logger::Instance().SetLogDirectory(pathutil::GetLogsDirectory());
    Logger::Instance().Info(L"Smoke test started");

    DriveDetector det;
    auto drives = det.EnumerateDrives();
    std::wcout << L"[1] Drive detection: " << drives.size() << L" ready volume(s)\n";
    if (drives.empty()) {
        std::wcout << L"FAIL: no drives found\n";
        return 1;
    }

    int opened = 0;
    int bootOk = 0;
    for (const auto& d : drives) {
        std::wcout << L"  " << d.letter << L"  " << d.fileSystemName
                   << L"  " << ToString(d.mediaType)
                   << L"  total=" << str::FormatBytes(d.totalBytes)
                   << L"  free=" << str::FormatBytes(d.freeBytes);
        if (!d.physicalDiskModel.empty()) std::wcout << L"  disk=" << d.physicalDiskModel;
        std::wcout << L"\n";

        VolumeReader reader;
        if (reader.Open(d.letter)) {
            ++opened;
            std::vector<std::uint8_t> boot;
            bool ok = reader.Read(0, boot, 512);
            if (ok) ++bootOk;
            std::wcout << L"    read-only open OK, boot512=" << (ok ? L"OK" : L"FAIL")
                       << L" size=" << str::FormatBytes(reader.Size()) << L"\n";
            reader.Close();
        } else {
            std::wcout << L"    read-only open FAILED (try Administrator)\n";
        }
    }

    SignatureDatabase db;
    std::wcout << L"[2] Signature database entries: " << db.All().size() << L"\n";
    std::wcout << L"[3] Volumes opened read-only: " << opened << L"/" << drives.size() << L"\n";
    std::wcout << L"[4] Boot sector reads OK: " << bootOk << L"/" << opened << L"\n";

    if (opened == 0) {
        std::wcout << L"RESULT: PARTIAL — app runs, but volume access needs Administrator.\n";
        return 2;
    }
    if (bootOk == 0) {
        std::wcout << L"RESULT: PARTIAL — opened volumes but boot read failed.\n";
        return 3;
    }
    std::wcout << L"RESULT: OK — drive detection + read-only volume access works.\n";
    return 0;
}
