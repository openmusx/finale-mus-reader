// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include <cstdint>
#include <iterator>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using FlagOptionsTarget = musx::dom::options::FlagOptions;

constexpr const char* flagOptionsReportPrefix = "options.flagOptions";
constexpr std::uint16_t flagBehaviorSelector = 5;
constexpr std::uint16_t flagHorizontalSelector = 73;
constexpr std::uint16_t flagVerticalSelector = 74;
constexpr std::uint16_t straightFlagSelector = 75;
constexpr std::uint16_t flagSpacingSelector = 76;
constexpr std::uint16_t codaFlagPositionSelector = 10;

constexpr musx::dom::Efix codaCurvedUpVertical = 5376;
constexpr musx::dom::Efix codaCurvedDownVertical = -5376;
constexpr musx::dom::Efix codaCurvedUpVertical16 = 5376;
constexpr musx::dom::Efix codaCurvedDownVertical16 = -5655;
constexpr musx::dom::Efix codaOriginVertical = 1536;
constexpr musx::dom::Evpu codaFlagSpacing = 24;

const FieldMapping fixedFlagFields[] = {
    MUS_BIT(FlagOptionsTarget, "05", GLOBALS_CMPER, 0, 2, 0, straightFlags),
    MUS_WORD(FlagOptionsTarget, "73", GLOBALS_CMPER, 0, 0, upHAdj),
    MUS_WORD(FlagOptionsTarget, "73", GLOBALS_CMPER, 0, 1, downHAdj),
    MUS_WORD(FlagOptionsTarget, "73", GLOBALS_CMPER, 0, 2, upHAdj2),
    MUS_WORD(FlagOptionsTarget, "73", GLOBALS_CMPER, 0, 3, downHAdj2),
    MUS_WORD(FlagOptionsTarget, "73", GLOBALS_CMPER, 0, 4, upHAdj16),
    MUS_WORD(FlagOptionsTarget, "73", GLOBALS_CMPER, 0, 5, downHAdj16),
    MUS_WORD(FlagOptionsTarget, "74", GLOBALS_CMPER, 0, 0, upVAdj),
    MUS_WORD(FlagOptionsTarget, "74", GLOBALS_CMPER, 0, 1, downVAdj),
    MUS_WORD(FlagOptionsTarget, "74", GLOBALS_CMPER, 0, 2, upVAdj2),
    MUS_WORD(FlagOptionsTarget, "74", GLOBALS_CMPER, 0, 3, downVAdj2),
    MUS_WORD(FlagOptionsTarget, "74", GLOBALS_CMPER, 0, 4, upVAdj16),
    MUS_WORD(FlagOptionsTarget, "74", GLOBALS_CMPER, 0, 5, downVAdj16),
    MUS_WORD(FlagOptionsTarget, "75", GLOBALS_CMPER, 0, 2, stUpHAdj),
    MUS_WORD(FlagOptionsTarget, "75", GLOBALS_CMPER, 0, 3, stDownHAdj),
    MUS_WORD(FlagOptionsTarget, "75", GLOBALS_CMPER, 0, 4, stUpVAdj),
    MUS_WORD(FlagOptionsTarget, "75", GLOBALS_CMPER, 0, 5, stDownVAdj),
    MUS_WORD(FlagOptionsTarget, "76", GLOBALS_CMPER, 0, 0, flagSpacing),
    MUS_WORD(FlagOptionsTarget, "76", GLOBALS_CMPER, 0, 1, secondaryGroupAdj),
};

const FieldMapping classFlagFields[] = {
    MUS_CLASS_BIT(FlagOptionsTarget, numericGlobalClass(flagBehaviorSelector), GLOBALS_CMPER, classWordOffset(2), 0, straightFlags),
    MUS_CLASS_WORD(FlagOptionsTarget, numericGlobalClass(flagHorizontalSelector), GLOBALS_CMPER, classWordOffset(0), upHAdj),
    MUS_CLASS_WORD(FlagOptionsTarget, numericGlobalClass(flagHorizontalSelector), GLOBALS_CMPER, classWordOffset(1), downHAdj),
    MUS_CLASS_WORD(FlagOptionsTarget, numericGlobalClass(flagHorizontalSelector), GLOBALS_CMPER, classWordOffset(2), upHAdj2),
    MUS_CLASS_WORD(FlagOptionsTarget, numericGlobalClass(flagHorizontalSelector), GLOBALS_CMPER, classWordOffset(3), downHAdj2),
    MUS_CLASS_WORD(FlagOptionsTarget, numericGlobalClass(flagHorizontalSelector), GLOBALS_CMPER, classWordOffset(4), upHAdj16),
    MUS_CLASS_WORD(FlagOptionsTarget, numericGlobalClass(flagHorizontalSelector), GLOBALS_CMPER, classWordOffset(5), downHAdj16),
    MUS_CLASS_WORD(FlagOptionsTarget, numericGlobalClass(flagVerticalSelector), GLOBALS_CMPER, classWordOffset(0), upVAdj),
    MUS_CLASS_WORD(FlagOptionsTarget, numericGlobalClass(flagVerticalSelector), GLOBALS_CMPER, classWordOffset(1), downVAdj),
    MUS_CLASS_WORD(FlagOptionsTarget, numericGlobalClass(flagVerticalSelector), GLOBALS_CMPER, classWordOffset(2), upVAdj2),
    MUS_CLASS_WORD(FlagOptionsTarget, numericGlobalClass(flagVerticalSelector), GLOBALS_CMPER, classWordOffset(3), downVAdj2),
    MUS_CLASS_WORD(FlagOptionsTarget, numericGlobalClass(flagVerticalSelector), GLOBALS_CMPER, classWordOffset(4), upVAdj16),
    MUS_CLASS_WORD(FlagOptionsTarget, numericGlobalClass(flagVerticalSelector), GLOBALS_CMPER, classWordOffset(5), downVAdj16),
    MUS_CLASS_WORD(FlagOptionsTarget, numericGlobalClass(straightFlagSelector), GLOBALS_CMPER, classWordOffset(2), stUpHAdj),
    MUS_CLASS_WORD(FlagOptionsTarget, numericGlobalClass(straightFlagSelector), GLOBALS_CMPER, classWordOffset(3), stDownHAdj),
    MUS_CLASS_WORD(FlagOptionsTarget, numericGlobalClass(straightFlagSelector), GLOBALS_CMPER, classWordOffset(4), stUpVAdj),
    MUS_CLASS_WORD(FlagOptionsTarget, numericGlobalClass(straightFlagSelector), GLOBALS_CMPER, classWordOffset(5), stDownVAdj),
    MUS_CLASS_WORD(FlagOptionsTarget, numericGlobalClass(flagSpacingSelector), GLOBALS_CMPER, classWordOffset(0), flagSpacing),
    MUS_CLASS_WORD(FlagOptionsTarget, numericGlobalClass(flagSpacingSelector), GLOBALS_CMPER, classWordOffset(1), secondaryGroupAdj),
};

const MappingTable& fixedFlagTable()
{
    static const MappingTable table{.reportPrefix = flagOptionsReportPrefix,
        .epochs = EpochMask::Uncompressed | EpochMask::Dcl,
        .applies = &storesFinale35OptionLayout,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<FlagOptionsTarget>,
        .fields = fixedFlagFields,
        .fieldCount = std::size(fixedFlagFields)};
    return table;
}

template <typename Reporting>
void reportCodaFlagField(Reporting& reporting, const char* member, typename Reporting::Origin origin, std::size_t blockOffset,
    std::size_t decodedOffset, std::int64_t rawValue)
{
    reporting.report().setField(reporting.template instanceKey<FlagOptionsTarget>(), member, {origin, blockOffset, decodedOffset, rawValue});
}

void reportCodaFlagOptions(const ImportContext& context, const FlagOptionsTarget& target, const GlobalSelectorWords& position)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto reportDefault = [&](const char* member, std::int64_t value) {
            reportCodaFlagField(reporting, member, Reporting::Origin::Finale27Default, 0, 0, value);
        };
        reportDefault("upHAdj", target.upHAdj);
        reportDefault("upHAdj2", target.upHAdj2);
        reportDefault("upHAdj16", target.upHAdj16);
        const auto reportBehavior = [&](const char* member, std::int64_t value) {
            reportCodaFlagField(reporting, member, Reporting::Origin::LegacyBehavior, 0, 0, value);
        };
        reportBehavior("downHAdj", target.downHAdj);
        reportBehavior("downHAdj2", target.downHAdj2);
        reportBehavior("downHAdj16", target.downHAdj16);
        reportBehavior("upVAdj2", target.upVAdj2);
        reportBehavior("downVAdj2", target.downVAdj2);
        reportBehavior("stUpHAdj", target.stUpHAdj);
        reportBehavior("stDownHAdj", target.stDownHAdj);
        reportBehavior("stUpVAdj", target.stUpVAdj);
        reportBehavior("stDownVAdj", target.stDownVAdj);
        reportBehavior("flagSpacing", target.flagSpacing);
        reportBehavior("secondaryGroupAdj", target.secondaryGroupAdj);
        for (auto member : {"upVAdj", "upVAdj16"}) {
            reportCodaFlagField(
                reporting, member, Reporting::Origin::LegacyMusAdjusted, position.blockOffset, position.decodedOffset, position.words[4]);
        }
        for (auto member : {"downVAdj", "downVAdj16"}) {
            reportCodaFlagField(
                reporting, member, Reporting::Origin::LegacyMusAdjusted, position.blockOffset, position.decodedOffset, position.words[5]);
        }
    });
}

void importCodaFlagOptions(const ImportContext& context)
{
    if (context.profile.epoch != FormatEpoch::CodaBanner) {
        return;
    }

    const auto position = readGlobalWords(context.index, context.profile, codaFlagPositionSelector);
    if (!position.present || position.words.size() < 6) {
        return;
    }

    const auto pooled = context.document->getOptions()->get<FlagOptionsTarget>();
    if (!pooled) {
        return;
    }
    const auto target = std::const_pointer_cast<FlagOptionsTarget>(pooled);

    // Coda stores vertical-origin switches rather than the later independent coordinates.
    // Words 4 and 5 select the upward and downward origins independently. Word 3 changes
    // with the horizontal-origin control, but its conversion to modern coordinates is
    // unverified, so the horizontal coordinates retain their seeded defaults.
    const auto upVertical = position.words[4] ? codaOriginVertical : codaCurvedUpVertical;
    const auto downVertical = position.words[5] ? -codaOriginVertical : codaCurvedDownVertical;
    const auto downVertical16 = position.words[5] ? -codaOriginVertical : codaCurvedDownVertical16;

    target->downHAdj = target->downHAdj2 = target->downHAdj16 = 0;
    target->upVAdj = target->upVAdj16 = upVertical;
    target->downVAdj = downVertical;
    target->upVAdj2 = codaOriginVertical;
    target->downVAdj2 = -codaOriginVertical;
    target->downVAdj16 = downVertical16;
    target->stUpHAdj = target->stDownHAdj = 0;
    target->stUpVAdj = codaOriginVertical;
    target->stDownVAdj = -codaOriginVertical;
    target->flagSpacing = codaFlagSpacing;
    target->secondaryGroupAdj = 0;

    reportCodaFlagOptions(context, *target, position);
}

const MappingTable& classFlagTable()
{
    static const MappingTable table{.reportPrefix = flagOptionsReportPrefix,
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<FlagOptionsTarget>,
        .fields = classFlagFields,
        .fieldCount = std::size(classFlagFields)};
    return table;
}

} // namespace

void importFlagOptions(const ImportContext& context)
{
    if (const auto options = context.document->getOptions()->get<FlagOptionsTarget>()) {
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            reporting.template defaultField<FlagOptionsTarget>("eighthFlagHoist", options->eighthFlagHoist);
        });
    }
    applyMappingTables({&fixedFlagTable(), &classFlagTable()}, context.index, context.profile, context.document, context.report);
    importCodaFlagOptions(context);
}

} // namespace options
} // namespace finale_mus_reader
