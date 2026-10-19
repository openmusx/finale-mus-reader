// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include "import/support/enigma_text.h"
#include "import/support/legacy_mapping.h"

namespace finale_mus_reader {
namespace texts {

void importLaterTextPool(const ImportContext& context);
void importHeaderFileInfoTexts(const ImportContext& context);
void importCodaStoredTexts(const ImportContext& context);

} // namespace texts
} // namespace finale_mus_reader
