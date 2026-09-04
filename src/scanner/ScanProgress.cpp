#include "scanner/ScanProgress.hpp"

#include <algorithm>
#include <thread>

namespace pcdr {

void ScanProgress::Reset(std::uint64_t bytesTotal, const std::wstring& status) {
    std::lock_guard lock(mutex_);
    stats_ = {};
    stats_.bytesTotal = bytesTotal;
    stats_.statusText = status;
    stats_.state = ScanState::Running;
    state_ = ScanState::Running;
    start_ = std::chrono::steady_clock::now();
    lastSample_ = start_;
    lastBytes_ = 0;
}

void ScanProgress::BeginPhase(std::uint64_t bytesTotal, const std::wstring& status) {
    std::lock_guard lock(mutex_);
    stats_.bytesTotal = bytesTotal ? bytesTotal : 1;
    stats_.bytesScanned = 0;
    stats_.progressPercent = 0.0;
    stats_.statusText = status;
    stats_.state = ScanState::Running;
    state_ = ScanState::Running;
    lastSample_ = std::chrono::steady_clock::now();
    lastBytes_ = 0;
    // keep start_ and filesFound
}

void ScanProgress::SetState(ScanState state) {
    state_ = state;
    std::lock_guard lock(mutex_);
    stats_.state = state;
}

ScanState ScanProgress::GetState() const {
    return state_.load();
}

bool ScanProgress::ShouldStop() const {
    const auto s = state_.load();
    return s == ScanState::Cancelling || s == ScanState::Idle;
}

bool ScanProgress::IsPaused() const {
    return state_.load() == ScanState::Paused;
}

void ScanProgress::RequestPause() {
    if (state_.load() == ScanState::Running) SetState(ScanState::Paused);
}

void ScanProgress::RequestResume() {
    if (state_.load() == ScanState::Paused) SetState(ScanState::Running);
}

void ScanProgress::RequestCancel() {
    SetState(ScanState::Cancelling);
}

void ScanProgress::WaitWhilePaused() const {
    while (IsPaused()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void ScanProgress::AddBytes(std::uint64_t bytes) {
    std::lock_guard lock(mutex_);
    stats_.bytesScanned += bytes;
    const auto now = std::chrono::steady_clock::now();
    stats_.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_);
    const auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSample_);
    if (dt.count() >= 250) {
        const double secs = dt.count() / 1000.0;
        if (secs > 0) {
            stats_.bytesPerSecond = (stats_.bytesScanned - lastBytes_) / secs;
        }
        lastSample_ = now;
        lastBytes_ = stats_.bytesScanned;
    }
    if (stats_.bytesTotal > 0) {
        stats_.progressPercent = std::min(100.0, (stats_.bytesScanned * 100.0) / stats_.bytesTotal);
        if (stats_.bytesPerSecond > 1.0) {
            const auto remain = stats_.bytesTotal > stats_.bytesScanned ? stats_.bytesTotal - stats_.bytesScanned : 0;
            stats_.eta = std::chrono::milliseconds(static_cast<std::int64_t>((remain / stats_.bytesPerSecond) * 1000.0));
        }
    }
}

void ScanProgress::SetBytesScanned(std::uint64_t bytes) {
    std::lock_guard lock(mutex_);
    const auto delta = bytes > stats_.bytesScanned ? bytes - stats_.bytesScanned : 0;
    stats_.bytesScanned = bytes;
    // reuse speed logic
    const auto now = std::chrono::steady_clock::now();
    stats_.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_);
    const auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSample_);
    if (dt.count() >= 250) {
        const double secs = dt.count() / 1000.0;
        if (secs > 0) stats_.bytesPerSecond = delta > 0 ? (bytes - lastBytes_) / secs : stats_.bytesPerSecond;
        lastSample_ = now;
        lastBytes_ = bytes;
    }
    if (stats_.bytesTotal > 0) {
        stats_.progressPercent = std::min(100.0, (stats_.bytesScanned * 100.0) / stats_.bytesTotal);
        if (stats_.bytesPerSecond > 1.0) {
            const auto remain = stats_.bytesTotal > stats_.bytesScanned ? stats_.bytesTotal - stats_.bytesScanned : 0;
            stats_.eta = std::chrono::milliseconds(static_cast<std::int64_t>((remain / stats_.bytesPerSecond) * 1000.0));
        }
    }
}

void ScanProgress::IncrementFiles(std::uint64_t count) {
    std::lock_guard lock(mutex_);
    stats_.filesFound += count;
}

void ScanProgress::SetStatus(const std::wstring& status) {
    std::lock_guard lock(mutex_);
    stats_.statusText = status;
}

ScanStats ScanProgress::Snapshot() const {
    std::lock_guard lock(mutex_);
    auto snap = stats_;
    snap.state = state_.load();
    return snap;
}

} // namespace pcdr
