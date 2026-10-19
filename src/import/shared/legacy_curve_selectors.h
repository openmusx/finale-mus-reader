// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <string_view>

namespace finale_mus_reader {
namespace options {
namespace legacy_curve {

inline constexpr std::string_view slurThicknessTag = "50";
inline constexpr std::string_view engraverSlurTag = "51";

consteval std::uint16_t numericSelector(std::string_view tag)
{
    return static_cast<std::uint16_t>((tag[0] - '0') * 10 + tag[1] - '0');
}

inline constexpr std::uint16_t slurThicknessSelector = numericSelector(slurThicknessTag);
inline constexpr std::uint16_t engraverSlurSelector = numericSelector(engraverSlurTag);

} // namespace legacy_curve
} // namespace options
} // namespace finale_mus_reader
