// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>

#include "import/support/text_encoding.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using LyricTarget = musx::dom::options::LyricOptions;
using LyricAlignJustify = musx::dom::AlignJustify;
using LyricSyllableStyle = LyricTarget::SyllablePosStyle;
using LyricSyllableType = LyricTarget::SyllablePosStyleType;
using LyricConnectIndex = LyricTarget::WordExtConnectIndex;
using LyricConnectStyle = LyricTarget::WordExtConnectStyle;
using LyricConnectType = LyricTarget::WordExtConnectStyleType;

// The lyric options are spread over six numeric globals rather than gathered into one record.
// Each is an ordinary numeric global, so through
// Finale 2006 its identity is the two decimal characters of the selector and from Finale 2007
// it is the class id the shared numericGlobalClass rule derives.
constexpr std::uint16_t hyphenSeparationSelector = 15;
constexpr std::uint32_t hyphenSeparationSlot = 1;

constexpr std::uint16_t smartWordExtSelector = 34;
constexpr std::uint32_t smartWordExtSlot = 5;

constexpr std::uint16_t smartHyphenSelector = 35;
constexpr std::uint32_t smartHyphenSlot = 5;

constexpr std::uint16_t wordExtConnectSelector = 55;

// The Lyric Options dialog's word-extension "Lift" and "Push", which musxdom spells as
// wordExtVertOffset and wordExtHorzOffset. Each has a numeric global of its own and word 5 of
// it, in every epoch including the Coda banner -- so unlike the rest of this class they need no
// gate at all beyond the record being present.
constexpr std::uint16_t wordExtLiftSelector = 29;
constexpr std::uint16_t wordExtPushSelector = 30;
constexpr std::uint32_t wordExtOffsetSlot = 5;

constexpr std::uint16_t wordExtSelector = 57;
constexpr std::uint32_t smartHyphenStartSlot = 0;
constexpr std::uint32_t wordExtNeedUnderscoreSlot = 1;
constexpr std::uint32_t wordExtMinLengthSlot = 2;
constexpr std::uint32_t wordExtOffsetToNoteheadSlot = 3;
constexpr std::uint32_t edgePunctuationSlot = 4;
/// @brief The first word of selector 57's variable-length tail.
/// @details The record is six words when the document keeps the stock ignore list and grows to
/// hold a custom one, so the tail begins immediately after the six scalars.
constexpr std::size_t punctuationTailWord = 6;

constexpr std::uint16_t wordExtLineWidthSelector = 67;
constexpr std::uint32_t wordExtLineWidthSlot = 5;

constexpr std::uint16_t syllablePosSelector = 87;

// Automatic lyric numbering, which arrives with Finale 2011 and lives in a record that grew to
// hold it rather than in one of its own.
constexpr std::uint16_t autoNumberSelector = 58;
constexpr std::uint32_t autoNumberVersesSlot = 6;
constexpr std::uint32_t autoNumberChorusesSlot = 7;
constexpr std::uint32_t autoNumberSectionsSlot = 8;
constexpr std::uint32_t autoNumberTypeSlot = 9;
/// @brief The word count of selector 58 before automatic numbering was added to it.
constexpr std::size_t autoNumberFamilyWords = 6;

/// @brief The word-extension line width of an era that has no record for it.
/// @details The eras with no record for this width behave as though it were 224. The pinned
/// Finale 27 baseline carries 115, which is one modern document's setting rather than anything
/// the older eras meant, so it is not the value to fall back to.
constexpr musx::dom::Efix unstatedWordExtLineWidth = 224;

/// @brief The first Enigma major version that stores the syllable edge punctuation setting.
/// @details **Finale 2011, not Finale 2012.** MakeMusic's manuals bracket it: the Finale 2010
/// Document Options-Lyrics dialog has neither "Ignore Syllable Edge Punctuation" nor a
/// "Punctuation to Ignore" field, the Finale 2011 dialog has both, and the Finale 2012 manual's
/// "Finale 2011 Interface Changes" page says the feature arrived in 2011 outright.
///
/// This is deliberately its own constant rather than a shared "Finale 2012" one: the Unicode
/// codepoint widening and the font-ordinal renumbering both fall at major 17, and this does
/// not.
/// @brief The first Enigma major version whose punctuation tail is verified as UTF-16.
/// @details The switch itself arrives one release earlier, at
/// Finale 2011, but Finale 2012 is the Unicode release. A tail
/// written by the intervening release would be 8-bit text of an undetermined code page, packed
/// either one or two characters to a word. Reading one through the Unicode rule would produce
/// mojibake for any non-ASCII punctuation, so such a tail is declined and reported rather than
/// decoded. See @ref captureLyricOptions.
constexpr const char* lyricReportPrefix = "options.lyricOptions";

// Every epoch whose records are 16-byte rows. Which of them can actually answer a given
// selector is left to that selector's own gate, because these six arrive at four different
// releases and none of the boundaries is the epoch boundary.
constexpr EpochMask fixedRowLyricEpochs = EpochMask::CodaBanner | EpochMask::FixedRow;

/// @brief Translates the legacy lyric alignment numbering into musxdom's @c AlignJustify.
/// @details The two orders disagree and neither is a rotation of the other: musxdom follows
/// Finale's general `Left, Right, Center` order from zero, while the lyric records number
/// their own list `1 = center, 2 = left, 3 = right`. 1 and 2 are established for all four
/// positions. **Believed: 3 is `right`.**
///
/// Zero is not a member of the legacy list. A record storing it says nothing translatable, so
/// the seeded default is kept rather than a fourth meaning invented for it.
std::optional<LyricAlignJustify> lyricAlignment(std::int16_t stored)
{
    switch (stored) {
    case 1: return LyricAlignJustify::Center;
    case 2: return LyricAlignJustify::Left;
    case 3: return LyricAlignJustify::Right;
    default: return std::nullopt;
    }
}

// Bit 15 of each position's flags word is "use this positioning", which musxdom spells as the
// style's own `on`. It applies to the three optional positions; the fourth word is a
// placeholder with no UI behind it.
constexpr std::uint8_t syllablePositionOnBit = 15;

// The four positions in the order the record stores them, which is musxdom's own enum order.
// The first is the fallback that always applies, so its stored flag decides nothing; musxdom
// documents `on` as meaningless for it.
constexpr LyricSyllableType syllablePositionOrder[] = {
    LyricSyllableType::Default,
    LyricSyllableType::WordExt,
    LyricSyllableType::First,
    LyricSyllableType::SystemStart,
};

constexpr const char* syllablePositionNames[] = {"default", "wordExt", "first", "systemStart"};

constexpr std::size_t syllablePositionWords = 3;

/// @brief The connection points a word extension can attach to, in the legacy numbering.
/// @details The legacy numbers are not this list's indices: they continue a wider entry
/// connection numbering that begins at note and stem attachments, and its lyric run starts at
/// 0x10. Two of the six are also in a different order from musxdom's enum, so the value cannot
/// be cast and has to be translated through this table.
///
/// All six numbers are established. Note that Finale's smart-shape entry connection enum,
/// which shares the numbering, has no entry for the dotted attachment and so places
/// `duration` one earlier than the lyric records do; the records govern here.
constexpr LyricConnectIndex legacyConnectIndexOrder[] = {
    LyricConnectIndex::LyricRightBottom,
    LyricConnectIndex::HeadRightLyrBaseline,
    LyricConnectIndex::DotRightLyrBaseline,
    LyricConnectIndex::DurationLyrBaseline,
    LyricConnectIndex::SystemLeft,
    LyricConnectIndex::SystemRight,
};

/// @brief The legacy number of the first lyric connection point.
constexpr std::int16_t firstLegacyConnectIndex = 0x10;

// The nine connection styles in the order the record stores them, which is musxdom's own enum
// order. Every one is present in every document that carries the table.
constexpr LyricConnectType wordExtConnectOrder[] = {
    LyricConnectType::DefaultStart,
    LyricConnectType::DefaultEnd,
    LyricConnectType::SystemStart,
    LyricConnectType::SystemEnd,
    LyricConnectType::DottedEnd,
    LyricConnectType::DurationEnd,
    LyricConnectType::OneEntryEnd,
    LyricConnectType::ZeroLengthEnd,
    LyricConnectType::ZeroOffset,
};

constexpr const char* wordExtConnectNames[] = {
    "defaultStart", "defaultEnd", "systemStart", "systemEnd", "dottedEnd", "durationEnd", "oneEntryEnd", "zeroLengthEnd", "zeroOffset"};

// Each connection style is three words: the connection point, then the two offsets.
constexpr std::size_t wordExtConnectWords = 3;
constexpr std::size_t wordExtConnectCount = std::size(wordExtConnectOrder);
constexpr std::size_t earlyWordExtConnectCount = wordExtConnectCount - 1;

/// @brief Whether this source stores the Finale 2004 generation of smart-lyric settings.
/// @details Selector 57 is the marker for the whole group, including the two switches that live
/// on its neighbors: smart hyphens on selector 35 word 5 and smart word extensions on selector
/// 34 word 5. Both selectors exist in every era, but the words mean nothing before Finale 2004:
/// they are zero in every document that predates the option, which arrives switched on. Reading
/// them on such a document would turn the option off on the strength of a word that meant
/// nothing yet. One earlier era stores 12 in selector 34 word 5, which is no boolean at all.
///
/// The pinned baseline already switches smart hyphens on, so the earlier era needs no assertion
/// of its own here -- only the gate.
bool storesSmartLyricOptions(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return readGlobalWords(index, profile, wordExtSelector).present;
}

/// @brief Whether this source stores the automatic lyric numbering fields.
/// @details Automatic numbering arrives with **Finale 2011** -- the Finale 2010 lyric dialog
/// has no such option and the Finale 2011 one does -- but it needs no version gate, because
/// **the record states its own layout**. Selector 58 is six words in every era before it and
/// twelve from Finale 2011 on, with the four new fields at words 6 to 9.
///
/// A shape marker is preferable to a version range wherever the record offers one: nothing then
/// has to be inferred about which release a file came from. The edge-punctuation gate a few
/// lines up has to be a version range only because its record does not change shape.
bool storesAutoNumbering(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    const auto family = readGlobalWords(index, profile, autoNumberSelector);
    return family.present && family.words.size() > autoNumberFamilyWords;
}

/// @brief Whether this source stores the word-extension line width.
/// @details Selector 67 is carried by the uncompressed, DCL and zlib epochs and by no
/// Coda-banner document. Its presence decides, because @ref captureLyricOptions asserts a value
/// when it is absent and the two must not be able to disagree.
bool storesWordExtLineWidth(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return readGlobalWords(index, profile, wordExtLineWidthSelector).present;
}

/// @brief Whether this source's era stores the syllable-edge punctuation setting.
/// @details The setting arrives with Finale 2011.
///
/// The word exists for seven releases before it means anything. Documents from before Finale
/// 2004 carry no selector 57 at all, and those from Finale 2004 through Finale 2010 carry it
/// with word 4 clear while behaving as though edge punctuation is *not* ignored. Reading the
/// word on such a document would switch the behavior off against what the era actually did.
/// That is what the gate is for. The pinned baseline behaves the same way, so an earlier
/// document left to the baseline would claim a behavior its era did not have; the assertion is
/// in @ref captureLyricOptions. From Finale 2011 the word decides.
///
/// This is a version gate because nothing structural distinguishes the two cases: the record is
/// twelve bytes in Finale 2007 and in Finale 2011 alike, so its shape says nothing and only the
/// release does. The gate is framed inside the zlib epoch and bounded below, so a file whose
/// version cannot be recovered reads as the earlier behavior -- the era every other epoch is in
/// anyway, so it fails closed onto the answer that is right for all but two releases.
bool storesEdgePunctuationSetting(const records::LegacyRecordIndex&, const SourceProfile& profile)
{
    return sourceAtOrAfter(profile, FormatEpoch::ZlibLegacy, versions::finale2011);
}

// The maximum hyphen separation, in the two epochs whose selector 15 states it.
//
// The Coda-banner epoch is deliberately excluded. Those documents carry a selector 15, but its
// word 1 is always zero and the rest of the record differs from the later one: the field is not
// there to be read, and reading the zero would replace a correct default with a hyphen
// separation of nothing. The baseline's 144 is the era's behavior, so it needs no assertion
// either.
const FieldMapping lyricHyphenFields[] = {
    MUS_WORD(LyricTarget, "15", GLOBALS_CMPER, /*incidence*/ 0, hyphenSeparationSlot, maxHyphenSeparation),
};

// The word-extension line width, wherever selector 67 exists. This is the one scalar of the
// class that varies from document to document, and it varies in all three epochs that carry
// it.
const FieldMapping lyricLineWidthFields[] = {
    MUS_WORD(LyricTarget, "67", GLOBALS_CMPER, /*incidence*/ 0, wordExtLineWidthSlot, wordExtLineWidth),
};

// Believed: the Finale 2004 generation stores the pinned-baseline values in these four slots;
// their individual meanings are not yet distinguishable.
//
// `smartHyphenStart` passes through as a number because musxdom's SmartHyphenStart is in the
// legacy order, `always, sometimes, never`. Believed: `always` and `never` occupy the positions
// given by that enum.
const FieldMapping smartLyricFields[] = {
    MUS_WORD(LyricTarget, "57", GLOBALS_CMPER, /*incidence*/ 0, smartHyphenStartSlot, smartHyphenStart),
    MUS_WORD(LyricTarget, "57", GLOBALS_CMPER, /*incidence*/ 0, wordExtMinLengthSlot, wordExtMinLength),
    MUS_WORD(LyricTarget, "57", GLOBALS_CMPER, /*incidence*/ 0, wordExtOffsetToNoteheadSlot, wordExtOffsetToNotehead),
    MUS_WORD(LyricTarget, "35", GLOBALS_CMPER, /*incidence*/ 0, smartHyphenSlot, useSmartHyphens),
    MUS_WORD(LyricTarget, "34", GLOBALS_CMPER, /*incidence*/ 0, smartWordExtSlot, useSmartWordExtensions),
    MUS_WORD(LyricTarget, "57", GLOBALS_CMPER, /*incidence*/ 0, wordExtNeedUnderscoreSlot, wordExtNeedUnderscore),
};

// Finale 2007 and later. The same logical words, reached through the shared numericGlobalClass
// rule and addressed by byte offset in the coalesced payload. Both byte orders occur in this
// era.
const FieldMapping classLyricHyphenFields[] = {
    MUS_CLASS_WORD(
        LyricTarget, numericGlobalClass(hyphenSeparationSelector), GLOBALS_CMPER, classWordOffset(hyphenSeparationSlot), maxHyphenSeparation),
};

const FieldMapping classLyricLineWidthFields[] = {
    MUS_CLASS_WORD(LyricTarget, numericGlobalClass(wordExtLineWidthSelector), GLOBALS_CMPER, classWordOffset(wordExtLineWidthSlot), wordExtLineWidth),
};

const FieldMapping classSmartLyricFields[] = {
    MUS_CLASS_WORD(LyricTarget, numericGlobalClass(wordExtSelector), GLOBALS_CMPER, classWordOffset(smartHyphenStartSlot), smartHyphenStart),
    MUS_CLASS_WORD(LyricTarget, numericGlobalClass(wordExtSelector), GLOBALS_CMPER, classWordOffset(wordExtMinLengthSlot), wordExtMinLength),
    MUS_CLASS_WORD(
        LyricTarget, numericGlobalClass(wordExtSelector), GLOBALS_CMPER, classWordOffset(wordExtOffsetToNoteheadSlot), wordExtOffsetToNotehead),
    MUS_CLASS_WORD(LyricTarget, numericGlobalClass(smartHyphenSelector), GLOBALS_CMPER, classWordOffset(smartHyphenSlot), useSmartHyphens),
    MUS_CLASS_WORD(LyricTarget, numericGlobalClass(smartWordExtSelector), GLOBALS_CMPER, classWordOffset(smartWordExtSlot), useSmartWordExtensions),
    MUS_CLASS_WORD(
        LyricTarget, numericGlobalClass(wordExtSelector), GLOBALS_CMPER, classWordOffset(wordExtNeedUnderscoreSlot), wordExtNeedUnderscore),
};

const MappingTable& lyricHyphenTable()
{
    static const MappingTable table{.reportPrefix = lyricReportPrefix,
        .epochs = EpochMask::FixedRow,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<LyricTarget>,
        .fields = lyricHyphenFields,
        .fieldCount = std::size(lyricHyphenFields)};
    return table;
}

const MappingTable& lyricLineWidthTable()
{
    // The presence test alone decides this one, as it must: the capture pass asserts a value
    // when the same test says the record is absent, so a mask that could disagree with it
    // would leave a document with neither a read value nor an assertion.
    static const MappingTable table{.reportPrefix = lyricReportPrefix,
        .epochs = fixedRowLyricEpochs,
        .applies = &storesWordExtLineWidth,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<LyricTarget>,
        .fields = lyricLineWidthFields,
        .fieldCount = std::size(lyricLineWidthFields)};
    return table;
}

const MappingTable& smartLyricTable()
{
    static const MappingTable table{.reportPrefix = lyricReportPrefix,
        .epochs = fixedRowLyricEpochs,
        .applies = &storesSmartLyricOptions,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<LyricTarget>,
        .fields = smartLyricFields,
        .fieldCount = std::size(smartLyricFields)};
    return table;
}

const MappingTable& classLyricHyphenTable()
{
    static const MappingTable table{.reportPrefix = lyricReportPrefix,
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<LyricTarget>,
        .fields = classLyricHyphenFields,
        .fieldCount = std::size(classLyricHyphenFields)};
    return table;
}

const MappingTable& classLyricLineWidthTable()
{
    static const MappingTable table{.reportPrefix = lyricReportPrefix,
        .epochs = EpochMask::Zlib,
        .applies = &storesWordExtLineWidth,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<LyricTarget>,
        .fields = classLyricLineWidthFields,
        .fieldCount = std::size(classLyricLineWidthFields)};
    return table;
}

// Finale 2012 and later only; see @ref storesEdgePunctuationSetting. musxdom spells the
// setting as the reverse of Finale's dialog, and so does the record: the word is set when
// punctuation is *used* rather than ignored, which is why it needs no inversion on the way in.
const FieldMapping classEdgePunctuationFields[] = {
    MUS_CLASS_WORD(LyricTarget, numericGlobalClass(wordExtSelector), GLOBALS_CMPER, classWordOffset(edgePunctuationSlot), lyricUseEdgePunctuation),
};

// Finale 2011 and later, selected by the record's own size rather than by a version. musxdom's
// AutoNumberingAlign is in the legacy order, None then Align, so the type passes through as a
// number, 0 being `None` and 1 `Align`.
const FieldMapping classAutoNumberFields[] = {
    MUS_CLASS_BIT(
        LyricTarget, numericGlobalClass(autoNumberSelector), GLOBALS_CMPER, classWordOffset(autoNumberVersesSlot), 0, showAutoNumbersOnVerses),
    MUS_CLASS_BIT(
        LyricTarget, numericGlobalClass(autoNumberSelector), GLOBALS_CMPER, classWordOffset(autoNumberChorusesSlot), 0, showAutoNumbersOnChoruses),
    MUS_CLASS_BIT(
        LyricTarget, numericGlobalClass(autoNumberSelector), GLOBALS_CMPER, classWordOffset(autoNumberSectionsSlot), 0, showAutoNumbersOnSections),
    MUS_CLASS_WORD(LyricTarget, numericGlobalClass(autoNumberSelector), GLOBALS_CMPER, classWordOffset(autoNumberTypeSlot), lyricAutoNumType),
};

const MappingTable& classAutoNumberTable()
{
    // The marker alone decides this one, and it must: the capture pass asserts the three
    // switches false when the same test says the record is the short one.
    static const MappingTable table{.reportPrefix = lyricReportPrefix,
        .epochs = EpochMask::Zlib,
        .applies = &storesAutoNumbering,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<LyricTarget>,
        .fields = classAutoNumberFields,
        .fieldCount = std::size(classAutoNumberFields)};
    return table;
}

const MappingTable& classEdgePunctuationTable()
{
    // The gate alone decides this one, and it must: the capture pass asserts the opposite when
    // the same test says the era has no setting, so a mask that could disagree with it would
    // leave a document with neither a read value nor an assertion.
    static const MappingTable table{.reportPrefix = lyricReportPrefix,
        .epochs = EpochMask::Zlib,
        .applies = &storesEdgePunctuationSetting,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<LyricTarget>,
        .fields = classEdgePunctuationFields,
        .fieldCount = std::size(classEdgePunctuationFields)};
    return table;
}

const MappingTable& classSmartLyricTable()
{
    static const MappingTable table{.reportPrefix = lyricReportPrefix,
        .epochs = EpochMask::Zlib,
        .applies = &storesSmartLyricOptions,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<LyricTarget>,
        .fields = classSmartLyricFields,
        .fieldCount = std::size(classSmartLyricFields)};
    return table;
}

template <typename Reporting>
void reportLyricField(Reporting& reporting, const std::string& member, typename Reporting::Origin origin, std::int64_t rawValue,
    std::size_t blockOffset = 0, std::size_t decodedOffset = 0)
{
    reporting.report().setField(reporting.template instanceKey<LyricTarget>(), member, {origin, blockOffset, decodedOffset, rawValue});
}

/// @brief The seeded style for one syllable position, created if the baseline lacked it.
std::shared_ptr<LyricSyllableStyle> syllableStyleFor(const std::shared_ptr<LyricTarget>& target, LyricSyllableType type)
{
    auto& slot = target->syllablePosStyles[type];
    if (!slot) {
        slot = std::make_shared<LyricSyllableStyle>();
    }
    return slot;
}

/// @brief The seeded style for one word-extension connection, created if the baseline lacked it.
std::shared_ptr<LyricConnectStyle> connectStyleFor(const std::shared_ptr<LyricTarget>& target, LyricConnectType type)
{
    auto& slot = target->wordExtConnectStyles[type];
    if (!slot) {
        slot = std::make_shared<LyricConnectStyle>();
    }
    return slot;
}

/// @brief Recovers the two collections and asserts what an era fixed rather than stored.
/// @details Both collections are fixed-length maps in musxdom rather than scalars, so neither
/// can be expressed as field mappings; both are read here from the numeric global's whole word
/// stream, which is one incidence family before Finale 2007 and one coalesced payload after.
/// Runs before the scalar tables, whose gates then leave the asserted fields alone.
void captureLyricOptions(
    const records::LegacyRecordIndex& index, const SourceProfile& profile, const musx::dom::DocumentPtr& document, ImportReport& report)
{
    const auto pooled = document->getOptions()->get<LyricTarget>();
    if (!pooled) {
        return;
    }
    // Pool instances are handed out const. Overlaying legacy values is the one reason this
    // library writes to them.
    auto target = std::const_pointer_cast<LyricTarget>(pooled);

    // Selector 87: four syllable positions, three words each, spread over two fixed rows. The
    // whole group arrives with Finale 2000, so its presence rather than a version decides
    // whether it can be read.
    const auto syllable = readGlobalWords(index, profile, syllablePosSelector);
    for (std::size_t position = 0; position < std::size(syllablePositionOrder); ++position) {
        const auto type = syllablePositionOrder[position];
        const auto style = syllableStyleFor(target, type);
        const auto name = [&] { return std::string("syllablePosStyles[") + syllablePositionNames[position] + ']'; };
        if (syllable.present) {
            const auto first = position * syllablePositionWords;
            if (const auto align = lyricAlignment(wordAt(syllable.words, first))) {
                style->align = *align;
                withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
                    reportLyricField(reporting, name() + ".align", Reporting::Origin::LegacyMus, wordAt(syllable.words, first), syllable.blockOffset,
                        syllable.decodedOffset);
                });
            }
            if (const auto justify = lyricAlignment(wordAt(syllable.words, first + 1))) {
                style->justify = *justify;
                withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
                    reportLyricField(reporting, name() + ".justify", Reporting::Origin::LegacyMus, wordAt(syllable.words, first + 1),
                        syllable.blockOffset, syllable.decodedOffset);
                });
            }
            const auto flags = static_cast<std::uint16_t>(wordAt(syllable.words, first + 2));
            style->on = (flags & (1U << syllablePositionOnBit)) != 0;
            withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
                reportLyricField(
                    reporting, name() + ".on", Reporting::Origin::LegacyMus, style->on ? 1 : 0, syllable.blockOffset, syllable.decodedOffset);
            });
        } else if (type != LyricSyllableType::Default) {
            // Before the selector exists the three optional positions are simply not applied,
            // and the pinned baseline says the opposite: it switches all three on. Leaving them
            // to the baseline would claim positioning the source never asked for. The
            // alignments are left alone, the baseline already carrying what the era did.
            style->on = false;
            withReporting(report,
                [&]<typename Reporting>(Reporting& reporting) { reportLyricField(reporting, name() + ".on", Reporting::Origin::LegacyBehavior, 0); });
        }
    }

    // Selector 55 begins as eight three-word styles in four fixed rows. The later layout adds
    // ZeroOffset as a ninth style in a fifth row, whose final three words are padding. The
    // payload length states which layout is present.
    //
    // Coda-banner documents are excluded outright: they reuse selector 55 for an unrelated
    // six-word option. The minimum eight-style length keeps those bytes from being mistaken for
    // this table even if the epoch were ever misclassified.
    const auto connect = readGlobalWords(index, profile, wordExtConnectSelector);
    const bool hasWordExtConnectTable =
        profile.epoch != FormatEpoch::CodaBanner && connect.present && connect.words.size() >= earlyWordExtConnectCount * wordExtConnectWords;
    if (hasWordExtConnectTable) {
        const auto elementCount = connect.words.size() >= wordExtConnectCount * wordExtConnectWords ? wordExtConnectCount : earlyWordExtConnectCount;
        for (std::size_t element = 0; element < elementCount; ++element) {
            const auto type = wordExtConnectOrder[element];
            const auto style = connectStyleFor(target, type);
            const auto first = element * wordExtConnectWords;
            const auto name = [&] { return std::string("wordExtConnectStyles[") + wordExtConnectNames[element] + ']'; };
            const auto stored = wordAt(connect.words, first);
            const auto ordinal = stored - firstLegacyConnectIndex;
            if (ordinal >= 0 && static_cast<std::size_t>(ordinal) < std::size(legacyConnectIndexOrder)) {
                style->connectIndex = legacyConnectIndexOrder[ordinal];
                withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
                    reportLyricField(
                        reporting, name() + ".connectIndex", Reporting::Origin::LegacyMus, stored, connect.blockOffset, connect.decodedOffset);
                });
            }
            style->xOffset = wordAt(connect.words, first + 1);
            style->yOffset = wordAt(connect.words, first + 2);
            withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
                reportLyricField(
                    reporting, name() + ".xOffset", Reporting::Origin::LegacyMus, style->xOffset, connect.blockOffset, connect.decodedOffset);
                reportLyricField(
                    reporting, name() + ".yOffset", Reporting::Origin::LegacyMus, style->yOffset, connect.blockOffset, connect.decodedOffset);
            });
        }
    }

    // "Lift" and "Push" have homes of their own on selectors 29 and 30, word 5 of each, and
    // those homes exist in **every** epoch -- which is what makes them the only fields of this
    // class a Coda-banner document can supply.
    const auto lift = readGlobalWords(index, profile, wordExtLiftSelector);
    if (lift.present) {
        target->wordExtVertOffset = wordAt(lift.words, wordExtOffsetSlot);
        withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
            reportLyricField(
                reporting, "wordExtVertOffset", Reporting::Origin::LegacyMus, target->wordExtVertOffset, lift.blockOffset, lift.decodedOffset);
        });
    }
    const auto push = readGlobalWords(index, profile, wordExtPushSelector);
    if (push.present) {
        target->wordExtHorzOffset = wordAt(push.words, wordExtOffsetSlot);
        withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
            reportLyricField(
                reporting, "wordExtHorzOffset", Reporting::Origin::LegacyMus, target->wordExtHorzOffset, push.blockOffset, push.decodedOffset);
        });
    }

    // musxdom keeps the same two numbers twice: as the class-level pair above and again as the
    // starting connection's own offsets. A source from Finale 2004 on states them twice as
    // well, in selector 55's first element, and the two always agree. Before selector 55 exists
    // there is no connection table to read, so the starting element takes the dialog's values,
    // which is the era's own behavior.
    if (!hasWordExtConnectTable && (lift.present || push.present)) {
        const auto starting = connectStyleFor(target, LyricConnectType::DefaultStart);
        starting->xOffset = target->wordExtHorzOffset;
        starting->yOffset = target->wordExtVertOffset;
    }

    // The word-extension line width predates its own record. A Coda-banner document has no
    // selector 67, and the pinned baseline's 115 is not what those documents mean: the era
    // behaves as 224, which is also what the earliest document carrying the selector stores.
    // This is the one value of the class the baseline supplies wrongly for an era, so it is
    // asserted rather than left.
    if (!storesWordExtLineWidth(index, profile)) {
        target->wordExtLineWidth = unstatedWordExtLineWidth;
        withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
            reportLyricField(reporting, "wordExtLineWidth", Reporting::Origin::LegacyBehavior, unstatedWordExtLineWidth);
        });
    }

    // Syllable edge punctuation is ignored by default in a Finale 27 document and was not
    // ignored at all in the earlier eras, so the baseline states the opposite of what they did.
    // See @ref storesEdgePunctuationSetting for the boundary.
    if (!storesEdgePunctuationSetting(index, profile)) {
        target->lyricUseEdgePunctuation = true;
        withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
            reportLyricField(reporting, "lyricUseEdgePunctuation", Reporting::Origin::LegacyBehavior, 1);
        });
    }

    // The list of punctuation to ignore is stored as a variable-length tail on selector 57
    // rather than in a record of its own, and **Finale writes the tail only when the list is
    // not the stock one**. The record is twelve bytes with no tail; a list of four characters
    // grows it to twenty-four, the six scalars unchanged and the characters appended followed
    // by a zero terminator.
    //
    // A document with no tail therefore means the stock list, and the right thing to do is
    // nothing: the pinned baseline states no <lyricPunctuationToIgnore> either, and musxdom's
    // LyricOptions::integrityCheck supplies exactly that set for an empty one. Writing it here
    // would be a second copy of a default musxdom already owns.
    if (storesEdgePunctuationSetting(index, profile)) {
        const auto lyric = readGlobalWords(index, profile, wordExtSelector);
        // **The tail has two layouts, and the Unicode release is the boundary.** From Finale
        // 2012 it is one 16-bit code unit per character; before that it is packed 8-bit bytes
        // in the saving platform's code page. The two are not variants of one rule and neither
        // can be read as the other -- the container is little-endian, so decoding the byte
        // string through the word path would transpose every pair of characters.
        //
        // The code page's bank comes from the document's own platform, as it does for a font
        // definition that carries no charset of its own. That is the only source available
        // here, this text belonging to no font record.
        const bool unicodeTail = versions::storesUnicodeCodepoints(profile.version);
        std::string ignored;
        std::size_t units = 0;
        if (unicodeTail) {
            for (std::size_t word = punctuationTailWord; word < lyric.words.size(); ++word) {
                const auto unit = static_cast<std::uint16_t>(lyric.words[word]);
                if (unit == 0) {
                    break;
                }
                ++units;
                if (unit >= 0xd800 && unit <= 0xdbff && word + 1 < lyric.words.size()) {
                    const auto low = static_cast<std::uint16_t>(lyric.words[word + 1]);
                    if (low >= 0xdc00 && low <= 0xdfff) {
                        ++word;
                        ++units;
                    }
                }
            }
            ignored = text::utf16ToUtf8(std::span(lyric.words).subspan((std::min)(punctuationTailWord, lyric.words.size())));
        } else if (const auto* row = index.getClassOthers().get(numericGlobalClass(wordExtSelector), GLOBALS_CMPER, 0, 0)) {
            // Bytes, so the payload is read directly rather than through the word stream the
            // rest of this class uses.
            const auto payload = index.getClassOthers().effectivePayloadOf(*row);
            std::string stored;
            for (std::size_t at = punctuationTailWord * 2; at < payload.size(); ++at) {
                if (payload[at] == 0) {
                    break;
                }
                stored += static_cast<char>(payload[at]);
            }
            units = stored.size();
            if (!stored.empty()) {
                // No font names an encoding for this string, so the document's own platform
                // decides. The platform overload of @ref text::toUtf8 owns that fallback.
                ignored = text::toUtf8(stored, profile.platform);
            }
        }
        if (!ignored.empty()) {
            target->lyricPunctuationToIgnore = ignored;
        }
        // The reported value is the number of code units or bytes the tail supplied, because
        // the field is text and the report carries numbers. Zero says the document kept the
        // stock list.
        withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
            reportLyricField(reporting, "lyricPunctuationToIgnore",
                ignored.empty() ? Reporting::Origin::Finale27Default : Reporting::Origin::LegacyMus, static_cast<std::int64_t>(units),
                lyric.blockOffset, lyric.decodedOffset);
        });
    }

    // Smart hyphens, smart word extensions and the underscore requirement all arrive together
    // with Finale 2004 and did not exist before it, so all three are false for an earlier
    // document rather than merely unstated.
    //
    // **The reason is about what this import can produce, not only about what the era did.**
    // Smart hyphens and smart word extensions do not stand on their own: each is implemented as
    // smart shapes, hyphen smart shapes for the one and word-extension smart shapes for the
    // other. Finale's own upgrade manufactures those, which is why the pinned baseline switches
    // both on. **This import does not manufacture them.** Leaving the switches on would
    // describe a document it has not built -- an option claiming a rendering that nothing in the
    // imported pools can draw -- so the honest value is false, and the resulting disagreement
    // with Finale's upgrade is deliberate. It is a disagreement about capability rather than
    // about what the bytes say.
    //
    // The underscore requirement is the quiet member of the group: the baseline already leaves
    // it false, and it is asserted for the same reason
    // MultimeasureRestOptions::noHorizontalStretch is, because the era's behavior is known
    // rather than inherited.
    if (!storesSmartLyricOptions(index, profile)) {
        target->useSmartHyphens = false;
        withReporting(report,
            [&]<typename Reporting>(Reporting& reporting) { reportLyricField(reporting, "useSmartHyphens", Reporting::Origin::LegacyBehavior, 0); });
        target->useSmartWordExtensions = false;
        withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
            reportLyricField(reporting, "useSmartWordExtensions", Reporting::Origin::LegacyBehavior, 0);
        });
        target->wordExtNeedUnderscore = false;
        withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
            reportLyricField(reporting, "wordExtNeedUnderscore", Reporting::Origin::LegacyBehavior, 0);
        });
    }

    // Automatic lyric numbering arrives with Finale 2011, so a document whose selector 58 is
    // still six words shows no automatic numbers at all. The pinned baseline agrees, and the
    // three switches are asserted anyway on the same footing as the smart-lyric group: the
    // era's behavior is known rather than inherited. `lyricAutoNumType` is left alone, because
    // the numbering type of a document that displays no numbers means nothing.
    if (!storesAutoNumbering(index, profile)) {
        target->showAutoNumbersOnVerses = false;
        withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
            reportLyricField(reporting, "showAutoNumbersOnVerses", Reporting::Origin::LegacyBehavior, 0);
        });
        target->showAutoNumbersOnChoruses = false;
        withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
            reportLyricField(reporting, "showAutoNumbersOnChoruses", Reporting::Origin::LegacyBehavior, 0);
        });
        target->showAutoNumbersOnSections = false;
        withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
            reportLyricField(reporting, "showAutoNumbersOnSections", Reporting::Origin::LegacyBehavior, 0);
        });
    }

    // The alternate hyphen font postdates Finale 2012, the last release this library opens, so
    // no legacy format has anywhere to put it and no legacy document uses a second font for its
    // hyphens. The switch is known false for every file that can arrive here.
    //
    // The pinned baseline also leaves it false, so the usual rule -- leave a field to the
    // baseline where the baseline already agrees, rather than keeping a second copy of one fact
    // -- would say to omit this. It is asserted anyway, for the same reason
    // MultimeasureRestOptions::noHorizontalStretch is: the two statements are not the same
    // statement. The baseline saying false is one Finale 27 document's setting, which a later
    // pinned baseline could legitimately change; the setting postdating every legacy format is
    // a fact about the formats, and that is what makes the value known rather than
    // synthesized.
    target->useAltHyphenFont = false;
    withReporting(report,
        [&]<typename Reporting>(Reporting& reporting) { reportLyricField(reporting, "useAltHyphenFont", Reporting::Origin::LegacyBehavior, 0); });

    // These members postdate every supported legacy layout. Their seeded values remain untouched;
    // musxdom creates the FontInfo placeholder after import.
    withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
        reportLyricField(reporting, "hyphenChar", Reporting::Origin::MusxOnly, static_cast<std::int64_t>(target->hyphenChar));
        reportLyricField(reporting, "altHyphenFont.fontId", Reporting::Origin::MusxOnly, 0);
        reportLyricField(reporting, "altHyphenFont.fontSize", Reporting::Origin::MusxOnly, 0);
        reportLyricField(reporting, "altHyphenFont.effects", Reporting::Origin::MusxOnly, 0);
    });
}

} // namespace

void importLyricOptions(const ImportContext& context)
{
    // The capture pass runs first because it establishes the fields no era-appropriate table
    // will read, and the table machinery leaves an already-reported field alone rather than
    // downgrading it to a synthesized default.
    captureLyricOptions(context.index, context.profile, context.document, context.report);
    applyMappingTables({&lyricHyphenTable(), &lyricLineWidthTable(), &smartLyricTable(), &classLyricHyphenTable(), &classLyricLineWidthTable(),
                           &classSmartLyricTable(), &classEdgePunctuationTable(), &classAutoNumberTable()},
        context.index, context.profile, context.document, context.report);
}

} // namespace options
} // namespace finale_mus_reader
