// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "import/shared/legacy_curve_selectors.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using SmartShapeTarget = musx::dom::options::SmartShapeOptions;
using legacy_curve::engraverSlurSelector;
using legacy_curve::engraverSlurTag;
using legacy_curve::slurThicknessSelector;
using legacy_curve::slurThicknessTag;

template <typename Enum, std::size_t Size>
consteval bool enumValuesMatchIndices(const std::array<Enum, Size>& values)
{
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (static_cast<std::size_t>(values[index]) != index) {
            return false;
        }
    }
    return true;
}

constexpr std::array smartShapeConnectionIndices{
    SmartShapeTarget::ConnectionIndex::HeadLeftTop,
    SmartShapeTarget::ConnectionIndex::HeadRightTop,
    SmartShapeTarget::ConnectionIndex::HeadRightBottom,
    SmartShapeTarget::ConnectionIndex::HeadLeftBottom,
    SmartShapeTarget::ConnectionIndex::StemLeftTop,
    SmartShapeTarget::ConnectionIndex::StemRightTop,
    SmartShapeTarget::ConnectionIndex::StemRightBottom,
    SmartShapeTarget::ConnectionIndex::StemLeftBottom,
    SmartShapeTarget::ConnectionIndex::NoteLeftTop,
    SmartShapeTarget::ConnectionIndex::NoteRightTop,
    SmartShapeTarget::ConnectionIndex::NoteRightBottom,
    SmartShapeTarget::ConnectionIndex::NoteLeftBottom,
    SmartShapeTarget::ConnectionIndex::NoteLeftCenter,
    SmartShapeTarget::ConnectionIndex::NoteRightCenter,
};
constexpr std::array smartShapeSlurConnectionTypes{
    SmartShapeTarget::SlurConnectStyleType::OverNoteStart,
    SmartShapeTarget::SlurConnectStyleType::OverNoteEnd,
    SmartShapeTarget::SlurConnectStyleType::OverStemStart,
    SmartShapeTarget::SlurConnectStyleType::OverStemEnd,
    SmartShapeTarget::SlurConnectStyleType::UnderNoteStart,
    SmartShapeTarget::SlurConnectStyleType::UnderNoteEnd,
    SmartShapeTarget::SlurConnectStyleType::UnderStemStart,
    SmartShapeTarget::SlurConnectStyleType::UnderStemEnd,
    SmartShapeTarget::SlurConnectStyleType::OverMixStemStart,
    SmartShapeTarget::SlurConnectStyleType::OverMixStemEnd,
    SmartShapeTarget::SlurConnectStyleType::OverStemGrace,
    SmartShapeTarget::SlurConnectStyleType::OverStemPrincipal,
    SmartShapeTarget::SlurConnectStyleType::UnderStemGrace,
    SmartShapeTarget::SlurConnectStyleType::UnderStemPrincipal,
    SmartShapeTarget::SlurConnectStyleType::UnderNoteGrace,
    SmartShapeTarget::SlurConnectStyleType::UnderStemNotePrincipal,
    SmartShapeTarget::SlurConnectStyleType::OverNoteGrace,
    SmartShapeTarget::SlurConnectStyleType::OverStemNotePrincipal,
    SmartShapeTarget::SlurConnectStyleType::OverBeamStart,
    SmartShapeTarget::SlurConnectStyleType::OverBeamEnd,
    SmartShapeTarget::SlurConnectStyleType::UnderBeamStart,
    SmartShapeTarget::SlurConnectStyleType::UnderBeamEnd,
    SmartShapeTarget::SlurConnectStyleType::OverMixFlagStart,
    SmartShapeTarget::SlurConnectStyleType::OverFlagStart,
    SmartShapeTarget::SlurConnectStyleType::UnderFlagStart,
    SmartShapeTarget::SlurConnectStyleType::OverTabNumStart,
    SmartShapeTarget::SlurConnectStyleType::OverTabNumEnd,
    SmartShapeTarget::SlurConnectStyleType::UnderTabNumStart,
    SmartShapeTarget::SlurConnectStyleType::UnderTabNumEnd,
};
constexpr std::array smartShapeTabSlideConnectionTypes{
    SmartShapeTarget::TabSlideConnectStyleType::DiffLevelPitchUpLineStart,
    SmartShapeTarget::TabSlideConnectStyleType::DiffLevelPitchUpLineEnd,
    SmartShapeTarget::TabSlideConnectStyleType::DiffLevelPitchUpSpaceStart,
    SmartShapeTarget::TabSlideConnectStyleType::DiffLevelPitchUpSpaceEnd,
    SmartShapeTarget::TabSlideConnectStyleType::DiffLevelPitchDownLineStart,
    SmartShapeTarget::TabSlideConnectStyleType::DiffLevelPitchDownLineEnd,
    SmartShapeTarget::TabSlideConnectStyleType::DiffLevelPitchDownSpaceStart,
    SmartShapeTarget::TabSlideConnectStyleType::DiffLevelPitchDownSpaceEnd,
    SmartShapeTarget::TabSlideConnectStyleType::SameLevelPitchUpLineStart,
    SmartShapeTarget::TabSlideConnectStyleType::SameLevelPitchUpLineEnd,
    SmartShapeTarget::TabSlideConnectStyleType::SameLevelPitchUpSpaceStart,
    SmartShapeTarget::TabSlideConnectStyleType::SameLevelPitchUpSpaceEnd,
    SmartShapeTarget::TabSlideConnectStyleType::SameLevelPitchDownLineStart,
    SmartShapeTarget::TabSlideConnectStyleType::SameLevelPitchDownLineEnd,
    SmartShapeTarget::TabSlideConnectStyleType::SameLevelPitchDownSpaceStart,
    SmartShapeTarget::TabSlideConnectStyleType::SameLevelPitchDownSpaceEnd,
    SmartShapeTarget::TabSlideConnectStyleType::SameLevelPitchSameStart,
    SmartShapeTarget::TabSlideConnectStyleType::SameLevelPitchSameEnd,
};
constexpr std::array smartShapeGlissandoConnectionTypes{
    SmartShapeTarget::GlissandoConnectStyleType::DefaultStart,
    SmartShapeTarget::GlissandoConnectStyleType::DefaultEnd,
};
constexpr std::array smartShapeBendCurveConnectionTypes{
    SmartShapeTarget::BendCurveConnectStyleType::NoteStart,
    SmartShapeTarget::BendCurveConnectStyleType::StaffEnd,
    SmartShapeTarget::BendCurveConnectStyleType::StaffStart,
    SmartShapeTarget::BendCurveConnectStyleType::NoteEnd,
    SmartShapeTarget::BendCurveConnectStyleType::StaffToTopLineStart,
    SmartShapeTarget::BendCurveConnectStyleType::StaffFromTopLineEnd,
    SmartShapeTarget::BendCurveConnectStyleType::StaffEndOffset,
    SmartShapeTarget::BendCurveConnectStyleType::StaffFromTopEndOffset,
};

static_assert(enumValuesMatchIndices(smartShapeConnectionIndices), "musxdom ConnectionIndex order must match the legacy stored indices");
static_assert(enumValuesMatchIndices(smartShapeSlurConnectionTypes), "musxdom slur connection-type order must match the legacy collection");
static_assert(enumValuesMatchIndices(smartShapeTabSlideConnectionTypes), "musxdom tab-slide connection-type order must match the legacy collection");
static_assert(enumValuesMatchIndices(smartShapeGlissandoConnectionTypes), "musxdom glissando connection-type order must match the legacy collection");
static_assert(
    enumValuesMatchIndices(smartShapeBendCurveConnectionTypes), "musxdom bend-curve connection-type order must match the legacy collection");

constexpr std::uint16_t smartShapeSelector(std::string_view tag)
{
    return static_cast<std::uint16_t>((tag[0] - '0') * 10 + tag[1] - '0');
}

constexpr std::string_view directionTag = "10";
constexpr std::string_view slurContourTag = "52";
constexpr std::string_view slurAdjustmentTag = "53";
constexpr std::string_view legacySlurThicknessTag = "59";
constexpr std::string_view lineStyleTag = "92";
constexpr std::string_view slurTipTag = "93";
constexpr std::string_view guitarBendTag = "97";
constexpr std::uint16_t smartShapeSlurConnectionSelector = 26;
constexpr std::uint16_t smartShapeTabSlideConnectionSelector = 90;
constexpr std::uint16_t smartShapeGlissandoConnectionSelector = 91;
constexpr std::uint16_t smartShapeBendCurveConnectionSelector = 98;
constexpr std::uint16_t directionSelector = smartShapeSelector(directionTag);
constexpr std::uint16_t slurContourSelector = smartShapeSelector(slurContourTag);
constexpr std::uint16_t slurAdjustmentSelector = smartShapeSelector(slurAdjustmentTag);
constexpr std::uint16_t lineStyleSelector = smartShapeSelector(lineStyleTag);
constexpr std::uint16_t slurTipSelector = smartShapeSelector(slurTipTag);
constexpr std::uint16_t guitarBendSelector = smartShapeSelector(guitarBendTag);

constexpr std::size_t controlStyleCount = 4;
constexpr std::size_t controlStyleWords = 3;
constexpr std::size_t controlStylePayloadWords = controlStyleCount * controlStyleWords;
constexpr std::size_t smartShapeConnectionStyleWords = 3;
constexpr std::size_t smartShapeConnectionIndexCount = smartShapeConnectionIndices.size();
constexpr musx::dom::Evpu finale26HookLength = 8;
// The editable default direction begins in Finale 2002 within the DCL epoch.
// Earlier epochs retain the seeded Automatic direction; every zlib file is later.

bool sourceStoresSmartShapeDirection(const SourceProfile& profile)
{
    if (sourceMatches(profile, EpochMask::Zlib)) {
        return true;
    }
    return sourceMatches(profile, EpochMask::Dcl) && sourceAtOrAfter(profile, FormatEpoch::DclLegacy, versions::finale2002)
           && !sourceAtOrAfter(profile, FormatEpoch::DclLegacy, versions::finale2007);
}

bool sourceHasPreFinale37FigureBehavior(const SourceProfile& profile)
{
    return sourceAtOrAfter(profile, FormatEpoch::UncompressedLegacy, versions::finale3_0)
           && !sourceAtOrAfter(profile, FormatEpoch::UncompressedLegacy, versions::finale3_7);
}

bool sourceIsFinale26(const SourceProfile& profile)
{
    return sourceMatches(profile, EpochMask::CodaBanner) && profile.version
           && VersionBound{profile.version->major, profile.version->minor} == versions::finale2_6;
}

bool predatesCustomLineCapability(const SourceProfile& profile)
{
    // Coda-banner files predate custom lines. Within the uncompressed epoch, internal
    // major version 5 is the capability boundary. DCL and zlib are wholly later.
    return !sourceAtOrAfter(profile, FormatEpoch::UncompressedLegacy, versions::finale2000);
}

bool predatesTabBendCurveCapability(const SourceProfile& profile)
{
    // Coda-banner and uncompressed files predate the bend-curve tool. Within the DCL
    // epoch, internal major version 8 is the capability boundary. Zlib is wholly later.
    return !sourceAtOrAfter(profile, FormatEpoch::DclLegacy, versions::finale2003);
}

bool hasModernSlurScalars(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    // Selector 97 belongs to the later scalar family. Earlier files reuse selectors
    // 50, 51, and 53 for a different slur layout, so those spellings alone are ambiguous.
    return readGlobalWords(index, profile, guitarBendSelector).present;
}

bool hasSlurContours(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    const auto family = readGlobalWords(index, profile, slurContourSelector);
    return family.present && family.words.size() == controlStylePayloadWords;
}

bool hasEarlySlurFamily(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return hasSlurContours(index, profile) && !hasModernSlurScalars(index, profile);
}

bool hasLineStyleSelectors(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return readGlobalWords(index, profile, lineStyleSelector).present;
}

bool hasFigureSettings(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    if (profile.epoch == FormatEpoch::ZlibLegacy) {
        return index.getClassOthers().get(zlibFigureClass, 11, 0, 0) != nullptr;
    }
    return index.getOthers().get(records::packTag(figureTag), 11, 0, 0) != nullptr;
}

bool avoidAccidentals(std::int64_t value)
{
    return value == 2;
}

double slurTipWidth(std::int64_t value)
{
    return static_cast<double>(value) / 10000.0;
}

const FieldMapping fixedSlurFields[] = {
    MUS_WORD(SmartShapeTarget, slurThicknessTag, GLOBALS_CMPER, 0, 0, slurThicknessCp1X),
    MUS_WORD(SmartShapeTarget, slurThicknessTag, GLOBALS_CMPER, 0, 1, slurThicknessCp1Y),
    MUS_WORD(SmartShapeTarget, slurThicknessTag, GLOBALS_CMPER, 0, 2, slurThicknessCp2X),
    MUS_WORD(SmartShapeTarget, slurThicknessTag, GLOBALS_CMPER, 0, 3, slurThicknessCp2Y),
    MUS_WORD_AS_IF(SmartShapeTarget, slurThicknessTag, GLOBALS_CMPER, 0, 4, nullptr, slurAvoidAccidentals, avoidAccidentals(value)),
    MUS_LONG(SmartShapeTarget, engraverSlurTag, GLOBALS_CMPER, 0, 0, LongWordOrder::HighFirst, maxSlurStretch),
    MUS_LONG(SmartShapeTarget, engraverSlurTag, GLOBALS_CMPER, 0, 2, LongWordOrder::HighFirst, maxSlurLift),
    MUS_WORD(SmartShapeTarget, engraverSlurTag, GLOBALS_CMPER, 0, 4, slurSymmetry),
    MUS_WORD(SmartShapeTarget, engraverSlurTag, GLOBALS_CMPER, 0, 5, useEngraverSlurs),
    MUS_WORD(SmartShapeTarget, slurAdjustmentTag, GLOBALS_CMPER, 0, 0, slurLeftBreakHorzAdj),
    MUS_WORD(SmartShapeTarget, slurAdjustmentTag, GLOBALS_CMPER, 0, 1, slurRightBreakHorzAdj),
    MUS_WORD(SmartShapeTarget, slurAdjustmentTag, GLOBALS_CMPER, 0, 2, slurBreakVertAdj),
    MUS_WORD(SmartShapeTarget, slurAdjustmentTag, GLOBALS_CMPER, 0, 3, slurAvoidStaffLines),
    MUS_WORD(SmartShapeTarget, slurAdjustmentTag, GLOBALS_CMPER, 0, 4, slurPadding),
    MUS_WORD(SmartShapeTarget, slurAdjustmentTag, GLOBALS_CMPER, 0, 5, maxSlurAngle),
    MUS_WORD(SmartShapeTarget, slurAdjustmentTag, GLOBALS_CMPER, 1, 0, slurAcciPadding),
    MUS_WORD(SmartShapeTarget, slurAdjustmentTag, GLOBALS_CMPER, 1, 1, slurDoStretchFirst),
    MUS_WORD(SmartShapeTarget, slurAdjustmentTag, GLOBALS_CMPER, 1, 2, slurStretchByPercent),
    MUS_WORD(SmartShapeTarget, slurAdjustmentTag, GLOBALS_CMPER, 1, 3, maxSlurStretchPercent),
    MUS_WORD(SmartShapeTarget, guitarBendTag, GLOBALS_CMPER, 1, 0, guitarBendUseParens),
    MUS_WORD(SmartShapeTarget, guitarBendTag, GLOBALS_CMPER, 1, 1, guitarBendHideBendTo),
    MUS_WORD(SmartShapeTarget, guitarBendTag, GLOBALS_CMPER, 1, 2, guitarBendGenText),
    MUS_WORD(SmartShapeTarget, guitarBendTag, GLOBALS_CMPER, 1, 3, guitarBendUseFull),
};

const FieldMapping codaSlurThicknessFields[] = {
    // Each control point is stored as a horizontal/vertical pair. Horizontal values
    // keep their sign; vertical values use the opposite sign from SmartShapeOptions.
    MUS_WORD(SmartShapeTarget, engraverSlurTag, GLOBALS_CMPER, 0, 0, slurThicknessCp1X),
    MUS_WORD_AS_IF(SmartShapeTarget, engraverSlurTag, GLOBALS_CMPER, 0, 1, nullptr, slurThicknessCp1Y, -value),
    MUS_WORD(SmartShapeTarget, engraverSlurTag, GLOBALS_CMPER, 0, 2, slurThicknessCp2X),
    MUS_WORD_AS_IF(SmartShapeTarget, engraverSlurTag, GLOBALS_CMPER, 0, 3, nullptr, slurThicknessCp2Y, -value),
};

const FieldMapping classSlurFields[] = {
    MUS_CLASS_WORD(SmartShapeTarget, numericGlobalClass(slurThicknessSelector), GLOBALS_CMPER, classWordOffset(0), slurThicknessCp1X),
    MUS_CLASS_WORD(SmartShapeTarget, numericGlobalClass(slurThicknessSelector), GLOBALS_CMPER, classWordOffset(1), slurThicknessCp1Y),
    MUS_CLASS_WORD(SmartShapeTarget, numericGlobalClass(slurThicknessSelector), GLOBALS_CMPER, classWordOffset(2), slurThicknessCp2X),
    MUS_CLASS_WORD(SmartShapeTarget, numericGlobalClass(slurThicknessSelector), GLOBALS_CMPER, classWordOffset(3), slurThicknessCp2Y),
    MUS_CLASS_WORD_AS_IF(SmartShapeTarget, numericGlobalClass(slurThicknessSelector), GLOBALS_CMPER, classWordOffset(4), nullptr,
        slurAvoidAccidentals, avoidAccidentals(value)),
    MUS_CLASS_LONG(
        SmartShapeTarget, numericGlobalClass(engraverSlurSelector), GLOBALS_CMPER, classWordOffset(0), LongWordOrder::HighFirst, maxSlurStretch),
    MUS_CLASS_LONG(
        SmartShapeTarget, numericGlobalClass(engraverSlurSelector), GLOBALS_CMPER, classWordOffset(2), LongWordOrder::HighFirst, maxSlurLift),
    MUS_CLASS_WORD(SmartShapeTarget, numericGlobalClass(engraverSlurSelector), GLOBALS_CMPER, classWordOffset(4), slurSymmetry),
    MUS_CLASS_WORD(SmartShapeTarget, numericGlobalClass(engraverSlurSelector), GLOBALS_CMPER, classWordOffset(5), useEngraverSlurs),
    MUS_CLASS_WORD(SmartShapeTarget, numericGlobalClass(slurAdjustmentSelector), GLOBALS_CMPER, classWordOffset(0), slurLeftBreakHorzAdj),
    MUS_CLASS_WORD(SmartShapeTarget, numericGlobalClass(slurAdjustmentSelector), GLOBALS_CMPER, classWordOffset(1), slurRightBreakHorzAdj),
    MUS_CLASS_WORD(SmartShapeTarget, numericGlobalClass(slurAdjustmentSelector), GLOBALS_CMPER, classWordOffset(2), slurBreakVertAdj),
    MUS_CLASS_WORD(SmartShapeTarget, numericGlobalClass(slurAdjustmentSelector), GLOBALS_CMPER, classWordOffset(3), slurAvoidStaffLines),
    MUS_CLASS_WORD(SmartShapeTarget, numericGlobalClass(slurAdjustmentSelector), GLOBALS_CMPER, classWordOffset(4), slurPadding),
    MUS_CLASS_WORD(SmartShapeTarget, numericGlobalClass(slurAdjustmentSelector), GLOBALS_CMPER, classWordOffset(5), maxSlurAngle),
    MUS_CLASS_WORD(SmartShapeTarget, numericGlobalClass(slurAdjustmentSelector), GLOBALS_CMPER, classWordOffset(6), slurAcciPadding),
    MUS_CLASS_WORD(SmartShapeTarget, numericGlobalClass(slurAdjustmentSelector), GLOBALS_CMPER, classWordOffset(7), slurDoStretchFirst),
    MUS_CLASS_WORD(SmartShapeTarget, numericGlobalClass(slurAdjustmentSelector), GLOBALS_CMPER, classWordOffset(8), slurStretchByPercent),
    MUS_CLASS_WORD(SmartShapeTarget, numericGlobalClass(slurAdjustmentSelector), GLOBALS_CMPER, classWordOffset(9), maxSlurStretchPercent),
    MUS_CLASS_WORD(SmartShapeTarget, numericGlobalClass(guitarBendSelector), GLOBALS_CMPER, classWordOffset(6), guitarBendUseParens),
    MUS_CLASS_WORD(SmartShapeTarget, numericGlobalClass(guitarBendSelector), GLOBALS_CMPER, classWordOffset(7), guitarBendHideBendTo),
    MUS_CLASS_WORD(SmartShapeTarget, numericGlobalClass(guitarBendSelector), GLOBALS_CMPER, classWordOffset(8), guitarBendGenText),
    MUS_CLASS_WORD(SmartShapeTarget, numericGlobalClass(guitarBendSelector), GLOBALS_CMPER, classWordOffset(9), guitarBendUseFull),
};

const FieldMapping fixedEarlySlurFields[] = {
    // Before the modern slur scalar family, one stored thickness supplies both
    // vertical thickness controls. Their horizontal controls have no located source.
    MUS_WORD(SmartShapeTarget, legacySlurThicknessTag, GLOBALS_CMPER, 0, 5, slurThicknessCp1Y),
    MUS_WORD(SmartShapeTarget, legacySlurThicknessTag, GLOBALS_CMPER, 0, 5, slurThicknessCp2Y),
    MUS_WORD(SmartShapeTarget, slurAdjustmentTag, GLOBALS_CMPER, 0, 0, slurLeftBreakHorzAdj),
    MUS_WORD(SmartShapeTarget, slurAdjustmentTag, GLOBALS_CMPER, 0, 1, slurRightBreakHorzAdj),
    MUS_WORD(SmartShapeTarget, slurAdjustmentTag, GLOBALS_CMPER, 0, 2, slurBreakVertAdj),
};

const FieldMapping fixedLineFields[] = {
    MUS_WORD(SmartShapeTarget, lineStyleTag, GLOBALS_CMPER, 0, 0, ssLineStyleCmpCustom),
    MUS_WORD(SmartShapeTarget, lineStyleTag, GLOBALS_CMPER, 0, 1, ssLineStyleCmpGlissando),
    MUS_WORD(SmartShapeTarget, lineStyleTag, GLOBALS_CMPER, 0, 2, ssLineStyleCmpTabSlide),
    MUS_WORD(SmartShapeTarget, lineStyleTag, GLOBALS_CMPER, 0, 3, ssLineStyleCmpTabBendCurve),
    MUS_FIELD_AS_IF(SmartShapeTarget, slurTipTag, GLOBALS_CMPER, 0, 0, ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, nullptr,
        smartSlurTipWidth, slurTipWidth(value)),
};

const FieldMapping classLineFields[] = {
    MUS_CLASS_WORD(SmartShapeTarget, numericGlobalClass(lineStyleSelector), GLOBALS_CMPER, classWordOffset(0), ssLineStyleCmpCustom),
    MUS_CLASS_WORD(SmartShapeTarget, numericGlobalClass(lineStyleSelector), GLOBALS_CMPER, classWordOffset(1), ssLineStyleCmpGlissando),
    MUS_CLASS_WORD(SmartShapeTarget, numericGlobalClass(lineStyleSelector), GLOBALS_CMPER, classWordOffset(2), ssLineStyleCmpTabSlide),
    MUS_CLASS_WORD(SmartShapeTarget, numericGlobalClass(lineStyleSelector), GLOBALS_CMPER, classWordOffset(3), ssLineStyleCmpTabBendCurve),
    MUS_CLASS_FIELD_AS_IF(SmartShapeTarget, numericGlobalClass(slurTipSelector), GLOBALS_CMPER, classWordOffset(0), ValueWidth::Long,
        LongWordOrder::HighFirst, BitRange{}, nullptr, smartSlurTipWidth, slurTipWidth(value)),
};

const FieldMapping fixedFigureFields[] = {
    MUS_WORD(SmartShapeTarget, figureTag, 11, 0, 0, crescHeight),
    MUS_WORD(SmartShapeTarget, figureTag, 11, 0, 1, crescLineWidth),
    MUS_WORD(SmartShapeTarget, figureTag, 11, 0, 2, hookLength),
    MUS_WORD(SmartShapeTarget, figureTag, 11, 0, 4, smartLineWidth),
    MUS_WORD(SmartShapeTarget, figureTag, 11, 0, 5, showOctavaAsText),
    MUS_LONG(SmartShapeTarget, figureTag, 12, 0, 0, LongWordOrder::HighFirst, smartDashOn),
    MUS_LONG(SmartShapeTarget, figureTag, 12, 0, 2, LongWordOrder::HighFirst, smartDashOff),
    MUS_WORD(SmartShapeTarget, figureTag, 12, 0, 4, crescHorizontal),
};

const FieldMapping classFigureFields[] = {
    MUS_CLASS_WORD(SmartShapeTarget, zlibFigureClass, 11, classWordOffset(0), crescHeight),
    MUS_CLASS_WORD(SmartShapeTarget, zlibFigureClass, 11, classWordOffset(1), crescLineWidth),
    MUS_CLASS_WORD(SmartShapeTarget, zlibFigureClass, 11, classWordOffset(2), hookLength),
    MUS_CLASS_WORD(SmartShapeTarget, zlibFigureClass, 11, classWordOffset(4), smartLineWidth),
    MUS_CLASS_WORD(SmartShapeTarget, zlibFigureClass, 11, classWordOffset(5), showOctavaAsText),
    MUS_CLASS_LONG(SmartShapeTarget, zlibFigureClass, 12, classWordOffset(0), LongWordOrder::HighFirst, smartDashOn),
    MUS_CLASS_LONG(SmartShapeTarget, zlibFigureClass, 12, classWordOffset(2), LongWordOrder::HighFirst, smartDashOff),
    MUS_CLASS_WORD(SmartShapeTarget, zlibFigureClass, 12, classWordOffset(4), crescHorizontal),
};

constexpr const char* smartShapeReportPrefix = "options.smartShapeOptions";

const MappingTable& codaSlurThicknessTable()
{
    static const MappingTable table{.reportPrefix = smartShapeReportPrefix,
        .epochs = EpochMask::CodaBanner,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<SmartShapeTarget>,
        .fields = codaSlurThicknessFields,
        .fieldCount = std::size(codaSlurThicknessFields)};
    return table;
}

const MappingTable& fixedSlurTable()
{
    static const MappingTable table{.reportPrefix = smartShapeReportPrefix,
        .epochs = EpochMask::FixedRow,
        .applies = &hasModernSlurScalars,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<SmartShapeTarget>,
        .fields = fixedSlurFields,
        .fieldCount = std::size(fixedSlurFields)};
    return table;
}

const MappingTable& classSlurTable()
{
    static const MappingTable table{.reportPrefix = smartShapeReportPrefix,
        .epochs = EpochMask::Zlib,
        .applies = &hasModernSlurScalars,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<SmartShapeTarget>,
        .fields = classSlurFields,
        .fieldCount = std::size(classSlurFields)};
    return table;
}

const MappingTable& fixedEarlySlurTable()
{
    static const MappingTable table{.reportPrefix = smartShapeReportPrefix,
        .epochs = EpochMask::FixedRow,
        .applies = &hasEarlySlurFamily,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<SmartShapeTarget>,
        .fields = fixedEarlySlurFields,
        .fieldCount = std::size(fixedEarlySlurFields)};
    return table;
}

const MappingTable& fixedLineTable()
{
    static const MappingTable table{.reportPrefix = smartShapeReportPrefix,
        .epochs = EpochMask::FixedRow,
        .applies = &hasLineStyleSelectors,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<SmartShapeTarget>,
        .fields = fixedLineFields,
        .fieldCount = std::size(fixedLineFields)};
    return table;
}

const MappingTable& classLineTable()
{
    static const MappingTable table{.reportPrefix = smartShapeReportPrefix,
        .epochs = EpochMask::Zlib,
        .applies = &hasLineStyleSelectors,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<SmartShapeTarget>,
        .fields = classLineFields,
        .fieldCount = std::size(classLineFields)};
    return table;
}

const MappingTable& fixedFigureTable()
{
    static const MappingTable table{.reportPrefix = smartShapeReportPrefix,
        .epochs = EpochMask::FixedRow,
        .applies = &hasFigureSettings,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<SmartShapeTarget>,
        .fields = fixedFigureFields,
        .fieldCount = std::size(fixedFigureFields)};
    return table;
}

const MappingTable& classFigureTable()
{
    static const MappingTable table{.reportPrefix = smartShapeReportPrefix,
        .epochs = EpochMask::Zlib,
        .applies = &hasFigureSettings,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<SmartShapeTarget>,
        .fields = classFigureFields,
        .fieldCount = std::size(classFigureFields)};
    return table;
}

void reportControlStyle(
    ImportReport& report, std::size_t index, const SmartShapeTarget::ControlStyle& style, std::size_t blockOffset, std::size_t decodedOffset)
{
    withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
        const auto prefix = "slurControlStyles[" + std::to_string(index) + "].";
        reporting.report().setField(reporting.template instanceKey<SmartShapeTarget>(), prefix + "span",
            {Reporting::Origin::LegacyMus, blockOffset, decodedOffset, style.span});
        reporting.report().setField(reporting.template instanceKey<SmartShapeTarget>(), prefix + "inset",
            {Reporting::Origin::LegacyMus, blockOffset, decodedOffset, style.inset});
        reporting.report().setField(reporting.template instanceKey<SmartShapeTarget>(), prefix + "height",
            {Reporting::Origin::LegacyMus, blockOffset, decodedOffset, style.height});
    });
}

void captureDirection(const ImportContext& context, const std::shared_ptr<SmartShapeTarget>& target)
{
    if (!sourceStoresSmartShapeDirection(context.profile)) {
        return;
    }

    const auto family = readGlobalWords(context.index, context.profile, directionSelector);
    if (!family.present || family.words.empty()) {
        return;
    }

    const auto stored = family.words.front();
    switch (stored) {
    case -1: target->direction = musx::dom::ShapeDirection::Under; break;
    case 0: target->direction = musx::dom::ShapeDirection::Automatic; break;
    case 1: target->direction = musx::dom::ShapeDirection::Over; break;
    default:
        // Values outside the stored enum do not designate a direction. Retain the
        // seeded default instead of coercing an unrelated positive or negative word.
        return;
    }

    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        reporting.report().setField(reporting.template instanceKey<SmartShapeTarget>(), "direction",
            {Reporting::Origin::LegacyMus, family.blockOffset, family.decodedOffset, stored});
    });
}

void captureControlStyles(const ImportContext& context, const std::shared_ptr<SmartShapeTarget>& target)
{
    const auto family = readGlobalWords(context.index, context.profile, slurContourSelector);
    // This family has twelve words after the Coda six-word floating-point layout.
    // Before the modern slur family appears, its last tuple is a zero placeholder.
    if (!family.present || family.words.size() != controlStylePayloadWords) {
        return;
    }

    constexpr std::array types{
        SmartShapeTarget::SlurControlStyleType::ShortSpan,
        SmartShapeTarget::SlurControlStyleType::MediumSpan,
        SmartShapeTarget::SlurControlStyleType::LongSpan,
        SmartShapeTarget::SlurControlStyleType::ExtraLongSpan,
    };
    const bool hasExtraLongStyle = hasModernSlurScalars(context.index, context.profile);
    const std::size_t storedStyleCount = hasExtraLongStyle ? types.size() : types.size() - 1;
    if (hasExtraLongStyle) {
        target->slurControlStyles.clear();
    }
    for (std::size_t index = 0; index < storedStyleCount; ++index) {
        const auto first = index * controlStyleWords;
        auto style = std::make_shared<SmartShapeTarget::ControlStyle>();
        style->span = wordAt(family.words, first);
        style->inset = wordAt(family.words, first + 1);
        style->height = wordAt(family.words, first + 2);
        reportControlStyle(context.report, index, *style, family.blockOffset, family.decodedOffset);
        target->slurControlStyles.insert_or_assign(types[index], std::move(style));
    }

    if (!hasExtraLongStyle) {
        const auto longStyle = target->slurControlStyles.at(SmartShapeTarget::SlurControlStyleType::LongSpan);
        const auto extraLongStyle = target->slurControlStyles.at(SmartShapeTarget::SlurControlStyleType::ExtraLongSpan);
        extraLongStyle->inset = longStyle->inset;
        extraLongStyle->height = longStyle->height;
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            const auto sourceFirst = (storedStyleCount - 1) * controlStyleWords;
            reporting.template defaultField<SmartShapeTarget>("slurControlStyles[3].span", extraLongStyle->span);
            reporting.report().setField(reporting.template instanceKey<SmartShapeTarget>(), "slurControlStyles[3].inset",
                {Reporting::Origin::LegacyBehavior, family.blockOffset, family.decodedOffset, wordAt(family.words, sourceFirst + 1)});
            reporting.report().setField(reporting.template instanceKey<SmartShapeTarget>(), "slurControlStyles[3].height",
                {Reporting::Origin::LegacyBehavior, family.blockOffset, family.decodedOffset, wordAt(family.words, sourceFirst + 2)});
        });
    }
}

void captureSlurAvoidStaffLinesAmount(const ImportContext& context, const std::shared_ptr<SmartShapeTarget>& target)
{
    const auto family = readGlobalWords(context.index, context.profile, slurThicknessSelector);
    constexpr std::size_t amountWord = 5;
    if (hasModernSlurScalars(context.index, context.profile) && family.present && family.words.size() > amountWord) {
        const auto stored = wordAt(family.words, amountWord);
        // Nonzero amounts are one-based. Believed: a zero word carries no usable
        // amount, so it does not override the seeded default.
        if (stored != 0) {
            target->slurAvoidStaffLinesAmt = stored - 1;
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                reporting.report().setField(reporting.template instanceKey<SmartShapeTarget>(), "slurAvoidStaffLinesAmt",
                    {Reporting::Origin::LegacyMus, family.blockOffset, family.decodedOffset, stored});
            });
            return;
        }
    }
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        reporting.template defaultField<SmartShapeTarget>("slurAvoidStaffLinesAmt", target->slurAvoidStaffLinesAmt);
    });
}

void applyPreFinale37FigureBehavior(const ImportContext& context, const std::shared_ptr<SmartShapeTarget>& target)
{
    // Finale 3.0 through 3.6 predates the hook-length setting. Believed: it also has no
    // independent crescendo line width.
    if (!sourceHasPreFinale37FigureBehavior(context.profile) || !hasFigureSettings(context.index, context.profile)) {
        return;
    }
    target->crescLineWidth = target->smartLineWidth;
    const auto reference = context.referenceDocument->getOptions()->get<SmartShapeTarget>();
    if (!reference) {
        throw std::logic_error("SmartShapeOptions reference document is incomplete");
    }
    target->hookLength = reference->hookLength;
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto* source = reporting.report().template findField<SmartShapeTarget>("smartLineWidth");
        const auto blockOffset = source ? source->blockOffset : 0;
        const auto decodedOffset = source ? source->decodedOffset : 0;
        const auto rawValue = source ? source->rawValue : target->smartLineWidth;
        reporting.report().setField(reporting.template instanceKey<SmartShapeTarget>(), "crescLineWidth",
            {Reporting::Origin::LegacyBehavior, blockOffset, decodedOffset, rawValue});
        reporting.template defaultField<SmartShapeTarget>("hookLength", target->hookLength);
    });
}

void applyLegacyHairpinOpeningBehavior(const ImportContext& context, const std::shared_ptr<SmartShapeTarget>& target)
{
    // Legacy formats have one hairpin opening. The separate short opening postdates MUS,
    // so both modern fields receive that one behavior even where its source remains unlocated.
    target->shortHairpinOpeningWidth = target->crescHeight;
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto* source = reporting.report().template findField<SmartShapeTarget>("crescHeight");
        const auto blockOffset = source ? source->blockOffset : 0;
        const auto decodedOffset = source ? source->decodedOffset : 0;
        const auto rawValue = source ? source->rawValue : target->crescHeight;
        reporting.report().setField(reporting.template instanceKey<SmartShapeTarget>(), "shortHairpinOpeningWidth",
            {Reporting::Origin::LegacyBehavior, blockOffset, decodedOffset, rawValue});
    });
}

void applySingleIncidenceSlurAdjustmentBehavior(const ImportContext& context, const std::shared_ptr<SmartShapeTarget>& target)
{
    const auto family = readGlobalWords(context.index, context.profile, slurAdjustmentSelector);
    // The enhanced-slur layout initially has no independent accidental-padding or
    // stretch-first fields. Its general padding applies to accidental avoidance.
    if (!hasModernSlurScalars(context.index, context.profile) || !family.present || family.words.size() != records::otherWordCount) {
        return;
    }
    target->slurAcciPadding = target->slurPadding;
    target->slurDoStretchFirst = false;
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto* paddingSource = reporting.report().template findField<SmartShapeTarget>("slurPadding");
        const auto blockOffset = paddingSource ? paddingSource->blockOffset : family.blockOffset;
        const auto decodedOffset = paddingSource ? paddingSource->decodedOffset : family.decodedOffset;
        const auto rawValue = paddingSource ? paddingSource->rawValue : target->slurPadding;
        reporting.report().setField(reporting.template instanceKey<SmartShapeTarget>(), "slurAcciPadding",
            {Reporting::Origin::LegacyBehavior, blockOffset, decodedOffset, rawValue});
        reporting.template behaviorField<SmartShapeTarget>("slurDoStretchFirst", 0);
    });
}

void applyFinale26HookBehavior(const ImportContext& context, const std::shared_ptr<SmartShapeTarget>& target)
{
    // Believed: Finale 2.6 has no stored hook-length preference; apply its fixed
    // behavior only inside the Coda epoch.
    if (!sourceIsFinale26(context.profile)) {
        return;
    }
    target->hookLength = finale26HookLength;
    withReporting(context.report,
        [&]<typename Reporting>(Reporting& reporting) { reporting.template behaviorField<SmartShapeTarget>("hookLength", finale26HookLength); });
}

template <typename Map>
void captureSmartShapeConnectionStyles(const ImportContext& context, const std::shared_ptr<SmartShapeTarget>& target, std::uint16_t selector,
    std::string_view collection, Map SmartShapeTarget::* member, std::size_t semanticStyleCount, bool trailingTupleAfterInitialLayout = false)
{
    const auto family = readGlobalWords(context.index, context.profile, selector);
    if (!family.present) {
        return;
    }

    auto& styles = target.get()->*member;
    auto tupleCount = family.words.size() / smartShapeConnectionStyleWords;
    // The slur family starts as two ordinary tuples. Its two later fixed shapes append
    // one structural tuple after the 25- or 29-element semantic collection.
    if (trailingTupleAfterInitialLayout && tupleCount > 2) {
        --tupleCount;
    }
    const auto storedStyleCount = (std::min)(tupleCount, semanticStyleCount);
    for (std::size_t index = 0; index < storedStyleCount; ++index) {
        const auto first = index * smartShapeConnectionStyleWords;
        const auto storedConnection = wordAt(family.words, first);
        if (storedConnection < 0 || static_cast<std::size_t>(storedConnection) >= smartShapeConnectionIndexCount) {
            continue;
        }
        auto style = std::make_shared<SmartShapeTarget::ConnectionStyle>();
        style->connectIndex = static_cast<SmartShapeTarget::ConnectionIndex>(storedConnection);
        style->xOffset = wordAt(family.words, first + 1);
        style->yOffset = wordAt(family.words, first + 2);
        styles.insert_or_assign(static_cast<typename Map::key_type>(index), std::move(style));
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            const auto prefix = std::string(collection) + "[" + std::to_string(index) + "].";
            const auto& recovered = *styles.at(static_cast<typename Map::key_type>(index));
            const auto instance = reporting.template instanceKey<SmartShapeTarget>();
            reporting.report().setField(
                instance, prefix + "connectIndex", {Reporting::Origin::LegacyMus, family.blockOffset, family.decodedOffset, storedConnection});
            reporting.report().setField(
                instance, prefix + "xOffset", {Reporting::Origin::LegacyMus, family.blockOffset, family.decodedOffset, recovered.xOffset});
            reporting.report().setField(
                instance, prefix + "yOffset", {Reporting::Origin::LegacyMus, family.blockOffset, family.decodedOffset, recovered.yOffset});
        });
    }
}

template <typename Map>
void reportSmartShapeConnectionStyleDefaults(const ImportContext& context, std::string_view collection, const Map& styles)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        std::vector<std::pair<typename Map::key_type, typename Map::mapped_type>> ordered(styles.begin(), styles.end());
        std::ranges::sort(ordered, {}, [](const auto& item) { return item.first; });
        for (std::size_t index = 0; index < ordered.size(); ++index) {
            const auto& style = *ordered[index].second;
            const auto prefix = std::string(collection) + "[" + std::to_string(index) + "].";
            const auto instance = reporting.template instanceKey<SmartShapeTarget>();
            if (!reporting.report().findField(instance, prefix + "connectIndex")) {
                reporting.report().setField(
                    instance, prefix + "connectIndex", {Reporting::Origin::Finale27Default, 0, 0, static_cast<std::int64_t>(style.connectIndex)});
            }
            if (!reporting.report().findField(instance, prefix + "xOffset")) {
                reporting.report().setField(instance, prefix + "xOffset", {Reporting::Origin::Finale27Default, 0, 0, style.xOffset});
            }
            if (!reporting.report().findField(instance, prefix + "yOffset")) {
                reporting.report().setField(instance, prefix + "yOffset", {Reporting::Origin::Finale27Default, 0, 0, style.yOffset});
            }
        }
    });
}

void reportRemainingSmartShapeFields(const ImportContext& context, const std::shared_ptr<SmartShapeTarget>& target)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto instance = reporting.template instanceKey<SmartShapeTarget>();
        reporting.report().setField(instance, "maximumShortHairpinLength", {Reporting::Origin::MusxOnly, 0, 0, target->maximumShortHairpinLength});
        if (!reporting.report().template findField<SmartShapeTarget>("direction")) {
            reporting.template defaultField<SmartShapeTarget>("direction", static_cast<std::int64_t>(target->direction));
        }
        reporting.report().setField(instance, "articAvoidSlurAmt", {Reporting::Origin::MusxOnly, 0, 0, target->articAvoidSlurAmt});
    });
    reportSmartShapeConnectionStyleDefaults(context, "slurConnectStyles", target->slurConnectStyles);
    reportSmartShapeConnectionStyleDefaults(context, "tabSlideConnectStyles", target->tabSlideConnectStyles);
    reportSmartShapeConnectionStyleDefaults(context, "glissandoConnectStyles", target->glissandoConnectStyles);
    reportSmartShapeConnectionStyleDefaults(context, "bendCurveConnectStyles", target->bendCurveConnectStyles);
}

void requestUnavailableToolLineDefaults(const ImportContext& context, const std::shared_ptr<SmartShapeTarget>& target)
{
    const bool needsAllToolLines = predatesCustomLineCapability(context.profile);
    if (!predatesTabBendCurveCapability(context.profile)) {
        return;
    }
    const auto reference = context.referenceDocument->getOptions()->get<SmartShapeTarget>();
    if (!reference) {
        throw std::logic_error("SmartShapeOptions reference document is incomplete");
    }

    struct LineRequest
    {
        musx::dom::Cmper SmartShapeTarget::* member;
        const char* reportMember;
    };
    constexpr std::array requests{
        LineRequest{&SmartShapeTarget::ssLineStyleCmpGlissando, "ssLineStyleCmpGlissando"},
        LineRequest{&SmartShapeTarget::ssLineStyleCmpTabSlide, "ssLineStyleCmpTabSlide"},
        LineRequest{&SmartShapeTarget::ssLineStyleCmpTabBendCurve, "ssLineStyleCmpTabBendCurve"},
    };
    const auto firstRequest = needsAllToolLines ? requests.begin() : requests.end() - 1;
    for (auto request = firstRequest; request != requests.end(); ++request) {
        const auto referenceId = reference.get()->*request->member;
        if (referenceId == 0) {
            throw std::logic_error("SmartShapeOptions reference document names a missing default custom line");
        }
        target.get()->*request->member = 0;
        context.pending.customLines.push_back(
            {referenceId, [target, member = request->member](musx::dom::Cmper resolved) { target.get()->*member = resolved; }});
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            reporting.state(context.pending.customLines.back().reportField) = {
                reporting.template instanceKey<SmartShapeTarget>(), request->reportMember};
        });
    }
}

} // namespace

void importSmartShapeOptions(const ImportContext& context)
{
    // The Coda epoch reuses selectors 50 through 53 for an older six-word curve layout.
    // Only selector 51's two thickness-control pairs are shared with this class.

    const auto pooled = context.document->getOptions()->get<SmartShapeTarget>();
    if (!pooled) {
        return;
    }
    const auto target = std::const_pointer_cast<SmartShapeTarget>(pooled);
    requestUnavailableToolLineDefaults(context, target);
    if (context.profile.epoch != FormatEpoch::CodaBanner) {
        captureControlStyles(context, target);
    }
    captureSmartShapeConnectionStyles(context, target, smartShapeSlurConnectionSelector, "slurConnectStyles", &SmartShapeTarget::slurConnectStyles,
        smartShapeSlurConnectionTypes.size(), true);
    captureSmartShapeConnectionStyles(context, target, smartShapeTabSlideConnectionSelector, "tabSlideConnectStyles",
        &SmartShapeTarget::tabSlideConnectStyles, smartShapeTabSlideConnectionTypes.size());
    captureSmartShapeConnectionStyles(context, target, smartShapeGlissandoConnectionSelector, "glissandoConnectStyles",
        &SmartShapeTarget::glissandoConnectStyles, smartShapeGlissandoConnectionTypes.size());
    captureSmartShapeConnectionStyles(context, target, smartShapeBendCurveConnectionSelector, "bendCurveConnectStyles",
        &SmartShapeTarget::bendCurveConnectStyles, smartShapeBendCurveConnectionTypes.size());
    captureDirection(context, target);
    applyMappingTables({&codaSlurThicknessTable(), &fixedEarlySlurTable(), &fixedSlurTable(), &classSlurTable(), &fixedLineTable(), &classLineTable(),
                           &fixedFigureTable(), &classFigureTable()},
        context.index, context.profile, context.document, context.report);
    applyPreFinale37FigureBehavior(context, target);
    captureSlurAvoidStaffLinesAmount(context, target);
    applySingleIncidenceSlurAdjustmentBehavior(context, target);
    applyFinale26HookBehavior(context, target);
    applyLegacyHairpinOpeningBehavior(context, target);
    reportRemainingSmartShapeFields(context, target);
}

} // namespace options
} // namespace finale_mus_reader
