// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>

#include "container/mus_container.h"
#include "finale_mus_reader/reader.h"
#include "import/support/deferred_order.h"
#include "musx/dom/Document.h"

namespace finale_mus_reader {

namespace detail {
struct ReaderResources;
} // namespace detail

/// @brief Reports whether the input begins with the legacy ENIGMA banner.
[[nodiscard]] bool hasBanner(const std::uint8_t* data, std::size_t size);

/// @brief Records the banner-derived source identity of the input in @p report.
/// @details Leaves the report untouched when the input carries no complete banner.
void describeSourceIdentity(const std::uint8_t* data, std::size_t size, ImportReport& report);

/// @brief Constructs the imported document for a parsed legacy MUS container.
/// @details This is the reader's own document factory. It begins a musxdom construction
/// session, seeds the pinned Finale 27 defaults, recovers the header, overlays every
/// confidently decoded legacy value, and finishes the session so musxdom validates the
/// completed document once. @p report must already carry the classified byte order and
/// source platform, and it receives the per-field origin of every supported value.
/// @p deferredOrder is a testing aid; the reader always uses the default.
[[nodiscard]] musx::dom::DocumentPtr createDocument(const container::ParsedContainer& parsed, const std::uint8_t* data, std::size_t size,
    const std::optional<std::filesystem::path>& sourcePath, const detail::ReaderResources& resources, XmlParser parseXml,
    DocumentParser parseDocument, ImportReport& report, DeferredOrder deferredOrder = DeferredOrder::AsRegistered);

} // namespace finale_mus_reader
