// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include <iterator>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using AccidentalOptionsTarget = musx::dom::options::AccidentalOptions;

constexpr std::uint16_t accidentalShapeSelector = 21;
constexpr std::uint16_t accidentalCrossLayerSelector = 22;
constexpr std::uint16_t accidentalSpacingSelector = 59;
constexpr std::uint16_t accidentalStartMeasureSelector = 41;

bool sourceStoresCrossLayerPositioning(const SourceProfile& profile)
{
    // Finale 2004 introduces this option at selector 22 word 0. Earlier versions use that word
    // for a beam option, so the version boundary is required within the DCL epoch.
    return sourceAtOrAfter(profile, FormatEpoch::DclLegacy, versions::finale2004);
}

// Fixed-row encodings address these fields by selector and the zlib encoding coalesces each
// selector into the class id derived by numericGlobalClass.
const FieldMapping fixedRowAccidentalFields[] = {
    MUS_WORD(AccidentalOptionsTarget, "21", GLOBALS_CMPER, 0, 3, minOverlap),
    MUS_WORD(AccidentalOptionsTarget, "21", GLOBALS_CMPER, 0, 5, multiCharSpace),
    MUS_WORD(AccidentalOptionsTarget, "59", GLOBALS_CMPER, 0, 3, acciNoteSpace),
    MUS_WORD(AccidentalOptionsTarget, "59", GLOBALS_CMPER, 0, 4, acciAcciSpace),
    MUS_WORD(AccidentalOptionsTarget, "41", GLOBALS_CMPER, 2, 2, startMeasureSepar),
};

const FieldMapping fixedRowCrossLayerFields[] = {
    MUS_WORD_IF_SOURCE(AccidentalOptionsTarget, "22", GLOBALS_CMPER, 0, 0, sourceStoresCrossLayerPositioning, crossLayerPositioning),
};

const FieldMapping classRecordAccidentalFields[] = {
    MUS_CLASS_WORD(AccidentalOptionsTarget, numericGlobalClass(accidentalShapeSelector), GLOBALS_CMPER, classWordOffset(3), minOverlap),
    MUS_CLASS_WORD(AccidentalOptionsTarget, numericGlobalClass(accidentalShapeSelector), GLOBALS_CMPER, classWordOffset(5), multiCharSpace),
    MUS_CLASS_WORD(
        AccidentalOptionsTarget, numericGlobalClass(accidentalCrossLayerSelector), GLOBALS_CMPER, classWordOffset(0), crossLayerPositioning),
    MUS_CLASS_WORD(AccidentalOptionsTarget, numericGlobalClass(accidentalSpacingSelector), GLOBALS_CMPER, classWordOffset(3), acciNoteSpace),
    MUS_CLASS_WORD(AccidentalOptionsTarget, numericGlobalClass(accidentalSpacingSelector), GLOBALS_CMPER, classWordOffset(4), acciAcciSpace),
    MUS_CLASS_WORD(
        AccidentalOptionsTarget, numericGlobalClass(accidentalStartMeasureSelector), GLOBALS_CMPER, classWordOffset(14), startMeasureSepar),
};

const MappingTable& fixedRowAccidentalTable()
{
    static const MappingTable table{.reportPrefix = "options.accidentalOptions",
        .epochs = EpochMask::CodaBanner | EpochMask::FixedRow,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<AccidentalOptionsTarget>,
        .fields = fixedRowAccidentalFields,
        .fieldCount = std::size(fixedRowAccidentalFields)};
    return table;
}

const MappingTable& classRecordAccidentalTable()
{
    static const MappingTable table{.reportPrefix = "options.accidentalOptions",
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<AccidentalOptionsTarget>,
        .fields = classRecordAccidentalFields,
        .fieldCount = std::size(classRecordAccidentalFields)};
    return table;
}

const MappingTable& fixedRowCrossLayerTable()
{
    static const MappingTable table{.reportPrefix = "options.accidentalOptions",
        .epochs = EpochMask::Dcl,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<AccidentalOptionsTarget>,
        .fields = fixedRowCrossLayerFields,
        .fieldCount = std::size(fixedRowCrossLayerFields)};
    return table;
}

void applyPreFinale2004CrossLayerBehavior(const ImportContext& context)
{
    if (!sourcePredatesVersion(context.profile, FormatEpoch::DclLegacy, versions::finale2004)) {
        return;
    }

    const auto pooled = context.document->getOptions()->get<AccidentalOptionsTarget>();
    if (!pooled) {
        return;
    }
    const auto target = std::const_pointer_cast<AccidentalOptionsTarget>(pooled);
    target->crossLayerPositioning = false;
    withReporting(context.report,
        [&]<typename Reporting>(Reporting& reporting) { reporting.template behaviorField<AccidentalOptionsTarget>("crossLayerPositioning", 0); });
}

void applyEarlyAccidentalSpacingBehavior(const ImportContext& context)
{
    // Finale 3.5 introduces adjustable accidental spacing. Believed: when both earlier slots
    // remain zero, Finale uses the hard-coded legacy spacing of 8. A nonzero word remains data.
    if (!sourcePredatesVersion(context.profile, FormatEpoch::UncompressedLegacy, versions::finale3_5)) {
        return;
    }

    const auto family = readNumericGlobalWords(context.index, accidentalSpacingSelector);
    if (!family.present || family.words.size() <= 4 || family.words[3] != 0 || family.words[4] != 0) {
        return;
    }

    const auto pooled = context.document->getOptions()->get<AccidentalOptionsTarget>();
    if (!pooled) {
        return;
    }
    const auto target = std::const_pointer_cast<AccidentalOptionsTarget>(pooled);
    target->acciNoteSpace = 8;
    target->acciAcciSpace = 8;

    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        for (const auto* member : {"acciNoteSpace", "acciAcciSpace"}) {
            if (auto* info = reporting.report().findField(reporting.template instanceKey<AccidentalOptionsTarget>(), member)) {
                info->origin = Reporting::Origin::LegacyBehavior;
                info->rawValue = 0;
            }
        }
    });
}

} // namespace

void importAccidentalOptions(const ImportContext& context)
{
    applyMappingTables({&fixedRowAccidentalTable(), &fixedRowCrossLayerTable(), &classRecordAccidentalTable()}, context.index, context.profile,
        context.document, context.report);
    applyPreFinale2004CrossLayerBehavior(context);
    applyEarlyAccidentalSpacingBehavior(context);
}

} // namespace options
} // namespace finale_mus_reader
