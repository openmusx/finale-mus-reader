// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include "import/support/legacy_mapping.h"

namespace finale_mus_reader {
namespace texts {

/// @brief Recovers the document's supported text classes from their legacy physical stores.
/// @details The text family shares framing and fallback state across several musxdom classes,
/// so one importer owns the family and keeps its era-specific storage private.
void importTexts(const ImportContext& context);

} // namespace texts
} // namespace finale_mus_reader
