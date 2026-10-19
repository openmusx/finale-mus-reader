// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "finale_mus_reader/reader.h"

namespace finale_mus_reader {
namespace container {

struct DecodedBlock
{
    BlockInfo info;
    std::vector<std::uint8_t> data;
};

struct ParsedContainer
{
    explicit ParsedContainer(FormatEpoch epoch)
        : formatEpoch(epoch)
    {}

    FormatEpoch formatEpoch;
    ByteOrder byteOrder = ByteOrder::Unknown;
    std::vector<DecodedBlock> blocks;
    std::size_t trailingByteCount{};
    /// @brief The Coda-banner text region: whatever follows the last record pool.
    /// @details That era keeps its text outside the pool chain, as length-prefixed chunks
    /// rather than in a block of its own, so it has nowhere else to be reported. Empty for
    /// every other epoch, which carries text in an ordinary block.
    std::vector<std::uint8_t> textRegion;
    /// @brief The Coda-banner format flags, header byte 0x81; zero for every other epoch.
    /// @details Each set bit marks data the document already stores in a later form.
    std::uint8_t codaFormatFlags{};
};

/// @brief The byte order a file states for itself, before any framing is read.
/// @return Nothing when the file is neither a signature-bearing nor a Coda-banner document, or
/// is too short to state an order.
[[nodiscard]] std::optional<ByteOrder> statedByteOrder(const std::uint8_t* data, std::size_t size);

/// @brief Decides whether a buffer is a legacy Finale score; see finale_mus_reader::recognize.
[[nodiscard]] MusRecognitionResult recognize(const std::uint8_t* data, std::size_t size);

[[nodiscard]] ParsedContainer parse(const std::uint8_t* data, std::size_t size);

} // namespace container
} // namespace finale_mus_reader
