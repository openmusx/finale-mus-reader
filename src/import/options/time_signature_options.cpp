// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include <iterator>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using TimeSignatureOptionsTarget = musx::dom::options::TimeSignatureOptions;

constexpr const char* timeSignatureOptionsReportPrefix = "options.timeSignatureOptions";
constexpr std::uint16_t timeSignatureDistanceSelector = 18;
constexpr std::uint16_t timeSignatureAbbreviationSelector = 19;
constexpr std::uint16_t timeSignatureCompositeSelector = 23;
constexpr std::uint16_t courtesyChangeSelector = 44;
constexpr std::uint16_t timeSignatureVerticalSelector = 67;
constexpr std::size_t timeSignatureDistanceWordsWithParts = 11;

const FieldMapping codaTimeSignatureFields[] = {
    MUS_NUMERIC_WORD(TimeSignatureOptionsTarget, timeSignatureDistanceSelector, 0, 5, timeUpperLift),
    MUS_NUMERIC_WORD(TimeSignatureOptionsTarget, timeSignatureDistanceSelector, 0, 3, timeFront),
    MUS_NUMERIC_WORD(TimeSignatureOptionsTarget, timeSignatureDistanceSelector, 0, 4, timeBack),
    MUS_NUMERIC_WORD(TimeSignatureOptionsTarget, timeSignatureAbbreviationSelector, 0, 2, timeSigDoAbrvCommon),
    MUS_NUMERIC_WORD(TimeSignatureOptionsTarget, timeSignatureAbbreviationSelector, 0, 3, timeSigDoAbrvCut),
    MUS_NUMERIC_WORD(TimeSignatureOptionsTarget, timeSignatureCompositeSelector, 0, 1, numCompositeDecimalPlaces),
};

const FieldMapping fixedTimeSignatureFields[] = {
    MUS_NUMERIC_WORD(TimeSignatureOptionsTarget, timeSignatureDistanceSelector, 0, 5, timeUpperLift),
    MUS_NUMERIC_WORD(TimeSignatureOptionsTarget, timeSignatureDistanceSelector, 0, 3, timeFront),
    MUS_NUMERIC_WORD(TimeSignatureOptionsTarget, timeSignatureDistanceSelector, 0, 4, timeBack),
    MUS_NUMERIC_WORD(TimeSignatureOptionsTarget, timeSignatureDistanceSelector, 1, 3, timeFrontParts),
    MUS_NUMERIC_WORD(TimeSignatureOptionsTarget, timeSignatureDistanceSelector, 1, 4, timeBackParts),
    MUS_NUMERIC_WORD(TimeSignatureOptionsTarget, timeSignatureDistanceSelector, 1, 0, timeUpperLiftParts),
    MUS_NUMERIC_WORD(TimeSignatureOptionsTarget, timeSignatureDistanceSelector, 1, 1, timeLowerLiftParts),
    MUS_NUMERIC_WORD(TimeSignatureOptionsTarget, timeSignatureDistanceSelector, 1, 2, timeAbrvLiftParts),
    MUS_NUMERIC_WORD(TimeSignatureOptionsTarget, timeSignatureAbbreviationSelector, 0, 2, timeSigDoAbrvCommon),
    MUS_NUMERIC_WORD(TimeSignatureOptionsTarget, timeSignatureAbbreviationSelector, 0, 3, timeSigDoAbrvCut),
    MUS_NUMERIC_WORD(TimeSignatureOptionsTarget, timeSignatureCompositeSelector, 0, 1, numCompositeDecimalPlaces),
    MUS_NUMERIC_BIT(TimeSignatureOptionsTarget, courtesyChangeSelector, 0, 3, 1, cautionaryTimeChanges),
    MUS_NUMERIC_WORD(TimeSignatureOptionsTarget, timeSignatureVerticalSelector, 0, 0, timeLowerLift),
    MUS_NUMERIC_WORD(TimeSignatureOptionsTarget, timeSignatureVerticalSelector, 0, 1, timeAbrvLift),
};

const FieldMapping classTimeSignatureFields[] = {
    MUS_CLASS_WORD(TimeSignatureOptionsTarget, numericGlobalClass(timeSignatureDistanceSelector), GLOBALS_CMPER, classWordOffset(5), timeUpperLift),
    MUS_CLASS_WORD(TimeSignatureOptionsTarget, numericGlobalClass(timeSignatureDistanceSelector), GLOBALS_CMPER, classWordOffset(3), timeFront),
    MUS_CLASS_WORD(TimeSignatureOptionsTarget, numericGlobalClass(timeSignatureDistanceSelector), GLOBALS_CMPER, classWordOffset(4), timeBack),
    MUS_CLASS_WORD(TimeSignatureOptionsTarget, numericGlobalClass(timeSignatureDistanceSelector), GLOBALS_CMPER, classWordOffset(9), timeFrontParts),
    MUS_CLASS_WORD(TimeSignatureOptionsTarget, numericGlobalClass(timeSignatureDistanceSelector), GLOBALS_CMPER, classWordOffset(10), timeBackParts),
    MUS_CLASS_WORD(
        TimeSignatureOptionsTarget, numericGlobalClass(timeSignatureDistanceSelector), GLOBALS_CMPER, classWordOffset(6), timeUpperLiftParts),
    MUS_CLASS_WORD(
        TimeSignatureOptionsTarget, numericGlobalClass(timeSignatureDistanceSelector), GLOBALS_CMPER, classWordOffset(7), timeLowerLiftParts),
    MUS_CLASS_WORD(
        TimeSignatureOptionsTarget, numericGlobalClass(timeSignatureDistanceSelector), GLOBALS_CMPER, classWordOffset(8), timeAbrvLiftParts),
    MUS_CLASS_WORD(
        TimeSignatureOptionsTarget, numericGlobalClass(timeSignatureAbbreviationSelector), GLOBALS_CMPER, classWordOffset(2), timeSigDoAbrvCommon),
    MUS_CLASS_WORD(
        TimeSignatureOptionsTarget, numericGlobalClass(timeSignatureAbbreviationSelector), GLOBALS_CMPER, classWordOffset(3), timeSigDoAbrvCut),
    MUS_CLASS_WORD(
        TimeSignatureOptionsTarget, numericGlobalClass(timeSignatureCompositeSelector), GLOBALS_CMPER, classWordOffset(1), numCompositeDecimalPlaces),
    MUS_CLASS_BIT(
        TimeSignatureOptionsTarget, numericGlobalClass(courtesyChangeSelector), GLOBALS_CMPER, classWordOffset(3), 1, cautionaryTimeChanges),
    MUS_CLASS_WORD(TimeSignatureOptionsTarget, numericGlobalClass(timeSignatureVerticalSelector), GLOBALS_CMPER, classWordOffset(0), timeLowerLift),
    MUS_CLASS_WORD(TimeSignatureOptionsTarget, numericGlobalClass(timeSignatureVerticalSelector), GLOBALS_CMPER, classWordOffset(1), timeAbrvLift),
};

const MappingTable& codaTimeSignatureTable()
{
    static const MappingTable table{.reportPrefix = timeSignatureOptionsReportPrefix,
        .epochs = EpochMask::CodaBanner,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TimeSignatureOptionsTarget>,
        .fields = codaTimeSignatureFields,
        .fieldCount = std::size(codaTimeSignatureFields)};
    return table;
}

const MappingTable& fixedTimeSignatureTable()
{
    static const MappingTable table{.reportPrefix = timeSignatureOptionsReportPrefix,
        .epochs = EpochMask::Uncompressed | EpochMask::Dcl,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TimeSignatureOptionsTarget>,
        .fields = fixedTimeSignatureFields,
        .fieldCount = std::size(fixedTimeSignatureFields)};
    return table;
}

const MappingTable& classTimeSignatureTable()
{
    static const MappingTable table{.reportPrefix = timeSignatureOptionsReportPrefix,
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TimeSignatureOptionsTarget>,
        .fields = classTimeSignatureFields,
        .fieldCount = std::size(classTimeSignatureFields)};
    return table;
}

void reportCodaTimeSignatureDefaults(const ImportContext& context)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        if (context.profile.epoch != FormatEpoch::CodaBanner) {
            return;
        }
        const auto target = context.document->getOptions()->get<TimeSignatureOptionsTarget>();
        if (!target) {
            return;
        }

        const auto key = reporting.template instanceKey<TimeSignatureOptionsTarget>();
        const auto reportDefault = [&](const char* member, std::int64_t value) {
            reporting.report().setField(key, member, {Reporting::Origin::Finale27Default, 0, 0, value});
        };
        // The Coda layout has no supported source for these later fields.
        reportDefault("cautionaryTimeChanges", target->cautionaryTimeChanges);
        reportDefault("timeLowerLift", target->timeLowerLift);
        reportDefault("timeAbrvLift", target->timeAbrvLift);
    });
}

void applySharedTimeSignatureDistances(const ImportContext& context)
{
    if (context.profile.epoch == FormatEpoch::ZlibLegacy) {
        return;
    }
    const auto distances = readGlobalWords(context.index, context.profile, timeSignatureDistanceSelector);
    if (!distances.present || distances.words.size() >= timeSignatureDistanceWordsWithParts) {
        return;
    }

    const auto pooled = context.document->getOptions()->get<TimeSignatureOptionsTarget>();
    if (!pooled) {
        return;
    }
    const auto target = std::const_pointer_cast<TimeSignatureOptionsTarget>(pooled);
    const auto reportShared = [&](const char* member, musx::dom::Evpu& parts, musx::dom::Evpu score) {
        parts = score;
        withReporting(context.report,
            [&]<typename Reporting>(Reporting& reporting) { reporting.template behaviorField<TimeSignatureOptionsTarget>(member, score); });
    };

    // A fixed-row selector-18 family without its second incidence has one set of distances
    // shared by score and parts. A second incidence carries independent parts values.
    reportShared("timeFrontParts", target->timeFrontParts, target->timeFront);
    reportShared("timeBackParts", target->timeBackParts, target->timeBack);
    reportShared("timeUpperLiftParts", target->timeUpperLiftParts, target->timeUpperLift);
    reportShared("timeLowerLiftParts", target->timeLowerLiftParts, target->timeLowerLift);
    reportShared("timeAbrvLiftParts", target->timeAbrvLiftParts, target->timeAbrvLift);
}

} // namespace

void importTimeSignatureOptions(const ImportContext& context)
{
    reportCodaTimeSignatureDefaults(context);
    applyMappingTables({&codaTimeSignatureTable(), &fixedTimeSignatureTable(), &classTimeSignatureTable()}, context.index, context.profile,
        context.document, context.report);
    applySharedTimeSignatureDistances(context);
}

} // namespace options
} // namespace finale_mus_reader
