// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include <iterator>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using AlternateNotationOptionsTarget = musx::dom::options::AlternateNotationOptions;

constexpr std::uint16_t alternateNotationSelector = 22;
constexpr std::uint16_t alternateNotationStemSelector = 43;
constexpr std::uint16_t alternateNotationNumberSelector = 46;

// Weak: before Finale 97, the arrival of selector 46 marks the layout whose six slash
// offsets use an origin one staff space above the later origin. Selector 46 stores another
// member of this option family and is absent from the earlier layout; it does not distinguish
// the Finale 97 change itself, so that upper boundary remains version-gated.
std::optional<std::int64_t> adjustEarlyAlternateNotationOrigin(
    std::int64_t value, const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    if (!sourcePredatesVersion(profile, FormatEpoch::UncompressedLegacy, versions::finale97)
        || !readGlobalWords(index, profile, alternateNotationNumberSelector).present) {
        return std::nullopt;
    }
    return value - static_cast<std::int64_t>(musx::dom::EVPU_PER_SPACE);
}

template <typename T>
void applyAlternateNotationBehavior(
    AlternateNotationOptionsTarget& target, T AlternateNotationOptionsTarget::* member, const char* name, T value, ImportReport& report)
{
    target.*member = value;
    withReporting(
        report, [&]<typename Reporting>(Reporting& reporting) { reporting.template behaviorField<AlternateNotationOptionsTarget>(name, value); });
}

template <typename T>
void adjustAlternateNotationValue(
    AlternateNotationOptionsTarget& target, T AlternateNotationOptionsTarget::* member, const char* name, T adjustment, ImportReport& report)
{
    target.*member -= adjustment;
    withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
        if (auto* info = reporting.report().findField(reporting.template instanceKey<AlternateNotationOptionsTarget>(), name)) {
            info->origin = Reporting::Origin::LegacyMusAdjusted;
        }
    });
}

// Weak: before selector 46, selector 43 marks the layout that stores three slash offsets
// from an origin two staff spaces above the modern one. Without selector 43 none of the six
// slash offsets is operative. Selector presence is the lower boundary because these sparse
// early version ranges do not state when the corresponding settings became available.
void applyPreSelector46Behavior(const ImportContext& context, AlternateNotationOptionsTarget& target)
{
    if (!sourcePredatesVersion(context.profile, FormatEpoch::UncompressedLegacy, versions::finale97)
        || readGlobalWords(context.index, context.profile, alternateNotationNumberSelector).present) {
        return;
    }

    const auto oneSpace = static_cast<decltype(target.halfSlashLift)>(musx::dom::EVPU_PER_SPACE);
#define APPLY_ALTERNATE_BEHAVIOR(member, value) \
    applyAlternateNotationBehavior(             \
        target, &AlternateNotationOptionsTarget::member, #member, static_cast<decltype(target.member)>(value), context.report)
    APPLY_ALTERNATE_BEHAVIOR(halfSlashLift, -oneSpace);
    APPLY_ALTERNATE_BEHAVIOR(wholeSlashLift, -oneSpace);
    APPLY_ALTERNATE_BEHAVIOR(dWholeSlashLift, -oneSpace);

    const bool hasStemOffsets = readGlobalWords(context.index, context.profile, alternateNotationStemSelector).present;
    if (hasStemOffsets) {
#define ADJUST_ALTERNATE_VALUE(member, value) \
    adjustAlternateNotationValue(             \
        target, &AlternateNotationOptionsTarget::member, #member, static_cast<decltype(target.member)>(value), context.report)
        const auto twoSpaces = static_cast<decltype(target.halfSlashLift)>(2 * oneSpace);
        ADJUST_ALTERNATE_VALUE(halfSlashStemLift, twoSpaces);
        ADJUST_ALTERNATE_VALUE(quartSlashStemLift, twoSpaces);
        ADJUST_ALTERNATE_VALUE(quartSlashLift, twoSpaces);
#undef ADJUST_ALTERNATE_VALUE
    } else {
        APPLY_ALTERNATE_BEHAVIOR(halfSlashStemLift, -oneSpace);
        APPLY_ALTERNATE_BEHAVIOR(quartSlashStemLift, -oneSpace);
        APPLY_ALTERNATE_BEHAVIOR(quartSlashLift, -oneSpace);
    }
    APPLY_ALTERNATE_BEHAVIOR(twoMeasNumLift, 0);
#undef APPLY_ALTERNATE_BEHAVIOR
}

// The fields occupy the same numeric-global word slots in every epoch. Fixed-row encodings
// address them by selector and the zlib encoding coalesces each selector into the class id
// derived by numericGlobalClass.
const FieldMapping fixedRowAlternateNotationFields[] = {
    MUS_WORD_ADJUSTED(AlternateNotationOptionsTarget, "22", GLOBALS_CMPER, 0, 1, &adjustEarlyAlternateNotationOrigin, halfSlashLift),
    MUS_WORD_ADJUSTED(AlternateNotationOptionsTarget, "22", GLOBALS_CMPER, 0, 2, &adjustEarlyAlternateNotationOrigin, wholeSlashLift),
    MUS_WORD_ADJUSTED(AlternateNotationOptionsTarget, "22", GLOBALS_CMPER, 0, 3, &adjustEarlyAlternateNotationOrigin, dWholeSlashLift),
    MUS_WORD_ADJUSTED(AlternateNotationOptionsTarget, "43", GLOBALS_CMPER, 0, 3, &adjustEarlyAlternateNotationOrigin, halfSlashStemLift),
    MUS_WORD_ADJUSTED(AlternateNotationOptionsTarget, "43", GLOBALS_CMPER, 0, 4, &adjustEarlyAlternateNotationOrigin, quartSlashStemLift),
    MUS_WORD_ADJUSTED(AlternateNotationOptionsTarget, "43", GLOBALS_CMPER, 0, 5, &adjustEarlyAlternateNotationOrigin, quartSlashLift),
    MUS_WORD(AlternateNotationOptionsTarget, "46", GLOBALS_CMPER, 0, 5, twoMeasNumLift),
};

const FieldMapping classRecordAlternateNotationFields[] = {
    MUS_CLASS_WORD(AlternateNotationOptionsTarget, numericGlobalClass(alternateNotationSelector), GLOBALS_CMPER, classWordOffset(1), halfSlashLift),
    MUS_CLASS_WORD(AlternateNotationOptionsTarget, numericGlobalClass(alternateNotationSelector), GLOBALS_CMPER, classWordOffset(2), wholeSlashLift),
    MUS_CLASS_WORD(AlternateNotationOptionsTarget, numericGlobalClass(alternateNotationSelector), GLOBALS_CMPER, classWordOffset(3), dWholeSlashLift),
    MUS_CLASS_WORD(
        AlternateNotationOptionsTarget, numericGlobalClass(alternateNotationStemSelector), GLOBALS_CMPER, classWordOffset(3), halfSlashStemLift),
    MUS_CLASS_WORD(
        AlternateNotationOptionsTarget, numericGlobalClass(alternateNotationStemSelector), GLOBALS_CMPER, classWordOffset(4), quartSlashStemLift),
    MUS_CLASS_WORD(
        AlternateNotationOptionsTarget, numericGlobalClass(alternateNotationStemSelector), GLOBALS_CMPER, classWordOffset(5), quartSlashLift),
    MUS_CLASS_WORD(
        AlternateNotationOptionsTarget, numericGlobalClass(alternateNotationNumberSelector), GLOBALS_CMPER, classWordOffset(5), twoMeasNumLift),
};

const MappingTable& fixedRowAlternateNotationTable()
{
    static const MappingTable table{.reportPrefix = "options.alternateNotationOptions",
        .epochs = EpochMask::CodaBanner | EpochMask::FixedRow,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<AlternateNotationOptionsTarget>,
        .fields = fixedRowAlternateNotationFields,
        .fieldCount = std::size(fixedRowAlternateNotationFields)};
    return table;
}

const MappingTable& classRecordAlternateNotationTable()
{
    static const MappingTable table{.reportPrefix = "options.alternateNotationOptions",
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<AlternateNotationOptionsTarget>,
        .fields = classRecordAlternateNotationFields,
        .fieldCount = std::size(classRecordAlternateNotationFields)};
    return table;
}

} // namespace

void importAlternateNotationOptions(const ImportContext& context)
{
    applyMappingTables(
        {&fixedRowAlternateNotationTable(), &classRecordAlternateNotationTable()}, context.index, context.profile, context.document, context.report);
    for (const auto& target : enumerateOptionsTarget<AlternateNotationOptionsTarget>(context.document)) {
        applyPreSelector46Behavior(context, *static_cast<AlternateNotationOptionsTarget*>(target.instance));
    }
}

} // namespace options
} // namespace finale_mus_reader
