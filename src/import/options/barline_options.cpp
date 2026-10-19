// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include <iterator>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using BarlineTarget = musx::dom::options::BarlineOptions;

constexpr const char* barlineReportPrefix = "options.barlineOptions";

constexpr std::uint16_t closeSelector = 3;
constexpr std::uint16_t finalSelector = 9;
constexpr std::uint16_t displaySelector = 36;
constexpr std::uint16_t thinWidthSelector = 58;
constexpr std::uint16_t barlineGeometrySelector = 67;
constexpr std::uint16_t dashSelector = 68;
constexpr std::int16_t codaBarlineWidth = 224;

bool sourceHasCloseBarlineOptions(const SourceProfile& profile)
{
    // Believed: the Coda-banner words at these locations belong to older, unrelated
    // settings. The uncompressed record family is the first layout that stores them as
    // the later close-barline choices.
    return sourceAtOrAfter(profile, FormatEpoch::UncompressedLegacy);
}

bool sourceHasPreviousBarlineStyleOption(const SourceProfile& profile)
{
    // Believed: this option begins in Finale 2000. Earlier uncompressed files reuse the
    // word but behave as though the later option were disabled. No record-shape marker
    // distinguishes the meanings, so an unknown uncompressed version fails closed.
    return sourceAtOrAfter(profile, FormatEpoch::UncompressedLegacy, versions::finale2000);
}

bool storesSplitLeftBarlineOptions(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    if (sourceAtOrAfter(profile, FormatEpoch::DclLegacy)) {
        return true;
    }
    if (profile.epoch == FormatEpoch::CodaBanner) {
        return false;
    }

    // Selector 67 belongs to the expanded barline-options family. Its presence states that
    // selector 36 carries separate single- and multiple-staff switches. If that family is
    // missing from an otherwise later uncompressed file, the inseparable older layout wins.
    return readNumericGlobalWords(index, barlineGeometrySelector).present;
}

bool storesUnifiedLeftBarlineOption(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return !storesSplitLeftBarlineOptions(index, profile);
}

bool sourceHasAutomaticFinalBarlineOption(const SourceProfile& profile)
{
    // The DCL record family is the first to store this option. Earlier layouts reuse the
    // word while Finale behaves as though the later option were disabled.
    return sourceAtOrAfter(profile, FormatEpoch::DclLegacy);
}

const FieldMapping fixedRowBarlineFields[] = {
    MUS_WORD_IF_SOURCE(BarlineTarget, "03", GLOBALS_CMPER, 0, 4, sourceHasCloseBarlineOptions, drawCloseSystemBarline),
    MUS_WORD_IF_SOURCE(BarlineTarget, "03", GLOBALS_CMPER, 0, 5, sourceHasCloseBarlineOptions, drawCloseFinalBarline),
    MUS_WORD_IF_SOURCE(BarlineTarget, "09", GLOBALS_CMPER, 0, 5, sourceHasAutomaticFinalBarlineOption, drawFinalBarlineOnLastMeas),
    MUS_FIELD_AS_IF(BarlineTarget, "36", GLOBALS_CMPER, 0, 1, ValueWidth::Word, LongWordOrder::HighFirst, BitRange{},
        sourceHasPreviousBarlineStyleOption, nullptr, leftBarlineUsePrevStyle, value != 0),
    MUS_WORD(BarlineTarget, "36", GLOBALS_CMPER, 0, 3, drawLeftBarlineMultipleStaves),
    MUS_WORD(BarlineTarget, "36", GLOBALS_CMPER, 0, 4, drawBarlines),
    MUS_WORD(BarlineTarget, "58", GLOBALS_CMPER, 0, 4, barlineWidth),
    MUS_WORD(BarlineTarget, "67", GLOBALS_CMPER, 0, 2, thickBarlineWidth),
    MUS_WORD(BarlineTarget, "67", GLOBALS_CMPER, 0, 3, doubleBarlineSpace),
    MUS_WORD(BarlineTarget, "67", GLOBALS_CMPER, 0, 4, finalBarlineSpace),
    MUS_LONG(BarlineTarget, "68", GLOBALS_CMPER, 0, 2, LongWordOrder::HighFirst, barlineDashOn),
    MUS_LONG(BarlineTarget, "68", GLOBALS_CMPER, 0, 4, LongWordOrder::HighFirst, barlineDashOff),
};

const FieldMapping unifiedLeftBarlineFields[] = {
    MUS_WORD(BarlineTarget, "36", GLOBALS_CMPER, 0, 3, drawLeftBarlineSingleStaff),
};

const FieldMapping splitLeftBarlineFields[] = {
    MUS_WORD(BarlineTarget, "36", GLOBALS_CMPER, 0, 2, drawLeftBarlineSingleStaff),
};

const FieldMapping classBarlineFields[] = {
    MUS_CLASS_WORD(BarlineTarget, numericGlobalClass(closeSelector), GLOBALS_CMPER, classWordOffset(4), drawCloseSystemBarline),
    MUS_CLASS_WORD(BarlineTarget, numericGlobalClass(closeSelector), GLOBALS_CMPER, classWordOffset(5), drawCloseFinalBarline),
    MUS_CLASS_WORD(BarlineTarget, numericGlobalClass(finalSelector), GLOBALS_CMPER, classWordOffset(5), drawFinalBarlineOnLastMeas),
    MUS_CLASS_WORD_AS_IF(
        BarlineTarget, numericGlobalClass(displaySelector), GLOBALS_CMPER, classWordOffset(1), nullptr, leftBarlineUsePrevStyle, value != 0),
    MUS_CLASS_WORD(BarlineTarget, numericGlobalClass(displaySelector), GLOBALS_CMPER, classWordOffset(2), drawLeftBarlineSingleStaff),
    MUS_CLASS_WORD(BarlineTarget, numericGlobalClass(displaySelector), GLOBALS_CMPER, classWordOffset(3), drawLeftBarlineMultipleStaves),
    MUS_CLASS_WORD(BarlineTarget, numericGlobalClass(displaySelector), GLOBALS_CMPER, classWordOffset(4), drawBarlines),
    MUS_CLASS_WORD(BarlineTarget, numericGlobalClass(thinWidthSelector), GLOBALS_CMPER, classWordOffset(4), barlineWidth),
    MUS_CLASS_WORD(BarlineTarget, numericGlobalClass(barlineGeometrySelector), GLOBALS_CMPER, classWordOffset(2), thickBarlineWidth),
    MUS_CLASS_WORD(BarlineTarget, numericGlobalClass(barlineGeometrySelector), GLOBALS_CMPER, classWordOffset(3), doubleBarlineSpace),
    MUS_CLASS_WORD(BarlineTarget, numericGlobalClass(barlineGeometrySelector), GLOBALS_CMPER, classWordOffset(4), finalBarlineSpace),
    MUS_CLASS_LONG(BarlineTarget, numericGlobalClass(dashSelector), GLOBALS_CMPER, classWordOffset(2), LongWordOrder::HighFirst, barlineDashOn),
    MUS_CLASS_LONG(BarlineTarget, numericGlobalClass(dashSelector), GLOBALS_CMPER, classWordOffset(4), LongWordOrder::HighFirst, barlineDashOff),
};

const MappingTable& fixedRowBarlineTable()
{
    static const MappingTable table{.reportPrefix = barlineReportPrefix,
        .epochs = EpochMask::CodaBanner | EpochMask::FixedRow,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<BarlineTarget>,
        .fields = fixedRowBarlineFields,
        .fieldCount = std::size(fixedRowBarlineFields)};
    return table;
}

const MappingTable& classBarlineTable()
{
    static const MappingTable table{.reportPrefix = barlineReportPrefix,
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<BarlineTarget>,
        .fields = classBarlineFields,
        .fieldCount = std::size(classBarlineFields)};
    return table;
}

const MappingTable& unifiedLeftBarlineTable()
{
    static const MappingTable table{.reportPrefix = barlineReportPrefix,
        .epochs = EpochMask::CodaBanner | EpochMask::Uncompressed,
        .applies = &storesUnifiedLeftBarlineOption,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<BarlineTarget>,
        .fields = unifiedLeftBarlineFields,
        .fieldCount = std::size(unifiedLeftBarlineFields)};
    return table;
}

const MappingTable& splitLeftBarlineTable()
{
    static const MappingTable table{.reportPrefix = barlineReportPrefix,
        .epochs = EpochMask::FixedRow,
        .applies = &storesSplitLeftBarlineOptions,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<BarlineTarget>,
        .fields = splitLeftBarlineFields,
        .fieldCount = std::size(splitLeftBarlineFields)};
    return table;
}

} // namespace

void importBarlineOptions(const ImportContext& context)
{
    applyMappingTables({&fixedRowBarlineTable(), &unifiedLeftBarlineTable(), &splitLeftBarlineTable(), &classBarlineTable()}, context.index,
        context.profile, context.document, context.report);

    const auto pooled = context.document->getOptions()->get<BarlineTarget>();
    if (!pooled) {
        return;
    }
    const auto target = std::const_pointer_cast<BarlineTarget>(pooled);

    if (context.profile.epoch == FormatEpoch::CodaBanner && !readNumericGlobalWords(context.index, thinWidthSelector).present) {
        // The Coda layout has no stored thin-barline width and renders it at 3.5 EVPUs.
        target->barlineWidth = codaBarlineWidth;
        withReporting(context.report,
            [&]<typename Reporting>(Reporting& reporting) { reporting.template behaviorField<BarlineTarget>("barlineWidth", codaBarlineWidth); });
    }

    if (!sourceHasAutomaticFinalBarlineOption(context.profile)) {
        target->drawFinalBarlineOnLastMeas = false;
        withReporting(context.report,
            [&]<typename Reporting>(Reporting& reporting) { reporting.template behaviorField<BarlineTarget>("drawFinalBarlineOnLastMeas", 0); });
    }

    // Legacy MUS predates the document option for double barlines before key changes.
    target->drawDoubleBarlineBeforeKeyChanges = false;
    withReporting(context.report,
        [&]<typename Reporting>(Reporting& reporting) { reporting.template behaviorField<BarlineTarget>("drawDoubleBarlineBeforeKeyChanges", 0); });
}

} // namespace options
} // namespace finale_mus_reader
