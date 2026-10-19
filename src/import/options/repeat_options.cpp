// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string_view>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using RepeatTarget = musx::dom::options::RepeatOptions;

constexpr std::uint16_t repeatSelector(std::string_view tag)
{
    return static_cast<std::uint16_t>((tag[0] - '0') * 10 + tag[1] - '0');
}

constexpr std::string_view bracketHeightTag = "05";
constexpr std::string_view maxPassesTag = "20";
constexpr std::string_view periodTag = "69";
constexpr std::string_view repeatLineTag = "70";
constexpr std::string_view repeatDotTag = "71";
constexpr std::string_view endingTag = "72";
constexpr std::string_view endingBackHookTag = "76";

RepeatTarget::WingStyle repeatWingStyle(std::int64_t value)
{
    // Legacy order is none, single, double, curved; musxdom follows the XML order
    // none, curved, single, double.
    switch (value) {
    case 1: return RepeatTarget::WingStyle::SingleLine;
    case 2: return RepeatTarget::WingStyle::DoubleLine;
    case 3: return RepeatTarget::WingStyle::Curved;
    default: return RepeatTarget::WingStyle::None;
    }
}

std::int64_t repeatMaxPasses(std::int64_t value)
{
    // Zero is the older format's sentinel for the twenty-pass behavior that later
    // releases store explicitly.
    return value == 0 ? 20 : value;
}

bool hasRepeatOptionsLayout(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    // A Coda row with the same two-character tag is not a numeric global in the later
    // fixed-row sense. Selector 72 is part of every located RepeatOptions family in the
    // encodings that support numeric globals; its absence retains the seeded object.
    if (profile.epoch == FormatEpoch::CodaBanner) {
        return false;
    }
    return readGlobalWords(index, profile, repeatSelector(endingTag)).present;
}

// The fixed-row representation is selected by its own record family rather than by
// dating the file. This leaves earlier uncompressed files at the seeded defaults.
const FieldMapping repeatFields[] = {
    MUS_WORD(RepeatTarget, bracketHeightTag, GLOBALS_CMPER, 0, 3, bracketHeight),
    MUS_WORD_AS_IF(RepeatTarget, maxPassesTag, GLOBALS_CMPER, 0, 3, nullptr, maxPasses, repeatMaxPasses(value)),
    MUS_WORD(RepeatTarget, periodTag, GLOBALS_CMPER, 0, 1, addPeriod),
    MUS_WORD(RepeatTarget, repeatLineTag, GLOBALS_CMPER, 0, 0, thickLineWidth),
    MUS_WORD(RepeatTarget, repeatLineTag, GLOBALS_CMPER, 0, 1, thinLineWidth),
    MUS_WORD(RepeatTarget, repeatLineTag, GLOBALS_CMPER, 0, 2, lineSpace),
    MUS_WORD(RepeatTarget, repeatLineTag, GLOBALS_CMPER, 0, 3, backToBackStyle),
    MUS_WORD(RepeatTarget, repeatLineTag, GLOBALS_CMPER, 0, 4, forwardDotHPos),
    MUS_WORD(RepeatTarget, repeatLineTag, GLOBALS_CMPER, 0, 5, backwardDotHPos),
    MUS_WORD(RepeatTarget, repeatDotTag, GLOBALS_CMPER, 0, 0, upperDotVPos),
    MUS_WORD(RepeatTarget, repeatDotTag, GLOBALS_CMPER, 0, 1, lowerDotVPos),
    MUS_WORD_AS_IF(RepeatTarget, repeatDotTag, GLOBALS_CMPER, 0, 2, nullptr, wingStyle, repeatWingStyle(value)),
    MUS_WORD(RepeatTarget, repeatDotTag, GLOBALS_CMPER, 0, 3, afterClefSpace),
    MUS_WORD(RepeatTarget, repeatDotTag, GLOBALS_CMPER, 0, 4, afterKeySpace),
    MUS_WORD(RepeatTarget, repeatDotTag, GLOBALS_CMPER, 0, 5, afterTimeSpace),
    MUS_WORD(RepeatTarget, endingTag, GLOBALS_CMPER, 0, 0, bracketHookLen),
    MUS_WORD(RepeatTarget, endingTag, GLOBALS_CMPER, 0, 1, bracketLineWidth),
    MUS_WORD(RepeatTarget, endingTag, GLOBALS_CMPER, 0, 2, bracketStartInset),
    MUS_WORD(RepeatTarget, endingTag, GLOBALS_CMPER, 0, 3, bracketEndInset),
    MUS_WORD(RepeatTarget, endingTag, GLOBALS_CMPER, 0, 4, bracketTextHPos),
    MUS_WORD(RepeatTarget, endingTag, GLOBALS_CMPER, 0, 5, bracketTextVPos),
    MUS_WORD(RepeatTarget, endingBackHookTag, GLOBALS_CMPER, 0, 2, bracketEndHookLen),
};

// Finale 2007 through Finale 2012. The same numeric globals are class records,
// and their single fixed-row incidence becomes one 12-byte payload.
const FieldMapping classRepeatFields[] = {
    MUS_CLASS_WORD(RepeatTarget, numericGlobalClass(repeatSelector(bracketHeightTag)), GLOBALS_CMPER, classWordOffset(3), bracketHeight),
    MUS_CLASS_WORD_AS_IF(RepeatTarget, numericGlobalClass(repeatSelector(maxPassesTag)), GLOBALS_CMPER, classWordOffset(3), nullptr, maxPasses,
        repeatMaxPasses(value)),
    MUS_CLASS_WORD(RepeatTarget, numericGlobalClass(repeatSelector(periodTag)), GLOBALS_CMPER, classWordOffset(1), addPeriod),
    MUS_CLASS_WORD(RepeatTarget, numericGlobalClass(repeatSelector(repeatLineTag)), GLOBALS_CMPER, classWordOffset(0), thickLineWidth),
    MUS_CLASS_WORD(RepeatTarget, numericGlobalClass(repeatSelector(repeatLineTag)), GLOBALS_CMPER, classWordOffset(1), thinLineWidth),
    MUS_CLASS_WORD(RepeatTarget, numericGlobalClass(repeatSelector(repeatLineTag)), GLOBALS_CMPER, classWordOffset(2), lineSpace),
    MUS_CLASS_WORD(RepeatTarget, numericGlobalClass(repeatSelector(repeatLineTag)), GLOBALS_CMPER, classWordOffset(3), backToBackStyle),
    MUS_CLASS_WORD(RepeatTarget, numericGlobalClass(repeatSelector(repeatLineTag)), GLOBALS_CMPER, classWordOffset(4), forwardDotHPos),
    MUS_CLASS_WORD(RepeatTarget, numericGlobalClass(repeatSelector(repeatLineTag)), GLOBALS_CMPER, classWordOffset(5), backwardDotHPos),
    MUS_CLASS_WORD(RepeatTarget, numericGlobalClass(repeatSelector(repeatDotTag)), GLOBALS_CMPER, classWordOffset(0), upperDotVPos),
    MUS_CLASS_WORD(RepeatTarget, numericGlobalClass(repeatSelector(repeatDotTag)), GLOBALS_CMPER, classWordOffset(1), lowerDotVPos),
    MUS_CLASS_WORD_AS_IF(RepeatTarget, numericGlobalClass(repeatSelector(repeatDotTag)), GLOBALS_CMPER, classWordOffset(2), nullptr, wingStyle,
        repeatWingStyle(value)),
    MUS_CLASS_WORD(RepeatTarget, numericGlobalClass(repeatSelector(repeatDotTag)), GLOBALS_CMPER, classWordOffset(3), afterClefSpace),
    MUS_CLASS_WORD(RepeatTarget, numericGlobalClass(repeatSelector(repeatDotTag)), GLOBALS_CMPER, classWordOffset(4), afterKeySpace),
    MUS_CLASS_WORD(RepeatTarget, numericGlobalClass(repeatSelector(repeatDotTag)), GLOBALS_CMPER, classWordOffset(5), afterTimeSpace),
    MUS_CLASS_WORD(RepeatTarget, numericGlobalClass(repeatSelector(endingTag)), GLOBALS_CMPER, classWordOffset(0), bracketHookLen),
    MUS_CLASS_WORD(RepeatTarget, numericGlobalClass(repeatSelector(endingTag)), GLOBALS_CMPER, classWordOffset(1), bracketLineWidth),
    MUS_CLASS_WORD(RepeatTarget, numericGlobalClass(repeatSelector(endingTag)), GLOBALS_CMPER, classWordOffset(2), bracketStartInset),
    MUS_CLASS_WORD(RepeatTarget, numericGlobalClass(repeatSelector(endingTag)), GLOBALS_CMPER, classWordOffset(3), bracketEndInset),
    MUS_CLASS_WORD(RepeatTarget, numericGlobalClass(repeatSelector(endingTag)), GLOBALS_CMPER, classWordOffset(4), bracketTextHPos),
    MUS_CLASS_WORD(RepeatTarget, numericGlobalClass(repeatSelector(endingTag)), GLOBALS_CMPER, classWordOffset(5), bracketTextVPos),
    MUS_CLASS_WORD(RepeatTarget, numericGlobalClass(repeatSelector(endingBackHookTag)), GLOBALS_CMPER, classWordOffset(2), bracketEndHookLen),
};

constexpr const char* repeatReportPrefix = "options.repeatOptions";

const MappingTable& repeatTable()
{
    static const MappingTable table{.reportPrefix = repeatReportPrefix,
        .epochs = EpochMask::FixedRow,
        .applies = &hasRepeatOptionsLayout,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<RepeatTarget>,
        .fields = repeatFields,
        .fieldCount = std::size(repeatFields)};
    return table;
}

const MappingTable& classRepeatTable()
{
    static const MappingTable table{.reportPrefix = repeatReportPrefix,
        .epochs = EpochMask::Zlib,
        .applies = &hasRepeatOptionsLayout,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<RepeatTarget>,
        .fields = classRepeatFields,
        .fieldCount = std::size(classRepeatFields)};
    return table;
}

} // namespace

void importRepeatOptions(const ImportContext& context)
{
    applyMappingTables({&repeatTable(), &classRepeatTable()}, context.index, context.profile, context.document, context.report);

    const auto pooled = context.document->getOptions()->get<RepeatTarget>();
    if (!pooled) {
        return;
    }
    const auto target = std::const_pointer_cast<RepeatTarget>(pooled);
    if (!hasRepeatOptionsLayout(context.index, context.profile)) {
        // Believed: documents without the numeric-global family use these fixed
        // repeat and ending-display values.
        target->addPeriod = false;
        target->thinLineWidth = 224;
        target->upperDotVPos = 0;
        target->lowerDotVPos = 0;
        target->bracketLineWidth = 224;
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            reporting.template behaviorField<RepeatTarget>("addPeriod", 0);
            reporting.template behaviorField<RepeatTarget>("thinLineWidth", 0);
            reporting.template behaviorField<RepeatTarget>("upperDotVPos", 0);
            reporting.template behaviorField<RepeatTarget>("lowerDotVPos", 0);
            reporting.template behaviorField<RepeatTarget>("bracketLineWidth", 0);
        });
        target->bracketEndAnchorThinLine = false;
        withReporting(context.report,
            [&]<typename Reporting>(Reporting& reporting) { reporting.template behaviorField<RepeatTarget>("bracketEndAnchorThinLine", 0); });
        return;
    }
    // Files that carry the legacy RepeatOptions family use the older anchor behavior.
    target->bracketEndAnchorThinLine = false;
    withReporting(context.report,
        [&]<typename Reporting>(Reporting& reporting) { reporting.template behaviorField<RepeatTarget>("bracketEndAnchorThinLine", 0); });
}

} // namespace options
} // namespace finale_mus_reader
