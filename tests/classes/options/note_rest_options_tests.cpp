// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

using NoteRestOptionsTarget = musx::dom::options::NoteRestOptions;

musx::dom::DocumentPtr makeNoteRestOptionsDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<NoteRestOptionsTarget>(document);
    options->doShapeNotes = false;
    options->doCrossStaffNotes = true;
    options->drop8thRest = 81;
    options->drop16thRest = 161;
    options->drop32ndRest = 321;
    options->drop64thRest = 641;
    options->drop128thRest = 1281;
    options->scaleManualPositioning = false;
    options->drawOutline = true;
    for (std::uint16_t index = 0; index < 12; ++index) {
        auto color = std::make_shared<NoteRestOptionsTarget::NoteColor>();
        color->red = static_cast<std::uint16_t>(100 + index);
        color->green = static_cast<std::uint16_t>(200 + index);
        color->blue = static_cast<std::uint16_t>(300 + index);
        options->noteColors.push_back(std::move(color));
    }
    document->getOptions()->add(NoteRestOptionsTarget::XmlNodeName, options);
    return std::move(session).finish();
}

std::shared_ptr<const NoteRestOptionsTarget> importNoteRestOptions(
    const finale_mus_reader::container::ParsedContainer& parsed, FormatEpoch epoch, ImportReport& report)
{
    const auto document = makeNoteRestOptionsDocument();
    const auto reference = makeNoteRestOptionsDocument();
    auto profile = profileFor(epoch == FormatEpoch::ZlibLegacy ? 15 : 4, 0);
    profile.epoch = epoch;
    profile.byteOrder = parsed.byteOrder;
    if (epoch == FormatEpoch::CodaBanner) {
        profile.version.reset();
    }
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{
        LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::options::importNoteRestOptions(context);
    return document->getOptions()->get<NoteRestOptionsTarget>();
}

void verifyCommonNoteRestFields(const NoteRestOptionsTarget& options, const ImportReport& report, bool coda = false)
{
    expectMapping(options.doShapeNotes != coda && !options.doCrossStaffNotes && options.scaleManualPositioning,
        "NoteRestOptions did not recover the located preference flags");
    for (const auto* member : {"doShapeNotes", "doCrossStaffNotes", "scaleManualPositioning"}) {
        const auto expectedOrigin = coda && std::string_view(member) == "doShapeNotes"             ? ValueOrigin::Finale27Default
                                    : coda && std::string_view(member) == "scaleManualPositioning" ? ValueOrigin::LegacyBehavior
                                                                                                   : ValueOrigin::LegacyMus;
        expectMapping(field(report, std::string("options.noteRestOptions.").append(member)).origin == expectedOrigin,
            std::string("NoteRestOptions did not report ").append(member).append(" with the expected origin"));
    }
    expectMapping(options.drawOutline, "NoteRestOptions disturbed a seeded notehead-color field");
    expectMapping(field(report, "options.noteRestOptions.drawOutline").origin == ValueOrigin::Finale27Default,
        "NoteRestOptions did not report drawOutline as defaulted");
    expectMapping(options.noteColors.size() == 12 && options.noteColors[7]->red == 107 && options.noteColors[7]->green == 207
                      && options.noteColors[7]->blue == 307,
        "NoteRestOptions disturbed the seeded note-color collection");
    expectMapping(field(report, "options.noteRestOptions.noteColors[7].red").origin == ValueOrigin::Finale27Default
                      && field(report, "options.noteRestOptions.noteColors[7].green").origin == ValueOrigin::Finale27Default
                      && field(report, "options.noteRestOptions.noteColors[7].blue").origin == ValueOrigin::Finale27Default,
        "NoteRestOptions did not report its absent note-color leaves as defaults");
}

TEST_CASE("Note/rest options recover fixed-row preferences", "[class]")
{
    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        const std::vector<SyntheticRow> rows{
            {GLOBALS_CMPER, "01", {0, static_cast<std::int16_t>(epoch == FormatEpoch::DclLegacy ? 1 : 0), 0, 0, 0, 0}},
            {1, "CS", {0, 0, 0, 0, 0, static_cast<std::int16_t>(epoch == FormatEpoch::UncompressedLegacy ? 0x0080 : 0)}},
            {GLOBALS_CMPER, "12", {0, 0, 0, 0, 0, 0}},
            {GLOBALS_CMPER, "41", {0, 0, 0, -36, -60, 1}},
            {GLOBALS_CMPER, "44", {3, -5, 7, 0, 0, 0}},
            {GLOBALS_CMPER, "75", {0, 0, 0, 0, 0, 0}},
        };
        ImportReport report(epoch);
        const auto options = importNoteRestOptions(makeContainer(rows, epoch), epoch, report);
        verifyCommonNoteRestFields(*options, report);
        expectMapping(options->drop8thRest == 3 && options->drop16thRest == -5 && options->drop32ndRest == 7 && options->drop64thRest == -36
                          && options->drop128thRest == -60,
            "NoteRestOptions did not recover the fixed-row rest drops");
        for (const auto* member : {"drop8thRest", "drop16thRest", "drop32ndRest", "drop64thRest", "drop128thRest"}) {
            expectMapping(field(report, std::string("options.noteRestOptions.").append(member)).origin == ValueOrigin::LegacyMus,
                std::string("NoteRestOptions did not report ").append(member).append(" as LegacyMus"));
        }
    }
}

TEST_CASE("Late uncompressed note/rest options recover the relocated shape-note switch", "[class]")
{
    const std::vector<SyntheticRow> rows{
        {GLOBALS_CMPER, "01", {0, 1, 0, 0, 0, 0}},
        {GLOBALS_CMPER, "12", {0, 0, 0, 0, 0, 0}},
        {GLOBALS_CMPER, "41", {0, 0, 0, -24, -48, 0}},
    };
    ImportReport report(FormatEpoch::UncompressedLegacy);
    const auto options = importNoteRestOptions(makeContainer(rows, FormatEpoch::UncompressedLegacy), FormatEpoch::UncompressedLegacy, report);

    REQUIRE(options->doShapeNotes);
    const auto* shapeNotes = report.findField<NoteRestOptionsTarget>("doShapeNotes");
    REQUIRE(shapeNotes);
    REQUIRE(shapeNotes->origin == ValueOrigin::LegacyMus);
    REQUIRE(shapeNotes->sourceIdentity == finale_mus_reader::numericGlobalTag(1));
}

TEST_CASE("Note/rest options recover class-record preferences in either byte order", "[class]")
{
    const std::vector<SyntheticClassRow> rows{
        {finale_mus_reader::numericGlobalClass(1), {0, 1}},
        {finale_mus_reader::numericGlobalClass(12), {0, 0, 0, 0, 0}},
        {finale_mus_reader::numericGlobalClass(41), {0, 0, 0, -24, -48, 1}},
        {finale_mus_reader::numericGlobalClass(44), {3, -5, 7}},
        {finale_mus_reader::numericGlobalClass(75), {0, 0, 0, 0, 0, 0}},
    };
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        ImportReport report(FormatEpoch::ZlibLegacy);
        const auto options = importNoteRestOptions(makeClassContainer(rows, byteOrder), FormatEpoch::ZlibLegacy, report);
        verifyCommonNoteRestFields(*options, report);
        expectMapping(options->drop8thRest == 3 && options->drop16thRest == -5 && options->drop32ndRest == 7 && options->drop64thRest == -24
                          && options->drop128thRest == -48,
            "NoteRestOptions did not recover the class-record rest drops");
    }
}

TEST_CASE("Pre-editable note/rest options recover all stored rest-position "
          "fields",
    "[class]")
{
    const std::vector<SyntheticRow> rows{
        {GLOBALS_CMPER, "01", {0, 0, 0, 0, 0, 0}},
        {1, "CS", {0, 0, 0, 0, 0, 0x0080}},
        {GLOBALS_CMPER, "12", {0, 0, 0, 0, 0, 0}},
        {GLOBALS_CMPER, "41", {91, 92, 93, -36, -60, 1}},
        {GLOBALS_CMPER, "44", {3, -5, 7, 0, 0, 0}},
    };
    ImportReport report(FormatEpoch::UncompressedLegacy);
    const auto options = importNoteRestOptions(makeContainer(rows, FormatEpoch::UncompressedLegacy), FormatEpoch::UncompressedLegacy, report);
    verifyCommonNoteRestFields(*options, report);
    expectMapping(options->drop8thRest == 3 && options->drop16thRest == -5 && options->drop32ndRest == 7 && options->drop64thRest == -36
                      && options->drop128thRest == -60,
        "NoteRestOptions did not recover the pre-editable rest positions");
    for (const auto* member : {"drop8thRest", "drop16thRest", "drop32ndRest", "drop64thRest", "drop128thRest"}) {
        expectMapping(field(report, std::string("options.noteRestOptions.").append(member)).origin == ValueOrigin::LegacyMus,
            std::string("NoteRestOptions did not recover ").append(member));
    }
}

TEST_CASE("Earliest Coda note/rest options retain seeded rest-position defaults", "[class]")
{
    const std::vector<SyntheticRow> rows{
        {GLOBALS_CMPER, "01", {0, 1, 0, 0, 0, 0}},
        {GLOBALS_CMPER, "12", {0, 0, 0, 0, 0, 0}},
        {GLOBALS_CMPER, "41", {0, 0, 0, -36, -60, 1}},
    };
    ImportReport report(FormatEpoch::CodaBanner);
    const auto options = importNoteRestOptions(makeContainer(rows, FormatEpoch::CodaBanner), FormatEpoch::CodaBanner, report);
    verifyCommonNoteRestFields(*options, report, true);
    expectMapping(options->drop8thRest == 81 && options->drop16thRest == 161 && options->drop32ndRest == 321 && options->drop64thRest == 641
                      && options->drop128thRest == 1281,
        "NoteRestOptions disturbed the seeded Coda rest positions");
    expectMapping(field(report, "options.noteRestOptions.drop64thRest").origin == ValueOrigin::Finale27Default
                      && field(report, "options.noteRestOptions.drop128thRest").origin == ValueOrigin::Finale27Default,
        "NoteRestOptions did not report the absent Coda rest positions as defaults");
}

TEST_CASE("Coda note/rest options recover the shape-note switch", "[class]")
{
    const std::vector<SyntheticRow> rows{
        {1, "CS", {0, 0, 0, 0, 0, 0x0080}},
        {GLOBALS_CMPER, "12", {0, 0, 0, 0, 0, 0}},
    };
    ImportReport report(FormatEpoch::CodaBanner);
    const auto options = importNoteRestOptions(makeContainer(rows, FormatEpoch::CodaBanner), FormatEpoch::CodaBanner, report);

    REQUIRE(options->doShapeNotes);
    const auto& recovered = field(report, "options.noteRestOptions.doShapeNotes");
    REQUIRE(recovered.origin == ValueOrigin::LegacyMus);
    REQUIRE(recovered.sourceIdentity == finale_mus_reader::records::packTag("CS"));
}

TEST_CASE("Finale 1.0 distinguishes the shape-note switch from its Staff float gate", "[class][reader]")
{
    for (const auto& [fixture, expected] : {
             std::pair{"evidence/F100/F100-noteshapes-enable.mus", true},
             std::pair{"evidence/F100/F100-noteshapes-float.mus", false},
         }) {
        const auto result = readFixture(fixture);
        const auto options = result.document->getOptions()->get<NoteRestOptionsTarget>();
        REQUIRE(options);
        INFO(fixture);
        REQUIRE(options->doShapeNotes == expected);
        const auto& source = field(result.report, "options.noteRestOptions.doShapeNotes");
        REQUIRE(source.origin == ValueOrigin::LegacyMus);
        REQUIRE(source.sourceIdentity == finale_mus_reader::records::packTag("CS"));
    }
}

TEST_CASE("Finale 1.0 uses fixed rest-position behavior", "[class][reader]")
{
    const auto result = readFixture("evidence/F100/F100-baseline.mus");
    const auto options = result.document->getOptions()->get<NoteRestOptionsTarget>();
    REQUIRE(options);
    REQUIRE(options->drop8thRest == 0);
    REQUIRE(options->drop16thRest == 0);
    REQUIRE(options->drop32ndRest == 0);
    REQUIRE(options->drop64thRest == -24);
    REQUIRE(options->drop128thRest == -48);
    REQUIRE(field(result.report, "options.noteRestOptions.drop64thRest").origin == ValueOrigin::Finale27Default);
    REQUIRE(field(result.report, "options.noteRestOptions.drop128thRest").origin == ValueOrigin::Finale27Default);
}

TEST_CASE("Notehead colors recover from the structurally complete class record", "[class]")
{
    std::vector<std::int16_t> colorWords{1};
    for (std::size_t channel = 0; channel < 3; ++channel) {
        for (std::size_t color = 0; color < 12; ++color) {
            const auto value = channel == 0 && color == 0 ? std::uint16_t{40000} : static_cast<std::uint16_t>(channel * 1000 + color);
            colorWords.push_back(static_cast<std::int16_t>(value));
        }
    }
    colorWords.insert(colorWords.end(), 5, 0);

    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        ImportReport report(FormatEpoch::ZlibLegacy);
        const auto options = importNoteRestOptions(
            makeClassContainer(finale_mus_reader::numericGlobalClass(99), colorWords, byteOrder), FormatEpoch::ZlibLegacy, report);
        REQUIRE(options->drawOutline);
        REQUIRE(options->noteColors.size() == 12);
        REQUIRE(options->noteColors[0]->red == 40000);
        REQUIRE(options->noteColors[4]->green == 1004);
        REQUIRE(options->noteColors[11]->blue == 2011);
        for (const auto* member : {"drawOutline", "noteColors[0].red", "noteColors[4].green", "noteColors[11].blue"}) {
            const auto& recovered = field(report, std::string("options.noteRestOptions.").append(member));
            REQUIRE(recovered.origin == ValueOrigin::LegacyMus);
            REQUIRE(recovered.sourceIdentity == finale_mus_reader::numericGlobalClass(99));
        }
    }
}

TEST_CASE("A malformed notehead-color record retains the seeded collection", "[class]")
{
    ImportReport report(FormatEpoch::ZlibLegacy);
    const auto options =
        importNoteRestOptions(makeClassContainer(finale_mus_reader::numericGlobalClass(99), std::vector<std::int16_t>(41, 1), ByteOrder::BigEndian),
            FormatEpoch::ZlibLegacy, report);
    REQUIRE(options->drawOutline);
    REQUIRE(options->noteColors[7]->red == 107);
    REQUIRE(field(report, "options.noteRestOptions.drawOutline").origin == ValueOrigin::Finale27Default);
    REQUIRE(field(report, "options.noteRestOptions.noteColors[7].red").origin == ValueOrigin::Finale27Default);
    REQUIRE_FALSE(report.diagnostics.empty());
}

TEST_CASE("Finale 2008 stores the outline switch and planar notehead colors", "[class][reader]")
{
    struct FixtureCase
    {
        const char* fixture;
        bool drawOutline;
        std::uint16_t firstGreen;
        std::uint16_t lastBlue;
    };
    for (const auto& fixtureCase : {
             FixtureCase{"evidence/F2008/F2008-empty.mus", true, 7168, 39680},
             FixtureCase{"evidence/F2008/F2008-notehead-colors.mus", false, 37711, 50739},
         }) {
        const auto result = readFixture(fixtureCase.fixture);
        const auto options = result.document->getOptions()->get<NoteRestOptionsTarget>();
        REQUIRE(options);
        INFO(fixtureCase.fixture);
        REQUIRE(options->drawOutline == fixtureCase.drawOutline);
        REQUIRE(options->noteColors.size() == 12);
        REQUIRE(options->noteColors[0]->red == 57856);
        REQUIRE(options->noteColors[0]->green == fixtureCase.firstGreen);
        REQUIRE(options->noteColors[11]->blue == fixtureCase.lastBlue);
        for (const auto* member : {"drawOutline", "noteColors[0].red", "noteColors[0].green", "noteColors[11].blue"}) {
            const auto& recovered = field(result.report, std::string("options.noteRestOptions.").append(member));
            REQUIRE(recovered.origin == ValueOrigin::LegacyMus);
            REQUIRE(recovered.sourceIdentity == finale_mus_reader::numericGlobalClass(99));
        }
    }
}

TEST_CASE("Finale 2.6.3 stores all rest positions and realizes long-rest "
          "toggles",
    "[class][reader]")
{
    struct FixtureCase
    {
        const char* fixture;
        musx::dom::Evpu drop64th;
        musx::dom::Evpu drop128th;
    };
    for (const auto& fixtureCase : {
             FixtureCase{"evidence/F263/F263-baseline.mus", -24, -48},
             FixtureCase{"evidence/F263/F263-nodrop-64th.mus", 0, -48},
             FixtureCase{"evidence/F263/F263-nodrop-128th.mus", -24, 0},
         }) {
        const auto result = readFixture(fixtureCase.fixture);
        const auto options = result.document->getOptions()->get<NoteRestOptionsTarget>();
        REQUIRE(options);
        INFO(fixtureCase.fixture);
        REQUIRE(options->drop8thRest == 12);
        REQUIRE(options->drop16thRest == -12);
        REQUIRE(options->drop32ndRest == -12);
        REQUIRE(options->drop64thRest == fixtureCase.drop64th);
        REQUIRE(options->drop128thRest == fixtureCase.drop128th);
        for (const auto* member : {"drop8thRest", "drop16thRest", "drop32ndRest"}) {
            const auto* recovered = result.report.findField<NoteRestOptionsTarget>(member);
            REQUIRE(recovered);
            REQUIRE(recovered->origin == ValueOrigin::LegacyMus);
            REQUIRE(recovered->sourceIdentity == finale_mus_reader::numericGlobalTag(44));
        }
        for (const auto* member : {"drop64thRest", "drop128thRest"}) {
            const auto* recovered = result.report.findField<NoteRestOptionsTarget>(member);
            REQUIRE(recovered);
            REQUIRE(recovered->origin == ValueOrigin::LegacyMus);
            REQUIRE(recovered->sourceIdentity == finale_mus_reader::numericGlobalTag(41));
        }
    }
}

TEST_CASE("Controlled note/rest options preserve rest positions through Finale 2012", "[class][reader]")
{
    for (const auto& [fixture, sourceIdentity] : {
             std::pair{"evidence/F372/F372-noteopts.mus", finale_mus_reader::numericGlobalTag(44)},
             std::pair{"evidence/F2012/F2012-F372-noteopts.mus", finale_mus_reader::numericGlobalClass(44)},
         }) {
        const auto result = readFixture(fixture);
        const auto options = result.document->getOptions()->get<NoteRestOptionsTarget>();
        REQUIRE(options);
        INFO(fixture);
        REQUIRE(options->doShapeNotes);
        REQUIRE(options->drop8thRest == 3);
        REQUIRE(options->drop16thRest == -5);
        REQUIRE(options->drop32ndRest == 7);
        REQUIRE(options->drop64thRest == -23);
        REQUIRE(options->drop128thRest == -47);
        for (const auto* member : {"drop8thRest", "drop16thRest", "drop32ndRest"}) {
            const auto* recovered = result.report.findField<NoteRestOptionsTarget>(member);
            REQUIRE(recovered);
            REQUIRE(recovered->origin == ValueOrigin::LegacyMus);
            REQUIRE(recovered->sourceIdentity == sourceIdentity);
        }
        const auto* shapeNotes = result.report.findField<NoteRestOptionsTarget>("doShapeNotes");
        REQUIRE(shapeNotes);
        REQUIRE(shapeNotes->origin == ValueOrigin::LegacyMus);
        REQUIRE(shapeNotes->sourceIdentity
                == (sourceIdentity == finale_mus_reader::numericGlobalTag(44) ? finale_mus_reader::records::packTag("CS")
                                                                              : finale_mus_reader::numericGlobalClass(1)));
    }

    const auto result = readFixture("evidence/F372/F372-noteopts-noshapes.mus");
    const auto options = result.document->getOptions()->get<NoteRestOptionsTarget>();
    REQUIRE(options);
    REQUIRE_FALSE(options->doShapeNotes);
    REQUIRE(options->drop8thRest == 3);
    REQUIRE(options->drop16thRest == -5);
    REQUIRE(options->drop32ndRest == 7);
    REQUIRE(options->drop64thRest == -23);
    REQUIRE(options->drop128thRest == -47);
    const auto* shapeNotes = result.report.findField<NoteRestOptionsTarget>("doShapeNotes");
    REQUIRE(shapeNotes);
    REQUIRE(shapeNotes->origin == ValueOrigin::LegacyMus);
    REQUIRE(shapeNotes->sourceIdentity == finale_mus_reader::records::packTag("CS"));
}
} // namespace
} // namespace finale_mus_reader_tests
