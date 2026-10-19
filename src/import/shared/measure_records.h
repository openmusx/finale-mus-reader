// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include "records/legacy_record_index.h"

#include <cstddef>
#include <cstdint>

namespace finale_mus_reader {

/// @brief The measure record's selector before the zlib epoch and its class in it.
inline constexpr records::LegacyTag measureTag = records::packTag("MS");
inline constexpr records::LegacyTag measureClass = 0x00b0;

/// @brief The measure record's second flag word.
inline constexpr std::size_t measureSecondaryFlagsSlot = 5;

/// @brief The second flag word's measure-has-expression bit, in the layout Finale 3.0 and later use.
inline constexpr std::uint16_t secondaryHasExpressionMask = 0x4000;

/// @brief The measure record's third flag word, present only once the record has twelve words.
inline constexpr std::size_t measureExtendedFlagsSlot = 8;

/// @brief The third flag word's measure-has-chord bit.
inline constexpr std::uint16_t extendedHasChordMask = 0x0008;

} // namespace finale_mus_reader
