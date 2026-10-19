// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include <iterator>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using GraceNoteOptionsTarget = musx::dom::options::GraceNoteOptions;

constexpr const char* graceNoteOptionsReportPrefix = "options.graceNoteOptions";
constexpr std::uint16_t tablatureGraceSizeSelector = 14;
constexpr std::uint16_t graceSizeSelector = 23;
constexpr std::uint16_t graceTimingSelector = 27;
constexpr std::uint16_t graceSlashBehaviorSelector = 44;
constexpr std::uint16_t codaLineWidthSelector = 54;

const FieldMapping codaGraceNoteFields[] = {
    MUS_WORD(GraceNoteOptionsTarget, "23", GLOBALS_CMPER, 0, 0, gracePerc),
};

const FieldMapping codaFloatGraceSlashWidthFields[] = {
    MUS_NUMERIC_FIELD_AS_IF(GraceNoteOptionsTarget, codaLineWidthSelector, 0, 0, ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr,
        nullptr, graceSlashWidth, legacyPointsToEfix(legacySinglePrecision(value))),
};

const FieldMapping codaMigratedGraceSlashWidthFields[] = {
    MUS_NUMERIC_FIELD_AS_IF(GraceNoteOptionsTarget, codaMigratedPointSizeSelector, 0, 1, ValueWidth::Word, LongWordOrder::HighFirst, BitRange{},
        nullptr, nullptr, graceSlashWidth, legacyTenThousandthsPointToEfix(value)),
};

const FieldMapping fixedGraceNoteFields[] = {
    MUS_WORD(GraceNoteOptionsTarget, "14", GLOBALS_CMPER, 0, 2, tabGracePerc),
    MUS_WORD(GraceNoteOptionsTarget, "23", GLOBALS_CMPER, 0, 0, gracePerc),
    MUS_WORD(GraceNoteOptionsTarget, "27", GLOBALS_CMPER, 0, 4, playbackDuration),
    MUS_WORD(GraceNoteOptionsTarget, "27", GLOBALS_CMPER, 0, 5, entryOffset),
    MUS_WORD(GraceNoteOptionsTarget, "44", GLOBALS_CMPER, 0, 4, slashFlaggedGraceNotes),
    MUS_NUMERIC_WORD(GraceNoteOptionsTarget, codaMigratedPointSizeSelector, 0, 1, graceSlashWidth),
};

const FieldMapping classGraceNoteFields[] = {
    MUS_CLASS_WORD(GraceNoteOptionsTarget, numericGlobalClass(tablatureGraceSizeSelector), GLOBALS_CMPER, classWordOffset(2), tabGracePerc),
    MUS_CLASS_WORD(GraceNoteOptionsTarget, numericGlobalClass(graceSizeSelector), GLOBALS_CMPER, classWordOffset(0), gracePerc),
    MUS_CLASS_WORD(GraceNoteOptionsTarget, numericGlobalClass(graceTimingSelector), GLOBALS_CMPER, classWordOffset(4), playbackDuration),
    MUS_CLASS_WORD(GraceNoteOptionsTarget, numericGlobalClass(graceTimingSelector), GLOBALS_CMPER, classWordOffset(5), entryOffset),
    MUS_CLASS_WORD(GraceNoteOptionsTarget, numericGlobalClass(graceSlashBehaviorSelector), GLOBALS_CMPER, classWordOffset(4), slashFlaggedGraceNotes),
    MUS_CLASS_WORD(GraceNoteOptionsTarget, numericGlobalClass(codaMigratedPointSizeSelector), GLOBALS_CMPER, classWordOffset(1), graceSlashWidth),
};

const MappingTable& codaGraceNoteTable()
{
    static const MappingTable table{.reportPrefix = graceNoteOptionsReportPrefix,
        .epochs = EpochMask::CodaBanner,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<GraceNoteOptionsTarget>,
        .fields = codaGraceNoteFields,
        .fieldCount = std::size(codaGraceNoteFields)};
    return table;
}

const MappingTable& fixedGraceNoteTable()
{
    static const MappingTable table{.reportPrefix = graceNoteOptionsReportPrefix,
        .epochs = EpochMask::Uncompressed | EpochMask::Dcl,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<GraceNoteOptionsTarget>,
        .fields = fixedGraceNoteFields,
        .fieldCount = std::size(fixedGraceNoteFields)};
    return table;
}

const MappingTable& codaFloatGraceSlashWidthTable()
{
    static const MappingTable table{.reportPrefix = graceNoteOptionsReportPrefix,
        .epochs = EpochMask::CodaBanner,
        .applies = &storesCodaFloatPointSizes,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<GraceNoteOptionsTarget>,
        .fields = codaFloatGraceSlashWidthFields,
        .fieldCount = std::size(codaFloatGraceSlashWidthFields)};
    return table;
}

const MappingTable& codaMigratedGraceSlashWidthTable()
{
    static const MappingTable table{.reportPrefix = graceNoteOptionsReportPrefix,
        .epochs = EpochMask::CodaBanner,
        .applies = &storesCodaMigratedPointSizes,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<GraceNoteOptionsTarget>,
        .fields = codaMigratedGraceSlashWidthFields,
        .fieldCount = std::size(codaMigratedGraceSlashWidthFields)};
    return table;
}

const MappingTable& classGraceNoteTable()
{
    static const MappingTable table{.reportPrefix = graceNoteOptionsReportPrefix,
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<GraceNoteOptionsTarget>,
        .fields = classGraceNoteFields,
        .fieldCount = std::size(classGraceNoteFields)};
    return table;
}

void reportCodaDefaults(const ImportContext& context)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        if (context.profile.epoch != FormatEpoch::CodaBanner) {
            return;
        }
        const auto target = context.document->getOptions()->get<GraceNoteOptionsTarget>();
        if (!target) {
            return;
        }

        const auto key = reporting.template instanceKey<GraceNoteOptionsTarget>();
        const auto reportDefault = [&](const char* member, std::int64_t value) {
            reporting.report().setField(key, member, {Reporting::Origin::Finale27Default, 0, 0, value});
        };
        // Coda supplies no supported source for these later fields. They deliberately retain
        // the pinned values instead of acquiring speculative mappings from unrelated records.
        reportDefault("tabGracePerc", target->tabGracePerc);
        reportDefault("playbackDuration", target->playbackDuration);
        reportDefault("entryOffset", target->entryOffset);
        reportDefault("slashFlaggedGraceNotes", target->slashFlaggedGraceNotes);
    });
}

} // namespace

void importGraceNoteOptions(const ImportContext& context)
{
    reportCodaDefaults(context);
    applyMappingTables({&codaGraceNoteTable(), &codaFloatGraceSlashWidthTable(), &codaMigratedGraceSlashWidthTable(), &fixedGraceNoteTable(),
                           &classGraceNoteTable()},
        context.index, context.profile, context.document, context.report);
}

} // namespace options
} // namespace finale_mus_reader
