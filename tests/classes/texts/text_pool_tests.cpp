// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

// Texts pool recovery: the `^keyword(n) ... ^end` stream, the expression text the earlier
// eras embed in their own records, and the File Info strings in the header.
//
// Fixture cases assert exact strings rather than "something was recovered", because the whole
// value of this class is that the characters and the commands come out right. Synthetic cases
// cover malformed streams, unknown keywords, and command codes with no known spelling.

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "container/mus_container.h"
#include "import/support/enigma_text.h"
#include "import/support/legacy_mapping.h"
#include "import/texts.h"
#include "records/legacy_record_index.h"

#include "finale_mus_reader/reader.h"
#include "musx/factory/DocumentFactory.h"
#include "musx/musx.h"

#ifndef MUSX_USE_PUGIXML
#define MUSX_USE_PUGIXML
#define FINALE_MUS_READER_TEXT_POOL_UNDEFINE_PUGIXML
#endif // !defined(MUSX_USE_PUGIXML)
#include "musx/xml/PugiXmlImpl.h"
#ifdef FINALE_MUS_READER_TEXT_POOL_UNDEFINE_PUGIXML
#undef MUSX_USE_PUGIXML
#undef FINALE_MUS_READER_TEXT_POOL_UNDEFINE_PUGIXML
#endif // defined(FINALE_MUS_READER_TEXT_POOL_UNDEFINE_PUGIXML)

namespace {

using finale_mus_reader::ByteOrder;
using finale_mus_reader::FormatEpoch;
using finale_mus_reader::ImportReport;
using finale_mus_reader::ImportResult;
using finale_mus_reader::Reader;
using finale_mus_reader::SourcePlatform;
using finale_mus_reader::SourceProfile;
using finale_mus_reader::ValueOrigin;
using finale_mus_reader::records::LegacyRecordIndex;
using TextPoolXmlDocument = musx::xml::pugi::Document;

void expectText(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ImportResult readTextFixture(const char* relative)
{
    return Reader::readWithReport<TextPoolXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / relative);
}

template <typename Target>
std::string textOf(const ImportResult& result, musx::dom::Cmper number)
{
    const auto instance = result.document->getTexts()->get<Target>(number);
    return instance ? instance->text : std::string("<absent>");
}

template <typename Target>
std::size_t countOf(const ImportResult& result)
{
    return result.document->getTexts()->getArray<Target>().size();
}

template <typename Target>
const finale_mus_reader::FieldInfo& textField(const ImportResult& result, musx::dom::Cmper cmper, std::string_view member)
{
    const auto* info = result.report.findField<Target>(member, musx::dom::SCORE_PARTID, cmper);
    if (info) {
        return *info;
    }
    throw std::runtime_error("no report entry for " + std::string(member));
}

// -- synthetic stream support ------------------------------------------------------------

finale_mus_reader::container::ParsedContainer makeTextContainer(std::string_view stream)
{
    finale_mus_reader::container::ParsedContainer parsed(FormatEpoch::UncompressedLegacy);
    parsed.byteOrder = ByteOrder::BigEndian;
    finale_mus_reader::container::DecodedBlock block;
    block.info.type = 0x0004;
    block.data.assign(stream.begin(), stream.end());
    block.info.decodedSize = block.data.size();
    parsed.blocks.push_back(std::move(block));
    return parsed;
}

/// @brief A document holding one named font, so a font command has something to resolve to.
musx::dom::DocumentPtr makeTextDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    auto document = session.getDocument();
    auto font = std::make_shared<musx::dom::others::FontDefinition>(
        document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper(4));
    font->name = "Times";
    font->charsetBank = musx::dom::others::FontDefinition::CharacterSetBank::MacOS;
    font->charsetVal = 0;
    document->getOthers()->add(musx::dom::others::FontDefinition::XmlNodeName, font);

    auto japaneseFont = std::make_shared<musx::dom::others::FontDefinition>(
        document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper(9));
    japaneseFont->name = "ヒラギノ明朝 Pro W3";
    japaneseFont->charsetBank = musx::dom::others::FontDefinition::CharacterSetBank::MacOS;
    japaneseFont->charsetVal = 1;
    document->getOthers()->add(musx::dom::others::FontDefinition::XmlNodeName, japaneseFont);

    auto unspellableFont = std::make_shared<musx::dom::others::FontDefinition>(
        document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper(10));
    unspellableFont->name = "p^sharp(";
    unspellableFont->charsetBank = musx::dom::others::FontDefinition::CharacterSetBank::MacOS;
    unspellableFont->charsetVal = 0;
    document->getOthers()->add(musx::dom::others::FontDefinition::XmlNodeName, unspellableFont);
    return document;
}

struct SyntheticImport
{
    SyntheticImport()
        : report(FormatEpoch::UncompressedLegacy)
    {}

    musx::dom::DocumentPtr document;
    ImportReport report;
};

template <typename Target>
std::string textOf(const SyntheticImport& result, musx::dom::Cmper number)
{
    const auto instance = result.document->getTexts()->get<Target>(number);
    return instance ? instance->text : std::string("<absent>");
}

/// @brief Runs the text pool importer alone over a synthetic stream.
SyntheticImport importStream(std::string_view stream)
{
    SyntheticImport result;
    result.document = makeTextDocument();
    const auto reference = makeTextDocument();
    const auto index = LegacyRecordIndex::build(makeTextContainer(stream));
    SourceProfile profile(FormatEpoch::UncompressedLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    profile.platform = SourcePlatform::MacOS;
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{
        index, profile, std::span<const std::uint8_t>{}, result.document, reference, result.report, pending, construction};
    finale_mus_reader::texts::importTexts(context);
    return result;
}

bool hasDiagnosticContaining(const ImportReport& report, std::string_view fragment)
{
    return std::any_of(report.diagnostics.begin(), report.diagnostics.end(),
        [&](const finale_mus_reader::Diagnostic& diagnostic) { return diagnostic.message.find(fragment) != std::string::npos; });
}

// -- cases -------------------------------------------------------------------------------

// The uncompressed epoch spells its commands out, so this is where `^efx` becomes `^nfx` and
// where a carriage return becomes a line feed. Every string below is what Finale 27's own
// conversion of the same file writes, character for character.
void testUncompressedTextPool()
{
    using namespace musx::dom::texts;
    const auto result = readTextFixture("evidence/F97/F97-fileinfo-short.mus");
    expectText(result.report.formatEpoch == FormatEpoch::UncompressedLegacy, "Finale 97 fixture was not classified as uncompressed");

    expectText(textOf<BlockText>(result, 1) == "^font(Times)^size(12)^nfx(67)^page(0)",
        "An `^efx` run did not become one `^nfx` with the bold, italic and absolute bits");
    expectText(textOf<BlockText>(result, 3) == "^font(Times)^size(24)^nfx(65)^title()", "A block text carrying an insert was not recovered exactly");
    expectText(textOf<BlockText>(result, 5)
                   == "^font(Times)^size(9)^nfx(64)Licensed by ASCAP\nOne Lincoln Plaza\n"
                      "New York, NY  10023",
        "Legacy carriage returns did not become line feeds");
    expectText(textOf<BlockText>(result, 9)
                   == "^font(Times)^size(9)^nfx(64)©^copyright() Great River Music\n"
                      "All rights reserved.",
        "A non-ASCII character in a text font was not decoded through its code page");
    expectText(countOf<BlockText>(result) == 6, "Finale 97 fixture should carry exactly six block texts");

    // Nothing in this document is a lyric or a smart shape, so an importer that invented
    // objects would show up here.
    expectText(
        countOf<LyricsVerse>(result) == 0 && countOf<SmartShapeText>(result) == 0, "Objects were created for classes the source does not contain");

    expectText(textField<BlockText>(result, musx::dom::Cmper(1), "text").origin == ValueOrigin::LegacyMus,
        "A recovered block text was not reported as recovered");
}

// File Info lives in the header rather than in any pool, and the long variant runs past the
// customary 0x200 boundary, so the pair also tests that the read is bounded by the body
// offset the header itself states rather than by a constant.
void testFileInfoText()
{
    using musx::dom::texts::FileInfoText;
    const auto shortInfo = readTextFixture("evidence/F97/F97-fileinfo-short.mus");
    const auto longInfo = readTextFixture("evidence/F97/F97-fileinfo-long.mus");
    constexpr std::string_view initial = "^font(Times)^size(14)^nfx(2)";

    expectText(textOf<FileInfoText>(shortInfo, musx::dom::Cmper(FileInfoText::TextType::Title)) == std::string(initial) + "File Info Short",
        "The header title was not recovered");
    expectText(textOf<FileInfoText>(shortInfo, musx::dom::Cmper(FileInfoText::TextType::Composer)) == std::string(initial) + "Robert Patterson",
        "The header composer was not recovered");
    expectText(textOf<FileInfoText>(shortInfo, musx::dom::Cmper(FileInfoText::TextType::Copyright)) == std::string(initial) + "2002",
        "The header copyright was not recovered");
    expectText(textOf<FileInfoText>(shortInfo, musx::dom::Cmper(FileInfoText::TextType::Description))
                   == std::string(initial) + "This is the end of the description.",
        "The header description was not recovered");
    expectText(countOf<FileInfoText>(shortInfo) == 4, "Only the four located File Info fields should produce objects");

    const auto longDescription = textOf<FileInfoText>(longInfo, musx::dom::Cmper(FileInfoText::TextType::Description));
    expectText(longDescription.size() > 0x200, "The long description was truncated at the customary header size");
    expectText(longDescription.rfind("This is the end of the file info")
                   == longDescription.size() - std::string_view("This is the end of the file info").size(),
        "The long description did not run to its own terminator");
    expectText(textOf<FileInfoText>(longInfo, musx::dom::Cmper(FileInfoText::TextType::Title)) == std::string(initial) + "File Info Long",
        "The long variant's title was not recovered");

    // A document that filled nothing in should produce no objects at all, which is also what
    // Finale 27 writes for the same file.
    const auto blank = readTextFixture("evidence/F2000/F2000-multilayer.mus");
    expectText(countOf<FileInfoText>(blank) == 0, "Empty header slots should not create File Info objects");
}

void testEarlyExpressionTextSynthesis()
{
    using musx::dom::texts::ExpressionText;
    for (const char* fixture : {"evidence/F97/F97-fileinfo-short.mus", "evidence/F2000/F2000-multilayer.mus", "evidence/F263/F263-baseline.mus"}) {
        const auto result = readTextFixture(fixture);
        const auto definitions = result.document->getOthers()->getArray<musx::dom::others::TextExpressionDef>(musx::dom::SCORE_PARTID);
        expectText(
            countOf<ExpressionText>(result) == definitions.size(), std::string(fixture) + " did not synthesize one text per expression definition");
    }

    // Pooled text retains its source numbering and count.
    const auto pooled = readTextFixture("evidence/F2006/F2006-embedded-tiff.mus");
    expectText(countOf<ExpressionText>(pooled) == 41, "Pooled expression text was lost during definition import");
}

// The compressed epochs write their commands in the binary form, so this is where the code
// table and the digit encoding are exercised.
void testCompressedTextPool()
{
    using namespace musx::dom::texts;
    const auto measureText = readTextFixture("evidence/F2001/F2001Win-meastext.mus");
    expectText(measureText.report.formatEpoch == FormatEpoch::DclLegacy && measureText.report.byteOrder == ByteOrder::LittleEndian,
        "Windows Finale 2001 fixture was not classified as little-endian DCL");
    expectText(textOf<BlockText>(measureText, 1) == "^font(Times New Roman)^size(12)^nfx(0)Measure-attachéd text: €2.99",
        "Windows-1252 block text was not recovered from Finale 2001");

    const auto sectionLyric = readTextFixture("evidence/F2001/F2001Win-section-lyric.mus");
    expectText(textOf<LyricsSection>(sectionLyric, 1) == "^font(Times New Roman)^size(12)^nfx(0)sec-tion ly-ric",
        "A section lyric was not recovered from the DCL text pool");

    const auto dcl = readTextFixture("evidence/F2006/F2006-single-title.mus");
    expectText(dcl.report.formatEpoch == FormatEpoch::DclLegacy, "Finale 2006 fixture was not classified as DCL");
    expectText(textOf<BlockText>(dcl, 1) == "^font(Times)^size(12)^nfx(0)TEST", "Binary font, size and style commands were not spelled out");
    const auto titleBlock = dcl.document->getOthers()->get<musx::dom::others::TextBlock>(musx::dom::SCORE_PARTID, 1);
    expectText(titleBlock && titleBlock->textType == musx::dom::others::TextBlock::TextType::Block
                   && textField<musx::dom::others::TextBlock>(dcl, musx::dom::Cmper(1), "textType").rawValue == 2004,
        "The Finale 2006 block-text discriminator was not recovered");

    const auto mixed = readTextFixture("evidence/F2006/F2006-embedded-tiff.mus");
    expectText(countOf<SmartShapeText>(mixed) == 60, "Smart shape texts were not recovered from the DCL text pool");
    expectText(countOf<ExpressionText>(mixed) == 41, "Expression texts were not recovered from the DCL text pool");
    // One record that mixes both spellings: a named font command, then binary size and style.
    expectText(textOf<BlockText>(mixed, 9) == "^font(Times)^size(14)^nfx(0)Clarinet in B^size(20)^flat()",
        "A block text mixing a named font with later commands was not recovered exactly");
    expectText(textOf<BlockText>(mixed, 13) == "^font(Times)^size(24)^nfx(1)^title()",
        "A block text written entirely in binary commands was not recovered exactly");
    expectText(textOf<SmartShapeText>(mixed, 3) == "^font(Maestro)^size(24)^nfx(0)¡", "A symbol-font smart shape character was decoded as text");
    const auto expressionBlock = mixed.document->getOthers()->get<musx::dom::others::TextBlock>(musx::dom::SCORE_PARTID, 52);
    expectText(expressionBlock && expressionBlock->textId == 35 && expressionBlock->textType == musx::dom::others::TextBlock::TextType::Expression
                   && textField<musx::dom::others::TextBlock>(mixed, musx::dom::Cmper(52), "textType").rawValue == 2006,
        "The Finale 2006 expression-text discriminator was not recovered");

    // Every supported binary command code appears once, so this checks the complete table.
    const auto inserts = readTextFixture("evidence/F2006/F2006-text-inserts.mus");
    struct ExpectedInsert
    {
        musx::dom::Cmper number;
        const char* text;
    };
    constexpr ExpectedInsert expectedBlocks[] = {
        {1, "^font(Times)^size(12)^nfx(0)^title()Title"},
        {2, "^font(Times)^size(12)^nfx(0)^composer() Composer"},
        {3, "^font(Times)^size(12)^nfx(0)^date(1) Current Date"},
        {4, "^font(Times)^size(12)^nfx(0)^fdate(0) File Date"},
        {5, "^font(Times)^size(12)^nfx(0)^page(0) Page Number"},
        // Eight digits holding a non-zero value: the case that shows the whole run is one
        // argument rather than two of four digits each.
        {6, "^font(Times)^size(12)^nfx(0)^page(275) Page Number With Offset"},
        {7, "^font(Times)^size(12)^nfx(0)^filename() File Name"},
        {8, "^font(Times)^size(12)^nfx(0)^fdate(2) File Date"},
        {9, "^font(Times)^size(12)^nfx(0)^cprsym() Copyright Symbol"},
        {10, "^font(Times)^size(12)^nfx(0)^copyright() Copyright Text"},
        {11, "^font(Times)^size(12)^nfx(0)^description() Description"},
        {12, "^font(Times)^size(12)^nfx(0)^sharp() Sharp"},
        {13, "^font(Times)^size(12)^nfx(0)^flat() Flat"},
        {14, "^font(Times)^size(12)^nfx(0)^natural() Natural"},
        {15, "^font(Times)^size(12)^nfx(0)^dbsharp() Double Sharp"},
        {16, "^font(Times)^size(12)^nfx(0)^dbflat() Double Flat"},
        {17, "^font(Times)^size(12)^nfx(0)^totpages() Total Pages"},
        {18, "^font(Times)^size(12)^nfx(0)^perftime(4) Performance Time"},
        // The three style commands are the only ones with an eight-digit argument besides
        // `^page`, and the only place a negative value appears. `0xfffffff3` is -13, not
        // 4294967283, and the same records carry a nibble of 0xf: the
        // `^superscript(15)` below is spelled `01 01 01 01 01 01 01 10`.
        {19, "^font(Times)^size(12)^nfx(0)^baseline(-13)baseline -13 ^baseline(0)0"},
        {20, "^font(Times)^size(12)^nfx(0)^tracking(7) tracking 7 ^tracking(0)0"},
        {21, "^font(Times)^size(12)^nfx(0)^superscript(15)superscript 15 ^superscript(-13)-13"},
    };
    for (const auto& expected : expectedBlocks) {
        expectText(textOf<BlockText>(inserts, expected.number) == expected.text,
            "Insert block " + std::to_string(expected.number) + " read as \"" + textOf<BlockText>(inserts, expected.number) + "\"");
    }
    constexpr ExpectedInsert expectedExpressions[] = {
        {1, "^font(Times)^size(12)^nfx(0)^value() Value Number"},
        {2, "^font(Times)^size(12)^nfx(0)^control() Controller Number"},
        {3, "^font(Times)^size(12)^nfx(0)^pass()Pass Number"},
    };
    for (const auto& expected : expectedExpressions) {
        expectText(textOf<ExpressionText>(inserts, expected.number) == expected.text,
            "Insert expression " + std::to_string(expected.number) + " read as \"" + textOf<ExpressionText>(inserts, expected.number) + "\"");
    }
    // Nothing in that document should have been dropped for want of a spelling.
    expectText(std::none_of(inserts.report.diagnostics.begin(), inserts.report.diagnostics.end(),
                   [](const finale_mus_reader::Diagnostic& diagnostic) { return diagnostic.message.find("could not read") != std::string::npos; }),
        "A command code in the insert fixture was not read");

    const auto zlib = readTextFixture("evidence/F2007/F2007-lyric-hyphens.mus");
    expectText(zlib.report.formatEpoch == FormatEpoch::ZlibLegacy, "Finale 2007 fixture was not classified as zlib");
    expectText(textOf<LyricsVerse>(zlib, 1) == "^font(Times)^size(12)^nfx(0)a-test", "A lyric verse was not recovered from the zlib text pool");
    const auto verse = zlib.document->getTexts()->get<LyricsVerse>(1);
    expectText(verse && !verse->syllables.empty(), "musxdom's own resolver did not build syllable information for the recovered verse");

    // Finale 2012 stores the pool in UTF-8, so the same command codes arrive as their two-byte
    // encoding. Reading both spellings is what keeps one code table serving every era.
    const auto unicode = readTextFixture("evidence/F2012/F2012-baseline.mus");
    expectText(textOf<BlockText>(unicode, 1) == "^font(Times New Roman)^size(12)^nfx(0)Score", "A UTF-8 era binary command was not recognized");
    const auto unicodeBlock = unicode.document->getOthers()->get<musx::dom::others::TextBlock>(musx::dom::SCORE_PARTID, 1);
    expectText(unicodeBlock && !unicodeBlock->roundCorners && unicodeBlock->cornerRadius == 0
                   && textField<musx::dom::others::TextBlock>(unicode, musx::dom::Cmper(1), "roundCorners").origin == ValueOrigin::LegacyBehavior
                   && textField<musx::dom::others::TextBlock>(unicode, musx::dom::Cmper(1), "cornerRadius").origin == ValueOrigin::LegacyBehavior,
        "The final legacy MUS era did not retain square-corner TextBlock behavior");
}

// Finale 2008 is where the text pool takes over File Info, where four inserts are appended
// past the alphabetical run, and where the one-digit argument appears. It is also the only
// big-endian specimen of its era anywhere.
void testFinale2008Inserts()
{
    using namespace musx::dom::texts;
    const auto result = readTextFixture("evidence/F2008/F2008-BE-text-inserts.mus");
    expectText(result.report.formatEpoch == FormatEpoch::ZlibLegacy && result.report.byteOrder == finale_mus_reader::ByteOrder::BigEndian,
        "The Finale 2008 fixture was not classified as big-endian zlib");

    // All seven File Info types, from the pool rather than the header, keyed by musxdom's own
    // enumeration. The header offsets are empty in this file, so nothing here can have come
    // from them.
    const char* expectedInfo[] = {"^font(Times)^size(12)^nfx(0)File Info Title", "^font(Times)^size(12)^nfx(0)File Info Composer",
        "^font(Times)^size(12)^nfx(0)File Info Copyright", "^font(Times)^size(12)^nfx(0)File Info Description",
        "^font(Times)^size(12)^nfx(0)File Info Lyricist", "^font(Times)^size(12)^nfx(0)File Info Arranger",
        "^font(Times)^size(12)^nfx(0)File Info Subtitle"};
    for (musx::dom::Cmper type = 1; type <= 7; ++type) {
        expectText(textOf<FileInfoText>(result, type) == expectedInfo[type - 1],
            "File Info type " + std::to_string(type) + " read as \"" + textOf<FileInfoText>(result, type) + "\"");
    }
    expectText(countOf<FileInfoText>(result) == 7, "The pool should have supplied exactly seven File Info objects");
    const auto firstBlock = result.document->getOthers()->get<musx::dom::others::TextBlock>(musx::dom::SCORE_PARTID, 1);
    expectText(firstBlock && firstBlock->textType == musx::dom::others::TextBlock::TextType::Block
                   && textField<musx::dom::others::TextBlock>(result, musx::dom::Cmper(1), "textType").rawValue == 2004,
        "The zlib TextBlock family discriminator was not recovered");

    // The four inserts appended after the alphabetical run.
    expectText(textOf<BlockText>(result, 2) == "^font(Times)^size(12)^nfx(0)^subtitle() Subtitle"
                   && textOf<BlockText>(result, 3) == "^font(Times)^size(12)^nfx(0)^arranger() Arranger"
                   && textOf<BlockText>(result, 4) == "^font(Times)^size(12)^nfx(0)^lyricist() Lyricist"
                   && textOf<BlockText>(result, 5) == "^font(Times)^size(12)^nfx(0)^partname() Partname",
        "An insert appended past the alphabetical run was not recovered");

    // The one-digit argument, and the one insert Finale 27 discards rather than converts.
    expectText(textOf<BlockText>(result, 6) == "^font(Times)^size(12)^nfx(0)^time(0) Time"
                   && textOf<BlockText>(result, 7) == "^font(Times)^size(12)^nfx(0)^time(1) Time with seconds",
        "The one-digit time argument was not read");

    // A block with no style commands receives the document's TextBlock default so that its
    // first literal begins under a complete Enigma formatting state.
    expectText(textOf<BlockText>(result, 1) == "^font(Times)^size(12)^nfx(0)FULL SCORE",
        "A block text with no style commands did not receive its default state");

    expectText(result.report.diagnostics.end()
                   == std::find_if(result.report.diagnostics.begin(), result.report.diagnostics.end(),
                       [](const finale_mus_reader::Diagnostic& diagnostic) {
                           return diagnostic.message.find("could not read") != std::string::npos
                                  || diagnostic.message.find("does not import") != std::string::npos;
                       }),
        "The Finale 2008 fixture still contains something this reader cannot read");
}

// Finale 2011, for the last four commands: the three that name a marking category's font and
// the automatic rehearsal mark. Finale 2009 introduced categories and Finale 2010 the
// rehearsal insert, so this is the earliest available release that can write all four.
void testFinale2011CategoryFonts()
{
    using namespace musx::dom::texts;
    const auto result = readTextFixture("evidence/F2011/F2011-text-inserts.mus");
    expectText(result.report.formatEpoch == FormatEpoch::ZlibLegacy, "The Finale 2011 fixture was not classified as zlib");

    // The three categorized font commands. Each stores a comparator, and each keeps its own
    // spelling because the marking category it names is the one thing `^fontid` cannot say.
    // The stored arguments are 9, 11 and 11 and resolve to two font faces.
    expectText(textOf<ExpressionText>(result, 1) == "^fontTxt(Times New Roman)^size(14)^nfx(1)Category Text Font"
                   && textOf<ExpressionText>(result, 2) == "^fontMus(EngraverTextT)^size(12)^nfx(0)Category Music Font"
                   && textOf<ExpressionText>(result, 3) == "^fontNum(EngraverTextT)^size(12)^nfx(0)Category Number Font",
        "A categorized font command was not resolved to its font definition");

    // `^rehearsal` takes no argument, and this record is what shows it: the byte after the code
    // is the leading space of " Rehearsal", which is not a digit byte and so cannot be one.
    expectText(textOf<ExpressionText>(result, 4) == "^fontTxt(Times New Roman)^size(12)^nfx(0)^rehearsal() Rehearsal",
        "The rehearsal insert was not recovered without an argument");

    expectText(textOf<BlockText>(result, 1) == "^font(Times New Roman)^size(12)^nfx(0)Score", "The Finale 2011 block text was not recovered");

    expectText(
        result.report.diagnostics.end()
            == std::find_if(result.report.diagnostics.begin(), result.report.diagnostics.end(),
                [](const finale_mus_reader::Diagnostic& diagnostic) { return diagnostic.message.find("could not read") != std::string::npos; }),
        "The Finale 2011 fixture still contains a command this reader cannot read");
}

// The earliest text pool divides itself into `^text` and `^lyrics` sections and terminates a
// record with the start of the next, where Finale 97 drops the markers and closes each record
// with `^end`. The stream says which framing it uses, so no version gate is involved.
void testEarlyTextPoolFraming()
{
    using namespace musx::dom::texts;
    const auto result = readTextFixture("evidence/F372/F372-fileinfo-text.mus");
    expectText(result.report.formatEpoch == FormatEpoch::UncompressedLegacy, "The Finale 3.7.2 fixture was not classified as uncompressed");

    // The `^text` section, whose last record ends at the marker opening the next section
    // rather than at another record.
    expectText(textOf<BlockText>(result, 1) == "^font(Times)^size(12)^nfx(0)^composer() Composer"
                   && textOf<BlockText>(result, 2) == "^font(Times)^size(12)^nfx(0)^page(511) Page",
        "An unterminated block text was not recovered: " + textOf<BlockText>(result, 2));
    expectText(countOf<BlockText>(result) == 2, "A section marker was read as a record of its own");

    // The `^lyrics` section. Its comparators are the document's own: neither sequential nor
    // unique across kinds, and its last record ends at the end of the stream.
    expectText(textOf<LyricsVerse>(result, 2) == "^font(Times)^size(12)^nfx(0)ly-ric verse two"
                   && textOf<LyricsChorus>(result, 3) == "^font(Times)^size(12)^nfx(0)Cho-rus ly-ric 3"
                   && textOf<LyricsSection>(result, 4) == "^font(Times)^size(12)^nfx(0)Sec-tion ly-ric 4",
        "A record of the lyrics section was not recovered");
    expectText(countOf<LyricsSection>(result) == 3, "The three sections of the fixture were not all recovered");

    // Section 2 states its style twice over. Both runs are kept: they are what the document
    // says and musxdom applies them in order to the same effect. Finale 27 writes only one.
    expectText(textOf<LyricsSection>(result, 2) == "^font(Times)^size(12)^nfx(0)^font(Times)^size(12)^nfx(0)Sec-tion ly-ric 4",
        "A repeated style run was not preserved: " + textOf<LyricsSection>(result, 2));

    // File Info is in the header at this release, which is the earliest whose dialog offers it.
    const char* expected[] = {"^font(Times)^size(12)^nfx(0)File Info Title", "^font(Times)^size(12)^nfx(0)File Info Composer",
        "^font(Times)^size(12)^nfx(0)File Info Copyright", "^font(Times)^size(12)^nfx(0)File Info Description"};
    for (musx::dom::Cmper type = 1; type <= 4; ++type) {
        expectText(textOf<FileInfoText>(result, type) == expected[type - 1],
            "File Info type " + std::to_string(type) + " read as \"" + textOf<FileInfoText>(result, type) + "\"");
    }

    // A document of the same release with both sections empty is the bare bytes
    // `^text^lyrics`. It must produce nothing, and must not report the markers as a stream
    // that stopped making sense.
    const auto empty = readTextFixture("evidence/F372/F372-baseline.mus");
    expectText(countOf<BlockText>(empty) == 0 && countOf<LyricsVerse>(empty) == 0, "An empty early text pool produced records");
    expectText(
        !hasDiagnosticContaining(empty.report, "stopped making sense"), "The section markers of an empty pool were reported as a malformed stream");
}

// Bookmark text is recovered only where the source keeps it in the text pool. Before that it is
// in the `BK` others family, and reading it is deferred on the same footing as expression text:
// the bookmark class itself is not imported, and text with no bookmark behind it claims more
// coverage than it has.
void testBookmarkText()
{
    using musx::dom::texts::BookmarkText;

    // Finale 2012 pools it, `^end`-terminated like every other record of that era, and the
    // text is UTF-8: the guillemets and the u-umlaut are two bytes each in the source.
    //
    // The recovered text is the characters and nothing else. A `^bookmark` chunk carries no
    // font commands, so no initial formatting state is synthesized for this class where every
    // other text class gets one.
    const auto pooled = readTextFixture("evidence/F2012/F2012-bookmarks.mus");
    expectText(textOf<BookmarkText>(pooled, 2) == "Page \u00fcber" && textOf<BookmarkText>(pooled, 3) == "Scroll \u00ab\u00bb Bookmark",
        "A pooled bookmark was not recovered: " + textOf<BookmarkText>(pooled, 2));
    expectText(countOf<BookmarkText>(pooled) == 2, "The two bookmarks of the fixture were not both recovered");

    // The same two bookmarks in the release they were authored in, where the text pool holds
    // none of them and the names come from the `BK` others family instead.
    const auto earlier = readTextFixture("evidence/F372/F372-bookmarks.mus");
    expectText(countOf<BookmarkText>(earlier) == 2,
        "The two bookmarks of the earlier fixture were not both synthesized from BK: " + std::to_string(countOf<BookmarkText>(earlier)));
    // The 8-bit name goes through the source platform's code page, so the same two characters
    // that are two bytes each in the Finale 2012 pool are one byte each here: `9f` is the
    // u-umlaut under Mac Roman and `c7 c8` the guillemet pair. Recovering the identical strings
    // from the two eras is what measures the conversion.
    expectText(textOf<BookmarkText>(earlier, 1) == "Scroll \u00ab\u00bb Bookmark" && textOf<BookmarkText>(earlier, 2) == "Page \u00fcber",
        "A BK bookmark name was not recovered: [" + textOf<BookmarkText>(earlier, 1) + "] [" + textOf<BookmarkText>(earlier, 2) + "]");

    // Finale 2008 keeps the same 36-word record as class 0x007b of the zlib pool, name and all,
    // so the era between the two is recovered from the record rather than from the pool. Six
    // bookmarks, whose names are what distinguishes them here.
    const auto zlibNamed = readTextFixture("evidence/F2008/F2008-bookmarks.mus");
    expectText(countOf<BookmarkText>(zlibNamed) == 6,
        "The six bookmarks of the Finale 2008 fixture were not all recovered: " + std::to_string(countOf<BookmarkText>(zlibNamed)));
    expectText(textOf<BookmarkText>(zlibNamed, 2) == "Page B\u00f6th" && textOf<BookmarkText>(zlibNamed, 5) == "Percent 150"
                   && textOf<BookmarkText>(zlibNamed, 6) == "Percent 75 off",
        "A Finale 2008 bookmark name was not recovered: " + textOf<BookmarkText>(zlibNamed, 2));

    // Finale 2001 on Windows stores these characters as Windows-1252 rather than Mac Roman:
    // `ab bb` is the guillemet pair and `e5` the a-ring. The source platform selects the
    // conversion.
    const auto windows = readTextFixture("evidence/F2001/F2001Win-bookmarks.mus");
    expectText(windows.report.formatEpoch == FormatEpoch::DclLegacy, "The Windows bookmark fixture was not classified as a DCL-era file");
    expectText(textOf<BookmarkText>(windows, 1) == "Scroll \u00ab\u00bb 9, 3" && textOf<BookmarkText>(windows, 2) == "P\u00e5ge Bookmark",
        "A Windows-1252 bookmark name was not converted: [" + textOf<BookmarkText>(windows, 1) + "] [" + textOf<BookmarkText>(windows, 2) + "]");

    // The comparators are allocated in the text pool's record order rather than by a fixed
    // cross-release numbering rule.
    expectText(
        textOf<BookmarkText>(earlier, 1) == textOf<BookmarkText>(pooled, 3) && textOf<BookmarkText>(earlier, 2) == textOf<BookmarkText>(pooled, 2),
        "The two eras did not recover the same bookmark names");
}

// The Coda-banner epoch keeps block text in the `HT` and `HS` others families and lyric text in
// the region behind the last record pool, so neither reaches the text-pool walk.
void testCodaBannerBlockTexts()
{
    using namespace musx::dom::texts;

    // `HS` word 2 packs the font comparator above the point size, so the two are read from one
    // word and a fixture whose size is 12 must not also report font 12.
    const auto shorter = readTextFixture("evidence/F100/F100-short-text.mus");
    expectText(shorter.report.formatEpoch == FormatEpoch::CodaBanner, "The Coda block-text fixture was not classified as a Coda-banner file");
    expectText(textOf<BlockText>(shorter, 1) == "^font(Monaco)^size(12)^nfx(0)short",
        "A Coda block text was not recovered: " + textOf<BlockText>(shorter, 1));
    const auto shortBlock = shorter.document->getOthers()->get<musx::dom::others::TextBlock>(musx::dom::SCORE_PARTID, 1);
    expectText(shortBlock && shortBlock->textId == 1 && shortBlock->lineSpacingPercentage == 100
                   && shortBlock->justify == musx::dom::others::TextBlock::TextJustify::Left && !shortBlock->newPos36 && shortBlock->shapeId == 0
                   && !shortBlock->showShape && shortBlock->wordWrap && !shortBlock->noExpandSingleWord,
        "The Finale 1.0 block's TextBlock attributes were not assembled from HS");

    // In the same controlled record, replace the opening three literal bytes with hashes.
    // The pair is escaped content; the remaining lone hash still names the page insert.
    const auto shortPath = std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F100/F100-short-text.mus";
    std::ifstream shortInput(shortPath, std::ios::binary);
    auto shortBytes = std::vector<std::uint8_t>(std::istreambuf_iterator<char>(shortInput), std::istreambuf_iterator<char>());
    constexpr std::string_view originalText = "short";
    const auto original = std::search(shortBytes.begin(), shortBytes.end(), originalText.begin(), originalText.end());
    expectText(original != shortBytes.end(), "The controlled Coda text bytes were not found");
    std::fill_n(original, 3, static_cast<std::uint8_t>('#'));
    const auto escapedInsert = Reader::readWithReport<TextPoolXmlDocument>(shortBytes);
    expectText(textOf<BlockText>(escapedInsert, 1) == "^font(Monaco)^size(12)^nfx(0)#^page(0)rt",
        "A doubled Coda insert character was not preserved as literal text");

    // The same document with one string lengthened. The record is a fixed four incidences
    // either way, so the previous save's bytes remain after the terminator and must not be
    // read as text.
    const auto longer = readTextFixture("evidence/F100/F100-long-text-w-insert.mus");
    expectText(textOf<BlockText>(longer, 1) == "^font(Monaco)^size(12)^nfx(0)longer with ^page(2) page insert",
        "A lengthened Coda block text was not recovered: " + textOf<BlockText>(longer, 1));

    // One insert character stands for whichever insert the block carries, and the style record
    // says which.
    const auto inserts = readTextFixture("evidence/F100/F100-text-other-inserts.mus");
    expectText(textOf<BlockText>(inserts, 1) == "^font(Charcoal)^size(12)^nfx(0)Text with date: ^date(0)"
                   && textOf<BlockText>(inserts, 2) == "^font(Charcoal)^size(12)^nfx(0)Text with time: ^time(0)",
        "A Coda insert was not read from its style record");

    // A second release, and the case that shows the style is not in the text record: these two
    // blocks differ in size and style while sharing every byte of their `HT` trailer.
    const auto later = readTextFixture("evidence/F263/F263-baseline.mus");
    expectText(textOf<BlockText>(later, 3) == "^font(Times)^size(36)^nfx(1)TITLE"
                   && textOf<BlockText>(later, 8) == "^font(Times)^size(14)^nfx(1)R. G. PATTERSON (1993)"
                   && textOf<BlockText>(later, 9) == "^font(Times)^size(12)^nfx(0)\u00a9 1993 Robert G. Patterson",
        "A Finale 2.6.3 block text was not recovered");
    expectText(countOf<BlockText>(later) == 9, "The Finale 2.6.3 fixture should carry exactly nine block texts");
    const auto blocks = later.document->getOthers()->getArray<musx::dom::others::TextBlock>(musx::dom::SCORE_PARTID);
    const auto centered = later.document->getOthers()->get<musx::dom::others::TextBlock>(musx::dom::SCORE_PARTID, 3);
    const auto right = later.document->getOthers()->get<musx::dom::others::TextBlock>(musx::dom::SCORE_PARTID, 2);
    const auto ordinaryCount = std::count_if(
        blocks.begin(), blocks.end(), [](const auto& block) { return block->textType == musx::dom::others::TextBlock::TextType::Block; });
    expectText(ordinaryCount == 9 && centered && centered->textId == 3 && centered->justify == musx::dom::others::TextBlock::TextJustify::Center
                   && !centered->noExpandSingleWord && right && right->textId == 2
                   && right->justify == musx::dom::others::TextBlock::TextJustify::Right && !right->noExpandSingleWord,
        "The Finale 2.6.3 TextBlocks did not retain HS ordering and justification");
}

// Lyric text is the one class this era keeps in its text region, spelled out rather than in
// binary command codes. Each record runs to the next keyword, there being no terminator.
void testCodaBannerLyricTexts()
{
    using namespace musx::dom::texts;
    const auto result = readTextFixture("evidence/F100/F100-lyric-text.mus");

    // Each partial initial run is completed from its lyric class's own default before the
    // first literal space. Explicit face, size, and effect commands remain untouched.
    expectText(textOf<LyricsVerse>(result, 1) == "^font(New York)^size(12)^nfx(0) ly-ric verse ",
        "A Coda lyric verse was not recovered: " + textOf<LyricsVerse>(result, 1));

    // An `^efx` run separated by spaces is still one run and still one `^nfx`. The spaces are
    // literal text and belong before the command, which is where Finale 27 puts them too.
    expectText(textOf<LyricsChorus>(result, 1) == "^font(Geneva)^size(12)^nfx(0)  ^nfx(1) chor-us text ",
        "A spaced effect run was not folded into one command: " + textOf<LyricsChorus>(result, 1));
    expectText(textOf<LyricsSection>(result, 1) == "^font(Palatino)^size(12)^nfx(0) ^size(13)  ^nfx(2) sec-tion text",
        "A Coda lyric section was not recovered: " + textOf<LyricsSection>(result, 1));

    // A document with no lyrics must produce none rather than an empty record per keyword.
    const auto silent = readTextFixture("evidence/F100/F100-baseline.mus");
    expectText(countOf<LyricsVerse>(silent) == 0 && countOf<LyricsChorus>(silent) == 0 && countOf<LyricsSection>(silent) == 0,
        "A Coda document with an empty lyric region produced lyric objects");
}

void testSyntheticStreamBoundaries()
{
    using namespace musx::dom::texts;

    // Reusing a conversion must not reuse the text object: comparators remain distinct even
    // when their source bytes and converted values are identical.
    const auto repeated = importStream("^block(1)^font(Times)a^end^block(2)^font(Times)a^end");
    expectText(repeated.document->getTexts()->getArray<BlockText>().size() == 2 && textOf<BlockText>(repeated, 1) == "^font(Times)a"
                   && textOf<BlockText>(repeated, 2) == "^font(Times)a",
        "An exact-source cache hit collapsed two text objects into one");

    // An escaped caret is content, and stays escaped so that musxdom reads it back as one.
    const auto escaped = importStream("^block(1)^font(Times)a^^b^end");
    expectText(textOf<BlockText>(escaped, 1) == "^font(Times)a^^b", "An escaped caret was not preserved");

    // A font command whose comparator the document does not define falls back to `^fontid`,
    // which is the one spelling that needs no definition to exist. The synthetic document
    // defines comparator 4 alone, so `^\x85` naming 4 is written out by name and the same
    // command naming 7 is not. The categorized commands fall back the same way, and that
    // costs the marking category they name -- the one thing `^fontid` cannot carry.
    // Digits are stored one greater than their value, so `\x05` is comparator 4 and `\x08` is 7.
    const auto fontStream = [](char code, char comparator) {
        return std::string("^block(1)^") + code + std::string("\x01\x01\x01", 3) + comparator + "a^end";
    };
    const auto namedFont = importStream(fontStream('\x85', '\x05'));
    expectText(textOf<BlockText>(namedFont, 1) == "^font(Times)a", "A defined comparator was not written out under the font's name");
    const auto unnamedFont = importStream(fontStream('\x85', '\x08'));
    expectText(textOf<BlockText>(unnamedFont, 1) == "^fontid(7)a", "An undefined comparator did not fall back to `^fontid`");
    const auto unnamedCategory = importStream(fontStream('\xa5', '\x08'));
    expectText(
        textOf<BlockText>(unnamedCategory, 1) == "^fontid(7)a", "An undefined comparator kept a categorized spelling it cannot name a font for");
    const auto namedCategory = importStream(fontStream('\xa5', '\x05'));
    expectText(textOf<BlockText>(namedCategory, 1) == "^fontMus(Times)a", "A defined comparator lost the marking category its command names");
    const auto unspellableFont = importStream(fontStream('\x85', '\x0b'));
    expectText(
        textOf<BlockText>(unspellableFont, 1) == "^fontid(10)a", "A font name that cannot fit Enigma argument syntax produced an invalid command");

    const auto spacedFontArgument = importStream("^block(1)^font (Times)^size(12)^efx(plain)Ped.^end");
    expectText(textOf<BlockText>(spacedFontArgument, 1) == "^font(Times)^size(12)^nfx(0)Ped.",
        "Whitespace between a font command and its argument made the font name literal");

    const auto emptyFont = importStream("^block(1)^font(Times)a^font(,4096)b^Font()c^end");
    expectText(textOf<BlockText>(emptyFont, 1) == "^font(Times)abc", "An empty font command was retained or cleared the active font");

    const std::string japaneseName("\x83\x71\x83\x89\x83\x4d\x83\x6d\x96\xbe\x92\xa9 Pro W3", 19);
    const std::string japaneseText("\x95\x73\x94\x40\x8b\x41", 6);
    const auto japanese = importStream("^block(1)^font(" + japaneseName + ",4097)" + japaneseText + "^end");
    expectText(textOf<BlockText>(japanese, 1) == "^font(ヒラギノ明朝 Pro W3)不如帰",
        "A font command did not use its packed character set for its name and literal text");

    const auto droppedEffects = importStream("^block(1)^font(Times)^size(48)^efx(plain)^efx(outline)^efx(shadow)^efx(bold)a^end");
    expectText(textOf<BlockText>(droppedEffects, 1) == "^font(Times)^size(48)^nfx(1)a",
        "Unsupported outline and shadow effects were not dropped from an effect run");
    const auto droppedRawEffects = importStream("^block(1)^font(Times)^size(48)^nfx(89)a^end");
    expectText(textOf<BlockText>(droppedRawEffects, 1) == "^font(Times)^size(48)^nfx(65)a",
        "Unsupported outline and shadow bits were not dropped from a raw nfx mask");

    // A keyword this reader does not import is named rather than silently skipped.
    const auto unknownKeyword = importStream("^block(1)x^end^cornet(2)y^end");
    expectText(unknownKeyword.document->getTexts()->getArray<BlockText>().size() == 1, "A recognized record before an unrecognized one was lost");
    expectText(hasDiagnosticContaining(unknownKeyword.report, "cornet"), "An unrecognized text keyword was not reported by name");

    // A binary command with no known spelling is dropped from the text and reported, because
    // guessing a name would produce a command that resolves to the wrong thing.
    // No length: the stream holds no NUL, so the literal measures itself. A hand-counted
    // length here was wrong by four and read past the end of the array, which every compiler
    // but the one it was written on refused.
    const auto unknownCode = importStream("^block(1)^\xff\x01\x01\x01\x02here^end");
    expectText(textOf<BlockText>(unknownCode, 1) == "here", "An unknown binary command did not leave the surrounding text intact");
    expectText(hasDiagnosticContaining(unknownCode.report, "0xff"), "An unknown binary command code was not reported");

    // A literal byte that falls in the digit range must survive: the argument width comes
    // from the table, so a four-digit argument takes four bytes and no more. Text set in a
    // symbol font is glyph numbers, which is how a byte this low reaches literal text at all.
    const std::string lowByteStream = std::string("^block(1)^\x84", 11) + std::string("\x01\x01\x01\x01", 4)   // the whole four-digit argument
                                      + std::string("\x10", 1)                // a literal byte that looks like a digit
                                      + "x^end";
    const auto lowByteLiteral = importStream(lowByteStream);
    expectText(textOf<BlockText>(lowByteLiteral, 1) == std::string("^nfx(0)\x10x", 9),
        "A literal byte in the digit range was eaten as a fifth argument digit, giving \"" + textOf<BlockText>(lowByteLiteral, 1) + "\"");

    // Bytes that are not a chunk stop the walk instead of being scanned past.
    const auto malformed = importStream("^block(1)good^end\x01\x02\x03\x04");
    expectText(malformed.document->getTexts()->getArray<BlockText>().size() == 1, "The record before the malformed bytes was not kept");
    expectText(hasDiagnosticContaining(malformed.report, "stopped making sense"), "A malformed text pool was not reported");

    // A record whose terminator never arrives is the truncation case, and must not be
    // half-imported.
    const auto truncated = importStream("^block(1)unfinished");
    expectText(truncated.document->getTexts()->getArray<BlockText>().empty(), "An unterminated record was imported anyway");
    expectText(hasDiagnosticContaining(truncated.report, "stopped making sense"), "An unterminated text pool was not reported");

    // An empty pool is not an error and produces nothing.
    const auto empty = importStream("");
    expectText(empty.document->getTexts()->getArray<BlockText>().empty() && empty.report.diagnostics.empty(), "An empty text pool was not silent");
}

void testNestedParenthesesInInitialFontName()
{
    const auto document = makeTextDocument();
    musx::dom::FontInfo defaultFont(document);
    defaultFont.fontSize = 14;
    defaultFont.setEnigmaStyles(2);
    bool fontSynthesized = false;
    bool sizeSynthesized = false;
    bool effectsSynthesized = false;
    constexpr std::string_view value = "^font(Missing Font (1))^size(12)^nfx(2)Gliss.";

    const auto initialized = finale_mus_reader::text::initializeEnigmaTextFontState(
        std::string(value), defaultFont, &fontSynthesized, &sizeSynthesized, &effectsSynthesized);
    expectText(initialized == value, "A parenthesis in a font name caused initial font state to be inserted inside it");
    expectText(!fontSynthesized && !sizeSynthesized && !effectsSynthesized,
        "A complete initial state with a parenthesized font name was reported as synthesized");

    musx::dom::FontInfo unspellableDefault(document);
    unspellableDefault.fontId = 10;
    unspellableDefault.fontSize = 12;
    unspellableDefault.setEnigmaStyles(0);
    const auto completed = finale_mus_reader::text::initializeEnigmaTextFontState("text", unspellableDefault);
    expectText(completed == "^fontid(10)^size(12)^nfx(0)text", "An unspellable default font name produced an invalid synthesized command");
}

void testEnigmaFontResolutionCache()
{
    const auto document = makeTextDocument();
    finale_mus_reader::text::EnigmaFontResolutionCache cache;
    const finale_mus_reader::text::EnigmaTextSource source{document, false, SourcePlatform::MacOS, nullptr, &cache};
    const auto convert = [&](std::string_view value) {
        return finale_mus_reader::text::toModernEnigmaText(
            std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(value.data()), value.size()), source);
    };

    expectText(convert("^font(Times)a").text == "^font(Times)a" && convert("^font(times)b").text == "^font(Times)b" && cache.fontIdsByName.size() == 1
                   && cache.fontIdsByName.at("times") == musx::dom::Cmper(4),
        "Equivalent font names were not resolved through one cached result");

    expectText(convert("^font(Missing)a").text == "^font(Missing)a" && convert("^font(Missing)b").text == "^font(Missing)b"
                   && cache.fontIdsByName.size() == 2 && !cache.fontIdsByName.at("missing"),
        "Repeated unresolved font names were not served by one cached result");
}

} // namespace

TEST_CASE("Uncompressed text pool", "[texts]")
{
    testUncompressedTextPool();
}
TEST_CASE("File info text", "[texts]")
{
    testFileInfoText();
}
TEST_CASE("Early expression text synthesis", "[texts]")
{
    testEarlyExpressionTextSynthesis();
}
TEST_CASE("Compressed text pools", "[texts]")
{
    testCompressedTextPool();
}
TEST_CASE("Finale 2008 inserts and pooled file info", "[texts]")
{
    testFinale2008Inserts();
}
TEST_CASE("Finale 2011 category fonts and rehearsal", "[texts]")
{
    testFinale2011CategoryFonts();
}
TEST_CASE("Early text pool framing", "[texts]")
{
    testEarlyTextPoolFraming();
}
TEST_CASE("Bookmark text", "[texts]")
{
    testBookmarkText();
}
TEST_CASE("Coda banner block texts", "[texts]")
{
    testCodaBannerBlockTexts();
}
TEST_CASE("Coda banner lyric texts", "[texts]")
{
    testCodaBannerLyricTexts();
}
TEST_CASE("Text pool stream boundaries", "[texts]")
{
    testSyntheticStreamBoundaries();
}
TEST_CASE("Parentheses in initial font name", "[texts]")
{
    testNestedParenthesesInInitialFontName();
}
TEST_CASE("Enigma font resolution cache", "[texts]")
{
    testEnigmaFontResolutionCache();
}
