#pragma once

#include "utils/Types.hpp"
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>

namespace pcdr {

class ScanProgress {
public:
    void Reset(std::uint64_t bytesTotal, const std::wstring& status);
    // Start a new scan phase without wiping elapsed time / filesFound.
    void BeginPhase(std::uint64_t bytesTotal, const std::wstring& status);
    void SetState(ScanState state);
    ScanState GetState() const;
    bool ShouldStop() const;
    bool IsPaused() const;
    void RequestPause();
    void RequestResume();
    void RequestCancel();
    void WaitWhilePaused() const;

    void AddBytes(std::uint64_t bytes);
    void SetBytesScanned(std::uint64_t bytes);
    void IncrementFiles(std::uint64_t count = 1);
    void SetStatus(const std::wstring& status);
    ScanStats Snapshot() const;

private:
    mutable std::mutex mutex_;
    ScanStats stats_{};
    std::atomic<ScanState> state_{ScanState::Idle};
    std::chrono::steady_clock::time_point start_{};
    std::chrono::steady_clock::time_point lastSample_{};
    std::uint64_t lastBytes_{0};
};

} // namespace pcdr
