// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include <cstdint>
#include <iterator>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using AugmentationDotTarget = musx::dom::options::AugmentationDotOptions;

constexpr const char* augmentationDotReportPrefix = "options.augmentationDotOptions";

const FieldMapping codaAugmentationDotFields[] = {
    MUS_WORD(AugmentationDotTarget, "21", GLOBALS_CMPER, 0, 1, dotOffset),
};

const FieldMapping fixedRowAugmentationDotFields[] = {
    MUS_WORD(AugmentationDotTarget, "21", GLOBALS_CMPER, 0, 0, dotUpFlagOffset),
    MUS_WORD(AugmentationDotTarget, "21", GLOBALS_CMPER, 0, 1, dotOffset),
    MUS_WORD(AugmentationDotTarget, "68", GLOBALS_CMPER, 0, 0, dotNoteOffset),
    MUS_WORD(AugmentationDotTarget, "68", GLOBALS_CMPER, 0, 1, dotLift),
    MUS_WORD_AS_IF(AugmentationDotTarget, "27", GLOBALS_CMPER, 0, 0, nullptr, adjMultipleVoices, value != 0),
};

const FieldMapping classAugmentationDotFields[] = {
    MUS_CLASS_WORD(AugmentationDotTarget, numericGlobalClass(21), GLOBALS_CMPER, classWordOffset(0), dotUpFlagOffset),
    MUS_CLASS_WORD(AugmentationDotTarget, numericGlobalClass(21), GLOBALS_CMPER, classWordOffset(1), dotOffset),
    MUS_CLASS_WORD(AugmentationDotTarget, numericGlobalClass(68), GLOBALS_CMPER, classWordOffset(0), dotNoteOffset),
    MUS_CLASS_WORD(AugmentationDotTarget, numericGlobalClass(68), GLOBALS_CMPER, classWordOffset(1), dotLift),
    MUS_CLASS_WORD_AS_IF(AugmentationDotTarget, numericGlobalClass(27), GLOBALS_CMPER, classWordOffset(0), nullptr, adjMultipleVoices, value != 0),
};

const MappingTable& codaAugmentationDotTable()
{
    // Only dotOffset has a located Coda-era source. The other selector meanings remain
    // unresolved and therefore retain their seeded values or explicit legacy behavior.
    static const MappingTable table{.reportPrefix = augmentationDotReportPrefix,
        .epochs = EpochMask::CodaBanner,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<AugmentationDotTarget>,
        .fields = codaAugmentationDotFields,
        .fieldCount = std::size(codaAugmentationDotFields)};
    return table;
}

const MappingTable& fixedRowAugmentationDotTable()
{
    static const MappingTable table{.reportPrefix = augmentationDotReportPrefix,
        .epochs = EpochMask::FixedRow,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<AugmentationDotTarget>,
        .fields = fixedRowAugmentationDotFields,
        .fieldCount = std::size(fixedRowAugmentationDotFields)};
    return table;
}

const MappingTable& classAugmentationDotTable()
{
    static const MappingTable table{.reportPrefix = augmentationDotReportPrefix,
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<AugmentationDotTarget>,
        .fields = classAugmentationDotFields,
        .fieldCount = std::size(classAugmentationDotFields)};
    return table;
}

} // namespace

void importAugmentationDotOptions(const ImportContext& context)
{
    applyMappingTables({&codaAugmentationDotTable(), &fixedRowAugmentationDotTable(), &classAugmentationDotTable()}, context.index, context.profile,
        context.document, context.report);

    const auto pooled = context.document->getOptions()->get<AugmentationDotTarget>();
    if (!pooled) {
        return;
    }
    const auto target = std::const_pointer_cast<AugmentationDotTarget>(pooled);

    if (sourceMatches(context.profile, EpochMask::CodaBanner)) {
        target->adjMultipleVoices = false;
        withReporting(context.report,
            [&]<typename Reporting>(Reporting& reporting) { reporting.template behaviorField<AugmentationDotTarget>("adjMultipleVoices", 0); });
    }

    target->useLegacyFlippedStemPositioning = true;
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        reporting.template behaviorField<AugmentationDotTarget>("useLegacyFlippedStemPositioning", 1);
    });
}

} // namespace options
} // namespace finale_mus_reader
