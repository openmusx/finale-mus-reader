// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include <cstdint>
#include <iterator>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using PianoBraceBracketTarget = musx::dom::options::PianoBraceBracketOptions;

constexpr const char* pianoBraceBracketReportPrefix = "options.pianoBraceBracketOptions";
constexpr std::uint16_t pianoBraceBracketDistanceSelector = 14;
constexpr std::uint16_t pianoBraceBracketThicknessSelector = 45;
constexpr std::uint16_t pianoBraceBracketCodaSelector = 55;
constexpr std::uint16_t pianoBraceBracketOuterBodySelector = 60;
constexpr std::uint16_t pianoBraceBracketOuterTipSelector = 61;
constexpr std::uint16_t pianoBraceBracketWidthSelector = 64;
constexpr std::uint16_t pianoBraceBracketInnerBodySelector = 65;
constexpr double legacyPianoBraceCenterThickness = 2.0;
constexpr double legacyPianoBraceTipThickness = 0.0;
constexpr double codaPianoBraceInnerHorizontal = 12.0;

double pianoBraceBracketFixedPoint(std::int64_t value)
{
    return static_cast<double>(value) / 10000.0;
}

double pianoBraceBracketCodaFloat(std::int64_t value)
{
    return legacySinglePrecision(value) * 4.0;
}

bool hasCodaPianoBraceLayout(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return readGlobalWords(index, profile, pianoBraceBracketCodaSelector).present;
}

bool sourceStoresDefaultBracketPosition(const SourceProfile& profile)
{
    // Believed: Finale 2004 is the first DCL layout in which selector 14 word 3
    // represents the default bracket position. Earlier layouts give the word another meaning.
    return sourceAtOrAfter(profile, FormatEpoch::DclLegacy, versions::finale2004);
}

bool sourceStoresPianoBraceThickness(const SourceProfile& profile)
{
    // Believed: Finale 3.7 is the first uncompressed layout in which
    // selector 45 stores these brace-thickness options. The exact boundary after
    // Finale 3.5 is open, so an unknown uncompressed version fails closed.
    return sourceAtOrAfter(profile, FormatEpoch::UncompressedLegacy, versions::finale3_7);
}

bool sourceUsesPreFinale37PianoBraceBehavior(const SourceProfile& profile)
{
    return sourceMatches(profile, EpochMask::Uncompressed) && sourcePredatesVersion(profile, FormatEpoch::UncompressedLegacy, versions::finale3_7);
}

const FieldMapping pianoBraceBracketCodaFields[] = {
    MUS_FIELD_AS_IF(PianoBraceBracketTarget, "55", GLOBALS_CMPER, 0, 2, ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, nullptr,
        innerTipH, pianoBraceBracketCodaFloat(value)),
    MUS_FIELD_AS_IF(PianoBraceBracketTarget, "55", GLOBALS_CMPER, 0, 4, ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, nullptr,
        innerBodyH, pianoBraceBracketCodaFloat(value)),
};

const FieldMapping pianoBraceBracketFixedFields[] = {
    MUS_WORD_IF_SOURCE(PianoBraceBracketTarget, "14", GLOBALS_CMPER, 0, 3, sourceStoresDefaultBracketPosition, defBracketPos),
    MUS_FIELD_AS_IF(PianoBraceBracketTarget, "45", GLOBALS_CMPER, 0, 2, ValueWidth::Long, LongWordOrder::HighFirst, BitRange{},
        sourceStoresPianoBraceThickness, nullptr, centerThickness, pianoBraceBracketFixedPoint(value)),
    MUS_FIELD_AS_IF(PianoBraceBracketTarget, "45", GLOBALS_CMPER, 0, 4, ValueWidth::Long, LongWordOrder::HighFirst, BitRange{},
        sourceStoresPianoBraceThickness, nullptr, tipThickness, pianoBraceBracketFixedPoint(value)),
    MUS_FIELD_AS_IF(PianoBraceBracketTarget, "60", GLOBALS_CMPER, 0, 0, ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, nullptr,
        outerBodyV, pianoBraceBracketFixedPoint(value)),
    MUS_FIELD_AS_IF(PianoBraceBracketTarget, "60", GLOBALS_CMPER, 0, 2, ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, nullptr,
        innerTipV, pianoBraceBracketFixedPoint(value)),
    MUS_FIELD_AS_IF(PianoBraceBracketTarget, "60", GLOBALS_CMPER, 0, 4, ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, nullptr,
        innerBodyV, pianoBraceBracketFixedPoint(value)),
    MUS_FIELD_AS_IF(PianoBraceBracketTarget, "61", GLOBALS_CMPER, 0, 0, ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, nullptr,
        outerTipH, pianoBraceBracketFixedPoint(value)),
    MUS_FIELD_AS_IF(PianoBraceBracketTarget, "61", GLOBALS_CMPER, 0, 2, ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, nullptr,
        outerTipV, pianoBraceBracketFixedPoint(value)),
    MUS_FIELD_AS_IF(PianoBraceBracketTarget, "61", GLOBALS_CMPER, 0, 4, ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, nullptr,
        outerBodyH, pianoBraceBracketFixedPoint(value)),
    MUS_FIELD_AS_IF(PianoBraceBracketTarget, "64", GLOBALS_CMPER, 0, 2, ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, nullptr,
        width, pianoBraceBracketFixedPoint(value)),
    MUS_FIELD_AS_IF(PianoBraceBracketTarget, "65", GLOBALS_CMPER, 0, 2, ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, nullptr,
        innerTipH, pianoBraceBracketFixedPoint(value)),
    MUS_FIELD_AS_IF(PianoBraceBracketTarget, "65", GLOBALS_CMPER, 0, 4, ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, nullptr,
        innerBodyH, pianoBraceBracketFixedPoint(value)),
};

const FieldMapping pianoBraceBracketClassFields[] = {
    MUS_CLASS_WORD(PianoBraceBracketTarget, numericGlobalClass(pianoBraceBracketDistanceSelector), GLOBALS_CMPER, classWordOffset(3), defBracketPos),
    MUS_CLASS_FIELD_AS_IF(PianoBraceBracketTarget, numericGlobalClass(pianoBraceBracketThicknessSelector), GLOBALS_CMPER, classWordOffset(2),
        ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, centerThickness, pianoBraceBracketFixedPoint(value)),
    MUS_CLASS_FIELD_AS_IF(PianoBraceBracketTarget, numericGlobalClass(pianoBraceBracketThicknessSelector), GLOBALS_CMPER, classWordOffset(4),
        ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, tipThickness, pianoBraceBracketFixedPoint(value)),
    MUS_CLASS_FIELD_AS_IF(PianoBraceBracketTarget, numericGlobalClass(pianoBraceBracketOuterBodySelector), GLOBALS_CMPER, classWordOffset(0),
        ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, outerBodyV, pianoBraceBracketFixedPoint(value)),
    MUS_CLASS_FIELD_AS_IF(PianoBraceBracketTarget, numericGlobalClass(pianoBraceBracketOuterBodySelector), GLOBALS_CMPER, classWordOffset(2),
        ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, innerTipV, pianoBraceBracketFixedPoint(value)),
    MUS_CLASS_FIELD_AS_IF(PianoBraceBracketTarget, numericGlobalClass(pianoBraceBracketOuterBodySelector), GLOBALS_CMPER, classWordOffset(4),
        ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, innerBodyV, pianoBraceBracketFixedPoint(value)),
    MUS_CLASS_FIELD_AS_IF(PianoBraceBracketTarget, numericGlobalClass(pianoBraceBracketOuterTipSelector), GLOBALS_CMPER, classWordOffset(0),
        ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, outerTipH, pianoBraceBracketFixedPoint(value)),
    MUS_CLASS_FIELD_AS_IF(PianoBraceBracketTarget, numericGlobalClass(pianoBraceBracketOuterTipSelector), GLOBALS_CMPER, classWordOffset(2),
        ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, outerTipV, pianoBraceBracketFixedPoint(value)),
    MUS_CLASS_FIELD_AS_IF(PianoBraceBracketTarget, numericGlobalClass(pianoBraceBracketOuterTipSelector), GLOBALS_CMPER, classWordOffset(4),
        ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, outerBodyH, pianoBraceBracketFixedPoint(value)),
    MUS_CLASS_FIELD_AS_IF(PianoBraceBracketTarget, numericGlobalClass(pianoBraceBracketWidthSelector), GLOBALS_CMPER, classWordOffset(2),
        ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, width, pianoBraceBracketFixedPoint(value)),
    MUS_CLASS_FIELD_AS_IF(PianoBraceBracketTarget, numericGlobalClass(pianoBraceBracketInnerBodySelector), GLOBALS_CMPER, classWordOffset(2),
        ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, innerTipH, pianoBraceBracketFixedPoint(value)),
    MUS_CLASS_FIELD_AS_IF(PianoBraceBracketTarget, numericGlobalClass(pianoBraceBracketInnerBodySelector), GLOBALS_CMPER, classWordOffset(4),
        ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, innerBodyH, pianoBraceBracketFixedPoint(value)),
};

const MappingTable& pianoBraceBracketFixedTable()
{
    static const MappingTable table{.reportPrefix = pianoBraceBracketReportPrefix,
        .epochs = EpochMask::FixedRow,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PianoBraceBracketTarget>,
        .fields = pianoBraceBracketFixedFields,
        .fieldCount = std::size(pianoBraceBracketFixedFields)};
    return table;
}

const MappingTable& pianoBraceBracketCodaTable()
{
    static const MappingTable table{.reportPrefix = pianoBraceBracketReportPrefix,
        .epochs = EpochMask::CodaBanner,
        .applies = &hasCodaPianoBraceLayout,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PianoBraceBracketTarget>,
        .fields = pianoBraceBracketCodaFields,
        .fieldCount = std::size(pianoBraceBracketCodaFields)};
    return table;
}

const MappingTable& pianoBraceBracketClassTable()
{
    static const MappingTable table{.reportPrefix = pianoBraceBracketReportPrefix,
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PianoBraceBracketTarget>,
        .fields = pianoBraceBracketClassFields,
        .fieldCount = std::size(pianoBraceBracketClassFields)};
    return table;
}

} // namespace

void importPianoBraceBracketOptions(const ImportContext& context)
{
    applyMappingTables({&pianoBraceBracketFixedTable(), &pianoBraceBracketCodaTable(), &pianoBraceBracketClassTable()}, context.index,
        context.profile, context.document, context.report);

    const auto pooled = context.document->getOptions()->get<PianoBraceBracketTarget>();
    if (!pooled) {
        return;
    }
    const auto target = std::const_pointer_cast<PianoBraceBracketTarget>(pooled);

    const auto applyBehavior = [&](double& property, const char* member, double value) {
        property = value;
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            reporting.template behaviorField<PianoBraceBracketTarget>(member, static_cast<std::int64_t>(value));
        });
    };

    if (sourceUsesPreFinale37PianoBraceBehavior(context.profile)) {
        applyBehavior(target->centerThickness, "centerThickness", legacyPianoBraceCenterThickness);
        applyBehavior(target->tipThickness, "tipThickness", legacyPianoBraceTipThickness);
    }

    if (!sourceMatches(context.profile, EpochMask::CodaBanner)) {
        return;
    }

    // Coda stores only two later-geometry values; the remaining geometry is fixed
    // behavior. When selector 55 is absent, those two values are fixed behavior too.
    applyBehavior(target->centerThickness, "centerThickness", legacyPianoBraceCenterThickness);
    applyBehavior(target->innerBodyV, "innerBodyV", 0.0);
    applyBehavior(target->innerTipV, "innerTipV", 0.0);
    applyBehavior(target->outerBodyH, "outerBodyH", 0.0);
    applyBehavior(target->outerBodyV, "outerBodyV", 0.0);
    applyBehavior(target->outerTipH, "outerTipH", 0.0);
    applyBehavior(target->outerTipV, "outerTipV", 0.0);
    applyBehavior(target->tipThickness, "tipThickness", legacyPianoBraceTipThickness);
    applyBehavior(target->width, "width", 12.0);
    if (!hasCodaPianoBraceLayout(context.index, context.profile)) {
        applyBehavior(target->innerTipH, "innerTipH", codaPianoBraceInnerHorizontal);
        applyBehavior(target->innerBodyH, "innerBodyH", codaPianoBraceInnerHorizontal);
    }
}

} // namespace options
} // namespace finale_mus_reader
