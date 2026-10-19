// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include <iterator>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using PageFormatOptionsTarget = musx::dom::options::PageFormatOptions;

constexpr const char* pageFormatReportPrefix = "options.pageFormatOptions";
constexpr std::uint16_t scoreDistanceSelector = 1;
constexpr std::uint16_t scoreRightMarginSelector = 2;
constexpr std::uint16_t scoreFirstSystemSelector = 3;
constexpr std::uint16_t firstMarginFlagsSelector = 13;
constexpr std::uint16_t pageHeightSelector = 14;
constexpr std::uint16_t pageWidthSelector = 15;
constexpr std::uint16_t leftMarginSelector = 16;
constexpr std::uint16_t systemMarginSelector = 17;
constexpr std::uint16_t pageScalingSelector = 39;
constexpr std::uint16_t systemScalingSelector = 76;
constexpr std::uint16_t partsFormatSelector = 77;
constexpr std::uint16_t staffHeightSelector = 93;
constexpr std::uint16_t fileInfoSelector = 10;
constexpr std::int64_t preAbsoluteStaffHeight = 96 * 16;

bool hasCodaAvoidMarginCollisionFlag(const records::LegacyRecordIndex& index)
{
    return index.getOthers().get(records::packTag("fi"), GLOBALS_CMPER, 0, 51) != nullptr;
}

std::optional<std::int64_t> adjustFirstSystemTop(std::int64_t value, const records::LegacyRecordIndex& index, const SourceProfile&)
{
    const auto systemMargins = readNumericGlobalWords(index, systemMarginSelector);
    if (!systemMargins.present || systemMargins.words.empty()) {
        return std::nullopt;
    }
    return value + systemMargins.words.front();
}

std::optional<std::int64_t> adjustUncompressedFirstSystemLeft(std::int64_t value, const records::LegacyRecordIndex& index, const SourceProfile&)
{
    const auto systemMargins = readNumericGlobalWords(index, systemMarginSelector);
    if (!systemMargins.present || systemMargins.words.size() < 2) {
        return std::nullopt;
    }
    return value + systemMargins.words[1];
}

std::optional<std::int64_t> adjustUncompressedFirstSystemTop(
    std::int64_t value, const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    const auto systemMargins = readNumericGlobalWords(index, systemMarginSelector);
    if (!systemMargins.present || systemMargins.words.empty()) {
        return std::nullopt;
    }
    value += systemMargins.words[0];
    if (!storesFinale35OptionLayout(index, profile)) {
        return value;
    }
    const auto firstSystem = readNumericGlobalWords(index, scoreFirstSystemSelector);
    if (!firstSystem.present || firstSystem.words.empty()) {
        return std::nullopt;
    }
    return value + firstSystem.words[0];
}

// The Coda dialog stores one page-format set. Finale's upgrade applies that set to both
// modern destinations and uses the same page margins for both page sides. The current
// system record supplies the top margin; the numeric globals supply the remaining values.
#define CODA_PAGE_FORMAT_FIELDS(destination)                                                                              \
    MUS_LONG(PageFormatOptionsTarget, "14", GLOBALS_CMPER, 0, 0, LongWordOrder::HighFirst, destination->pageHeight),      \
        MUS_LONG(PageFormatOptionsTarget, "15", GLOBALS_CMPER, 0, 2, LongWordOrder::HighFirst, destination->pageWidth),   \
        MUS_WORD(PageFormatOptionsTarget, "39", GLOBALS_CMPER, 0, 2, destination->pagePercent),                           \
        MUS_WORD(PageFormatOptionsTarget, "16", GLOBALS_CMPER, 0, 0, destination->leftPageMarginTop),                     \
        MUS_WORD(PageFormatOptionsTarget, "16", GLOBALS_CMPER, 0, 1, destination->leftPageMarginLeft),                    \
        MUS_WORD(PageFormatOptionsTarget, "16", GLOBALS_CMPER, 0, 2, destination->leftPageMarginBottom),                  \
        MUS_WORD(PageFormatOptionsTarget, "16", GLOBALS_CMPER, 0, 3, destination->leftPageMarginRight),                   \
        MUS_WORD(PageFormatOptionsTarget, "16", GLOBALS_CMPER, 0, 0, destination->rightPageMarginTop),                    \
        MUS_WORD(PageFormatOptionsTarget, "16", GLOBALS_CMPER, 0, 1, destination->rightPageMarginLeft),                   \
        MUS_WORD(PageFormatOptionsTarget, "16", GLOBALS_CMPER, 0, 2, destination->rightPageMarginBottom),                 \
        MUS_WORD(PageFormatOptionsTarget, "16", GLOBALS_CMPER, 0, 3, destination->rightPageMarginRight),                  \
        MUS_WORD(PageFormatOptionsTarget, "IU", 0, 0, 2, destination->sysMarginTop),                                      \
        MUS_WORD(PageFormatOptionsTarget, "17", GLOBALS_CMPER, 0, 1, destination->sysMarginLeft),                         \
        MUS_WORD(PageFormatOptionsTarget, "17", GLOBALS_CMPER, 0, 3, destination->sysMarginBottom),                       \
        MUS_WORD(PageFormatOptionsTarget, "17", GLOBALS_CMPER, 0, 2, destination->sysMarginRight),                        \
        MUS_WORD(PageFormatOptionsTarget, "17", GLOBALS_CMPER, 0, 0, destination->sysDistanceBetween),                    \
        MUS_WORD(PageFormatOptionsTarget, "16", GLOBALS_CMPER, 0, 0, destination->firstPageMarginTop),                    \
        MUS_WORD_ADJUSTED(PageFormatOptionsTarget, "IU", 0, 0, 2, &adjustFirstSystemTop, destination->firstSysMarginTop), \
        MUS_WORD(PageFormatOptionsTarget, "17", GLOBALS_CMPER, 0, 1, destination->firstSysMarginLeft),                    \
        MUS_WORD(PageFormatOptionsTarget, "01", GLOBALS_CMPER, 0, 5, destination->firstSysMarginDistance)

const FieldMapping codaPageFormatFields[] = {
    CODA_PAGE_FORMAT_FIELDS(pageFormatScore),
    CODA_PAGE_FORMAT_FIELDS(pageFormatParts),
    MUS_BIT(PageFormatOptionsTarget, "fi", GLOBALS_CMPER, 51, 5, 0, avoidSystemMarginCollisions),
};

#undef CODA_PAGE_FORMAT_FIELDS

#define SCORE_COMMON_PAGE_FORMAT_FIELDS(prefix, selector, offset)                                                                                    \
    MUS_##prefix##_LONG(                                                                                                                             \
        PageFormatOptionsTarget, selector(pageHeightSelector), GLOBALS_CMPER, offset(0), LongWordOrder::HighFirst, pageFormatScore->pageHeight),     \
        MUS_##prefix##_LONG(                                                                                                                         \
            PageFormatOptionsTarget, selector(pageWidthSelector), GLOBALS_CMPER, offset(2), LongWordOrder::HighFirst, pageFormatScore->pageWidth),   \
        MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(pageScalingSelector), GLOBALS_CMPER, offset(2), pageFormatScore->pagePercent),         \
        MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(leftMarginSelector), GLOBALS_CMPER, offset(0), pageFormatScore->leftPageMarginTop),    \
        MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(leftMarginSelector), GLOBALS_CMPER, offset(1), pageFormatScore->leftPageMarginLeft),   \
        MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(leftMarginSelector), GLOBALS_CMPER, offset(2), pageFormatScore->leftPageMarginBottom), \
        MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(leftMarginSelector), GLOBALS_CMPER, offset(3), pageFormatScore->leftPageMarginRight),  \
        MUS_##prefix##_WORD(                                                                                                                         \
            PageFormatOptionsTarget, selector(scoreRightMarginSelector), GLOBALS_CMPER, offset(0), pageFormatScore->rightPageMarginTop),             \
        MUS_##prefix##_WORD(                                                                                                                         \
            PageFormatOptionsTarget, selector(scoreRightMarginSelector), GLOBALS_CMPER, offset(1), pageFormatScore->rightPageMarginLeft),            \
        MUS_##prefix##_WORD(                                                                                                                         \
            PageFormatOptionsTarget, selector(scoreRightMarginSelector), GLOBALS_CMPER, offset(2), pageFormatScore->rightPageMarginBottom),          \
        MUS_##prefix##_WORD(                                                                                                                         \
            PageFormatOptionsTarget, selector(scoreRightMarginSelector), GLOBALS_CMPER, offset(3), pageFormatScore->rightPageMarginRight),           \
        MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(systemMarginSelector), GLOBALS_CMPER, offset(1), pageFormatScore->sysMarginLeft),      \
        MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(systemMarginSelector), GLOBALS_CMPER, offset(3), pageFormatScore->sysMarginBottom),    \
        MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(systemMarginSelector), GLOBALS_CMPER, offset(2), pageFormatScore->sysMarginRight),     \
        MUS_##prefix##_WORD(                                                                                                                         \
            PageFormatOptionsTarget, selector(scoreDistanceSelector), GLOBALS_CMPER, offset(5), pageFormatScore->firstSysMarginDistance),            \
        MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(scoreRightMarginSelector), GLOBALS_CMPER, offset(4), pageFormatScore->facingPages)

#define SYSTEM_SCALING_PAGE_FORMAT_FIELDS(prefix, selector, offset)                                                                       \
    MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(systemScalingSelector), GLOBALS_CMPER, offset(3), pageFormatScore->sysPercent), \
        MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(systemScalingSelector), GLOBALS_CMPER, offset(4), pageFormatParts->sysPercent)

#define SCORE_DCL_PAGE_FORMAT_FIELDS(prefix, selector, offset)                                                                             \
    MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(systemMarginSelector), GLOBALS_CMPER, offset(0), pageFormatScore->sysMarginTop), \
        MUS_##prefix##_WORD(                                                                                                               \
            PageFormatOptionsTarget, selector(scoreDistanceSelector), GLOBALS_CMPER, offset(4), pageFormatScore->sysDistanceBetween),      \
        MUS_##prefix##_WORD(                                                                                                               \
            PageFormatOptionsTarget, selector(scoreRightMarginSelector), GLOBALS_CMPER, offset(5), pageFormatScore->firstPageMarginTop),   \
        MUS_##prefix##_WORD(                                                                                                               \
            PageFormatOptionsTarget, selector(scoreFirstSystemSelector), GLOBALS_CMPER, offset(0), pageFormatScore->firstSysMarginTop),    \
        MUS_##prefix##_WORD(                                                                                                               \
            PageFormatOptionsTarget, selector(scoreFirstSystemSelector), GLOBALS_CMPER, offset(1), pageFormatScore->firstSysMarginLeft),   \
        MUS_##prefix##_WORD(                                                                                                               \
            PageFormatOptionsTarget, selector(firstMarginFlagsSelector), GLOBALS_CMPER, offset(5), pageFormatScore->differentFirstSysMargin)

// The token-pasting wrapper above keeps the score layout stated once while selecting either
// fixed-row word slots or their coalesced class-record byte offsets.
#define MUS_FIXED_LONG(Class, tagValue, selectorValue, wordIndex, order, member) \
    MUS_IDENTIFIED_FIELD_IF(                                                     \
        Class, tagValue, selectorValue, (wordIndex) / 6, (wordIndex) % 6, ValueWidth::Long, order, BitRange{}, nullptr, nullptr, member)
#define MUS_FIXED_WORD(Class, tagValue, selectorValue, wordIndex, member)                                                                 \
    MUS_IDENTIFIED_FIELD_IF(Class, tagValue, selectorValue, (wordIndex) / 6, (wordIndex) % 6, ValueWidth::Word, LongWordOrder::HighFirst, \
        BitRange{}, nullptr, nullptr, member)
#define FIXED_SELECTOR(value) numericGlobalTag(value)
#define FIXED_OFFSET(value) value

const FieldMapping fixedScoreCommonPageFormatFields[] = {
    SCORE_COMMON_PAGE_FORMAT_FIELDS(FIXED, FIXED_SELECTOR, FIXED_OFFSET),
};

const FieldMapping fixedScoreDifferentFirstPageField[] = {
    MUS_WORD(PageFormatOptionsTarget, "13", GLOBALS_CMPER, 0, 4, pageFormatScore->differentFirstPageMargin),
};

const FieldMapping fixedScoreDclPageFormatFields[] = {
    SCORE_DCL_PAGE_FORMAT_FIELDS(FIXED, FIXED_SELECTOR, FIXED_OFFSET),
};

const FieldMapping fixedSystemScalingPageFormatFields[] = {
    SYSTEM_SCALING_PAGE_FORMAT_FIELDS(FIXED, FIXED_SELECTOR, FIXED_OFFSET),
};

const FieldMapping fixedStaffHeightFields[] = {
    MUS_WORD(PageFormatOptionsTarget, "93", GLOBALS_CMPER, 0, 2, pageFormatScore->rawStaffHeight),
    MUS_WORD(PageFormatOptionsTarget, "93", GLOBALS_CMPER, 0, 4, pageFormatParts->rawStaffHeight),
};

const FieldMapping fixedScoreUncompressedPageFormatFields[] = {
    MUS_WORD(PageFormatOptionsTarget, "17", GLOBALS_CMPER, 0, 0, pageFormatScore->sysDistanceBetween),
    MUS_WORD_ADJUSTED(PageFormatOptionsTarget, "03", GLOBALS_CMPER, 0, 1, &adjustUncompressedFirstSystemLeft, pageFormatScore->firstSysMarginLeft),
};

const FieldMapping fixedScorePreFinale35UpperSystemFields[] = {
    MUS_WORD(PageFormatOptionsTarget, "IU", 0, 0, 2, pageFormatScore->sysMarginTop),
    MUS_WORD_ADJUSTED(PageFormatOptionsTarget, "IU", 0, 0, 2, &adjustUncompressedFirstSystemTop, pageFormatScore->firstSysMarginTop),
};

const FieldMapping fixedScorePreFinale35LowerSystemFields[] = {
    MUS_WORD(PageFormatOptionsTarget, "Iu", 0, 0, 2, pageFormatScore->sysMarginTop),
    MUS_WORD_ADJUSTED(PageFormatOptionsTarget, "Iu", 0, 0, 2, &adjustUncompressedFirstSystemTop, pageFormatScore->firstSysMarginTop),
};

// The expanded current-system row stores its system top as a signed long whose
// two words follow the source platform's order.
#define FINALE35_SYSTEM_FIELDS(tagValue, order)                                                 \
    MUS_LONG(PageFormatOptionsTarget, tagValue, 0, 0, 4, order, pageFormatScore->sysMarginTop), \
        withSourceAdjustment(                                                                   \
            MUS_LONG(PageFormatOptionsTarget, tagValue, 0, 0, 4, order, pageFormatScore->firstSysMarginTop), &adjustUncompressedFirstSystemTop)

const FieldMapping fixedScoreFinale35BigEndianUpperSystemFields[] = {
    FINALE35_SYSTEM_FIELDS("IU", LongWordOrder::HighFirst),
};

const FieldMapping fixedScoreFinale35BigEndianLowerSystemFields[] = {
    FINALE35_SYSTEM_FIELDS("Iu", LongWordOrder::HighFirst),
};

const FieldMapping fixedScoreFinale35LittleEndianUpperSystemFields[] = {
    FINALE35_SYSTEM_FIELDS("IU", LongWordOrder::LowFirst),
};

const FieldMapping fixedScoreFinale35LittleEndianLowerSystemFields[] = {
    FINALE35_SYSTEM_FIELDS("Iu", LongWordOrder::LowFirst),
};

#undef FINALE35_SYSTEM_FIELDS

const FieldMapping fixedScalarCollisionPageFormatFields[] = {
    MUS_BIT(PageFormatOptionsTarget, figureTag, fileInfoSelector, 0, 2, 0, avoidSystemMarginCollisions),
};

const FieldMapping fixedPackedCollisionPageFormatFields[] = {
    MUS_BIT(PageFormatOptionsTarget, figureTag, fileInfoSelector, 0, 2, 15, avoidSystemMarginCollisions),
};

#undef FIXED_OFFSET
#undef FIXED_SELECTOR

#define MUS_CLASS_LAYOUT_LONG(Class, classValue, selectorValue, byteOffset, order, member) \
    MUS_CLASS_LONG(Class, classValue, selectorValue, byteOffset, order, member)
#define MUS_CLASS_LAYOUT_WORD(Class, classValue, selectorValue, byteOffset, member) \
    MUS_CLASS_WORD(Class, classValue, selectorValue, byteOffset, member)
#define CLASS_SELECTOR(value) numericGlobalClass(value)
#define CLASS_OFFSET(value) classWordOffset(value)

const FieldMapping classScorePageFormatFields[] = {
    SCORE_COMMON_PAGE_FORMAT_FIELDS(CLASS_LAYOUT, CLASS_SELECTOR, CLASS_OFFSET),
    SCORE_DCL_PAGE_FORMAT_FIELDS(CLASS_LAYOUT, CLASS_SELECTOR, CLASS_OFFSET),
    MUS_CLASS_WORD(PageFormatOptionsTarget, numericGlobalClass(firstMarginFlagsSelector), GLOBALS_CMPER, classWordOffset(4),
        pageFormatScore->differentFirstPageMargin),
};

const FieldMapping classSystemScalingPageFormatFields[] = {
    SYSTEM_SCALING_PAGE_FORMAT_FIELDS(CLASS_LAYOUT, CLASS_SELECTOR, CLASS_OFFSET),
};

const FieldMapping classStaffHeightFields[] = {
    MUS_CLASS_WORD(
        PageFormatOptionsTarget, numericGlobalClass(staffHeightSelector), GLOBALS_CMPER, classWordOffset(2), pageFormatScore->rawStaffHeight),
    MUS_CLASS_WORD(
        PageFormatOptionsTarget, numericGlobalClass(staffHeightSelector), GLOBALS_CMPER, classWordOffset(4), pageFormatParts->rawStaffHeight),
};

const FieldMapping classOuterPageFormatFields[] = {
    MUS_CLASS_BIT(PageFormatOptionsTarget, zlibFigureClass, fileInfoSelector, classWordOffset(2), 15, avoidSystemMarginCollisions),
};

#undef CLASS_OFFSET
#undef CLASS_SELECTOR
#undef MUS_CLASS_LAYOUT_WORD
#undef MUS_CLASS_LAYOUT_LONG
#undef SCORE_DCL_PAGE_FORMAT_FIELDS
#undef SCORE_COMMON_PAGE_FORMAT_FIELDS
#undef SYSTEM_SCALING_PAGE_FORMAT_FIELDS

#define PARTS_PAGE_FORMAT_DIMENSIONS(prefix, selector, offset, order)                                                                          \
    MUS_##prefix##_LONG(PageFormatOptionsTarget, selector(partsFormatSelector), GLOBALS_CMPER, offset(0), order, pageFormatParts->pageHeight), \
        MUS_##prefix##_LONG(PageFormatOptionsTarget, selector(partsFormatSelector), GLOBALS_CMPER, offset(2), order, pageFormatParts->pageWidth)

#define PARTS_COMMON_PAGE_FORMAT_FIELDS(prefix, selector, offset)                                                                                    \
    MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(partsFormatSelector), GLOBALS_CMPER, offset(4), pageFormatParts->pagePercent),             \
        MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(partsFormatSelector), GLOBALS_CMPER, offset(5), pageFormatParts->leftPageMarginTop),   \
        MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(partsFormatSelector), GLOBALS_CMPER, offset(6), pageFormatParts->leftPageMarginLeft),  \
        MUS_##prefix##_WORD(                                                                                                                         \
            PageFormatOptionsTarget, selector(partsFormatSelector), GLOBALS_CMPER, offset(7), pageFormatParts->leftPageMarginBottom),                \
        MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(partsFormatSelector), GLOBALS_CMPER, offset(8), pageFormatParts->leftPageMarginRight), \
        MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(partsFormatSelector), GLOBALS_CMPER, offset(9), pageFormatParts->rightPageMarginTop),  \
        MUS_##prefix##_WORD(                                                                                                                         \
            PageFormatOptionsTarget, selector(partsFormatSelector), GLOBALS_CMPER, offset(10), pageFormatParts->rightPageMarginLeft),                \
        MUS_##prefix##_WORD(                                                                                                                         \
            PageFormatOptionsTarget, selector(partsFormatSelector), GLOBALS_CMPER, offset(11), pageFormatParts->rightPageMarginBottom),              \
        MUS_##prefix##_WORD(                                                                                                                         \
            PageFormatOptionsTarget, selector(partsFormatSelector), GLOBALS_CMPER, offset(12), pageFormatParts->rightPageMarginRight),               \
        MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(partsFormatSelector), GLOBALS_CMPER, offset(14), pageFormatParts->sysMarginLeft),      \
        MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(partsFormatSelector), GLOBALS_CMPER, offset(15), pageFormatParts->sysMarginBottom),    \
        MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(partsFormatSelector), GLOBALS_CMPER, offset(16), pageFormatParts->sysMarginRight)

#define PARTS_DCL_PAGE_FORMAT_FIELDS(prefix, selector, offset)                                                                                       \
    MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(partsFormatSelector), GLOBALS_CMPER, offset(13), pageFormatParts->sysMarginTop),           \
        MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(partsFormatSelector), GLOBALS_CMPER, offset(17), pageFormatParts->sysDistanceBetween), \
        MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(partsFormatSelector), GLOBALS_CMPER, offset(18), pageFormatParts->facingPages),        \
        MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(partsFormatSelector), GLOBALS_CMPER, offset(21), pageFormatParts->firstPageMarginTop), \
        MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(partsFormatSelector), GLOBALS_CMPER, offset(22), pageFormatParts->firstSysMarginTop),  \
        MUS_##prefix##_WORD(PageFormatOptionsTarget, selector(partsFormatSelector), GLOBALS_CMPER, offset(23), pageFormatParts->firstSysMarginLeft), \
        MUS_##prefix##_WORD(                                                                                                                         \
            PageFormatOptionsTarget, selector(partsFormatSelector), GLOBALS_CMPER, offset(24), pageFormatParts->firstSysMarginDistance),             \
        MUS_##prefix##_WORD(                                                                                                                         \
            PageFormatOptionsTarget, selector(partsFormatSelector), GLOBALS_CMPER, offset(19), pageFormatParts->differentFirstSysMargin),            \
        MUS_##prefix##_WORD(                                                                                                                         \
            PageFormatOptionsTarget, selector(partsFormatSelector), GLOBALS_CMPER, offset(20), pageFormatParts->differentFirstPageMargin)

#define FIXED_SELECTOR(value) numericGlobalTag(value)
#define FIXED_OFFSET(value) value

const FieldMapping fixedPartsCommonPageFormatFields[] = {
    PARTS_COMMON_PAGE_FORMAT_FIELDS(FIXED, FIXED_SELECTOR, FIXED_OFFSET),
};

// Selector 77 stores its two dimensions in the source's long-word order, unlike the
// score dimension selectors, which remain high-word first on both platforms.
const FieldMapping fixedPartsBigEndianDimensions[] = {
    PARTS_PAGE_FORMAT_DIMENSIONS(FIXED, FIXED_SELECTOR, FIXED_OFFSET, LongWordOrder::HighFirst),
};

const FieldMapping fixedPartsLittleEndianDimensions[] = {
    PARTS_PAGE_FORMAT_DIMENSIONS(FIXED, FIXED_SELECTOR, FIXED_OFFSET, LongWordOrder::LowFirst),
};

const FieldMapping fixedPartsDclPageFormatFields[] = {
    PARTS_DCL_PAGE_FORMAT_FIELDS(FIXED, FIXED_SELECTOR, FIXED_OFFSET),
};

const FieldMapping fixedPartsUncompressedPageFormatFields[] = {
    MUS_WORD(PageFormatOptionsTarget, "77", GLOBALS_CMPER, 2, 1, pageFormatParts->sysDistanceBetween),
    MUS_WORD(PageFormatOptionsTarget, "77", GLOBALS_CMPER, 2, 5, pageFormatParts->facingPages),
};

#undef FIXED_OFFSET
#undef FIXED_SELECTOR
#undef MUS_FIXED_WORD
#undef MUS_FIXED_LONG

#define MUS_CLASS_LAYOUT_LONG(Class, classValue, selectorValue, byteOffset, order, member) \
    MUS_CLASS_LONG(Class, classValue, selectorValue, byteOffset, order, member)
#define MUS_CLASS_LAYOUT_WORD(Class, classValue, selectorValue, byteOffset, member) \
    MUS_CLASS_WORD(Class, classValue, selectorValue, byteOffset, member)
#define CLASS_SELECTOR(value) numericGlobalClass(value)
#define CLASS_OFFSET(value) classWordOffset(value)

const FieldMapping classPartsCommonPageFormatFields[] = {
    PARTS_COMMON_PAGE_FORMAT_FIELDS(CLASS_LAYOUT, CLASS_SELECTOR, CLASS_OFFSET),
    PARTS_DCL_PAGE_FORMAT_FIELDS(CLASS_LAYOUT, CLASS_SELECTOR, CLASS_OFFSET),
};

const FieldMapping classPartsBigEndianDimensions[] = {
    PARTS_PAGE_FORMAT_DIMENSIONS(CLASS_LAYOUT, CLASS_SELECTOR, CLASS_OFFSET, LongWordOrder::HighFirst),
};

const FieldMapping classPartsLittleEndianDimensions[] = {
    PARTS_PAGE_FORMAT_DIMENSIONS(CLASS_LAYOUT, CLASS_SELECTOR, CLASS_OFFSET, LongWordOrder::LowFirst),
};

#undef CLASS_OFFSET
#undef CLASS_SELECTOR
#undef MUS_CLASS_LAYOUT_WORD
#undef MUS_CLASS_LAYOUT_LONG
#undef PARTS_DCL_PAGE_FORMAT_FIELDS
#undef PARTS_COMMON_PAGE_FORMAT_FIELDS
#undef PARTS_PAGE_FORMAT_DIMENSIONS

const MappingTable& codaPageFormatTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::CodaBanner,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = codaPageFormatFields,
        .fieldCount = std::size(codaPageFormatFields)};
    return table;
}

const MappingTable& fixedScoreCommonPageFormatTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::FixedRow,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = fixedScoreCommonPageFormatFields,
        .fieldCount = std::size(fixedScoreCommonPageFormatFields)};
    return table;
}

bool fixedSourceStoresDifferentFirstPageMargin(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    if (profile.epoch == FormatEpoch::DclLegacy) {
        return true;
    }
    // Selector 77 arrives with the expanded page-format layout and is a structural
    // marker for this flag. A damaged later file missing that selector is ambiguous
    // and retains the seeded false value.
    return profile.epoch == FormatEpoch::UncompressedLegacy && readNumericGlobalWords(index, partsFormatSelector).present;
}

const MappingTable& fixedScoreDifferentFirstPageTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::FixedRow,
        .applies = &fixedSourceStoresDifferentFirstPageMargin,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = fixedScoreDifferentFirstPageField,
        .fieldCount = std::size(fixedScoreDifferentFirstPageField)};
    return table;
}

bool fixedSourceStoresScalarCollisionFlag(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return profile.epoch == FormatEpoch::UncompressedLegacy && !storesFinale35OptionLayout(index, profile);
}

const MappingTable& fixedScalarCollisionPageFormatTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::Uncompressed,
        .applies = &fixedSourceStoresScalarCollisionFlag,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = fixedScalarCollisionPageFormatFields,
        .fieldCount = std::size(fixedScalarCollisionPageFormatFields)};
    return table;
}

bool fixedSourceStoresPackedCollisionFlag(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return profile.epoch == FormatEpoch::DclLegacy
           || (profile.epoch == FormatEpoch::UncompressedLegacy && storesFinale35OptionLayout(index, profile));
}

const MappingTable& fixedPackedCollisionPageFormatTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::FixedRow,
        .applies = &fixedSourceStoresPackedCollisionFlag,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = fixedPackedCollisionPageFormatFields,
        .fieldCount = std::size(fixedPackedCollisionPageFormatFields)};
    return table;
}

const MappingTable& fixedScoreDclPageFormatTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::Dcl,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = fixedScoreDclPageFormatFields,
        .fieldCount = std::size(fixedScoreDclPageFormatFields)};
    return table;
}

bool fixedSourceStoresFinale2002PageFormatFields(const records::LegacyRecordIndex&, const SourceProfile& profile)
{
    // System scaling and absolute staff height begin with Finale 2002. Earlier words at
    // those selectors are placeholders or unrelated values.
    return sourceAtOrAfter(profile, FormatEpoch::DclLegacy, versions::finale2002);
}

const MappingTable& fixedSystemScalingPageFormatTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::Dcl,
        .applies = &fixedSourceStoresFinale2002PageFormatFields,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = fixedSystemScalingPageFormatFields,
        .fieldCount = std::size(fixedSystemScalingPageFormatFields)};
    return table;
}

const MappingTable& fixedStaffHeightTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::Dcl,
        .applies = &fixedSourceStoresFinale2002PageFormatFields,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = fixedStaffHeightFields,
        .fieldCount = std::size(fixedStaffHeightFields)};
    return table;
}

bool hasUncompressedUpperSystemRecord(const records::LegacyRecordIndex& index, const SourceProfile&)
{
    return index.getOthers().get(records::packTag("IU"), 0, 0, 0) != nullptr;
}

bool hasUncompressedLowerSystemRecord(const records::LegacyRecordIndex& index, const SourceProfile&)
{
    return index.getOthers().get(records::packTag("Iu"), 0, 0, 0) != nullptr;
}

bool storesPreFinale35PageFormatLayout(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return !storesFinale35OptionLayout(index, profile);
}

bool storesPreFinale35UpperSystemLayout(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return storesPreFinale35PageFormatLayout(index, profile) && hasUncompressedUpperSystemRecord(index, profile);
}

bool storesPreFinale35LowerSystemLayout(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return storesPreFinale35PageFormatLayout(index, profile) && hasUncompressedLowerSystemRecord(index, profile);
}

bool storesFinale35BigEndianUpperSystemLayout(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return storesFinale35OptionLayout(index, profile) && profile.byteOrder == ByteOrder::BigEndian
           && hasUncompressedUpperSystemRecord(index, profile);
}

bool storesFinale35BigEndianLowerSystemLayout(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return storesFinale35OptionLayout(index, profile) && profile.byteOrder == ByteOrder::BigEndian
           && hasUncompressedLowerSystemRecord(index, profile);
}

bool storesFinale35LittleEndianUpperSystemLayout(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return storesFinale35OptionLayout(index, profile) && profile.byteOrder == ByteOrder::LittleEndian
           && hasUncompressedUpperSystemRecord(index, profile);
}

bool storesFinale35LittleEndianLowerSystemLayout(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return storesFinale35OptionLayout(index, profile) && profile.byteOrder == ByteOrder::LittleEndian
           && hasUncompressedLowerSystemRecord(index, profile);
}

const MappingTable& fixedScoreUncompressedPageFormatTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::Uncompressed,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = fixedScoreUncompressedPageFormatFields,
        .fieldCount = std::size(fixedScoreUncompressedPageFormatFields)};
    return table;
}

const MappingTable& fixedScorePreFinale35UpperSystemTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::Uncompressed,
        .applies = &storesPreFinale35UpperSystemLayout,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = fixedScorePreFinale35UpperSystemFields,
        .fieldCount = std::size(fixedScorePreFinale35UpperSystemFields)};
    return table;
}

const MappingTable& fixedScorePreFinale35LowerSystemTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::Uncompressed,
        .applies = &storesPreFinale35LowerSystemLayout,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = fixedScorePreFinale35LowerSystemFields,
        .fieldCount = std::size(fixedScorePreFinale35LowerSystemFields)};
    return table;
}

const MappingTable& fixedScoreFinale35BigEndianUpperSystemTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::Uncompressed,
        .applies = &storesFinale35BigEndianUpperSystemLayout,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = fixedScoreFinale35BigEndianUpperSystemFields,
        .fieldCount = std::size(fixedScoreFinale35BigEndianUpperSystemFields)};
    return table;
}

const MappingTable& fixedScoreFinale35BigEndianLowerSystemTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::Uncompressed,
        .applies = &storesFinale35BigEndianLowerSystemLayout,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = fixedScoreFinale35BigEndianLowerSystemFields,
        .fieldCount = std::size(fixedScoreFinale35BigEndianLowerSystemFields)};
    return table;
}

const MappingTable& fixedScoreFinale35LittleEndianUpperSystemTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::Uncompressed,
        .applies = &storesFinale35LittleEndianUpperSystemLayout,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = fixedScoreFinale35LittleEndianUpperSystemFields,
        .fieldCount = std::size(fixedScoreFinale35LittleEndianUpperSystemFields)};
    return table;
}

const MappingTable& fixedScoreFinale35LittleEndianLowerSystemTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::Uncompressed,
        .applies = &storesFinale35LittleEndianLowerSystemLayout,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = fixedScoreFinale35LittleEndianLowerSystemFields,
        .fieldCount = std::size(fixedScoreFinale35LittleEndianLowerSystemFields)};
    return table;
}

const MappingTable& fixedPartsCommonPageFormatTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::FixedRow,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = fixedPartsCommonPageFormatFields,
        .fieldCount = std::size(fixedPartsCommonPageFormatFields)};
    return table;
}

bool pageFormatUsesBigEndianDimensions(const records::LegacyRecordIndex&, const SourceProfile& profile)
{
    return profile.byteOrder == ByteOrder::BigEndian;
}

bool pageFormatUsesLittleEndianDimensions(const records::LegacyRecordIndex&, const SourceProfile& profile)
{
    return profile.byteOrder == ByteOrder::LittleEndian;
}

const MappingTable& fixedPartsBigEndianDimensionsTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::FixedRow,
        .applies = &pageFormatUsesBigEndianDimensions,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = fixedPartsBigEndianDimensions,
        .fieldCount = std::size(fixedPartsBigEndianDimensions)};
    return table;
}

const MappingTable& fixedPartsLittleEndianDimensionsTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::FixedRow,
        .applies = &pageFormatUsesLittleEndianDimensions,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = fixedPartsLittleEndianDimensions,
        .fieldCount = std::size(fixedPartsLittleEndianDimensions)};
    return table;
}

const MappingTable& fixedPartsDclPageFormatTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::Dcl,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = fixedPartsDclPageFormatFields,
        .fieldCount = std::size(fixedPartsDclPageFormatFields)};
    return table;
}

const MappingTable& fixedPartsUncompressedPageFormatTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::Uncompressed,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = fixedPartsUncompressedPageFormatFields,
        .fieldCount = std::size(fixedPartsUncompressedPageFormatFields)};
    return table;
}

const MappingTable& classScorePageFormatTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = classScorePageFormatFields,
        .fieldCount = std::size(classScorePageFormatFields)};
    return table;
}

const MappingTable& classSystemScalingPageFormatTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = classSystemScalingPageFormatFields,
        .fieldCount = std::size(classSystemScalingPageFormatFields)};
    return table;
}

const MappingTable& classStaffHeightTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = classStaffHeightFields,
        .fieldCount = std::size(classStaffHeightFields)};
    return table;
}

const MappingTable& classOuterPageFormatTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = classOuterPageFormatFields,
        .fieldCount = std::size(classOuterPageFormatFields)};
    return table;
}

const MappingTable& classPartsCommonPageFormatTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = classPartsCommonPageFormatFields,
        .fieldCount = std::size(classPartsCommonPageFormatFields)};
    return table;
}

const MappingTable& classPartsBigEndianDimensionsTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::Zlib,
        .applies = &pageFormatUsesBigEndianDimensions,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = classPartsBigEndianDimensions,
        .fieldCount = std::size(classPartsBigEndianDimensions)};
    return table;
}

const MappingTable& classPartsLittleEndianDimensionsTable()
{
    static const MappingTable table{.reportPrefix = pageFormatReportPrefix,
        .epochs = EpochMask::Zlib,
        .applies = &pageFormatUsesLittleEndianDimensions,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<PageFormatOptionsTarget>,
        .fields = classPartsLittleEndianDimensions,
        .fieldCount = std::size(classPartsLittleEndianDimensions)};
    return table;
}

void applyPageFormatBehavior(const ImportContext& context, PageFormatOptionsTarget& target)
{
    const auto reportBehavior = [&](const char* member, std::int64_t value) {
        withReporting(context.report,
            [&]<typename Reporting>(Reporting& reporting) { reporting.template behaviorField<PageFormatOptionsTarget>(member, value); });
    };

    if (context.profile.epoch == FormatEpoch::CodaBanner && !hasCodaAvoidMarginCollisionFlag(context.index)) {
        // Coda documents with the option carry file-info incidence 51; earlier files do
        // not. Absence is therefore the capability marker, though a damaged later file
        // missing that incidence is indistinguishable and receives the earlier behavior.
        target.avoidSystemMarginCollisions = false;
        reportBehavior("avoidSystemMarginCollisions", target.avoidSystemMarginCollisions);
    }
    if (sourcePredatesVersion(context.profile, FormatEpoch::DclLegacy, versions::finale2002)) {
        // Before the absolute staff-height preference, Page Format uses the fixed
        // 96-EVPU height represented by the modern field's sixteenth-EVPU units.
        if (target.pageFormatScore) {
            target.pageFormatScore->rawStaffHeight = preAbsoluteStaffHeight;
            reportBehavior("pageFormatScore.rawStaffHeight", target.pageFormatScore->rawStaffHeight);
        }
        if (target.pageFormatParts) {
            target.pageFormatParts->rawStaffHeight = preAbsoluteStaffHeight;
            reportBehavior("pageFormatParts.rawStaffHeight", target.pageFormatParts->rawStaffHeight);
        }
    }
    if (context.profile.epoch != FormatEpoch::UncompressedLegacy || !target.pageFormatScore || !target.pageFormatParts) {
        return;
    }

    auto& score = *target.pageFormatScore;
    if (!storesFinale35OptionLayout(context.index, context.profile)) {
        // Before facing-page and first-system-indent settings, the left-page and
        // ordinary-system margins govern their corresponding modern fields.
        score.rightPageMarginTop = score.leftPageMarginTop;
        score.rightPageMarginLeft = score.leftPageMarginLeft;
        score.rightPageMarginBottom = score.leftPageMarginBottom;
        score.rightPageMarginRight = score.leftPageMarginRight;
        score.firstSysMarginLeft = score.sysMarginLeft;
        reportBehavior("pageFormatScore.rightPageMarginTop", score.rightPageMarginTop);
        reportBehavior("pageFormatScore.rightPageMarginLeft", score.rightPageMarginLeft);
        reportBehavior("pageFormatScore.rightPageMarginBottom", score.rightPageMarginBottom);
        reportBehavior("pageFormatScore.rightPageMarginRight", score.rightPageMarginRight);
        reportBehavior("pageFormatScore.firstSysMarginLeft", score.firstSysMarginLeft);
    }
    // Believed: an uncompressed document with no current-system row uses the era's
    // standard system top while retaining its stored inter-system distance.
    if (!hasUncompressedUpperSystemRecord(context.index, context.profile) && !hasUncompressedLowerSystemRecord(context.index, context.profile)) {
        score.sysMarginTop = -80;
        score.firstSysMarginTop = score.sysMarginTop + score.sysDistanceBetween;
        reportBehavior("pageFormatScore.sysMarginTop", score.sysMarginTop);
        reportBehavior("pageFormatScore.firstSysMarginTop", score.firstSysMarginTop);
    }
    score.firstPageMarginTop = score.leftPageMarginTop;
    reportBehavior("pageFormatScore.firstPageMarginTop", score.firstPageMarginTop);

    auto& parts = *target.pageFormatParts;
    const auto storedParts = readNumericGlobalWords(context.index, partsFormatSelector);
    if (!storedParts.present) {
        // Selector 77 is the shape marker for an independent parts page format. Without
        // it, the legacy dialog's single set applies to both modern destinations. A later
        // damaged file that loses the selector is indistinguishable and follows this rule.
        parts = score;
#define REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(member) reportBehavior("pageFormatParts." #member, parts.member)
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(pageHeight);
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(pageWidth);
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(pagePercent);
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(sysPercent);
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(rawStaffHeight);
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(leftPageMarginTop);
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(leftPageMarginLeft);
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(leftPageMarginBottom);
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(leftPageMarginRight);
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(rightPageMarginTop);
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(rightPageMarginLeft);
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(rightPageMarginBottom);
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(rightPageMarginRight);
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(sysMarginTop);
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(sysMarginLeft);
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(sysMarginBottom);
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(sysMarginRight);
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(sysDistanceBetween);
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(firstPageMarginTop);
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(firstSysMarginTop);
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(firstSysMarginLeft);
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(firstSysMarginDistance);
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(facingPages);
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(differentFirstSysMargin);
        REPORT_PARTS_PAGE_FORMAT_BEHAVIOR(differentFirstPageMargin);
#undef REPORT_PARTS_PAGE_FORMAT_BEHAVIOR
        return;
    }

    parts.rawStaffHeight = score.rawStaffHeight;
    parts.sysMarginTop = score.sysMarginTop;
    parts.firstPageMarginTop = parts.leftPageMarginTop;
    reportBehavior("pageFormatParts.rawStaffHeight", parts.rawStaffHeight);
    reportBehavior("pageFormatParts.sysMarginTop", parts.sysMarginTop);
    reportBehavior("pageFormatParts.firstPageMarginTop", parts.firstPageMarginTop);

    if (storedParts.words.size() > 20) {
        // The uncompressed selector stores first-system offsets rather than the later
        // absolute first-system values and switches at the same word positions.
        parts.firstSysMarginTop = parts.sysMarginTop + parts.sysDistanceBetween + storedParts.words[19];
        parts.firstSysMarginLeft = parts.sysMarginLeft + storedParts.words[20];
        reportBehavior("pageFormatParts.firstSysMarginTop", parts.firstSysMarginTop);
        reportBehavior("pageFormatParts.firstSysMarginLeft", parts.firstSysMarginLeft);
    }
}

void reportRemainingPageFormatFields(const ImportContext& context, const PageFormatOptionsTarget& target)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto instance = reporting.template instanceKey<PageFormatOptionsTarget>();
        reporting.report().setField(
            instance, "adjustPageScope", {Reporting::Origin::Finale27Default, 0, 0, static_cast<std::int64_t>(target.adjustPageScope)});
        if (!reporting.report().findField(instance, "avoidSystemMarginCollisions")) {
            reporting.unmappedField(instance, "avoidSystemMarginCollisions", target.avoidSystemMarginCollisions);
        }
    });
}

} // namespace

void importPageFormatOptions(const ImportContext& context)
{
    applyMappingTables(
        {&codaPageFormatTable(), &fixedScoreCommonPageFormatTable(), &fixedScoreDifferentFirstPageTable(), &fixedScalarCollisionPageFormatTable(),
            &fixedPackedCollisionPageFormatTable(), &fixedScoreDclPageFormatTable(), &fixedSystemScalingPageFormatTable(), &fixedStaffHeightTable(),
            &fixedScoreUncompressedPageFormatTable(), &fixedScorePreFinale35UpperSystemTable(), &fixedScorePreFinale35LowerSystemTable(),
            &fixedScoreFinale35BigEndianUpperSystemTable(), &fixedScoreFinale35BigEndianLowerSystemTable(),
            &fixedScoreFinale35LittleEndianUpperSystemTable(), &fixedScoreFinale35LittleEndianLowerSystemTable(), &fixedPartsCommonPageFormatTable(),
            &fixedPartsBigEndianDimensionsTable(), &fixedPartsLittleEndianDimensionsTable(), &fixedPartsDclPageFormatTable(),
            &fixedPartsUncompressedPageFormatTable(), &classScorePageFormatTable(), &classSystemScalingPageFormatTable(), &classStaffHeightTable(),
            &classOuterPageFormatTable(), &classPartsCommonPageFormatTable(), &classPartsBigEndianDimensionsTable(),
            &classPartsLittleEndianDimensionsTable()},
        context.index, context.profile, context.document, context.report);
    if (const auto pooled = context.document->getOptions()->get<PageFormatOptionsTarget>()) {
        const auto target = std::const_pointer_cast<PageFormatOptionsTarget>(pooled);
        applyPageFormatBehavior(context, *target);
        reportRemainingPageFormatFields(context, *target);
    }
}

} // namespace options
} // namespace finale_mus_reader
