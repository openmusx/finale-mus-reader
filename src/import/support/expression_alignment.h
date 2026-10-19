// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once
#include "musx/musx.h"
#include <optional>

namespace finale_mus_reader {

inline std::optional<musx::dom::AlignJustify> expressionJustification(std::uint16_t stored)
{
    using A = musx::dom::AlignJustify;
    switch (stored) {
    case 0: return A::Left;
    case 1: return A::Center;
    case 2: return A::Right;
    default: return std::nullopt;
    }
}

inline std::optional<musx::dom::others::HorizontalMeasExprAlign> expressionHorizontalAlignment(std::uint16_t stored)
{
    using A = musx::dom::others::HorizontalMeasExprAlign;
    switch (stored) {
    case 0: return A::LeftBarline;
    case 1: return A::StartTimeSig;
    case 2: return A::AfterClefKeyTime;
    case 3: return A::Manual;
    case 4: return A::CenterOverBarlines;
    case 5: return A::CenterOverMusic;
    case 6: return A::RightBarline;
    case 7: return A::StartOfMusic;
    case 9: return A::LeftOfAllNoteheads;
    case 10: return A::Stem;
    case 11: return A::CenterPrimaryNotehead;
    case 12: return A::CenterAllNoteheads;
    case 13: return A::LeftOfPrimaryNotehead;
    case 14: return A::RightOfAllNoteheads;
    default: return std::nullopt;
    }
}

inline std::optional<musx::dom::others::VerticalMeasExprAlign> expressionVerticalAlignment(std::uint16_t stored)
{
    using A = musx::dom::others::VerticalMeasExprAlign;
    switch (stored) {
    case 0: return A::AboveStaff;
    case 1: return A::BelowStaff;
    case 2: return A::Manual;
    case 3: return A::RefLine;
    case 4: return A::TopNote;
    case 5: return A::BottomNote;
    case 6: return A::AboveEntry;
    case 7: return A::BelowEntry;
    case 8: return A::AboveStaffOrEntry;
    case 9: return A::BelowStaffOrEntry;
    default: return std::nullopt;
    }
}

} // namespace finale_mus_reader
