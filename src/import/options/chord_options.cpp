// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include <iterator>
#include <string_view>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using ChordOptionsTarget = musx::dom::options::ChordOptions;

// Only the typed fixed-row epoch has the fully mapped selector-41 layout. The Coda form uses
// word 2 as a flag word whose individual meanings are not yet distinguished.
constexpr EpochMask chordFixedRowEpochs = EpochMask::FixedRow;
constexpr std::uint16_t codaChordSelector = 41;
constexpr std::size_t codaShowFretboardsSlot = 2;
constexpr std::uint16_t codaShowFretboardsMask = 0x000c;
constexpr std::int64_t preFinale37ChordAccidentalLift = 12;
constexpr double unstatedChordPercent = 100.0;

bool sourceStoresChordAccidentalLifts(const SourceProfile& profile)
{
    return sourceAtOrAfter(profile, FormatEpoch::UncompressedLegacy, versions::finale3_7);
}

bool sourceStoresSimpleChordSpelling(const SourceProfile& profile)
{
    return sourceAtOrAfter(profile, FormatEpoch::UncompressedLegacy, versions::finale3_0);
}

bool sourceStoresChordPlayback(const SourceProfile& profile)
{
    return sourceAtOrAfter(profile, FormatEpoch::DclLegacy, versions::finale2003);
}

bool sourceStoresChordPercent(const SourceProfile& profile)
{
    return sourceAtOrAfter(profile, FormatEpoch::ZlibLegacy, versions::finale2010);
}

double chordPercentFromFixed(std::int64_t value)
{
    return static_cast<double>(value) / 10000.0;
}

ChordOptionsTarget::ChordAlignment chordAlignmentFromLegacy(std::int64_t value)
{
    return value == 0 ? ChordOptionsTarget::ChordAlignment::Left : ChordOptionsTarget::ChordAlignment::Center;
}

ChordOptionsTarget::ChordStyle chordStyleFromLegacy(std::int64_t value)
{
    switch (value) {
    case 1: return ChordOptionsTarget::ChordStyle::Roman;
    case 2: return ChordOptionsTarget::ChordStyle::NashvilleA;
    case 3: return ChordOptionsTarget::ChordStyle::German;
    case 4: return ChordOptionsTarget::ChordStyle::Solfeggio;
    case 5: return ChordOptionsTarget::ChordStyle::European;
    case 6: return ChordOptionsTarget::ChordStyle::Scandinavian;
    case 7: return ChordOptionsTarget::ChordStyle::NashvilleB;
    default: return ChordOptionsTarget::ChordStyle::Standard;
    }
}

bool hasChordOptionsLayout(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return readGlobalWords(index, profile, 41).present;
}

const FieldMapping chordFields[] = {
    MUS_FIELD_AS_IF(ChordOptionsTarget, "37", GLOBALS_CMPER, 0, 3, ValueWidth::Word, LongWordOrder::HighFirst, BitRange{},
        &sourceStoresChordAccidentalLifts, nullptr, chordSharpLift, value),
    MUS_FIELD_AS_IF(ChordOptionsTarget, "37", GLOBALS_CMPER, 0, 4, ValueWidth::Word, LongWordOrder::HighFirst, BitRange{},
        &sourceStoresChordAccidentalLifts, nullptr, chordFlatLift, value),
    MUS_FIELD_AS_IF(ChordOptionsTarget, "37", GLOBALS_CMPER, 0, 5, ValueWidth::Word, LongWordOrder::HighFirst, BitRange{},
        &sourceStoresChordAccidentalLifts, nullptr, chordNaturalLift, value),
    MUS_WORD(ChordOptionsTarget, "41", GLOBALS_CMPER, 0, 2, showFretboards),
    MUS_WORD(ChordOptionsTarget, "41", GLOBALS_CMPER, 1, 0, fretStyleId),
    MUS_WORD(ChordOptionsTarget, "41", GLOBALS_CMPER, 1, 1, fretInstId),
    MUS_FIELD_AS_IF(ChordOptionsTarget, "41", GLOBALS_CMPER, 1, 3, ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, nullptr,
        fretPercent, chordPercentFromFixed(value)),
    MUS_WORD(ChordOptionsTarget, "41", GLOBALS_CMPER, 1, 5, multiFretItemsPerStr),
    MUS_WORD(ChordOptionsTarget, "41", GLOBALS_CMPER, 2, 0, italicizeCapoChords),
    MUS_WORD_AS_IF(ChordOptionsTarget, "44", GLOBALS_CMPER, 0, 5, nullptr, chordAlignment, chordAlignmentFromLegacy(value)),
    MUS_WORD_AS_IF(ChordOptionsTarget, "45", GLOBALS_CMPER, 0, 0, nullptr, chordStyle, chordStyleFromLegacy(value)),
    MUS_FIELD_AS_IF(ChordOptionsTarget, "45", GLOBALS_CMPER, 0, 1, ValueWidth::Word, LongWordOrder::HighFirst, BitRange{},
        &sourceStoresSimpleChordSpelling, nullptr, useSimpleChordSpelling, value),
    MUS_FIELD_AS_IF(ChordOptionsTarget, "76", GLOBALS_CMPER, 0, 5, ValueWidth::Word, LongWordOrder::HighFirst, (BitRange{1, 1}),
        &sourceStoresChordPlayback, nullptr, chordPlayback, value != 0),
};

const FieldMapping classChordFields[] = {
    MUS_CLASS_WORD(ChordOptionsTarget, numericGlobalClass(37), GLOBALS_CMPER, classWordOffset(3), chordSharpLift),
    MUS_CLASS_WORD(ChordOptionsTarget, numericGlobalClass(37), GLOBALS_CMPER, classWordOffset(4), chordFlatLift),
    MUS_CLASS_WORD(ChordOptionsTarget, numericGlobalClass(37), GLOBALS_CMPER, classWordOffset(5), chordNaturalLift),
    MUS_CLASS_WORD(ChordOptionsTarget, numericGlobalClass(41), GLOBALS_CMPER, classWordOffset(2), showFretboards),
    MUS_CLASS_WORD(ChordOptionsTarget, numericGlobalClass(41), GLOBALS_CMPER, classWordOffset(6), fretStyleId),
    MUS_CLASS_WORD(ChordOptionsTarget, numericGlobalClass(41), GLOBALS_CMPER, classWordOffset(7), fretInstId),
    MUS_CLASS_FIELD_AS_IF(ChordOptionsTarget, numericGlobalClass(41), GLOBALS_CMPER, classWordOffset(9), ValueWidth::Long, LongWordOrder::HighFirst,
        BitRange{}, nullptr, fretPercent, chordPercentFromFixed(value)),
    MUS_CLASS_WORD(ChordOptionsTarget, numericGlobalClass(41), GLOBALS_CMPER, classWordOffset(11), multiFretItemsPerStr),
    MUS_CLASS_WORD(ChordOptionsTarget, numericGlobalClass(41), GLOBALS_CMPER, classWordOffset(12), italicizeCapoChords),
    MUS_CLASS_WORD_AS_IF(
        ChordOptionsTarget, numericGlobalClass(44), GLOBALS_CMPER, classWordOffset(5), nullptr, chordAlignment, chordAlignmentFromLegacy(value)),
    MUS_CLASS_WORD_AS_IF(
        ChordOptionsTarget, numericGlobalClass(45), GLOBALS_CMPER, classWordOffset(0), nullptr, chordStyle, chordStyleFromLegacy(value)),
    MUS_CLASS_WORD(ChordOptionsTarget, numericGlobalClass(45), GLOBALS_CMPER, classWordOffset(1), useSimpleChordSpelling),
    MUS_CLASS_BIT(ChordOptionsTarget, numericGlobalClass(76), GLOBALS_CMPER, classWordOffset(5), 1, chordPlayback),
};

const FieldMapping storedChordPercentFields[] = {
    FieldMapping{"chordPercent", FieldKind::Number,
        SourceLocation{numericGlobalClass(41), GLOBALS_CMPER, 0, classWordOffset(16), ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}},
        &sourceStoresChordPercent,
        [](void* instance, std::int64_t value) { static_cast<ChordOptionsTarget*>(instance)->chordPercent = chordPercentFromFixed(value); },
        [](const void* instance) -> std::int64_t { return readAs(static_cast<const ChordOptionsTarget*>(instance)->chordPercent); }},
};

const MappingTable& chordTable()
{
    static const MappingTable table{.reportPrefix = "options.chordOptions",
        .epochs = chordFixedRowEpochs,
        .applies = &hasChordOptionsLayout,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<ChordOptionsTarget>,
        .fields = chordFields,
        .fieldCount = std::size(chordFields)};
    return table;
}

const MappingTable& classChordTable()
{
    static const MappingTable table{.reportPrefix = "options.chordOptions",
        .epochs = EpochMask::Zlib,
        .applies = &hasChordOptionsLayout,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<ChordOptionsTarget>,
        .fields = classChordFields,
        .fieldCount = std::size(classChordFields)};
    return table;
}

const MappingTable& storedChordPercentTable()
{
    // The scaling value is stored only from Finale 2010 onward. This gate intentionally
    // remains inside the zlib epoch; an unversioned zlib source cannot prove that it carries
    // the later setting and therefore receives the earlier era's fixed behavior.
    static const MappingTable table{.reportPrefix = "options.chordOptions",
        .epochs = EpochMask::Zlib,
        .applies = &hasChordOptionsLayout,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<ChordOptionsTarget>,
        .fields = storedChordPercentFields,
        .fieldCount = std::size(storedChordPercentFields)};
    return table;
}

void reportChordDefault(const ImportContext& context, const char* member)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) { reporting.template defaultField<ChordOptionsTarget>(member, 0); });
}

void reportChordBehavior(const ImportContext& context, const char* member, std::int64_t rawValue)
{
    withReporting(
        context.report, [&]<typename Reporting>(Reporting& reporting) { reporting.template behaviorField<ChordOptionsTarget>(member, rawValue); });
}

void importCodaShowFretboards(const ImportContext& context, ChordOptionsTarget& target)
{
    if (context.profile.epoch != FormatEpoch::CodaBanner) {
        return;
    }
    const auto source = readGlobalWords(context.index, context.profile, codaChordSelector);
    if (!source.present || source.words.size() <= codaShowFretboardsSlot) {
        return;
    }

    const auto raw = static_cast<std::uint16_t>(source.words[codaShowFretboardsSlot]);
    const auto state = raw & codaShowFretboardsMask;
    if (state != 0 && state != codaShowFretboardsMask) {
        return;
    }

    target.showFretboards = state == codaShowFretboardsMask;
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        reporting.report().setField(reporting.template instanceKey<ChordOptionsTarget>(), "showFretboards",
            {Reporting::Origin::LegacyMus, source.blockOffset, source.decodedOffset + codaShowFretboardsSlot * 2, raw,
                numericGlobalTag(codaChordSelector)});
    });
}

} // namespace

void importChordOptions(const ImportContext& context)
{
    const auto pooled = context.document->getOptions()->get<ChordOptionsTarget>();
    if (!pooled) {
        return;
    }
    const auto target = std::const_pointer_cast<ChordOptionsTarget>(pooled);
    const auto reference = context.referenceDocument->getOptions()->get<ChordOptionsTarget>();
    if (!reference) {
        return;
    }

    applyMappingTables(
        {&chordTable(), &classChordTable(), &storedChordPercentTable()}, context.index, context.profile, context.document, context.report);
    importCodaShowFretboards(context, *target);

    reportChordDefault(context, "useFretboardFont");

    if (!sourceStoresChordAccidentalLifts(context.profile)) {
        target->chordSharpLift = preFinale37ChordAccidentalLift;
        target->chordFlatLift = preFinale37ChordAccidentalLift;
        target->chordNaturalLift = preFinale37ChordAccidentalLift;
        reportChordBehavior(context, "chordSharpLift", preFinale37ChordAccidentalLift);
        reportChordBehavior(context, "chordFlatLift", preFinale37ChordAccidentalLift);
        reportChordBehavior(context, "chordNaturalLift", preFinale37ChordAccidentalLift);
    }
    if (context.profile.epoch == FormatEpoch::CodaBanner) {
        target->useSimpleChordSpelling = false;
        reportChordBehavior(context, "useSimpleChordSpelling", 0);
    }

    if (context.profile.epoch != FormatEpoch::CodaBanner && !sourceStoresChordPercent(context.profile)) {
        target->chordPercent = unstatedChordPercent;
        reportChordBehavior(context, "chordPercent", static_cast<std::int64_t>(unstatedChordPercent));
    }

    // A source-owned fretboard style or instrument keeps its own comparator; one naming a definition
    // the document does not contain takes the pinned default instead. Both pools belong to other
    // importers, so the test cannot run here: it is registered for the phase that follows every
    // importer, which is what lets this class sit anywhere in the registry.
    context.pending.defer(DeferredStage::ResolveStoredRefs, [&context, target, reference] {
        const auto& others = *context.document->getOthers();
        if (target->fretStyleId == 0 || !others.get<musx::dom::others::FretboardStyle>(musx::dom::SCORE_PARTID, target->fretStyleId)) {
            target->fretStyleId = reference->fretStyleId;
            reportChordDefault(context, "fretStyleId");
        }
        if (target->fretInstId == 0 || !others.get<musx::dom::others::FretInstrument>(musx::dom::SCORE_PARTID, target->fretInstId)) {
            target->fretInstId = reference->fretInstId;
            reportChordDefault(context, "fretInstId");
        }
    });
}

} // namespace options
} // namespace finale_mus_reader
