// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

// Early HS/HT text recovery. The Coda-banner epoch also keeps numbered block texts and
// lyrics in a separate text region; later files can carry HS/HT blocks alongside a numbered text pool.
//
// Block text is in the `HT` and `HS` others families, one pair per block. `HT` holds the
// characters and `HS` holds the style, and they are keyed alike: `HS` incidence n describes the
// nth `HT` record of the same comparator.
//
// The text region after the last record pool has two length-prefixed chunks. The first
// can carry `^block(n)` text named by `PT`; the second carries lyrics. Both use spelled-out Enigma.

#include "import/texts.h"
#include "import/texts/internal.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "import/shared/coda_text_records.h"
#include "import/shared/page_text_records.h"
#include "import/support/enigma_text.h"
#include "import/support/legacy_mapping.h"
#include "import/support/text_encoding.h"
#include "reader/timing.h"
#include "records/legacy_record_index.h"

#include "musx/musx.h"

namespace finale_mus_reader {
namespace texts {
namespace {

using musx::dom::Cmper;
using records::LegacyRow;
using CodaTextFontType = musx::dom::options::FontOptions::FontType;

constexpr records::LegacyTag codaStyleRecord = records::packTag("HS");
constexpr records::LegacyTag codaTextRecord = records::packTag("HT");

// A block's characters occupy four consecutive `HT` incidences, so 48 bytes, however short the
// string is. What follows the terminator is a previous save's bytes and layout data, neither of
// which this reads.
// `HS` word 2 packs the font comparator and point size, with their byte positions depending
// on the container epoch. Word 3 is the `nfx` style mask. Words 0 and 1 are the block's
// position, which musxdom keeps elsewhere.
constexpr std::size_t codaStyleEffectsSlot = 3;
constexpr std::size_t codaStyleInsertArgumentSlot = 4;
constexpr std::size_t codaStyleFlagsSlot = 5;

// This era writes an insert as a single character rather than as a command, and the same
// character stands for whichever insert the block carries. Which one it is comes from the style
// record: word 5 selects it and word 4 is its argument -- a page offset for `^page`, a format
// ordinal for `^date`, the seconds flag for `^time`.
//
// **Believed: the selector is the two bits above the low pair of word 5.** Three values are
// established, one per insert the era offers, and every block with no insert leaves them clear.
// Whether they are a two-bit field or two independent flags cannot be told apart by three
// observations; an unlisted value is reported rather than guessed at.
constexpr char codaInsertCharacter = '#';
constexpr unsigned codaInsertSelectorShift = 2;
constexpr std::uint16_t codaInsertSelectorMask = 0x3;

constexpr std::string_view codaInsertCommands[] = {"page", "date", "time"};

/// @brief Restates one block as an Enigma string, which is the form musxdom reads.
/// @details The style commands are synthesized because the era states them in a record rather
/// than in the text, and only what `HS` carries is written. A document whose page offset is
/// zero still gets `^page(0)` wherever its text holds the insert, the insert being what the
/// document states.
std::string spellCodaBlock(const LegacyRow& style, FormatEpoch epoch, std::string_view characters, bool& unknownInsert)
{
    const auto fontSize = coda_text::styleFontSize(style, epoch);
    std::string result = "^font(Font" + std::to_string(fontSize.font) + ")";
    result += "^size(" + std::to_string(fontSize.size) + ")";
    result += "^nfx(" + std::to_string(style.words[codaStyleEffectsSlot]) + ")";

    const auto selector =
        static_cast<std::uint16_t>((static_cast<std::uint16_t>(style.words[codaStyleFlagsSlot]) >> codaInsertSelectorShift) & codaInsertSelectorMask);
    std::string insert;
    if (selector < std::size(codaInsertCommands)) {
        insert = '^' + std::string(codaInsertCommands[selector]) + '(' + std::to_string(style.words[codaStyleInsertArgumentSlot]) + ')';
    }
    for (std::size_t at = 0; at < characters.size(); ++at) {
        const auto character = characters[at];
        if (character == codaInsertCharacter) {
            // A doubled insert character is escaped literal content and becomes one hash,
            // parallel to Enigma's `^^` spelling for a literal caret.
            if (at + 1 < characters.size() && characters[at + 1] == codaInsertCharacter) {
                result.push_back(codaInsertCharacter);
                ++at;
                continue;
            }
            // A selector with no command behind it leaves the character as it stands: the
            // block still says something, and inventing an insert would say the wrong thing.
            if (insert.empty()) {
                unknownInsert = true;
                result.push_back(character);
            } else {
                result += insert;
            }
        } else if (character == '^') {
            // A literal caret survives as the escape every era spells it with.
            result += "^^";
        } else {
            result.push_back(character);
        }
    }
    return result;
}

/// @brief Converts a synthesized Enigma string and adds it to the document.
template <typename Target>
void addCodaText(
    const ImportContext& context, const text::EnigmaTextSource& source, Cmper number, const std::string& spelled, CodaTextFontType defaultFontType)
{
    FINALE_MUS_READER_TIMING_INCREMENT(timing::Counter::TextRecords, 1);
    FINALE_MUS_READER_TIMING_INCREMENT(timing::Counter::TextRecordBytes, spelled.size());
    auto recordSource = source;
    recordSource.initialFont = musx::dom::options::FontOptions::getFontInfoOrNull(context.document, defaultFontType);
    auto converted =
        text::toModernEnigmaText(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(spelled.data()), spelled.size()), recordSource);
    auto instance = std::make_shared<Target>(context.document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, number);
    instance->text = std::move(converted.text);
    context.document->getTexts()->add(Target::XmlNodeName, std::move(instance));

    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<Target>(musx::dom::SCORE_PARTID, number);
        reporting.textField(key, "text", converted);
        reporting.report().setField(key, "text", {Reporting::Origin::LegacyMus, 0, 0, static_cast<std::int64_t>(converted.text.size())});
    });
}

void importCodaBlockTexts(const ImportContext& context, const text::EnigmaTextSource& source)
{
    const auto& pool = context.index.getOthers();
    Cmper number = 0;
    for (const auto& existing : context.document->getTexts()->getArray<musx::dom::texts::BlockText>()) {
        number = (std::max)(number, existing->getTextNumber());
    }
    for (const auto cmper : pool.cmpersForTag(codaTextRecord)) {
        const auto characters = pool.getArray(codaTextRecord, cmper);
        const auto records = static_cast<std::uint32_t>(characters.size() / coda_text::rowsPerBlock);
        for (std::uint32_t record = 0; record < records; ++record) {
            const auto* style = pool.get(codaStyleRecord, cmper, 0, record);
            if (!style) {
                // Without the style record there is no font, size or page offset to state, and
                // no way to tell the insert from a literal `#`. Reporting the gap is better
                // than writing a block that says something the document does not.
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Warning, "A legacy text block carries characters with no matching style record and "
                                                            "was not imported."});
                continue;
            }
            if (number == (std::numeric_limits<Cmper>::max)()) {
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Warning, "A legacy text block exceeds the available text comparators and was not imported."});
                return;
            }
            ++number;
            bool unknownInsert = false;
            auto spelled = spellCodaBlock(*style, context.profile.epoch, coda_text::readBlockCharacters(pool, characters, record), unknownInsert);
            if (unknownInsert) {
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Warning, "A legacy text block carries an insert this reader has no command for; the "
                                                            "insert character was kept as it stands."});
            }
            addCodaText<musx::dom::texts::BlockText>(context, source, number, spelled, CodaTextFontType::TextBlock);
            context.pending.codaTextBlockByStyle.emplace(std::pair{cmper, static_cast<musx::dom::Inci>(record)}, number);
        }
    }
}

// The lyric keywords this era spells, and the musxdom class each names. `verse`, `chorus` and
// `section` are the same three the later text pool uses; only the framing around them differs.
struct CodaTextKeyword
{
    std::string_view keyword;
    void (*add)(const ImportContext&, const text::EnigmaTextSource&, Cmper, const std::string&, CodaTextFontType);
    CodaTextFontType defaultFontType;
};

constexpr CodaTextKeyword codaTextKeywords[] = {
    {"block", &addCodaText<musx::dom::texts::BlockText>, CodaTextFontType::TextBlock},
    {"verse", &addCodaText<musx::dom::texts::LyricsVerse>, CodaTextFontType::LyricVerse},
    {"chorus", &addCodaText<musx::dom::texts::LyricsChorus>, CodaTextFontType::LyricChorus},
    {"section", &addCodaText<musx::dom::texts::LyricsSection>, CodaTextFontType::LyricSection},
};

/// @brief The chunks of the text region, each a four-byte big-endian count and that many bytes.
std::vector<std::string_view> readCodaChunks(std::span<const std::uint8_t> region)
{
    constexpr std::size_t countBytes = 4;
    std::vector<std::string_view> result;
    std::size_t at = 0;
    while (at + countBytes <= region.size()) {
        std::size_t length = 0;
        for (std::size_t i = 0; i < countBytes; ++i) {
            length = (length << 8U) | region[at + i];
        }
        at += countBytes;
        if (length > region.size() - at) {
            break;
        }
        result.emplace_back(reinterpret_cast<const char*>(region.data() + at), length);
        at += length;
    }
    return result;
}

/// @brief The keyword and number a record opens with, when it opens with one.
struct CodaTextRecord
{
    std::size_t keyword{};
    Cmper number{};
    std::size_t bodyStart{};
};

std::optional<CodaTextRecord> readCodaTextHeader(std::string_view chunk, std::size_t at)
{
    if (at >= chunk.size() || chunk[at] != '^') {
        return std::nullopt;
    }
    for (std::size_t keyword = 0; keyword < std::size(codaTextKeywords); ++keyword) {
        const auto name = codaTextKeywords[keyword].keyword;
        if (at + 1 + name.size() >= chunk.size() || chunk.compare(at + 1, name.size(), name) != 0 || chunk[at + 1 + name.size()] != '(') {
            continue;
        }
        std::size_t digits = at + 2 + name.size();
        Cmper number = 0;
        while (digits < chunk.size() && chunk[digits] >= '0' && chunk[digits] <= '9') {
            number = static_cast<Cmper>(number * 10 + (chunk[digits] - '0'));
            ++digits;
        }
        if (digits >= chunk.size() || chunk[digits] != ')') {
            return std::nullopt;
        }
        // One separating space belongs to the framing rather than to the text. Finale drops it
        // when it converts these records, and a lyric that began with a space it never had
        // would shift every syllable.
        std::size_t body = digits + 1;
        if (body < chunk.size() && chunk[body] == ' ') {
            ++body;
        }
        return CodaTextRecord{keyword, number, body};
    }
    return std::nullopt;
}

void importCodaTextRegion(const ImportContext& context, const text::EnigmaTextSource& source)
{
    const auto chunks = readCodaChunks(context.index.getTexts());
    for (const auto chunk : chunks) {
        std::size_t at = 0;
        while (at < chunk.size()) {
            const auto header = readCodaTextHeader(chunk, at);
            if (!header) {
                ++at;
                continue;
            }
            // A record runs to the next one, there being no terminator: the commands inside a
            // record also begin with a caret, so only a keyword this reader knows ends it.
            std::size_t end = header->bodyStart;
            while (end < chunk.size() && !readCodaTextHeader(chunk, end)) {
                ++end;
            }
            auto body = std::string(chunk.substr(header->bodyStart, end - header->bodyStart));
            while (!body.empty() && body.back() == '\0') {
                body.pop_back();
            }
            const auto& keyword = codaTextKeywords[header->keyword];
            keyword.add(context, source, header->number, body, keyword.defaultFontType);
            at = end;
        }
    }
}

} // namespace

void importCodaStoredTexts(const ImportContext& context)
{
    const bool codaBanner = context.profile.epoch == FormatEpoch::CodaBanner;
    if (!codaBanner && !others::hasLegacyPageTextStyle(context)) {
        return;
    }
    // HS/HT bytes predate Unicode and use a code page in either container layout.
    text::EnigmaFontResolutionCache fontResolutionCache;
    const text::EnigmaTextSource source{context.document, /*utf8*/ false, context.profile.platform, nullptr, &fontResolutionCache};
    if (codaBanner) {
        importCodaTextRegion(context, source);
    }
    importCodaBlockTexts(context, source);
}

} // namespace texts
} // namespace finale_mus_reader
