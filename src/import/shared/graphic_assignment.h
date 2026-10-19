// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

// Shared layout operations for page, shape, and measure graphic assignments.

#include <cstddef>
#include <cstdint>
#include <span>

#include "musx/musx.h"

namespace finale_mus_reader {

inline constexpr std::size_t graphicAssignmentWordCount = 18;

template <typename Target>
void populateGraphicAssignmentCommon(Target& target, std::span<const std::int16_t> words)
{
    target.version = static_cast<std::uint16_t>(words[0]);
    target.left = words[1];
    target.bottom = words[2];
    target.width = words[3];
    target.height = words[4];
    target.fDescId = static_cast<musx::dom::Cmper>(words[5]);
    target.hidden = (static_cast<std::uint16_t>(words[7]) & 0x0010U) != 0;
    target.savedRecord = words[11] != 0;
    target.origWidth = words[12];
    target.origHeight = words[13];
    target.graphicCmper = static_cast<musx::dom::Cmper>(words[17]);
}

template <bool HasPositionFrom, typename Target>
void populateGraphicAssignmentPosition(Target& target, std::uint16_t packed)
{
    using H = typename Target::HorizontalAlignment;
    using V = typename Target::VerticalAlignment;
    switch (packed & 0x0007U) {
    case 0x0001: target.hAlign = H::Left; break;
    case 0x0002: target.hAlign = H::Right; break;
    case 0x0004: target.hAlign = H::Center; break;
    default: break;
    }
    switch (packed & 0x0038U) {
    case 0x0008: target.vAlign = V::Top; break;
    case 0x0010: target.vAlign = V::Bottom; break;
    case 0x0020: target.vAlign = V::Center; break;
    default: break;
    }
    if constexpr (HasPositionFrom) {
        using P = typename Target::PositionFrom;
        if ((packed & 0x0080U) != 0) {
            target.posFrom = P::PageEdge;
        } else if ((packed & 0x0040U) != 0) {
            target.posFrom = P::Margins;
        }
    }
    target.fixedPerc = (packed & 0x0100U) != 0;
}

} // namespace finale_mus_reader
