// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <set>
#include <utility>

#include "import/support/legacy_mapping.h"
#include "musx/musx.h"

namespace finale_mus_reader {

/// @brief Part and shape numbers of stored SmartShapes, keyed `{partId, shapeNum}`.
using SmartShapeKeys = std::set<std::pair<musx::dom::Cmper, musx::dom::Cmper>>;

/// @brief The stored SmartShapes the reader does not recover, as Finale's upgrade does not.
/// @details An entry-based shape that names an endpoint entry the file does not store cannot be
/// placed there by musxdom. The reader skips such a shape when Finale drops it: when Finale's file
/// maintenance deletes it because none of its marks is on an existing entry, or when its stored
/// entry lies outside the staff it names. A shape Finale keeps is recovered with its missing
/// endpoint. A part's copy follows the score's decision when the score's record can be read.
///
/// The staff test reads the document's frames, so the decision is available only to deferred
/// steps. It is computed once and shared, so every importer of an object that names a shape
/// agrees with the shape importer.
[[nodiscard]] const SmartShapeKeys& findSkippedSmartShapes(const ImportContext& context);

/// @brief Whether an object in @p partId that names @p shapeNum names a skipped shape.
/// @details A skipped score shape takes every part's copy with it, and a part without its own
/// copy resolves to the score's, so either key skips the object. A part's skipped copy does not
/// skip the score's objects, whose shape the reader keeps.
[[nodiscard]] inline bool namesSkippedSmartShape(const SmartShapeKeys& skipped, musx::dom::Cmper partId, musx::dom::Cmper shapeNum)
{
    return skipped.contains({partId, shapeNum}) || skipped.contains({musx::dom::SCORE_PARTID, shapeNum});
}

} // namespace finale_mus_reader
