// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "import/support/legacy_mapping.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace percussion {

/// @brief Identifies one stored percussion-layout assignment: a staff's DS record or a Staff
/// Style's FY record, in one source part.
struct LegacyAssignmentKey
{
    musx::dom::Cmper partId{};
    bool style{};
    musx::dom::Cmper cmper{};

    auto operator<=>(const LegacyAssignmentKey&) const = default;
};

/// @brief One note of an upgraded legacy percussion layout.
struct LegacyLayoutNote
{
    musx::dom::PercussionNoteTypeId type{};
    std::int16_t staffPosition{};
    std::uint16_t closedNotehead{}; ///< The stored notehead character byte.
    std::uint16_t openNotehead{};   ///< The stored half-note character byte, also used for longer values.
    std::uint16_t playbackKey{};    ///< The stored output MIDI note that selected the note type.
    std::optional<records::LegacyRow> row; ///< The selected row; absent when the selection names no stored row.

    /// @brief Whether two notes have the same type, staff position, and noteheads.
    [[nodiscard]] bool sameAs(const LegacyLayoutNote& other) const;
};

/// @brief How the upgrade treats one stored assignment.
struct LegacyAssignment
{
    RecordFamilySource source;
    records::LegacyRow row; ///< The first stored row, which owns the assignment.
    musx::dom::Cmper storedLayoutId{};
    /// @brief The upgraded layout comparator; disengaged when the upgrade deletes the assignment.
    std::optional<musx::dom::Cmper> layoutId;
    /// @brief Note types by input MIDI key, for an assignment whose selection was converted.
    std::map<int, musx::dom::PercussionNoteTypeId> inputTypes;
    /// @brief The layout constructed for this assignment before identical layouts merged.
    std::vector<LegacyLayoutNote> convertedNotes;
};

/// @brief The complete upgrade of a document's legacy percussion layouts.
struct LegacyConversion
{
    std::map<LegacyAssignmentKey, LegacyAssignment> assignments;
    /// @brief Upgraded layouts by their final comparator.
    std::map<musx::dom::Cmper, std::vector<LegacyLayoutNote>> layouts;
    /// @brief Note-type replacements applied to a final layout by its conversion-table entry.
    std::map<musx::dom::Cmper, std::map<musx::dom::PercussionNoteTypeId, musx::dom::PercussionNoteTypeId>> retypes;
};

/// @brief Whether a source stores legacy percussion layouts that the upgrade converts.
[[nodiscard]] bool storesLegacyLayouts(const ImportContext& context);

/// @brief Returns the document's legacy percussion upgrade, computing it on first use.
/// @details The conversion reads the Staff, StaffStyle, and Scroll View StaffUsed objects, so a
/// caller must run in a deferred stage no earlier than @ref DeferredStage::NormalizeStoredValues.
[[nodiscard]] const LegacyConversion& legacyConversion(const ImportContext& context);

/// @brief Returns the MIDI note Finale's default percussion MIDI map assigns to a note type.
/// @details A duplicate type falls back to its base type, then to each parent type in turn.
/// Disengaged when no type in that chain has a General MIDI note.
[[nodiscard]] std::optional<std::uint8_t> defaultMidiNote(musx::dom::PercussionNoteTypeId type);

} // namespace percussion
} // namespace finale_mus_reader
