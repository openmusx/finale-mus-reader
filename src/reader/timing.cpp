// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "reader/timing.h"

#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
#include <stdexcept>
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)

namespace finale_mus_reader {
namespace timing {

std::string_view phaseName(Phase phase)
{
#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
    switch (phase) {
    case Phase::FileIo: return "file_io";
    case Phase::ContainerParse: return "container_parse";
    case Phase::SourceReport: return "source_report";
    case Phase::EmbeddedGraphics: return "embedded_graphics";
    case Phase::ConstructionBegin: return "construction_begin";
    case Phase::DefaultsParse: return "defaults_parse";
    case Phase::DefaultsSeed: return "defaults_seed";
    case Phase::HeaderRecovery: return "header_recovery";
    case Phase::MacSymbolFonts: return "mac_symbol_fonts";
    case Phase::RecordIndex: return "record_index";
    case Phase::ImportAcciAmountFlats: return "acci_amount_flats";
    case Phase::ImportAcciAmountSharps: return "acci_amount_sharps";
    case Phase::ImportAccidentalAlterations: return "accidental_alterations";
    case Phase::ImportAccidentalOptions: return "accidental_options";
    case Phase::ImportAcciOrderFlats: return "acci_order_flats";
    case Phase::ImportAcciOrderSharps: return "acci_order_sharps";
    case Phase::ImportAlternateNotationOptions: return "alternate_notation_options";
    case Phase::ImportArticulationAssigns: return "articulation_assigns";
    case Phase::ImportArticulationDefs: return "articulation_defs";
    case Phase::ImportAugmentationDotOptions: return "augmentation_dot_options";
    case Phase::ImportBarlineOptions: return "barline_options";
    case Phase::ImportBaselines: return "baselines";
    case Phase::ImportBeamAlterationsDownStem: return "beam_alterations_down_stem";
    case Phase::ImportBeamAlterationsUpStem: return "beam_alterations_up_stem";
    case Phase::ImportBeamExtensionDownStem: return "beam_extension_down_stem";
    case Phase::ImportBeamExtensionUpStem: return "beam_extension_up_stem";
    case Phase::ImportBeamOptions: return "beam_options";
    case Phase::ImportBeamStubDirection: return "beam_stub_direction";
    case Phase::ImportBeatChartElements: return "beat_chart_elements";
    case Phase::ImportBookmarks: return "bookmarks";
    case Phase::ImportCenterShapes: return "center_shapes";
    case Phase::ImportChordAssigns: return "chord_assigns";
    case Phase::ImportChordOptions: return "chord_options";
    case Phase::ImportChordSuffixElements: return "chord_suffix_elements";
    case Phase::ImportChordSuffixPlayback: return "chord_suffix_playback";
    case Phase::ImportClefLists: return "clef_lists";
    case Phase::ImportClefOctaveFlats: return "clef_octave_flats";
    case Phase::ImportClefOctaveSharps: return "clef_octave_sharps";
    case Phase::ImportClefOptions: return "clef_options";
    case Phase::ImportCrossStaff: return "cross_staff";
    case Phase::ImportCustomDownStem: return "custom_down_stem";
    case Phase::ImportCustomUpStem: return "custom_up_stem";
    case Phase::ImportDotAlterations: return "dot_alterations";
    case Phase::ImportDrumStaff: return "drum_staff";
    case Phase::ImportEntries: return "entries";
    case Phase::ImportEntrySize: return "entry_size";
    case Phase::ImportFilePath: return "file_path";
    case Phase::ImportFlagOptions: return "flag_options";
    case Phase::ImportFontDefinitions: return "font_definitions";
    case Phase::ImportFontOptions: return "font_options";
    case Phase::ImportFrames: return "frames";
    case Phase::ImportFretboardDiagrams: return "fretboard_diagrams";
    case Phase::ImportFretboardGroups: return "fretboard_groups";
    case Phase::ImportFretboardStyles: return "fretboard_styles";
    case Phase::ImportFretInstruments: return "fret_instruments";
    case Phase::ImportGFrameHolds: return "gframe_holds";
    case Phase::ImportGraceNoteOptions: return "grace_note_options";
    case Phase::ImportIndependentStaffDetails: return "independent_staff_details";
    case Phase::ImportKeyAttributes: return "key_attributes";
    case Phase::ImportKeyFormats: return "key_formats";
    case Phase::ImportKeyMapArrays: return "key_map_arrays";
    case Phase::ImportKeySignatureOptions: return "key_signature_options";
    case Phase::ImportKeySymbolListElements: return "key_symbol_list_elements";
    case Phase::ImportLayerAttributes: return "layer_atts";
    case Phase::ImportLineCurveOptions: return "line_curve_options";
    case Phase::ImportLyricAssignChorus: return "lyric_assign_chorus";
    case Phase::ImportLyricAssignSection: return "lyric_assign_section";
    case Phase::ImportLyricAssignVerse: return "lyric_assign_verse";
    case Phase::ImportLyricEntryInfo: return "lyric_entry_info";
    case Phase::ImportLyricOptions: return "lyric_options";
    case Phase::ImportMarkingCategories: return "marking_categories";
    case Phase::ImportMeasureExprAssigns: return "measure_expr_assigns";
    case Phase::ImportMeasureGraphicAssignments: return "meas_graphic_assigns";
    case Phase::ImportMeasureNumberIndividualPositioning: return "measure_number_individual_positioning";
    case Phase::ImportMeasureNumberRegions: return "measure_number_regions";
    case Phase::ImportMeasures: return "measures";
    case Phase::ImportMeasureTextAssigns: return "meas_text_assigns";
    case Phase::ImportMiscOptions: return "misc_options";
    case Phase::ImportMultimeasureRestOptions: return "mmrest_options";
    case Phase::ImportMultimeasureRests: return "mmrests";
    case Phase::ImportMultiStaffGroupIds: return "multi_staff_group_ids";
    case Phase::ImportMultiStaffInstrumentGroups: return "multi_staff_instrument_groups";
    case Phase::ImportMusicSpacingOptions: return "spacing_options";
    case Phase::ImportMusicSymbolOptions: return "music_symbol_options";
    case Phase::ImportNamePositionAbbreviated: return "name_position_abbreviated";
    case Phase::ImportNamePositionFull: return "name_position_full";
    case Phase::ImportNamePositionStyleAbbreviated: return "name_position_style_abbreviated";
    case Phase::ImportNamePositionStyleFull: return "name_position_style_full";
    case Phase::ImportNoteAlterations: return "note_alterations";
    case Phase::ImportNoteRestOptions: return "note_rest_options";
    case Phase::ImportPageFormatOptions: return "page_format_options";
    case Phase::ImportPageGraphicAssignments: return "page_graphic_assigns";
    case Phase::ImportPages: return "pages";
    case Phase::ImportPageTextAssigns: return "page_text_assigns";
    case Phase::ImportPartDefinitions: return "part_defs";
    case Phase::ImportPartGlobals: return "part_globals";
    case Phase::ImportPartVoicing: return "part_voicing";
    case Phase::ImportPercussionNoteCodes: return "percussion_note_codes";
    case Phase::ImportPercussionNoteInfo: return "percussion_note_info";
    case Phase::ImportPianoBraceBracketOptions: return "piano_brace_bracket_options";
    case Phase::ImportPlaybackRouteNames: return "playback_route_names";
    case Phase::ImportPlaybackRoutes: return "playback_routes";
    case Phase::ImportRepeatBackIndividualPositioning: return "repeat_back_individual_positioning";
    case Phase::ImportRepeatBacks: return "repeat_backs";
    case Phase::ImportRepeatEndingStartIndividualPositioning: return "repeat_ending_start_individual_positioning";
    case Phase::ImportRepeatEndingStarts: return "repeat_ending_starts";
    case Phase::ImportRepeatEndingTextIndividualPositioning: return "repeat_ending_text_individual_positioning";
    case Phase::ImportRepeatEndingTexts: return "repeat_ending_texts";
    case Phase::ImportRepeatOptions: return "repeat_options";
    case Phase::ImportRepeatPassLists: return "repeat_pass_lists";
    case Phase::ImportSecondaryBeamAlterationsDownStem: return "secondary_beam_alterations_down_stem";
    case Phase::ImportSecondaryBeamAlterationsUpStem: return "secondary_beam_alterations_up_stem";
    case Phase::ImportSecondaryBeamBreak: return "secondary_beam_break";
    case Phase::ImportShapeDefinitions: return "shape_definitions";
    case Phase::ImportShapeExpressionDefs: return "shape_expression_defs";
    case Phase::ImportShapeGraphicAssignments: return "shape_graphic_assigns";
    case Phase::ImportShapeNotes: return "shape_notes";
    case Phase::ImportShapeNoteStyles: return "shape_note_styles";
    case Phase::ImportSmartShapeCustomLines: return "ss_line_styles";
    case Phase::ImportSmartShapeEntryAssigns: return "smart_shape_entry_assigns";
    case Phase::ImportSmartShapeMeasureAssigns: return "smart_shape_measure_assigns";
    case Phase::ImportSmartShapeOptions: return "smart_shape_options";
    case Phase::ImportSmartShapes: return "smart_shapes";
    case Phase::ImportSplitMeasures: return "split_measures";
    case Phase::ImportStaff: return "staff";
    case Phase::ImportStaffGroups: return "staff_groups";
    case Phase::ImportStaffLists: return "staff_lists";
    case Phase::ImportStaffOptions: return "staff_options";
    case Phase::ImportStaffPlayData: return "staff_play_data";
    case Phase::ImportStaffSizes: return "staff_sizes";
    case Phase::ImportStaffStyleAssignments: return "staff_style_assignments";
    case Phase::ImportStaffStyles: return "staff_styles";
    case Phase::ImportStaffSystems: return "staff_systems";
    case Phase::ImportStaffUsed: return "staff_used";
    case Phase::ImportStemAlterations: return "stem_alterations";
    case Phase::ImportStemAlterationsUnderBeam: return "stem_alterations_under_beam";
    case Phase::ImportStemOptions: return "stem_options";
    case Phase::ImportSystemLocks: return "system_locks";
    case Phase::ImportTablatureNoteMods: return "tablature_note_mods";
    case Phase::ImportTempoChanges: return "tempo_changes";
    case Phase::ImportTextBlocks: return "text_blocks";
    case Phase::ImportTextExpressionDefs: return "text_expression_defs";
    case Phase::ImportTextExpressionEnclosures: return "text_expression_enclosures";
    case Phase::ImportTextOptions: return "text_options";
    case Phase::ImportTextRepeatAssigns: return "text_repeat_assigns";
    case Phase::ImportTextRepeatDefs: return "text_repeat_defs";
    case Phase::ImportTextRepeatEnclosures: return "text_repeat_enclosures";
    case Phase::ImportTextRepeatIndividualPositioning: return "text_repeat_individual_positioning";
    case Phase::ImportTextRepeatTexts: return "text_repeat_texts";
    case Phase::ImportTexts: return "texts";
    case Phase::ImportTieAlterEnds: return "tie_alter_ends";
    case Phase::ImportTieAlterStarts: return "tie_alter_starts";
    case Phase::ImportTieOptions: return "tie_options";
    case Phase::ImportTimeCompositeLowers: return "time_composite_lowers";
    case Phase::ImportTimeCompositeUppers: return "time_composite_uppers";
    case Phase::ImportTimeSignatureOptions: return "time_signature_options";
    case Phase::ImportTonalCenterFlats: return "tonal_center_flats";
    case Phase::ImportTonalCenterSharps: return "tonal_center_sharps";
    case Phase::ImportTupletDefs: return "tuplet_defs";
    case Phase::ImportTupletOptions: return "tuplet_options";
    case Phase::TextLaterPool: return "text_later_pool";
    case Phase::TextPoolFraming: return "text_pool_framing";
    case Phase::TextConversion: return "text_conversion";
    case Phase::TextLiteralEncoding: return "text_literal_encoding";
    case Phase::TextFontState: return "text_font_state";
    case Phase::TextObjectConstruction: return "text_object_construction";
    case Phase::TextReportConstruction: return "text_report_construction";
    case Phase::TextDomInsertion: return "text_dom_insertion";
    case Phase::TextHeaderFileInfo: return "text_header_file_info";
    case Phase::TextCodaStored: return "text_coda_stored";
    case Phase::DeferredReferences: return "deferred_references";
    case Phase::ConstructionFinish: return "construction_finish";
    case Phase::DiagnosticDelivery: return "diagnostic_delivery";
    case Phase::Count: break;
    }
    throw std::logic_error("invalid reader timing phase");
#else
    static_cast<void>(phase);
    return {};
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
}

std::string_view counterName(Counter counter)
{
#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
    switch (counter) {
    case Counter::TextRecords: return "text_records";
    case Counter::TextRecordBytes: return "text_record_bytes";
    case Counter::TextCommands: return "text_commands";
    case Counter::TextLiteralRuns: return "text_literal_runs";
    case Counter::TextLiteralBytes: return "text_literal_bytes";
    case Counter::TextCacheHits: return "text_cache_hits";
    case Counter::TextCacheMisses: return "text_cache_misses";
    case Counter::TextCacheAvoidedBytes: return "text_cache_avoided_bytes";
    case Counter::TextFontResolutionCacheHits: return "text_font_resolution_cache_hits";
    case Counter::TextFontResolutionCacheMisses: return "text_font_resolution_cache_misses";
    case Counter::TextInitialFontCacheHits: return "text_initial_font_cache_hits";
    case Counter::TextInitialFontCacheMisses: return "text_initial_font_cache_misses";
    case Counter::Count: break;
    }
    throw std::logic_error("invalid reader timing counter");
#else
    static_cast<void>(counter);
    return {};
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
}

std::string_view containerCandidateName(ContainerCandidate candidate)
{
#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
    switch (candidate) {
    case ContainerCandidate::Uncompressed: return "uncompressed";
    case ContainerCandidate::Dcl: return "dcl";
    case ContainerCandidate::Zlib: return "zlib";
    }
    throw std::logic_error("invalid container timing candidate");
#else
    static_cast<void>(candidate);
    return {};
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
}

std::string_view containerAttemptResultName(ContainerAttemptResult result)
{
#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
    switch (result) {
    case ContainerAttemptResult::Accepted: return "accepted";
    case ContainerAttemptResult::InputBounds: return "input_bounds";
    case ContainerAttemptResult::FirstType: return "first_type";
    case ContainerAttemptResult::Framing: return "framing";
    case ContainerAttemptResult::Inflate: return "inflate";
    case ContainerAttemptResult::Checksum: return "checksum";
    case ContainerAttemptResult::Incomplete: return "incomplete";
    }
    throw std::logic_error("invalid container timing result");
#else
    static_cast<void>(result);
    return {};
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
}

#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)

namespace {

thread_local Session* activeSession{};

} // namespace

Session::Session()
    : previous_(activeSession)
{
    activeSession = this;
}

Session::~Session()
{
    activeSession = previous_;
}

void Session::record(Phase phase, double durationMs)
{
    durations_[static_cast<std::size_t>(phase)] += durationMs;
}

void Session::record(ContainerAttemptMeasurement measurement)
{
    containerAttempts_.push_back(std::move(measurement));
}

void Session::increment(Counter counter, std::size_t amount)
{
    counters_[static_cast<std::size_t>(counter)] += amount;
}

std::vector<Measurement> Session::measurements() const
{
    std::vector<Measurement> result;
    for (std::size_t index = 0; index < durations_.size(); ++index) {
        if (durations_[index] != 0.0) {
            result.push_back({static_cast<Phase>(index), durations_[index]});
        }
    }
    return result;
}

std::vector<ContainerAttemptMeasurement> Session::containerAttempts() const
{
    return containerAttempts_;
}

std::vector<CounterMeasurement> Session::counters() const
{
    std::vector<CounterMeasurement> result;
    for (std::size_t index = 0; index < counters_.size(); ++index) {
        if (counters_[index] != 0) {
            result.push_back({static_cast<Counter>(index), counters_[index]});
        }
    }
    return result;
}

Scope::Scope(Phase phase)
    : session_(activeSession), phase_(phase), started_(session_ ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{})
{}

Scope::~Scope()
{
    if (!session_) {
        return;
    }
    const std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - started_;
    session_->record(phase_, elapsed.count());
}

ContainerAttempt::ContainerAttempt(ContainerCandidate candidate, bool bigEndian)
    : session_(activeSession),
      measurement_{.candidate = candidate, .bigEndian = bigEndian},
      started_(session_ ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{})
{}

ContainerAttempt::~ContainerAttempt()
{
    if (!session_ || finished_) {
        return;
    }
    finish(ContainerAttemptResult::Incomplete);
}

void ContainerAttempt::beginDecompression(std::size_t compressedBytes)
{
    ++measurement_.decompressionCalls;
    measurement_.compressedInputBytes += compressedBytes;
}

void ContainerAttempt::decompressionSucceeded(std::size_t decompressedBytes)
{
    ++measurement_.decompressedBlocks;
    measurement_.decompressedBytes += decompressedBytes;
}

void ContainerAttempt::finish(ContainerAttemptResult result)
{
    if (!session_ || finished_) {
        return;
    }
    measurement_.result = result;
    const std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - started_;
    measurement_.durationMs = elapsed.count();
    session_->record(std::move(measurement_));
    finished_ = true;
}

void increment(Counter counter, std::size_t amount)
{
    if (activeSession) {
        activeSession->increment(counter, amount);
    }
}

#else

Session::Session() = default;
Session::~Session() = default;

std::vector<Measurement> Session::measurements() const
{
    return {};
}

std::vector<ContainerAttemptMeasurement> Session::containerAttempts() const
{
    return {};
}

std::vector<CounterMeasurement> Session::counters() const
{
    return {};
}

void increment(Counter, std::size_t) {}

#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)

} // namespace timing
} // namespace finale_mus_reader
