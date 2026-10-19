// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include "import/support/legacy_mapping.h"

namespace finale_mus_reader {
namespace details {

/// @brief Recovers per-note accidental display changes.
void importAccidentalAlterations(const ImportContext& context);

/// @brief Recovers articulation assignments attached to entries.
void importArticulationAssigns(const ImportContext& context);

/// @brief Recovers staff and system baseline displacements.
void importBaselines(const ImportContext& context);

/// @brief Recovers primary downstem beam adjustments attached to entries.
void importBeamAlterationsDownStem(const ImportContext& context);

/// @brief Recovers primary upstem beam adjustments attached to entries.
void importBeamAlterationsUpStem(const ImportContext& context);

/// @brief Recovers downstem beam extensions attached to entries.
void importBeamExtensionDownStem(const ImportContext& context);

/// @brief Recovers upstem beam extensions attached to entries.
void importBeamExtensionUpStem(const ImportContext& context);

/// @brief Recovers entry beam stub direction masks.
void importBeamStubDirection(const ImportContext& context);

/// @brief Recovers center adjustments for smart shapes spanning measures.
void importCenterShapes(const ImportContext& context);

/// @brief Recovers chord symbols and their measure positions.
void importChordAssigns(const ImportContext& context);

/// @brief Recovers per-clef flat octave placements for custom key signatures.
void importClefOctaveFlats(const ImportContext& context);

/// @brief Recovers per-clef sharp octave placements for custom key signatures.
void importClefOctaveSharps(const ImportContext& context);

/// @brief Recovers cross-staff assignments attached to notes.
void importCrossStaff(const ImportContext& context);

/// @brief Recovers custom downstem shapes attached to entries.
void importCustomDownStem(const ImportContext& context);

/// @brief Recovers custom upstem shapes attached to entries.
void importCustomUpStem(const ImportContext& context);

/// @brief Recovers augmentation-dot offsets and spacing attached to notes.
void importDotAlterations(const ImportContext& context);

/// @brief Recovers custom entry sizes.
void importEntrySize(const ImportContext& context);

/// @brief Recovers source fretboard diagrams.
void importFretboardDiagrams(const ImportContext& context);

/// @brief Recovers GFrameHold objects and the earlier alternate-notation behavior.
void importGFrameHolds(const ImportContext& context);

/// @brief Recovers per-staff independent key and time signatures.
void importIndependentStaffDetails(const ImportContext& context);

/// @brief Recovers accidental strings for custom key symbol lists.
void importKeySymbolListElements(const ImportContext& context);

/// @brief Recovers chorus lyric assignments attached to entries.
void importLyricAssignChorus(const ImportContext& context);

/// @brief Recovers section lyric assignments attached to entries.
void importLyricAssignSection(const ImportContext& context);

/// @brief Recovers verse lyric assignments attached to entries.
void importLyricAssignVerse(const ImportContext& context);

/// @brief Recovers per-entry lyric justification and alignment overrides.
void importLyricEntryInfo(const ImportContext& context);

/// @brief Recovers graphics anchored to a staff and measure.
void importMeasureGraphicAssignments(const ImportContext& context);

/// @brief Recovers individual measure-number placement, visibility, and enclosures.
void importMeasureNumberIndividualPositioning(const ImportContext& context);

/// @brief Recovers text blocks anchored to a staff and measure.
void importMeasureTextAssigns(const ImportContext& context);

/// @brief Recovers per-note notehead display changes.
void importNoteAlterations(const ImportContext& context);

/// @brief Recovers percussion-note type assignments for individual notes.
void importPercussionNoteCodes(const ImportContext& context);

/// @brief Recovers secondary downstem beam adjustments attached to entries.
void importSecondaryBeamAlterationsDownStem(const ImportContext& context);

/// @brief Recovers secondary upstem beam adjustments attached to entries.
void importSecondaryBeamAlterationsUpStem(const ImportContext& context);

/// @brief Recovers secondary beam breaks attached to entries.
void importSecondaryBeamBreak(const ImportContext& context);

/// @brief Recovers staff and document shape-note settings.
void importShapeNotes(const ImportContext& context);

/// @brief Recovers staff-style shape-note settings.
void importShapeNoteStyles(const ImportContext& context);

/// @brief Recovers SmartShape assignments attached to entries.
void importSmartShapeEntryAssigns(const ImportContext& context);

/// @brief Recovers staff groups and their measure ranges.
void importStaffGroups(const ImportContext& context);

/// @brief Recovers per-staff size overrides and updates retained staff systems.
void importStaffSizes(const ImportContext& context);

/// @brief Recovers unbeamed entry stem adjustments.
void importStemAlterations(const ImportContext& context);

/// @brief Recovers entry stem adjustments under beams.
void importStemAlterationsUnderBeam(const ImportContext& context);

/// @brief Imports stored tablature string assignments.
void importTablatureNoteMods(const ImportContext& context);

/// @brief Recovers tie alterations attached to ending notes.
void importTieAlterEnd(const ImportContext& context);

/// @brief Recovers tie alterations attached to starting notes.
void importTieAlterStart(const ImportContext& context);

/// @brief Recovers tuplets attached to entries.
void importTupletDefs(const ImportContext& context);

} // namespace details
} // namespace finale_mus_reader
