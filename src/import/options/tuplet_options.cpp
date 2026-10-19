// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include <iterator>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using TupletOptionsTarget = musx::dom::options::TupletOptions;

constexpr const char* tupletOptionsReportPrefix = "options.tupletOptions";
constexpr std::uint16_t tupletPreferencesSelector = 56;
constexpr std::uint16_t tupletNumberOffsetSelector = 14;
constexpr std::uint16_t tupletSlopeSelector = 23;
constexpr std::uint16_t tupletThicknessSelector = 69;
constexpr std::size_t shortTupletPreferenceWords = 6;
constexpr std::size_t expandedTupletPreferenceWords = 15;

constexpr TupletOptionsTarget::NumberStyle tupletNumberStyle(std::int64_t value)
{
    // The two note-bearing encodings name the opposite visual result in EnigmaXML.
    if (value == 3) {
        return TupletOptionsTarget::NumberStyle::RatioPlusBothNotes;
    }
    if (value == 4) {
        return TupletOptionsTarget::NumberStyle::RatioPlusDenominatorNote;
    }
    return static_cast<TupletOptionsTarget::NumberStyle>(value);
}

bool storesExpandedTupletPreferences(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    // The 15-word family is the format's own marker for the expanded layout. Earlier files
    // carry only the six-word prefix, if they carry selector 56 at all.
    const auto words = readGlobalWords(index, profile, tupletPreferencesSelector);
    return words.present && words.words.size() >= expandedTupletPreferenceWords;
}

bool usesShortTupletPreferences(const SourceProfile& profile, const GlobalSelectorWords& words)
{
    if (!sourceMatches(profile, EpochMask::CodaBanner | EpochMask::Uncompressed)) {
        return false;
    }
    return words.present && words.words.size() == shortTupletPreferenceWords;
}

bool storesShortTupletPreferences(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return usesShortTupletPreferences(profile, readGlobalWords(index, profile, tupletPreferencesSelector));
}

bool sourceStoresFinale2005TupletPreferences(const SourceProfile& profile)
{
    // Finale 2005 gives expanded word 4 its secondary-flag meaning and stores maximum slope.
    // In earlier expanded layouts word 4 is obsolete and selector 23 word 5 is unrelated.
    return sourceAtOrAfter(profile, FormatEpoch::DclLegacy, versions::finale2005);
}

bool sourcePredatesFinale2005TupletPreferences(const SourceProfile& profile)
{
    return sourcePredatesVersion(profile, FormatEpoch::DclLegacy, versions::finale2005);
}

bool storesTupletLineWidth(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return readGlobalWords(index, profile, tupletThicknessSelector).present;
}

const FieldMapping fixedTupletPrefixFields[] = {
    MUS_NUMERIC_WORD(TupletOptionsTarget, tupletPreferencesSelector, 0, 0, displayNumber),
    MUS_NUMERIC_WORD(TupletOptionsTarget, tupletPreferencesSelector, 0, 1, displayDuration),
    MUS_NUMERIC_WORD(TupletOptionsTarget, tupletPreferencesSelector, 0, 2, referenceNumber),
    MUS_NUMERIC_WORD(TupletOptionsTarget, tupletPreferencesSelector, 0, 3, referenceDuration),
    MUS_NUMERIC_WORD(TupletOptionsTarget, tupletPreferencesSelector, 0, 5, tupOffX),
};

const FieldMapping shortTupletAlwaysFlatFields[] = {
    // The short preference uses bit 0 for the always-flat behavior. The expanded pre-2005
    // structure gives this word a different meaning.
    MUS_NUMERIC_BIT(TupletOptionsTarget, tupletPreferencesSelector, 0, 4, 0, alwaysFlat),
};

const FieldMapping shortUncompressedTupletFields[] = {
    MUS_NUMERIC_BIT(TupletOptionsTarget, tupletPreferencesSelector, 0, 4, 1, fullDura),
};

const FieldMapping fixedExpandedTupletFields[] = {
    MUS_NUMERIC_WORD(TupletOptionsTarget, tupletPreferencesSelector, 1, 0, tupOffY),
    MUS_NUMERIC_WORD(TupletOptionsTarget, tupletPreferencesSelector, 1, 1, brackOffX),
    MUS_NUMERIC_WORD(TupletOptionsTarget, tupletPreferencesSelector, 1, 2, brackOffY),
    MUS_NUMERIC_FIELD_AS_IF(TupletOptionsTarget, tupletPreferencesSelector, 1, 3, ValueWidth::Word, LongWordOrder::HighFirst, (BitRange{0, 4}),
        nullptr, nullptr, numStyle, tupletNumberStyle(value)),
    MUS_NUMERIC_FIELD_AS_IF(TupletOptionsTarget, tupletPreferencesSelector, 1, 3, ValueWidth::Word, LongWordOrder::HighFirst, (BitRange{4, 3}),
        nullptr, nullptr, posStyle, static_cast<TupletOptionsTarget::PositioningStyle>(value)),
    MUS_NUMERIC_BIT(TupletOptionsTarget, tupletPreferencesSelector, 1, 3, 7, allowHorz),
    MUS_NUMERIC_BIT(TupletOptionsTarget, tupletPreferencesSelector, 1, 3, 8, ignoreHorzNumOffset),
    MUS_NUMERIC_BIT(TupletOptionsTarget, tupletPreferencesSelector, 1, 3, 9, breakBracket),
    MUS_NUMERIC_BIT(TupletOptionsTarget, tupletPreferencesSelector, 1, 3, 10, matchHooks),
    MUS_NUMERIC_BIT(TupletOptionsTarget, tupletPreferencesSelector, 1, 3, 11, useBottomNote),
    MUS_NUMERIC_FIELD_AS_IF(TupletOptionsTarget, tupletPreferencesSelector, 1, 3, ValueWidth::Word, LongWordOrder::HighFirst, (BitRange{12, 2}),
        nullptr, nullptr, brackStyle, static_cast<TupletOptionsTarget::BracketStyle>(value)),
    MUS_NUMERIC_BIT(TupletOptionsTarget, tupletPreferencesSelector, 1, 3, 14, smartTuplet),
    MUS_NUMERIC_WORD(TupletOptionsTarget, tupletPreferencesSelector, 1, 4, leftHookLen),
    MUS_NUMERIC_WORD(TupletOptionsTarget, tupletPreferencesSelector, 1, 5, leftHookExt),
    MUS_NUMERIC_WORD(TupletOptionsTarget, tupletPreferencesSelector, 2, 0, rightHookLen),
    MUS_NUMERIC_WORD(TupletOptionsTarget, tupletPreferencesSelector, 2, 1, rightHookExt),
    MUS_NUMERIC_WORD(TupletOptionsTarget, tupletPreferencesSelector, 2, 2, manualSlopeAdj),
};

const FieldMapping fixedFinale2005TupletFields[] = {
    MUS_NUMERIC_BIT(TupletOptionsTarget, tupletPreferencesSelector, 0, 4, 0, alwaysFlat),
    MUS_NUMERIC_BIT(TupletOptionsTarget, tupletPreferencesSelector, 0, 4, 1, fullDura),
    MUS_NUMERIC_BIT(TupletOptionsTarget, tupletPreferencesSelector, 0, 4, 2, metricCenter),
    MUS_NUMERIC_BIT(TupletOptionsTarget, tupletPreferencesSelector, 0, 4, 3, avoidStaff),
    MUS_NUMERIC_WORD(TupletOptionsTarget, tupletSlopeSelector, 0, 5, tupMaxSlope),
};

const FieldMapping fixedTupletNumberOffsetFields[] = {
    MUS_NUMERIC_WORD(TupletOptionsTarget, tupletNumberOffsetSelector, 0, 4, tupNUpstemOffset),
    MUS_NUMERIC_WORD(TupletOptionsTarget, tupletNumberOffsetSelector, 0, 5, tupNDownstemOffset),
};

const FieldMapping fixedTupletLineWidthFields[] = {
    MUS_NUMERIC_WORD(TupletOptionsTarget, tupletThicknessSelector, 0, 0, tupLineWidth),
};

const FieldMapping classTupletPrefixFields[] = {
    MUS_CLASS_WORD(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(0), displayNumber),
    MUS_CLASS_WORD(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(1), displayDuration),
    MUS_CLASS_WORD(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(2), referenceNumber),
    MUS_CLASS_WORD(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(3), referenceDuration),
    MUS_CLASS_WORD(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(5), tupOffX),
};

const FieldMapping classExpandedTupletFields[] = {
    MUS_CLASS_WORD(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(6), tupOffY),
    MUS_CLASS_WORD(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(7), brackOffX),
    MUS_CLASS_WORD(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(8), brackOffY),
    MUS_CLASS_SELECTED_BITS_AS(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(9), 0, 4, numStyle,
        tupletNumberStyle(value)),
    MUS_CLASS_SELECTED_BITS_AS(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(9), 4, 3, posStyle,
        static_cast<TupletOptionsTarget::PositioningStyle>(value)),
    MUS_CLASS_BIT(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(9), 7, allowHorz),
    MUS_CLASS_BIT(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(9), 8, ignoreHorzNumOffset),
    MUS_CLASS_BIT(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(9), 9, breakBracket),
    MUS_CLASS_BIT(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(9), 10, matchHooks),
    MUS_CLASS_BIT(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(9), 11, useBottomNote),
    MUS_CLASS_SELECTED_BITS_AS(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(9), 12, 2,
        brackStyle, static_cast<TupletOptionsTarget::BracketStyle>(value)),
    MUS_CLASS_BIT(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(9), 14, smartTuplet),
    MUS_CLASS_WORD(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(10), leftHookLen),
    MUS_CLASS_WORD(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(11), leftHookExt),
    MUS_CLASS_WORD(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(12), rightHookLen),
    MUS_CLASS_WORD(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(13), rightHookExt),
    MUS_CLASS_WORD(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(14), manualSlopeAdj),
};

const FieldMapping classFinale2005TupletFields[] = {
    MUS_CLASS_BIT(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(4), 0, alwaysFlat),
    MUS_CLASS_BIT(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(4), 1, fullDura),
    MUS_CLASS_BIT(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(4), 2, metricCenter),
    MUS_CLASS_BIT(TupletOptionsTarget, numericGlobalClass(tupletPreferencesSelector), GLOBALS_CMPER, classWordOffset(4), 3, avoidStaff),
    MUS_CLASS_WORD(TupletOptionsTarget, numericGlobalClass(tupletNumberOffsetSelector), GLOBALS_CMPER, classWordOffset(4), tupNUpstemOffset),
    MUS_CLASS_WORD(TupletOptionsTarget, numericGlobalClass(tupletNumberOffsetSelector), GLOBALS_CMPER, classWordOffset(5), tupNDownstemOffset),
    MUS_CLASS_WORD(TupletOptionsTarget, numericGlobalClass(tupletThicknessSelector), GLOBALS_CMPER, classWordOffset(0), tupLineWidth),
    MUS_CLASS_WORD(TupletOptionsTarget, numericGlobalClass(tupletSlopeSelector), GLOBALS_CMPER, classWordOffset(5), tupMaxSlope),
};

const MappingTable& fixedTupletPrefixTable()
{
    static const MappingTable table{.reportPrefix = tupletOptionsReportPrefix,
        .epochs = EpochMask::CodaBanner | EpochMask::FixedRow,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TupletOptionsTarget>,
        .fields = fixedTupletPrefixFields,
        .fieldCount = std::size(fixedTupletPrefixFields)};
    return table;
}

const MappingTable& fixedExpandedTupletTable()
{
    static const MappingTable table{.reportPrefix = tupletOptionsReportPrefix,
        .epochs = EpochMask::CodaBanner | EpochMask::FixedRow,
        .applies = &storesExpandedTupletPreferences,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TupletOptionsTarget>,
        .fields = fixedExpandedTupletFields,
        .fieldCount = std::size(fixedExpandedTupletFields)};
    return table;
}

const MappingTable& fixedFinale2005TupletTable()
{
    static const MappingTable table{.reportPrefix = tupletOptionsReportPrefix,
        .epochs = EpochMask::Dcl,
        .sourceApplies = &sourceStoresFinale2005TupletPreferences,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TupletOptionsTarget>,
        .fields = fixedFinale2005TupletFields,
        .fieldCount = std::size(fixedFinale2005TupletFields)};
    return table;
}

const MappingTable& shortTupletAlwaysFlatTable()
{
    static const MappingTable table{.reportPrefix = tupletOptionsReportPrefix,
        .epochs = EpochMask::CodaBanner | EpochMask::Uncompressed,
        .applies = &storesShortTupletPreferences,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TupletOptionsTarget>,
        .fields = shortTupletAlwaysFlatFields,
        .fieldCount = std::size(shortTupletAlwaysFlatFields)};
    return table;
}

const MappingTable& shortUncompressedTupletTable()
{
    static const MappingTable table{.reportPrefix = tupletOptionsReportPrefix,
        .epochs = EpochMask::Uncompressed,
        .applies = &storesShortTupletPreferences,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TupletOptionsTarget>,
        .fields = shortUncompressedTupletFields,
        .fieldCount = std::size(shortUncompressedTupletFields)};
    return table;
}

const MappingTable& fixedTupletNumberOffsetTable()
{
    static const MappingTable table{.reportPrefix = tupletOptionsReportPrefix,
        .epochs = EpochMask::CodaBanner | EpochMask::FixedRow,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TupletOptionsTarget>,
        .fields = fixedTupletNumberOffsetFields,
        .fieldCount = std::size(fixedTupletNumberOffsetFields)};
    return table;
}

const MappingTable& fixedTupletLineWidthTable()
{
    static const MappingTable table{.reportPrefix = tupletOptionsReportPrefix,
        .epochs = EpochMask::CodaBanner | EpochMask::FixedRow,
        .applies = &storesTupletLineWidth,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TupletOptionsTarget>,
        .fields = fixedTupletLineWidthFields,
        .fieldCount = std::size(fixedTupletLineWidthFields)};
    return table;
}

const MappingTable& classTupletPrefixTable()
{
    static const MappingTable table{.reportPrefix = tupletOptionsReportPrefix,
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TupletOptionsTarget>,
        .fields = classTupletPrefixFields,
        .fieldCount = std::size(classTupletPrefixFields)};
    return table;
}

const MappingTable& classExpandedTupletTable()
{
    static const MappingTable table{.reportPrefix = tupletOptionsReportPrefix,
        .epochs = EpochMask::Zlib,
        .applies = &storesExpandedTupletPreferences,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TupletOptionsTarget>,
        .fields = classExpandedTupletFields,
        .fieldCount = std::size(classExpandedTupletFields)};
    return table;
}

const MappingTable& classFinale2005TupletTable()
{
    static const MappingTable table{.reportPrefix = tupletOptionsReportPrefix,
        .epochs = EpochMask::Zlib,
        .sourceApplies = &sourceStoresFinale2005TupletPreferences,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TupletOptionsTarget>,
        .fields = classFinale2005TupletFields,
        .fieldCount = std::size(classFinale2005TupletFields)};
    return table;
}

void applyAutoBracketStyle(const ImportContext& context)
{
    const auto words = readGlobalWords(context.index, context.profile, tupletPreferencesSelector);
    constexpr std::size_t secondaryFlagsWord = 4;
    constexpr std::size_t primaryFlagsWord = 9;
    constexpr std::uint16_t secondaryAutoBracket = 0x0010;
    constexpr std::uint16_t primaryAutoBracket = 0x8000;
    if (!words.present || words.words.size() <= primaryFlagsWord) {
        return;
    }

    const auto primary = static_cast<std::uint16_t>(words.words[primaryFlagsWord]);
    auto style =
        (primary & primaryAutoBracket) != 0 ? TupletOptionsTarget::AutoBracketStyle::UnbeamedOnly : TupletOptionsTarget::AutoBracketStyle::Always;
    if (sourceStoresFinale2005TupletPreferences(context.profile) && (primary & primaryAutoBracket) != 0
        && (static_cast<std::uint16_t>(words.words[secondaryFlagsWord]) & secondaryAutoBracket) != 0) {
        style = TupletOptionsTarget::AutoBracketStyle::NeverBeamSide;
    }

    const auto pooled = context.document->getOptions()->get<TupletOptionsTarget>();
    if (!pooled) {
        return;
    }
    std::const_pointer_cast<TupletOptionsTarget>(pooled)->autoBracketStyle = style;
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        reporting.report().setField(reporting.template instanceKey<TupletOptionsTarget>(), "autoBracketStyle",
            {Reporting::Origin::LegacyMus, words.blockOffset, words.decodedOffset + primaryFlagsWord * sizeof(std::int16_t), readAs(style)});
    });
}

void applyUnstoredTupletBehavior(const ImportContext& context)
{
    const auto pooled = context.document->getOptions()->get<TupletOptionsTarget>();
    if (!pooled) {
        return;
    }
    const auto target = std::const_pointer_cast<TupletOptionsTarget>(pooled);
    const auto reportBehavior = [&](const char* member, auto& destination, auto value) {
        destination = value;
        withReporting(context.report,
            [&]<typename Reporting>(Reporting& reporting) { reporting.template behaviorField<TupletOptionsTarget>(member, readAs(value)); });
    };

    if (sourcePredatesFinale2005TupletPreferences(context.profile)) {
        // Before the secondary flag word became meaningful, tuplets did not avoid the staff.
        reportBehavior("avoidStaff", target->avoidStaff, false);
    }

    const auto words = readGlobalWords(context.index, context.profile, tupletPreferencesSelector);
    const bool codaWithoutPreferences = context.profile.epoch == FormatEpoch::CodaBanner && !words.present;
    if (!codaWithoutPreferences && !usesShortTupletPreferences(context.profile, words)) {
        return;
    }

    // The pre-expanded layout cannot store these later appearance controls. Its behavior differs
    // from the modern baseline only for the values listed here.
    reportBehavior("autoBracketStyle", target->autoBracketStyle, TupletOptionsTarget::AutoBracketStyle::Always);
    reportBehavior("tupOffY", target->tupOffY, musx::dom::Evpu{});
    reportBehavior("numStyle", target->numStyle, TupletOptionsTarget::NumberStyle::Nothing);
    reportBehavior("posStyle", target->posStyle, TupletOptionsTarget::PositioningStyle::Manual);
    reportBehavior("breakBracket", target->breakBracket, false);
    reportBehavior("matchHooks", target->matchHooks, false);
    reportBehavior("brackStyle", target->brackStyle, TupletOptionsTarget::BracketStyle::Nothing);
    reportBehavior("smartTuplet", target->smartTuplet, false);
    reportBehavior("leftHookLen", target->leftHookLen, musx::dom::Evpu{});
    reportBehavior("rightHookLen", target->rightHookLen, musx::dom::Evpu{});
    if (!storesTupletLineWidth(context.index, context.profile)) {
        reportBehavior("tupLineWidth", target->tupLineWidth, musx::dom::Efix{224});
    }
}

void reportUnmappedTupletFields(const ImportContext& context)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto target = context.document->getOptions()->get<TupletOptionsTarget>();
        if (!target) {
            return;
        }
        const auto key = reporting.template instanceKey<TupletOptionsTarget>();
#define FINALE_MUS_READER_UNMAPPED_TUPLET(member) reporting.unmappedField(key, #member, readAs(target->member))
        FINALE_MUS_READER_UNMAPPED_TUPLET(displayNumber);
        FINALE_MUS_READER_UNMAPPED_TUPLET(displayDuration);
        FINALE_MUS_READER_UNMAPPED_TUPLET(referenceNumber);
        FINALE_MUS_READER_UNMAPPED_TUPLET(referenceDuration);
        FINALE_MUS_READER_UNMAPPED_TUPLET(alwaysFlat);
        FINALE_MUS_READER_UNMAPPED_TUPLET(fullDura);
        FINALE_MUS_READER_UNMAPPED_TUPLET(metricCenter);
        FINALE_MUS_READER_UNMAPPED_TUPLET(avoidStaff);
        FINALE_MUS_READER_UNMAPPED_TUPLET(autoBracketStyle);
        FINALE_MUS_READER_UNMAPPED_TUPLET(tupOffX);
        FINALE_MUS_READER_UNMAPPED_TUPLET(tupOffY);
        FINALE_MUS_READER_UNMAPPED_TUPLET(brackOffX);
        FINALE_MUS_READER_UNMAPPED_TUPLET(brackOffY);
        FINALE_MUS_READER_UNMAPPED_TUPLET(numStyle);
        FINALE_MUS_READER_UNMAPPED_TUPLET(posStyle);
        FINALE_MUS_READER_UNMAPPED_TUPLET(allowHorz);
        FINALE_MUS_READER_UNMAPPED_TUPLET(ignoreHorzNumOffset);
        FINALE_MUS_READER_UNMAPPED_TUPLET(breakBracket);
        FINALE_MUS_READER_UNMAPPED_TUPLET(matchHooks);
        FINALE_MUS_READER_UNMAPPED_TUPLET(useBottomNote);
        FINALE_MUS_READER_UNMAPPED_TUPLET(brackStyle);
        FINALE_MUS_READER_UNMAPPED_TUPLET(smartTuplet);
        FINALE_MUS_READER_UNMAPPED_TUPLET(leftHookLen);
        FINALE_MUS_READER_UNMAPPED_TUPLET(leftHookExt);
        FINALE_MUS_READER_UNMAPPED_TUPLET(rightHookLen);
        FINALE_MUS_READER_UNMAPPED_TUPLET(rightHookExt);
        FINALE_MUS_READER_UNMAPPED_TUPLET(manualSlopeAdj);
        FINALE_MUS_READER_UNMAPPED_TUPLET(tupMaxSlope);
        FINALE_MUS_READER_UNMAPPED_TUPLET(tupLineWidth);
        FINALE_MUS_READER_UNMAPPED_TUPLET(tupNUpstemOffset);
        FINALE_MUS_READER_UNMAPPED_TUPLET(tupNDownstemOffset);
#undef FINALE_MUS_READER_UNMAPPED_TUPLET
    });
}

} // namespace

void importTupletOptions(const ImportContext& context)
{
    applyMappingTables({&fixedTupletPrefixTable(), &fixedExpandedTupletTable(), &shortTupletAlwaysFlatTable(), &shortUncompressedTupletTable(),
                           &fixedFinale2005TupletTable(), &fixedTupletNumberOffsetTable(), &fixedTupletLineWidthTable(), &classTupletPrefixTable(),
                           &classExpandedTupletTable(), &classFinale2005TupletTable()},
        context.index, context.profile, context.document, context.report);
    applyAutoBracketStyle(context);
    applyUnstoredTupletBehavior(context);
    reportUnmappedTupletFields(context);
}

} // namespace options
} // namespace finale_mus_reader
