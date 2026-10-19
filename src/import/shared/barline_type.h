// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

#include "musx/dom/Others.h"

namespace finale_mus_reader {

/// @brief Translates barline codes shared by measure and staff-group records.
[[nodiscard]] constexpr musx::dom::others::Measure::BarlineType barlineTypeOf(std::uint16_t code)
{
    using BarlineType = musx::dom::others::Measure::BarlineType;
    switch (code) {
    case 1: return BarlineType::Normal;
    case 2: return BarlineType::Double;
    case 3: return BarlineType::Dashed;
    case 4: return BarlineType::Solid;
    case 5: return BarlineType::Final;
    case 6: return BarlineType::Tick;
    case 14: return BarlineType::Custom;
    case 15: return BarlineType::OptionsDefault;
    default: return BarlineType::None;
    }
}

} // namespace finale_mus_reader
