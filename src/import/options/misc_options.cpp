// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include <iterator>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using MiscOptionsTarget = musx::dom::options::MiscOptions;

constexpr const char* miscOptionsReportPrefix = "options.miscOptions";
constexpr std::uint16_t showRepeatsForPartsSelector = 16;
constexpr std::uint16_t pickupSelector = 17;

const FieldMapping fixedMiscFields[] = {
    MUS_NUMERIC_WORD(MiscOptionsTarget, pickupSelector, 0, 5, pickupValue),
};

const FieldMapping fixedLayerMiscFields[] = {
    MUS_WORD(MiscOptionsTarget, figureTag, 10, 0, 4, showActiveLayerOnly),
};

const FieldMapping fixedRepeatPartMiscFields[] = {
    MUS_NUMERIC_WORD(MiscOptionsTarget, showRepeatsForPartsSelector, 0, 5, showRepeatsForParts),
};

const FieldMapping classMiscFields[] = {
    MUS_CLASS_WORD(MiscOptionsTarget, numericGlobalClass(showRepeatsForPartsSelector), GLOBALS_CMPER, classWordOffset(5), showRepeatsForParts),
    MUS_CLASS_WORD(MiscOptionsTarget, numericGlobalClass(pickupSelector), GLOBALS_CMPER, classWordOffset(5), pickupValue),
    MUS_CLASS_WORD(MiscOptionsTarget, zlibFigureClass, 10, classWordOffset(4), showActiveLayerOnly),
};

const MappingTable& fixedMiscTable()
{
    static const MappingTable table{.reportPrefix = miscOptionsReportPrefix,
        .epochs = EpochMask::CodaBanner | EpochMask::Uncompressed | EpochMask::Dcl,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<MiscOptionsTarget>,
        .fields = fixedMiscFields,
        .fieldCount = std::size(fixedMiscFields)};
    return table;
}

const MappingTable& fixedLayerMiscTable()
{
  // The Coda epoch predates layers and has no Show Active Layer Only setting;
  // its seeded Finale 27 default is retained deliberately.
    static const MappingTable table{.reportPrefix = miscOptionsReportPrefix,
        .epochs = EpochMask::Uncompressed | EpochMask::Dcl,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<MiscOptionsTarget>,
        .fields = fixedLayerMiscFields,
        .fieldCount = std::size(fixedLayerMiscFields)};
    return table;
}

const MappingTable& fixedRepeatPartMiscTable()
{
    static const MappingTable table{.reportPrefix = miscOptionsReportPrefix,
        .epochs = EpochMask::CodaBanner | EpochMask::Uncompressed | EpochMask::Dcl,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<MiscOptionsTarget>,
        .fields = fixedRepeatPartMiscFields,
        .fieldCount = std::size(fixedRepeatPartMiscFields)};
    return table;
}

const MappingTable& classMiscTable()
{
    static const MappingTable table{.reportPrefix = miscOptionsReportPrefix,
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<MiscOptionsTarget>,
        .fields = classMiscFields,
        .fieldCount = std::size(classMiscFields)};
    return table;
}

void applyLegacyMiscBehavior(const ImportContext& context)
{
    const auto pooled = context.document->getOptions()->get<MiscOptionsTarget>();
    if (!pooled) {
        return;
    }
    const auto target = std::const_pointer_cast<MiscOptionsTarget>(pooled);

  // These settings postdate the supported legacy formats, whose fixed
  // behavior differs from the pinned Finale 27 defaults.
    target->consolidateRestsAcrossLayers = false;
    target->alignMeasureNumbersWithBarlines = false;
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        reporting.template behaviorField<MiscOptionsTarget>("consolidateRestsAcrossLayers", 0);
        reporting.template behaviorField<MiscOptionsTarget>("alignMeasureNumbersWithBarlines", 0);
    });
}

void reportRemainingMiscFields(const ImportContext& context, const std::shared_ptr<const MiscOptionsTarget>& target)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto instance = reporting.template instanceKey<MiscOptionsTarget>();
        reporting.report().setField(instance, "shapeDesignerDashLength", {Reporting::Origin::Finale27Default, 0, 0, target->shapeDesignerDashLength});
        reporting.report().setField(instance, "shapeDesignerDashSpace", {Reporting::Origin::Finale27Default, 0, 0, target->shapeDesignerDashSpace});
        reporting.report().setField(instance, "restWidthAdjust", {Reporting::Origin::Finale27Default, 0, 0, target->restWidthAdjust});
        reporting.report().setField(instance, "dblWholeVertAdjust", {Reporting::Origin::Finale27Default, 0, 0, target->dblWholeVertAdjust});
        reporting.report().setField(
            instance, "keepWrittenOctaveInConcertPitch", {Reporting::Origin::Finale27Default, 0, 0, target->keepWrittenOctaveInConcertPitch});
        if (context.profile.epoch == FormatEpoch::CodaBanner) {
            reporting.report().setField(instance, "showActiveLayerOnly", {Reporting::Origin::Finale27Default, 0, 0, target->showActiveLayerOnly});
        }
    });
}

} // namespace

void importMiscOptions(const ImportContext& context)
{
    applyMappingTables({&fixedMiscTable(), &fixedLayerMiscTable(), &fixedRepeatPartMiscTable(), &classMiscTable()}, context.index, context.profile,
        context.document, context.report);
    applyLegacyMiscBehavior(context);
    if (const auto target = context.document->getOptions()->get<MiscOptionsTarget>()) {
        reportRemainingMiscFields(context, target);
    }
}

} // namespace options
} // namespace finale_mus_reader
