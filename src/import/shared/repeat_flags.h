// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <array>

#include "musx/dom/Others.h"

namespace finale_mus_reader::others::repeat {

inline constexpr std::array actions{musx::dom::others::RepeatActionType::JumpAuto, musx::dom::others::RepeatActionType::JumpAbsolute,
    musx::dom::others::RepeatActionType::JumpRelative, musx::dom::others::RepeatActionType::JumpToMark, musx::dom::others::RepeatActionType::Stop,
    musx::dom::others::RepeatActionType::NoJump};

inline constexpr std::array triggers{
    musx::dom::others::RepeatTriggerType::Always, musx::dom::others::RepeatTriggerType::OnPass, musx::dom::others::RepeatTriggerType::UntilPass};

} // namespace finale_mus_reader::others::repeat
