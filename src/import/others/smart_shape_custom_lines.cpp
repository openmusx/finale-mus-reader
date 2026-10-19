// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>

#include "import/support/text_encoding.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using CustomLine = musx::dom::others::SmartShapeCustomLine;

constexpr const char* ssLineReportPrefix = "others.smartShapeCustomLine";

// Two-character tag through Finale 2006; the zlib serialization renumbers it to class
// 0x00de. The tag text is spelled once here: the field rows pack it and the tables name the
// packed value.
//
// One record is six incidences of six words, or one 72-byte class payload -- the same 36-word
// stream in either encoding.
#define SS_LINE_TAG_TEXT "ls"
constexpr auto ssLineTag = records::packTag(SS_LINE_TAG_TEXT);
constexpr records::LegacyTag ssLineClass = 0x00de;

// Logical word slots of one record, 0-indexed over that whole stream. Slots 5, 10, 12, 34 and
// 35 are left unrecovered rather than guessed; they correspond to no musxdom member and nothing
// establishes what they hold. Slot 5 is the only one that is ever nonzero: a Char line stores
// -1 there, with no established meaning.
//
// Five slots are shared between mutually exclusive layouts, which the record itself states in
// the two type slots below: the line style selects what slots 1 through 4 mean, and each cap
// type selects whether its value slot is an arrowhead comparator or a hook length. A Char line
// spends slots 2, 3 and 4 on the ordinary legacy font tuple of id, size and style mask.
constexpr std::uint32_t ssLineStyleSlot = 0;
constexpr std::uint32_t ssLineCharSlot = 1;          // Char only
constexpr std::uint32_t ssLineWidthSlot = 1;        // Solid and Dashed only
constexpr std::uint32_t ssLineDashOnSlot = 2;       // Dashed only
constexpr std::uint32_t ssLineFontIdSlot = 2;       // Char only
constexpr std::uint32_t ssLineDashOffSlot = 3;      // Dashed only
constexpr std::uint32_t ssLineFontSizeSlot = 3;     // Char only
constexpr std::uint32_t ssLineFontEfxSlot = 4;      // Char only
constexpr std::uint32_t ssLineBaselineShiftSlot = 6; // Char only
constexpr std::uint32_t ssLineCapStartTypeSlot = 7;
constexpr std::uint32_t ssLineCapEndTypeSlot = 8;
constexpr std::uint32_t ssLineCapStartValueSlot = 9;
constexpr std::uint32_t ssLineCapEndValueSlot = 11;
constexpr std::uint32_t ssLineFlagsSlot = 13;
constexpr std::uint32_t ssLineLeftStartTextSlot = 14;
constexpr std::uint32_t ssLineLeftContTextSlot = 15;
constexpr std::uint32_t ssLineRightEndTextSlot = 16;
constexpr std::uint32_t ssLineCenterFullTextSlot = 17;
constexpr std::uint32_t ssLineCenterAbbrTextSlot = 18;
constexpr std::uint32_t ssLineLeftStartXSlot = 19;
constexpr std::uint32_t ssLineLeftStartYSlot = 20;
constexpr std::uint32_t ssLineLeftContXSlot = 21;
constexpr std::uint32_t ssLineLeftContYSlot = 22;
constexpr std::uint32_t ssLineRightEndXSlot = 23;
constexpr std::uint32_t ssLineRightEndYSlot = 24;
constexpr std::uint32_t ssLineCenterFullXSlot = 25;
constexpr std::uint32_t ssLineCenterFullYSlot = 26;
constexpr std::uint32_t ssLineCenterAbbrXSlot = 27;
constexpr std::uint32_t ssLineCenterAbbrYSlot = 28;
// The line adjustments follow, in the order musxdom declares them. Finale keeps one vertical
// control for the whole line and writes it to both of the two slots that hold one, so a
// document never has @ref CustomLine::lineStartY and @ref CustomLine::lineEndY disagreeing.
constexpr std::uint32_t ssLineStartXSlot = 29;
constexpr std::uint32_t ssLineStartYSlot = 30;
constexpr std::uint32_t ssLineEndXSlot = 31;
constexpr std::uint32_t ssLineEndYSlot = 32;
constexpr std::uint32_t ssLineContXSlot = 33;

// The four bits of the flags word with a known meaning. The rest are left unread rather than
// guessed, as everywhere else a flags word is only partly understood.
constexpr std::uint8_t ssLineMakeHorzBit = 0;
constexpr std::uint8_t ssLineAfterLeftStartTextBit = 1;
constexpr std::uint8_t ssLineBeforeRightEndTextBit = 2;
constexpr std::uint8_t ssLineAfterLeftContTextBit = 3;

/// @brief The last slot of the parameter block the line style selects.
/// @details Slots 1 through here belong to whichever of the three parameter layouts the style
/// names; everything after is common to all three.
constexpr std::uint32_t ssLineParamsEndSlot = 6;

/// @brief The word a logical slot occupies in a source that stores the line character wide.
/// @details Finale 2012 widened the character from one word to two. The character belongs to
/// the Char parameter block, so **only that block's own later fields move with it**: a Solid or
/// Dashed record has no character and keeps its width and dash lengths exactly where the
/// earlier layouts put them. The block is one word longer in every record all the same, so
/// everything after it moves whatever the line style is.
///
/// This is the Finale 2012 boundary shared with the clef and stem-connection tables.
constexpr std::uint32_t ssLineUnicodeSlot(std::uint32_t slot)
{
    return slot > ssLineParamsEndSlot ? slot + 1 : slot;
}

/// @brief The same, for a field of the Char parameter block, which follows the wide character.
constexpr std::uint32_t ssLineUnicodeCharSlot(std::uint32_t slot)
{
    return slot > ssLineCharSlot ? slot + 1 : slot;
}

// The addressings of the one layout below. A fixed-row family is addressed as one continuous
// word stream, so a logical slot is already its absolute word index; the zlib class payload
// states the same stream by byte. Only the wide layout needs to distinguish the Char parameter
// block from everything else, so the earlier encodings supply the same function twice.
constexpr std::uint32_t ssLineRowWord(std::uint32_t slot)
{
    return slot;
}
constexpr std::uint32_t ssLineNarrowByte(std::uint32_t slot)
{
    return classWordOffset(slot);
}
constexpr std::uint32_t ssLineWideByte(std::uint32_t slot)
{
    return classWordOffset(ssLineUnicodeSlot(slot));
}
constexpr std::uint32_t ssLineWideCharByte(std::uint32_t slot)
{
    return classWordOffset(ssLineUnicodeCharSlot(slot));
}

/// @brief The line style a raw slot-0 value names.
/// @details The stored order is the legacy one and does not match the declared order of
/// musxdom's enumerators, so it is converted rather than cast. A value outside the three is
/// left at the constructed default; the raw word is reported for every record, so an
/// unrecognized one is visible in the report as the value it actually held.
CustomLine::LineStyle ssLineStyleFromRaw(std::int64_t raw)
{
    switch (raw) {
    case 1: return CustomLine::LineStyle::Dashed;
    case 2: return CustomLine::LineStyle::Char;
    default: return CustomLine::LineStyle::Solid;
    }
}

/// @brief The line cap a raw cap-type value names, in the same converted sense.
CustomLine::LineCapType ssLineCapFromRaw(std::int64_t raw)
{
    switch (raw) {
    case 1: return CustomLine::LineCapType::ArrowheadPreset;
    case 2: return CustomLine::LineCapType::ArrowheadCustom;
    case 3: return CustomLine::LineCapType::Hook;
    default: return CustomLine::LineCapType::None;
    }
}

// Which of the record's mutually exclusive fields exist, read from the record's own
// deciding slot. Every row that names one of these appears after the row that reads the slot
// it tests.
bool ssLineIsChar(const void* instance)
{
    return static_cast<const CustomLine*>(instance)->lineStyle == CustomLine::LineStyle::Char;
}

bool ssLineIsSolid(const void* instance)
{
    return static_cast<const CustomLine*>(instance)->lineStyle == CustomLine::LineStyle::Solid;
}

bool ssLineIsDashed(const void* instance)
{
    return static_cast<const CustomLine*>(instance)->lineStyle == CustomLine::LineStyle::Dashed;
}

bool ssLineCapIsArrowhead(CustomLine::LineCapType cap)
{
    return cap == CustomLine::LineCapType::ArrowheadPreset || cap == CustomLine::LineCapType::ArrowheadCustom;
}

bool ssLineStartIsArrowhead(const void* instance)
{
    return ssLineCapIsArrowhead(static_cast<const CustomLine*>(instance)->lineCapStartType);
}

bool ssLineStartIsHook(const void* instance)
{
    return static_cast<const CustomLine*>(instance)->lineCapStartType == CustomLine::LineCapType::Hook;
}

bool ssLineEndIsArrowhead(const void* instance)
{
    return ssLineCapIsArrowhead(static_cast<const CustomLine*>(instance)->lineCapEndType);
}

bool ssLineEndIsHook(const void* instance)
{
    return static_cast<const CustomLine*>(instance)->lineCapEndType == CustomLine::LineCapType::Hook;
}

/// @brief Creates one line style with all three parameter blocks allocated.
/// @details Which block a record uses is the record's own business, but a field row cannot
/// allocate the block it writes into: a contained object needs a shared pointer to its owner,
/// which only creation holds. All three are therefore allocated here and
/// @ref ssLineFinalize drops the two the record does not select, so the finished
/// object carries exactly the block its line style names, as musxdom documents.
MappingTarget createSmartShapeCustomLine(
    const musx::dom::DocumentPtr& document, const RecordFamilySource& source, const records::LegacyRow& row, std::uint16_t cmper)
{
    auto instance = createOthersRecordTarget<CustomLine>(document, source, row, cmper);
    if (!instance) {
        return {};
    }
    instance->charParams = std::make_shared<CustomLine::CharParams>(instance);
    instance->solidParams = std::make_shared<CustomLine::SolidParams>(instance);
    instance->dashedParams = std::make_shared<CustomLine::DashedParams>(instance);
    auto* raw = instance.get();
    document->getOthers()->add(CustomLine::XmlNodeName, std::move(instance));
    return makeMappingTarget(row.partId, cmper, raw);
}

/// @brief Drops the parameter blocks the record does not select, and decodes its character.
/// @details The character is a byte in the encoding of the font that draws it, exactly as a run
/// of legacy text is, and that font is named by a field of this same record: it cannot be
/// decoded until both are in hand, which is what makes this a finalizer rather than part of the
/// character's own row. musxdom carries a code point, so a Mac Roman 199 in a text font is the
/// code point 171 and not the number the file stored.
///
/// From Finale 2012 the record stores a code point outright and there is nothing to decode.
/// @ref versions::storesUnicodeCodepoints is the same question the wide table's gate asks, so
/// the two cannot disagree about where that boundary falls.
void ssLineFinalize(void* instance, const SourceProfile& profile, const musx::dom::DocumentPtr& document)
{
    auto* line = static_cast<CustomLine*>(instance);
    if (line->lineStyle != CustomLine::LineStyle::Char) {
        line->charParams.reset();
    }
    if (line->lineStyle != CustomLine::LineStyle::Solid) {
        line->solidParams.reset();
    }
    if (line->lineStyle != CustomLine::LineStyle::Dashed) {
        line->dashedParams.reset();
    }
    if (!line->charParams || versions::storesUnicodeCodepoints(profile.version)) {
        return;
    }
    // A font the document does not define leaves the stored byte alone. Naming a letter on the
    // strength of a font that is not there would be a guess, where the byte is what the file
    // actually holds and stays recoverable.
    line->charParams->lineChar = text::codepointFromByte(
        static_cast<std::uint8_t>(line->charParams->lineChar), document, line->charParams->font->fontId, text::UnresolvedFontFallback::Symbol);
}

// The layout is one fact stated in three addressings, so the row list is written once here
// and each table supplies the row emitters for its own encoding before expanding it. The
// emitters are undefined again after the last table.
//
// Row order carries meaning: a row whose destination exists only for some records tests a
// field of the same record, and the machinery applies rows in this order, so every such row
// follows the row that reads the slot deciding it.
#define SS_LINE_FIELD_LIST                                                                                                                          \
    SS_LINE_ENUM(ssLineStyleSlot, lineStyle, ssLineStyleFromRaw(value)), SS_LINE_CHAR,                                                              \
        SS_LINE_CHAR_WORD(ssLineFontIdSlot, charParams->font->fontId),                                                                              \
        SS_LINE_CHAR_WORD(                                                                                                                          \
            ssLineFontSizeSlot, charParams->font->fontSize),    /* The style mask is decoded by musxdom, which owns what its bits mean. */             \
        SS_LINE_CHAR_SET(ssLineFontEfxSlot, charParams->font, effects, setEnigmaStyles, std::uint16_t),                                             \
        SS_LINE_CHAR_WORD(ssLineBaselineShiftSlot, charParams->baselineShiftEms),                                                                   \
        SS_LINE_WORD_IF(ssLineWidthSlot, &ssLineIsSolid, solidParams->lineWidth),                                                                   \
        SS_LINE_WORD_IF(ssLineWidthSlot, &ssLineIsDashed, dashedParams->lineWidth),                                                                 \
        SS_LINE_WORD_IF(ssLineDashOnSlot, &ssLineIsDashed, dashedParams->dashOn),                                                                   \
        SS_LINE_WORD_IF(ssLineDashOffSlot, &ssLineIsDashed, dashedParams->dashOff),                                                                 \
        SS_LINE_ENUM(ssLineCapStartTypeSlot, lineCapStartType, ssLineCapFromRaw(value)),                                                            \
        SS_LINE_ENUM(ssLineCapEndTypeSlot, lineCapEndType, ssLineCapFromRaw(value)),                                                                \
        SS_LINE_WORD_IF(ssLineCapStartValueSlot, &ssLineStartIsArrowhead, lineCapStartArrowId),                                                     \
        SS_LINE_WORD_IF(ssLineCapStartValueSlot, &ssLineStartIsHook, lineCapStartHookLength),                                                       \
        SS_LINE_WORD_IF(ssLineCapEndValueSlot, &ssLineEndIsArrowhead, lineCapEndArrowId),                                                           \
        SS_LINE_WORD_IF(ssLineCapEndValueSlot, &ssLineEndIsHook, lineCapEndHookLength), SS_LINE_BIT(ssLineFlagsSlot, ssLineMakeHorzBit, makeHorz),  \
        SS_LINE_BIT(ssLineFlagsSlot, ssLineAfterLeftStartTextBit, lineAfterLeftStartText),                                                          \
        SS_LINE_BIT(ssLineFlagsSlot, ssLineBeforeRightEndTextBit, lineBeforeRightEndText),                                                          \
        SS_LINE_BIT(ssLineFlagsSlot, ssLineAfterLeftContTextBit, lineAfterLeftContText), SS_LINE_WORD(ssLineLeftStartTextSlot, leftStartRawTextId), \
        SS_LINE_WORD(ssLineLeftContTextSlot, leftContRawTextId), SS_LINE_WORD(ssLineRightEndTextSlot, rightEndRawTextId),                           \
        SS_LINE_WORD(ssLineCenterFullTextSlot, centerFullRawTextId), SS_LINE_WORD(ssLineCenterAbbrTextSlot, centerAbbrRawTextId),                   \
        SS_LINE_WORD(ssLineLeftStartXSlot, leftStartX), SS_LINE_WORD(ssLineLeftStartYSlot, leftStartY),                                             \
        SS_LINE_WORD(ssLineLeftContXSlot, leftContX), SS_LINE_WORD(ssLineLeftContYSlot, leftContY), SS_LINE_WORD(ssLineRightEndXSlot, rightEndX),   \
        SS_LINE_WORD(ssLineRightEndYSlot, rightEndY), SS_LINE_WORD(ssLineCenterFullXSlot, centerFullX),                                             \
        SS_LINE_WORD(ssLineCenterFullYSlot, centerFullY), SS_LINE_WORD(ssLineCenterAbbrXSlot, centerAbbrX),                                         \
        SS_LINE_WORD(ssLineCenterAbbrYSlot, centerAbbrY), SS_LINE_WORD(ssLineStartXSlot, lineStartX), SS_LINE_WORD(ssLineStartYSlot, lineStartY),   \
        SS_LINE_WORD(ssLineEndXSlot, lineEndX), SS_LINE_WORD(ssLineEndYSlot, lineEndY), SS_LINE_WORD(ssLineContXSlot, lineContX)

// Fixed rows, Finale 2000 through 2006. The character is one word, narrowed to its low byte
// because a source may store it either zero- or sign-extended.
#define SS_LINE_WORD(slot, member) MUS_WORD(CustomLine, SS_LINE_TAG_TEXT, CMPER_FROM_TARGET, 0, ssLineRowWord(slot), member)
#define SS_LINE_WORD_IF(slot, applies, member) MUS_WORD_IF(CustomLine, SS_LINE_TAG_TEXT, CMPER_FROM_TARGET, 0, ssLineRowWord(slot), applies, member)
#define SS_LINE_BIT(slot, bitIndex, member) MUS_BIT(CustomLine, SS_LINE_TAG_TEXT, CMPER_FROM_TARGET, 0, ssLineRowWord(slot), bitIndex, member)
#define SS_LINE_ENUM(slot, member, conversion) \
    MUS_BITS_AS(CustomLine, SS_LINE_TAG_TEXT, CMPER_FROM_TARGET, 0, ssLineRowWord(slot), 0, 0, member, conversion)
#define SS_LINE_CHAR_WORD(slot, member) MUS_WORD_IF(CustomLine, SS_LINE_TAG_TEXT, CMPER_FROM_TARGET, 0, ssLineRowWord(slot), &ssLineIsChar, member)
#define SS_LINE_CHAR_SET(slot, owner, field, setter, Stored) \
    MUS_SET_IF(CustomLine, SS_LINE_TAG_TEXT, CMPER_FROM_TARGET, 0, ssLineRowWord(slot), &ssLineIsChar, owner, field, setter, Stored)
#define SS_LINE_CHAR                                                                                                                       \
    MUS_WORD_AS_IF(CustomLine, SS_LINE_TAG_TEXT, CMPER_FROM_TARGET, 0, ssLineRowWord(ssLineCharSlot), &ssLineIsChar, charParams->lineChar, \
        narrowCodepoint(static_cast<std::int16_t>(value)))

const FieldMapping ssLineFixedRowFields[] = {SS_LINE_FIELD_LIST};

#undef SS_LINE_WORD
#undef SS_LINE_WORD_IF
#undef SS_LINE_BIT
#undef SS_LINE_ENUM
#undef SS_LINE_CHAR_WORD
#undef SS_LINE_CHAR_SET
#undef SS_LINE_CHAR

// Class records, Finale 2007 through 2011: the same word stream addressed by byte, and the
// same one-word character.
#define SS_LINE_WORD(slot, member) MUS_CLASS_WORD(CustomLine, ssLineClass, CMPER_FROM_TARGET, ssLineNarrowByte(slot), member)
#define SS_LINE_WORD_IF(slot, applies, member) MUS_CLASS_WORD_IF(CustomLine, ssLineClass, CMPER_FROM_TARGET, ssLineNarrowByte(slot), applies, member)
#define SS_LINE_BIT(slot, bitIndex, member) MUS_CLASS_BIT(CustomLine, ssLineClass, CMPER_FROM_TARGET, ssLineNarrowByte(slot), bitIndex, member)
#define SS_LINE_ENUM(slot, member, conversion) MUS_CLASS_BITS_AS(CustomLine, ssLineClass, ssLineNarrowByte(slot), 0, 0, member, conversion)
#define SS_LINE_CHAR_WORD(slot, member) MUS_CLASS_WORD_IF(CustomLine, ssLineClass, CMPER_FROM_TARGET, ssLineNarrowByte(slot), &ssLineIsChar, member)
#define SS_LINE_CHAR_SET(slot, owner, field, setter, Stored) \
    MUS_CLASS_SET_IF(CustomLine, ssLineClass, CMPER_FROM_TARGET, ssLineNarrowByte(slot), &ssLineIsChar, owner, field, setter, Stored)
#define SS_LINE_CHAR                                                                                                                        \
    MUS_CLASS_WORD_AS_IF(CustomLine, ssLineClass, CMPER_FROM_TARGET, ssLineNarrowByte(ssLineCharSlot), &ssLineIsChar, charParams->lineChar, \
        narrowCodepoint(static_cast<std::int16_t>(value)))

const FieldMapping ssLineNarrowClassFields[] = {SS_LINE_FIELD_LIST};

#undef SS_LINE_WORD
#undef SS_LINE_WORD_IF
#undef SS_LINE_BIT
#undef SS_LINE_ENUM
#undef SS_LINE_CHAR_WORD
#undef SS_LINE_CHAR_SET
#undef SS_LINE_CHAR

// Class records from Finale 2012, where the character became a 32-bit codepoint across two
// words and shifted everything after it. The low half comes first, which is the same word
// order @ref wideCodepoint states for the passes that read a word stream directly.
#define SS_LINE_WORD(slot, member) MUS_CLASS_WORD(CustomLine, ssLineClass, CMPER_FROM_TARGET, ssLineWideByte(slot), member)
#define SS_LINE_WORD_IF(slot, applies, member) MUS_CLASS_WORD_IF(CustomLine, ssLineClass, CMPER_FROM_TARGET, ssLineWideByte(slot), applies, member)
#define SS_LINE_BIT(slot, bitIndex, member) MUS_CLASS_BIT(CustomLine, ssLineClass, CMPER_FROM_TARGET, ssLineWideByte(slot), bitIndex, member)
#define SS_LINE_ENUM(slot, member, conversion) MUS_CLASS_BITS_AS(CustomLine, ssLineClass, ssLineWideByte(slot), 0, 0, member, conversion)
#define SS_LINE_CHAR_WORD(slot, member) MUS_CLASS_WORD_IF(CustomLine, ssLineClass, CMPER_FROM_TARGET, ssLineWideCharByte(slot), &ssLineIsChar, member)
#define SS_LINE_CHAR_SET(slot, owner, field, setter, Stored) \
    MUS_CLASS_SET_IF(CustomLine, ssLineClass, CMPER_FROM_TARGET, ssLineWideCharByte(slot), &ssLineIsChar, owner, field, setter, Stored)
#define SS_LINE_CHAR                                                                                                                          \
    MUS_CLASS_LONG_IF(CustomLine, ssLineClass, CMPER_FROM_TARGET, ssLineWideCharByte(ssLineCharSlot), LongWordOrder::LowFirst, &ssLineIsChar, \
        charParams->lineChar)

const FieldMapping ssLineWideClassFields[] = {SS_LINE_FIELD_LIST};

#undef SS_LINE_WORD
#undef SS_LINE_WORD_IF
#undef SS_LINE_BIT
#undef SS_LINE_ENUM
#undef SS_LINE_CHAR_WORD
#undef SS_LINE_CHAR_SET
#undef SS_LINE_CHAR
#undef SS_LINE_FIELD_LIST
#undef SS_LINE_TAG_TEXT

// The Coda-banner epoch is deliberately absent from every gate: the record does not exist
// before Finale 2000, which lies inside the uncompressed epoch, so no earlier layout can be
// stated and none is needed.
const MappingTable& ssLineFixedRowTable()
{
    static const MappingTable table{.reportPrefix = ssLineReportPrefix,
        .epochs = EpochMask::FixedRow,
        .targetKind = TargetKind::OthersFromRecords,
        .recordIdentity = ssLineTag,
        .createTarget = &createSmartShapeCustomLine,
        .fields = ssLineFixedRowFields,
        .fieldCount = std::size(ssLineFixedRowFields),
        .finalizeTarget = &ssLineFinalize};
    return table;
}

// Unrestricted rather than bounded above at Finale 2011, because a zlib source whose version
// could not be recovered must still be read, and the narrow character is the safe direction
// for it: @ref versions::storesUnicodeCodepoints makes the same choice for the same reason.
// The wide table below supersedes every row of this one wherever a version is present and
// says Finale 2012.
const MappingTable& ssLineNarrowClassTable()
{
    static const MappingTable table{.reportPrefix = ssLineReportPrefix,
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OthersFromRecords,
        .recordIdentity = ssLineClass,
        .createTarget = &createSmartShapeCustomLine,
        .fields = ssLineNarrowClassFields,
        .fieldCount = std::size(ssLineNarrowClassFields),
        .finalizeTarget = &ssLineFinalize};
    return table;
}

const MappingTable& ssLineWideClassTable()
{
    static constexpr auto sourceHasWideCharacters = [](const SourceProfile& profile) {
        return sourceAtOrAfter(profile, FormatEpoch::ZlibLegacy, versions::finale2012);
    };
    static const MappingTable table{.reportPrefix = ssLineReportPrefix,
        .epochs = EpochMask::Zlib,
        .sourceApplies = sourceHasWideCharacters,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OthersFromRecords,
        .recordIdentity = ssLineClass,
        .createTarget = &createSmartShapeCustomLine,
        .fields = ssLineWideClassFields,
        .fieldCount = std::size(ssLineWideClassFields),
        .finalizeTarget = &ssLineFinalize};
    return table;
}

} // namespace

void importSmartShapeCustomLines(const ImportContext& context)
{
    // These tables construct only the custom lines stored by the source. Smart Shape options
    // independently request the baseline lines required by formats that predate this class;
    // deferred reference resolution adds those after every source-owned pool has been read.
    applyMappingTables({&ssLineFixedRowTable(), &ssLineNarrowClassTable(), &ssLineWideClassTable()}, context.index, context.profile, context.document,
        context.report);

    // Every font comparator this import leaves in the document must be registered, or
    // musxdom supplies no placeholder definition for it and `FontInfo::getName` throws. Only
    // a Char line has a font, and only after the tables have settled which lines those are.
    for (const auto& line : context.document->getOthers()->getArray<CustomLine>(musx::dom::SCORE_PARTID)) {
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            reporting.report().setInstanceOrigin(
                reporting.template instanceKey<CustomLine>(musx::dom::SCORE_PARTID, line->getCmper()), Reporting::Origin::LegacyMus);
        });
        if (line->charParams) {
            line->charParams->font->fontId = context.construction.assignFontId(line->charParams->font->fontId);
        }
    }
}

} // namespace others
} // namespace finale_mus_reader
