// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include "musx/musx.h"
#include <cstdint>

namespace finale_mus_reader {

/// @brief Assigns an unsigned byte font ID and point size from one packed word.
inline void assignPackedFont(
    musx::dom::FontInfo& font, musx::factory::ConstructionContext& construction, std::uint16_t packed, bool sizeInLowByte = false)
{
    const auto low = static_cast<std::uint16_t>(packed & 0xffU);
    const auto high = static_cast<std::uint16_t>(packed >> 8U);
    font.fontId = construction.assignFontId(sizeInLowByte ? high : low);
    font.fontSize = sizeInLowByte ? low : high;
}

} // namespace finale_mus_reader
