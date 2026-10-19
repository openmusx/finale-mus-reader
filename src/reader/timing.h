// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include <vector>

#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
#include <chrono>
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)

namespace finale_mus_reader {
namespace timing {

enum class Phase : std::size_t {
    FileIo,
    ContainerParse,
    SourceReport,
    EmbeddedGraphics,
    ConstructionBegin,
    DefaultsParse,
    DefaultsSeed,
    HeaderRecovery,
    MacSymbolFonts,
    RecordIndex,
    ImportAcciAmountFlats,
    ImportAcciAmountSharps,
    ImportAccidentalAlterations,
    ImportAccidentalOptions,
    ImportAcciOrderFlats,
    ImportAcciOrderSharps,
    ImportAlternateNotationOptions,
    ImportArticulationAssigns,
    ImportArticulationDefs,
    ImportAugmentationDotOptions,
    ImportBarlineOptions,
    ImportBaselines,
    ImportBeamAlterationsDownStem,
    ImportBeamAlterationsUpStem,
    ImportBeamExtensionDownStem,
    ImportBeamExtensionUpStem,
    ImportBeamOptions,
    ImportBeamStubDirection,
    ImportBeatChartElements,
    ImportBookmarks,
    ImportCenterShapes,
    ImportChordAssigns,
    ImportChordOptions,
    ImportChordSuffixElements,
    ImportChordSuffixPlayback,
    ImportClefLists,
    ImportClefOctaveFlats,
    ImportClefOctaveSharps,
    ImportClefOptions,
    ImportCrossStaff,
    ImportCustomDownStem,
    ImportCustomUpStem,
    ImportDotAlterations,
    ImportDrumStaff,
    ImportEntries,
    ImportEntrySize,
    ImportFilePath,
    ImportFlagOptions,
    ImportFontDefinitions,
    ImportFontOptions,
    ImportFrames,
    ImportFretboardDiagrams,
    ImportFretboardGroups,
    ImportFretboardStyles,
    ImportFretInstruments,
    ImportGFrameHolds,
    ImportGraceNoteOptions,
    ImportIndependentStaffDetails,
    ImportKeyAttributes,
    ImportKeyFormats,
    ImportKeyMapArrays,
    ImportKeySignatureOptions,
    ImportKeySymbolListElements,
    ImportLayerAttributes,
    ImportLineCurveOptions,
    ImportLyricAssignChorus,
    ImportLyricAssignSection,
    ImportLyricAssignVerse,
    ImportLyricEntryInfo,
    ImportLyricOptions,
    ImportMarkingCategories,
    ImportMeasureExprAssigns,
    ImportMeasureGraphicAssignments,
    ImportMeasureNumberIndividualPositioning,
    ImportMeasureNumberRegions,
    ImportMeasures,
    ImportMeasureTextAssigns,
    ImportMiscOptions,
    ImportMultimeasureRestOptions,
    ImportMultimeasureRests,
    ImportMultiStaffGroupIds,
    ImportMultiStaffInstrumentGroups,
    ImportMusicSpacingOptions,
    ImportMusicSymbolOptions,
    ImportNamePositionAbbreviated,
    ImportNamePositionFull,
    ImportNamePositionStyleAbbreviated,
    ImportNamePositionStyleFull,
    ImportNoteAlterations,
    ImportNoteRestOptions,
    ImportPageFormatOptions,
    ImportPageGraphicAssignments,
    ImportPages,
    ImportPageTextAssigns,
    ImportPartDefinitions,
    ImportPartGlobals,
    ImportPartVoicing,
    ImportPercussionNoteCodes,
    ImportPercussionNoteInfo,
    ImportPianoBraceBracketOptions,
    ImportPlaybackRouteNames,
    ImportPlaybackRoutes,
    ImportRepeatBackIndividualPositioning,
    ImportRepeatBacks,
    ImportRepeatEndingStartIndividualPositioning,
    ImportRepeatEndingStarts,
    ImportRepeatEndingTextIndividualPositioning,
    ImportRepeatEndingTexts,
    ImportRepeatOptions,
    ImportRepeatPassLists,
    ImportSecondaryBeamAlterationsDownStem,
    ImportSecondaryBeamAlterationsUpStem,
    ImportSecondaryBeamBreak,
    ImportShapeDefinitions,
    ImportShapeExpressionDefs,
    ImportShapeGraphicAssignments,
    ImportShapeNotes,
    ImportShapeNoteStyles,
    ImportSmartShapeCustomLines,
    ImportSmartShapeEntryAssigns,
    ImportSmartShapeMeasureAssigns,
    ImportSmartShapeOptions,
    ImportSmartShapes,
    ImportSplitMeasures,
    ImportStaff,
    ImportStaffGroups,
    ImportStaffLists,
    ImportStaffOptions,
    ImportStaffPlayData,
    ImportStaffSizes,
    ImportStaffStyleAssignments,
    ImportStaffStyles,
    ImportStaffSystems,
    ImportStaffUsed,
    ImportStemAlterations,
    ImportStemAlterationsUnderBeam,
    ImportStemOptions,
    ImportSystemLocks,
    ImportTablatureNoteMods,
    ImportTempoChanges,
    ImportTextBlocks,
    ImportTextExpressionDefs,
    ImportTextExpressionEnclosures,
    ImportTextOptions,
    ImportTextRepeatAssigns,
    ImportTextRepeatDefs,
    ImportTextRepeatEnclosures,
    ImportTextRepeatIndividualPositioning,
    ImportTextRepeatTexts,
    ImportTexts,
    ImportTieAlterEnds,
    ImportTieAlterStarts,
    ImportTieOptions,
    ImportTimeCompositeLowers,
    ImportTimeCompositeUppers,
    ImportTimeSignatureOptions,
    ImportTonalCenterFlats,
    ImportTonalCenterSharps,
    ImportTupletDefs,
    ImportTupletOptions,
    TextLaterPool,
    TextPoolFraming,
    TextConversion,
    TextLiteralEncoding,
    TextFontState,
    TextObjectConstruction,
    TextReportConstruction,
    TextDomInsertion,
    TextHeaderFileInfo,
    TextCodaStored,
    DeferredReferences,
    ConstructionFinish,
    DiagnosticDelivery,
    Count
};

enum class Counter : std::size_t {
    TextRecords,
    TextRecordBytes,
    TextCommands,
    TextLiteralRuns,
    TextLiteralBytes,
    TextCacheHits,
    TextCacheMisses,
    TextCacheAvoidedBytes,
    TextFontResolutionCacheHits,
    TextFontResolutionCacheMisses,
    TextInitialFontCacheHits,
    TextInitialFontCacheMisses,
    Count
};

enum class ContainerCandidate {
    Uncompressed,
    Dcl,
    Zlib
};

enum class ContainerAttemptResult {
    Accepted,
    InputBounds,
    FirstType,
    Framing,
    Inflate,
    Checksum,
    Incomplete
};

struct Measurement
{
    Phase phase{};
    double durationMs{};
};

struct CounterMeasurement
{
    Counter counter{};
    std::size_t value{};
};

struct ContainerAttemptMeasurement
{
    ContainerCandidate candidate{};
    bool bigEndian{};
    ContainerAttemptResult result{};
    double durationMs{};
    std::size_t decompressionCalls{};
    std::size_t decompressedBlocks{};
    std::size_t compressedInputBytes{};
    std::size_t decompressedBytes{};
};

[[nodiscard]] std::string_view phaseName(Phase phase);
[[nodiscard]] std::string_view counterName(Counter counter);
[[nodiscard]] std::string_view containerCandidateName(ContainerCandidate candidate);
[[nodiscard]] std::string_view containerAttemptResultName(ContainerAttemptResult result);

class Session
{
public:
    Session();
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    [[nodiscard]] std::vector<Measurement> measurements() const;
    [[nodiscard]] std::vector<CounterMeasurement> counters() const;
    [[nodiscard]] std::vector<ContainerAttemptMeasurement> containerAttempts() const;

private:
#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
    friend class Scope;
    friend class ContainerAttempt;
    friend void increment(Counter counter, std::size_t amount);
    void record(Phase phase, double durationMs);
    void record(ContainerAttemptMeasurement measurement);
    void increment(Counter counter, std::size_t amount);

    Session* previous_{};
    std::array<double, static_cast<std::size_t>(Phase::Count)> durations_{};
    std::array<std::size_t, static_cast<std::size_t>(Counter::Count)> counters_{};
    std::vector<ContainerAttemptMeasurement> containerAttempts_;
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
};

#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)

class Scope
{
public:
    explicit Scope(Phase phase);
    ~Scope();

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

private:
    Session* session_{};
    Phase phase_{};
    std::chrono::steady_clock::time_point started_;
};

class ContainerAttempt
{
public:
    ContainerAttempt(ContainerCandidate candidate, bool bigEndian);
    ~ContainerAttempt();

    ContainerAttempt(const ContainerAttempt&) = delete;
    ContainerAttempt& operator=(const ContainerAttempt&) = delete;

    void beginDecompression(std::size_t compressedBytes);
    void decompressionSucceeded(std::size_t decompressedBytes);
    void finish(ContainerAttemptResult result);

private:
    Session* session_{};
    ContainerAttemptMeasurement measurement_{};
    std::chrono::steady_clock::time_point started_;
    bool finished_{};
};

void increment(Counter counter, std::size_t amount = 1);

#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)

} // namespace timing
} // namespace finale_mus_reader

#define FINALE_MUS_READER_TIMING_JOIN_IMPL(a, b) a##b
#define FINALE_MUS_READER_TIMING_JOIN(a, b) FINALE_MUS_READER_TIMING_JOIN_IMPL(a, b)

#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
#define FINALE_MUS_READER_TIMED_SCOPE(phase) \
    const ::finale_mus_reader::timing::Scope FINALE_MUS_READER_TIMING_JOIN(finaleMusReaderTimingScope_, __LINE__)(phase)
#define FINALE_MUS_READER_CONTAINER_ATTEMPT(name, candidate, bigEndian) ::finale_mus_reader::timing::ContainerAttempt name(candidate, bigEndian)
#define FINALE_MUS_READER_CONTAINER_DECOMPRESSION_BEGIN(name, bytes) name.beginDecompression(bytes)
#define FINALE_MUS_READER_CONTAINER_DECOMPRESSION_SUCCEEDED(name, bytes) name.decompressionSucceeded(bytes)
#define FINALE_MUS_READER_CONTAINER_ATTEMPT_FINISH(name, result) name.finish(result)
#define FINALE_MUS_READER_TIMING_INCREMENT(counter, amount) ::finale_mus_reader::timing::increment(counter, amount)
#else
#define FINALE_MUS_READER_TIMED_SCOPE(phase) static_cast<void>(0)
#define FINALE_MUS_READER_CONTAINER_ATTEMPT(name, candidate, bigEndian) static_cast<void>(0)
#define FINALE_MUS_READER_CONTAINER_DECOMPRESSION_BEGIN(name, bytes) static_cast<void>(0)
#define FINALE_MUS_READER_CONTAINER_DECOMPRESSION_SUCCEEDED(name, bytes) static_cast<void>(0)
#define FINALE_MUS_READER_CONTAINER_ATTEMPT_FINISH(name, result) static_cast<void>(0)
#define FINALE_MUS_READER_TIMING_INCREMENT(counter, amount) static_cast<void>(0)
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
