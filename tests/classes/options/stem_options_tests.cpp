// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

/// @brief A document whose StemOptions already carries connections from somewhere else.
/// @details Seeded deliberately, because the capture pass must drop them: stem connections
/// belong to the document that stated them and name that document's fonts.
musx::dom::DocumentPtr makeStemDocument()
{
    using StemOptions = musx::dom::options::StemOptions;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<StemOptions>(document);
    options->stemLength = 84;
    for (char32_t symbol : {U'΄', U'΅', U'Ά'}) {
        auto connection = std::make_shared<StemOptions::StemConnection>();
        connection->symbol = symbol;
        connection->fontId = 99;
        options->stemConnections.push_back(std::move(connection));
    }
    document->getOptions()->add(StemOptions::XmlNodeName, options);
    return std::move(session).finish();
}

std::shared_ptr<musx::dom::options::StemOptions> captureStems(const finale_mus_reader::container::ParsedContainer& parsed,
    const SourceProfile& profile, const musx::dom::DocumentPtr& document, ImportReport& report)
{
    musx::factory::ConstructionContext construction;
    finale_mus_reader::options::captureStemOptions(LegacyRecordIndex::build(parsed), profile, document, report, construction);
    return std::const_pointer_cast<musx::dom::options::StemOptions>(document->getOptions()->get<musx::dom::options::StemOptions>());
}

// Finale 3.5 changed every unit in the stem family at once, and the reader decides that
// boundary from the size of the connection collection rather than from the version. The
// versions below therefore include one outside the era entirely: whatever a file claims to
// be, the collection it carries is what dates it. Both shapes are built here.
void testStemPreFinale35Units()
{
    const auto tableWith = [](std::size_t slots) {
        std::vector<SyntheticRow> rows{
            {GLOBALS_CMPER, "20", {0, 0, 0, 0, 7, 5}},   // stem length and shortened length
            {GLOBALS_CMPER, "21", {0, 0, 18, 0, 0, 0}},  // reverse stem adjustment
        };
        // The connection family, whose size is the era marker. Only the first carries data.
        rows.push_back({GLOBALS_CMPER, "40", {0, 192, 12, -12, 0, 0}});
        for (std::size_t i = 1; i < slots; ++i) {
            rows.push_back({GLOBALS_CMPER, "40", {0, 0, 0, 0, 0, 0}});
        }
        return makeContainer(rows);
    };
    const auto runImport = [](const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile, ImportReport& report) {
        const auto document = makeStemDocument();
        const auto reference = makeStemDocument();
        const auto index = LegacyRecordIndex::build(parsed);
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::options::importStemOptions(context);
        return document->getOptions()->get<musx::dom::options::StemOptions>();
    };

    constexpr std::size_t earlySlots = 32;
    constexpr std::size_t modernSlots = 128;
    // Staff positions become Evpu and Evpu becomes Efix, whatever the version says. The third
    // profile is a version from well outside the era, which the marker must override.
    auto coda = profileFor(0, 0);
    coda.epoch = FormatEpoch::CodaBanner;
    coda.version.reset();
    for (const auto& profile : {profileFor(3, 0), profileFor(3, 2), profileFor(15, 0), coda}) {
        ImportReport report(FormatEpoch::UncompressedLegacy);
        const auto options = runImport(tableWith(earlySlots), profile, report);
        expectMapping(options->stemLength == 7 * 12 && options->shortStemLength == 5 * 12 && options->revStemAdj == 18 * 12,
            "A pre-Finale-3.5 stem length was not converted from staff positions");
        expectMapping(options->stemConnections.size() == 1 && options->stemConnections[0]->upStemVert == 12 * 64,
            "A pre-Finale-3.5 stem adjustment was not converted from Evpu");
        // The half-stem length is not stored in that era, so it keeps the seeded default.
        expectMapping(field(report, "options.stemOptions.halfStemLength").origin == ValueOrigin::Finale27Default,
            "A pre-Finale-3.5 document claimed a half-stem length it does not store");
    }
    // The same words in the later shape are already in the modern units, and the version that
    // would have said otherwise is ignored.
    for (const auto& profile : {profileFor(3, 0), profileFor(5, 0), profileFor(15, 0)}) {
        ImportReport report(FormatEpoch::UncompressedLegacy);
        const auto options = runImport(tableWith(modernSlots), profile, report);
        expectMapping(options->stemLength == 7 && options->shortStemLength == 5 && options->revStemAdj == 18,
            "A Finale 3.5 stem length was scaled as though it were staff positions");
        expectMapping(options->stemConnections[0]->upStemVert == 12, "A Finale 3.5 stem adjustment was scaled as though it were Evpu");
    }
}

// Nothing about the collection may come from the pinned baseline, and a source that states
// no connections must produce none rather than inheriting three.
void testStemConnectionsAreSourceOwned()
{
    ImportReport report(FormatEpoch::UncompressedLegacy);
    const auto document = makeStemDocument();
    const auto options = captureStems(makeContainer({{GLOBALS_CMPER, "94", {1, 2, 3, 4, 5, 6}}}), profileFor(5, 0), document, report);
    expectMapping(options->stemConnections.empty(), "A document with no stem-connection record kept the seeded connections");
    expectMapping(options->stemLength == 84, "Clearing the collection disturbed the seeded scalars");
}

// A Finale 2012 record frequently holds a stale copy of the pre-Unicode default table, which
// through the widened element reads as one implausible symbol. Recovering nothing is what
// Finale sees; recovering the old table would assert a layout the era does not use.
void testStemStaleUnicodeRecord()
{
    using finale_mus_reader::numericGlobalClass;
    // The bytes an untouched Finale 2012 document carries: the pre-Unicode table, whose
    // first two words the widened element reads as the single symbol 0x030000c0.
    const auto parsed =
        makeClassContainer(numericGlobalClass(40), {0, 192, 768, -768, 0, 0, 0, 131, -2304, 2304, -1024, 1024, 0, 0}, ByteOrder::LittleEndian);
    SourceProfile profile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    SourceVersion version;
    version.major = 17;
    profile.version = version;

    ImportReport report(FormatEpoch::UncompressedLegacy);
    const auto document = makeStemDocument();
    const auto options = captureStems(parsed, profile, document, report);
    expectMapping(options->stemConnections.empty(), "A stale pre-Unicode stem record was read as though it were the widened layout");
    expectMapping(std::any_of(report.diagnostics.begin(), report.diagnostics.end(),
                      [](const auto& entry) {
                          return entry.level == musx::util::Logger::LogLevel::Verbose
                                 && entry.message.find("not a Unicode codepoint") != std::string::npos;
                      }),
        "The stale stem-connection record produced no Verbose diagnostic");
    expectMapping(anyMappingReportedField(report,
                      [](const auto& member, const auto& value) { return member == "stemConnections[0].symbol" && value.rawValue == 0x030000c0; }),
        "The out-of-range symbol was not reported as raw evidence");

    // The same words in a pre-Unicode file are the table they always were.
    SourceProfile earlier = profile;
    SourceVersion narrow;
    narrow.major = 13;
    earlier.version = narrow;
    ImportReport narrowReport(FormatEpoch::UncompressedLegacy);
    const auto narrowDocument = makeStemDocument();
    const auto narrowOptions = captureStems(parsed, earlier, narrowDocument, narrowReport);
    expectMapping(narrowOptions->stemConnections.size() == 2 && narrowOptions->stemConnections[0]->symbol == 192
                      && narrowOptions->stemConnections[1]->symbol == 131,
        "The twelve-byte zlib element stopped decoding before Finale 2012");
}

// A connection names its font by comparator. A dangling one is preserved rather than replaced,
// because a default would invent a typeface the source never named; registering it is what lets
// musxdom mint and log a placeholder for it at the end of construction instead of leaving the
// comparator unusable, which is what testDanglingFontComparatorRequiresAssignment verifies.
void testStemFontReferenceValidation()
{
    ImportReport report(FormatEpoch::UncompressedLegacy);
    const auto document = makeStemDocument();
    const auto options = captureStems(makeContainer({{GLOBALS_CMPER, "40", {7, 192, 768, -768, 0, 0}}, {GLOBALS_CMPER, "40", {0, 0, 0, 0, 0, 0}}}),
        profileFor(5, 0), document, report);
    expectMapping(options->stemConnections.size() == 1 && options->stemConnections[0]->fontId == 7,
        "A stem connection did not keep the font comparator the source stated");
}

// No real Finale save can stand in for a comparator that resolves to nothing: Finale always
// writes some definition for every comparator it uses, even a placeholder of its own. The
// state this reader must survive -- a hand-edited or otherwise malformed source naming a font
// id its own table never defines -- has to be built synthetically instead. This constructs it
// directly against musxdom's own construction session, twice: once with a raw assignment and
// once through the construction operation every importer assignment uses.
//
// Both documents give a TextOptions symbol insert a font id no FontDefinition in the document
// answers. A raw assignment remains unresolved; assignFontId registers the same value as part
// of the assignment and resolves it to musxdom's "Missing Font (n)" spelling at finish.
void testDanglingFontComparatorRequiresAssignment()
{
    using TextOptions = musx::dom::options::TextOptions;
    using Insert = musx::dom::options::AccidentalInsertSymbolType;
    constexpr musx::dom::Cmper danglingFontId = 909;

    const auto buildDocument = [](bool registerComparator) {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        auto options = std::make_shared<TextOptions>(document);
        options->textLineSpacingPercent = 100;
        auto insert = std::make_shared<TextOptions::InsertSymbolInfo>(options);
        auto font = std::make_shared<musx::dom::FontInfo>(document, /*sizeIsPercent*/ true);
        font->fontId = registerComparator ? session.getConstructionContext().assignFontId(danglingFontId) : danglingFontId;
        font->fontSize = 12;
        insert->symFont = std::move(font);
        options->symbolInserts[Insert::Sharp] = std::move(insert);
        document->getOptions()->add(TextOptions::XmlNodeName, options);
        return std::move(session).finish();
    };

    const auto getSymFont = [](const musx::dom::DocumentPtr& document) {
        return document->getOptions()->get<TextOptions>()->symbolInserts.at(Insert::Sharp)->symFont;
    };

    const auto unregistered = buildDocument(false);
    expectMapping(getSymFont(unregistered)->fontId == danglingFontId, "An unregistered dangling font comparator did not survive construction");
    bool threw = false;
    try {
        (void)getSymFont(unregistered)->getName();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    expectMapping(threw, "An unregistered dangling font comparator no longer throws out of FontInfo::getName; "
                         "the contrast this test relies on is gone, not just the registered half of it");

    const auto registered = buildDocument(true);
    expectMapping(getSymFont(registered)->fontId == danglingFontId, "A registered dangling font comparator did not survive construction");
    std::string name;
    bool registeredThrew = false;
    try {
        name = getSymFont(registered)->getName();
    } catch (const std::exception&) {
        registeredThrew = true;
    }
    expectMapping(!registeredThrew, "A registered dangling font comparator threw out of FontInfo::getName instead of "
                                    "resolving to musxdom's placeholder");
    expectMapping(name == "Missing Font (" + std::to_string(danglingFontId) + ")",
        "A registered dangling font comparator did not resolve to the placeholder spelling "
        "Finale's own conversions use");
}

void testStemConnectionCapture()
{
    using StemOptions = musx::dom::options::StemOptions;
    const auto read = [](const char* relative) { return readFixture(relative, fixtureLegacySymbolFonts); };
    const auto stems = [](const ImportResult& result) {
        const auto options = result.document->getOptions()->get<StemOptions>();
        expect(static_cast<bool>(options), "The imported document has no stem options");
        return options;
    };
    // Every era stores this connection for the default music font, so agreement across all
    // of them is what shows the three physical layouts describe one logical table. The
    // adjustments are Efix here even where the source stated Evpu.
    const auto expectDefaultConnection = [&](const auto& options, const char* era) {
        expect(!options->stemConnections.empty(), std::string("No stem connection was recovered from ") + era);
        const auto& first = options->stemConnections.front();
        expect(first->fontId == 0 && first->symbol == 192, std::string("The default stem connection was not recovered from ") + era);
        expect(first->upStemVert == 768 && first->downStemVert == -768 && first->upStemHorz == 0 && first->downStemHorz == 0,
            std::string("The default stem adjustments were wrong for ") + era);
    };

    // Coda banner. This era states the adjustments in Evpu, so 12 and -12 must arrive as 768
    // and -768. The report keeps the stored Evpu word.
    const auto f100 = read("evidence/F100/F100-baseline.mus");
    expect(f100.report.formatEpoch == FormatEpoch::CodaBanner, "The Finale 1.0.0 fixture is not the Coda-banner epoch");
    expectDefaultConnection(stems(f100), "Finale 1.0.0");
    expect(field(f100, "options.stemOptions.stemConnections[0].upStemVert").rawValue == 12,
        "The stored Evpu adjustment was not reported for Finale 1.0.0");
    expect(field(f100, "options.stemOptions.stemConnections[0].upStemVert").origin == ValueOrigin::LegacyMus,
        "A recovered stem adjustment was not reported as read from the source");
    expectDefaultConnection(stems(read("evidence/F263/F263-baseline.mus")), "Finale 2.6.3");

    // Uncompressed, from Finale 3.5 on: the same words, already in Efix. The raw report
    // value separates the two eras, because the assigned Efix cannot.
    const auto f372 = read("evidence/F372/F372-baseline.mus");
    expect(f372.report.formatEpoch == FormatEpoch::UncompressedLegacy, "The Finale 3.7.2 fixture is not the uncompressed epoch");
    expectDefaultConnection(stems(f372), "Finale 3.7.2");
    expect(field(f372, "options.stemOptions.stemConnections[0].upStemVert").rawValue == 768,
        "A Finale 3.7.2 adjustment was scaled as though it were Evpu");
    expectDefaultConnection(stems(read("evidence/F2000/F2000-baseline.mus")), "Finale 2000");

    // Finale 97 exercises the terminator case: 32 of its 128 incidences carry data, but only
    // three precede the first element with no symbol. The reader ignores the 29 after it.
    const auto f97 = read("evidence/F97/Fin97-baseline.mus");
    const auto f97Stems = stems(f97);
    expectDefaultConnection(f97Stems, "Finale 97");
    expect(f97Stems->stemConnections.size() == 3, "The stem-connection table did not stop at its terminator");
    const auto& flagUp = f97Stems->stemConnections[1];
    expect(flagUp->symbol == 131 && flagUp->upStemVert == -2304 && flagUp->downStemVert == 2304 && flagUp->upStemHorz == -1024
               && flagUp->downStemHorz == 1024,
        "The Finale 97 flag connection was not recovered");
    expect(f97Stems->stemConnections[2]->symbol == 132, "The third Finale 97 connection was not recovered");

    // DCL, then the zlib era's two element widths. Finale 2007 keeps the twelve-byte element
    // in a big-endian class record; Finale 2012 widened the symbol to a long, so reading it
    // as the narrow element would leave upStemVert at zero and slide the pair one word down.
    const auto f2005 = read("evidence/F2005/F2005-baseline.mus");
    expect(f2005.report.formatEpoch == FormatEpoch::DclLegacy, "The Finale 2005 fixture is not the DCL epoch");
    expectDefaultConnection(stems(f2005), "Finale 2005");

    const auto f2007 = read("evidence/F2007/F2007-lyric-hyphens.mus");
    expect(f2007.report.byteOrder == ByteOrder::BigEndian, "The Finale 2007 fixture is not big-endian");
    expectDefaultConnection(stems(f2007), "Finale 2007");

    const auto f2012 = read("evidence/F2012/F2012-upstem-flags.mus");
    expect(f2012.report.formatEpoch == FormatEpoch::ZlibLegacy, "The Finale 2012 fixture is not the zlib epoch");
    expectDefaultConnection(stems(f2012), "Finale 2012");

    // Nothing may come from the baseline. Every recovered connection is reported, and a
    // document that stores one connection has one, not the Finale 27 table's three.
    for (const auto* result : {&f100, &f372, &f2005, &f2007, &f2012}) {
        expect(result->document->getOptions()->get<StemOptions>()->stemConnections.size() == 1,
            "A single-connection document did not keep exactly its own connection");
    }
    expect(!anyReportedField(f100.report,
               [](const auto& member, const FieldInfo& value) {
                   return member.find("stemConnections") != std::string::npos && value.origin != ValueOrigin::LegacyMus;
               }),
        "A stem connection was reported as anything other than recovered");
}

TEST_CASE("Stem connection capture", "[class][reader]")
{
    testStemConnectionCapture();
}

void testStemScalarRecovery()
{
    using StemOptions = musx::dom::options::StemOptions;
    const auto read = [](const char* relative) {
        return Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / relative);
    };
    struct Expected
    {
        const char* path;
        const char* era;
        int halfStemLength;
        int stemLength;
        int shortStemLength;
        int revStemAdj;
        int stemWidth;
        int stemOffset;
        bool useStemConnections;
    };
    // Coda banner: the three lengths are stated in staff positions and scale by twelve, while
    // the half-stem length is not stored at all. Finale 1.0.0 stores point-valued stem width
    // and lift floats; Finale 2.6.3 migrates them into ten-thousandths fields, whose presence
    // makes them authoritative. Both forms convert to modern Efix here.
    const Expected fixtures[] = {
        {"evidence/F100/F100-baseline.mus", "Finale 1.0.0", 18, 84, 60, 216, 128, 128, false},
        {"evidence/F100/F100-stemdef-lines.mus", "edited Finale 1.0.0", 18, 84, 60, 216, 696, 414, false},
        {"evidence/F263/F263-baseline.mus", "Finale 2.6.3", 18, 84, 60, 216, 128, 128, true},
        {"evidence/F263/F263-F100-stemdef-lines.mus", "migrated Finale 2.6.3", 18, 84, 60, 216, 696, 414, false},
        // Finale 3.7 onward: every field in its own place, in Evpu and Efix.
        {"evidence/F372/F372-baseline.mus", "Finale 3.7.2", 18, 84, 60, 432, 118, 128, false},
        {"evidence/F97/Fin97-baseline.mus", "Finale 97", 18, 84, 60, 216, 128, 128, true},
        {"evidence/F2000/F2000-baseline.mus", "Finale 2000", 18, 84, 60, 432, 118, 256, false},
        {"evidence/F2005/F2005-baseline.mus", "Finale 2005", 18, 84, 60, 432, 224, 256, false},
        // The zlib era addresses the same options by byte offset. Finale 2007 is big-endian
        // and Finale 2012 little-endian, and the stem offset is the field that tells them
        // apart: it is two payload words, high word first, not a plain four-byte read.
        {"evidence/F2007/F2007-lyric-hyphens.mus", "Finale 2007", 18, 84, 60, 432, 224, 256, false},
        {"evidence/F2012/F2012-upstem-flags.mus", "Finale 2012", 18, 84, 60, 216, 115, 256, true},
    };
    for (const auto& fixture : fixtures) {
        const auto result = read(fixture.path);
        const auto stems = result.document->getOptions()->get<StemOptions>();
        expect(static_cast<bool>(stems), std::string("No stem options for ") + fixture.era);
        const auto wrong = [&](const char* field) { return std::string("The stem ") + field + " was wrong for " + fixture.era; };
        expect(stems->halfStemLength == fixture.halfStemLength, wrong("half length"));
        expect(stems->stemLength == fixture.stemLength, wrong("length"));
        expect(stems->shortStemLength == fixture.shortStemLength, wrong("short length"));
        expect(stems->revStemAdj == fixture.revStemAdj, wrong("reverse adjustment"));
        expect(stems->stemWidth == fixture.stemWidth, wrong("width"));
        expect(stems->stemOffset == fixture.stemOffset, wrong("offset"));
        expect(stems->useStemConnections == fixture.useStemConnections, wrong("connection switch"));
        // None of these fixtures sets the reverse-stemming bit.
        expect(!stems->noReverseStems, wrong("reverse-stemming bit"));
    }

    // What separates the two eras is provenance, not the value. Before Finale 3.5 the stored
    // word is a twelfth of the Evpu it becomes, and the report keeps that stored word; the
    // half-stem length reports as a Finale 27 default, while both sizes are source owned.
    const auto f263 = read("evidence/F263/F263-baseline.mus");
    expect(
        field(f263, "options.stemOptions.stemLength").rawValue == 7 && field(f263, "options.stemOptions.stemLength").origin == ValueOrigin::LegacyMus,
        "The stored staff-position stem length was not reported for the Coda era");
    expect(field(f263, "options.stemOptions.halfStemLength").origin == ValueOrigin::Finale27Default,
        "The unstated Coda half-stem length was not kept as a pinned default");
    for (const char* target : {"options.stemOptions.stemWidth", "options.stemOptions.stemOffset"}) {
        expect(
            field(f263, target).origin == ValueOrigin::LegacyMus, std::string("A stored Coda stem size was not reported as source owned: ") + target);
    }
    expect(field(f263, "options.stemOptions.stemWidth").sourceIdentity == finale_mus_reader::numericGlobalTag(64)
               && field(f263, "options.stemOptions.stemOffset").sourceIdentity == finale_mus_reader::numericGlobalTag(65),
        "The migrated Coda stem sizes did not report their later selectors");
    const auto f100 = read("evidence/F100/F100-stemdef-lines.mus");
    expect(field(f100, "options.stemOptions.stemWidth").sourceIdentity == finale_mus_reader::numericGlobalTag(54)
               && field(f100, "options.stemOptions.stemOffset").sourceIdentity == finale_mus_reader::numericGlobalTag(55),
        "The original Coda stem sizes did not report their float selectors");
    const auto f97 = read("evidence/F97/Fin97-baseline.mus");
    expect(field(f97, "options.stemOptions.stemLength").rawValue == 84, "A Finale 97 stem length was scaled as though it were staff positions");
    expect(field(f97, "options.stemOptions.stemWidth").origin == ValueOrigin::LegacyMus,
        "The Finale 97 stem thickness was not recovered from the source");

    // The packed spelling of the reverse-stemming flag: in Finale 2002 it is bit 2 of selector
    // 41 word 1, where Finale 1.0.0 and 3.7.2 use bit 0. The same fixture sets the normal stem
    // length to 96.
    const auto f2002Edit = read("evidence/F2002/F2002-norevstem-len96.mus");
    const auto f2002EditStems = f2002Edit.document->getOptions()->get<StemOptions>();
    expect(f2002EditStems->stemLength == 96, "The controlled Finale 2002 stem length was not recovered");
    expect(f2002EditStems->noReverseStems, "The packed reverse-stemming flag was not read from bit 2");
    expect(f2002EditStems->shortStemLength == 60 && f2002EditStems->halfStemLength == 18 && f2002EditStems->revStemAdj == 432,
        "A controlled Finale 2002 stem edit disturbed a field it did not touch");

    // Finale 3.7.2: the half-stem length is selector 03 word 2 (19 here), and "Display Reverse
    // Stemming" is bit 0 of selector 41 word 1, the Finale 1.0.0 spelling rather than the bit 2
    // of later eras.
    const auto f372Edit = read("evidence/F372/F372-revstem-halfstem.mus");
    const auto f372EditStems = f372Edit.document->getOptions()->get<StemOptions>();
    expect(f372EditStems->halfStemLength == 19, "The controlled Finale 3.7.2 half-stem length was not recovered");
    expect(f372EditStems->noReverseStems, "The Finale 3.7.2 reverse-stemming flag was not read from bit 0");
    expect(f372EditStems->stemLength == 84 && f372EditStems->shortStemLength == 60 && f372EditStems->revStemAdj == 432
               && f372EditStems->stemWidth == 118,
        "A controlled Finale 3.7.2 stem edit disturbed a field it did not touch");

    // Finale 1.0.0 states stem lengths in staff positions. This fixture lengthens the normal
    // and shortened stems by one staff position each and switches off "Display Reverse
    // Stemming".
    const auto changed = read("evidence/F100/F100-stemopts-changed.mus");
    const auto changedStems = changed.document->getOptions()->get<StemOptions>();
    expect(changedStems->stemLength == 96 && changedStems->shortStemLength == 72,
        "The controlled Finale 1.0.0 stem lengths were not converted from staff positions");
    expect(field(changed, "options.stemOptions.stemLength").rawValue == 8, "The stored staff-position count for the edited length was not reported");
    // The Coda era keeps this flag in bit 0 where every later era uses bit 2. Reading the
    // later bit here would leave it false.
    expect(changedStems->noReverseStems, "The Coda-era reverse-stemming flag was not recovered from its own bit");
    expect(!changedStems->useStemConnections && changedStems->revStemAdj == 216 && changedStems->halfStemLength == 18,
        "A controlled stem edit disturbed the fields it did not touch");

    // The reverse stem adjustment is the third length that era states in staff positions:
    // selector 21 word 2, here 25, converting to 300 Evpu at twelve Evpu per position.
    const auto revstem = read("evidence/F100/F100-revstem-25.mus");
    const auto revstemStems = revstem.document->getOptions()->get<StemOptions>();
    expect(revstemStems->revStemAdj == 300, "The controlled Finale 1.0.0 reverse stem adjustment was not converted");
    expect(field(revstem, "options.stemOptions.revStemAdj").rawValue == 25, "The stored staff-position reverse adjustment was not reported");
    expect(revstemStems->stemLength == 84 && revstemStems->shortStemLength == 60, "The reverse-adjustment save disturbed the two stem lengths");

    // In Finale 1.0.0 the stem-connection switch is selector 31 word 5.
    const auto enabled = read("evidence/F100/F100-stemconn-enabled.mus");
    const auto enabledStems = enabled.document->getOptions()->get<StemOptions>();
    expect(enabledStems->useStemConnections, "The controlled Finale 1.0.0 connection switch was not recovered");
    expect(field(enabled, "options.stemOptions.useStemConnections").origin == ValueOrigin::LegacyMus,
        "The recovered connection switch was not reported as read from the source");
    expect(enabledStems->stemLength == 84 && enabledStems->shortStemLength == 60 && !enabledStems->noReverseStems
               && enabledStems->stemConnections.size() == 1,
        "Enabling stem connections disturbed a field the save did not touch");

    // The third save chose "Disable" on a document that was already disabled and changed no
    // record at all, the Finale 1.0.0 dialog giving no indication of the current state. It is
    // the regression test for finding no difference where there is none.
    const auto disabled = read("evidence/F100/F100-stemconn-disabled.mus");
    const auto disabledStems = disabled.document->getOptions()->get<StemOptions>();
    expect(!disabledStems->useStemConnections && disabledStems->stemLength == 84 && disabledStems->shortStemLength == 60
               && !disabledStems->noReverseStems,
        "The Finale 1.0.0 connection switch save did not read like its baseline");
}

TEST_CASE("Stem scalar recovery", "[class][reader]")
{
    testStemScalarRecovery();
}

TEST_CASE("Stem pre-Finale-3.5 units", "[class]")
{
    testStemPreFinale35Units();
}

TEST_CASE("Stem connections are source owned", "[class]")
{
    testStemConnectionsAreSourceOwned();
}

TEST_CASE("Stale Unicode stem record", "[class]")
{
    testStemStaleUnicodeRecord();
}

TEST_CASE("Stem font reference validation", "[class]")
{
    testStemFontReferenceValidation();
}

TEST_CASE("Dangling font comparator requires construction assignment", "[class]")
{
    testDanglingFontComparatorRequiresAssignment();
}
} // namespace
} // namespace finale_mus_reader_tests
