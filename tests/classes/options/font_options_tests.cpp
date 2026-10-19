// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

void testMissingRecoveredFontDefinitionFallback()
{
    using FontDefinition = musx::dom::others::FontDefinition;
    using FontOptions = musx::dom::options::FontOptions;
    using FontType = FontOptions::FontType;

    auto targetSession = musx::factory::DocumentFactory::begin();
    const auto targetDocument = targetSession.getDocument();
    auto targetOptions = std::make_shared<FontOptions>(targetDocument);
    targetDocument->getOptions()->add(FontOptions::XmlNodeName, targetOptions);
    const auto addTargetFont = [&](musx::dom::Cmper cmper, const char* name) {
        auto font = std::make_shared<FontDefinition>(targetDocument, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, cmper);
        font->name = name;
        targetDocument->getOthers()->add(FontDefinition::XmlNodeName, font);
    };
    addTargetFont(0, "Seville");
    addTargetFont(5, "Arial");
    const auto addMissingOption = [&](FontType type, int size, std::uint16_t effects) {
        auto font = std::make_shared<musx::dom::FontInfo>(targetDocument);
        font->fontId = 99;
        font->fontSize = size;
        font->setEnigmaStyles(effects);
        targetOptions->fontOptions.emplace(type, font);
    };
    addMissingOption(FontType::Fretboard, 36, 1);
    addMissingOption(FontType::Tablature, 12, 2);

    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto referenceDocument = referenceSession.getDocument();
    auto referenceOptions = std::make_shared<FontOptions>(referenceDocument);
    referenceDocument->getOptions()->add(FontOptions::XmlNodeName, referenceOptions);
    // The reference carries a size and effects distinct from the source's, so the
    // assertions below can tell which document each part of the tuple came from.
    const auto addReference = [&](FontType type, musx::dom::Cmper cmper, const char* name, int size, std::uint16_t effects) {
        auto definition = std::make_shared<FontDefinition>(referenceDocument, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, cmper);
        definition->name = name;
        referenceDocument->getOthers()->add(FontDefinition::XmlNodeName, definition);
        auto font = std::make_shared<musx::dom::FontInfo>(referenceDocument);
        font->fontId = cmper;
        font->fontSize = size;
        font->setEnigmaStyles(effects);
        referenceOptions->fontOptions.emplace(type, font);
    };
    addReference(FontType::Fretboard, 3, "Seville", 24, 4);
    addReference(FontType::Tablature, 4, " arIAL ", 18, 5);

    ImportReport report(FormatEpoch::UncompressedLegacy);
    finale_mus_reader::options::repairMissingRecoveredFontDefinitions(
        targetDocument, referenceDocument, targetOptions, report, targetSession.getConstructionContext());

    // The whole tuple comes from the reference, not just the face. A point size is not
    // independent of the face it was chosen for, so pairing a substituted face with the
    // source's size would produce a combination present in neither document. The source
    // values here are 36/1 and 12/2; both must be gone.
    const auto fretboard = targetOptions->getFontInfo(FontType::Fretboard);
    expectMapping(fretboard->fontId == 6 && fretboard->fontSize == 24 && fretboard->getEnigmaStyles() == 4,
        "A same-type reference face was not cloned after the highest target comparator");
    expectMapping(targetDocument->getOthers()->get<FontDefinition>(musx::dom::SCORE_PARTID, 6)->name == "Seville",
        "The cloned same-type reference face did not retain its reference spelling");
    const auto tablature = targetOptions->getFontInfo(FontType::Tablature);
    expectMapping(tablature->fontId == 5 && tablature->fontSize == 18 && tablature->getEnigmaStyles() == 5,
        "A normalized nonzero target face was not reused by the fallback");
    expectMapping(targetDocument->getOthers()->getArray<FontDefinition>(musx::dom::SCORE_PARTID).size() == 3,
        "The fallback introduced a duplicate nonzero font name");
    // The fallback is silent by design: it is a considered substitution that leaves the
    // document usable, and a warning would surface it in user interfaces as though
    // something had gone wrong. Callers distinguish substituted values from recovered ones
    // through the reported ValueOrigin, not through a message.
    expectMapping(std::none_of(report.diagnostics.begin(), report.diagnostics.end(),
                      [](const finale_mus_reader::Diagnostic& entry) { return entry.level == musx::util::Logger::LogLevel::Warning; }),
        "The designed-in font substitution emitted a user-facing warning");
    expectMapping(report.diagnostics.size() == 2, "The font substitution was not recorded at verbose level");
    const auto finished = std::move(targetSession).finish();
    expectMapping(
        !finished->getOthers()->get<FontDefinition>(musx::dom::SCORE_PARTID, 99), "A replaced font reference left an unused missing-font definition");
}

void testFontDefinitions()
{
    const auto path = std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F2002/F2002-baseline.mus";
    const auto result = Reader::readWithReport<TestXmlDocument>(path);
    using musx::dom::others::FontDefinition;
    const auto fonts = result.document->getOthers()->getArray<FontDefinition>(musx::dom::SCORE_PARTID);
    // Nine source definitions, plus three introduced from the baseline: one for a FontOptions
    // type this source does not store, one for the copied tablature clef shapes, and one for the
    // pre-2009 marking categories. Imported objects bring the typefaces they reference with them.
    expect(fonts.size() == 12, "F2002 font table plus required fallback fonts is incorrect: " + std::to_string(fonts.size()));

    const auto fontAt = [&](musx::dom::Cmper cmper) {
        const auto font = result.document->getOthers()->get<FontDefinition>(musx::dom::SCORE_PARTID, cmper);
        expect(static_cast<bool>(font), "Missing font definition " + std::to_string(cmper));
        return font;
    };

    const auto maestro = fontAt(0);
    expect(maestro->name == "Maestro", "Font 0 name was not recovered");
    expect(maestro->charsetBank == FontDefinition::CharacterSetBank::MacOS, "Font 0 character set bank was not recovered");
    expect(maestro->charsetVal == 0xfff, "Font 0 character set value was not recovered");
    expect(maestro->calcIsSymbolFont(), "Font 0 should be a symbol font");

    const auto times = fontAt(1);
    expect(times->name == "Times", "Font 1 name was not recovered");
    expect(times->charsetVal == 0 && !times->calcIsSymbolFont(), "A text font was reported as a symbol font");

    // A name longer than one row continues into the following incidences.
    expect(fontAt(5)->name == "Maestro Percussion", "A font name spanning incidences was not assembled");

    expect(fontAt(9)->name == "Times New Roman", "The unmatched fallback face did not retain the reference spelling");
}

void testConfiguredSymbolFontDefinitions()
{
    using FontDefinition = musx::dom::others::FontDefinition;
    const auto parsed = makeContainer({
        {20, "FN", {0x1000, 0, 0, 0, 0, 0}},
        {20, "FN", {0x4d61, 0x634c, 0x6973, 0x7465, 0x6400, 0}},
        {21, "FN", {0x2000, 0, 0, 0, 0, 0}},
        {21, "FN", {0x5769, 0x6e4c, 0x6973, 0x7465, 0x6400, 0}},
    });
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = referenceSession.getDocument();
    SourceProfile profile(FormatEpoch::UncompressedLegacy);
    profile.version = SourceVersion{.major = 3, .minor = 2};
    profile.byteOrder = ByteOrder::BigEndian;
    const finale_mus_reader::text::SymbolFontNames symbolFonts{"maclisted", "winlisted"};
    profile.symbolFontNames = &symbolFonts;
    ImportReport report(FormatEpoch::UncompressedLegacy);
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{
        LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};

    finale_mus_reader::others::importFontDefinitions(context);

    const auto mac = document->getOthers()->get<FontDefinition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(20));
    const auto win = document->getOthers()->get<FontDefinition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(21));
    expect(mac && mac->name == "MacListed" && mac->charsetBank == FontDefinition::CharacterSetBank::MacOS
               && mac->charsetVal == FontDefinition::SYMBOL_CHARSET_MAC && mac->calcIsSymbolFont(),
        "A configured Mac symbol font was not persisted as a symbol definition");
    expect(win && win->name == "WinListed" && win->charsetBank == FontDefinition::CharacterSetBank::Windows
               && win->charsetVal == FontDefinition::SYMBOL_CHARSET_WIN && win->calcIsSymbolFont(),
        "A configured Windows symbol font was not persisted as a symbol definition");
    for (const auto cmper : {musx::dom::Cmper(20), musx::dom::Cmper(21)}) {
        const auto* charset = report.findField<FontDefinition>("charsetVal", musx::dom::SCORE_PARTID, cmper);
        expect(charset && charset->origin == ValueOrigin::LegacyMusAdjusted && charset->rawValue == 0,
            "A configured symbol charset did not retain its stored value and adjusted origin");
        expect(finale_mus_reader::text::codepointFromByte(0xfa, document, cmper, finale_mus_reader::text::UnresolvedFontFallback::Text) == 0xfa,
            "Character decoding did not use the persisted symbol charset");
    }

    const std::string configuredName = "Times\n";
    const std::vector<std::uint8_t> configuredBytes(configuredName.begin(), configuredName.end());
    const finale_mus_reader::ReaderOptions options{std::span<const std::uint8_t>(configuredBytes)};
    const auto path = std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F2002/F2002-baseline.mus";
    const auto imported = Reader::readWithReport<TestXmlDocument>(path, options);
    const auto times = imported.document->getOthers()->get<FontDefinition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    expect(times && times->charsetBank == FontDefinition::CharacterSetBank::MacOS && times->charsetVal == FontDefinition::SYMBOL_CHARSET_MAC,
        "ReaderOptions did not persist a configured symbol font in the returned document");
    const auto* importedCharset = imported.report.findField<FontDefinition>("charsetVal", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    expect(importedCharset && importedCharset->origin == ValueOrigin::LegacyMusAdjusted && importedCharset->rawValue == 0,
        "The returned document's adjusted symbol charset lost its stored provenance");
}

void testHeaderlessFontDefinitionOrigins()
{
    using FontDefinition = musx::dom::others::FontDefinition;
    const auto result = readFixture("evidence/F263/F263-baseline.mus");
    const auto fonts = result.document->getOthers()->getArray<FontDefinition>(musx::dom::SCORE_PARTID);
    expect(!fonts.empty(), "The headerless font fixture recovered no definitions");
    for (const auto& font : fonts) {
        const auto key = finale_mus_reader::instanceKey<FontDefinition>(musx::dom::SCORE_PARTID, font->getCmper());
        const auto* name = result.report.findField(key, "name");
        for (const auto* member : {"charsetBank", "charsetVal", "pitch", "family"}) {
            if (name) {
                const auto* info = result.report.findField(key, member);
                expect(info, std::string("A headerless font characteristic has no recorded origin: ") + member + " for font "
                                 + std::to_string(font->getCmper()));
            } else {
                expect(result.report.findInstanceOrigin(key), std::string("A fallback font characteristic has no recorded default: ") + member);
            }
        }
    }
}

void testUncompressedFontOptions()
{
    using FontOptions = musx::dom::options::FontOptions;
    using FontType = FontOptions::FontType;
    const std::array<std::pair<const char*, std::array<int, 4>>, 3> eraFixtures{{{"evidence/F372/F372-baseline.mus", {24, 24, 24, 24}},
        {"evidence/F97/F97-fileinfo-short.mus", {28, 28, 24, 26}}, {"evidence/F2000/F2000-multilayer.mus", {28, 28, 24, 26}}}};
    for (const auto& [path, sizes] : eraFixtures) {
        const auto result = readFixture(path);
        expect(result.report.formatEpoch == FormatEpoch::UncompressedLegacy, "An uncompressed fixture was not classified as uncompressed");
        const auto options = result.document->getOptions()->get<FontOptions>();
        expect(options && options->fontOptions.size() == 45, "Uncompressed font options were not completed to the modern type set");
        expect(options->getFontInfo(FontType::Music)->fontSize == sizes[0] && options->getFontInfo(FontType::Key)->fontSize == sizes[1]
                   && options->getFontInfo(FontType::Clef)->fontSize == sizes[2] && options->getFontInfo(FontType::Time)->fontSize == sizes[3],
            std::string("Uncompressed font options were not recovered from ") + path);
        expect(field(result, "options.fontOptions[0].fontSize").origin == ValueOrigin::LegacyMus,
            std::string("Uncompressed font options were reported as defaults from ") + path);
        expect(field(result, "options.fontOptions[13].fontId").origin == ValueOrigin::LegacyMus, "The uncompressed tablature slot was not mapped");
    }
}

TEST_CASE("Font definitions", "[class][reader]")
{
    testFontDefinitions();
}
TEST_CASE("Configured symbol font definitions", "[class][reader]")
{
    testConfiguredSymbolFontDefinitions();
}
TEST_CASE("Headerless font definitions report their fixed characteristics", "[class][reader]")
{
    testHeaderlessFontDefinitionOrigins();
}

void testFontOptionsCapture()
{
    using FontOptions = musx::dom::options::FontOptions;
    using FontType = FontOptions::FontType;
    const auto read = [](const char* relative) {
        return Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / relative);
    };

    const auto f2002 = read("evidence/F2002/F2002-baseline.mus");
    const auto fixed = f2002.document->getOptions()->get<FontOptions>();
    expect(fixed && fixed->fontOptions.size() == 45, "Finale 2002 font options were not completed to the modern type set");
    const auto tuplet = fixed->getFontInfo(FontType::Tuplet);
    expect(tuplet->fontId == 1 && tuplet->fontSize == 10, "A fixed-row font-options tuple was not captured");
    expect(tuplet->bold && tuplet->italic && !tuplet->underline, "Font option effects were not expanded into musxdom booleans");
    expect(field(f2002, "others.fontName[0].name").origin == ValueOrigin::LegacyMus,
        "A record-created font definition did not retain its comparator in diagnostics");
    expect(field(f2002, "options.fontOptions[7].effects").rawValue == 3, "The raw fixed-row effects mask was not reported");
    expect(field(f2002, "options.fontOptionsPhysical[13].fontId").origin == ValueOrigin::LegacyMus,
        "The Finale 2002 drawing-time tablature slot was not retained as physical evidence");
    expect(field(f2002, "options.fontOptions[13].fontId").origin == ValueOrigin::LegacyMus,
        "Finale 2002 physical slot 28 was not mapped to semantic tablature");
    expect(field(f2002, "options.fontOptions[28].fontId").origin == ValueOrigin::Finale27Default,
        "Finale 2002 percussion was not supplied by the baseline");
    expect(field(f2002, "options.fontOptions[40].fontId").origin == ValueOrigin::Finale27Default,
        "A modern bend font absent from Finale 2002 was not synthesized");
    expect(fixed->getFontInfo(FontType::TimeParts)->fontId == 0, "A synthesized baseline font id 0 did not pass through unchanged");

    const auto f2005 = read("evidence/F2005/F2005-baseline.mus");
    const auto laterFixed = f2005.document->getOptions()->get<FontOptions>();
    expect(laterFixed && laterFixed->fontOptions.size() == 45, "Finale 2005 font options were not completed to the modern type set");
    expect(field(f2005, "options.fontOptionsPhysical[43].fontId").rawValue == 0
               && field(f2005, "options.fontOptionsPhysical[43].fontSize").rawValue == 0
               && field(f2005, "options.fontOptionsPhysical[43].effects").rawValue == 0,
        "The Finale 2005 structural-fill tuple was not retained as physical evidence");
    expect(field(f2005, "options.fontOptions[43].fontId").origin == ValueOrigin::Finale27Default
               && field(f2005, "options.fontOptions[44].fontId").origin == ValueOrigin::Finale27Default,
        "Finale 2005 time-parts fonts were not supplied by the baseline");

    const auto f2007 = read("evidence/F2007/F2007-lyric-hyphens.mus");
    const auto zlib = f2007.document->getOptions()->get<FontOptions>();
    expect(zlib && zlib->fontOptions.size() == 45, "The zlib font-options payload did not populate its live tuple range");
    const auto zlibTuplet = zlib->getFontInfo(FontType::Tuplet);
    expect(zlibTuplet->fontId == tuplet->fontId && zlibTuplet->fontSize == tuplet->fontSize
               && zlibTuplet->getEnigmaStyles() == tuplet->getEnigmaStyles(),
        "The zlib tuple layout disagrees with the fixed-row layout");
    expect(field(f2007, "options.fontOptionsPhysical[45].fontId").rawValue == 0
               && field(f2007, "options.fontOptionsPhysical[45].fontSize").rawValue == 0
               && field(f2007, "options.fontOptionsPhysical[45].effects").rawValue == 0,
        "The terminal physical zlib tuple was not captured in the report");

    // The 13/28 renumbering happens at Finale 2012, not Finale 2003 as the documentation
    // said. Finale 2007 is inside the zlib epoch but before the boundary, so it must still
    // take the earlier layout: tablature comes from physical 28, and percussion is not
    // stored at all. Pinning this side matters more than the modern side, because the
    // previous code got precisely this wrong for every 2003-2011 document.
    expect(field(f2007, "options.fontOptions[13].fontId").origin == ValueOrigin::LegacyMus,
        "Finale 2007 tablature was not recovered from physical slot 28");
    expect(field(f2007, "options.fontOptions[28].fontId").origin == ValueOrigin::Finale27Default,
        "Finale 2007 percussion was not left to the baseline; it is not stored before 2012");

    // Finale 2011 leaves drawing-time contents in the holding slot. They stay in the report
    // as physical evidence, but Finale discards them, so they are not a diagnostic.
    const auto f2011 = read("evidence/F2011/F2011-baseline.mus");
    expect(
        field(f2011, "options.fontOptionsPhysical[13].fontSize").rawValue != 0, "The Finale 2011 holding slot was not retained as physical evidence");
    expect(std::none_of(f2011.report.diagnostics.begin(), f2011.report.diagnostics.end(),
               [](const auto& d) { return d.message.find("font-options tuple") != std::string::npos; }),
        "The Finale 2011 holding slot was reported as an unmapped tuple");

    const auto f2012 = read("evidence/F2012/F2012-upstem-flags.mus");
    const auto littleEndianZlib = f2012.document->getOptions()->get<FontOptions>();
    expect(littleEndianZlib && littleEndianZlib->fontOptions.size() == 45, "The little-endian zlib font-options payload was not captured");
    // The far side of the same boundary: Finale 2012 stores both, at the modern ordinals.
    expect(field(f2012, "options.fontOptions[13].fontId").origin == ValueOrigin::LegacyMus
               && field(f2012, "options.fontOptions[28].fontId").origin == ValueOrigin::LegacyMus,
        "Finale 2012 did not recover both tablature and percussion from stored tuples");
    const auto music = littleEndianZlib->getFontInfo(FontType::Music);
    expect(music->fontId == 0 && music->fontSize == 24 && field(f2012, "options.fontOptions[0].fontSize").rawValue == 24,
        "A little-endian zlib font-options word was byte-swapped incorrectly");

    const auto f100Baseline = read("evidence/F100/F100-baseline.mus");
    const auto earlyBaseline = f100Baseline.document->getOptions()->get<FontOptions>();
    expect(earlyBaseline && earlyBaseline->fontOptions.size() == 45, "Finale 1.0.0 font options were not completed");
    expect(earlyBaseline->getFontInfo(FontType::Music)->fontId == 0 && earlyBaseline->getFontInfo(FontType::Music)->fontSize == 71,
        "Finale 1.0.0 music font tuple was not recovered");
    expect(earlyBaseline->getFontInfo(FontType::TextBlock)->fontId == 1 && earlyBaseline->getFontInfo(FontType::TextBlock)->fontSize == 12,
        "Finale 1.0.0 text-block font tuple was not recovered");
    expect(earlyBaseline->getFontInfo(FontType::LyricVerse)->fontId == 1 && earlyBaseline->getFontInfo(FontType::LyricVerse)->fontSize == 12,
        "Finale 1.0.0 lyric-verse font tuple was not recovered");

    const auto f100Music = read("evidence/F100/F100-music-font.mus");
    const auto earlyMusic = f100Music.document->getOptions()->get<FontOptions>()->getFontInfo(FontType::Music);
    expect(earlyMusic->fontId == 12 && earlyMusic->fontSize == 60 && earlyMusic->getEnigmaStyles() == 0,
        "The controlled Finale 1.0.0 music-font edit was not recovered");

    const auto f263Baseline = read("evidence/F263/F263-baseline.mus");
    const auto f263Music = read("evidence/F263/F263-music-font.mus");
    const auto f263BaselineOptions = f263Baseline.document->getOptions()->get<FontOptions>();
    const auto f263MusicOptions = f263Music.document->getOptions()->get<FontOptions>();
    expect(f263BaselineOptions && f263MusicOptions && f263BaselineOptions->fontOptions.size() == 45 && f263MusicOptions->fontOptions.size() == 45,
        "Finale 2.6.3 font options were not completed");
    const auto f263ChangedMusic = f263MusicOptions->getFontInfo(FontType::Music);
    expect(f263ChangedMusic->fontId == 28 && f263ChangedMusic->fontSize == 24 && f263ChangedMusic->italic,
        "The controlled Finale 2.6.3 music-font edit was not recovered");

    // Finale 27 derives a JazzPerc percussion preference when it upgrades the changed
    // fixture, but the MUS file contains no independently sourced percussion preference.
    // It therefore remains the selected platform reference value in both imports.
    for (const auto* result : {&f263Baseline, &f263Music}) {
        const auto percussion = result->document->getOptions()->get<FontOptions>()->getFontInfo(FontType::Percussion);
        expect(percussion->fontId == 77 && percussion->fontSize == 24 && percussion->getEnigmaStyles() == 0
                   && percussion->getName() == "Maestro Percussion",
            "Pre-2003 percussion did not retain the reference FontOptions value");
        expect(field(*result, "options.fontOptions[28].fontId").origin == ValueOrigin::Finale27Default,
            "Pre-2003 percussion was reported as though it came from the MUS file");
    }

    const auto f100Text = read("evidence/F100/F100-text-font.mus");
    const auto earlyText = f100Text.document->getOptions()->get<FontOptions>()->getFontInfo(FontType::TextBlock);
    expect(earlyText->fontId == 2 && earlyText->fontSize == 17 && earlyText->bold && earlyText->italic,
        "The controlled Finale 1.0.0 text-font edit was not recovered");

    const auto f100Lyric = read("evidence/F100/F100-lyric-verse.mus");
    const auto earlyLyric = f100Lyric.document->getOptions()->get<FontOptions>()->getFontInfo(FontType::LyricVerse);
    expect(earlyLyric->fontId == 3 && earlyLyric->fontSize == 13 && earlyLyric->underline
               && field(f100Lyric, "options.fontOptions[9].effects").rawValue == 28,
        "The controlled Finale 1.0.0 lyric-font edit or effects mask was not recovered");

    const auto expectEarlyFont = [&](const char* path, FontType type, musx::dom::Cmper fontId, int size, std::uint16_t rawEffects) {
        auto result = read(path);
        const auto font = result.document->getOptions()->get<FontOptions>()->getFontInfo(type);
        expect(font->fontId == fontId && font->fontSize == size, std::string("Controlled early font was not recovered from ") + path);
        expect(field(result, "options.fontOptions[" + std::to_string(static_cast<std::size_t>(type)) + "].effects").rawValue == rawEffects,
            std::string("Controlled early effects mask was not reported from ") + path);
        return result;
    };

    const auto f100Accis = expectEarlyFont("evidence/F100/F100-accis.mus", FontType::ChordAcci, 2, 8, 0);
    const auto f100Chord = expectEarlyFont("evidence/F100/F100-chord.mus", FontType::Chord, 3, 9, 8);
    const auto f100Chorus = expectEarlyFont("evidence/F100/F100-chorus.mus", FontType::LyricChorus, 4, 11, 4);
    const auto f100Clef = expectEarlyFont("evidence/F100/F100-clef.mus", FontType::Clef, 4, 33, 4);
    const auto f100Ending = expectEarlyFont("evidence/F100/F100-ending.mus", FontType::Ending, 9, 19, 0);
    const auto f100Key = expectEarlyFont("evidence/F100/F100-key-font.mus", FontType::Key, 4, 13, 2);
    const auto f100Name = expectEarlyFont("evidence/F100/F100-name.mus", FontType::StaffNames, 7, 12, 1);
    // The Coda-banner era has one "Name" preference where Finale 3.0 and later store four
    // separate name tuples, so the single recovered value has to reach all four types.
    // Recovering StaffNames alone would split a document that was never split.
    for (const auto companion : {FontType::AbbrvStaffNames, FontType::GroupNames, FontType::AbbrvGroupNames}) {
        const auto font = f100Name.document->getOptions()->get<FontOptions>()->getFontInfo(companion);
        expect(font->fontId == 7 && font->fontSize == 12, "The Finale 1.0.0 Name preference did not reach every modern name font type");
        expect(field(f100Name, "options.fontOptions[" + std::to_string(static_cast<std::size_t>(companion)) + "].fontId").origin
                   == ValueOrigin::LegacyBehavior,
            "A propagated name font was not reported as restored era behavior");
    }
    expect(field(f100Name, "options.fontOptions[" + std::to_string(static_cast<std::size_t>(FontType::StaffNames)) + "].fontId").origin
               == ValueOrigin::LegacyMus,
        "The Name preference itself must still report as recovered from the source");

    // Finale 3.0 stores the four name types separately, so the fan-out must stop at the
    // Coda-banner epoch rather than overwriting three real recovered values.
    {
        const auto f97 = read("evidence/F372/F372-baseline.mus");
        for (const auto companion : {FontType::AbbrvStaffNames, FontType::GroupNames, FontType::AbbrvGroupNames}) {
            expect(field(f97, "options.fontOptions[" + std::to_string(static_cast<std::size_t>(companion)) + "].fontId").origin
                       == ValueOrigin::LegacyMus,
                "The Coda name fan-out leaked into an epoch that stores the types separately");
        }
    }

    const auto f100Section = expectEarlyFont("evidence/F100/F100-section.mus", FontType::LyricSection, 3, 19, 16);
    const auto f100Time = expectEarlyFont("evidence/F100/F100-time.mus", FontType::Time, 4, 17, 16);
    const auto f100Tuplet = expectEarlyFont("evidence/F100/F100-tuplet.mus", FontType::Tuplet, 20, 17, 0);
    expect(f100Tuplet.document->getOptions()->get<FontOptions>()->getFontInfo(FontType::ChordAcci)->fontId == 20,
        "The ChordAcci side effect in the controlled tuplet save was not recovered");
    const auto f100NoMmRests = expectEarlyFont("evidence/F100/F100-nommrests.mus", FontType::MultiMeasRest, 1, 24, 0);
    const auto f100MmRests = expectEarlyFont("evidence/F100/F100-mmrests.mus", FontType::MultiMeasRest, 2, 27, 2);
    expect(f100MmRests.document->getOptions()->get<FontOptions>()->getFontInfo(FontType::MultiMeasRest)->italic,
        "The Finale 1.0.0 multimeasure-rest number font effects were not recovered");
    expect(field(f100MmRests, "options.fontOptions[" + std::to_string(static_cast<std::size_t>(FontType::MultiMeasRest)) + "].fontId").origin
               == ValueOrigin::LegacyMus,
        "The Finale 1.0.0 multimeasure-rest number font was not reported as recovered");
    // From Finale 3.0 the number font is an ordinary entry of the default-font array.
    const auto f300NoMmRests = expectEarlyFont("evidence/F300/F300-nommrest.mus", FontType::MultiMeasRest, 0, 24, 0);
    const auto f300MmRests = expectEarlyFont("evidence/F300/F300-mmrest.mus", FontType::MultiMeasRest, 2, 11, 1);
    expect(f300MmRests.document->getOptions()->get<FontOptions>()->getFontInfo(FontType::MultiMeasRest)->bold,
        "The Finale 3.0 multimeasure-rest number font effects were not recovered");

    for (const auto* result : {&f2002, &f2005, &f2007, &f2012, &f100Baseline, &f100Music, &f263Baseline, &f263Music, &f100Text, &f100Lyric,
             &f100Accis, &f100Chord, &f100Chorus, &f100Clef, &f100Ending, &f100Key, &f100Name, &f100Section, &f100Time, &f100Tuplet, &f100NoMmRests,
             &f100MmRests, &f300NoMmRests, &f300MmRests}) {
        const auto options = result->document->getOptions()->get<FontOptions>();
        for (const auto& [type, font] : options->fontOptions) {
            (void)type;
            expect(static_cast<bool>(result->document->getOthers()->get<musx::dom::others::FontDefinition>(musx::dom::SCORE_PARTID, font->fontId)),
                "A completed font option has a dangling font id");
        }
    }
}

TEST_CASE("Font options capture", "[class][reader]")
{
    testFontOptionsCapture();
}
TEST_CASE("Uncompressed font options", "[class][reader]")
{
    testUncompressedFontOptions();
}

TEST_CASE("Missing recovered font definition fallback", "[class]")
{
    testMissingRecoveredFontDefinitionFallback();
}

} // namespace
} // namespace finale_mus_reader_tests
