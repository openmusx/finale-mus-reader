// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"
#include "import/details.h"
#include "staff_test_support.h"

#include "support/finale_version.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using Staff = musx::dom::others::Staff;

constexpr std::size_t staffFieldManifestSize = 101;
constexpr musx::dom::Cmper staffCmper1{1};
constexpr musx::dom::Cmper staffCmper7{7};
constexpr musx::dom::Cmper staffCmper9{9};
constexpr musx::dom::Cmper staffCmper10{10};
constexpr musx::dom::Cmper staffCmper11{11};
constexpr std::string_view staffAlternateNotationFields[] = {
    "altNotation",
    "altLayer",
    "altHideArtics",
    "altHideLyrics",
    "altHideSmartShapes",
    "altRhythmStemsUp",
    "altSlashDots",
    "altHideOtherNotes",
    "altHideOtherArtics",
    "altHideExpressions",
    "altHideOtherLyrics",
    "altHideOtherSmartShapes",
    "altHideOtherExpressions",
};

const FieldInfo& staffField(const ImportReport& report, std::string_view member, musx::dom::Cmper cmper = 7)
{
    const auto* value = report.findField(finale_mus_reader::instanceKey<Staff>(musx::dom::SCORE_PARTID, cmper), member);
    expect(value != nullptr, "Missing Staff report field " + std::string(member));
    return *value;
}

TEST_CASE("The Finale 2000 Staff base layout recovers its complete raw field "
          "surface")
{
    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            const auto parsed = makeContainer({{7, "IS", {-48, 0x0b17, static_cast<std::int16_t>(0xff12), 6, 0x1803, 0x69bd}},
                                                  {7, "IS", {0x0302, 1, 4, 48, static_cast<std::int16_t>(0xcfbd), static_cast<std::int16_t>(0xf5fb)}},
                                                  {7, "IS", {static_cast<std::int16_t>(0xfafb), static_cast<std::int16_t>(0xfdfc), -4, 9, 10, 0}}},
                epoch, byteOrder);
            const auto document = emptyStaffDocument();
            auto profile = SourceProfile(epoch);
            if (epoch == FormatEpoch::UncompressedLegacy) {
                SourceVersion version;
                version.major = finale_mus_reader::versions::finale2000.major;
                profile.version = version;
            }
            profile.byteOrder = byteOrder;
            const auto report = staffImport(parsed, profile, document);

            const auto staff = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 7);
            REQUIRE(staff);
            CHECK(staff->notationStyle == Staff::NotationStyle::Percussion);
            CHECK(staff->altNotation == Staff::AlternateNotation::Rhythmic);
            CHECK(staff->altLayer == 1);
            CHECK_FALSE(staff->altHideArtics);
            CHECK_FALSE(staff->altHideLyrics);
            CHECK_FALSE(staff->altHideSmartShapes);
            CHECK(staff->altRhythmStemsUp);
            CHECK(staff->altSlashDots);
            CHECK_FALSE(staff->altHideOtherNotes);
            CHECK_FALSE(staff->altHideOtherArtics);
            CHECK_FALSE(staff->altHideExpressions);
            CHECK(staff->useNoteShapes);
            CHECK(staff->useNoteFont);
            CHECK_FALSE(staff->hasStyles);
            CHECK(staff->hideFretboards);
            CHECK(staff->hideLyrics);
            CHECK(staff->showNameInParts);
            CHECK(staff->showNoteColors);
            REQUIRE(staff->noteFont);
            CHECK(staff->noteFont->fontId == 6);
            CHECK(staff->noteFont->fontSize == 24);
            CHECK(staff->noteFont->bold);
            CHECK(staff->noteFont->italic);
            CHECK(staff->defaultClef == 2);
            CHECK(staff->transposedClef == 3);
            CHECK_FALSE(staff->staffLines);
            REQUIRE(staff->customStaff);
            CHECK(*staff->customStaff == std::vector<int>{13});
            CHECK(staff->botBarlineOffset == -2 * evpusPerSpace);
            CHECK(staff->capoPos == 23);
            CHECK(staff->lowestFret == 11);
            CHECK(staff->topBarlineOffset == 2 * evpusPerSpace);
            REQUIRE(staff->transposition);
            REQUIRE(staff->transposition->keysig);
            CHECK(staff->transposition->setToClef);
            CHECK(staff->transposition->noSimplifyKey);
            CHECK(staff->transposition->keysig->interval == -2);
            CHECK(staff->transposition->keysig->adjust == -3);
            CHECK((staff->floatKeys && staff->floatTime && staff->blineBreak));
            CHECK((staff->hideMeasNums && staff->hideNameInScore && staff->noKey));
            CHECK(staff->hideChords);
            CHECK_FALSE(staff->redisplayLayerAccis);
            CHECK(staff->hideTimeSigs);
            CHECK(staff->hideTimeSigsInParts == staff->hideTimeSigs);
            CHECK_FALSE(staff->hideKeySigsShowAccis);
            CHECK(staff->dwRestOffset == -5);
            CHECK(staff->wRestOffset == -6);
            CHECK(staff->hRestOffset == -4);
            CHECK(staff->otherRestOffset == -3);
            CHECK(staff->stemReversal == -4);
            CHECK(staff->fullNameTextId == 9);
            CHECK(staff->abbrvNameTextId == 10);
            CHECK(staff->lineSpace == evpusPerSpace);
            CHECK(staff->botRepeatDotOff == -5);
            CHECK(staff->topRepeatDotOff == -3);
            CHECK(staffField(report, "notationStyle").origin == ValueOrigin::LegacyMus);
            CHECK(staffField(report, "capoPos").origin == ValueOrigin::LegacyMus);
            CHECK(staffField(report, "lowestFret").origin == ValueOrigin::LegacyMus);
            CHECK(staffField(report, "hideLyrics").origin == ValueOrigin::LegacyMus);
            CHECK(staffField(report, "altHideOtherLyrics").origin == ValueOrigin::LegacyMus);
            CHECK(staffField(report, "lineSpace").origin == ValueOrigin::Finale27Default);
            CHECK(staffField(report, "botRepeatDotOff").origin == ValueOrigin::LegacyBehavior);
            CHECK(staffField(report, "topRepeatDotOff").origin == ValueOrigin::LegacyBehavior);
            CHECK(staffField(report, "redisplayLayerAccis").origin == ValueOrigin::LegacyBehavior);
            CHECK(staffField(report, "hideTimeSigsInParts").origin == ValueOrigin::LegacyBehavior);
            CHECK(staffField(report, "hideKeySigsShowAccis").origin == ValueOrigin::LegacyBehavior);
            CHECK(staffField(report, "transposition.chromatic.alteration").origin == ValueOrigin::LegacyMus);
            CHECK(staffField(report, "transposition.chromatic.diatonic").origin == ValueOrigin::LegacyMus);
            CHECK(reportedFieldCount(report) == staffFieldManifestSize);
        }
    }
}

TEST_CASE("Finale 2000 custom Staff masks preserve line order across both words")
{
    for (const auto& [top, bottom, expected] : {
             std::tuple<std::int16_t, std::int16_t, std::vector<int>>{1, 0x000f, {11, 12, 13, 14}},
             std::tuple<std::int16_t, std::int16_t, std::vector<int>>{1, 0x001e, {12, 13, 14, 15}},
             std::tuple<std::int16_t, std::int16_t, std::vector<int>>{static_cast<std::int16_t>(0x8021), 0, {0, 10}},
             std::tuple<std::int16_t, std::int16_t, std::vector<int>>{1, 0, {}},
         }) {
        const auto parsed = makeContainer(
            {
                {7, "IS", {0, 0, 0, 0, 0, 0}},
                {7, "IS", {0, top, bottom, 0, 0, 0}},
                {7, "IS", {0, 0, 0, 0, 0, 0}},
            },
            FormatEpoch::UncompressedLegacy);
        const auto document = emptyStaffDocument();
        auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
        profile.version = SourceVersion{.major = finale_mus_reader::versions::finale2000.major};
        profile.byteOrder = ByteOrder::BigEndian;
        const auto report = staffImport(parsed, profile, document);

        const auto staff = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 7);
        REQUIRE(staff);
        REQUIRE(staff->customStaff);
        CHECK(*staff->customStaff == expected);
        if (expected.empty()) {
            CHECK(staff->botRepeatDotOff == -5);
            CHECK(staff->topRepeatDotOff == -3);
            CHECK(staffField(report, "botRepeatDotOff").origin == ValueOrigin::LegacyBehavior);
            CHECK(staffField(report, "topRepeatDotOff").origin == ValueOrigin::LegacyBehavior);
        }
    }

    struct CustomStaffCase
    {
        const char* path;
        std::vector<int> lines;
        int bottomRepeatDot;
        int topRepeatDot;
    };
    for (const auto& expected : {
             CustomStaffCase{"evidence/F2000/F2000-lines-upper4.mus", {11, 12, 13, 14}, -3, -1},
             CustomStaffCase{"evidence/F2000/F2000-lines-lower4.mus", {12, 13, 14, 15}, -5, -3},
             CustomStaffCase{"evidence/F2000/F2000-lines-cl10.mus", {10}, 1, 3},
             CustomStaffCase{"evidence/F2000/F2000-lines-cl0.mus", {0}, 21, 23},
         }) {
        const auto result = readFixture(expected.path);
        const auto staff = result.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
        REQUIRE(staff);
        REQUIRE(staff->customStaff);
        CHECK(*staff->customStaff == expected.lines);
        CHECK(staff->botRepeatDotOff == expected.bottomRepeatDot);
        CHECK(staff->topRepeatDotOff == expected.topRepeatDot);
        const auto* bottomDot = result.report.findField<Staff>("botRepeatDotOff", musx::dom::SCORE_PARTID, staffCmper1);
        const auto* topDot = result.report.findField<Staff>("topRepeatDotOff", musx::dom::SCORE_PARTID, staffCmper1);
        REQUIRE(bottomDot);
        REQUIRE(topDot);
        CHECK(bottomDot->origin == ValueOrigin::LegacyBehavior);
        CHECK(topDot->origin == ValueOrigin::LegacyBehavior);
    }
}

TEST_CASE("Legacy Staff alternate notation accepts the terminal Blank value")
{
    const auto parsed = makeContainer(
        {
            {7, "IS", {0, 0, 6, 0, 0, 0}},
            {7, "IS", {0, 0, 5, 0, 0, 0}},
            {7, "IS", {0, 0, 0, 0, 0, 0}},
        },
        FormatEpoch::UncompressedLegacy);
    const auto document = emptyStaffDocument();
    auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
    profile.version = SourceVersion{.major = finale_mus_reader::versions::finale2000.major};
    profile.byteOrder = ByteOrder::BigEndian;
    const auto report = staffImport(parsed, profile, document);

    const auto staff = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 7);
    REQUIRE(staff);
    CHECK(staff->altNotation == Staff::AlternateNotation::Blank);
    CHECK(staff->altHideSmartShapes);
    CHECK(staffField(report, "altHideSmartShapes").origin == ValueOrigin::LegacyMusAdjusted);
}

TEST_CASE("Finale 2000 through 2008 combine Smart Shape hiding sources")
{
    struct Expected
    {
        Staff::AlternateNotation notation;
        bool hideSmartShapes;
    };
    for (const auto expected : {
             Expected{Staff::AlternateNotation::Normal, false},
             Expected{Staff::AlternateNotation::SlashBeats, true},
             Expected{Staff::AlternateNotation::Rhythmic, false},
             Expected{Staff::AlternateNotation::OneBarRepeat, true},
             Expected{Staff::AlternateNotation::TwoBarRepeat, true},
             Expected{Staff::AlternateNotation::BlankWithRests, true},
             Expected{Staff::AlternateNotation::Blank, true},
         }) {
        const auto altFlags = std::int16_t(0x0100 | static_cast<std::int16_t>(expected.notation));
        const auto parsed = makeContainer(
            {{7, "IS", {0, 0, altFlags, 0, 0, 0}}, {7, "IS", {0, 0, 0, 0, 0, 0}}, {7, "IS", {0, 0, 0, 0, 0, 0}}}, FormatEpoch::UncompressedLegacy);
        const auto document = emptyStaffDocument();
        auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
        profile.version = SourceVersion{.major = finale_mus_reader::versions::finale2000.major};
        profile.byteOrder = ByteOrder::BigEndian;
        const auto report = staffImport(parsed, profile, document);
        const auto staff = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 7);

        REQUIRE(staff);
        CHECK(staff->altHideSmartShapes == expected.hideSmartShapes);
        CHECK(staffField(report, "altHideSmartShapes").origin == ValueOrigin::LegacyMusAdjusted);
    }
}

TEST_CASE("Finale 2009 directly recovers alternate-notation Smart Shape hiding")
{
    std::vector<std::int16_t> words(42, 0);
    words[2] = std::int16_t(0x0401);
    const auto parsed = makeClassContainer(0x00e7, words, ByteOrder::LittleEndian, 7);
    const auto document = emptyStaffDocument();
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.version = SourceVersion{.major = finale_mus_reader::versions::finale2009.major};
    profile.byteOrder = ByteOrder::LittleEndian;
    const auto report = staffImport(parsed, profile, document);
    const auto staff = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 7);

    REQUIRE(staff);
    CHECK(staff->altNotation == Staff::AlternateNotation::SlashBeats);
    CHECK_FALSE(staff->altHideSmartShapes);
    CHECK(staffField(report, "altHideSmartShapes").origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Finale 2000 through 2008 expand the note-attached-items setting "
          "without expressions")
{
    const auto baseline = readFixture("evidence/F2008/F2008-empty.mus");
    const auto baselineStaff = baseline.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(baselineStaff);
    CHECK_FALSE(baselineStaff->altHideExpressions);

    for (const auto* path : {
             "evidence/F2000/F2000-staff-unshownoteitems.mus",
             "evidence/F2006/F2006-staff-nonoteitems.mus",
             "evidence/F2008/F2008-staff-nohidenoteitems.mus",
         }) {
        const auto result = readFixture(path);
        const auto staff = result.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
        REQUIRE(staff);
        CHECK(staff->altNotation == Staff::AlternateNotation::Normal);
        CHECK(staff->altHideArtics);
        CHECK(staff->altHideLyrics);
        CHECK(staff->altHideSmartShapes);
        CHECK_FALSE(staff->altHideExpressions);
        CHECK(staff->hideFretboards);
        CHECK(staff->hideChords);
        CHECK_FALSE(staff->hasStyles);

        for (const auto* member : {"altHideArtics", "altHideLyrics", "hideFretboards", "hideChords"}) {
            const auto* field = result.report.findField<Staff>(member, musx::dom::SCORE_PARTID, staffCmper1);
            REQUIRE(field);
            CHECK(field->origin == ValueOrigin::LegacyMus);
        }
        CHECK(staffField(result.report, "altHideSmartShapes", staffCmper1).origin == ValueOrigin::LegacyMusAdjusted);
        CHECK(staffField(result.report, "altHideExpressions", staffCmper1).origin == ValueOrigin::Finale27Default);
        const auto* hasStyles = result.report.findField<Staff>("hasStyles", musx::dom::SCORE_PARTID, staffCmper1);
        REQUIRE(hasStyles);
        CHECK(hasStyles->origin == ValueOrigin::LegacyMusAdjusted);
    }
}

TEST_CASE("Short post-Finale-2000 Staff layouts expand the aggregate "
          "other-layer settings")
{
    struct Expected
    {
        const char* path;
        bool hideOtherNotes;
        bool hideOtherItems;
    };
    for (const auto expected : {
             Expected{"evidence/F2000/F2000-staff-hideothernotes.mus", true, false},
             Expected{"evidence/F2000/F2000-staff-hideotheritems.mus", false, true},
         }) {
        const auto result = readFixture(expected.path);
        const auto staff = result.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
        REQUIRE(staff);

        CHECK_FALSE(staff->altHideArtics);
        CHECK_FALSE(staff->altHideLyrics);
        CHECK_FALSE(staff->altHideSmartShapes);
        CHECK_FALSE(staff->altHideExpressions);
        CHECK(staff->altHideOtherNotes == expected.hideOtherNotes);
        CHECK(staff->altHideOtherArtics == expected.hideOtherItems);
        CHECK(staff->altHideOtherLyrics == expected.hideOtherItems);
        CHECK(staff->altHideOtherSmartShapes == expected.hideOtherItems);
        CHECK(staff->altHideOtherExpressions == expected.hideOtherItems);
        for (const auto member : staffAlternateNotationFields) {
            const auto* field = result.report.findField<Staff>(member, musx::dom::SCORE_PARTID, staffCmper1);
            REQUIRE(field);
            CHECK(field->origin
                  == (member == "altHideSmartShapes"   ? ValueOrigin::LegacyMusAdjusted
                      : member == "altHideExpressions" ? ValueOrigin::Finale27Default
                                                       : ValueOrigin::LegacyMus));
        }
    }
}

TEST_CASE("Pre-Finale-2000 Staff alternate notation is legacy "
          "behavior")
{
    for (const auto* path : {
             "evidence/F98/F98-baseline.mus",
             "evidence/F98/F98-altnotation-full.mus",
             "evidence/F98/F98-altnotation-partial.mus",
         }) {
        const auto result = readFixture(path);
        const auto staff = result.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
        REQUIRE(staff);

        CHECK(staff->altNotation == Staff::AlternateNotation::Normal);
        CHECK(staff->altLayer == 0);
        CHECK_FALSE(staff->altHideArtics);
        CHECK_FALSE(staff->altHideLyrics);
        CHECK_FALSE(staff->altHideSmartShapes);
        CHECK_FALSE(staff->altRhythmStemsUp);
        CHECK(staff->altSlashDots);
        CHECK_FALSE(staff->altHideOtherNotes);
        CHECK_FALSE(staff->altHideOtherArtics);
        CHECK_FALSE(staff->altHideExpressions);
        CHECK_FALSE(staff->altHideOtherLyrics);
        CHECK_FALSE(staff->altHideOtherSmartShapes);
        CHECK_FALSE(staff->altHideOtherExpressions);

        for (const auto member : staffAlternateNotationFields) {
            const auto* field = result.report.findField<Staff>(member, musx::dom::SCORE_PARTID, staffCmper1);
            REQUIRE(field);
            CHECK(field->origin == ValueOrigin::LegacyBehavior);
        }
    }
}

TEST_CASE("Finale 3.7.2 uses the later Staff notehead-font representation")
{
    const auto result = readFixture("evidence/F372/F372-notehead-font.mus");
    const auto staff = result.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(staff);
    REQUIRE(staff->noteFont);
    CHECK(staff->useNoteFont);
    CHECK(staff->noteFont->fontId == 6);
    CHECK(staff->noteFont->fontSize == 23);
    CHECK_FALSE(staff->noteFont->bold);
    CHECK_FALSE(staff->noteFont->italic);
    CHECK_FALSE(staff->noteFont->underline);

    for (const auto* member : {
             "useNoteFont",
             "noteFont.fontId",
             "noteFont.fontSize",
         }) {
        const auto* field = result.report.findField<Staff>(member, musx::dom::SCORE_PARTID, staffCmper1);
        REQUIRE(field);
        CHECK(field->origin == ValueOrigin::LegacyMus);
    }
}

TEST_CASE("The zlib Staff class retains the base words and its established "
          "extension")
{
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        std::vector<std::int16_t> words{0, 0, 0x0700, 0, 0, 0, 0, 0, 5, 0, 0, 0, static_cast<std::int16_t>(0xfcfc), static_cast<std::int16_t>(0xfcfc),
            -4, 0, 0, static_cast<std::int16_t>(0xfdfb)};
        const auto appendLong = [&](std::int32_t value) {
            const auto bits = static_cast<std::uint32_t>(value);
            const auto high = static_cast<std::int16_t>(bits >> 16U);
            const auto low = static_cast<std::int16_t>(bits & 0xffffU);
            if (byteOrder == ByteOrder::BigEndian) {
                words.insert(words.end(), {high, low});
            } else {
                words.insert(words.end(), {low, high});
            }
        };
        if (byteOrder == ByteOrder::BigEndian) {
            words.insert(words.end(), {0, 1536, -1, -1024});
        } else {
            words.insert(words.end(), {1536, 0, -1024, -1});
        }
        words.insert(words.end(), {static_cast<std::int16_t>(0x9bb4), 23});
        appendLong(-64);
        appendLong(128);
        appendLong(-192);
        appendLong(256);
        appendLong(-320);
        appendLong(384);
        words.push_back(0x0002);
        const auto parsed = makeClassContainer(0x00e7, words, byteOrder, 7);
        const auto document = emptyStaffDocument();
        auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
        profile.byteOrder = byteOrder;
        const auto report = staffImport(parsed, profile, document);

        const auto staff = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 7);
        REQUIRE(staff);
        REQUIRE(staff->staffLines);
        CHECK(*staff->staffLines == 5);
        CHECK(staff->lineSpace == evpusPerSpace);
        CHECK(staff->botRepeatDotOff == -5);
        CHECK(staff->topRepeatDotOff == -3);
        CHECK(staff->vertTabNumOff == -1024);
        CHECK(staff->showTabClefAllSys);
        CHECK(staff->hideRests);
        CHECK_FALSE(staff->hideTies);
        CHECK(staff->hideDots);
        CHECK_FALSE(staff->hideStems);
        CHECK(staff->stemDirection == musx::dom::StemDirection::AlwaysDown);
        CHECK(staff->stemStartFromStaff);
        CHECK(staff->stemsFixedEnd);
        CHECK(staff->useTabLetters);
        CHECK(staff->hideBeams);
        CHECK(staff->breakTabLinesAtNotes);
        CHECK(staff->stemsFixedStart);
        CHECK(staff->hideTuplets);
        CHECK(staff->hideMode == Staff::HideMode::ScoreParts);
        CHECK(staff->fretInstId == 23);
        CHECK(staff->horzStemOffUp == -64);
        CHECK(staff->horzStemOffDown == 128);
        CHECK(staff->vertStemStartOffUp == -192);
        CHECK(staff->vertStemStartOffDown == 256);
        CHECK(staff->vertStemEndOffUp == -320);
        CHECK(staff->vertStemEndOffDown == 384);
        CHECK(staff->altHideOtherLyrics);
        CHECK_FALSE(staff->altHideOtherSmartShapes);
        CHECK(staff->altHideOtherExpressions);
        CHECK(staffField(report, "lineSpace").origin == ValueOrigin::LegacyMus);
        CHECK(staffField(report, "lineSpace").rawValue == 1536);
        CHECK(staffField(report, "vertTabNumOff").origin == ValueOrigin::LegacyMus);
        CHECK(staffField(report, "vertTabNumOff").rawValue == -1024);
        CHECK(staffField(report, "stemDirection").origin == ValueOrigin::LegacyMus);
        CHECK(staffField(report, "fretInstId").rawValue == 23);
        CHECK(staffField(report, "horzStemOffUp").rawValue == -64);
        CHECK(staffField(report, "altHideOtherLyrics").origin == ValueOrigin::LegacyMus);
        CHECK(reportedFieldCount(report) == staffFieldManifestSize);
    }
}

TEST_CASE("Controlled Finale 2005 tablature settings recover from Staff")
{
    struct TabCase
    {
        const char* fixture;
        int capoPos;
        int lowestFret;
        bool showClefs;
        bool useLetters;
        bool breakLines;
        bool hideTuplets;
    };
    constexpr TabCase cases[]{
        {"evidence/F2005/F2005-tab-breaklines.mus", 23, 11, false, false, true, false},
        {"evidence/F2005/F2005-tab-notuplets.mus", 5, 13, false, false, false, true},
        {"evidence/F2005/F2005-tab-showclefs.mus", 23, 11, true, false, false, false},
        {"evidence/F2005/F2005-tab-showletters.mus", 23, 11, false, true, false, false},
    };

    for (const auto& expected : cases) {
        const auto result = readFixture(expected.fixture);
        const auto staff = result.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
        REQUIRE(staff);
        CHECK(staff->notationStyle == Staff::NotationStyle::Tablature);
        CHECK(staff->capoPos == expected.capoPos);
        CHECK(staff->lowestFret == expected.lowestFret);
        CHECK(staff->showTabClefAllSys == expected.showClefs);
        CHECK(staff->useTabLetters == expected.useLetters);
        CHECK(staff->breakTabLinesAtNotes == expected.breakLines);
        CHECK(staff->hideTuplets == expected.hideTuplets);
        CHECK(staff->fretInstId == 2);
        CHECK(staff->vertTabNumOff == -1088);

        const auto* capoSource = result.report.findField<Staff>("capoPos", musx::dom::SCORE_PARTID, staffCmper1);
        const auto* lowestFretSource = result.report.findField<Staff>("lowestFret", musx::dom::SCORE_PARTID, staffCmper1);
        REQUIRE(capoSource);
        REQUIRE(lowestFretSource);
        CHECK(capoSource->origin == ValueOrigin::LegacyMus);
        CHECK(lowestFretSource->origin == ValueOrigin::LegacyMus);
    }
}

TEST_CASE("Staff hiding changes representation in Finale 2011")
{
    std::vector<std::int16_t> words(23, 0);
    words[11] = 0x0004;
    words[22] = static_cast<std::int16_t>(0x8000);
    const auto parsed = makeClassContainer(0x00e7, words, ByteOrder::BigEndian, 7);

    const auto importAt = [&](finale_mus_reader::VersionBound version) {
        const auto document = emptyStaffDocument();
        auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
        profile.version = SourceVersion{.major = version.major, .minor = version.minor, .maint = version.maint};
        profile.byteOrder = ByteOrder::BigEndian;
        auto report = staffImport(parsed, profile, document);
        const auto staff = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 7);
        REQUIRE(staff);
        return std::pair{staff, std::move(report)};
    };

    const auto [finale2010Staff, finale2010Report] = importAt(finale_mus_reader::versions::finale2010);
    CHECK(finale2010Staff->hideMode == Staff::HideMode::Cutaway);
    CHECK(staffField(finale2010Report, "hideMode").origin == ValueOrigin::LegacyMus);

    const auto [finale2011Staff, finale2011Report] = importAt(finale_mus_reader::versions::finale2011);
    CHECK(finale2011Staff->hideMode == Staff::HideMode::ScoreParts);
    CHECK(staffField(finale2011Report, "hideMode").origin == ValueOrigin::LegacyMus);
}

TEST_CASE("The six-word Staff layout survives the uncompressed container "
          "transition")
{
    const auto parsed = makeContainer(
        {
            {7, "IS", {4, 0, 0, 0, 0, 0x0010}},
            {7, "IA", {0, 0, 3, 0, 0, 0x0040}},
        },
        FormatEpoch::UncompressedLegacy);
    const auto document = emptyStaffDocument();
    auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
    profile.version = SourceVersion{.major = 3, .minor = 0};
    profile.byteOrder = ByteOrder::BigEndian;
    const auto report = staffImport(parsed, profile, document);

    const auto staff = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 7);
    REQUIRE(staff);
    CHECK(staff->defaultClef == 4);
    CHECK(staff->hideTimeSigs);
    CHECK(staff->staffLines == 3);
    CHECK(staffField(report, "defaultClef").origin == ValueOrigin::LegacyMus);
    CHECK(staffField(report, "staffLines").origin == ValueOrigin::LegacyMus);
    CHECK(staffField(report, "lineSpace").origin == ValueOrigin::Finale27Default);
    CHECK(reportedFieldCount(report) == staffFieldManifestSize);
}

TEST_CASE("The distinct Coda Staff layout constructs a valid explicitly "
          "partial Staff")
{
    const auto parsed = makeContainer({{7, "IS", {0, 0, 4, 1024, 0, 0}}}, FormatEpoch::CodaBanner);
    const auto document = emptyStaffDocument();
    auto profile = SourceProfile(FormatEpoch::CodaBanner);
    profile.byteOrder = ByteOrder::BigEndian;
    const auto report = staffImport(parsed, profile, document);

    const auto staff = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 7);
    REQUIRE(staff);
    CHECK(staff->staffLines == 5);
    REQUIRE(staff->noteFont);
    CHECK(staff->noteFont->fontSize == 24);
    CHECK(staff->lineSpace == evpusPerSpace);
    CHECK(staff->dwRestOffset == -4);
    CHECK(staff->wRestOffset == -4);
    CHECK(staff->hRestOffset == -4);
    CHECK(staff->otherRestOffset == -4);
    CHECK(staff->stemReversal == -4);
    CHECK(staff->botRepeatDotOff == -5);
    CHECK(staff->topRepeatDotOff == -3);
    CHECK(staff->altNotation == Staff::AlternateNotation::Normal);
    CHECK(staff->altLayer == 0);
    CHECK_FALSE(staff->altHideArtics);
    CHECK_FALSE(staff->altHideLyrics);
    CHECK_FALSE(staff->altHideSmartShapes);
    CHECK_FALSE(staff->altRhythmStemsUp);
    CHECK(staff->altSlashDots);
    CHECK_FALSE(staff->altHideOtherNotes);
    CHECK_FALSE(staff->altHideOtherArtics);
    CHECK_FALSE(staff->altHideExpressions);
    CHECK_FALSE(staff->altHideOtherLyrics);
    CHECK_FALSE(staff->altHideOtherSmartShapes);
    CHECK_FALSE(staff->altHideOtherExpressions);
    CHECK_FALSE(staff->redisplayLayerAccis);
    CHECK_FALSE(staff->hideTimeSigsInParts);
    CHECK_FALSE(staff->hideKeySigsShowAccis);
    CHECK(staff->instUuid == musx::dom::uuid::Unknown);
    CHECK(staffField(report, "staffLines").origin == ValueOrigin::LegacyBehavior);
    CHECK(staffField(report, "noteFont.fontSize").origin == ValueOrigin::Finale27Default);
    CHECK(staffField(report, "lineSpace").origin == ValueOrigin::Finale27Default);
    CHECK(staffField(report, "dwRestOffset").origin == ValueOrigin::Finale27Default);
    CHECK(staffField(report, "stemReversal").origin == ValueOrigin::Finale27Default);
    CHECK(staffField(report, "fretInstId").origin == ValueOrigin::Finale27Default);
    CHECK(staffField(report, "botRepeatDotOff").origin == ValueOrigin::Finale27Default);
    CHECK(staffField(report, "topRepeatDotOff").origin == ValueOrigin::Finale27Default);
    for (const auto* member : {
             "transposedClef",
             "capoPos",
             "lowestFret",
             "showNameInParts",
             "showNoteColors",
             "hideRepeatBottomDot",
             "hideRepeatTopDot",
             "flatBeams",
             "hideFretboards",
             "hideLyrics",
             "noOptimize",
             "hideBarlines",
             "hideRptBars",
             "hideChords",
         }) {
        CHECK(staffField(report, member).origin == ValueOrigin::Finale27Default);
    }
    CHECK(staffField(report, "defaultClef").origin == ValueOrigin::LegacyMus);
    CHECK(staffField(report, "hideTimeSigs").origin == ValueOrigin::LegacyMus);
    for (const auto* member : {"customStaff", "notationStyle", "useNoteShapes", "useNoteFont", "blankMeasure"}) {
        CHECK(staffField(report, member).origin == ValueOrigin::Finale27Default);
    }
    CHECK(staffField(report, "vertTabNumOff").origin == ValueOrigin::LegacyBehavior);
    for (const auto member : staffAlternateNotationFields) {
        CHECK(staffField(report, member).origin == ValueOrigin::LegacyBehavior);
    }
    CHECK(staffField(report, "redisplayLayerAccis").origin == ValueOrigin::LegacyBehavior);
    CHECK(staffField(report, "hideTimeSigsInParts").origin == ValueOrigin::LegacyBehavior);
    CHECK(staffField(report, "hideKeySigsShowAccis").origin == ValueOrigin::LegacyBehavior);
    CHECK(staffField(report, "instUuid").origin == ValueOrigin::LegacyBehavior);
    CHECK(staffField(report, "autoNumbering").origin == ValueOrigin::LegacyBehavior);
    CHECK(staffField(report, "useAutoNumbering").origin == ValueOrigin::LegacyBehavior);
    CHECK(reportedFieldCount(report) == staffFieldManifestSize);
}

TEST_CASE("Coda Staff default clef occupies the low three bits of its display word")
{
    const auto parsed = makeContainer({{7, "IS", {7, 0, 4, 1024, 0, 0x051c}}}, FormatEpoch::CodaBanner);
    const auto document = emptyStaffDocument();
    auto profile = SourceProfile(FormatEpoch::CodaBanner);
    profile.byteOrder = ByteOrder::BigEndian;
    const auto report = staffImport(parsed, profile, document);

    const auto staff = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 7);
    REQUIRE(staff);
    CHECK(staff->defaultClef == 4);
    CHECK(staff->hideClefs);
    CHECK(staffField(report, "defaultClef").origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Coda Staff display flags retain their later meanings")
{
    constexpr std::int16_t displayFlags = static_cast<std::int16_t>(0xf738);
    const auto parsed = makeContainer({{7, "IS", {0, 0, 4, 1024, 0, displayFlags}}}, FormatEpoch::CodaBanner);
    const auto document = emptyStaffDocument();
    auto profile = SourceProfile(FormatEpoch::CodaBanner);
    profile.byteOrder = ByteOrder::BigEndian;
    const auto report = staffImport(parsed, profile, document);

    const auto staff = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 7);
    REQUIRE(staff);
    CHECK(staff->floatKeys);
    CHECK(staff->floatTime);
    CHECK(staff->blineBreak);
    CHECK(staff->rbarBreak);
    CHECK(staff->hideMeasNums);
    CHECK(staff->hideRepeats);
    CHECK(staff->hideNameInScore);
    CHECK(staff->hideKeySigs);
    CHECK(staff->hideTimeSigs);
    CHECK(staff->hideTimeSigsInParts);
    CHECK(staff->hideClefs);
    CHECK(staff->noKey);
    for (const auto* member : {
             "floatKeys",
             "floatTime",
             "blineBreak",
             "rbarBreak",
             "hideMeasNums",
             "hideRepeats",
             "hideNameInScore",
             "hideKeySigs",
             "hideTimeSigs",
             "hideClefs",
         }) {
        CHECK(staffField(report, member).origin == ValueOrigin::LegacyMus);
    }
    CHECK(staffField(report, "noKey").origin == ValueOrigin::LegacyMusAdjusted);
}

TEST_CASE("Early uncompressed six-word Staff keeps key controls separate")
{
    const auto parsed = makeContainer({{7, "IS", {0, 0, 4, 1024, 0, 0x0020}}}, FormatEpoch::UncompressedLegacy);
    const auto document = emptyStaffDocument();
    auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    const auto report = staffImport(parsed, profile, document);

    const auto staff = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 7);
    REQUIRE(staff);
    CHECK(staff->hideKeySigs);
    CHECK_FALSE(staff->noKey);
    CHECK(staffField(report, "hideKeySigs").origin == ValueOrigin::LegacyMus);
    CHECK(staffField(report, "noKey").origin == ValueOrigin::Finale27Default);
}

TEST_CASE("Coda Staff attributes distinguish line-count and one-line forms")
{
    const auto parsed = makeContainer(
        {
            {7, "IS", {0, 0, 4, 1024, 0, 0}},
            {7, "IA", {0, 0, 0, 0, 0, 0x0040}},
            {8, "IS", {0, 0, 4, 1024, 0, 0}},
            {8, "IA", {0, 0, 3, 0, 0, 0x0040}},
            {9, "IS", {0, 0, 4, 1024, 0, 0}},
            {9, "IA", {0, 0, 1, 0, 0, 0x0040}},
            {10, "IS", {0, 0, 4, 1024, 0, 0}},
            {10, "IA", {0, -948, -1, 2, 0x0900, 0x0340}},
        },
        FormatEpoch::CodaBanner);
    const auto document = emptyStaffDocument();
    auto profile = SourceProfile(FormatEpoch::CodaBanner);
    profile.byteOrder = ByteOrder::BigEndian;
    const auto report = staffImport(parsed, profile, document);

    const auto fiveLine = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 7);
    const auto threeLine = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 8);
    const auto centered = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 9);
    const auto legacyTab = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 10);
    REQUIRE(fiveLine);
    REQUIRE(threeLine);
    REQUIRE(centered);
    REQUIRE(legacyTab);
    CHECK(fiveLine->staffLines == 0);
    CHECK(fiveLine->botRepeatDotOff == -5);
    CHECK(fiveLine->topRepeatDotOff == -3);
    CHECK(threeLine->staffLines == 3);
    CHECK_FALSE(fiveLine->useNoteFont);
    REQUIRE(centered->customStaff);
    CHECK(*centered->customStaff == std::vector<int>{13});
    CHECK(centered->botBarlineOffset == -2 * evpusPerSpace);
    CHECK(centered->topBarlineOffset == 2 * evpusPerSpace);
    REQUIRE(legacyTab->customStaff);
    CHECK(*legacyTab->customStaff == std::vector<int>{11});
    CHECK(legacyTab->blankMeasure);
    CHECK(legacyTab->vertTabNumOff == -1024);
    CHECK(legacyTab->botRepeatDotOff == -1);
    CHECK(legacyTab->topRepeatDotOff == 1);
    CHECK(legacyTab->fretInstId == 2);
    const auto fret = document->getOthers()->get<musx::dom::others::FretInstrument>(musx::dom::SCORE_PARTID, legacyTab->fretInstId);
    REQUIRE(fret);
    CHECK(fret->numFrets == 20);
    CHECK(fret->numStrings == 1);
    CHECK(fret->name == "E5");
    REQUIRE(fret->strings.size() == 1);
    CHECK(fret->strings.front()->pitch == 76);
    const auto* fretSource = report.findField<Staff>("fretInstId", musx::dom::SCORE_PARTID, staffCmper10);
    REQUIRE(fretSource);
    CHECK(fretSource->origin == ValueOrigin::LegacyBehavior);
}

TEST_CASE("Coda Staff attributes recover independent note settings")
{
    const auto parsed = makeContainer(
        {
            {7, "IS", {0, 0, 4, 1024, 0, 0}},
            {7, "IA", {0, 0, 0, 5, 0x0f00, 0x00a0}},
            {8, "IS", {0, 0, 4, 1024, 0, 0}},
            {8, "IA", {0, 0, 0, 2, 0, 0x0200}},
            {9, "IS", {0, 0, 4, 1024, 0, 0}},
            {9, "IA", {0, 0, 0, 3, 0, 0x0200}},
        },
        FormatEpoch::CodaBanner);
    const auto document = emptyStaffDocument(2);
    auto profile = SourceProfile(FormatEpoch::CodaBanner);
    profile.byteOrder = ByteOrder::BigEndian;
    const auto report = staffImport(parsed, profile, document);

    const auto standardStaff = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 7);
    const auto defaultTabFontStaff = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 8);
    const auto independentTabFontStaff = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 9);
    REQUIRE(standardStaff);
    REQUIRE(defaultTabFontStaff);
    REQUIRE(independentTabFontStaff);
    CHECK(standardStaff->useNoteFont);
    CHECK(standardStaff->useNoteShapes);
    REQUIRE(standardStaff->noteFont);
    CHECK(standardStaff->noteFont->fontId == 5);
    CHECK(standardStaff->noteFont->fontSize == 15);
    CHECK(defaultTabFontStaff->useNoteFont);
    CHECK(independentTabFontStaff->useNoteFont);
    CHECK(staffField(report, "useNoteFont").origin == ValueOrigin::LegacyMus);
    CHECK(standardStaff->notationStyle == Staff::NotationStyle::Standard);
    CHECK(staffField(report, "notationStyle").origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(staffField(report, "useNoteShapes").origin == ValueOrigin::LegacyMusAdjusted);
    const auto* tabFontSource = report.findField<Staff>("useNoteFont", musx::dom::SCORE_PARTID, staffCmper9);
    REQUIRE(tabFontSource);
    CHECK(tabFontSource->origin == ValueOrigin::LegacyBehavior);
}

TEST_CASE("Finale 1.0 Staff float controls recover against their enabled baseline", "[class][reader]")
{
    struct FixtureCase
    {
        const char* fixture;
        bool floatKeys;
        bool floatTime;
        bool useNoteFont;
        bool useNoteShapes;
    };
    for (const auto& fixtureCase : {
             FixtureCase{"evidence/F100/staffopts/F100-floatkey.mus", true, false, false, false},
             FixtureCase{"evidence/F100/staffopts/F100-floattime.mus", false, true, false, false},
             FixtureCase{"evidence/F100/staffopts/F100-floatfont.mus", false, false, true, false},
             FixtureCase{"evidence/F100/staffopts/F100-floatnoteshapes.mus", false, false, false, true},
         }) {
        const auto result = readFixture(fixtureCase.fixture);
        const auto staff = result.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
        REQUIRE(staff);
        INFO(fixtureCase.fixture);
        CHECK(staff->floatKeys == fixtureCase.floatKeys);
        CHECK(staff->floatTime == fixtureCase.floatTime);
        CHECK(staff->useNoteFont == fixtureCase.useNoteFont);
        CHECK(staff->useNoteShapes == fixtureCase.useNoteShapes);
        if (fixtureCase.useNoteFont) {
            REQUIRE(staff->noteFont);
            CHECK(staff->noteFont->fontId == 5);
            CHECK(staff->noteFont->fontSize == 15);
        }
    }
}

TEST_CASE("Finale 1.0 Staff display controls recover against the original baseline", "[class][reader]")
{
    struct FixtureCase
    {
        const char* fixture;
        const char* field;
        bool Staff::* member;
    };
    for (const auto& fixtureCase : {
             FixtureCase{"evidence/F100/staffopts/F100-break-barlines.mus", "blineBreak", &Staff::blineBreak},
             FixtureCase{"evidence/F100/staffopts/F100-break-repeatbars.mus", "rbarBreak", &Staff::rbarBreak},
             FixtureCase{"evidence/F100/staffopts/F100-hide-clefs.mus", "hideClefs", &Staff::hideClefs},
             FixtureCase{"evidence/F100/staffopts/F100-hide-endingsrpts.mus", "hideRepeats", &Staff::hideRepeats},
             FixtureCase{"evidence/F100/staffopts/F100-hide-measnums.mus", "hideMeasNums", &Staff::hideMeasNums},
             FixtureCase{"evidence/F100/staffopts/F100-hide-name.mus", "hideNameInScore", &Staff::hideNameInScore},
             FixtureCase{"evidence/F100/staffopts/F100-hide-timesigs.mus", "hideTimeSigs", &Staff::hideTimeSigs},
             FixtureCase{"evidence/F100/staffopts/F100-negate-keys.mus", "hideKeySigs", &Staff::hideKeySigs},
         }) {
        const auto result = readFixture(fixtureCase.fixture);
        const auto staff = result.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
        REQUIRE(staff);
        INFO(fixtureCase.fixture);
        CHECK(staff.get()->*fixtureCase.member);
        CHECK(staffField(result.report, fixtureCase.field, 1).origin == ValueOrigin::LegacyMus);
    }

    const auto timeResult = readFixture("evidence/F100/staffopts/F100-hide-timesigs.mus");
    const auto timeStaff = timeResult.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(timeStaff);
    CHECK(timeStaff->hideTimeSigsInParts);

    const auto keyResult = readFixture("evidence/F100/staffopts/F100-negate-keys.mus");
    const auto keyStaff = keyResult.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(keyStaff);
    CHECK(keyStaff->noKey);
    CHECK(staffField(keyResult.report, "noKey", 1).origin == ValueOrigin::LegacyMusAdjusted);
}

TEST_CASE("Coda Staff line overrides decode signed ordinary and custom forms")
{
    const auto parsed = makeContainer(
        {
            {7, "IS", {0, 0, 4, 1024, 0, 0}},
            {7, "IA", {0, 0, 2, 0, 0, 0x0040}},
            {8, "IS", {0, 0, 4, 1024, 0, 0}},
            {8, "IA", {0, 0, 4, 0, 0, 0x0040}},
            {9, "IS", {0, 0, 4, 1024, 0, 0}},
            {9, "IA", {0, 0, 17, 0, 0, 0x0040}},
            {10, "IS", {0, 0, 4, 1024, 0, 0}},
            {10, "IA", {0, 0, -3, 0, 0, 0x0040}},
            {11, "IS", {0, 0, 4, 1024, 0, 0}},
            {11, "IA", {0, 0, -11, 0, 0, 0x0040}},
            {12, "IS", {0, 0, 4, 1024, 0, 0}},
            {12, "IA", {0, 0, 17, 0, 0, 0}},
        },
        FormatEpoch::CodaBanner);
    const auto document = emptyStaffDocument();
    auto profile = SourceProfile(FormatEpoch::CodaBanner);
    profile.byteOrder = ByteOrder::BigEndian;
    const auto report = staffImport(parsed, profile, document);

    const auto twoLine = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 7);
    const auto fourLine = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 8);
    const auto seventeenLine = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 9);
    REQUIRE(twoLine);
    REQUIRE(fourLine);
    REQUIRE(seventeenLine);
    CHECK(twoLine->staffLines == 2);
    CHECK(fourLine->staffLines == 4);
    CHECK(seventeenLine->staffLines == 17);
    for (const auto& staff : {twoLine, fourLine, seventeenLine}) {
        CHECK(staff->botRepeatDotOff == -5);
        CHECK(staff->topRepeatDotOff == -3);
    }

    const auto line13 = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 10);
    REQUIRE(line13);
    REQUIRE(line13->customStaff);
    CHECK(*line13->customStaff == std::vector<int>{13});
    CHECK(line13->botBarlineOffset == 0);
    CHECK(line13->topBarlineOffset == 2 * evpusPerSpace);
    CHECK(line13->botRepeatDotOff == -5);
    CHECK(line13->topRepeatDotOff == -3);

    const auto line21 = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 11);
    REQUIRE(line21);
    REQUIRE(line21->customStaff);
    CHECK(*line21->customStaff == std::vector<int>{21});
    CHECK(line21->botBarlineOffset == 0);
    CHECK(line21->topBarlineOffset == 10 * evpusPerSpace);
    CHECK(line21->botRepeatDotOff == -21);
    CHECK(line21->topRepeatDotOff == -19);

    const auto inactive = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 12);
    REQUIRE(inactive);
    CHECK(inactive->staffLines == 5);
    const auto* topBarline = report.findField<Staff>("topBarlineOffset", musx::dom::SCORE_PARTID, staffCmper11);
    REQUIRE(topBarline);
    CHECK(topBarline->origin == ValueOrigin::LegacyMusAdjusted);
    for (const auto staffId : {musx::dom::Cmper{7}, musx::dom::Cmper{8}, musx::dom::Cmper{9}}) {
        const auto* bottomDot = report.findField<Staff>("botRepeatDotOff", musx::dom::SCORE_PARTID, staffId);
        const auto* topDot = report.findField<Staff>("topRepeatDotOff", musx::dom::SCORE_PARTID, staffId);
        REQUIRE(bottomDot);
        REQUIRE(topDot);
        CHECK(bottomDot->origin == ValueOrigin::Finale27Default);
        CHECK(topDot->origin == ValueOrigin::Finale27Default);
    }
}

TEST_CASE("Short one-string tablature Staffs retain their legacy behavior")
{
    struct LegacyTabCase
    {
        finale_mus_reader::VersionBound version;
        bool modernLineCount;
    };
    constexpr LegacyTabCase cases[]{
        {finale_mus_reader::versions::finale97, false},
        {finale_mus_reader::versions::finale98, false},
        {finale_mus_reader::versions::finale2000, true},
    };

    for (const auto& testCase : cases) {
        const std::int16_t topLines = testCase.modernLineCount ? 0 : 1;
        const std::int16_t primary = testCase.modernLineCount ? 0x0301 : 0x0340;
        const auto parsed = makeContainer(
            {
                {7, "IS", {0, -948, 0, 3, 0x0900, primary}},
                {7, "IS", {0, topLines, 1, 0, 0, 0x0f38}},
                {7, "IS", {-772, -772, -4, 1, 0, 0}},
            },
            FormatEpoch::UncompressedLegacy);
        const auto document = emptyStaffDocument();
        auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
        profile.version = SourceVersion{.major = testCase.version.major, .minor = testCase.version.minor, .maint = testCase.version.maint};
        profile.byteOrder = ByteOrder::BigEndian;
        const auto report = staffImport(parsed, profile, document);

        const auto staff = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 7);
        REQUIRE(staff);
        if (testCase.modernLineCount) {
            CHECK(staff->staffLines == 1);
        } else {
            CHECK_FALSE(staff->staffLines);
            REQUIRE(staff->customStaff);
            CHECK(*staff->customStaff == std::vector<int>{11});
        }
        CHECK(staff->capoPos == 0);
        CHECK(staff->lowestFret == 0);
        CHECK(staff->vertTabNumOff == -1024);
        CHECK(staff->showTabClefAllSys);
        CHECK(staff->hideRests);
        CHECK(staff->hideDots);
        CHECK(staff->hideStems);
        CHECK(staff->hideTuplets);
        CHECK_FALSE(staff->breakTabLinesAtNotes);
        CHECK(staff->fretInstId == 2);
        CHECK(staff->botRepeatDotOff == (testCase.modernLineCount ? -5 : -1));
        CHECK(staff->topRepeatDotOff == (testCase.modernLineCount ? -3 : 1));
        if (!testCase.modernLineCount) {
            CHECK(staff->hideRepeatBottomDot);
            CHECK(staff->hideRepeatTopDot);
        }
        const auto fret = document->getOthers()->get<musx::dom::others::FretInstrument>(musx::dom::SCORE_PARTID, staff->fretInstId);
        REQUIRE(fret);
        CHECK(fret->name == "E5");
        REQUIRE(fret->strings.size() == 1);
        CHECK(fret->strings.front()->pitch == 76);
        CHECK(staffField(report, "vertTabNumOff").origin == ValueOrigin::LegacyMus);
        CHECK(staffField(report, "fretInstId").origin == ValueOrigin::LegacyBehavior);
        CHECK(staffField(report, "showTabClefAllSys").origin == ValueOrigin::LegacyBehavior);
        CHECK(staffField(report, "breakTabLinesAtNotes").origin == ValueOrigin::Finale27Default);
    }

    const auto controlled = readFixture("evidence/F2000/F2000-tablature.mus");
    const auto controlledStaff = controlled.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(controlledStaff);
    CHECK_FALSE(controlledStaff->breakTabLinesAtNotes);
    CHECK(staffField(controlled.report, "breakTabLinesAtNotes", 1).origin == ValueOrigin::Finale27Default);
}

TEST_CASE("The 18-word tablature positions continue into the DCL epoch")
{
    const auto parsed = makeContainer(
        {
            {7, "IS", {0, -948, 0, 3, 0x0900, 0x0301}},
            {7, "IS", {0, 0, 1, 0, 0, 0x0f38}},
            {7, "IS", {-772, -772, -4, 1, 0, 0}},
        },
        FormatEpoch::DclLegacy);
    const auto document = emptyStaffDocument();
    auto profile = SourceProfile(FormatEpoch::DclLegacy);
    profile.version = SourceVersion{.major = finale_mus_reader::versions::finale2001.major};
    profile.byteOrder = ByteOrder::BigEndian;
    staffImport(parsed, profile, document);

    const auto staff = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 7);
    REQUIRE(staff);
    CHECK(staff->staffLines == 1);
    CHECK(staff->capoPos == 0);
    CHECK(staff->lowestFret == 0);
    CHECK(staff->vertTabNumOff == -1024);
    CHECK(staff->fretInstId == 2);
    const auto fret = document->getOthers()->get<musx::dom::others::FretInstrument>(musx::dom::SCORE_PARTID, staff->fretInstId);
    REQUIRE(fret);
    REQUIRE(fret->strings.size() == 1);
    CHECK(fret->strings.front()->pitch == 76);
}

TEST_CASE("Controlled Finale 2002 Staff settings use the 18-word layout")
{
    const auto ignored = readFixture("evidence/F2002/F2002-ignore-key.mus");
    const auto ignoredStaff = ignored.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(ignoredStaff);
    CHECK(ignoredStaff->noKey);
    CHECK(staffField(ignored.report, "fretInstId", 1).origin == ValueOrigin::Finale27Default);
    constexpr std::string_view noTailFields[] = {
        "showTabClefAllSys",
        "hideRests",
        "hideTies",
        "hideDots",
        "hideStems",
        "stemDirection",
        "stemStartFromStaff",
        "stemsFixedEnd",
        "useTabLetters",
        "hideBeams",
        "breakTabLinesAtNotes",
        "stemsFixedStart",
        "hideTuplets",
        "horzStemOffUp",
        "horzStemOffDown",
        "vertStemStartOffUp",
        "vertStemStartOffDown",
        "vertStemEndOffUp",
        "vertStemEndOffDown",
    };
    for (const auto field : noTailFields) {
        CHECK(staffField(ignored.report, field, 1).origin == ValueOrigin::Finale27Default);
    }

    const auto tablature = readFixture("evidence/F2002/F2002-tablature.mus");
    const auto tablatureStaff = tablature.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(tablatureStaff);
    CHECK(tablatureStaff->notationStyle == Staff::NotationStyle::Tablature);
    CHECK(tablatureStaff->capoPos == 0);
    CHECK(tablatureStaff->lowestFret == 0);
    CHECK(tablatureStaff->fretInstId != 0);
    const auto tablatureFret =
        tablature.document->getOthers()->get<musx::dom::others::FretInstrument>(musx::dom::SCORE_PARTID, tablatureStaff->fretInstId);
    REQUIRE(tablatureFret);
    REQUIRE(tablatureFret->strings.size() == 1);
    CHECK(tablatureFret->strings.front()->pitch == 0);
    CHECK(staffField(tablature.report, "fretInstId", 1).origin == ValueOrigin::LegacyBehavior);
    CHECK(tablatureStaff->showTabClefAllSys);
    CHECK(tablatureStaff->hideRests);
    CHECK(tablatureStaff->hideDots);
    CHECK(tablatureStaff->hideStems);
    CHECK(tablatureStaff->hideTuplets);
    CHECK_FALSE(tablatureStaff->breakTabLinesAtNotes);
    for (const auto field : {"showTabClefAllSys", "hideRests", "hideDots", "hideStems", "hideTuplets"}) {
        CHECK(staffField(tablature.report, field, 1).origin == ValueOrigin::LegacyBehavior);
    }
    CHECK(staffField(tablature.report, "breakTabLinesAtNotes", 1).origin == ValueOrigin::Finale27Default);
    for (const auto field : {"hideTies", "stemDirection", "stemStartFromStaff", "stemsFixedEnd", "useTabLetters", "hideBeams", "stemsFixedStart",
             "horzStemOffUp", "horzStemOffDown", "vertStemStartOffUp", "vertStemStartOffDown", "vertStemEndOffUp", "vertStemEndOffDown"}) {
        CHECK(staffField(tablature.report, field, 1).origin == ValueOrigin::Finale27Default);
    }

    const auto baseKey = readFixture("evidence/F2002/F2002-tablature-basekey47.mus");
    const auto baseKeyStaff = baseKey.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(baseKeyStaff);
    const auto baseKeyFret = baseKey.document->getOthers()->get<musx::dom::others::FretInstrument>(musx::dom::SCORE_PARTID, baseKeyStaff->fretInstId);
    REQUIRE(baseKeyFret);
    REQUIRE(baseKeyFret->strings.size() == 1);
    CHECK(baseKeyFret->strings.front()->pitch == 47);
    CHECK(baseKeyFret->name == "B2");
    CHECK(staffField(baseKey.report, "fretInstId", 1).origin == ValueOrigin::LegacyBehavior);
}

TEST_CASE("Parallel Staff names supplement missing stored name references")
{
    const auto parsed = makeContainer({
        {7, "IS", {0, 0, 0, 0, 0, 0}},
        {7, "IS", {0, 5, 0, 0, 0, 0}},
        {7, "IS", {0, 0, 0, 0, 0, 0}},
        {7, "IN", {0x4e61, 0x6d65, 0, 0, 0, 0}},
        {7, "in", {0x4162, 0x6272, 0, 0, 0, 0}},
        {8, "IS", {0, 0, 0, 0, 0, 0}},
        {8, "IS", {0, 5, 0, 0, 0, 0}},
        {8, "IS", {0, 0, 0, 21, 22, 0}},
        {8, "IN", {0x4967, 0x6e6f, 0x7265, 0x6400, 0, 0}},
        {8, "in", {0x4967, 0x6e6f, 0x7265, 0x6400, 0, 0}},
    });
    const auto document = emptyStaffDocument();
    auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
    profile.version = SourceVersion{.major = 3, .minor = 5};
    profile.byteOrder = ByteOrder::BigEndian;
    const auto report = staffImport(parsed, profile, document);

    const auto recovered = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 7);
    REQUIRE(recovered);
    CHECK(recovered->getFullName() == "Name");
    CHECK(recovered->getAbbreviatedName() == "Abbr");
    CHECK(recovered->fullNameTextId != 0);
    CHECK(recovered->abbrvNameTextId != 0);
    const auto* fullName = report.findField<Staff>("fullNameTextId", musx::dom::SCORE_PARTID, staffCmper7);
    const auto* abbreviatedName = report.findField<Staff>("abbrvNameTextId", musx::dom::SCORE_PARTID, staffCmper7);
    REQUIRE(fullName);
    REQUIRE(abbreviatedName);
    CHECK(fullName->origin == ValueOrigin::LegacyBehavior);
    CHECK(abbreviatedName->origin == ValueOrigin::LegacyBehavior);

    const auto stored = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 8);
    REQUIRE(stored);
    CHECK(stored->fullNameTextId == 21);
    CHECK(stored->abbrvNameTextId == 22);

    const auto finale37Document = emptyStaffDocument();
    profile.version = SourceVersion{.major = 3, .minor = 7};
    staffImport(parsed, profile, finale37Document);
    const auto finale37Staff = finale37Document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 7);
    REQUIRE(finale37Staff);
    CHECK(finale37Staff->fullNameTextId == 0);
    CHECK(finale37Staff->abbrvNameTextId == 0);
}

TEST_CASE("Synthesized parallel Staff names leave stored early TextBlock comparators to measure text")
{
    // Early measure text names TextBlock 1 through its PT connector; the Staff names synthesized
    // for Staff 7 must allocate around it rather than take it.
    auto parsed = makeContainer({
        {7, "IS", {0, 0, 0, 0, 0, 0}},
        {7, "IS", {0, 5, 0, 0, 0, 0}},
        {7, "IS", {0, 0, 0, 0, 0, 0}},
        {7, "IN", {0x4e61, 0x6d65, 0, 0, 0, 0}},
        {7, "in", {0x4162, 0x6272, 0, 0, 0, 0}},
        {1, "PT", {9}},
    });
    parsed.blocks.push_back(makeDetailContainer(FormatEpoch::UncompressedLegacy, 7, 1, {1, 20, 30, 0, 0}, "MT").blocks.front());
    const auto document = emptyStaffDocument();
    auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
    profile.version = SourceVersion{.major = 3, .minor = 5};
    profile.byteOrder = ByteOrder::BigEndian;
    staffImport(parsed, profile, document, false, &finale_mus_reader::details::importMeasureTextAssigns);

    const auto assignment = document->getDetails()->get<musx::dom::details::MeasureTextAssign>(musx::dom::SCORE_PARTID, 7, 1, musx::dom::Inci(0));
    REQUIRE(assignment);
    CHECK(assignment->block == 1);
    const auto block = assignment->getTextBlock();
    REQUIRE(block);
    CHECK(block->textId == 9);
    const auto staff = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 7);
    REQUIRE(staff);
    CHECK(staff->fullNameTextId != 0);
    CHECK(staff->fullNameTextId != 1);
    CHECK(staff->abbrvNameTextId != 0);
    CHECK(staff->abbrvNameTextId != 1);
}

TEST_CASE("Finale 1.0 Staff properties recover from the six-word row")
{
    const auto result = readFixture("evidence/F100/F100-staffprops.mus");
    const auto staff = result.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(staff);
    CHECK(staff->staffLines == 5);
    REQUIRE(staff->noteFont);
    CHECK(staff->noteFont->fontSize == 24);
    CHECK(staff->defaultClef == 2);
    REQUIRE(staff->transposition);
    REQUIRE(staff->transposition->keysig);
    CHECK(staff->transposition->keysig->interval == 5);
    CHECK(staff->transposition->keysig->adjust == 3);
    CHECK(staff->lineSpace == evpusPerSpace);
    CHECK(staff->dwRestOffset == -4);
    CHECK(staff->wRestOffset == -4);
    CHECK(staff->hRestOffset == -4);
    CHECK(staff->otherRestOffset == -4);
    CHECK(staff->stemReversal == -4);
    CHECK(staff->botRepeatDotOff == -5);
    CHECK(staff->topRepeatDotOff == -3);
    CHECK(staff->getFullName() == "Blown Horn");
    CHECK(staff->getAbbreviatedName().empty());
    REQUIRE(staff->fullNameTextId != 0);
    const auto nameBlock = result.document->getOthers()->get<musx::dom::others::TextBlock>(musx::dom::SCORE_PARTID, staff->fullNameTextId);
    REQUIRE(nameBlock);
    CHECK(nameBlock->textId != 0);
    CHECK(nameBlock->textType == musx::dom::others::TextBlock::TextType::Block);
    CHECK(nameBlock->lineSpacingPercentage == 100);
    CHECK(nameBlock->newPos36);
    CHECK(nameBlock->showShape);
    CHECK(nameBlock->wordWrap);

    const auto field = [&](const char* member) { return result.report.findField<Staff>(member, musx::dom::SCORE_PARTID, staffCmper1); };
    REQUIRE(field("defaultClef"));
    REQUIRE(field("transposition.keysig.interval"));
    REQUIRE(field("dwRestOffset"));
    REQUIRE(field("noteFont.fontSize"));
    CHECK(field("defaultClef")->origin == ValueOrigin::LegacyMus);
    CHECK(field("transposition.keysig.interval")->origin == ValueOrigin::LegacyMus);
    CHECK(field("dwRestOffset")->origin == ValueOrigin::Finale27Default);
    CHECK(field("noteFont.fontSize")->origin == ValueOrigin::Finale27Default);
    REQUIRE(field("fullNameTextId"));
    CHECK(field("fullNameTextId")->origin == ValueOrigin::LegacyBehavior);
}

TEST_CASE("Early synthesized staff-name TextBlocks take their justification from name positioning")
{
    using TextBlock = musx::dom::others::TextBlock;
    for (const auto& [fixture, justification, raw] : {
             std::tuple{"evidence/F263/F263-namepos.mus", TextBlock::TextJustify::Right, std::int64_t{1}},
             std::tuple{"evidence/F263/F263-namepos-left.mus", TextBlock::TextJustify::Left, std::int64_t{0}},
         }) {
        const auto result = readFixture(fixture);
        const auto staff = result.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, staffCmper1);
        REQUIRE(staff);
        REQUIRE(staff->fullNameTextId != 0);
        const auto block = result.document->getOthers()->get<TextBlock>(musx::dom::SCORE_PARTID, staff->fullNameTextId);
        REQUIRE(block);
        CHECK(block->justify == justification);
        const auto* field = result.report.findField<TextBlock>("justify", musx::dom::SCORE_PARTID, staff->fullNameTextId);
        REQUIRE(field);
        CHECK(field->origin == ValueOrigin::LegacyMusAdjusted);
        CHECK(field->rawValue == raw);
    }
}

TEST_CASE("The six-word Staff transposition word carries its set-to-clef index")
{
    const auto result = readFixture("evidence/F100/staffopts/F100-transp-settoclef7.mus");
    const auto staff = result.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(staff);
    CHECK(staff->transposedClef == 7);
    REQUIRE(staff->transposition);
    CHECK(staff->transposition->setToClef);
    CHECK_FALSE(staff->transposition->noSimplifyKey);
    CHECK_FALSE(staff->transposition->chromatic);
    REQUIRE(staff->transposition->keysig);
    CHECK(staff->transposition->keysig->interval == 0);
    CHECK(staff->transposition->keysig->adjust == 0);
    const auto* field = result.report.findField<Staff>("transposedClef", musx::dom::SCORE_PARTID, staffCmper1);
    REQUIRE(field);
    CHECK(field->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Finale 2.6.3 optional Staff attributes recover custom lines and "
          "notehead font")
{
    const auto result = readFixture("evidence/F263/F263-staffopts.mus");
    const auto staff = result.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(staff);
    CHECK_FALSE(staff->staffLines);
    REQUIRE(staff->customStaff);
    CHECK(*staff->customStaff == std::vector<int>{13});
    CHECK(staff->botBarlineOffset == -2 * evpusPerSpace);
    CHECK(staff->topBarlineOffset == 2 * evpusPerSpace);
    CHECK(staff->notationStyle == Staff::NotationStyle::Tablature);
    CHECK(staff->vertTabNumOff == 768);
    CHECK(staff->fretInstId == 2);
    CHECK(staff->showTabClefAllSys);
    CHECK(staff->hideRests);
    CHECK(staff->hideDots);
    CHECK(staff->hideStems);
    CHECK(staff->hideTuplets);
    CHECK_FALSE(staff->hideTies);
    CHECK_FALSE(staff->hideBeams);
    CHECK_FALSE(staff->useTabLetters);
    CHECK_FALSE(staff->breakTabLinesAtNotes);
    CHECK(staff->useNoteFont);
    REQUIRE(staff->noteFont);
    CHECK(staff->noteFont->fontId == 33);
    CHECK(staff->noteFont->getName() == "AGaramond");
    CHECK(staff->noteFont->fontSize == 13);
    CHECK_FALSE(staff->noteFont->bold);
    CHECK(staff->noteFont->italic);
    CHECK(staff->defaultClef == 1);
    CHECK(staff->hideTimeSigs);
    CHECK(staff->hideTimeSigsInParts);
    CHECK(staff->dwRestOffset == -4);
    CHECK(staff->stemReversal == -4);
    CHECK(staff->botRepeatDotOff == -5);
    CHECK(staff->topRepeatDotOff == -3);
    CHECK(staff->getFullName() == "Horn Blown");
    CHECK(staff->getAbbreviatedName() == "H.B.");
    REQUIRE(staff->fullNameTextId != 0);
    REQUIRE(staff->abbrvNameTextId != 0);
    CHECK(staff->fullNameTextId != staff->abbrvNameTextId);
    const auto fret = result.document->getOthers()->get<musx::dom::others::FretInstrument>(musx::dom::SCORE_PARTID, staff->fretInstId);
    REQUIRE(fret);
    REQUIRE(fret->strings.size() == 1);
    CHECK(fret->strings.front()->pitch == 1);
    CHECK(fret->name == "C#-1");
    const auto fullNameBlock = result.document->getOthers()->get<musx::dom::others::TextBlock>(musx::dom::SCORE_PARTID, staff->fullNameTextId);
    const auto abbreviatedNameBlock =
        result.document->getOthers()->get<musx::dom::others::TextBlock>(musx::dom::SCORE_PARTID, staff->abbrvNameTextId);
    REQUIRE(fullNameBlock);
    REQUIRE(abbreviatedNameBlock);
    CHECK(fullNameBlock->textId != 0);
    CHECK(abbreviatedNameBlock->textId != 0);
    CHECK(fullNameBlock->textId != abbreviatedNameBlock->textId);

    const auto field = [&](const char* member) { return result.report.findField<Staff>(member, musx::dom::SCORE_PARTID, staffCmper1); };
    REQUIRE(field("customStaff"));
    REQUIRE(field("botBarlineOffset"));
    REQUIRE(field("topBarlineOffset"));
    REQUIRE(field("notationStyle"));
    REQUIRE(field("vertTabNumOff"));
    REQUIRE(field("fretInstId"));
    REQUIRE(field("showTabClefAllSys"));
    REQUIRE(field("hideRests"));
    REQUIRE(field("hideDots"));
    REQUIRE(field("hideStems"));
    REQUIRE(field("hideTuplets"));
    REQUIRE(field("noteFont.fontSize"));
    REQUIRE(field("hideTimeSigs"));
    CHECK(field("customStaff")->origin == ValueOrigin::LegacyMus);
    CHECK(field("botBarlineOffset")->origin == ValueOrigin::LegacyBehavior);
    CHECK(field("topBarlineOffset")->origin == ValueOrigin::LegacyBehavior);
    CHECK(field("notationStyle")->origin == ValueOrigin::LegacyMus);
    CHECK(field("vertTabNumOff")->origin == ValueOrigin::LegacyMus);
    CHECK(field("fretInstId")->origin == ValueOrigin::LegacyBehavior);
    CHECK(field("showTabClefAllSys")->origin == ValueOrigin::LegacyBehavior);
    CHECK(field("hideRests")->origin == ValueOrigin::LegacyBehavior);
    CHECK(field("hideDots")->origin == ValueOrigin::LegacyBehavior);
    CHECK(field("hideStems")->origin == ValueOrigin::LegacyBehavior);
    CHECK(field("hideTuplets")->origin == ValueOrigin::LegacyBehavior);
    CHECK(field("noteFont.fontSize")->origin == ValueOrigin::LegacyMus);
    CHECK(field("hideTimeSigs")->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Finale 2.6.3 Staff attributes recover the remaining tablature controls")
{
    const auto checkDefaultNoteheadFontSize = [](const auto& result, const auto& staff) {
        const auto defaultFont =
            musx::dom::options::FontOptions::getFontInfoOrNull(result.document, musx::dom::options::FontOptions::FontType::Noteheads);
        REQUIRE(defaultFont);
        REQUIRE(staff->noteFont);
        CHECK(staff->noteFont->fontSize == defaultFont->fontSize);
    };
    const auto baseline = readFixture("evidence/F263/staffopts/F263-tabstaff.mus");
    const auto baselineStaff = baseline.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(baselineStaff);
    CHECK(baselineStaff->notationStyle == Staff::NotationStyle::Tablature);
    CHECK(baselineStaff->useNoteFont);
    checkDefaultNoteheadFontSize(baseline, baselineStaff);
    CHECK(staffField(baseline.report, "noteFont.fontSize", 1).origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(baselineStaff->fretInstId == 2);
    const auto baselineFret =
        baseline.document->getOthers()->get<musx::dom::others::FretInstrument>(musx::dom::SCORE_PARTID, baselineStaff->fretInstId);
    REQUIRE(baselineFret);
    REQUIRE(baselineFret->strings.size() == 1);
    CHECK(baselineFret->strings.front()->pitch == 0);
    CHECK(baselineFret->name == "C-1");

    const auto baseKey = readFixture("evidence/F263/staffopts/F263-tabstaff-basekey48.mus");
    const auto baseKeyStaff = baseKey.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(baseKeyStaff);
    checkDefaultNoteheadFontSize(baseKey, baseKeyStaff);
    const auto baseKeyFret = baseKey.document->getOthers()->get<musx::dom::others::FretInstrument>(musx::dom::SCORE_PARTID, baseKeyStaff->fretInstId);
    REQUIRE(baseKeyFret);
    REQUIRE(baseKeyFret->strings.size() == 1);
    CHECK(baseKeyFret->strings.front()->pitch == 48);
    CHECK(baseKeyFret->name == "C3");

    const auto font = readFixture("evidence/F263/staffopts/F263-tabstaff-font13ital.mus");
    const auto fontStaff = font.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(fontStaff);
    REQUIRE(fontStaff->noteFont);
    CHECK(fontStaff->noteFont->fontId == 126);
    CHECK(fontStaff->noteFont->fontSize == 13);
    CHECK(fontStaff->noteFont->italic);
    CHECK(staffField(font.report, "noteFont.fontSize", 1).origin == ValueOrigin::LegacyMus);

    const auto offset = readFixture("evidence/F263/staffopts/F263-tabstaff-yoff29.mus");
    const auto offsetStaff = offset.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(offsetStaff);
    checkDefaultNoteheadFontSize(offset, offsetStaff);
    CHECK(offsetStaff->vertTabNumOff == 29 * 256);
}

TEST_CASE("Finale 2.6.3 Staff attributes recover blank empty measures")
{
    const auto result = readFixture("evidence/F263/staffopts/F263-blank-emptymeas.mus");
    const auto staff = result.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(staff);
    CHECK(staff->blankMeasure);
}

TEST_CASE("The Finale 2012 Staff extension recovers its stored instrument UUID")
{
    constexpr std::array<std::uint8_t, 16> uuidBytes{0xa9, 0x25, 0x64, 0x8a, 0xab, 0xc9, 0x4d, 0xc7, 0xa6, 0x19, 0xa6, 0xce, 0x35, 0x5a, 0xd3, 0x3c};
    std::vector<std::int16_t> words(40, 0);
    for (std::size_t index = 0; index < uuidBytes.size(); index += 2) {
        words.push_back(static_cast<std::int16_t>(uuidBytes[index] | (static_cast<std::uint16_t>(uuidBytes[index + 1]) << 8U)));
    }
    const auto parsed = makeClassContainer(0x00e7, words, ByteOrder::LittleEndian, 7);
    const auto document = emptyStaffDocument();
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    const auto report = staffImport(parsed, profile, document);

    const auto staff = document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 7);
    REQUIRE(staff);
    CHECK(staff->instUuid == "a925648a-abc9-4dc7-a619-a6ce355ad33c");
    CHECK(staffField(report, "instUuid").origin == ValueOrigin::LegacyMus);
    CHECK(staffField(report, "instUuid").rawValue == 16);
    CHECK(reportedFieldCount(report) == staffFieldManifestSize);
}

TEST_CASE("The Finale 2012 Staff layout adds independent staff-line hiding")
{
    const auto finale2011 = readFixture("evidence/F2011/F2011-baseline.mus");
    const auto baseline = readFixture("evidence/F2012/F2012-baseline.mus");
    const auto hidden = readFixture("evidence/F2012/F2012-hidestafflines.mus");

    const auto earlierStaff = finale2011.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
    const auto baselineStaff = baseline.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
    const auto hiddenStaff = hidden.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(earlierStaff);
    REQUIRE(baselineStaff);
    REQUIRE(hiddenStaff);
    CHECK_FALSE(earlierStaff->hideStaffLines);
    CHECK_FALSE(baselineStaff->hideStaffLines);
    CHECK(hiddenStaff->hideStaffLines);

    const auto* earlierSource = finale2011.report.findField<Staff>("hideStaffLines", musx::dom::SCORE_PARTID, staffCmper1);
    const auto* baselineSource = baseline.report.findField<Staff>("hideStaffLines", musx::dom::SCORE_PARTID, staffCmper1);
    const auto* hiddenSource = hidden.report.findField<Staff>("hideStaffLines", musx::dom::SCORE_PARTID, staffCmper1);
    REQUIRE(earlierSource);
    REQUIRE(baselineSource);
    REQUIRE(hiddenSource);
    CHECK(earlierSource->origin == ValueOrigin::Finale27Default);
    CHECK(baselineSource->origin == ValueOrigin::LegacyMus);
    CHECK(hiddenSource->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("The Finale 2012 Staff layout adds automatic name numbering")
{
    const auto finale2011 = readFixture("evidence/F2011/F2011-baseline.mus");
    const auto baseline = readFixture("evidence/F2012/F2012-baseline.mus");
    const auto numbered = readFixture("evidence/F2012/F2012-staff-autonum.mus");

    const auto earlierStaff = finale2011.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
    const auto baselineStaff = baseline.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
    const auto numberedStaff = numbered.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(earlierStaff);
    REQUIRE(baselineStaff);
    REQUIRE(numberedStaff);
    CHECK(earlierStaff->instUuid == musx::dom::uuid::Unknown);
    CHECK(static_cast<int>(earlierStaff->autoNumbering) == 0);
    CHECK_FALSE(earlierStaff->useAutoNumbering);
    CHECK(static_cast<int>(baselineStaff->autoNumbering) == 0);
    CHECK_FALSE(baselineStaff->useAutoNumbering);
    CHECK(numberedStaff->useAutoNumbering);
    CHECK(numberedStaff->autoNumbering == Staff::AutoNumberingStyle::OrdinalPrefix);

    const auto* earlierStyle = finale2011.report.findField<Staff>("autoNumbering", musx::dom::SCORE_PARTID, staffCmper1);
    const auto* earlierUuid = finale2011.report.findField<Staff>("instUuid", musx::dom::SCORE_PARTID, staffCmper1);
    const auto* earlierEnabled = finale2011.report.findField<Staff>("useAutoNumbering", musx::dom::SCORE_PARTID, staffCmper1);
    const auto* baselineStyle = baseline.report.findField<Staff>("autoNumbering", musx::dom::SCORE_PARTID, staffCmper1);
    const auto* baselineEnabled = baseline.report.findField<Staff>("useAutoNumbering", musx::dom::SCORE_PARTID, staffCmper1);
    const auto* numberedStyle = numbered.report.findField<Staff>("autoNumbering", musx::dom::SCORE_PARTID, staffCmper1);
    const auto* numberedEnabled = numbered.report.findField<Staff>("useAutoNumbering", musx::dom::SCORE_PARTID, staffCmper1);
    REQUIRE(earlierStyle);
    REQUIRE(earlierUuid);
    REQUIRE(earlierEnabled);
    REQUIRE(baselineStyle);
    REQUIRE(baselineEnabled);
    REQUIRE(numberedStyle);
    REQUIRE(numberedEnabled);
    CHECK(earlierStyle->origin == ValueOrigin::LegacyBehavior);
    CHECK(earlierUuid->origin == ValueOrigin::LegacyBehavior);
    CHECK(earlierEnabled->origin == ValueOrigin::LegacyBehavior);
    CHECK(baselineStyle->origin == ValueOrigin::LegacyMus);
    CHECK(baselineEnabled->origin == ValueOrigin::LegacyMus);
    CHECK(numberedStyle->origin == ValueOrigin::LegacyMus);
    CHECK(numberedEnabled->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("A short post-Coda Staff record is rejected")
{
    const auto parsed = makeClassContainer(0x00e7, std::vector<std::int16_t>(17), ByteOrder::LittleEndian, 7);
    const auto document = emptyStaffDocument();
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    const auto report = staffImport(parsed, profile, document);

    CHECK_FALSE(document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, 7));
    REQUIRE(report.diagnostics.size() == 1);
    CHECK(report.diagnostics.front().message.find("shorter than its base layout") != std::string::npos);
}

TEST_CASE("Finale 2000 StaffStyles share notation-derived Smart Shape hiding")
{
    using StaffStyle = musx::dom::others::StaffStyle;
    const auto result = readFixture("evidence/F2000/F2000-F372-facingpages-parts.mus");
    constexpr bool expected[] = {true, false, true, true, true};

    for (std::size_t index = 0; index < std::size(expected); ++index) {
        const auto cmper = static_cast<musx::dom::Cmper>(index + 1);
        const auto style = result.document->getOthers()->get<StaffStyle>(musx::dom::SCORE_PARTID, cmper);
        REQUIRE(style);
        CHECK(style->altHideSmartShapes == expected[index]);
        CHECK_FALSE(style->altHideExpressions);
        const auto* field = result.report.findField<StaffStyle>("altHideSmartShapes", musx::dom::SCORE_PARTID, cmper);
        REQUIRE(field);
        CHECK(field->origin == ValueOrigin::LegacyMusAdjusted);
        const auto* expressions = result.report.findField<StaffStyle>("altHideExpressions", musx::dom::SCORE_PARTID, cmper);
        REQUIRE(expressions);
        CHECK(expressions->origin == ValueOrigin::Finale27Default);
    }
}

TEST_CASE("StaffStyle reuses each Staff payload layout and decodes its trailer")
{
    using StaffStyle = musx::dom::others::StaffStyle;
    struct Expected
    {
        const char* path;
        const char* name;
    };
    constexpr Expected expected[] = {
        {"evidence/F2000/F2000-staff-style.mus", "1-line staff"},
        {"evidence/F2003/F2003-staffstyle.mus", "1-staff line"},
        {"evidence/F2011/F2011-staffstyle.mus", "1-line staff"},
    };
    for (const auto& item : expected) {
        const auto result = readFixture(item.path);
        const auto style = result.document->getOthers()->get<StaffStyle>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1});
        REQUIRE(style);
        CHECK(style->styleName == item.name);
        CHECK(style->customStaff == std::optional<std::vector<int>>{{13}});
        CHECK(style->lineSpace == 24);
        CHECK(style->botBarlineOffset == -24);
        CHECK(style->topBarlineOffset == 24);
        CHECK(style->dwRestOffset == -5);
        CHECK(style->wRestOffset == -6);
        CHECK(style->hRestOffset == -4);
        CHECK(style->otherRestOffset == -4);
        CHECK(style->stemReversal == -4);
        CHECK(style->botRepeatDotOff == -5);
        CHECK(style->topRepeatDotOff == -3);
        REQUIRE(style->masks);
        CHECK(style->masks->staffType);
        CHECK_FALSE(style->copyable);
        CHECK(style->addToMenu);
    }
}

TEST_CASE("Finale 2000 StaffStyle menu and copy controls are independent")
{
    using StaffStyle = musx::dom::others::StaffStyle;
    struct Expected
    {
        const char* path;
        bool addToMenu;
        bool copyable;
    };
    constexpr Expected expected[] = {
        {"evidence/F2000/F2000-style-menu.mus", true, false},
        {"evidence/F2000/F2000-style-nomenu.mus", false, false},
        {"evidence/F2000/F2000-style-menu-copyable.mus", true, true},
    };

    for (const auto& item : expected) {
        const auto result = readFixture(item.path);
        const auto style = result.document->getOthers()->get<StaffStyle>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1});
        REQUIRE(style);
        CHECK(style->addToMenu == item.addToMenu);
        CHECK(style->copyable == item.copyable);
    }

    const auto implicitStyle = [](std::int16_t secondMask) {
        const auto parsed = makeContainer(
            {{1, "SY", {0, 0, 0, 0, 0, 0}}, {1, "SY", {0, 0, 0, 0, 0, 0}}, {1, "SY", {0, 0, 0, 0, 0, 0}}, {1, "SY", {0, secondMask, 0, 0, 0, 0}},
                {1, "SY", {0, 0, 0, 0, 0, 0}}, {1, "SY", {0, 0, 0, 0, 0, 0}}, {1, "SY", {0, 0, 0, 0, 0, 0}}, {1, "SY", {0, 0, 0, 0, 0, 0}}});
        const auto document = emptyStaffDocument();
        staffImport(parsed, profileFor(finale_mus_reader::versions::finale2000.major), document, true);
        return document->getOthers()->get<StaffStyle>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1});
    };
    const auto ordinary = implicitStyle(0);
    REQUIRE(ordinary);
    CHECK(ordinary->addToMenu);
    CHECK_FALSE(ordinary->copyable);
    const auto alternateNotation = implicitStyle(0x1000);
    REQUIRE(alternateNotation);
    CHECK(alternateNotation->addToMenu);
    CHECK(alternateNotation->copyable);
}

TEST_CASE("StaffStyle maps every stored mask and control bit")
{
    using StaffStyle = musx::dom::others::StaffStyle;
    const auto parsed = makeContainer({{1, "SY", {-24, 0, 0, 0, 0, 0}}, {1, "SY", {0, 1, 4, 24, 0, 0}}, {1, "SY", {-1285, -772, -4, 0, 0, 0}},
        {1, "SY", {-1, -1, 0x03ff, 0, 0, 0x0006}}, {1, "SY", {0x540d, 0x5465, 0x7374, 0, 0, 0}}, {1, "SY", {0, 0, 0, 0, 0, 0}},
        {1, "SY", {0, 0, 0, 0, 0, 0}}, {1, "SY", {0, 0, 0, 0, 0, 0}}});
    const auto document = emptyStaffDocument();
    auto report = staffImport(parsed, profileFor(finale_mus_reader::versions::finale2000.major), document, true);
    const auto style = document->getOthers()->get<StaffStyle>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1});
    REQUIRE(style);
    CHECK(style->styleName == "T\nTest");
    CHECK(style->copyable);
    CHECK(style->addToMenu);
    REQUIRE(style->masks);
#define CHECK_STAFF_STYLE_MASK(member) CHECK(style->masks->member)
    CHECK_STAFF_STYLE_MASK(floatNoteheadFont);
    CHECK_STAFF_STYLE_MASK(useNoteShapes);
    CHECK_STAFF_STYLE_MASK(flatBeams);
    CHECK_STAFF_STYLE_MASK(blankMeasureRest);
    CHECK_STAFF_STYLE_MASK(noOptimize);
    CHECK_STAFF_STYLE_MASK(notationStyle);
    CHECK_STAFF_STYLE_MASK(defaultClef);
    CHECK_STAFF_STYLE_MASK(staffType);
    CHECK_STAFF_STYLE_MASK(transposition);
    CHECK_STAFF_STYLE_MASK(blineBreak);
    CHECK_STAFF_STYLE_MASK(rbarBreak);
    CHECK_STAFF_STYLE_MASK(negMnumb);
    CHECK_STAFF_STYLE_MASK(negRepeat);
    CHECK_STAFF_STYLE_MASK(negNameScore);
    CHECK_STAFF_STYLE_MASK(hideBarlines);
    CHECK_STAFF_STYLE_MASK(fullName);
    CHECK_STAFF_STYLE_MASK(abrvName);
    CHECK_STAFF_STYLE_MASK(floatKeys);
    CHECK_STAFF_STYLE_MASK(floatTime);
    CHECK_STAFF_STYLE_MASK(hideRptBars);
    CHECK_STAFF_STYLE_MASK(negKey);
    CHECK_STAFF_STYLE_MASK(negTime);
    CHECK_STAFF_STYLE_MASK(negClef);
    CHECK_STAFF_STYLE_MASK(hideStaff);
    CHECK_STAFF_STYLE_MASK(noKey);
    CHECK_STAFF_STYLE_MASK(fullNamePos);
    CHECK_STAFF_STYLE_MASK(abrvNamePos);
    CHECK_STAFF_STYLE_MASK(altNotation);
    CHECK_STAFF_STYLE_MASK(showTies);
    CHECK_STAFF_STYLE_MASK(showDots);
    CHECK_STAFF_STYLE_MASK(showRests);
    CHECK_STAFF_STYLE_MASK(showStems);
    CHECK_STAFF_STYLE_MASK(hideChords);
    CHECK_STAFF_STYLE_MASK(hideFretboards);
    CHECK_STAFF_STYLE_MASK(hideLyrics);
    CHECK_STAFF_STYLE_MASK(showNameParts);
    CHECK_STAFF_STYLE_MASK(showNoteColors);
    CHECK_STAFF_STYLE_MASK(hideStaffLines);
    CHECK_STAFF_STYLE_MASK(redisplayLayerAccis);
    CHECK_STAFF_STYLE_MASK(negTimeParts);
    CHECK_STAFF_STYLE_MASK(hideKeySigsShowAccis);
#undef CHECK_STAFF_STYLE_MASK

    const auto key = finale_mus_reader::instanceKey<StaffStyle>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1});
    REQUIRE(report.fields.contains(key));
    CHECK(report.fields.at(key).size() == 145);
    CHECK(report.fields.at(key).at("styleName").origin == ValueOrigin::LegacyMus);
    CHECK(report.fields.at(key).at("masks.staffType").origin == ValueOrigin::LegacyMus);
    CHECK(report.fields.at(key).at("masks.negTimeParts").origin == ValueOrigin::LegacyBehavior);
    for (const auto* member : {
             "transposition.setToClef",
             "transposition.noSimplifyKey",
             "transposition.keysig.interval",
             "transposition.keysig.adjust",
             "transposition.chromatic.alteration",
             "transposition.chromatic.diatonic",
         }) {
        CHECK(report.fields.at(key).at(member).origin == ValueOrigin::LegacyMus);
    }
}

TEST_CASE("StaffStyle alternate notation mask includes its chord and fretboard "
          "overrides")
{
    using StaffStyle = musx::dom::others::StaffStyle;
    const auto parsed = makeContainer(
        {{1, "SY", {-24, 0, 0, 0, 0, 0}}, {1, "SY", {0, 1, 4, 24, 0, 0}}, {1, "SY", {-1285, -772, -4, 0, 0, 0}}, {1, "SY", {0, 0x1000, 0, 0, 0, 0}},
            {1, "SY", {0x5465, 0x7374, 0, 0, 0, 0}}, {1, "SY", {0, 0, 0, 0, 0, 0}}, {1, "SY", {0, 0, 0, 0, 0, 0}}, {1, "SY", {0, 0, 0, 0, 0, 0}}});
    const auto document = emptyStaffDocument();
    const auto report = staffImport(parsed, profileFor(finale_mus_reader::versions::finale2000.major), document, true);
    const auto style = document->getOthers()->get<StaffStyle>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1});

    REQUIRE(style);
    REQUIRE(style->masks);
    CHECK(style->masks->altNotation);
    CHECK(style->masks->hideChords);
    CHECK(style->masks->hideFretboards);

    const auto& fields = report.fields.at(finale_mus_reader::instanceKey<StaffStyle>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}));
    CHECK(fields.at("masks.altNotation").decodedOffset == fields.at("masks.hideChords").decodedOffset);
    CHECK(fields.at("masks.altNotation").decodedOffset == fields.at("masks.hideFretboards").decodedOffset);
}

TEST_CASE("Legacy Note Shapes notation uses the staff-specific StaffStyle mask")
{
    using StaffStyle = musx::dom::others::StaffStyle;
    const auto parsed = makeContainer({{1, "SY", {-24, 0, 0, 0, 0, 0x0080}}, {1, "SY", {0, 1, 4, 24, 0, 0}}, {1, "SY", {-1285, -772, -4, 0, 0, 0}},
        {1, "SY", {0x0004, 0, 0, 0, 0, 0}}, {1, "SY", {0x5465, 0x7374, 0, 0, 0}}, {1, "SY", {0, 0, 0, 0, 0, 0}}, {1, "SY", {0, 0, 0, 0, 0, 0}},
        {1, "SY", {0, 0, 0, 0, 0, 0}}});
    const auto document = emptyStaffDocument();
    const auto report = staffImport(parsed, profileFor(finale_mus_reader::versions::finale2000.major), document, true);
    const auto style = document->getOthers()->get<StaffStyle>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1});

    REQUIRE(style);
    REQUIRE(style->masks);
    CHECK(style->notationStyle == Staff::NotationStyle::Standard);
    CHECK(style->useNoteShapes);
    CHECK_FALSE(style->masks->notationStyle);
    CHECK(style->masks->useNoteShapes);

    const auto& fields = report.fields.at(finale_mus_reader::instanceKey<StaffStyle>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}));
    CHECK(fields.at("notationStyle").origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(fields.at("useNoteShapes").origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(fields.at("masks.notationStyle").origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(fields.at("masks.useNoteShapes").origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(fields.at("masks.notationStyle").decodedOffset == fields.at("masks.useNoteShapes").decodedOffset);
}

TEST_CASE("Finale 2006 Note Shapes StaffStyle uses only its staff-specific "
          "notation mask")
{
    using StaffStyle = musx::dom::others::StaffStyle;
    const auto result = readFixture("evidence/F2006/F2006-embedded-tif.mus");
    const auto style = result.document->getOthers()->get<StaffStyle>(musx::dom::SCORE_PARTID, musx::dom::Cmper{19});

    REQUIRE(style);
    CHECK(style->styleName == "15.  Note Shapes");
    CHECK(style->notationStyle == Staff::NotationStyle::Standard);
    CHECK(style->useNoteShapes);
    REQUIRE(style->masks);
    CHECK_FALSE(style->masks->notationStyle);
    CHECK(style->masks->useNoteShapes);

    const auto* notationMask = result.report.findField<StaffStyle>("masks.notationStyle", musx::dom::SCORE_PARTID, musx::dom::Cmper{19});
    const auto* noteShapesMask = result.report.findField<StaffStyle>("masks.useNoteShapes", musx::dom::SCORE_PARTID, musx::dom::Cmper{19});
    REQUIRE(notationMask);
    REQUIRE(noteShapesMask);
    CHECK(notationMask->origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(noteShapesMask->origin == ValueOrigin::LegacyMusAdjusted);
}

TEST_CASE("Finale 2012 Note Shapes settings retain independent StaffStyle masks")
{
    using StaffStyle = musx::dom::others::StaffStyle;
    std::vector<std::int16_t> words(48, 0);
    words[5] = 0x0080;
    words.insert(words.end(), {0x0004, 0, 0, 0, 0, 0});
    words.resize(150, 0);
    const auto parsed = makeClassContainer(0x00e8, words, ByteOrder::LittleEndian, 1);
    const auto document = emptyStaffDocument();
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    profile.version = SourceVersion{.major = finale_mus_reader::versions::finale2012.major};
    const auto report = staffImport(parsed, profile, document, true);
    const auto style = document->getOthers()->get<StaffStyle>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1});

    REQUIRE(style);
    REQUIRE(style->masks);
    CHECK(style->notationStyle == Staff::NotationStyle::Standard);
    CHECK(style->useNoteShapes);
    CHECK(style->masks->notationStyle);
    CHECK_FALSE(style->masks->useNoteShapes);

    const auto& fields = report.fields.at(finale_mus_reader::instanceKey<StaffStyle>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}));
    CHECK(fields.at("notationStyle").origin == ValueOrigin::LegacyMus);
    CHECK(fields.at("useNoteShapes").origin == ValueOrigin::LegacyMus);
    CHECK(fields.at("masks.notationStyle").origin == ValueOrigin::LegacyMus);
    CHECK(fields.at("masks.useNoteShapes").origin == ValueOrigin::LegacyMus);
}

TEST_CASE("StaffStyle selects and decodes its Unicode trailer structurally")
{
    using StaffStyle = musx::dom::others::StaffStyle;
    std::vector<std::int16_t> words(48, 0);
    words.insert(words.end(), {0, 0x1000, 0x0006, 0, 0, 0x0006});
    words.resize(150, 0);
    const std::u16string name = u"Élan U0001D11E";
    for (std::size_t index = 0; index < name.size(); ++index) {
        words[54 + index] = static_cast<std::int16_t>(name[index]);
    }
    const auto parsed = makeClassContainer(0x00e8, words, ByteOrder::LittleEndian, 1);
    const auto document = emptyStaffDocument();
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    const auto report = staffImport(parsed, profile, document, true);

    const auto style = document->getOthers()->get<StaffStyle>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1});
    REQUIRE(style);
    CHECK(style->styleName == "Élan U0001D11E");
    CHECK(style->copyable);
    CHECK(style->addToMenu);
    REQUIRE(style->masks);
    CHECK(style->masks->altNotation);
    CHECK(style->masks->hideChords);
    CHECK(style->masks->hideFretboards);
    CHECK(report.fields.at(finale_mus_reader::instanceKey<StaffStyle>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1})).at("styleName").origin
          == ValueOrigin::LegacyMus);
}

TEST_CASE("Finale 2012 StaffStyle records recover their stored names and controls")
{
    using StaffStyle = musx::dom::others::StaffStyle;
    const auto result = readFixture("evidence/F2012/F2012-bookmarks.mus");
    constexpr std::string_view names[] = {
        "Normal Notation", "Slash Notation", "Rhythmic Notation", "One Bar Repeats", "Two Bar Repeats", "Blank Notation"};
    for (std::size_t index = 0; index < std::size(names); ++index) {
        const auto style = result.document->getOthers()->get<StaffStyle>(musx::dom::SCORE_PARTID, static_cast<musx::dom::Cmper>(index + 1));
        REQUIRE(style);
        CHECK(style->styleName == names[index]);
        CHECK(style->copyable);
        CHECK(style->addToMenu);
        REQUIRE(style->masks);
        CHECK(style->masks->altNotation);
        CHECK(style->masks->hideChords);
        CHECK(style->masks->hideFretboards);
    }
}

TEST_CASE("StaffStyle uses its Finale 2012 boundary for unrepresented record "
          "lengths")
{
    using StaffStyle = musx::dom::others::StaffStyle;
    const auto parsed = makeClassContainer(0x00e8, std::vector<std::int16_t>(100), ByteOrder::LittleEndian, 1);
    const auto importWithVersion = [&](std::uint8_t major) {
        const auto document = emptyStaffDocument();
        auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
        profile.byteOrder = ByteOrder::LittleEndian;
        profile.version = SourceVersion{.major = major};
        return std::pair{document, staffImport(parsed, profile, document, true)};
    };

    const auto [finale2011Document, finale2011Report] = importWithVersion(finale_mus_reader::versions::finale2011.major);
    CHECK(finale2011Document->getOthers()->get<StaffStyle>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}));
    CHECK(finale2011Report.diagnostics.empty());

    const auto [finale2012Document, finale2012Report] = importWithVersion(finale_mus_reader::versions::finale2012.major);
    CHECK_FALSE(finale2012Document->getOthers()->get<StaffStyle>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}));
    REQUIRE(finale2012Report.diagnostics.size() == 1);
    CHECK(finale2012Report.diagnostics.front().message.find("shorter than") != std::string::npos);
}

TEST_CASE("Direct-record StaffStyle remains absent when no source record exists")
{
    using StaffStyle = musx::dom::others::StaffStyle;
    for (const auto path : {"evidence/F2000/F2000-baseline.mus", "evidence/F2011/F2011-baseline.mus"}) {
        const auto result = readFixture(path);
        CHECK(result.document->getOthers()->getAllSources<StaffStyle>().empty());
    }
}
} // namespace
} // namespace finale_mus_reader_tests
