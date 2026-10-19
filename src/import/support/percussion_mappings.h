// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "finale_mus_reader/reader.h"
#include "musx/dom/Fundamentals.h"

namespace finale_mus_reader {
namespace percussion {

/// @brief A percussion MIDI map's note type for each MIDI note, keeping the first listed.
using MappingTable = std::unordered_map<std::uint16_t, musx::dom::PercussionNoteTypeId>;

/// @brief Caller-supplied percussion MIDI maps and Finale's legacy map conversion table.
class MappingTables
{
public:
    /// @brief Returns the MIDI map a legacy layout name converts to: the first conversion-table
    /// entry for that name whose file and note-name list were supplied.
    [[nodiscard]] const MappingTable* conversionTarget(std::string_view layoutName) const;

private:
    std::map<std::pair<std::string, std::string>, MappingTable> m_namedTables;
    std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> m_conversions;

    friend MappingTables parseMappingTables(std::span<const PercussionMappingXml>, std::span<const std::uint8_t>, XmlParser);
};

/// @brief Parses Finale MIDI Device Annotation XML buffers through the caller's
/// XML backend.
/// @details Within one filename, the first `NoteNameList` with each name is retained.
[[nodiscard]] MappingTables parseMappingTables(
    std::span<const PercussionMappingXml> documents, std::span<const std::uint8_t> conversionTable, XmlParser parseXml);

} // namespace percussion
} // namespace finale_mus_reader
