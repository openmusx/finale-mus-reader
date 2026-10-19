// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <stdexcept>

#include "import/support/legacy_mapping.h"
#include "musx/musx.h"

namespace finale_mus_reader {

inline const musx::dom::others::Staff& finale27StaffDefaults(const ImportContext& context)
{
    const auto result = context.referenceDocument->getOthers()->get<musx::dom::others::Staff>(musx::dom::SCORE_PARTID, 1);
    if (!result) {
        throw std::logic_error("Finale 27 reference is missing its standard Staff");
    }
    return *result;
}

inline int finale27NoteheadFontSize(const ImportContext& context)
{
    const auto font =
        musx::dom::options::FontOptions::getFontInfoOrNull(context.referenceDocument, musx::dom::options::FontOptions::FontType::Noteheads);
    if (!font) {
        throw std::logic_error("Finale 27 reference is missing its notehead font default");
    }
    return font->fontSize;
}

} // namespace finale_mus_reader
