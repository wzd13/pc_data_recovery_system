#include "scanner/ScanEngine.hpp"
#include "drive/VolumeReader.hpp"
#include "filesystem/FileSystemFactory.hpp"
#include "carving/RawCarver.hpp"
#include "utils/Logger.hpp"
#include "settings/Settings.hpp"

#include <chrono>

namespace pcdr {

ScanEngine::ScanEngine() = default;

ScanEngine::~ScanEngine() {
    Cancel();
    if (worker_.joinable()) worker_.join();
}

void ScanEngine::SetProgressCallback(ProgressCallback cb) { progressCb_ = std::move(cb); }
void ScanEngine::SetFileFoundCallback(FileFoundCallback cb) { fileCb_ = std::move(cb); }

bool ScanEngine::Start(const DriveInfo& drive, const ScanOptions& options, ResultStore& results) {
    if (running_.exchange(true)) return false;
    if (worker_.joinable()) worker_.join();
    results.Clear();
    worker_ = std::thread(&ScanEngine::Worker, this, drive, options, &results);
    return true;
}

void ScanEngine::Pause() { progress_.RequestPause(); }
void ScanEngine::Resume() { progress_.RequestResume(); }

void ScanEngine::Cancel() {
    progress_.RequestCancel();
}

bool ScanEngine::IsRunning() const { return running_.load(); }

ScanStats ScanEngine::GetStats() const { return progress_.Snapshot(); }

void ScanEngine::Worker(DriveInfo drive, ScanOptions options, ResultStore* results) {
    Logger::Instance().Info(L"Scan started on " + drive.letter + L" mode=" + ToString(options.mode));

    if (options.enabledExtensions.empty()) {
        options.enabledExtensions = Settings::Instance().Get().enabledExtensions;
    }
    if (options.maxFileSize == 0) {
        options.maxFileSize = Settings::Instance().Get().maxFileSize;
    }

    VolumeReader reader;
    if (!reader.Open(drive.letter)) {
        progress_.SetStatus(L"Cannot open volume in read-only mode. Try running as Administrator.");
        progress_.SetState(ScanState::Failed);
        running_ = false;
        if (progressCb_) progressCb_(progress_.Snapshot());
        return;
    }

    auto lastProgressPost = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    auto lastFilePost = lastProgressPost;
    std::uint64_t pendingFiles = 0;

    auto emit = [&](const RecoveredFile& f) {
        RecoveredFile copy = f;
        copy.id = results->Add(copy);
        ++pendingFiles;

        const auto now = std::chrono::steady_clock::now();
        // Throttle UI file notifications — full list rebuilds are expensive.
        if (fileCb_ && (pendingFiles >= 25 ||
                        now - lastFilePost >= std::chrono::milliseconds(400))) {
            fileCb_(copy);
            pendingFiles = 0;
            lastFilePost = now;
        }

        if (progressCb_ && now - lastProgressPost >= std::chrono::milliseconds(200)) {
            progressCb_(progress_.Snapshot());
            lastProgressPost = now;
        }
    };

    bool ok = false;
    if (options.mode == ScanMode::Raw) {
        options.carveChunkSize = 4u << 20; // 4 MiB chunks for RAW mode
        RawCarver carver;
        ok = carver.Carve(reader, drive, options, progress_, emit);
    } else {
        auto scanner = FileSystemFactory::Create(drive.fileSystem);
        if (!scanner) {
            progress_.SetStatus(L"Filesystem not supported for metadata scan. Falling back to limited RAW...");
            options.carveChunkSize = 4u << 20;
            RawCarver carver;
            ok = carver.Carve(reader, drive, options, progress_, emit);
        } else if (options.mode == ScanMode::Quick) {
            ok = scanner->QuickScan(reader, drive, options, progress_, emit);
        } else {
            // Deep Scan: thorough filesystem pass only.
            // Full-volume RAW carving of large drives (e.g. 448GB) can take many hours and
            // previously flooded the UI — use dedicated RAW Recovery mode for that.
            ok = scanner->DeepScan(reader, drive, options, progress_, emit);
            if (ok && !progress_.ShouldStop()) {
                progress_.SetStatus(L"Deep Scan filesystem pass finished. Use RAW Recovery for full signature carve.");
            }
        }
    }

    // Flush any remaining UI notifications
    if (fileCb_ && pendingFiles > 0) {
        RecoveredFile dummy;
        dummy.id = 0;
        fileCb_(dummy);
    }

    reader.Close();

    if (progress_.GetState() == ScanState::Cancelling) {
        progress_.SetStatus(L"Scan cancelled");
        progress_.SetState(ScanState::Idle);
    } else if (!ok) {
        progress_.SetState(ScanState::Failed);
    } else {
        progress_.SetStatus(L"Scan completed");
        progress_.SetState(ScanState::Completed);
    }

    running_ = false;
    if (progressCb_) progressCb_(progress_.Snapshot());
    Logger::Instance().Info(L"Scan finished on " + drive.letter);
}

} // namespace pcdr
