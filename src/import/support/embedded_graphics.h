// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include "container/mus_container.h"
#include "finale_mus_reader/reader.h"
#include "musx/dom/Document.h"

namespace finale_mus_reader {

/// @brief Recovers embedded graphic files from the container's stored graphic block.
[[nodiscard]] musx::dom::EmbeddedGraphicsMap recoverEmbeddedGraphics(const container::ParsedContainer& parsed, ImportReport& report);

} // namespace finale_mus_reader
