// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT
#pragma once

#include "musx/musx.h"
#include <cstdint>
#include <span>

namespace finale_mus_reader {

// Reads the six-word enclosure shared by pooled and contained legacy objects.
// The caller validates the payload and supplies six unsigned words in host byte order.
bool decodeLegacyEnclosure(musx::dom::others::Enclosure& target, std::span<const std::uint16_t, 6> words);

} // namespace finale_mus_reader
