// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include "import/support/legacy_mapping.h"

namespace finale_mus_reader {
namespace options {

// One importer per musxdom options class, and the registry calls nothing else. Everything a
// class needs -- how many physical layouts it has, which epochs each covers, whether a
// collection must be captured before its scalars are overlaid, and what must be checked once
// they have run -- is decided inside that class's own translation unit.
//
// Individual stages that a test drives on their own are declared in options/test_access.h,
// which no library code includes. A stage no test drives is file-local to its class.

/// @brief Recovers the located scalar fields of options::AccidentalOptions.
/// @details Fields without an established legacy location retain the pinned baseline value.
void importAccidentalOptions(const ImportContext& context);

/// @brief Recovers options::AlternateNotationOptions across all four epochs.
void importAlternateNotationOptions(const ImportContext& context);

/// @brief Recovers options::AugmentationDotOptions.
void importAugmentationDotOptions(const ImportContext& context);

/// @brief Recovers options::BarlineOptions across all four epochs.
void importBarlineOptions(const ImportContext& context);

/// @brief Recovers options::BeamOptions across all four epochs.
/// @details Stored values are overlaid on the pinned defaults; early behavior that differs
/// from those defaults is supplied explicitly.
void importBeamOptions(const ImportContext& context);

/// @brief Recovers options::ChordOptions, including source-owned default fret references.
void importChordOptions(const ImportContext& context);

/// @brief Recovers options::ClefOptions: its definition collection and its scalars.
void importClefOptions(const ImportContext& context);

/// @brief Recovers the located scalar fields of options::FlagOptions.
/// @details Fields without an established legacy location retain the pinned baseline value.
void importFlagOptions(const ImportContext& context);

/// @brief Recovers options::FontOptions and completes the modern type set.
void importFontOptions(const ImportContext& context);

/// @brief Recovers options::GraceNoteOptions.
/// @details The Coda-banner layout has only one located field; the others retain their
/// pinned baseline values and are identified as pinned defaults.
void importGraceNoteOptions(const ImportContext& context);

/// @brief Recovers options::KeySignatureOptions across all four epochs.
/// @details The Coda-banner layout stores a smaller field set than later layouts.
void importKeySignatureOptions(const ImportContext& context);

/// @brief Recovers the located scalar fields of options::LineCurveOptions.
/// @details Fields unavailable in one legacy layout retain the pinned baseline value; MUSX-only
/// enclosure-corner controls are replaced with the corresponding legacy behavior.
void importLineCurveOptions(const ImportContext& context);

/// @brief Recovers options::LyricOptions: its two collections and its scalars.
/// @details Its capture pass and its tables are file-local: no test drives either alone,
/// because both collections and the era assertions around them are exercised through this
/// entry point.
void importLyricOptions(const ImportContext& context);

/// @brief Recovers the located scalar fields of options::MiscOptions.
/// @details Unlocated legacy fields retain the pinned baseline value.
void importMiscOptions(const ImportContext& context);

/// @brief Recovers options::MultimeasureRestOptions across all four epochs.
/// @details Its two stages are file-local: no test drives either one alone, because the class
/// has no collection to build and both are exercised through this entry point.
void importMultimeasureRestOptions(const ImportContext& context);

/// @brief Recovers options::MusicSpacingOptions.
void importMusicSpacingOptions(const ImportContext& context);

/// @brief Recovers options::MusicSymbolOptions.
void importMusicSymbolOptions(const ImportContext& context);

/// @brief Recovers the located scalar fields of options::NoteRestOptions.
/// @details Fields without an established legacy location retain the pinned baseline value;
/// Coda manual positioning is supplied as legacy behavior.
void importNoteRestOptions(const ImportContext& context);

/// @brief Recovers both score and parts page-format settings across all four epochs.
/// @details Coda-banner documents store one set, which populates both modern contained
/// objects. Adjustment scope retains the seeded document default; collision avoidance is
/// recovered where stored and otherwise supplied from established legacy behavior.
void importPageFormatOptions(const ImportContext& context);

/// @brief Recovers options::PianoBraceBracketOptions.
void importPianoBraceBracketOptions(const ImportContext& context);

/// @brief Recovers the legacy scalar fields of options::RepeatOptions.
/// @details The document-level staff-list selection has no located legacy field and remains
/// seeded from the reference document.
void importRepeatOptions(const ImportContext& context);

/// @brief Recovers the located scalar and slur-contour fields of
/// options::SmartShapeOptions.
void importSmartShapeOptions(const ImportContext& context);

/// @brief Recovers the located default staff- and group-name positioning fields.
/// @details Staff-spacing fields and name-position fields without an established legacy
/// location retain their pinned baseline values.
void importStaffOptions(const ImportContext& context);

/// @brief Recovers options::StemOptions, whose connections are a source-owned collection.
void importStemOptions(const ImportContext& context);

/// @brief Recovers the scalar fields of options::TextOptions across all four epochs.
/// @details The accidental symbol inserts are not implemented yet and keep the pinned
/// baseline's values; see the note at the end of the importer.
void importTextOptions(const ImportContext& context);

/// @brief Recovers options::TieOptions scalars, connection styles, and control styles.
/// @details Unlocated Coda-banner fields retain the pinned baseline values.
void importTieOptions(const ImportContext& context);

/// @brief Recovers the located score and linked-parts time-signature options.
/// @details Coda-banner documents retain pinned defaults for fields absent from that layout.
void importTimeSignatureOptions(const ImportContext& context);

/// @brief Recovers options::TupletOptions from stored preferences and source-era behavior.
/// @details Fields absent from a source layout retain the pinned baseline unless that layout's
/// behavior differs from the baseline.
void importTupletOptions(const ImportContext& context);

} // namespace options
} // namespace finale_mus_reader
