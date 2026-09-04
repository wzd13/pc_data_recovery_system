#pragma once

#include "utils/Types.hpp"
#include "scanner/ScanProgress.hpp"
#include "scanner/ResultStore.hpp"

#include <thread>
#include <atomic>
#include <memory>
#include <functional>

namespace pcdr {

class ScanEngine {
public:
    ScanEngine();
    ~ScanEngine();

    void SetProgressCallback(ProgressCallback cb);
    void SetFileFoundCallback(FileFoundCallback cb);

    bool Start(const DriveInfo& drive, const ScanOptions& options, ResultStore& results);
    void Pause();
    void Resume();
    void Cancel();
    bool IsRunning() const;
    ScanStats GetStats() const;

private:
    void Worker(DriveInfo drive, ScanOptions options, ResultStore* results);
    ProgressCallback progressCb_;
    FileFoundCallback fileCb_;
    ScanProgress progress_;
    std::thread worker_;
    std::atomic<bool> running_{false};
};

} // namespace pcdr
