// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <memory>
#include <vector>

#include "musx/xml/XmlInterface.h"

namespace finale_mus_reader {
namespace soundmap {

/// @brief Owned sound-map documents in caller-specified order, shared by imports.
/// @details Retaining the complete XML preserves sound order, MIDI commands, instrument
/// mappings, and extension elements without imposing a particular lookup policy.
using Documents = std::vector<std::unique_ptr<const musx::xml::IXmlDocument>>;

} // namespace soundmap
} // namespace finale_mus_reader
