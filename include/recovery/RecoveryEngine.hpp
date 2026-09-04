#pragma once

#include "utils/Types.hpp"
#include "drive/VolumeReader.hpp"
#include "scanner/ResultStore.hpp"
#include "database/HistoryDatabase.hpp"

#include <functional>
#include <vector>
#include <atomic>

namespace pcdr {

class RecoveryEngine {
public:
    using ItemCallback = std::function<void(const RecoveryItemResult&, std::size_t done, std::size_t total)>;

    bool Recover(const DriveInfo& sourceDrive,
                 const std::vector<RecoveredFile>& files,
                 const RecoveryOptions& options,
                 ResultStore& results,
                 HistoryDatabase& history,
                 const ItemCallback& onItem,
                 std::wstring& errorMessage);

    static bool IsDestinationOnSource(const std::wstring& destination, const std::wstring& sourceDrive);
    static std::wstring GenerateReport(const std::vector<RecoveryItemResult>& items,
                                       const DriveInfo& source,
                                       const RecoveryOptions& options);

private:
    bool RecoverOne(VolumeReader& reader, const RecoveredFile& file,
                    const RecoveryOptions& options, RecoveryItemResult& out);
};

} // namespace pcdr
