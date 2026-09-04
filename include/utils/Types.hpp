#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <functional>

namespace pcdr {

enum class DriveMediaType {
    Unknown,
    InternalHdd,
    InternalSsd,
    ExternalHdd,
    UsbFlash,
    UsbExternalSsd,
    MemoryCard,
    Optical,
    Network,
    Removable
};

enum class FileSystemType {
    Unknown,
    Ntfs,
    Fat32,
    ExFat,
    Fat16,
    Other
};

enum class ScanMode {
    Quick,
    Deep,
    Raw
};

enum class DiscoveryMethod {
    FileSystem,
    RawCarve
};

enum class Confidence {
    Excellent,
    Good,
    Partial,
    Corrupted
};

enum class RecoveryStatus {
    Pending,
    Recovered,
    Failed,
    Skipped,
    Unsupported
};

enum class ScanState {
    Idle,
    Running,
    Paused,
    Cancelling,
    Completed,
    Failed
};

struct DriveInfo {
    std::wstring letter;           // e.g. L"C:"
    std::wstring volumeName;
    std::wstring fileSystemName;
    FileSystemType fileSystem = FileSystemType::Unknown;
    DriveMediaType mediaType = DriveMediaType::Unknown;
    std::uint64_t totalBytes = 0;
    std::uint64_t usedBytes = 0;
    std::uint64_t freeBytes = 0;
    std::uint32_t bytesPerSector = 512;
    std::uint32_t sectorsPerCluster = 8;
    std::wstring physicalDiskModel;
    std::wstring physicalDiskSerial;
    std::uint32_t physicalDiskNumber = 0;
    bool isReady = false;
    bool isReadOnly = false;
    bool isSystemDrive = false;
};

struct RecoveredFile {
    std::uint64_t id = 0;
    std::wstring displayName;
    std::wstring extension;
    std::wstring originalPath;
    std::wstring mimeHint;
    std::uint64_t sizeBytes = 0;
    std::optional<std::int64_t> modifiedUtc; // FILETIME-like 100ns since 1601, or unix
    DiscoveryMethod method = DiscoveryMethod::FileSystem;
    Confidence confidence = Confidence::Good;
    RecoveryStatus status = RecoveryStatus::Pending;
    FileSystemType sourceFs = FileSystemType::Unknown;
    std::wstring sourceDrive;
    std::uint64_t startOffset = 0;   // absolute byte offset on volume
    std::uint64_t dataLength = 0;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> runs; // offset,length runs
    bool isFragmented = false;
    bool previewable = false;
    std::wstring typeLabel;
};

struct ScanOptions {
    ScanMode mode = ScanMode::Quick;
    std::uint64_t maxFileSize = 4ULL * 1024 * 1024 * 1024; // 4 GiB
    bool includeHidden = true;
    bool includeSystem = false;
    std::vector<std::wstring> enabledExtensions; // empty = all
    std::uint32_t carveChunkSize = 1024 * 1024; // 1 MiB
};

struct ScanStats {
    ScanState state = ScanState::Idle;
    double progressPercent = 0.0;
    std::uint64_t bytesScanned = 0;
    std::uint64_t bytesTotal = 0;
    std::uint64_t filesFound = 0;
    double bytesPerSecond = 0.0;
    std::chrono::milliseconds elapsed{0};
    std::chrono::milliseconds eta{0};
    std::wstring statusText;
};

struct RecoveryOptions {
    std::wstring destinationFolder;
    bool overwriteExisting = false;
    bool preserveStructure = false;
};

struct RecoveryItemResult {
    std::uint64_t fileId = 0;
    std::wstring fileName;
    bool success = false;
    std::wstring message;
    std::wstring outputPath;
};

struct HistoryEntry {
    std::uint64_t id = 0;
    std::int64_t timestampUtc = 0;
    std::wstring sourceDrive;
    std::wstring destination;
    std::uint32_t filesRequested = 0;
    std::uint32_t successCount = 0;
    std::uint32_t failureCount = 0;
    std::uint32_t durationMs = 0;
    std::wstring notes;
};

using ProgressCallback = std::function<void(const ScanStats&)>;
using FileFoundCallback = std::function<void(const RecoveredFile&)>;
using LogCallback = std::function<void(const std::wstring&)>;

inline const wchar_t* ToString(Confidence c) {
    switch (c) {
    case Confidence::Excellent: return L"Excellent";
    case Confidence::Good: return L"Good";
    case Confidence::Partial: return L"Partial";
    case Confidence::Corrupted: return L"Corrupted";
    }
    return L"Unknown";
}

inline const wchar_t* ToString(DiscoveryMethod m) {
    return m == DiscoveryMethod::RawCarve ? L"RAW Carve" : L"Filesystem";
}

inline const wchar_t* ToString(ScanMode m) {
    switch (m) {
    case ScanMode::Quick: return L"Quick Scan";
    case ScanMode::Deep: return L"Deep Scan";
    case ScanMode::Raw: return L"RAW Recovery";
    }
    return L"Scan";
}

inline const wchar_t* ToString(DriveMediaType t) {
    switch (t) {
    case DriveMediaType::InternalHdd: return L"Internal HDD";
    case DriveMediaType::InternalSsd: return L"Internal SSD";
    case DriveMediaType::ExternalHdd: return L"External HDD";
    case DriveMediaType::UsbFlash: return L"USB Flash Drive";
    case DriveMediaType::UsbExternalSsd: return L"USB External SSD";
    case DriveMediaType::MemoryCard: return L"Memory Card";
    case DriveMediaType::Optical: return L"Optical";
    case DriveMediaType::Network: return L"Network";
    case DriveMediaType::Removable: return L"Removable";
    default: return L"Unknown";
    }
}

inline const wchar_t* ToString(RecoveryStatus s) {
    switch (s) {
    case RecoveryStatus::Pending: return L"Pending";
    case RecoveryStatus::Recovered: return L"Recovered";
    case RecoveryStatus::Failed: return L"Failed";
    case RecoveryStatus::Skipped: return L"Skipped";
    case RecoveryStatus::Unsupported: return L"Unsupported";
    }
    return L"Unknown";
}

} // namespace pcdr
