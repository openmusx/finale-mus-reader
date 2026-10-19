// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <limits>
#include <span>
#include <string>

#include "import/support/legacy_mapping.h"

namespace finale_mus_reader::coda_text {

constexpr std::uint32_t rowsPerBlock = 4;

struct StyleFontSize
{
    std::uint16_t font;
    std::uint16_t size;
};

inline StyleFontSize styleFontSize(const records::LegacyRow& style, FormatEpoch epoch)
{
    const auto packed = static_cast<std::uint16_t>(style.words[2]);
    const auto high = static_cast<std::uint16_t>(packed >> 8U);
    const auto low = static_cast<std::uint16_t>(packed & 0x00ffU);
    return epoch == FormatEpoch::CodaBanner ? StyleFontSize{high, low} : StyleFontSize{low, high};
}

/// @brief Reads the characters paired with one Coda HS style incidence.
inline std::string readBlockCharacters(const records::LegacyRowPool& pool, std::span<const records::LegacyRow> textRows, std::uint32_t styleIncidence)
{
    const auto first = static_cast<std::uint64_t>(styleIncidence) * rowsPerBlock;
    if (first > (std::numeric_limits<std::uint32_t>::max)()) {
        return {};
    }
    return readRowText(pool, textRows, static_cast<std::uint32_t>(first), rowsPerBlock);
}

} // namespace finale_mus_reader::coda_text
