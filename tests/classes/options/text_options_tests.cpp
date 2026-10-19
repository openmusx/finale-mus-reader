// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

/// @brief A document whose TextOptions carries the pinned baseline's line spacing.
/// @details The baseline seeds the percent spelling, which is what a source stating the
/// absolute spelling has to displace. Seeding both would let an implementation that never
/// clears the baseline pass.
musx::dom::DocumentPtr makeTextOptionsDocument()
{
    using TextOptions = musx::dom::options::TextOptions;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<TextOptions>(document);
    options->textLineSpacingPercent = 100;
    options->tabSpaces = 4;
    options->textWordWrap = true;
    options->textExpandSingleWord = true;
    document->getOptions()->add(TextOptions::XmlNodeName, options);
    // The pinned baseline always carries all five inserts, and a source with no block of its
    // own is completed from them, so the stand-in must carry them too.
    using Insert = musx::dom::options::AccidentalInsertSymbolType;
    const std::array<std::tuple<Insert, int, int, char32_t>, 5> seeds{{{Insert::Sharp, 35, 34, U'#'}, {Insert::Flat, 60, 19, U'b'},
        {Insert::Natural, 50, 34, U'n'}, {Insert::DblSharp, 40, 34, U'Ü'}, {Insert::DblFlat, 60, 19, U'º'}}};
    for (const auto& [type, tracking, shift, character] : seeds) {
        auto insert = std::make_shared<TextOptions::InsertSymbolInfo>(options);
        insert->trackingBefore = tracking;
        insert->baselineShiftPerc = shift;
        insert->symChar = character;
        auto font = std::make_shared<musx::dom::FontInfo>(document, /*sizeIsPercent*/ true);
        font->fontSize = 100;
        insert->symFont = std::move(font);
        options->symbolInserts[type] = std::move(insert);
    }
    return std::move(session).finish();
}

// Selectors 5 and 13 are carried by every era; 81, 82 and 83 arrive with Finale 97. The reader
// decides that from the records present rather than from the version, so a document that
// predates them must recover the two old fields and leave the other eleven at the baseline.
// The two enums are checked here because Finale orders its alignment lists first, opposite,
// center while musxdom puts center second.
void testTextOptionsScalars()
{
    using TextOptions = musx::dom::options::TextOptions;
    const auto runImport = [](const std::vector<SyntheticRow>& rows, const SourceProfile& profile, ImportReport& report) {
        const auto document = makeTextOptionsDocument();
        const auto reference = makeTextOptionsDocument();
        const auto index = LegacyRecordIndex::build(makeContainer(rows));
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::options::importTextOptions(context);
        return document->getOptions()->get<TextOptions>();
    };

    // A Coda-banner document: the two old selectors and nothing else.
    {
        ImportReport report(FormatEpoch::UncompressedLegacy);
        auto coda = profileFor(2, 6);
        coda.epoch = FormatEpoch::CodaBanner;
        coda.version.reset();
        const auto options = runImport({{GLOBALS_CMPER, "05", {0, 0, 0, 0, 1, 2}}, {GLOBALS_CMPER, "13", {7, 0, 0, 0, 0, 0}}}, coda, report);
        expectMapping(options->showTimeSeconds && options->dateFormat == musx::dom::DateFormat::Abbrev && options->tabSpaces == 7,
            "The Coda-banner era did not recover the date stamp and tab spacing");
        expectMapping(options->textLineSpacingPercent.has_value() && options->textLineSpacingPercent.value() == 100
                          && !options->textLineSpacingEvpu.has_value(),
            "A document with no selector 82 lost the baseline line spacing");
        expectMapping(options->textJustify == TextOptions::TextJustify::Left && options->textVertAlign == TextOptions::VerticalAlignment::Top,
            "A document with no selector 83 did not keep the baseline alignment");
    }

    // Finale 97 onward, with the full set. Selector 82 states a percent, so the percent member
    // is engaged and the absolute one stays empty.
    {
        ImportReport report(FormatEpoch::UncompressedLegacy);
        const auto options = runImport(
            {
                {GLOBALS_CMPER, "05", {0, 0, 0, 0, 1, 1}},
                {GLOBALS_CMPER, "13", {7, 0, 0, 0, 0, 0}},
                {GLOBALS_CMPER, "81", {-1, -6, 0, 2016, -1, -3168}},
                {GLOBALS_CMPER, "82", {89, 1, 0, 11, 4, 1}},
                {GLOBALS_CMPER, "83", {1, 1, 0, 1, 0, 0}},
            },
            profileFor(3, 8), report);
        expectMapping(options->dateFormat == musx::dom::DateFormat::Long && options->tabSpaces == 7,
            "The Finale 97 era did not recover the date stamp and tab spacing");
        // Three 32-bit values in the framework's high-word-first order, two of them negative.
        expectMapping(options->textTracking == -6 && options->textBaselineShift == 2016 && options->textSuperscript == -3168,
            "The 32-bit text metrics were not assembled high word first");
        expectMapping(
            options->textLineSpacingPercent.has_value() && options->textLineSpacingPercent.value() == 89 && !options->textLineSpacingEvpu.has_value(),
            "A stated percent line spacing did not engage the percent member alone");
        expectMapping(
            !options->textWordWrap && options->textPageOffset == 11 && options->textExpandSingleWord, "Selector 82 was not read from its own slots");
        // Stored 4 is ForcedFull in both spellings; stored 1 is Right, which musxdom numbers 1
        // as well. The exchange is proved by the vertical alignment below.
        expectMapping(options->textJustify == TextOptions::TextJustify::ForcedFull
                          && options->textHorzAlign == TextOptions::HorizontalAlignment::Right && options->textIsEdgeAligned,
            "Selector 83 or the justification was not read from its own slots");
        expectMapping(options->textVertAlign == TextOptions::VerticalAlignment::Bottom, "A stored vertical alignment of 1 did not become Bottom");
    }

    // The same record with the two exchanged enum values, which is what separates Finale's
    // order from musxdom's: a stored 2 is center in both lists.
    {
        ImportReport report(FormatEpoch::UncompressedLegacy);
        const auto options = runImport(
            {
                {GLOBALS_CMPER, "82", {100, 1, 1, 0, 2, 1}},
                {GLOBALS_CMPER, "83", {0, 2, 0, 0, 0, 0}},
            },
            profileFor(3, 8), report);
        expectMapping(options->textJustify == TextOptions::TextJustify::Center, "A stored justification of 2 did not become Center");
        expectMapping(options->textVertAlign == TextOptions::VerticalAlignment::Center, "A stored vertical alignment of 2 did not become Center");
    }

    // Selector 82 word 1 clear: the same word 0 is an absolute distance, and the baseline's
    // percent must not survive beside it. musxdom's own integrity check rejects both engaged.
    {
        ImportReport report(FormatEpoch::UncompressedLegacy);
        const auto options = runImport({{GLOBALS_CMPER, "82", {72, 0, 1, 0, 0, 1}}}, profileFor(3, 8), report);
        expectMapping(
            options->textLineSpacingEvpu.has_value() && options->textLineSpacingEvpu.value() == 72 && !options->textLineSpacingPercent.has_value(),
            "A stated absolute line spacing did not displace the baseline percent");
        expectMapping(field(report, "options.textOptions.textLineSpacingEvpu").origin == ValueOrigin::LegacyMus,
            "The absolute line spacing was not reported as recovered");
    }

    // The zlib epoch reaches the same words through the numericGlobalClass rule, addressed by
    // byte offset. Both byte orders are exercised, because the 32-bit rule is one rule for
    // both: two 16-bit words with the high word first, each word in the container's order.
    const auto runClassImport = [](const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile, ImportReport& report) {
        const auto document = makeTextOptionsDocument();
        const auto reference = makeTextOptionsDocument();
        const auto index = LegacyRecordIndex::build(parsed);
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::options::importTextOptions(context);
        return document->getOptions()->get<TextOptions>();
    };

    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        auto profile = profileFor(16, 0);
        profile.epoch = FormatEpoch::ZlibLegacy;
        profile.byteOrder = byteOrder;

        ImportReport layoutReport(FormatEpoch::UncompressedLegacy);
        const auto layout =
            runClassImport(makeClassContainer(finale_mus_reader::numericGlobalClass(82), {137, 0, 0, 77, 3, 0}, byteOrder), profile, layoutReport);
        expectMapping(
            layout->textLineSpacingEvpu.has_value() && layout->textLineSpacingEvpu.value() == 137 && !layout->textLineSpacingPercent.has_value(),
            "The zlib epoch did not route line spacing by word 1");
        expectMapping(layout->textJustify == TextOptions::TextJustify::Full && layout->textPageOffset == 77 && !layout->textExpandSingleWord
                          && !layout->textWordWrap,
            "The zlib epoch did not read selector 82 from its own offsets");

        ImportReport metricsReport(FormatEpoch::UncompressedLegacy);
        const auto metrics = runClassImport(
            makeClassContainer(finale_mus_reader::numericGlobalClass(81), {-1, -6, 0, 2016, -1, -3168}, byteOrder), profile, metricsReport);
        expectMapping(metrics->textTracking == -6 && metrics->textBaselineShift == 2016 && metrics->textSuperscript == -3168,
            "The zlib epoch did not assemble the 32-bit metrics high word first");
    }
}

// The five accidental inserts are a direct block with three physical layouts. Each is built
// here from the bytes a real fixture of its era carries, so a layout that regressed would have
// to produce the same five characters from a different stride to pass.
void testTextOptionsSymbolInserts()
{
    using TextOptions = musx::dom::options::TextOptions;
    using Insert = musx::dom::options::AccidentalInsertSymbolType;
    const auto runImport = [](const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile, ImportReport& report) {
        const auto document = makeTextOptionsDocument();
        const auto reference = makeTextOptionsDocument();
        const auto index = LegacyRecordIndex::build(parsed);
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::options::importTextOptions(context);
        return document->getOptions()->get<TextOptions>();
    };
    // Every era stores the same five characters, which is what makes a layout error visible.
    const auto expectDefaults = [](const auto& options, const std::string& what) {
        const std::array<std::pair<Insert, char32_t>, 5> chars{
            {{Insert::Sharp, U'#'}, {Insert::Flat, U'b'}, {Insert::Natural, U'n'}, {Insert::DblSharp, U'Ü'}, {Insert::DblFlat, U'º'}}};
        for (const auto& [type, expected] : chars) {
            const auto found = options->symbolInserts.find(type);
            expectMapping(found != options->symbolInserts.end() && found->second && found->second->symChar == expected,
                what + " did not recover the stored insert characters");
        }
        expectMapping(options->symbolInserts.at(Insert::Sharp)->trackingBefore == 35 && options->symbolInserts.at(Insert::Flat)->trackingBefore == 50
                          && options->symbolInserts.at(Insert::Natural)->trackingBefore == 0
                          && options->symbolInserts.at(Insert::DblSharp)->trackingBefore == 40
                          && options->symbolInserts.at(Insert::DblFlat)->trackingBefore == 60,
            what + " did not recover the stored tracking");
        expectMapping(
            options->symbolInserts.at(Insert::Sharp)->baselineShiftPerc == 34 && options->symbolInserts.at(Insert::Flat)->baselineShiftPerc == 19,
            what + " did not recover the stored baseline shift");
        expectMapping(options->symbolInserts.at(Insert::Sharp)->symFont && options->symbolInserts.at(Insert::Sharp)->symFont->fontSize == 100,
            what + " did not recover the stored font size");
    };

    // Finale 3.7-2000: a 17-byte element with a one-byte character, read little-endian on a
    // big-endian file. These are the words a Finale 97 fixture carries.
    {
        ImportReport report(FormatEpoch::UncompressedLegacy);
        const std::vector<SyntheticRow> rows{
            {GLOBALS_CMPER, "78", {35, 0, 0, 0, 34, 0}},
            {GLOBALS_CMPER, "78", {100, 0, 12835, 0, 0, 0}},
            {GLOBALS_CMPER, "78", {4864, 0, 25600, 0, 25088, 0}},
            {GLOBALS_CMPER, "78", {0, 0, 0, 34, 0, 100}},
            {GLOBALS_CMPER, "78", {0, 10350, 0, 0, 0, 8704}},
            {GLOBALS_CMPER, "78", {0, 25600, 0, -9216, 60, 0}},
            {GLOBALS_CMPER, "78", {0, 0, 19, 0, 100, 0}},
            {GLOBALS_CMPER, "78", {186, 0, 0, 0, 0, 0}},
        };
        const auto options = runImport(makeContainer(rows), profileFor(3, 8), report);
        expectDefaults(options, "The 17-byte insert layout");
    }

    // Finale 2001-2010: an 18-byte element, container order, the character in a whole word of
    // which only the low byte counts. The double sharp and double flat are stored
    // sign-extended here, as four Finale 2006 fixtures do.
    {
        ImportReport report(FormatEpoch::UncompressedLegacy);
        const std::vector<SyntheticRow> rows{
            {GLOBALS_CMPER, "78", {0, 35, 0, 0, 34, 0}},
            {GLOBALS_CMPER, "78", {100, 0, 35, 0, 50, 0}},
            {GLOBALS_CMPER, "78", {0, 19, 0, 100, 0, 98}},
            {GLOBALS_CMPER, "78", {0, 0, 0, 0, 34, 0}},
            {GLOBALS_CMPER, "78", {100, 0, 110, 0, 40, 0}},
            {GLOBALS_CMPER, "78", {0, 34, 0, 100, 0, -36}},
            {GLOBALS_CMPER, "78", {0, 60, 0, 0, 19, 0}},
            {GLOBALS_CMPER, "78", {100, 0, -70, 0, 0, 0}},
        };
        auto profile = profileFor(12, 0);
        profile.epoch = FormatEpoch::DclLegacy;
        const auto options = runImport(makeContainer(rows, FormatEpoch::DclLegacy), profile, report);
        expectDefaults(options, "The 18-byte insert layout");
    }

    // Finale 2012: a 20-byte element with the character widened to a long. Each element is ten
    // words: the two trackings high word first, then the shift, font tuple and character.
    {
        std::vector<std::int16_t> words;
        const auto element = [&words](std::int16_t tb, std::int16_t bsp, std::int16_t chr) {
            for (const std::int16_t value : {std::int16_t(0), tb, std::int16_t(0), std::int16_t(0), bsp, std::int16_t(0), std::int16_t(100),
                     std::int16_t(0), chr, std::int16_t(0)}) {
                words.push_back(value);
            }
        };
        element(35, 34, 35);
        element(50, 19, 98);
        element(0, 34, 110);
        element(40, 34, 220);
        element(60, 19, 186);
        words.insert(words.end(), 4, 0);

        ImportReport report(FormatEpoch::UncompressedLegacy);
        auto profile = profileFor(16, 0);
        profile.epoch = FormatEpoch::ZlibLegacy;
        profile.byteOrder = ByteOrder::LittleEndian;
        const auto options =
            runImport(makeClassContainer(finale_mus_reader::numericGlobalClass(78), words, ByteOrder::LittleEndian), profile, report);
        expectDefaults(options, "The 20-byte insert layout");

        // A character above the basic multilingual plane is the only value that can tell the
        // two candidate word orders apart, and it says the character is a plain little-endian
        // long: U+26469 is stored low word first, where the high-word-first order the two
        // trackings use would give 0x64690002 and no codepoint at all. Taken from
        // tests/evidence/F2012/F2012-dblsharp-insert-outside-BMP.
        auto astral = words;
        const std::size_t dblSharpChar = 3 * 10 + 8;
        astral[dblSharpChar] = static_cast<std::int16_t>(0x6469);
        astral[dblSharpChar + 1] = 2;
        ImportReport astralReport(FormatEpoch::UncompressedLegacy);
        const auto withAstral =
            runImport(makeClassContainer(finale_mus_reader::numericGlobalClass(78), astral, ByteOrder::LittleEndian), profile, astralReport);
        expectMapping(withAstral->symbolInserts.at(Insert::DblSharp)->symChar == U'\U00026469',
            "A symbol character above the basic multilingual plane was not read low word first");
    }

    // A source with no insert block at all -- every Coda-banner and Finale 3.0-3.5 document.
    // All five come from the pinned baseline and must say so, and the font must be cloned into
    // this document rather than shared with the reference.
    {
        ImportReport report(FormatEpoch::UncompressedLegacy);
        auto coda = profileFor(2, 6);
        coda.epoch = FormatEpoch::CodaBanner;
        coda.version.reset();
        const auto options = runImport(makeContainer({{GLOBALS_CMPER, "13", {7, 0, 0, 0, 0, 0}}}, FormatEpoch::CodaBanner), coda, report);
        expectMapping(options->symbolInserts.size() == 5, "A source with no insert block did not receive the baseline's five inserts");
        expectMapping(options->symbolInserts.at(Insert::Sharp)->symFont != nullptr, "A completed insert has no font");
        expectMapping(field(report, "options.textOptions.symbolInserts[sharp].symChar").origin == ValueOrigin::Finale27Default,
            "A completed insert was not reported as a synthesized default");
    }
}

TEST_CASE("Text options scalars", "[class]")
{
    testTextOptionsScalars();
}
TEST_CASE("Text options symbol inserts", "[class]")
{
    testTextOptionsSymbolInserts();
}

} // namespace
} // namespace finale_mus_reader_tests
