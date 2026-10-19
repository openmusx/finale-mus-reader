// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace smart_shape_adjustment_flags {

constexpr std::uint8_t activeBit = 14;

inline bool active(std::int64_t flags)
{
    return (flags & (std::uint16_t(1) << activeBit)) != 0;
}

inline musx::dom::smartshape::DirectionType direction(std::int64_t flags)
{
    using Direction = musx::dom::smartshape::DirectionType;
    return (flags & 0x0200U) ? Direction::Over : (flags & 0x0100U) ? Direction::Under : Direction::None;
}

inline musx::dom::smartshape::EntryConnectionType entryConnection(std::int64_t flags)
{
    return static_cast<musx::dom::smartshape::EntryConnectionType>(flags & 0x00ffU);
}

} // namespace smart_shape_adjustment_flags
} // namespace finale_mus_reader
