#include "recovery/RecoveryEngine.hpp"
#include "utils/FileUtil.hpp"
#include "utils/PathUtil.hpp"
#include "utils/StringUtil.hpp"
#include "utils/TimeUtil.hpp"
#include "utils/Logger.hpp"

#include <windows.h>
#include <algorithm>
#include <sstream>

namespace pcdr {

bool RecoveryEngine::IsDestinationOnSource(const std::wstring& destination, const std::wstring& sourceDrive) {
    if (destination.size() < 2 || sourceDrive.size() < 2) return false;
    return towupper(destination[0]) == towupper(sourceDrive[0]) && destination[1] == L':' && sourceDrive[1] == L':';
}

bool RecoveryEngine::RecoverOne(VolumeReader& reader, const RecoveredFile& file,
                                const RecoveryOptions& options, RecoveryItemResult& out) {
    out.fileId = file.id;
    out.fileName = file.displayName;

    if (file.status == RecoveryStatus::Unsupported || (file.runs.empty() && file.dataLength == 0)) {
        out.success = false;
        out.message = L"Unsupported or incomplete recovery metadata";
        return false;
    }

    if (file.dataLength == 0 || file.sizeBytes == 0) {
        out.success = false;
        out.message = L"Zero-length file";
        return false;
    }

    const auto destPath = fileutil::MakeUniquePath(options.destinationFolder, file.displayName);
    out.outputPath = destPath;

    // Read file data from volume runs
    std::vector<std::uint8_t> buffer;
    buffer.reserve(static_cast<std::size_t>(std::min(file.sizeBytes, static_cast<std::uint64_t>(64 * 1024 * 1024))));

    std::uint64_t remaining = file.sizeBytes;
    if (!file.runs.empty()) {
        for (const auto& run : file.runs) {
            if (remaining == 0) break;
            const std::uint64_t take = std::min(remaining, run.second);
            std::vector<std::uint8_t> chunk;
            if (!reader.Read(run.first, chunk, static_cast<std::size_t>(take))) {
                out.success = false;
                out.message = L"Failed to read source data (read-only)";
                return false;
            }
            buffer.insert(buffer.end(), chunk.begin(), chunk.end());
            remaining -= take;
        }
    } else {
        // Contiguous fallback
        std::vector<std::uint8_t> chunk;
        if (!reader.Read(file.startOffset, chunk, static_cast<std::size_t>(file.sizeBytes))) {
            out.success = false;
            out.message = L"Failed to read source data";
            return false;
        }
        buffer = std::move(chunk);
        remaining = 0;
    }

    if (buffer.empty()) {
        out.success = false;
        out.message = L"No data recovered";
        return false;
    }

    // For resident NTFS files, buffer may contain entire MFT record — attempt to extract $DATA resident.
    // If runs were populated correctly this path won't trigger incorrectly.

    if (!fileutil::WriteFileBytes(destPath, buffer.data(), buffer.size())) {
        out.success = false;
        out.message = L"Failed to write destination file";
        return false;
    }

    out.success = true;
    out.message = L"Recovered";
    return true;
}

bool RecoveryEngine::Recover(const DriveInfo& sourceDrive,
                             const std::vector<RecoveredFile>& files,
                             const RecoveryOptions& options,
                             ResultStore& results,
                             HistoryDatabase& history,
                             const ItemCallback& onItem,
                             std::wstring& errorMessage) {
    if (files.empty()) {
        errorMessage = L"No files selected for recovery.";
        return false;
    }
    if (options.destinationFolder.empty()) {
        errorMessage = L"Recovery destination is not set.";
        return false;
    }
    if (IsDestinationOnSource(options.destinationFolder, sourceDrive.letter)) {
        errorMessage = L"Refusing to recover onto the source drive. Choose another drive/folder.";
        return false;
    }
    if (!fileutil::EnsureDirectory(options.destinationFolder)) {
        errorMessage = L"Cannot create destination folder.";
        return false;
    }

    VolumeReader reader;
    if (!reader.Open(sourceDrive.letter)) {
        errorMessage = L"Cannot open source volume read-only for recovery.";
        return false;
    }

    const auto started = GetTickCount64();
    std::vector<RecoveryItemResult> itemResults;
    std::uint32_t okCount = 0, failCount = 0;

    for (std::size_t i = 0; i < files.size(); ++i) {
        RecoveryItemResult item;
        const bool ok = RecoverOne(reader, files[i], options, item);
        if (ok) {
            ++okCount;
            results.UpdateStatus(files[i].id, RecoveryStatus::Recovered);
        } else {
            ++failCount;
            results.UpdateStatus(files[i].id, RecoveryStatus::Failed);
        }
        itemResults.push_back(item);
        if (onItem) onItem(item, i + 1, files.size());
    }

    reader.Close();

    HistoryEntry he;
    he.id = static_cast<std::uint64_t>(timeutil::NowUnixSeconds());
    he.timestampUtc = timeutil::NowUnixSeconds();
    he.sourceDrive = sourceDrive.letter;
    he.destination = options.destinationFolder;
    he.filesRequested = static_cast<std::uint32_t>(files.size());
    he.successCount = okCount;
    he.failureCount = failCount;
    he.durationMs = static_cast<std::uint32_t>(GetTickCount64() - started);
    he.notes = L"Recovery session";
    history.Add(he);

    const auto report = GenerateReport(itemResults, sourceDrive, options);
    const auto reportPath = fileutil::Combine(options.destinationFolder,
        L"recovery_report_" + std::to_wstring(he.timestampUtc) + L".txt");
    fileutil::WriteTextFile(reportPath, report);

    Logger::Instance().Info(L"Recovery finished: " + std::to_wstring(okCount) + L" ok, " +
                            std::to_wstring(failCount) + L" failed");
    return failCount == 0;
}

std::wstring RecoveryEngine::GenerateReport(const std::vector<RecoveryItemResult>& items,
                                            const DriveInfo& source,
                                            const RecoveryOptions& options) {
    std::wstringstream ss;
    ss << L"PC Data Recovery — Recovery Report\r\n";
    ss << L"Generated: " << timeutil::FormatNowLocal() << L"\r\n";
    ss << L"Source drive: " << source.letter << L" (" << source.volumeName << L")\r\n";
    ss << L"Destination: " << options.destinationFolder << L"\r\n";
    ss << L"Files: " << items.size() << L"\r\n\r\n";
    for (const auto& it : items) {
        ss << (it.success ? L"[OK]   " : L"[FAIL] ")
           << it.fileName << L" — " << it.message;
        if (!it.outputPath.empty()) ss << L" -> " << it.outputPath;
        ss << L"\r\n";
    }
    return ss.str();
}

} // namespace pcdr
