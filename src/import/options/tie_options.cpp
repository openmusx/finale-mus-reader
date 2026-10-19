// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "import/shared/legacy_curve_selectors.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using TieOptionsTarget = musx::dom::options::TieOptions;

constexpr std::uint16_t tieScalarSelector = 84;
constexpr std::uint16_t tieConnectionSelector = 85;
constexpr std::uint16_t tieContourSelector = 86;
constexpr std::uint16_t tieTipSelector = 97;
constexpr std::uint16_t tieFrontSelector = 41;
constexpr std::uint16_t earlyShortTieSelector = 17;
constexpr std::uint16_t earlyTieContourSelector = 22;
constexpr std::size_t tieConnectionCount = 12;
constexpr std::size_t tieContourCount = 4;
constexpr std::size_t tieContourWords = 7;
constexpr const char* tieOptionsReportPrefix = "options.tieOptions";

constexpr std::array earlyTieStartConnectionTypes{
    musx::dom::TieConnectStyleType::OverStartPosInner,
    musx::dom::TieConnectStyleType::UnderStartPosInner,
    musx::dom::TieConnectStyleType::OverHighestNoteStemStartPosOver,
    musx::dom::TieConnectStyleType::UnderLowestNoteStemStartPosUnder,
};

constexpr std::array earlyTieEndConnectionTypes{
    musx::dom::TieConnectStyleType::OverEndPosInner,
    musx::dom::TieConnectStyleType::UnderEndPosInner,
    musx::dom::TieConnectStyleType::OverHighestNoteStemEndPosOver,
    musx::dom::TieConnectStyleType::UnderLowestNoteStemEndPosUnder,
};

constexpr auto secondsPlacement(std::int64_t value)
{
    return value == 0 ? TieOptionsTarget::SecondsPlacement::None : TieOptionsTarget::SecondsPlacement::ShiftForSeconds;
}

constexpr auto chordDirection(std::int64_t value)
{
    return value >= 0 && value <= 2 ? static_cast<TieOptionsTarget::ChordTieDirType>(value) : TieOptionsTarget::ChordTieDirType::Unknown;
}

constexpr auto mixedStemDirection(std::int64_t value)
{
    return value >= 0 && value <= 2 ? static_cast<TieOptionsTarget::MixedStemDirection>(value) : TieOptionsTarget::MixedStemDirection::OppositeFirst;
}

const FieldMapping fixedTieScalarFields[] = {
    MUS_NUMERIC_WORD(TieOptionsTarget, tieFrontSelector, 2, 3, frontTieSepar),
    MUS_NUMERIC_WORD(TieOptionsTarget, tieScalarSelector, 0, 0, thicknessRight),
    MUS_NUMERIC_WORD(TieOptionsTarget, tieScalarSelector, 0, 1, thicknessLeft),
    MUS_NUMERIC_WORD(TieOptionsTarget, tieScalarSelector, 0, 2, breakForTimeSigs),
    MUS_NUMERIC_WORD(TieOptionsTarget, tieScalarSelector, 0, 3, breakForKeySigs),
    MUS_NUMERIC_WORD(TieOptionsTarget, tieScalarSelector, 0, 4, breakTimeSigLeftHOffset),
    MUS_NUMERIC_WORD(TieOptionsTarget, tieScalarSelector, 0, 5, breakTimeSigRightHOffset),
    MUS_NUMERIC_WORD(TieOptionsTarget, tieScalarSelector, 1, 0, breakKeySigLeftHOffset),
    MUS_NUMERIC_WORD(TieOptionsTarget, tieScalarSelector, 1, 1, breakKeySigRightHOffset),
    MUS_NUMERIC_WORD(TieOptionsTarget, tieScalarSelector, 1, 2, sysBreakLeftHAdj),
    MUS_NUMERIC_WORD(TieOptionsTarget, tieScalarSelector, 1, 3, sysBreakRightHAdj),
    MUS_NUMERIC_WORD(TieOptionsTarget, tieScalarSelector, 1, 4, useOuterPlacement),
    MUS_NUMERIC_FIELD_AS_IF(TieOptionsTarget, tieScalarSelector, 1, 5, ValueWidth::Word, LongWordOrder::HighFirst, BitRange{}, nullptr, nullptr,
        secondsPlacement, secondsPlacement(value)),
    MUS_NUMERIC_FIELD_AS_IF(TieOptionsTarget, tieScalarSelector, 2, 0, ValueWidth::Word, LongWordOrder::HighFirst, BitRange{}, nullptr, nullptr,
        chordTieDirType, chordDirection(value)),
    MUS_NUMERIC_WORD(TieOptionsTarget, tieScalarSelector, 2, 1, chordTieDirOpposingSeconds),
    MUS_NUMERIC_FIELD_AS_IF(TieOptionsTarget, tieScalarSelector, 2, 2, ValueWidth::Word, LongWordOrder::HighFirst, BitRange{}, nullptr, nullptr,
        mixedStemDirection, mixedStemDirection(value)),
    MUS_NUMERIC_WORD(TieOptionsTarget, tieScalarSelector, 2, 3, afterSingleDot),
    MUS_NUMERIC_WORD(TieOptionsTarget, tieScalarSelector, 2, 4, afterMultipleDots),
    MUS_NUMERIC_WORD(TieOptionsTarget, tieScalarSelector, 2, 5, beforeAcciSingleNote),
    MUS_NUMERIC_FIELD_AS_IF(TieOptionsTarget, tieScalarSelector, 3, 0, ValueWidth::Word, LongWordOrder::HighFirst, (BitRange{1, 1}), nullptr, nullptr,
        specialPosMode, value != 0 ? TieOptionsTarget::SpecialPosMode::Avoid : TieOptionsTarget::SpecialPosMode::None),
    MUS_NUMERIC_WORD(TieOptionsTarget, tieScalarSelector, 3, 1, avoidStaffLinesDistance),
    MUS_NUMERIC_FIELD_AS_IF(TieOptionsTarget, tieScalarSelector, 3, 2, ValueWidth::Word, LongWordOrder::HighFirst, BitRange{}, nullptr, nullptr,
        insetStyle, value == 0 ? TieOptionsTarget::InsetStyle::Fixed : TieOptionsTarget::InsetStyle::Percent),
    MUS_NUMERIC_WORD(TieOptionsTarget, tieScalarSelector, 3, 3, useInterpolation),
    MUS_NUMERIC_WORD(TieOptionsTarget, tieScalarSelector, 3, 4, useTieEndCtlStyle),
    MUS_NUMERIC_WORD(TieOptionsTarget, tieScalarSelector, 3, 5, avoidStaffLinesOnly),
    MUS_NUMERIC_FIELD_AS_IF(TieOptionsTarget, tieTipSelector, 0, 2, ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, nullptr,
        tieTipWidth, static_cast<double>(value) / 10000.0),
};

const FieldMapping classTieScalarFields[] = {
    MUS_CLASS_WORD(TieOptionsTarget, numericGlobalClass(tieFrontSelector), GLOBALS_CMPER, classWordOffset(15), frontTieSepar),
    MUS_CLASS_WORD(TieOptionsTarget, numericGlobalClass(tieScalarSelector), GLOBALS_CMPER, classWordOffset(0), thicknessRight),
    MUS_CLASS_WORD(TieOptionsTarget, numericGlobalClass(tieScalarSelector), GLOBALS_CMPER, classWordOffset(1), thicknessLeft),
    MUS_CLASS_WORD(TieOptionsTarget, numericGlobalClass(tieScalarSelector), GLOBALS_CMPER, classWordOffset(2), breakForTimeSigs),
    MUS_CLASS_WORD(TieOptionsTarget, numericGlobalClass(tieScalarSelector), GLOBALS_CMPER, classWordOffset(3), breakForKeySigs),
    MUS_CLASS_WORD(TieOptionsTarget, numericGlobalClass(tieScalarSelector), GLOBALS_CMPER, classWordOffset(4), breakTimeSigLeftHOffset),
    MUS_CLASS_WORD(TieOptionsTarget, numericGlobalClass(tieScalarSelector), GLOBALS_CMPER, classWordOffset(5), breakTimeSigRightHOffset),
    MUS_CLASS_WORD(TieOptionsTarget, numericGlobalClass(tieScalarSelector), GLOBALS_CMPER, classWordOffset(6), breakKeySigLeftHOffset),
    MUS_CLASS_WORD(TieOptionsTarget, numericGlobalClass(tieScalarSelector), GLOBALS_CMPER, classWordOffset(7), breakKeySigRightHOffset),
    MUS_CLASS_WORD(TieOptionsTarget, numericGlobalClass(tieScalarSelector), GLOBALS_CMPER, classWordOffset(8), sysBreakLeftHAdj),
    MUS_CLASS_WORD(TieOptionsTarget, numericGlobalClass(tieScalarSelector), GLOBALS_CMPER, classWordOffset(9), sysBreakRightHAdj),
    MUS_CLASS_WORD(TieOptionsTarget, numericGlobalClass(tieScalarSelector), GLOBALS_CMPER, classWordOffset(10), useOuterPlacement),
    MUS_CLASS_WORD_AS_IF(TieOptionsTarget, numericGlobalClass(tieScalarSelector), GLOBALS_CMPER, classWordOffset(11), nullptr, secondsPlacement,
        secondsPlacement(value)),
    MUS_CLASS_WORD_AS_IF(
        TieOptionsTarget, numericGlobalClass(tieScalarSelector), GLOBALS_CMPER, classWordOffset(12), nullptr, chordTieDirType, chordDirection(value)),
    MUS_CLASS_WORD(TieOptionsTarget, numericGlobalClass(tieScalarSelector), GLOBALS_CMPER, classWordOffset(13), chordTieDirOpposingSeconds),
    MUS_CLASS_WORD_AS_IF(TieOptionsTarget, numericGlobalClass(tieScalarSelector), GLOBALS_CMPER, classWordOffset(14), nullptr, mixedStemDirection,
        mixedStemDirection(value)),
    MUS_CLASS_WORD(TieOptionsTarget, numericGlobalClass(tieScalarSelector), GLOBALS_CMPER, classWordOffset(15), afterSingleDot),
    MUS_CLASS_WORD(TieOptionsTarget, numericGlobalClass(tieScalarSelector), GLOBALS_CMPER, classWordOffset(16), afterMultipleDots),
    MUS_CLASS_WORD(TieOptionsTarget, numericGlobalClass(tieScalarSelector), GLOBALS_CMPER, classWordOffset(17), beforeAcciSingleNote),
    MUS_CLASS_SELECTED_BITS_AS(TieOptionsTarget, numericGlobalClass(tieScalarSelector), GLOBALS_CMPER, classWordOffset(18), 1, 1, specialPosMode,
        value != 0 ? TieOptionsTarget::SpecialPosMode::Avoid : TieOptionsTarget::SpecialPosMode::None),
    MUS_CLASS_WORD(TieOptionsTarget, numericGlobalClass(tieScalarSelector), GLOBALS_CMPER, classWordOffset(19), avoidStaffLinesDistance),
    MUS_CLASS_WORD_AS_IF(TieOptionsTarget, numericGlobalClass(tieScalarSelector), GLOBALS_CMPER, classWordOffset(20), nullptr, insetStyle,
        value == 0 ? TieOptionsTarget::InsetStyle::Fixed : TieOptionsTarget::InsetStyle::Percent),
    MUS_CLASS_WORD(TieOptionsTarget, numericGlobalClass(tieScalarSelector), GLOBALS_CMPER, classWordOffset(21), useInterpolation),
    MUS_CLASS_WORD(TieOptionsTarget, numericGlobalClass(tieScalarSelector), GLOBALS_CMPER, classWordOffset(22), useTieEndCtlStyle),
    MUS_CLASS_WORD(TieOptionsTarget, numericGlobalClass(tieScalarSelector), GLOBALS_CMPER, classWordOffset(23), avoidStaffLinesOnly),
    MUS_CLASS_FIELD_AS_IF(TieOptionsTarget, numericGlobalClass(tieTipSelector), GLOBALS_CMPER, classWordOffset(2), ValueWidth::Long,
        LongWordOrder::HighFirst, BitRange{}, nullptr, tieTipWidth, static_cast<double>(value) / 10000.0),
};

const MappingTable& fixedTieScalarTable()
{
    // The Coda-banner epoch carries neither selector 41 incidence 2 nor the later
    // tie-option selector families, so it intentionally retains the seeded
    // values.
    static const MappingTable table{.reportPrefix = tieOptionsReportPrefix,
        .epochs = EpochMask::FixedRow,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TieOptionsTarget>,
        .fields = fixedTieScalarFields,
        .fieldCount = std::size(fixedTieScalarFields)};
    return table;
}

const MappingTable& classTieScalarTable()
{
    static const MappingTable table{.reportPrefix = tieOptionsReportPrefix,
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TieOptionsTarget>,
        .fields = classTieScalarFields,
        .fieldCount = std::size(classTieScalarFields)};
    return table;
}

// Called from inside reporting callbacks, so it takes the callback's writer rather than opening
// a nested one.
template <typename Reporting>
void reportRecoveredTieField(Reporting& reporting, std::string_view member, std::int64_t value, std::size_t blockOffset, std::size_t decodedOffset)
{
    reporting.report().setField(
        reporting.template instanceKey<TieOptionsTarget>(), std::string(member), {Reporting::Origin::LegacyMus, blockOffset, decodedOffset, value});
}

void reportAdjustedTieField(
    const ImportContext& context, std::string_view member, std::int64_t value, std::size_t blockOffset, std::size_t decodedOffset)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        reporting.report().setField(reporting.template instanceKey<TieOptionsTarget>(), std::string(member),
            {Reporting::Origin::LegacyMusAdjusted, blockOffset, decodedOffset, value});
    });
}

void reportTieBehaviorField(const ImportContext& context, std::string_view member, std::int64_t value)
{
    withReporting(context.report,
        [&]<typename Reporting>(Reporting& reporting) { reporting.template behaviorField<TieOptionsTarget>(std::string(member), value); });
}

template <typename Reporting>
void reportDefaultTieField(Reporting& reporting, std::string_view member, std::int64_t value)
{
    reporting.template defaultField<TieOptionsTarget>(std::string(member), value);
}

void captureTieConnections(const ImportContext& context, const std::shared_ptr<TieOptionsTarget>& target)
{
    const auto family = readGlobalWords(context.index, context.profile, tieConnectionSelector);
    if (!family.present || family.words.size() < tieConnectionCount * 2) {
        return;
    }

    target->tieConnectStyles.clear();
    for (std::size_t index = 0; index < tieConnectionCount; ++index) {
        const auto type = static_cast<musx::dom::TieConnectStyleType>(index);
        auto style = std::make_shared<TieOptionsTarget::ConnectStyle>();
        style->offsetX = wordAt(family.words, index * 2);
        style->offsetY = wordAt(family.words, index * 2 + 1);
        withReporting(context.report, [&](auto& reporting) {
            const auto prefix = "tieConnectStyles[" + std::to_string(index) + "].";
            reportRecoveredTieField(reporting, prefix + "offsetX", style->offsetX, family.blockOffset, family.decodedOffset);
            reportRecoveredTieField(reporting, prefix + "offsetY", style->offsetY, family.blockOffset, family.decodedOffset);
        });
        target->tieConnectStyles.emplace(type, std::move(style));
    }
}

void captureTieContours(const ImportContext& context, const std::shared_ptr<TieOptionsTarget>& target)
{
    const auto family = readGlobalWords(context.index, context.profile, tieContourSelector);
    if (!family.present || family.words.size() < tieContourCount * tieContourWords) {
        return;
    }

    target->tieControlStyles.clear();
    for (std::size_t index = 0; index < tieContourCount; ++index) {
        const auto first = index * tieContourWords;
        auto style = std::make_shared<TieOptionsTarget::ControlStyle>();
        style->span = wordAt(family.words, first);
        style->cp1 = std::make_shared<TieOptionsTarget::ControlPoint>();
        style->cp1->insetRatio = wordAt(family.words, first + 1);
        style->cp1->height = wordAt(family.words, first + 2);
        style->cp1->insetFixed = wordAt(family.words, first + 3);
        style->cp2 = std::make_shared<TieOptionsTarget::ControlPoint>();
        style->cp2->insetRatio = wordAt(family.words, first + 4);
        style->cp2->height = wordAt(family.words, first + 5);
        style->cp2->insetFixed = wordAt(family.words, first + 6);
        withReporting(context.report, [&](auto& reporting) {
            const auto prefix = "tieControlStyles[" + std::to_string(index) + "].";
            reportRecoveredTieField(reporting, prefix + "span", style->span, family.blockOffset, family.decodedOffset);
            reportRecoveredTieField(reporting, prefix + "cp1.insetRatio", style->cp1->insetRatio, family.blockOffset, family.decodedOffset);
            reportRecoveredTieField(reporting, prefix + "cp1.height", style->cp1->height, family.blockOffset, family.decodedOffset);
            reportRecoveredTieField(reporting, prefix + "cp1.insetFixed", style->cp1->insetFixed, family.blockOffset, family.decodedOffset);
            reportRecoveredTieField(reporting, prefix + "cp2.insetRatio", style->cp2->insetRatio, family.blockOffset, family.decodedOffset);
            reportRecoveredTieField(reporting, prefix + "cp2.height", style->cp2->height, family.blockOffset, family.decodedOffset);
            reportRecoveredTieField(reporting, prefix + "cp2.insetFixed", style->cp2->insetFixed, family.blockOffset, family.decodedOffset);
        });
        target->tieControlStyles.emplace(static_cast<TieOptionsTarget::ControlStyleType>(index), std::move(style));
    }
}

void captureEarlyTieOptions(const ImportContext& context, const std::shared_ptr<TieOptionsTarget>& target)
{
    if (context.profile.epoch != FormatEpoch::CodaBanner && context.profile.epoch != FormatEpoch::UncompressedLegacy) {
        return;
    }

    // Selector 84 replaces the scattered early fields with the direct TieOptions
    // layout.
    if (readGlobalWords(context.index, context.profile, tieScalarSelector).present) {
        return;
    }

    const auto curve = readGlobalWords(context.index, context.profile, legacy_curve::engraverSlurSelector);
    if (curve.present && curve.words.size() >= 4) {
        target->thicknessLeft = -wordAt(curve.words, 1);
        target->thicknessRight = -wordAt(curve.words, 3);
        reportAdjustedTieField(context, "thicknessLeft", wordAt(curve.words, 1), curve.blockOffset, curve.decodedOffset);
        reportAdjustedTieField(context, "thicknessRight", wordAt(curve.words, 3), curve.blockOffset, curve.decodedOffset);
    }

    const auto contour = readGlobalWords(context.index, context.profile, earlyTieContourSelector);
    if (curve.present && curve.words.size() >= 3 && contour.present && contour.words.size() >= 5) {
        const auto baseHeight = wordAt(contour.words, 4);
        const std::array heights{baseHeight + wordAt(curve.words, 0), baseHeight + wordAt(curve.words, 2)};
        for (std::size_t index = 0; index < tieContourCount; ++index) {
            const auto found = target->tieControlStyles.find(static_cast<TieOptionsTarget::ControlStyleType>(index));
            if (found == target->tieControlStyles.end() || !found->second->cp1 || !found->second->cp2) {
                continue;
            }
            const auto divisor = index == 0 ? 2 : 1;
            found->second->cp1->height = heights[0] / divisor;
            found->second->cp2->height = heights[1] / divisor;
            withReporting(context.report, [&](auto&) {
                const auto prefix = "tieControlStyles[" + std::to_string(index) + "].";
                reportAdjustedTieField(context, prefix + "cp1.height", baseHeight, contour.blockOffset, contour.decodedOffset);
                reportAdjustedTieField(context, prefix + "cp2.height", baseHeight, contour.blockOffset, contour.decodedOffset);
            });
        }
    }

    const auto placement = readGlobalWords(context.index, context.profile, legacy_curve::slurThicknessSelector);
    if (!placement.present || placement.words.size() < 6) {
        return;
    }

    const auto recoverHorizontal = [&](const auto& types, std::int16_t value) {
        // A stored zero leaves the era's context-dependent placement default in
        // force.
        if (value == 0) {
            return;
        }
        for (const auto type : types) {
            const auto found = target->tieConnectStyles.find(type);
            if (found == target->tieConnectStyles.end()) {
                continue;
            }
            found->second->offsetX = value;
            withReporting(context.report, [&](auto& reporting) {
                const auto member = "tieConnectStyles[" + std::to_string(static_cast<std::size_t>(type)) + "].offsetX";
                reportRecoveredTieField(reporting, member, value, placement.blockOffset, placement.decodedOffset);
            });
        }
    };
    recoverHorizontal(earlyTieStartConnectionTypes, wordAt(placement.words, 0));
    recoverHorizontal(earlyTieEndConnectionTypes, wordAt(placement.words, 2));

    const auto shortTie = readGlobalWords(context.index, context.profile, earlyShortTieSelector);
    if (shortTie.present && shortTie.words.size() >= 5) {
        const auto basePosition = wordAt(shortTie.words, 4);
        const auto recoverVertical = [&](musx::dom::TieConnectStyleType overType, musx::dom::TieConnectStyleType underType, std::int16_t adjustment) {
            const auto position = static_cast<std::int16_t>(basePosition + adjustment);
            for (const auto& [type, value] : std::array{std::pair{overType, position}, std::pair{underType, static_cast<std::int16_t>(-position)}}) {
                const auto found = target->tieConnectStyles.find(type);
                if (found == target->tieConnectStyles.end()) {
                    continue;
                }
                found->second->offsetY = value;
                withReporting(context.report, [&](auto&) {
                    const auto member = "tieConnectStyles[" + std::to_string(static_cast<std::size_t>(type)) + "].offsetY";
                    reportAdjustedTieField(context, member, position, shortTie.blockOffset, shortTie.decodedOffset);
                });
            }
        };
        recoverVertical(
            musx::dom::TieConnectStyleType::OverStartPosInner, musx::dom::TieConnectStyleType::UnderStartPosInner, wordAt(placement.words, 1));
        recoverVertical(
            musx::dom::TieConnectStyleType::OverEndPosInner, musx::dom::TieConnectStyleType::UnderEndPosInner, wordAt(placement.words, 3));
    }

    const std::array fixedInsets{
        static_cast<std::int16_t>(wordAt(placement.words, 4) - wordAt(placement.words, 0)),
        static_cast<std::int16_t>(wordAt(placement.words, 2) - wordAt(placement.words, 5)),
    };
    for (std::size_t index = 0; index + 1 < tieContourCount; ++index) {
        const auto found = target->tieControlStyles.find(static_cast<TieOptionsTarget::ControlStyleType>(index));
        if (found == target->tieControlStyles.end() || !found->second->cp1 || !found->second->cp2) {
            continue;
        }
        found->second->cp1->insetFixed = fixedInsets[0];
        found->second->cp2->insetFixed = fixedInsets[1];
        withReporting(context.report, [&](auto&) {
            const auto prefix = "tieControlStyles[" + std::to_string(index) + "].";
            reportAdjustedTieField(context, prefix + "cp1.insetFixed", fixedInsets[0], placement.blockOffset, placement.decodedOffset);
            reportAdjustedTieField(context, prefix + "cp2.insetFixed", fixedInsets[1], placement.blockOffset, placement.decodedOffset);
        });
    }
}

void applyUnstoredFrontTieSeparation(const ImportContext& context, const std::shared_ptr<TieOptionsTarget>& target)
{
    constexpr std::size_t frontTieSeparationWord = 15;
    const auto family = readGlobalWords(context.index, context.profile, tieFrontSelector);
    if (family.words.size() > frontTieSeparationWord) {
        return;
    }

    // Layouts without this word use zero separation as fixed behavior.
    target->frontTieSepar = 0;
    reportTieBehaviorField(context, "frontTieSepar", 0);
}

void applyScatteredTieBehavior(const ImportContext& context, const std::shared_ptr<TieOptionsTarget>& target)
{
    if (context.profile.epoch != FormatEpoch::CodaBanner && context.profile.epoch != FormatEpoch::UncompressedLegacy) {
        return;
    }

    // Selector 84 supersedes every fixed behavior in this scattered layout.
    if (readGlobalWords(context.index, context.profile, tieScalarSelector).present) {
        return;
    }

    target->specialPosMode = TieOptionsTarget::SpecialPosMode::None;
    target->sysBreakLeftHAdj = 0;
    reportTieBehaviorField(context, "specialPosMode", 0);
    reportTieBehaviorField(context, "sysBreakLeftHAdj", 0);

    if (context.profile.epoch == FormatEpoch::UncompressedLegacy) {
        target->breakForTimeSigs = false;
        target->breakForKeySigs = false;
        target->useOuterPlacement = false;
        target->secondsPlacement = TieOptionsTarget::SecondsPlacement::None;
        target->chordTieDirType = TieOptionsTarget::ChordTieDirType::OutsideInside;
        target->chordTieDirOpposingSeconds = false;
        target->mixedStemDirection = TieOptionsTarget::MixedStemDirection::OppositeFirst;
        target->beforeAcciSingleNote = false;
        target->useTieEndCtlStyle = true;
        for (const auto member : {"breakForTimeSigs", "breakForKeySigs", "useOuterPlacement", "secondsPlacement", "chordTieDirType",
                 "chordTieDirOpposingSeconds", "beforeAcciSingleNote"}) {
            reportTieBehaviorField(context, member, 0);
        }
        reportTieBehaviorField(context, "mixedStemDirection", 2);
        reportTieBehaviorField(context, "useTieEndCtlStyle", 1);
    }

    if (readGlobalWords(context.index, context.profile, tieContourSelector).present) {
        return;
    }
    constexpr musx::dom::Efix scatteredTieInsetRatio = 512;
    for (std::size_t index = 0; index < tieContourCount; ++index) {
        const auto found = target->tieControlStyles.find(static_cast<TieOptionsTarget::ControlStyleType>(index));
        if (found == target->tieControlStyles.end() || !found->second->cp1 || !found->second->cp2) {
            continue;
        }
        found->second->cp1->insetRatio = scatteredTieInsetRatio;
        found->second->cp2->insetRatio = scatteredTieInsetRatio;
        const bool suppliesSpan = context.profile.epoch == FormatEpoch::UncompressedLegacy && index + 1 < tieContourCount;
        if (suppliesSpan) {
            found->second->span = 48;
        }
        withReporting(context.report, [&](auto&) {
            const auto prefix = "tieControlStyles[" + std::to_string(index) + "].";
            if (suppliesSpan) {
                reportTieBehaviorField(context, prefix + "span", 48);
            }
            reportTieBehaviorField(context, prefix + "cp1.insetRatio", scatteredTieInsetRatio);
            reportTieBehaviorField(context, prefix + "cp2.insetRatio", scatteredTieInsetRatio);
        });
    }
}

void reportRemainingTieFields(const ImportContext& context, const TieOptionsTarget& target)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto instance = reporting.template instanceKey<TieOptionsTarget>();
        for (std::size_t index = 0; index < tieConnectionCount; ++index) {
            const auto found = target.tieConnectStyles.find(static_cast<musx::dom::TieConnectStyleType>(index));
            if (found == target.tieConnectStyles.end()) {
                continue;
            }
            const auto prefix = "tieConnectStyles[" + std::to_string(index) + "].";
            if (!reporting.report().findField(instance, prefix + "offsetX")) {
                reportDefaultTieField(reporting, prefix + "offsetX", found->second->offsetX);
            }
            if (!reporting.report().findField(instance, prefix + "offsetY")) {
                reportDefaultTieField(reporting, prefix + "offsetY", found->second->offsetY);
            }
        }
        for (std::size_t index = 0; index < tieContourCount; ++index) {
            const auto found = target.tieControlStyles.find(static_cast<TieOptionsTarget::ControlStyleType>(index));
            if (found == target.tieControlStyles.end() || !found->second->cp1 || !found->second->cp2) {
                continue;
            }
            const auto prefix = "tieControlStyles[" + std::to_string(index) + "].";
            const auto reportDefault = [&](std::string_view member, std::int64_t value) {
                if (!reporting.report().findField(instance, member)) {
                    reportDefaultTieField(reporting, member, value);
                }
            };
            reportDefault(prefix + "span", found->second->span);
            reportDefault(prefix + "cp1.insetRatio", found->second->cp1->insetRatio);
            reportDefault(prefix + "cp1.height", found->second->cp1->height);
            reportDefault(prefix + "cp1.insetFixed", found->second->cp1->insetFixed);
            reportDefault(prefix + "cp2.insetRatio", found->second->cp2->insetRatio);
            reportDefault(prefix + "cp2.height", found->second->cp2->height);
            reportDefault(prefix + "cp2.insetFixed", found->second->cp2->insetFixed);
        }
    });
}

} // namespace

void importTieOptions(const ImportContext& context)
{
    const auto pooled = context.document->getOptions()->get<TieOptionsTarget>();
    if (!pooled) {
        return;
    }
    const auto target = std::const_pointer_cast<TieOptionsTarget>(pooled);
    captureTieConnections(context, target);
    captureTieContours(context, target);
    captureEarlyTieOptions(context, target);
    applyMappingTables({&fixedTieScalarTable(), &classTieScalarTable()}, context.index, context.profile, context.document, context.report);
    applyUnstoredFrontTieSeparation(context, target);
    applyScatteredTieBehavior(context, target);
    reportRemainingTieFields(context, *target);
}

} // namespace options
} // namespace finale_mus_reader
