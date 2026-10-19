// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/texts.h"
#include "import/texts/internal.h"

#include <algorithm>
#include <array>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

#include "import/support/enigma_text.h"
#include "musx/musx.h"
#include "reader/timing.h"

namespace finale_mus_reader {
namespace texts {

namespace {

using musx::dom::Cmper;
using TextFontType = musx::dom::options::FontOptions::FontType;

// The text pool is the one pre-2007 pool that is not made of records, and the one place the
// legacy format describes itself in words. It holds `^keyword(n) ... ^end` chunks end to end,
// so the class of each chunk is stated by the file rather than inferred from a comparator or
// a position.
//
// The pool has its own block per epoch: 0x0004 uncompressed, 0x0012 DCL, 0x0017 zlib. A
// chunk's number is the comparator musxdom keys the pool by.
//
// **The Coda-banner epoch is uncovered here, by intent rather than by oversight.** That era has
// a text region -- two length-prefixed chunks after the last record pool -- but its block text
// lives in the `HT` record family instead, interleaved with binary
// layout data whose framing is unresolved. Reading `HT` is separate work; nothing here covers
// it.
struct TextKeyword
{
    std::string_view keyword;
    void (*create)(const musx::dom::DocumentPtr& document, musx::dom::TextsPool& pool, Cmper number, std::string&& text);
    std::string_view nodeName;
    [[no_unique_address]] ReportClass reportClass;
    /// @brief The class default that completes a record's initial formatting state.
    /// @details Absent for a class whose text carries no font at all, which also leaves its
    /// literal bytes to the source platform's encoding rather than to a font's charset.
    std::optional<TextFontType> defaultFontType;
    /// @brief Optional test on the record's number, for a class that constrains it.
    /// @details `texts::FileInfoText` throws on a number outside its own enumeration, so a
    /// malformed chunk would abort the import rather than being skipped. Nothing else in the
    /// pool restricts its comparator.
    bool (*accepts)(Cmper number);
};

template <typename Target>
void createText(const musx::dom::DocumentPtr& document, musx::dom::TextsPool& pool, Cmper number, std::string&& text)
{
    auto instance = std::make_shared<Target>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, number);
    instance->text = std::move(text);
    pool.add(Target::XmlNodeName, std::move(instance));
}

template <typename Target>
TextKeyword textKeyword(std::string_view keyword, std::optional<TextFontType> defaultFontType, bool (*accepts)(Cmper) = nullptr)
{
    return TextKeyword{keyword, &createText<Target>, Target::XmlNodeName, ReportClass::of<Target>(), defaultFontType, accepts};
}

bool isFileInfoType(Cmper number)
{
    using FileInfoTarget = musx::dom::texts::FileInfoText;
    return number > 0 && number <= Cmper(FileInfoTarget::TextType::Subtitle);
}

// The keywords, and the musxdom class each one names.
//
// `block`, `smartshape`, `expression` and `verse` are established. Unverified: `chorus` and
// `section`. They follow `verse` because Finale treats the three lyric kinds as one family
// everywhere else. An unrecognized keyword is reported by name below.
const TextKeyword textKeywords[] = {
    textKeyword<musx::dom::texts::BlockText>("block", TextFontType::TextBlock),
    textKeyword<musx::dom::texts::LyricsVerse>("verse", TextFontType::LyricVerse),
    textKeyword<musx::dom::texts::LyricsChorus>("chorus", TextFontType::LyricChorus),
    textKeyword<musx::dom::texts::LyricsSection>("section", TextFontType::LyricSection),
    textKeyword<musx::dom::texts::SmartShapeText>("smartshape", TextFontType::TextBlock),
    // A bookmark's text carries no style commands of its own, and musxdom documents any Enigma
    // insert appearing in one as meaningless. It is read through the same converter regardless:
    // a caret still has to survive as an escaped one.
    //
    // **It is the one class given no default font**, so nothing is prepended and the record is
    // stored as its characters alone. Finale 27 writes a bookmark's text the same way, and the
    // source states no font either -- a `^bookmark` chunk holds nothing between its header and
    // its `^end` -- so a synthesized `^font`/`^size`/`^nfx` prefix would be this reader's
    // invention rather than anything the document says. A bookmark name is a label rather than
    // engraved text, which is the same reason musxdom calls its inserts meaningless.
    textKeyword<musx::dom::texts::BookmarkText>("bookmark", std::nullopt),
    textKeyword<musx::dom::texts::ExpressionText>("expression", TextFontType::Expression),
    // File Info starts out in the header and becomes ordinary pool records in a later era.
    // Where in between the move happens does not matter here: the header pass fills in only
    // the types the pool did not supply, so each document states for itself which way it
    // stores them. The number is musxdom's own `FileInfoText::TextType`.
    textKeyword<musx::dom::texts::FileInfoText>("fileInfo", TextFontType::TextBlock, &isFileInfoType),
};

constexpr std::string_view recordTerminator = "^end";

bool isLetter(std::uint8_t value)
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

/// @brief One chunk as the stream states it, before any of it is interpreted.
struct RawRecord
{
    std::string_view keyword;
    Cmper number{};
    std::span<const std::uint8_t> body;
    /// @brief Offset of the first byte after this chunk's terminator.
    std::size_t next{};
    /// @brief Offset of the chunk's own first byte, for reporting.
    std::size_t start{};
};

/// @brief Reads the chunk beginning at @p at, or nothing when the bytes are not one.
/// @brief The length of a section marker at @p at, or zero when there is none.
/// @details The earliest streams divide themselves into sections with a bare `^text` and
/// `^lyrics`, a keyword carrying no comparator. Finale 97 drops them, each record naming its
/// own kind. Some documents spell the same marker with an empty argument list instead,
/// `^text()` and `^lyrics()`, which is accepted here too -- but only for those two keywords:
/// an empty list is also how an ordinary inline command with no argument reads (`^composer()`,
/// `^date()`, and the like), and those must stay in the body rather than being mistaken for a
/// section boundary. A nonempty list is a numbered record and is left to fail there rather
/// than be consumed as a marker. Markers say nothing a record does not, so they are skipped
/// rather than read; what they are needed for is telling the two framings apart.
std::size_t sectionMarkerAt(std::span<const std::uint8_t> stream, std::size_t at)
{
    if (at >= stream.size() || stream[at] != '^') {
        return 0;
    }
    std::size_t cursor = at + 1;
    while (cursor < stream.size() && isLetter(stream[cursor])) {
        ++cursor;
    }
    if (cursor == at + 1) {
        return 0;
    }
    if (cursor < stream.size() && stream[cursor] == '(') {
        if (cursor + 1 < stream.size() && stream[cursor + 1] == ')') {
            const std::string_view keyword(reinterpret_cast<const char*>(stream.data() + at + 1), cursor - at - 1);
            if (keyword == "text" || keyword == "lyrics") {
                return cursor + 2 - at;
            }
        }
        return 0;
    }
    return cursor - at;
}

/// @brief Whether a record of a kind this reader knows begins at @p at.
/// @details Used to find the end of a record in the framing that has no terminator. Only a
/// known keyword ends a record: the body is full of commands that also open with a caret, and
/// an unknown keyword is more likely to be one of those than a record.
bool startsKnownRecord(std::span<const std::uint8_t> stream, std::size_t at)
{
    if (at >= stream.size() || stream[at] != '^') {
        return false;
    }
    std::size_t cursor = at + 1;
    while (cursor < stream.size() && isLetter(stream[cursor])) {
        ++cursor;
    }
    if (cursor >= stream.size() || stream[cursor] != '(') {
        return false;
    }
    const std::string_view keyword(reinterpret_cast<const char*>(stream.data() + at + 1), cursor - at - 1);
    return std::any_of(std::begin(textKeywords), std::end(textKeywords), [&](const TextKeyword& entry) { return entry.keyword == keyword; });
}

std::optional<RawRecord> readRecord(std::span<const std::uint8_t> stream, std::size_t at, bool terminated)
{
    if (at >= stream.size() || stream[at] != '^') {
        return std::nullopt;
    }
    std::size_t cursor = at + 1;
    const std::size_t keywordStart = cursor;
    while (cursor < stream.size() && isLetter(stream[cursor])) {
        ++cursor;
    }
    if (cursor == keywordStart || cursor >= stream.size() || stream[cursor] != '(') {
        return std::nullopt;
    }
    RawRecord record;
    record.start = at;
    record.keyword = std::string_view(reinterpret_cast<const char*>(stream.data() + keywordStart), cursor - keywordStart);
    ++cursor;
    // A comparator is sixteen bits, so a longer run of digits is not one, whatever it is.
    // Bounding it here is what stops a malformed stream from wrapping into a plausible number.
    constexpr std::size_t maximumComparatorDigits = 5;
    const std::size_t numberStart = cursor;
    std::uint32_t number = 0;
    while (cursor < stream.size() && stream[cursor] >= '0' && stream[cursor] <= '9' && cursor - numberStart < maximumComparatorDigits) {
        number = number * 10 + static_cast<std::uint32_t>(stream[cursor] - '0');
        ++cursor;
    }
    if (cursor == numberStart || cursor >= stream.size() || stream[cursor] != ')' || number > (std::numeric_limits<std::uint16_t>::max)()) {
        return std::nullopt;
    }
    record.number = static_cast<Cmper>(number);
    ++cursor;

    // The terminator is a plain search rather than a parse. A caret inside the body is either
    // a command, which cannot spell `end` and then stop, or an escaped `^^`, whose second
    // caret cannot begin `^end` either without the first having consumed it.
    const std::string_view remaining(reinterpret_cast<const char*>(stream.data() + cursor), stream.size() - cursor);
    if (!terminated) {
        // No terminator in this framing: a record runs to the next one, to the marker that
        // opens the next section, or to the end of the stream.
        std::size_t end = cursor;
        while (end < stream.size() && !startsKnownRecord(stream, end) && sectionMarkerAt(stream, end) == 0) {
            ++end;
        }
        record.body = stream.subspan(cursor, end - cursor);
        record.next = end;
        return record;
    }
    const auto terminator = remaining.find(recordTerminator);
    if (terminator == std::string_view::npos) {
        return std::nullopt;
    }
    record.body = stream.subspan(cursor, terminator);
    record.next = cursor + terminator + recordTerminator.size();
    return record;
}

void reportUnread(
    ImportReport& report, const std::vector<std::uint8_t>& codes, const std::vector<std::string>& effects, const std::vector<std::string>& keywords)
{
    if (!codes.empty()) {
        std::string message = "Legacy text commands this reader could not read were "
                              "dropped; their codes are";
        for (const auto code : codes) {
            message += ' ' + std::format("0x{:02x}", code);
        }
        report.diagnostics.push_back({musx::util::Logger::LogLevel::Info, message + "."});
    }
    if (!effects.empty()) {
        // Warning, not info: argumentsAt() already trims stray whitespace before an argument
        // reaches this table, which was the one known source of a real effect name failing to
        // match. What's left is a name this reader's vocabulary genuinely does not cover, and
        // the styling it names is silently dropped from the recovered text.
        std::string message = "Legacy text effects with no known bit were ignored:";
        for (const auto& effect : effects) {
            message += ' ' + effect;
        }
        report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning, message + "."});
    }
    if (!keywords.empty()) {
        std::string message = "The text pool named record kinds this reader does not import:";
        for (const auto& keyword : keywords) {
            message += ' ' + keyword;
        }
        report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning, message + "."});
    }
}

void rememberTextPoolName(std::vector<std::string>& list, std::string_view value)
{
    if (std::find(list.begin(), list.end(), value) == list.end()) {
        list.emplace_back(value);
    }
}

} // namespace

void importLaterTextPool(const ImportContext& context)
{
    FINALE_MUS_READER_TIMED_SCOPE(timing::Phase::TextLaterPool);
    // The Coda-banner epoch's text stream is length-prefixed chunks rather than
    // `^keyword(n) ... ^end` records. Its numbered blocks and lyrics use that framing,
    // while HS/HT blocks use record rows; walking the stream here would report a malformed pool.
    if (context.profile.epoch == FormatEpoch::CodaBanner) {
        return;
    }
    const auto stream = context.index.getTexts();
    if (stream.empty()) {
        return;
    }

    // Finale 2012 converted stored text to Unicode, which is the same boundary the option
    // records cross when a symbol codepoint widens from a word to a long. It is one change to
    // how the file stores characters, so it is asked for once rather than named again here.
    const text::EnigmaTextSource source{
        context.document, versions::storesUnicodeCodepoints(context.profile.version), context.profile.platform, nullptr};

    // The stream states which of the two framings it uses. The earliest one opens with a
    // `^text` section marker and terminates a record with the start of the next; Finale 97
    // drops the markers and closes each record with `^end`. Reading the opening bytes is what
    // keeps this off a version range, and off the epoch, which spans both.
    const bool terminated = sectionMarkerAt(stream, 0) == 0;

    std::vector<std::uint8_t> unknownCodes;
    std::vector<std::string> unknownEffects;
    std::vector<std::string> unknownKeywords;
    text::EnigmaFontResolutionCache fontResolutionCache;
    // Each cache has one initial-font context. The source bytes are therefore the complete
    // key: document, encoding, platform and default font stay fixed for the cache's lifetime.
    std::array<std::unordered_map<std::string_view, text::ConvertedEnigmaText>, std::size(textKeywords)> convertedByKeyword;
    std::array<std::shared_ptr<const musx::dom::FontInfo>, std::size(textKeywords)> initialFonts;
    std::array<bool, std::size(textKeywords)> initialFontsCached{};
    auto& textPool = *context.document->getTexts();
    std::size_t at = 0;
    while (at < stream.size()) {
        if (const auto marker = sectionMarkerAt(stream, at)) {
            at += marker;
            continue;
        }
        std::optional<RawRecord> record;
        {
            FINALE_MUS_READER_TIMED_SCOPE(timing::Phase::TextPoolFraming);
            record = readRecord(stream, at, terminated);
        }
        if (!record) {
            // Stopping is deliberate. The chunks are packed end to end with nothing between
            // them, so bytes that are not a chunk mean the stream is not a text pool, and
            // scanning ahead for the next plausible keyword would slice some other structure
            // into text blocks that were never there.
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Warning, "The text pool stopped making sense at offset " + std::to_string(at) + " of "
                                                            + std::to_string(stream.size()) + "; the text after that point was not imported."});
            break;
        }
        at = record->next;

        const auto found = std::find_if(
            std::begin(textKeywords), std::end(textKeywords), [&](const TextKeyword& entry) { return entry.keyword == record->keyword; });
        if (found == std::end(textKeywords)) {
            rememberTextPoolName(unknownKeywords, record->keyword);
            continue;
        }
        if (found->accepts && !found->accepts(record->number)) {
            rememberTextPoolName(unknownKeywords, std::string(record->keyword) + '(' + std::to_string(record->number) + ')');
            continue;
        }

        const auto keywordIndex = static_cast<std::size_t>(found - std::begin(textKeywords));
        if (!initialFontsCached[keywordIndex]) {
            FINALE_MUS_READER_TIMING_INCREMENT(timing::Counter::TextInitialFontCacheMisses, 1);
            initialFonts[keywordIndex] =
                found->defaultFontType ? musx::dom::options::FontOptions::getFontInfoOrNull(context.document, *found->defaultFontType) : nullptr;
            initialFontsCached[keywordIndex] = true;
        } else {
            FINALE_MUS_READER_TIMING_INCREMENT(timing::Counter::TextInitialFontCacheHits, 1);
        }
        auto recordSource = source;
        recordSource.initialFont = initialFonts[keywordIndex];
        recordSource.fontResolutionCache = &fontResolutionCache;
        FINALE_MUS_READER_TIMING_INCREMENT(timing::Counter::TextRecords, 1);
        FINALE_MUS_READER_TIMING_INCREMENT(timing::Counter::TextRecordBytes, record->body.size());
        text::ConvertedEnigmaText converted;
        {
            FINALE_MUS_READER_TIMED_SCOPE(timing::Phase::TextConversion);
            const std::string_view sourceText(reinterpret_cast<const char*>(record->body.data()), record->body.size());
            auto& cache = convertedByKeyword[keywordIndex];
            if (const auto cached = cache.find(sourceText); cached != cache.end()) {
                FINALE_MUS_READER_TIMING_INCREMENT(timing::Counter::TextCacheHits, 1);
                FINALE_MUS_READER_TIMING_INCREMENT(timing::Counter::TextCacheAvoidedBytes, record->body.size());
                converted = cached->second;
            } else {
                FINALE_MUS_READER_TIMING_INCREMENT(timing::Counter::TextCacheMisses, 1);
                converted = text::toModernEnigmaText(record->body, recordSource);
                cache.emplace(sourceText, converted);
            }
        }
        for (const auto code : converted.unreadCommandCodes) {
            if (std::find(unknownCodes.begin(), unknownCodes.end(), code) == unknownCodes.end()) {
                unknownCodes.push_back(code);
            }
        }
        for (const auto& effect : converted.unknownEffectNames) {
            rememberTextPoolName(unknownEffects, effect);
        }

        FINALE_MUS_READER_TIMED_SCOPE(timing::Phase::TextObjectConstruction);
        {
            FINALE_MUS_READER_TIMED_SCOPE(timing::Phase::TextReportConstruction);
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto instance = reporting.instanceKey(found->reportClass, musx::dom::SCORE_PARTID, record->number);
                reporting.report().setField(
                    instance, "text", {Reporting::Origin::LegacyMus, 0, record->start, static_cast<std::int64_t>(converted.text.size())});
                reporting.textField(instance, "text", converted);
            });
        }

        {
            FINALE_MUS_READER_TIMED_SCOPE(timing::Phase::TextDomInsertion);
            found->create(context.document, textPool, record->number, std::move(converted.text));
        }
    }

    reportUnread(context.report, unknownCodes, unknownEffects, unknownKeywords);
}

void importTexts(const ImportContext& context)
{
    // The physical readers are ordered so a text-pool FileInfoText wins over its older
    // header spelling; the header pass fills only types the pool did not provide. The Coda
    // pass is disjoint by epoch and leaves both later stores untouched.
    importLaterTextPool(context);
    {
        FINALE_MUS_READER_TIMED_SCOPE(timing::Phase::TextHeaderFileInfo);
        importHeaderFileInfoTexts(context);
    }
    {
        FINALE_MUS_READER_TIMED_SCOPE(timing::Phase::TextCodaStored);
        importCodaStoredTexts(context);
    }
}

} // namespace texts
} // namespace finale_mus_reader
