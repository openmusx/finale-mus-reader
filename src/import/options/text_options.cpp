// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "import/support/text_encoding.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using TextTarget = musx::dom::options::TextOptions;

// This class is spread across five numeric globals. Two of them are as old as the format and
// three arrive with Finale 97; see @ref storesTextLayoutOptions for why the split needs no
// gate. The accidental symbol inserts are a direct block on a sixth selector, handled by
// @ref captureSymbolInserts rather than by the tables.
constexpr std::uint16_t dateSelector = 5;
constexpr std::uint16_t tabSelector = 13;
constexpr std::uint16_t metricsSelector = 81;
constexpr std::uint16_t layoutSelector = 82;
constexpr std::uint16_t alignSelector = 83;

// Word slots of selector 82. Slot 1 is the line-spacing units and is handled outside the
// tables, because it selects which musxdom member slot 0 populates rather than carrying a
// value of its own.
constexpr std::uint32_t lineSpacingValueSlot = 0;
constexpr std::uint32_t lineSpacingIsPercentSlot = 1;

/// @brief Whether this source stores selector 82 at all.
/// @details Selectors 81, 82 and 83 arrive together with Finale 97, which is the same boundary
/// the multimeasure-rest defaults find for selector 83.
///
/// **The pre-97 epochs are covered and the exclusion is intended.** Reading presence rather
/// than dating the file keeps the boundary out of a version range whose lower end would have
/// to be guessed somewhere between 3.7 and 97, and it means a document from before the
/// selectors existed recovers nothing from them and correctly reports the eleven fields as
/// synthesized Finale 27 defaults. That is the fallback strategy working: the era had no such
/// settings to store: that era's UI exposes only tab spacing and date format, and those two
/// fields are on selectors 5 and 13, which every era carries, so they
/// are recovered from the same words in a Finale 1.0.0 document as in a Finale 2012 one.
///
/// Only the line-spacing pair consults this. The other tables need no gate of their own: an
/// absent record simply resolves no value, and the field is then reported as the synthesized
/// default it already holds. The pair is different because its two tables must stay
/// complementary -- musxdom rejects a TextOptions with both members engaged or neither -- so
/// the absent case has to be named rather than left to fall through both.
bool storesTextLayoutOptions(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    if (profile.epoch == FormatEpoch::ZlibLegacy) {
        return index.getClassOthers().get(numericGlobalClass(layoutSelector), GLOBALS_CMPER, 0, 0) != nullptr;
    }
    return readNumericGlobalWords(index, layoutSelector).present;
}

// Selectors 5 and 13, which every epoch carries. The words are the same in the earliest era as
// in the latest.
//
// `dateFormat` needs no translation: the legacy short, long and Mac-long codes are 0, 1 and 2,
// and musxdom's DateFormat has the same three in the same order.
const FieldMapping textStampFields[] = {
    MUS_WORD(TextTarget, "05", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 4, showTimeSeconds),
    MUS_WORD(TextTarget, "05", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 5, dateFormat),
    MUS_WORD(TextTarget, "13", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 0, tabSpaces),
};

// Selector 81, three 32-bit values as two 16-bit words with the high word first, each word in
// the container's byte order. That is one rule for both byte
// orders: -6 is stored as the word pair (-1, -6) and 42 as (0, 42), whichever order the
// container uses.
const FieldMapping textMetricsFields[] = {
    MUS_LONG(TextTarget, "81", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 0, LongWordOrder::HighFirst, textTracking),
    MUS_LONG(TextTarget, "81", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 2, LongWordOrder::HighFirst, textBaselineShift),
    MUS_LONG(TextTarget, "81", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 4, LongWordOrder::HighFirst, textSuperscript),
};

// Selectors 82 and 83, less the two words that need more than a slot each: word 0 of 82, whose
// destination depends on word 1, and word 1 of 82 itself.
//
// Word 2 of selector 83 is deliberately unmapped. It is 0 before Finale 2007 and 1 from that
// release on, and the text-options dialog clears it, so that dialog owns it -- but it is no
// field of this class. It is the same word the multimeasure-rest note records as set without
// automatic updating being on. It stays **open**.
const FieldMapping textLayoutFields[] = {
    MUS_WORD(TextTarget, "82", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 2, textWordWrap),
    MUS_WORD(TextTarget, "82", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 3, textPageOffset),
    MUS_BITS_AS(TextTarget, "82", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 4,
        /*firstBit*/ 0, /*bitCount*/ 0, textJustify, static_cast<TextTarget::TextJustify>(legacyCenterOppositeOrder(value))),
    MUS_WORD(TextTarget, "82", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 5, textExpandSingleWord),
    MUS_WORD(TextTarget, "83", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 0, textHorzAlign),
    MUS_BITS_AS(TextTarget, "83", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 1,
        /*firstBit*/ 0, /*bitCount*/ 0, textVertAlign, static_cast<TextTarget::VerticalAlignment>(legacyCenterOppositeOrder(value))),
    MUS_WORD(TextTarget, "83", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 3, textIsEdgeAligned),
};

// Finale 2007 and later reach the same words through the shared numericGlobalClass rule,
// addressed by byte offset in the coalesced payload.
const FieldMapping classTextStampFields[] = {
    MUS_CLASS_WORD(TextTarget, numericGlobalClass(dateSelector), GLOBALS_CMPER, classWordOffset(4), showTimeSeconds),
    MUS_CLASS_WORD(TextTarget, numericGlobalClass(dateSelector), GLOBALS_CMPER, classWordOffset(5), dateFormat),
    MUS_CLASS_WORD(TextTarget, numericGlobalClass(tabSelector), GLOBALS_CMPER, classWordOffset(0), tabSpaces),
};

const FieldMapping classTextMetricsFields[] = {
    MUS_CLASS_LONG(TextTarget, numericGlobalClass(metricsSelector), GLOBALS_CMPER, classWordOffset(0), LongWordOrder::HighFirst, textTracking),
    MUS_CLASS_LONG(TextTarget, numericGlobalClass(metricsSelector), GLOBALS_CMPER, classWordOffset(2), LongWordOrder::HighFirst, textBaselineShift),
    MUS_CLASS_LONG(TextTarget, numericGlobalClass(metricsSelector), GLOBALS_CMPER, classWordOffset(4), LongWordOrder::HighFirst, textSuperscript),
};

const FieldMapping classTextLayoutFields[] = {
    MUS_CLASS_WORD(TextTarget, numericGlobalClass(layoutSelector), GLOBALS_CMPER, classWordOffset(2), textWordWrap),
    MUS_CLASS_WORD(TextTarget, numericGlobalClass(layoutSelector), GLOBALS_CMPER, classWordOffset(3), textPageOffset),
    MUS_CLASS_SELECTED_BITS_AS(TextTarget, numericGlobalClass(layoutSelector), GLOBALS_CMPER, classWordOffset(4), /*firstBit*/ 0, /*bitCount*/ 0,
        textJustify, static_cast<TextTarget::TextJustify>(legacyCenterOppositeOrder(value))),
    MUS_CLASS_WORD(TextTarget, numericGlobalClass(layoutSelector), GLOBALS_CMPER, classWordOffset(5), textExpandSingleWord),
    MUS_CLASS_WORD(TextTarget, numericGlobalClass(alignSelector), GLOBALS_CMPER, classWordOffset(0), textHorzAlign),
    MUS_CLASS_SELECTED_BITS_AS(TextTarget, numericGlobalClass(alignSelector), GLOBALS_CMPER, classWordOffset(1), /*firstBit*/ 0, /*bitCount*/ 0,
        textVertAlign, static_cast<TextTarget::VerticalAlignment>(legacyCenterOppositeOrder(value))),
    MUS_CLASS_WORD(TextTarget, numericGlobalClass(alignSelector), GLOBALS_CMPER, classWordOffset(3), textIsEdgeAligned),
};

constexpr const char* textReportPrefix = "options.textOptions";

// Every epoch whose records are 16-byte rows. The Coda-banner epoch is included deliberately:
// it carries selectors 5 and 13, and the presence test keeps it out of the other three.
constexpr EpochMask fixedRowTextEpochs = EpochMask::CodaBanner | EpochMask::FixedRow;

const MappingTable& textStampTable()
{
    static const MappingTable table{.reportPrefix = textReportPrefix,
        .epochs = fixedRowTextEpochs,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TextTarget>,
        .fields = textStampFields,
        .fieldCount = std::size(textStampFields)};
    return table;
}

const MappingTable& textMetricsTable()
{
    static const MappingTable table{.reportPrefix = textReportPrefix,
        .epochs = fixedRowTextEpochs,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TextTarget>,
        .fields = textMetricsFields,
        .fieldCount = std::size(textMetricsFields)};
    return table;
}

const MappingTable& textLayoutTable()
{
    static const MappingTable table{.reportPrefix = textReportPrefix,
        .epochs = fixedRowTextEpochs,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TextTarget>,
        .fields = textLayoutFields,
        .fieldCount = std::size(textLayoutFields)};
    return table;
}

const MappingTable& classTextStampTable()
{
    static const MappingTable table{.reportPrefix = textReportPrefix,
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TextTarget>,
        .fields = classTextStampFields,
        .fieldCount = std::size(classTextStampFields)};
    return table;
}

const MappingTable& classTextMetricsTable()
{
    static const MappingTable table{.reportPrefix = textReportPrefix,
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TextTarget>,
        .fields = classTextMetricsFields,
        .fieldCount = std::size(classTextMetricsFields)};
    return table;
}

const MappingTable& classTextLayoutTable()
{
    static const MappingTable table{.reportPrefix = textReportPrefix,
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TextTarget>,
        .fields = classTextLayoutFields,
        .fieldCount = std::size(classTextLayoutFields)};
    return table;
}

/// @brief Whether selector 82 states its line spacing as a percent rather than a distance.
/// @details Word 1 selects which musxdom member word 0 belongs in: set means percent, clear
/// means an absolute distance. That is why the value needs two tables rather than one row --
/// the destination depends on a second word, not on the value read.
///
/// musxdom has no boolean of its own for the mode -- it carries one member or the other and
/// never both -- so this word is read to route the value and is never itself reported as a
/// recovered field. Percent is what documents overwhelmingly store.
///
/// A source with no selector 82 answers false, which costs nothing: the tables that depend on
/// this one are gated on the same record's presence.
bool statesLineSpacingAsPercent(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    const auto encoding = profile.epoch == FormatEpoch::ZlibLegacy ? RecordEncoding::ClassRecord : RecordEncoding::FixedRow;
    SourceLocation source;
    source.identity = encoding == RecordEncoding::ClassRecord ? numericGlobalClass(layoutSelector) : records::packTag("82");
    source.selector = GLOBALS_CMPER;
    source.incidence = 0;
    source.wordSlot = encoding == RecordEncoding::ClassRecord ? classWordOffset(lineSpacingIsPercentSlot) : lineSpacingIsPercentSlot;
    const auto resolved = readSourceValue(index, encoding, GLOBALS_CMPER, source, profile.byteOrder);
    return resolved && resolved->value != 0;
}

bool statesLineSpacingAsEvpu(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return storesTextLayoutOptions(index, profile) && !statesLineSpacingAsPercent(index, profile);
}

// The two halves of selector 82 word 0. Exactly one table applies to any document that carries
// the selector, and musxdom's own integrity check rejects a TextOptions with both members
// engaged or neither, so the pair must stay complementary.
const FieldMapping lineSpacingPercentFields[] = {
    MUS_WORD(TextTarget, "82", GLOBALS_CMPER, /*incidence*/ 0, lineSpacingValueSlot, textLineSpacingPercent),
};

const FieldMapping lineSpacingEvpuFields[] = {
    MUS_WORD(TextTarget, "82", GLOBALS_CMPER, /*incidence*/ 0, lineSpacingValueSlot, textLineSpacingEvpu),
};

const FieldMapping classLineSpacingPercentFields[] = {
    MUS_CLASS_WORD(TextTarget, numericGlobalClass(layoutSelector), GLOBALS_CMPER, classWordOffset(lineSpacingValueSlot), textLineSpacingPercent),
};

const FieldMapping classLineSpacingEvpuFields[] = {
    MUS_CLASS_WORD(TextTarget, numericGlobalClass(layoutSelector), GLOBALS_CMPER, classWordOffset(lineSpacingValueSlot), textLineSpacingEvpu),
};

const MappingTable& lineSpacingPercentTable()
{
    static const MappingTable table{.reportPrefix = textReportPrefix,
        .epochs = fixedRowTextEpochs,
        .applies = &statesLineSpacingAsPercent,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TextTarget>,
        .fields = lineSpacingPercentFields,
        .fieldCount = std::size(lineSpacingPercentFields)};
    return table;
}

const MappingTable& lineSpacingEvpuTable()
{
    static const MappingTable table{.reportPrefix = textReportPrefix,
        .epochs = fixedRowTextEpochs,
        .applies = &statesLineSpacingAsEvpu,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TextTarget>,
        .fields = lineSpacingEvpuFields,
        .fieldCount = std::size(lineSpacingEvpuFields)};
    return table;
}

const MappingTable& classLineSpacingPercentTable()
{
    static const MappingTable table{.reportPrefix = textReportPrefix,
        .epochs = EpochMask::Zlib,
        .applies = &statesLineSpacingAsPercent,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TextTarget>,
        .fields = classLineSpacingPercentFields,
        .fieldCount = std::size(classLineSpacingPercentFields)};
    return table;
}

const MappingTable& classLineSpacingEvpuTable()
{
    static const MappingTable table{.reportPrefix = textReportPrefix,
        .epochs = EpochMask::Zlib,
        .applies = &statesLineSpacingAsEvpu,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<TextTarget>,
        .fields = classLineSpacingEvpuFields,
        .fieldCount = std::size(classLineSpacingEvpuFields)};
    return table;
}

/// @brief Clears the baseline's line spacing before either table engages the right member.
/// @details The pinned baseline seeds the percent spelling. A source that states the absolute
/// one would otherwise leave that percent engaged beside it, which is exactly the shape
/// musxdom's `TextOptions::integrityCheck` rejects. Runs only when the source has the record
/// to replace it with, so a document from before Finale 97 keeps the baseline value.
void clearSeededLineSpacing(const records::LegacyRecordIndex& index, const SourceProfile& profile, const musx::dom::DocumentPtr& document)
{
    if (!storesTextLayoutOptions(index, profile)) {
        return;
    }
    const auto pooled = document->getOptions()->get<TextTarget>();
    if (!pooled) {
        return;
    }
    // Pool instances are handed out const. Overlaying legacy values is the one reason this
    // library writes to them.
    auto target = std::const_pointer_cast<TextTarget>(pooled);
    target->textLineSpacingPercent.reset();
    target->textLineSpacingEvpu.reset();
}

// ---------------------------------------------------------------------------------------
// Accidental symbol inserts
// ---------------------------------------------------------------------------------------

// The five inserts are a direct block rather than a field map: one element per accidental, in
// musxdom's own AccidentalInsertSymbolType order. The block is at selector 78 in the fixed-row
// epochs and at the class id the numericGlobalClass rule derives from it in the zlib epoch.
constexpr std::uint16_t insertSelector = 78;

// Element field offsets, which are the same in every layout. Only the width of the character
// changes, and with it the element size.
constexpr std::size_t insertTrackingBeforeOffset = 0;
constexpr std::size_t insertTrackingAfterOffset = 4;
constexpr std::size_t insertBaselineShiftOffset = 8;
constexpr std::size_t insertFontIdOffset = 10;
constexpr std::size_t insertFontSizeOffset = 12;
constexpr std::size_t insertFontEffectsOffset = 14;
constexpr std::size_t insertSymCharOffset = 16;

/// @brief Which physical shape of the insert block a source carries.
enum class InsertLayout {
    /// @brief Finale 3.7-2000. 17 bytes, a one-byte character, little-endian throughout.
    EarlyByteChar,
    /// @brief Finale 2001-2010. 18 bytes, a two-byte character of which only the low byte counts.
    NarrowChar,
    /// @brief Finale 2012. 20 bytes, the character widened to a long for Unicode.
    WideChar,
};

[[nodiscard]] constexpr std::size_t elementSize(InsertLayout layout)
{
    switch (layout) {
    case InsertLayout::EarlyByteChar: return 17;
    case InsertLayout::NarrowChar: return 18;
    case InsertLayout::WideChar: return 20;
    }
    return 0;
}

// The order musxdom names them in, which is also the order they are stored in.
constexpr musx::dom::options::AccidentalInsertSymbolType insertOrder[] = {
    musx::dom::options::AccidentalInsertSymbolType::Sharp,
    musx::dom::options::AccidentalInsertSymbolType::Flat,
    musx::dom::options::AccidentalInsertSymbolType::Natural,
    musx::dom::options::AccidentalInsertSymbolType::DblSharp,
    musx::dom::options::AccidentalInsertSymbolType::DblFlat,
};

constexpr std::size_t insertCount = std::size(insertOrder);

const char* const insertNames[insertCount] = {"sharp", "flat", "natural", "dblSharp", "dblFlat"};

/// @brief The insert block's payload bytes, in the order its own struct is laid out.
struct InsertBlock
{
    bool present{};
    std::vector<std::uint8_t> bytes;
    InsertLayout layout = InsertLayout::NarrowChar;
    std::size_t blockOffset{};
    std::size_t decodedOffset{};
};

/// @brief Serializes a fixed-row word stream back to the bytes the file holds.
/// @details The row decoder hands out words rather than bytes, and this block is a byte
/// structure with an odd-sized element, so the bytes have to be reassembled to address it.
void appendWords(std::vector<std::uint8_t>& bytes, const std::vector<std::int16_t>& words, ByteOrder order)
{
    for (const auto word : words) {
        const auto raw = static_cast<std::uint16_t>(word);
        if (order == ByteOrder::BigEndian) {
            bytes.push_back(static_cast<std::uint8_t>(raw >> 8U));
            bytes.push_back(static_cast<std::uint8_t>(raw));
        } else {
            bytes.push_back(static_cast<std::uint8_t>(raw));
            bytes.push_back(static_cast<std::uint8_t>(raw >> 8U));
        }
    }
}

/// @brief Locates the insert block and decides which layout the source carries.
/// @details The layout follows from the epoch and, inside the zlib epoch, from the payload's
/// own length, so no version gate is needed anywhere.
///
/// **The Coda-banner epoch carries no such block and the omission is intended.** Selector 78
/// does not exist there and the era's UI exposes no accidental-insert settings. Finale 3.0-3.5
/// is the same case for a different reason: the record has not appeared yet. Both keep the
/// pinned baseline's five inserts, which is what the fallback strategy is for. Exactly where
/// between 3.5 and 3.7 the block first appears is not established.
///
/// **Finale 3.7-2000 is a byte structure read little-endian**, whatever the container says.
/// Its element is 17 bytes because the character is a single byte, and an odd stride cannot be
/// addressed by 16-bit words at all: the fields land at alternating parities. Reassembling the
/// row words little-endian recovers the structure on a big-endian file and is a no-op on a
/// little-endian one, so one rule serves both. Every document of the era seen so far is
/// big-endian, so whether the rule is "opposite to the container" or "always little-endian"
/// cannot be told apart; a Windows Finale 3.x-2000 document would be needed to separate them.
///
/// **Finale 27 mis-converts this era, and the disagreement is deliberate.** Its upgrade uses
/// the right 17-byte stride but reads the fields as though the element were the later 18-byte
/// one, reporting the sharp insert's tracking as 2293760 -- the bytes 00 23 00 00 read as a
/// big-endian long -- and its character as 50, which is the first byte of the next element.
/// Read at the era's own stride the same records give the values every other era stores. A
/// comparison against Finale 27's conversion will therefore show this era disagreeing on nearly
/// every field, and that is the intended result.
///
/// A document that was converted by that upgrade and re-saved carries the mis-converted values
/// permanently. Those are reproduced exactly, because there the bytes really do say that.
InsertBlock readInsertBlock(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    InsertBlock result;
    if (profile.epoch == FormatEpoch::ZlibLegacy) {
        const auto* row = index.getClassOthers().get(numericGlobalClass(insertSelector), GLOBALS_CMPER, 0, 0);
        if (!row) {
            return result;
        }
        const auto payload = index.getClassOthers().effectivePayloadOf(*row);
        result.present = true;
        result.bytes.assign(payload.begin(), payload.end());
        // The payload's own length states the layout: Finale 2007-2010 write 96 bytes and
        // Finale 2012 writes 108. No other length occurs.
        result.layout = result.bytes.size() >= insertCount * elementSize(InsertLayout::WideChar) ? InsertLayout::WideChar : InsertLayout::NarrowChar;
        result.blockOffset = row->blockOffset;
        result.decodedOffset = row->decodedOffset;
        return result;
    }
    const auto family = readNumericGlobalWords(index, insertSelector);
    if (!family.present) {
        return result;
    }
    result.present = true;
    result.layout = profile.epoch == FormatEpoch::DclLegacy ? InsertLayout::NarrowChar : InsertLayout::EarlyByteChar;
    appendWords(result.bytes, family.words, result.layout == InsertLayout::EarlyByteChar ? ByteOrder::LittleEndian : profile.byteOrder);
    result.blockOffset = family.blockOffset;
    result.decodedOffset = family.decodedOffset;
    return result;
}

/// @brief Reads one unsigned value of the given width from an element.
[[nodiscard]] std::uint32_t readInsertField(const InsertBlock& block, std::size_t elementBase, std::size_t offset, std::size_t width, ByteOrder order)
{
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width; ++i) {
        const auto byte = static_cast<std::uint32_t>(block.bytes[elementBase + offset + i]);
        // The early layout is little-endian throughout; the later two follow the container,
        // except that a 32-bit value is two 16-bit words with the high word first.
        const bool littleEndian = block.layout == InsertLayout::EarlyByteChar || order == ByteOrder::LittleEndian;
        value |= byte << (8U * (littleEndian ? i : width - 1 - i));
    }
    return value;
}

/// @brief Reads a 32-bit field stored as two 16-bit words, high word first.
/// @details One rule for both byte
/// orders: a big-endian Finale 2005 file stores 1000 as 00 00 03 e8 and a little-endian Finale
/// 2012 file stores it as 00 00 e8 03. The early layout is a plain little-endian long instead,
/// which reads the same way once its bytes have been reassembled.
[[nodiscard]] std::int32_t readInsertLong(const InsertBlock& block, std::size_t elementBase, std::size_t offset, ByteOrder order)
{
    if (block.layout == InsertLayout::EarlyByteChar) {
        return static_cast<std::int32_t>(readInsertField(block, elementBase, offset, 4, order));
    }
    const auto high = readInsertField(block, elementBase, offset, 2, order);
    const auto low = readInsertField(block, elementBase, offset + 2, 2, order);
    return static_cast<std::int32_t>((high << 16U) | low);
}

/// @brief Reads the symbol character, whose width is what separates the three layouts.
/// @details Before Finale 2012 the character is a byte. The 18-byte layout gives it a whole
/// word, and some documents of that era sign-extend it -- `ff dc` for 220 where others of the
/// same version store `00 dc` -- so only characters above 127 are affected. The low byte is
/// what counts. Finale 2012 widened the field to a long for Unicode and it must not be masked;
/// it reads as a plain little-endian long. **Unverified:** for a value above 0xFFFF, whether
/// that is a long or a low-word-first pair.
[[nodiscard]] char32_t readInsertChar(const InsertBlock& block, std::size_t elementBase, ByteOrder order)
{
    switch (block.layout) {
    case InsertLayout::EarlyByteChar: return static_cast<char32_t>(readInsertField(block, elementBase, insertSymCharOffset, 1, order));
    case InsertLayout::NarrowChar: return static_cast<char32_t>(readInsertField(block, elementBase, insertSymCharOffset, 2, order) & 0xFFU);
    case InsertLayout::WideChar: break;
    }
    const auto low = readInsertField(block, elementBase, insertSymCharOffset, 2, order);
    const auto high = readInsertField(block, elementBase, insertSymCharOffset + 2, 2, order);
    return static_cast<char32_t>((high << 16U) | low);
}

template <typename Reporting>
void reportInsertField(Reporting& reporting, const char* insertName, const char* member, typename Reporting::Origin origin, std::int64_t rawValue,
    const InsertBlock& block)
{
    typename Reporting::FieldInfo info{origin, 0, 0, rawValue};
    if (origin == Reporting::Origin::LegacyMus) {
        info.blockOffset = block.blockOffset;
        info.decodedOffset = block.decodedOffset;
    }
    reporting.report().setField(
        reporting.template instanceKey<TextTarget>(), "symbolInserts[" + std::string(insertName) + "]." + member, std::move(info));
}

/// @brief Rebuilds the five accidental inserts from the source's own block.
/// @details Every element is replaced rather than overlaid, because the block states all five
/// together: a source that carries it has its own values for each, and leaving a baseline
/// element in place beside four recovered ones would mix two documents' settings.
///
/// The font comparator is the source's own, and font definitions are recovered before any
/// options class, so it needs no remapping. A nonzero comparator the source does not define is
/// kept exactly as stored and noted at Info, the same policy the multimeasure-rest H-bar shape
/// uses: substituting the baseline's font would name a different document's typeface, and
/// ordinary documents commonly name resources their own file never carries. The note
/// asks the document whether it defines the comparator rather than testing the comparator
/// against zero: what zero means is musxdom's business, and a document that defines it has
/// nothing to report.
bool captureSymbolInserts(const records::LegacyRecordIndex& index, const SourceProfile& profile, const musx::dom::DocumentPtr& document,
    ImportReport& report, musx::factory::ConstructionContext& construction)
{
    const auto block = readInsertBlock(index, profile);
    if (!block.present) {
        return false;
    }
    const auto pooled = document->getOptions()->get<TextTarget>();
    if (!pooled) {
        return false;
    }
    const auto stride = elementSize(block.layout);
    if (block.bytes.size() < insertCount * stride) {
        report.diagnostics.push_back(
            {musx::util::Logger::LogLevel::Warning, "The text options symbol-insert block holds " + std::to_string(block.bytes.size())
                                                        + " bytes, too few for five elements; the Finale 27 defaults are kept."});
        return false;
    }
    // Pool instances are handed out const. Overlaying legacy values is the one reason this
    // library writes to them.
    auto target = std::const_pointer_cast<TextTarget>(pooled);

    for (std::size_t ordinal = 0; ordinal < insertCount; ++ordinal) {
        const auto base = ordinal * stride;
        const auto* name = insertNames[ordinal];
        auto insert = std::make_shared<TextTarget::InsertSymbolInfo>(pooled);
        insert->trackingBefore = readInsertLong(block, base, insertTrackingBeforeOffset, profile.byteOrder);
        insert->trackingAfter = readInsertLong(block, base, insertTrackingAfterOffset, profile.byteOrder);
        insert->baselineShiftPerc = static_cast<std::int16_t>(readInsertField(block, base, insertBaselineShiftOffset, 2, profile.byteOrder));

        // The font is the ordinary Enigma tuple: comparator, size, effects bitmask. musxdom
        // owns the bitmask, and it drops the same two bits Finale 27 drops -- a stored 56
        // becomes strikeout alone in both -- so the mask is handed to it whole rather than
        // decoded here. The size is a percent of the preceding font size in an Enigma string,
        // which is what the second constructor argument states.
        const auto fontId = static_cast<musx::dom::Cmper>(readInsertField(block, base, insertFontIdOffset, 2, profile.byteOrder));
        const auto fontSize = static_cast<std::int16_t>(readInsertField(block, base, insertFontSizeOffset, 2, profile.byteOrder));
        const auto effects = static_cast<std::uint16_t>(readInsertField(block, base, insertFontEffectsOffset, 2, profile.byteOrder));
        auto font = std::make_shared<musx::dom::FontInfo>(document, /*sizeIsPercent*/ true);
        font->fontId = construction.assignFontId(fontId);
        font->fontSize = fontSize;
        font->setEnigmaStyles(effects);
        insert->symFont = std::move(font);

        // The character is a byte in the encoding of the font just built, not a code point,
        // and is decoded by the same rule that decodes a run of legacy text. An insert left at
        // its default names a music font, whose byte is a glyph number and comes through
        // unchanged; one pointed at a text font is decoded through that font's code page.
        const auto storedChar = readInsertChar(block, base, profile.byteOrder);
        insert->symChar = block.layout == InsertLayout::WideChar ? storedChar
                                                                 : text::codepointFromByte(static_cast<std::uint8_t>(storedChar), document, fontId,
                                                                       text::UnresolvedFontFallback::Symbol);

        target->symbolInserts[insertOrder[ordinal]] = std::move(insert);

        withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
            reportInsertField(
                reporting, name, "trackingBefore", Reporting::Origin::LegacyMus, target->symbolInserts[insertOrder[ordinal]]->trackingBefore, block);
            reportInsertField(
                reporting, name, "trackingAfter", Reporting::Origin::LegacyMus, target->symbolInserts[insertOrder[ordinal]]->trackingAfter, block);
            reportInsertField(reporting, name, "baselineShiftPerc", Reporting::Origin::LegacyMus,
                target->symbolInserts[insertOrder[ordinal]]->baselineShiftPerc, block);
            reportInsertField(reporting, name, "symFont.fontId", Reporting::Origin::LegacyMus, fontId, block);
            reportInsertField(reporting, name, "symFont.fontSize", Reporting::Origin::LegacyMus, fontSize, block);
            reportInsertField(reporting, name, "symFont.effects", Reporting::Origin::LegacyMus, effects, block);
        });
        // The byte the source stored, not the code point it decoded to, matching how the
        // clef and stem-connection reports name the same kind of value.
        withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
            reportInsertField(reporting, name, "symChar", Reporting::Origin::LegacyMus, static_cast<std::int64_t>(storedChar), block);
        });
    }
    return true;
}

/// @brief Reports the baseline's own inserts where the source carries no block of its own.
/// @details The options pool is seeded from the pinned baseline, so a document with no
/// selector 78 already holds five inserts before this pass runs -- the Coda-banner era and
/// Finale 3.0-3.5, which store none. Nothing needs copying; what needs doing is
/// saying so, and repairing the one thing seeding cannot get right.
///
/// **A seeded font comparator belongs to the baseline's numbering, not to this document's.**
/// The two documents number their font definitions independently, so a comparator carried
/// across unchanged would name whatever font happens to sit at that number here. musxdom owns
/// the rule for fixing it -- match by normalized name, treat zero as the default music font,
/// and add a definition only when no name matches -- and `importFontDefinitionInto` is its one
/// implementation, the same one `FontOptions` calls. It is called for every comparator
/// including zero: zero is the default-music-font sentinel, and the helper both returns it
/// unchanged and guarantees a definition exists at zero in this document, which is exactly the
/// part a caller that skipped the call for zero would leave undone.
void reportSeededSymbolInserts(const musx::dom::DocumentPtr& document, const musx::dom::DocumentPtr& referenceDocument, ImportReport& report,
    musx::factory::ConstructionContext& construction)
{
    const auto pooled = document->getOptions()->get<TextTarget>();
    if (!pooled) {
        return;
    }
    auto target = std::const_pointer_cast<TextTarget>(pooled);

    for (std::size_t ordinal = 0; ordinal < insertCount; ++ordinal) {
        const auto type = insertOrder[ordinal];
        const auto found = target->symbolInserts.find(type);
        if (found == target->symbolInserts.end() || !found->second) {
            continue;
        }
        auto& insert = *found->second;
        const auto* name = insertNames[ordinal];

        musx::dom::Cmper fontId = 0;
        if (insert.symFont) {
            const auto referenceFont =
                referenceDocument->getOthers()->get<musx::dom::others::FontDefinition>(musx::dom::SCORE_PARTID, insert.symFont->fontId);
            if (!referenceFont) {
                report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Warning, "The Finale 27 " + std::string(name)
                                                                + " insert names a font definition the baseline does not carry;"
                                                                  " substituted font id 0."});
            } else if (const auto resolved = musx::dom::importFontDefinitionInto(document, referenceFont, baselineObjectReporter(report))) {
                fontId = *resolved;
            } else {
                report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning,
                    "No free font comparator remained for Finale 27 font \"" + referenceFont->name + "\"; substituted font id 0."});
            }
            insert.symFont->fontId = construction.assignFontId(fontId);
        }

        withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
            const InsertBlock absent;
            reportInsertField(reporting, name, "trackingBefore", Reporting::Origin::Finale27Default, insert.trackingBefore, absent);
            reportInsertField(reporting, name, "trackingAfter", Reporting::Origin::Finale27Default, insert.trackingAfter, absent);
            reportInsertField(reporting, name, "baselineShiftPerc", Reporting::Origin::Finale27Default, insert.baselineShiftPerc, absent);
            reportInsertField(reporting, name, "symFont.fontId", Reporting::Origin::Finale27Default, fontId, absent);
            reportInsertField(
                reporting, name, "symFont.fontSize", Reporting::Origin::Finale27Default, insert.symFont ? insert.symFont->fontSize : 0, absent);
            reportInsertField(reporting, name, "symFont.effects", Reporting::Origin::Finale27Default,
                insert.symFont ? insert.symFont->getEnigmaStyles() : 0, absent);
            reportInsertField(reporting, name, "symChar", Reporting::Origin::Finale27Default, static_cast<std::int64_t>(insert.symChar), absent);
        });
    }
}

} // namespace

void importTextOptions(const ImportContext& context)
{
    clearSeededLineSpacing(context.index, context.profile, context.document);
    // The options pool is already seeded from the pinned baseline, so the inserts exist
    // either way. A source that carries the block replaces all five together; one that does
    // not keeps the baseline's, which then need their font comparators translated into this
    // document's numbering and reporting as the synthesized defaults they are.
    const bool recovered = captureSymbolInserts(context.index, context.profile, context.document, context.report, context.construction);
    applyMappingTables(
        {&textStampTable(), &textMetricsTable(), &textLayoutTable(), &lineSpacingPercentTable(), &lineSpacingEvpuTable(), &classTextStampTable(),
            &classTextMetricsTable(), &classTextLayoutTable(), &classLineSpacingPercentTable(), &classLineSpacingEvpuTable()},
        context.index, context.profile, context.document, context.report);

    if (!recovered) {
        reportSeededSymbolInserts(context.document, context.referenceDocument, context.report, context.construction);
    }
}

} // namespace options
} // namespace finale_mus_reader
