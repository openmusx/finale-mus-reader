// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/support/legacy_mapping.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "import/details.h"
#include "import/entries.h"
#include "import/options.h"
#include "import/others.h"
#include "import/texts.h"
#include "reader/timing.h"

namespace finale_mus_reader {

double legacySinglePrecision(std::int64_t value)
{
    return static_cast<double>(std::bit_cast<float>(static_cast<std::uint32_t>(value)));
}

musx::dom::Efix legacyPointsToEfix(double value)
{
    return static_cast<musx::dom::Efix>(std::llround(value * musx::dom::EVPU_PER_POINT * musx::dom::EFIX_PER_EVPU));
}

musx::dom::Efix legacyTenThousandthsPointToEfix(std::int64_t value)
{
    constexpr double storedUnitsPerPoint = 10000.0;
    return legacyPointsToEfix(static_cast<double>(value) / storedUnitsPerPoint);
}

std::string readRowText(
    const records::LegacyRowPool& pool, std::span<const records::LegacyRow> family, std::uint32_t firstIncidence, std::uint32_t incidenceCount)
{
    std::string result;
    for (const auto& row : family) {
        if (row.inci < firstIncidence || row.inci - firstIncidence >= incidenceCount) {
            continue;
        }
        const auto bytes = pool.effectivePayloadOf(row);
        const auto terminator = std::find(bytes.begin(), bytes.end(), std::uint8_t{0});
        result.append(reinterpret_cast<const char*>(bytes.data()), static_cast<std::size_t>(terminator - bytes.begin()));
        if (terminator != bytes.end()) {
            break;
        }
    }
    return result;
}

std::optional<RecordFamilySource> selectRecordFamilySource(const ImportContext& context, const records::LegacyRowPool& fixedPool,
    const records::LegacyRowPool& classPool, records::LegacyTag fixedTag, records::LegacyTag classId, bool details,
    std::span<const CompactPartLayout> compactPartLayouts)
{
    switch (context.profile.epoch) {
    case FormatEpoch::CodaBanner:
    case FormatEpoch::UncompressedLegacy:
    case FormatEpoch::DclLegacy: return RecordFamilySource{&fixedPool, fixedTag, false, details};
    case FormatEpoch::ZlibLegacy: return RecordFamilySource{&classPool, classId, true, details, compactPartLayouts};
    }
    return std::nullopt;
}

std::vector<std::uint16_t> recordPartIds(const RecordFamilySource& source)
{
    if (!source.classRecords) {
        return {musx::dom::SCORE_PARTID};
    }
    return source.pool->partIdsForTag(source.identity);
}

std::vector<std::pair<std::uint16_t, std::uint16_t>> recordKeys(const RecordFamilySource& source)
{
    std::vector<std::pair<std::uint16_t, std::uint16_t>> result;
    for (const auto partId : recordPartIds(source)) {
        for (const auto cmper : source.pool->cmpersForTag(source.identity, partId)) {
            result.emplace_back(partId, cmper);
        }
    }
    return result;
}

musx::dom::EnigmaBase::ShareMode recordShareMode(const RecordFamilySource& source, const records::LegacyRow& row)
{
    using ShareMode = musx::dom::EnigmaBase::ShareMode;
    if (!source.classRecords || row.partId == musx::dom::SCORE_PARTID) {
        return ShareMode::All;
    }
    if (row.continuationSize != 0) {
        return ShareMode::Partial;
    }

    for (const auto& layout : source.compactPartLayouts) {
        if (row.payloadSize != layout.partPayloadSize) {
            continue;
        }
        const auto* score = source.pool->get(source.identity, row.cmper1, row.cmper2, row.inci, musx::dom::SCORE_PARTID);
        if (score && score->payloadSize == layout.scorePayloadSize) {
            return ShareMode::Partial;
        }
    }
    return ShareMode::None;
}

std::vector<std::uint8_t> collectRecordPayload(const RecordFamilySource& source, std::span<const records::LegacyRow> rows)
{
    std::vector<std::uint8_t> result;
    for (const auto& row : rows) {
        const auto payload = source.pool->effectivePayloadOf(row);
        result.insert(result.end(), payload.begin(), payload.end());
    }
    return result;
}

std::vector<std::int16_t> collectRecordWords(const RecordFamilySource& source, std::span<const records::LegacyRow> rows, ByteOrder byteOrder)
{
    return payloadWords(collectRecordPayload(source, rows), byteOrder);
}

std::uint16_t payloadWord(std::span<const std::uint8_t> payload, std::size_t offset, ByteOrder byteOrder)
{
    return byteOrder == ByteOrder::BigEndian ? static_cast<std::uint16_t>((static_cast<std::uint16_t>(payload[offset]) << 8U) | payload[offset + 1])
                                             : static_cast<std::uint16_t>(payload[offset] | (static_cast<std::uint16_t>(payload[offset + 1]) << 8U));
}

std::int32_t payloadLong(std::span<const std::uint8_t> payload, std::size_t offset, ByteOrder byteOrder, LongWordOrder wordOrder)
{
    const auto first = payloadWord(payload, offset, byteOrder);
    const auto second = payloadWord(payload, offset + 2, byteOrder);
    const auto high = wordOrder == LongWordOrder::HighFirst ? first : second;
    const auto low = wordOrder == LongWordOrder::HighFirst ? second : first;
    return static_cast<std::int32_t>((static_cast<std::uint32_t>(high) << 16U) | low);
}

std::optional<std::uint16_t> partUnlinkedBits(
    const RecordFamilySource& source, std::span<const records::LegacyRow> rows, std::size_t wordSlot, ByteOrder byteOrder)
{
    if (!source.classRecords || rows.empty()) {
        return std::nullopt;
    }
    const auto& row = source.rowOfWord(rows, wordSlot);
    if (row.partId == musx::dom::SCORE_PARTID || !row.continuationOverlayReady) {
        return std::nullopt;
    }
    constexpr std::size_t continuationPrefixSize = 4;
    const auto continuation = source.pool->continuationOf(row);
    const auto offset = continuationPrefixSize + source.byteOffsetInRow(wordSlot * sizeof(std::uint16_t));
    if (offset + sizeof(std::uint16_t) > continuation.size()) {
        return std::nullopt;
    }
    return payloadWord(continuation, offset, byteOrder);
}

bool partRecordUnlinked(const RecordFamilySource& source, std::span<const records::LegacyRow> rows, std::size_t at,
    std::span<const std::uint16_t> comparedBits, ByteOrder byteOrder)
{
    if (!source.classRecords || rows.empty()) {
        return true;
    }
    const auto& row = source.rowOfWord(rows, at);
    if (row.partId == musx::dom::SCORE_PARTID || !row.continuationOverlayReady) {
        return true;
    }
    for (std::size_t word = 0; word < comparedBits.size(); ++word) {
        if ((partUnlinkedBits(source, rows, at + word, byteOrder).value_or(0) & comparedBits[word]) != 0) {
            return true;
        }
    }
    return false;
}

std::string payloadString(std::span<const std::uint8_t> payload, std::size_t offset, std::size_t capacity)
{
    const auto available = (std::min)(capacity, payload.size() - offset);
    std::string result(reinterpret_cast<const char*>(payload.data() + offset), available);
    if (const auto end = result.find('\0'); end != std::string::npos) {
        result.resize(end);
    }
    return result;
}

namespace legacy_mapping_internal {

// Named rather than anonymous: ReportState<MappedFieldData<Reporting>> holds this by value and
// has external linkage, and under the unity build GCC rejects an anonymous-namespace member
// there (-Wsubobject-linkage). The name stays distinctive for the same reason every file-local
// name does.

template <typename Reporting>
struct MappedFieldData
{
    typename Reporting::InstanceKey instance;
    std::string member;
    typename Reporting::FieldInfo info;
    bool preserveExisting{};
};

} // namespace legacy_mapping_internal

namespace {

/// @brief Every musxdom class this reader recovers, one entry each, grouped by pool.
/// @details The registry says what is imported and in what order, and nothing else. How many
/// physical layouts a class has, which epochs each covers, and whether it needs a capture
/// pass before its tables or a check after them are decisions its own translation unit owns.
///
/// Order is a dependency statement wherever one class reads what another has already built.
/// FontDefinitions and FontOptions form a bootstrap stage: option and text classes name font
/// comparators, and text conversion also needs its class default before decoding literal bytes.
/// Options run before other pools, so importers in those pools may read recovered option values.
/// The remaining importers follow Finale's pool order. Within the others pool, a custom line
/// style can therefore decode its stored character through the charset of its named font.
///
/// The list is written out rather than assembled by self-registration, so that a static
/// archive cannot discard an importer nothing else references.
struct RegisteredImporter
{
#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
    timing::Phase phase;
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
    ClassImporter importer;
};

#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
#define FINALE_MUS_READER_IMPORTER(phase, importer) {timing::Phase::phase, importer}
#else
#define FINALE_MUS_READER_IMPORTER(phase, importer) {importer}
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)

const std::vector<RegisteredImporter>& registeredImporters()
{
    // One line per importer, alphabetical within each pool. The bootstrap pair and the options
    // pool run before other pools. Within a pool, order is not a contract: an importer that needs
    // another class's objects registers a check on @ref PendingReferences::checks, which runs
    // after every importer.
    // clang-format off
    static const std::vector<RegisteredImporter> result = {
        // Bootstrap, and the one ordered pair: font definitions are the pool that font options
        // resolve their ids against, and both run before anything that reads a recovered font.
        FINALE_MUS_READER_IMPORTER(ImportFontDefinitions, &others::importFontDefinitions),
        FINALE_MUS_READER_IMPORTER(ImportFontOptions, &options::importFontOptions),
        // options
        FINALE_MUS_READER_IMPORTER(ImportAccidentalOptions, &options::importAccidentalOptions),
        FINALE_MUS_READER_IMPORTER(ImportAlternateNotationOptions, &options::importAlternateNotationOptions),
        FINALE_MUS_READER_IMPORTER(ImportAugmentationDotOptions, &options::importAugmentationDotOptions),
        FINALE_MUS_READER_IMPORTER(ImportBarlineOptions, &options::importBarlineOptions),
        FINALE_MUS_READER_IMPORTER(ImportBeamOptions, &options::importBeamOptions),
        FINALE_MUS_READER_IMPORTER(ImportChordOptions, &options::importChordOptions),
        FINALE_MUS_READER_IMPORTER(ImportClefOptions, &options::importClefOptions),
        FINALE_MUS_READER_IMPORTER(ImportFlagOptions, &options::importFlagOptions),
        FINALE_MUS_READER_IMPORTER(ImportGraceNoteOptions, &options::importGraceNoteOptions),
        FINALE_MUS_READER_IMPORTER(ImportKeySignatureOptions, &options::importKeySignatureOptions),
        FINALE_MUS_READER_IMPORTER(ImportLineCurveOptions, &options::importLineCurveOptions),
        FINALE_MUS_READER_IMPORTER(ImportLyricOptions, &options::importLyricOptions),
        FINALE_MUS_READER_IMPORTER(ImportMiscOptions, &options::importMiscOptions),
        FINALE_MUS_READER_IMPORTER(ImportMultimeasureRestOptions, &options::importMultimeasureRestOptions),
        FINALE_MUS_READER_IMPORTER(ImportMusicSpacingOptions, &options::importMusicSpacingOptions),
        FINALE_MUS_READER_IMPORTER(ImportMusicSymbolOptions, &options::importMusicSymbolOptions),
        FINALE_MUS_READER_IMPORTER(ImportNoteRestOptions, &options::importNoteRestOptions),
        FINALE_MUS_READER_IMPORTER(ImportPageFormatOptions, &options::importPageFormatOptions),
        FINALE_MUS_READER_IMPORTER(ImportPianoBraceBracketOptions, &options::importPianoBraceBracketOptions),
        FINALE_MUS_READER_IMPORTER(ImportRepeatOptions, &options::importRepeatOptions),
        FINALE_MUS_READER_IMPORTER(ImportSmartShapeOptions, &options::importSmartShapeOptions),
        FINALE_MUS_READER_IMPORTER(ImportStaffOptions, &options::importStaffOptions),
        FINALE_MUS_READER_IMPORTER(ImportStemOptions, &options::importStemOptions),
        FINALE_MUS_READER_IMPORTER(ImportTextOptions, &options::importTextOptions),
        FINALE_MUS_READER_IMPORTER(ImportTieOptions, &options::importTieOptions),
        FINALE_MUS_READER_IMPORTER(ImportTimeSignatureOptions, &options::importTimeSignatureOptions),
        FINALE_MUS_READER_IMPORTER(ImportTupletOptions, &options::importTupletOptions),
        // others
        FINALE_MUS_READER_IMPORTER(ImportAcciAmountFlats, &others::importAcciAmountFlats),
        FINALE_MUS_READER_IMPORTER(ImportAcciAmountSharps, &others::importAcciAmountSharps),
        FINALE_MUS_READER_IMPORTER(ImportAcciOrderFlats, &others::importAcciOrderFlats),
        FINALE_MUS_READER_IMPORTER(ImportAcciOrderSharps, &others::importAcciOrderSharps),
        FINALE_MUS_READER_IMPORTER(ImportArticulationDefs, &others::importArticulationDefs),
        FINALE_MUS_READER_IMPORTER(ImportBeatChartElements, &others::importBeatChartElements),
        FINALE_MUS_READER_IMPORTER(ImportBookmarks, &others::importBookmarks),
        FINALE_MUS_READER_IMPORTER(ImportChordSuffixElements, &others::importChordSuffixElements),
        FINALE_MUS_READER_IMPORTER(ImportChordSuffixPlayback, &others::importChordSuffixPlayback),
        FINALE_MUS_READER_IMPORTER(ImportClefLists, &others::importClefLists),
        FINALE_MUS_READER_IMPORTER(ImportDrumStaff, &others::importDrumStaff),
        FINALE_MUS_READER_IMPORTER(ImportFilePath, &others::importFilePath),
        FINALE_MUS_READER_IMPORTER(ImportFrames, &others::importFrames),
        FINALE_MUS_READER_IMPORTER(ImportFretboardGroups, &others::importFretboardGroups),
        FINALE_MUS_READER_IMPORTER(ImportFretboardStyles, &others::importFretboardStyles),
        FINALE_MUS_READER_IMPORTER(ImportFretInstruments, &others::importFretInstruments),
        FINALE_MUS_READER_IMPORTER(ImportKeyAttributes, &others::importKeyAttributes),
        FINALE_MUS_READER_IMPORTER(ImportKeyFormats, &others::importKeyFormats),
        FINALE_MUS_READER_IMPORTER(ImportKeyMapArrays, &others::importKeyMapArrays),
        FINALE_MUS_READER_IMPORTER(ImportLayerAttributes, &others::importLayerAttributes),
        FINALE_MUS_READER_IMPORTER(ImportMarkingCategories, &others::importMarkingCategories),
        FINALE_MUS_READER_IMPORTER(ImportMeasureExprAssigns, &others::importMeasureExprAssigns),
        FINALE_MUS_READER_IMPORTER(ImportMeasureNumberRegions, &others::importMeasureNumberRegions),
        FINALE_MUS_READER_IMPORTER(ImportMeasures, &others::importMeasures),
        FINALE_MUS_READER_IMPORTER(ImportMultimeasureRests, &others::importMultimeasureRests),
        FINALE_MUS_READER_IMPORTER(ImportMultiStaffGroupIds, &others::importMultiStaffGroupIds),
        FINALE_MUS_READER_IMPORTER(ImportMultiStaffInstrumentGroups, &others::importMultiStaffInstrumentGroups),
        FINALE_MUS_READER_IMPORTER(ImportNamePositionAbbreviated, &others::importNamePositionAbbreviated),
        FINALE_MUS_READER_IMPORTER(ImportNamePositionFull, &others::importNamePositionFull),
        FINALE_MUS_READER_IMPORTER(ImportNamePositionStyleAbbreviated, &others::importNamePositionStyleAbbreviated),
        FINALE_MUS_READER_IMPORTER(ImportNamePositionStyleFull, &others::importNamePositionStyleFull),
        FINALE_MUS_READER_IMPORTER(ImportPageGraphicAssignments, &others::importPageGraphicAssignments),
        FINALE_MUS_READER_IMPORTER(ImportPages, &others::importPages),
        FINALE_MUS_READER_IMPORTER(ImportPageTextAssigns, &others::importPageTextAssigns),
        FINALE_MUS_READER_IMPORTER(ImportPartDefinitions, &others::importPartDefinitions),
        FINALE_MUS_READER_IMPORTER(ImportPartGlobals, &others::importPartGlobals),
        FINALE_MUS_READER_IMPORTER(ImportPartVoicing, &others::importPartVoicing),
        FINALE_MUS_READER_IMPORTER(ImportPercussionNoteInfo, &others::importPercussionNoteInfo),
        FINALE_MUS_READER_IMPORTER(ImportPlaybackRouteNames, &others::importPlaybackRouteNames),
        FINALE_MUS_READER_IMPORTER(ImportPlaybackRoutes, &others::importPlaybackRoutes),
        FINALE_MUS_READER_IMPORTER(ImportRepeatBackIndividualPositioning, &others::importRepeatBackIndividualPositioning),
        FINALE_MUS_READER_IMPORTER(ImportRepeatBacks, &others::importRepeatBacks),
        FINALE_MUS_READER_IMPORTER(ImportRepeatEndingStartIndividualPositioning, &others::importRepeatEndingStartIndividualPositioning),
        FINALE_MUS_READER_IMPORTER(ImportRepeatEndingStarts, &others::importRepeatEndingStarts),
        FINALE_MUS_READER_IMPORTER(ImportRepeatEndingTextIndividualPositioning, &others::importRepeatEndingTextIndividualPositioning),
        FINALE_MUS_READER_IMPORTER(ImportRepeatEndingTexts, &others::importRepeatEndingTexts),
        FINALE_MUS_READER_IMPORTER(ImportRepeatPassLists, &others::importRepeatPassLists),
        FINALE_MUS_READER_IMPORTER(ImportShapeDefinitions, &others::importShapeDefinitions),
        FINALE_MUS_READER_IMPORTER(ImportShapeExpressionDefs, &others::importShapeExpressionDefs),
        FINALE_MUS_READER_IMPORTER(ImportShapeGraphicAssignments, &others::importShapeGraphicAssignments),
        FINALE_MUS_READER_IMPORTER(ImportSmartShapeCustomLines, &others::importSmartShapeCustomLines),
        FINALE_MUS_READER_IMPORTER(ImportSmartShapeMeasureAssigns, &others::importSmartShapeMeasureAssigns),
        FINALE_MUS_READER_IMPORTER(ImportSmartShapes, &others::importSmartShapes),
        FINALE_MUS_READER_IMPORTER(ImportSplitMeasures, &others::importSplitMeasures),
        FINALE_MUS_READER_IMPORTER(ImportStaff, &others::importStaff),
        FINALE_MUS_READER_IMPORTER(ImportStaffLists, &others::importStaffLists),
        FINALE_MUS_READER_IMPORTER(ImportStaffPlayData, &others::importStaffPlayData),
        FINALE_MUS_READER_IMPORTER(ImportStaffStyleAssignments, &others::importStaffStyleAssignments),
        FINALE_MUS_READER_IMPORTER(ImportStaffStyles, &others::importStaffStyles),
        FINALE_MUS_READER_IMPORTER(ImportStaffSystems, &others::importStaffSystems),
        FINALE_MUS_READER_IMPORTER(ImportStaffUsed, &others::importStaffUsed),
        FINALE_MUS_READER_IMPORTER(ImportSystemLocks, &others::importSystemLocks),
        FINALE_MUS_READER_IMPORTER(ImportTempoChanges, &others::importTempoChanges),
        FINALE_MUS_READER_IMPORTER(ImportTextBlocks, &others::importTextBlocks),
        FINALE_MUS_READER_IMPORTER(ImportTextExpressionDefs, &others::importTextExpressionDefs),
        FINALE_MUS_READER_IMPORTER(ImportTextExpressionEnclosures, &others::importTextExpressionEnclosures),
        FINALE_MUS_READER_IMPORTER(ImportTextRepeatAssigns, &others::importTextRepeatAssigns),
        FINALE_MUS_READER_IMPORTER(ImportTextRepeatDefs, &others::importTextRepeatDefs),
        FINALE_MUS_READER_IMPORTER(ImportTextRepeatEnclosures, &others::importTextRepeatEnclosures),
        FINALE_MUS_READER_IMPORTER(ImportTextRepeatIndividualPositioning, &others::importTextRepeatIndividualPositioning),
        FINALE_MUS_READER_IMPORTER(ImportTextRepeatTexts, &others::importTextRepeatTexts),
        FINALE_MUS_READER_IMPORTER(ImportTimeCompositeLowers, &others::importTimeCompositeLowers),
        FINALE_MUS_READER_IMPORTER(ImportTimeCompositeUppers, &others::importTimeCompositeUppers),
        FINALE_MUS_READER_IMPORTER(ImportTonalCenterFlats, &others::importTonalCenterFlats),
        FINALE_MUS_READER_IMPORTER(ImportTonalCenterSharps, &others::importTonalCenterSharps),
        // details
        FINALE_MUS_READER_IMPORTER(ImportAccidentalAlterations, &details::importAccidentalAlterations),
        FINALE_MUS_READER_IMPORTER(ImportArticulationAssigns, &details::importArticulationAssigns),
        FINALE_MUS_READER_IMPORTER(ImportBaselines, &details::importBaselines),
        FINALE_MUS_READER_IMPORTER(ImportBeamAlterationsDownStem, &details::importBeamAlterationsDownStem),
        FINALE_MUS_READER_IMPORTER(ImportBeamAlterationsUpStem, &details::importBeamAlterationsUpStem),
        FINALE_MUS_READER_IMPORTER(ImportBeamExtensionDownStem, &details::importBeamExtensionDownStem),
        FINALE_MUS_READER_IMPORTER(ImportBeamExtensionUpStem, &details::importBeamExtensionUpStem),
        FINALE_MUS_READER_IMPORTER(ImportBeamStubDirection, &details::importBeamStubDirection),
        FINALE_MUS_READER_IMPORTER(ImportCenterShapes, &details::importCenterShapes),
        FINALE_MUS_READER_IMPORTER(ImportChordAssigns, &details::importChordAssigns),
        FINALE_MUS_READER_IMPORTER(ImportClefOctaveFlats, &details::importClefOctaveFlats),
        FINALE_MUS_READER_IMPORTER(ImportClefOctaveSharps, &details::importClefOctaveSharps),
        FINALE_MUS_READER_IMPORTER(ImportCrossStaff, &details::importCrossStaff),
        FINALE_MUS_READER_IMPORTER(ImportCustomDownStem, &details::importCustomDownStem),
        FINALE_MUS_READER_IMPORTER(ImportCustomUpStem, &details::importCustomUpStem),
        FINALE_MUS_READER_IMPORTER(ImportDotAlterations, &details::importDotAlterations),
        FINALE_MUS_READER_IMPORTER(ImportEntrySize, &details::importEntrySize),
        FINALE_MUS_READER_IMPORTER(ImportFretboardDiagrams, &details::importFretboardDiagrams),
        FINALE_MUS_READER_IMPORTER(ImportGFrameHolds, &details::importGFrameHolds),
        FINALE_MUS_READER_IMPORTER(ImportIndependentStaffDetails, &details::importIndependentStaffDetails),
        FINALE_MUS_READER_IMPORTER(ImportKeySymbolListElements, &details::importKeySymbolListElements),
        FINALE_MUS_READER_IMPORTER(ImportLyricAssignChorus, &details::importLyricAssignChorus),
        FINALE_MUS_READER_IMPORTER(ImportLyricAssignSection, &details::importLyricAssignSection),
        FINALE_MUS_READER_IMPORTER(ImportLyricAssignVerse, &details::importLyricAssignVerse),
        FINALE_MUS_READER_IMPORTER(ImportLyricEntryInfo, &details::importLyricEntryInfo),
        FINALE_MUS_READER_IMPORTER(ImportMeasureGraphicAssignments, &details::importMeasureGraphicAssignments),
        FINALE_MUS_READER_IMPORTER(ImportMeasureNumberIndividualPositioning, &details::importMeasureNumberIndividualPositioning),
        FINALE_MUS_READER_IMPORTER(ImportMeasureTextAssigns, &details::importMeasureTextAssigns),
        FINALE_MUS_READER_IMPORTER(ImportNoteAlterations, &details::importNoteAlterations),
        FINALE_MUS_READER_IMPORTER(ImportPercussionNoteCodes, &details::importPercussionNoteCodes),
        FINALE_MUS_READER_IMPORTER(ImportSecondaryBeamAlterationsDownStem, &details::importSecondaryBeamAlterationsDownStem),
        FINALE_MUS_READER_IMPORTER(ImportSecondaryBeamAlterationsUpStem, &details::importSecondaryBeamAlterationsUpStem),
        FINALE_MUS_READER_IMPORTER(ImportSecondaryBeamBreak, &details::importSecondaryBeamBreak),
        FINALE_MUS_READER_IMPORTER(ImportShapeNotes, &details::importShapeNotes),
        FINALE_MUS_READER_IMPORTER(ImportShapeNoteStyles, &details::importShapeNoteStyles),
        FINALE_MUS_READER_IMPORTER(ImportSmartShapeEntryAssigns, &details::importSmartShapeEntryAssigns),
        FINALE_MUS_READER_IMPORTER(ImportStaffGroups, &details::importStaffGroups),
        FINALE_MUS_READER_IMPORTER(ImportStaffSizes, &details::importStaffSizes),
        FINALE_MUS_READER_IMPORTER(ImportStemAlterations, &details::importStemAlterations),
        FINALE_MUS_READER_IMPORTER(ImportStemAlterationsUnderBeam, &details::importStemAlterationsUnderBeam),
        FINALE_MUS_READER_IMPORTER(ImportTablatureNoteMods, &details::importTablatureNoteMods),
        FINALE_MUS_READER_IMPORTER(ImportTieAlterEnds, &details::importTieAlterEnd),
        FINALE_MUS_READER_IMPORTER(ImportTieAlterStarts, &details::importTieAlterStart),
        FINALE_MUS_READER_IMPORTER(ImportTupletDefs, &details::importTupletDefs),
        // entries
        FINALE_MUS_READER_IMPORTER(ImportEntries, &entries::importEntries),
        // texts
        FINALE_MUS_READER_IMPORTER(ImportTexts, &texts::importTexts),
    };
    // clang-format on
    return result;
}

#undef FINALE_MUS_READER_IMPORTER

// Reads a numeric value from a class-identified record, addressed by byte offset inside a
// single record's payload rather than by word slot across an incidence stream.
std::optional<ResolvedValue> readClassValue(
    const records::LegacyRecordIndex& index, std::uint16_t cmper, const SourceLocation& source, ByteOrder byteOrder, std::uint16_t partId)
{
    const auto* row = index.getClassOthers().get(source.identity, cmper, 0, source.incidence, partId);
    if (!row) {
        return std::nullopt;
    }
    const auto payload = index.getClassOthers().effectivePayloadOf(*row);
    const std::size_t width = source.width == ValueWidth::Long ? 4 : source.width == ValueWidth::Byte ? 1 : 2;
    if (source.wordSlot + width > payload.size()) {
        return std::nullopt;
    }
    // Payload numbers follow the file's byte order, as everything numeric does. Finale 2007
    // in particular is mostly big-endian. Each width is read through its own signed type,
    // because the fixed-row path reads one through a signed word and the two encodings must
    // not disagree about the same logical option: an Evpu of -12 read unsigned arrives as
    // 65524 and is assigned as such. A bit range is exempt, being a magnitude rather than a
    // number with a sign, and takes its value from the unsigned payload below.
    const auto readWord = [&](std::size_t at) {
        return byteOrder == ByteOrder::BigEndian ? static_cast<std::uint16_t>((static_cast<std::uint16_t>(payload[at]) << 8U) | payload[at + 1])
                                                 : static_cast<std::uint16_t>(payload[at] | (static_cast<std::uint16_t>(payload[at + 1]) << 8U));
    };
    std::int64_t value = 0;
    if (width == 4) {
        // Not a plain four-byte read: the zlib serialization kept the two payload words the
        // fixed rows carried, so their order is the mapping's own and the same long-word
        // order rule decides it. On a little-endian file the two differ,
        // and the stem offset is the field that shows it.
        const auto first = readWord(source.wordSlot);
        const auto second = readWord(source.wordSlot + 2);
        const std::uint32_t combined = source.longOrder == LongWordOrder::HighFirst ? (static_cast<std::uint32_t>(first) << 16U) | second
                                                                                    : (static_cast<std::uint32_t>(second) << 16U) | first;
        value = static_cast<std::int32_t>(combined);
    } else if (width == 2) {
        value = static_cast<std::int16_t>(readWord(source.wordSlot));
    } else {
        value = static_cast<std::int8_t>(payload[source.wordSlot]);
    }
    return ResolvedValue{extractBits(value, source.bits), row->blockOffset, row->decodedOffset};
}

// Reads text from a class-identified record: the payload runs to its own end, so the name
// simply occupies whatever remains after the fields before it.
std::optional<std::string> readClassText(
    const records::LegacyRecordIndex& index, std::uint16_t cmper, const SourceLocation& source, std::uint16_t partId)
{
    const auto* row = index.getClassOthers().get(source.identity, cmper, 0, 0, partId);
    if (!row) {
        return std::nullopt;
    }
    const auto payload = index.getClassOthers().effectivePayloadOf(*row);
    if (source.wordSlot >= payload.size()) {
        return std::nullopt;
    }
    std::string text(reinterpret_cast<const char*>(payload.data() + source.wordSlot), payload.size() - source.wordSlot);
    if (const auto end = text.find('\0'); end != std::string::npos) {
        text.resize(end);
    }
    return text;
}

std::optional<ResolvedValue> readValue(const records::LegacyRecordIndex& index, std::uint16_t cmper, const SourceLocation& source)
{
    const auto tag = source.identity;
    const std::size_t wordIndex = static_cast<std::size_t>(source.incidence) * records::otherWordCount + source.wordSlot;
    const auto first = index.word(tag, cmper, wordIndex);
    if (!first) {
        return std::nullopt;
    }

    std::int64_t value = 0;
    if (source.width == ValueWidth::Long) {
        // The word index is absolute, so a four-byte value whose words fall in different
        // incidences resolves without special handling.
        const auto second = index.word(tag, cmper, wordIndex + 1);
        if (!second) {
            return std::nullopt;
        }
        const auto firstWord = static_cast<std::uint16_t>(first->value);
        const auto secondWord = static_cast<std::uint16_t>(second->value);
        const std::uint32_t combined = source.longOrder == LongWordOrder::HighFirst ? (static_cast<std::uint32_t>(firstWord) << 16U) | secondWord
                                                                                    : (static_cast<std::uint32_t>(secondWord) << 16U) | firstWord;
        value = static_cast<std::int32_t>(combined);
    } else if (source.width == ValueWidth::Byte) {
        // A one-byte field is selected through a 16-bit slot and then narrowed.
        // **Unverified: which half it keeps.** No one-byte mapping is promoted until a
        // document settles it.
        value = static_cast<std::int8_t>(static_cast<std::uint16_t>(first->value) & 0xffU);
    } else {
        value = first->value;
    }

    return ResolvedValue{extractBits(value, source.bits), first->blockOffset, first->decodedOffset};
}

std::optional<std::string> readText(const records::LegacyRecordIndex& index, std::uint16_t cmper, const SourceLocation& source)
{
    const auto family = index.getOthers().getArray(source.identity, cmper);
    const auto present = std::any_of(family.begin(), family.end(), [&](const records::LegacyRow& row) { return row.inci >= source.incidence; });
    if (!present) {
        return std::nullopt;
    }
    return readRowText(index.getOthers(), family, source.incidence);
}

/// @brief One destination field: what to report, and where to read it if this file can.
struct EffectiveField
{
    /// @brief Supplies the field name and the accessor that reads the seeded default.
    /// The destination is a property of the musxdom class, so any table variant will do.
    const FieldMapping* reporting{};
    /// @brief The source location applicable to this file, or null when no table applies.
    const FieldMapping* readable{};
};

using legacy_mapping_internal::MappedFieldData;

/// @brief Preserves a field's pre-overlay default and records the source selected by decoding.
class MappedFieldReport
{
public:
    MappedFieldReport(ImportReport& report, const MappingTable& table, const MappingTarget& target, const EffectiveField& field)
        : m_report(report)
    {
        withReporting(m_report, [&]<typename Reporting>(Reporting& reporting) {
            auto& data = reporting.state(m_data);
            data.instance = reporting.instanceKey(target.reportClass, target.partId,
                table.targetKind == TargetKind::OptionsSingleton ? std::optional<musx::dom::Cmper>{} : std::optional<musx::dom::Cmper>{target.cmper});
            data.member = reporting.memberName(field.reporting->fieldName);
            // A capture pass may already have established this field's provenance. An
            // unreadable table field must not replace that entry with a seeded default.
            data.preserveExisting = !field.readable && reporting.report().findField(data.instance, data.member);
            if (!data.preserveExisting && field.reporting->read) {
                data.info.rawValue = field.reporting->read(target.instance);
            }
        });
    }

    void recovered(const ResolvedValue& source, bool adjusted, std::uint16_t identity)
    {
        withReporting(m_report, [&]<typename Reporting>(Reporting& reporting) {
            reporting.state(m_data).info = {adjusted ? Reporting::Origin::LegacyMusAdjusted : Reporting::Origin::LegacyMus, source.blockOffset,
                source.decodedOffset, source.value, identity};
        });
    }

    void text(std::string_view value)
    {
        withReporting(m_report, [&]<typename Reporting>(Reporting& reporting) {
            auto& info = reporting.state(m_data).info;
            info.origin = Reporting::Origin::LegacyMus;
            info.rawValue = static_cast<std::int64_t>(value.size());
        });
    }

    void finish()
    {
        withReporting(m_report, [&]<typename Reporting>(Reporting& reporting) {
            auto& data = reporting.state(m_data);
            if (!data.preserveExisting) {
                reporting.report().setField(data.instance, std::move(data.member), data.info);
            }
        });
    }

private:
    ImportReport& m_report;
    [[no_unique_address]] ReportState<MappedFieldData> m_data;
};

/// @brief The tables for one destination class, with their fields layered.
struct EffectiveTable
{
    const MappingTable* table{};
    std::vector<EffectiveField> fields;
    /// @brief Whether @ref table is one that applies to this file, rather than merely the
    /// first registered for the prefix.
    bool applicable{};
};

/// @details Every registered field is reported, whether or not this file can supply it,
/// so a document from an era the tables do not cover still shows its supported fields
/// sitting at their synthesized defaults. The gates decide only what is readable.
std::vector<EffectiveTable> buildEffectiveTables(
    const std::vector<const MappingTable*>& tables, const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    std::vector<EffectiveTable> result;
    for (const auto* table : tables) {
        const bool tableApplies = sourceMatches(profile, table->epochs) && (!table->sourceApplies || table->sourceApplies(profile))
                                  && (!table->applies || table->applies(index, profile));
        const auto found = std::find_if(result.begin(), result.end(),
            [&](const EffectiveTable& candidate) { return std::string_view(candidate.table->reportPrefix) == table->reportPrefix; });
        auto& effective = found != result.end() ? *found : result.emplace_back(EffectiveTable{table, {}, false});
        // Tables that share a prefix layer onto one destination, but they may differ in how
        // their records are encoded and enumerated. The group must be represented by a table
        // that actually applies to this file, otherwise a later era would be read through an
        // earlier era's encoding and find nothing.
        if (tableApplies && !effective.applicable) {
            effective.table = table;
            effective.applicable = true;
        }
        for (std::size_t i = 0; i < table->fieldCount; ++i) {
            const auto* field = table->fields + i;
            const bool readable = tableApplies && (!field->sourceApplies || field->sourceApplies(profile));
            const auto existing = std::find_if(effective.fields.begin(), effective.fields.end(),
                [&](const EffectiveField& candidate) { return std::string_view(candidate.reporting->fieldName) == field->fieldName; });
            if (existing == effective.fields.end()) {
                effective.fields.push_back({field, readable ? field : nullptr});
            } else if (readable) {
                // A later table supersedes an earlier one for the same field, so a field
                // that moves in a later version costs one override row rather than a new
                // table.
                existing->readable = field;
            }
        }
    }
    return result;
}

} // namespace

musx::dom::ImportObjectCallback baselineObjectReporter(ImportReport& report)
{
    musx::dom::ImportObjectCallback result;
    withReporting(report, [&]<typename Reporting>(Reporting&) {
        result = [&report](const musx::dom::EnigmaBase& object) {
            Reporting reporting(report);
            if (const auto instance = reporting.importedInstance(object)) {
                reporting.report().setInstanceOrigin(*instance, Reporting::Origin::Finale27Default);
            }
        };
    });
    return result;
}

std::optional<ResolvedValue> readSourceValue(const records::LegacyRecordIndex& index, RecordEncoding encoding, std::uint16_t cmper,
    const SourceLocation& source, ByteOrder byteOrder, std::uint16_t partId)
{
    return encoding == RecordEncoding::ClassRecord ? readClassValue(index, cmper, source, byteOrder, partId) : readValue(index, cmper, source);
}

GlobalSelectorWords readNumericGlobalWords(const records::LegacyRecordIndex& index, std::uint16_t selector)
{
    GlobalSelectorWords result;
    const auto rows = index.getOthers().getArray(numericGlobalTag(selector), GLOBALS_CMPER);
    result.present = !rows.empty();
    if (result.present) {
        result.blockOffset = rows.front().blockOffset;
        result.decodedOffset = rows.front().decodedOffset;
    }
    for (const auto& row : rows) {
        for (std::uint8_t slot = 0; slot < row.wordCount; ++slot) {
            result.words.push_back(row.words[slot]);
        }
    }
    return result;
}

GlobalSelectorWords readGlobalWords(const records::LegacyRecordIndex& index, const SourceProfile& profile, std::uint16_t selector)
{
    if (profile.epoch != FormatEpoch::ZlibLegacy) {
        return readNumericGlobalWords(index, selector);
    }
    GlobalSelectorWords result;
    const auto* row = index.getClassOthers().get(numericGlobalClass(selector), GLOBALS_CMPER, 0, 0);
    if (!row) {
        return result;
    }
    result.words = payloadWords(index.getClassOthers().effectivePayloadOf(*row), profile.byteOrder);
    result.present = true;
    result.blockOffset = row->blockOffset;
    result.decodedOffset = row->decodedOffset;
    return result;
}

bool storesCodaMigratedPointSizes(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return profile.epoch == FormatEpoch::CodaBanner && readGlobalWords(index, profile, codaMigratedPointSizeSelector).present;
}

bool storesCodaFloatPointSizes(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return profile.epoch == FormatEpoch::CodaBanner && !storesCodaMigratedPointSizes(index, profile);
}

bool storesLoneStemFlagLayout(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    if (profile.epoch != FormatEpoch::CodaBanner && profile.epoch != FormatEpoch::UncompressedLegacy) {
        return false;
    }
    constexpr std::uint16_t beamFlagsSelector = 41;
    constexpr std::size_t beamFlagsSlot = 1;
    const auto words = readNumericGlobalWords(index, beamFlagsSelector);
    if (!words.present || words.words.size() <= beamFlagsSlot) {
        return true;
    }
    constexpr std::uint16_t loneStemFlag = 0x0001;
    return (static_cast<std::uint16_t>(words.words[beamFlagsSlot]) & ~loneStemFlag) == 0;
}

bool storesPackedBeamFlagLayout(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return !storesLoneStemFlagLayout(index, profile);
}

bool storesFinale35OptionLayout(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    constexpr std::uint16_t finale35MarkerSelector = 75;
    return readGlobalWords(index, profile, finale35MarkerSelector).present;
}

bool storesPreFinale35StemAndBeamUnits(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    if (profile.epoch == FormatEpoch::ZlibLegacy) {
        return false;
    }
    constexpr std::uint16_t stemConnectionSelector = 40;
    constexpr std::size_t earlyConnectionCount = 32;
    constexpr std::size_t connectionWords = 6;
    const auto family = readNumericGlobalWords(index, stemConnectionSelector);
    return family.present && family.words.size() <= earlyConnectionCount * connectionWords;
}

bool storesFinale35StemAndBeamUnits(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return !storesPreFinale35StemAndBeamUnits(index, profile);
}

std::int16_t wordAt(const std::vector<std::int16_t>& words, std::size_t index)
{
    return index < words.size() ? words[index] : static_cast<std::int16_t>(0);
}

std::vector<std::int16_t> payloadWords(std::span<const std::uint8_t> payload, ByteOrder byteOrder)
{
    std::vector<std::int16_t> words;
    words.reserve(payload.size() / 2);
    for (std::size_t offset = 0; offset + 2 <= payload.size(); offset += 2) {
        words.push_back(byteOrder == ByteOrder::BigEndian
                            ? static_cast<std::int16_t>((static_cast<std::uint16_t>(payload[offset]) << 8U) | payload[offset + 1])
                            : static_cast<std::int16_t>(payload[offset] | (static_cast<std::uint16_t>(payload[offset + 1]) << 8U)));
    }
    return words;
}

std::uint32_t narrowCodepoint(std::int16_t stored)
{
    return static_cast<std::uint32_t>(static_cast<std::uint8_t>(stored));
}

std::uint32_t wideCodepoint(std::int16_t low, std::int16_t high)
{
    return static_cast<std::uint32_t>(static_cast<std::uint16_t>(low)) | (static_cast<std::uint32_t>(static_cast<std::uint16_t>(high)) << 16U);
}

namespace {

/// @brief Copies every requested reference object into the document and fills in its comparator.
/// @details The one phase that belongs to no single class: it drains what every importer asked
/// for. File-local because @ref applyLegacyMappings is its only caller and the ordering rule --
/// that this runs after every pool is filled, and that nothing may allocate an `others`
/// comparator afterwards -- is enforced there rather than by any caller of this header.
void resolveDeferredReferences(const musx::dom::DocumentPtr& document, const musx::dom::DocumentPtr& referenceDocument, PendingReferences& pending,
    ImportReport& report, DeferredOrder order)
{
    const auto reportImported = baselineObjectReporter(report);
    // Keyed by the reference comparator, and scoped to this one import. Two clefs naming the same
    // reference shape share a copy; this is not a search of the target for something equivalent,
    // which is never correct for a shape.
    std::map<musx::dom::Cmper, musx::dom::Cmper> copiedShapes;
    bool reportedFailure = false;
    for (auto& request : pending.shapes) {
        musx::dom::Cmper resolved = 0;
        if (const auto found = copiedShapes.find(request.referenceShapeId); found != copiedShapes.end()) {
            resolved = found->second;
        } else if (const auto source =
                       referenceDocument->getOthers()->get<musx::dom::others::ShapeDef>(musx::dom::SCORE_PARTID, request.referenceShapeId)) {
            if (const auto imported = musx::dom::others::importShapeDefInto(document, source, reportImported)) {
                resolved = *imported;
                copiedShapes.emplace(request.referenceShapeId, resolved);
            }
        }
        if (resolved == 0) {
            if (!reportedFailure) {
                report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Warning, "A Finale 27 shape could not be copied into this document; the clefs that "
                                                            "use it read as blank."});
                reportedFailure = true;
            }
            continue;
        }
        request.assign(resolved);
        withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
            const auto& field = reporting.state(request.reportField);
            if (auto* info = reporting.report().findField(field.instance, field.member)) {
                info->rawValue = resolved;
            }
        });
    }
    if (!pending.shapes.empty()) {
        report.diagnostics.push_back(
            {musx::util::Logger::LogLevel::Verbose, "Copied " + std::to_string(copiedShapes.size()) + " Finale 27 shape(s) for "
                                                        + std::to_string(pending.shapes.size()) + " clef definition(s)."});
    }

    std::map<musx::dom::Cmper, musx::dom::Cmper> copiedCustomLines;
    reportedFailure = false;
    for (auto& request : pending.customLines) {
        musx::dom::Cmper resolved = 0;
        if (const auto found = copiedCustomLines.find(request.referenceLineId); found != copiedCustomLines.end()) {
            resolved = found->second;
        } else if (const auto source = referenceDocument->getOthers()->get<musx::dom::others::SmartShapeCustomLine>(
                       musx::dom::SCORE_PARTID, request.referenceLineId)) {
            if (const auto imported = musx::dom::others::importSmartShapeCustomLineInto(document, source, reportImported)) {
                resolved = *imported;
                copiedCustomLines.emplace(request.referenceLineId, resolved);
            }
        }
        if (resolved == 0) {
            if (!reportedFailure) {
                report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Warning, "A Finale 27 custom line could not be copied into this document; the "
                                                            "Smart Shape tools that use it have no line style."});
                reportedFailure = true;
            }
            continue;
        }
        request.assign(resolved);
        withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
            const auto& field = reporting.state(request.reportField);
            if (auto* info = reporting.report().findField(field.instance, field.member)) {
                info->origin = Reporting::Origin::Finale27Default;
                info->rawValue = resolved;
            }
        });
    }
    if (!pending.customLines.empty()) {
        report.diagnostics.push_back(
            {musx::util::Logger::LogLevel::Verbose, "Copied " + std::to_string(copiedCustomLines.size()) + " Finale 27 custom line(s) for "
                                                        + std::to_string(pending.customLines.size()) + " Smart Shape option field(s)."});
    }

    // Last within this phase: a deferred step may read anything the copies above just added.
    runDeferredChecks(pending, order);
}

} // namespace

void applyLegacyMappings(const records::LegacyRecordIndex& index, const SourceProfile& profile, std::span<const std::uint8_t> source,
    const musx::dom::DocumentPtr& document, const musx::dom::DocumentPtr& referenceDocument, ImportReport& report,
    musx::factory::ConstructionContext& construction, DeferredOrder order)
{
    if (!referenceDocument || referenceDocument == document) {
        throw std::logic_error("Legacy mappings require a separate, fully formed reference document");
    }
    PendingReferences pending;
    const ImportContext context{index, profile, source, document, referenceDocument, report, pending, construction};
    for (const auto& entry : registeredImporters()) {
        FINALE_MUS_READER_TIMED_SCOPE(entry.phase);
        entry.importer(context);
    }
    // Last, and it must stay last. Copying a reference object allocates comparators, so this
    // cannot run while any pool is still being filled. It is the one phase that belongs to no
    // single class: it drains what every importer asked for.
    {
        FINALE_MUS_READER_TIMED_SCOPE(timing::Phase::DeferredReferences);
        resolveDeferredReferences(document, referenceDocument, pending, report, order);
    }
}

// A Coda-banner document states which form its combined note references take: format flag 0x02
// is set when the `CN`, `Ts` and `Te` details name a Note ID, and clear when they name the Note's
// TCD instead. Every later epoch uses Note IDs.
constexpr std::uint8_t codaNoteIdReferencesFlag = 0x02;

NoteReferenceMode combinedNoteReferenceMode(const SourceProfile& profile)
{
    if (profile.epoch != FormatEpoch::CodaBanner || (profile.codaFormatFlags & codaNoteIdReferencesFlag) != 0) {
        return NoteReferenceMode::NoteIdOnly;
    }
    return NoteReferenceMode::TcdOnly;
}

void deferNoteIdRemap(const ImportContext& context, const std::shared_ptr<musx::dom::NoteDetailsBase>& detail,
    std::function<void(musx::dom::NoteNumber)> assign, NoteReferenceMode mode)
{
    context.pending.defer(DeferredStage::ResolveStoredRefs, [&context, detail, assign = std::move(assign), mode] {
        const auto sourceId = detail->getNoteId();
        const auto found = context.pending.noteIdsByEntryIndex.find(detail->getEntryNumber());
        if (mode != NoteReferenceMode::TcdOnly && found != context.pending.noteIdsByEntryIndex.end()) {
            const auto& [original, final] = found->second;
            std::optional<musx::dom::NoteNumber> replacement;
            std::size_t matchingSlots = 0;
            for (std::size_t index = 0; index < original.size(); ++index) {
                if (original[index] != sourceId) {
                    continue;
                }
                ++matchingSlots;
                if (original[index] != final[index]) {
                    replacement = final[index];
                }
            }
            if (replacement && matchingSlots > 1) {
                context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning,
                    "Note detail has an ambiguous Note ID for Entry " + std::to_string(detail->getEntryNumber()) + "."});
                return;
            }
            if (replacement) {
                assign(*replacement);
            }
            if (matchingSlots != 0) {
                return;
            }
        }

        if (mode == NoteReferenceMode::NoteIdOnly) {
            return;
        }
        const auto entry = context.document->getEntries()->get(detail->getEntryNumber());
        const auto storedTcd = context.pending.noteTcdByEntryIndex.find(detail->getEntryNumber());
        if (!entry || storedTcd == context.pending.noteTcdByEntryIndex.end() || entry->notes.size() != storedTcd->second.size()) {
            return;
        }
        std::optional<std::size_t> matchingIndex;
        for (std::size_t index = 0; index < storedTcd->second.size(); ++index) {
            if (storedTcd->second[index] != sourceId) {
                continue;
            }
            if (matchingIndex) {
                context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning,
                    "Note detail TCD matches multiple Notes for Entry " + std::to_string(detail->getEntryNumber()) + "."});
                return;
            }
            matchingIndex = index;
        }
        if (matchingIndex) {
            assign(entry->notes[*matchingIndex]->getNoteId());
        }
    });
}

void runDeferredChecks(PendingReferences& pending, DeferredOrder order)
{
    for (const auto& stage : pending.steps) {
        if (order == DeferredOrder::ReversedWithinStage) {
            for (auto step = stage.rbegin(); step != stage.rend(); ++step) {
                (*step)();
            }
        } else {
            for (const auto& step : stage) {
                step();
            }
        }
    }
}

void applyMappingTables(const std::vector<const MappingTable*>& tables, const records::LegacyRecordIndex& index, const SourceProfile& profile,
    const musx::dom::DocumentPtr& document, ImportReport& report, const std::function<bool(std::uint16_t partId, std::uint16_t cmper)>& skipRecord)
{
    for (const auto& effective : buildEffectiveTables(tables, index, profile)) {
        const auto& table = *effective.table;
        std::vector<MappingTarget> targets;
        if (table.targetKind == TargetKind::OthersFromRecords) {
            // The legacy file is the only source of these objects, so they are created from
            // the comparators the records themselves carry rather than found in the pool.
            // Nothing is created when no table applies to this file: with no seeded object to
            // overlay there is no default to report either, and an object built from a record
            // no table can read would be an empty fabrication.
            const auto& pool = table.encoding == RecordEncoding::ClassRecord ? index.getClassOthers() : index.getOthers();
            if (effective.applicable) {
                const RecordFamilySource source{
                    &pool, table.recordIdentity, table.encoding == RecordEncoding::ClassRecord, false, table.compactPartLayouts};
                for (const auto& [partId, cmper] : recordKeys(source)) {
                    const auto rows = pool.getArray(table.recordIdentity, cmper, 0, partId);
                    if (rows.empty() || (skipRecord && skipRecord(partId, cmper))) {
                        continue;
                    }
                    auto target = table.createTarget(document, source, rows.front(), cmper);
                    if (target.instance) {
                        targets.push_back(target);
                    }
                }
            }
        } else {
            targets = table.enumerateTargets(document);
        }

        for (const auto& target : targets) {
            for (const auto& field : effective.fields) {
                // A record that states its own layout decides which of its mutually exclusive
                // fields exist. Such a field is neither read nor reported for a record that
                // does not select it, because the destination itself is absent there. The test
                // belongs to the destination rather than to any one era, so it is taken from
                // the reporting row, and it sees every field declared before it already
                // applied.
                if (field.reporting->targetApplies && !field.reporting->targetApplies(target.instance)) {
                    continue;
                }
                MappedFieldReport fieldReport(report, table, target, field);

                if (field.readable) {
                    const auto selector = table.targetKind == TargetKind::OptionsSingleton ? field.readable->source.selector : target.cmper;
                    const bool classRecord = table.encoding == RecordEncoding::ClassRecord;
                    if (field.readable->kind == FieldKind::Text) {
                        const auto text = classRecord ? readClassText(index, selector, field.readable->source, target.partId)
                                                      : readText(index, selector, field.readable->source);
                        if (text) {
                            field.readable->applyText(target.instance, *text);
                            fieldReport.text(*text);
                        }
                        fieldReport.finish();
                        continue;
                    }
                    const auto resolved = readSourceValue(index, table.encoding, selector, field.readable->source, profile.byteOrder, target.partId);
                    if (resolved) {
                        const auto adjusted =
                            field.readable->sourceAdjustment ? field.readable->sourceAdjustment(resolved->value, index, profile) : std::nullopt;
                        field.readable->apply(target.instance, adjusted.value_or(resolved->value));
                        fieldReport.recovered(*resolved, adjusted.has_value(), field.readable->source.identity);
                    }
                }
                fieldReport.finish();
            }
            if (table.finalizeTarget) {
                table.finalizeTarget(target.instance, profile, document);
            }
        }
    }
}

} // namespace finale_mus_reader
