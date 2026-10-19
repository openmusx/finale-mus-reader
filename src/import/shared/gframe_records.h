// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>

#include "records/legacy_record_index.h"

namespace finale_mus_reader::details::gframe {

/// @brief The fixed-row detail that holds one staff's frame in one measure.
inline constexpr auto tag = records::packTag("GF");

/// @brief The flags word before Finale 98.
inline constexpr std::size_t earlyFlagsSlot = 4;

/// @brief The word that holds the frame's clef index, or its clef list's comparator when the
/// flags carry @ref clefListBit, in a Coda-banner frame.
inline constexpr std::size_t codaClefSlot = 1;

/// @brief Marks a frame whose clef word names a clef list.
inline constexpr std::uint16_t clefListBit = 0x0400;

} // namespace finale_mus_reader::details::gframe
