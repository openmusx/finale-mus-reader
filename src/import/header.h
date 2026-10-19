// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "container/mus_container.h"
#include "finale_mus_reader/reader.h"
#include "musx/dom/Document.h"

namespace finale_mus_reader {
namespace header {

[[nodiscard]] bool hasBanner(const std::uint8_t* data, std::size_t size);

/// @brief Fills in banner, product, platform, and version from the 0x200 header.
/// @param report Its @ref ImportReport::byteOrder, when already known, is the order the header's
/// 32-bit fields are read in. When it is `Unknown` it is set from the order the header states.
void describeSourceIdentity(const std::uint8_t* data, std::size_t size, ImportReport& report);

[[nodiscard]] musx::dom::header::HeaderPtr recover(const std::uint8_t* data, std::size_t size, const ImportReport& report);

} // namespace header
} // namespace finale_mus_reader
