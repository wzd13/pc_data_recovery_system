#pragma once

#include "carving/SignatureDatabase.hpp"
#include "drive/VolumeReader.hpp"
#include "scanner/ScanProgress.hpp"
#include "utils/Types.hpp"

namespace pcdr {

class RawCarver {
public:
    explicit RawCarver(SignatureDatabase db = {});

    bool Carve(VolumeReader& reader,
               const DriveInfo& drive,
               const ScanOptions& options,
               ScanProgress& progress,
               const FileFoundCallback& onFile);

private:
    SignatureDatabase db_;
    static bool MatchAt(const std::uint8_t* data, std::size_t size, const FileSignature& sig);
    static std::uint64_t EstimateLength(const std::uint8_t* data, std::size_t size,
                                        const FileSignature& sig, std::uint64_t maxSize);
};

} // namespace pcdr
