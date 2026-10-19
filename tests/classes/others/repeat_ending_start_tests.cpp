// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using RepeatEndingStart = musx::dom::others::RepeatEndingStart;
using RepeatPassList = musx::dom::others::RepeatPassList;
using RepeatBack = musx::dom::others::RepeatBack;
using RepeatEndingText = musx::dom::others::RepeatEndingText;

void checkEnding(const finale_mus_reader::container::ParsedContainer& parsed, std::optional<SourceVersion> version, bool modern)
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    profile.version = version;
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importRepeatEndingStarts(context);
    finale_mus_reader::others::importRepeatPassLists(context);
    const auto ending = document->getOthers()->get<RepeatEndingStart>(musx::dom::SCORE_PARTID, musx::dom::Cmper(7));
    REQUIRE(ending);
    CHECK(ending->staffList == (modern ? 9 : 0));
    CHECK(ending->targetValue == -2);
    CHECK(ending->textHPos == 24);
    CHECK(ending->leftHPos == -35);
    CHECK(ending->leftVPos == 12);
    CHECK(ending->individualPlacement);
    CHECK(ending->topStaffOnly == modern);
    CHECK(ending->hidden == modern);
    CHECK(ending->jumpIfIgnoring);
    CHECK(ending->jumpAction == (modern ? musx::dom::others::RepeatActionType::JumpAbsolute : musx::dom::others::RepeatActionType::JumpRelative));
    CHECK(ending->trigger == musx::dom::others::RepeatTriggerType::OnPass);
    CHECK(ending->endLineVPos == 42);
    CHECK(ending->textVPos == 18);
    CHECK(ending->rightHPos == 81);
    CHECK(ending->rightVPos == -23);
    const auto passes = document->getOthers()->get<RepeatPassList>(musx::dom::SCORE_PARTID, musx::dom::Cmper(7));
    REQUIRE(passes);
    CHECK(passes->values == std::vector<int>{1, 3});
    for (const auto* name : {"staffList", "targetValue", "textHPos", "leftHPos", "leftVPos", "individualPlacement", "topStaffOnly", "hidden",
             "jumpAction", "trigger", "jumpIfIgnoring", "endLineVPos", "textVPos", "rightHPos", "rightVPos"}) {
        REQUIRE(report.findField<RepeatEndingStart>(name, musx::dom::SCORE_PARTID, musx::dom::Cmper(7)));
    }
    CHECK(report.findField<RepeatEndingStart>("trigger", musx::dom::SCORE_PARTID, musx::dom::Cmper(7))->origin == ValueOrigin::LegacyBehavior);
    CHECK(report.findField<RepeatEndingStart>("staffList", musx::dom::SCORE_PARTID, musx::dom::Cmper(7))->origin
          == (modern ? ValueOrigin::LegacyMus : ValueOrigin::Finale27Default));
    CHECK(report.findField<RepeatPassList>("values[0]", musx::dom::SCORE_PARTID, musx::dom::Cmper(7))->rawValue == 1);
    CHECK(report.findField<RepeatPassList>("values[1]", musx::dom::SCORE_PARTID, musx::dom::Cmper(7))->rawValue == 3);
    CHECK(RepeatEndingStart::xmlMappingArray().size() == 15);
    CHECK(RepeatPassList::xmlMappingArray().size() == 1);
}

TEST_CASE("Repeat ending and pass list decode the fixed-row layout", "[class][repeat-ending]")
{
    const auto rows =
        std::vector<SyntheticRow>{{7, "ES", {0, -2, 24, -35, 12, 0x3001}}, {7, "ES", {0, 42, 18, 81, -23, 0}}, {7, "EE", {1, 3, 0, 0, 0, 0}}};
    checkEnding(makeContainer(rows, FormatEpoch::CodaBanner), SourceVersion{.major = 2}, false);
    checkEnding(makeContainer(rows, FormatEpoch::UncompressedLegacy), SourceVersion{.major = 5}, false);
    checkEnding(makeContainer(rows, FormatEpoch::DclLegacy), SourceVersion{.major = 9}, false);
}

TEST_CASE("Repeat ending and pass list decode the Finale 2005 and zlib layouts", "[class][repeat-ending]")
{
    const auto rows =
        std::vector<SyntheticRow>{{7, "ES", {9, -2, 24, -35, 12, 0x2417}}, {7, "ES", {0, 42, 18, 81, -23, 0}}, {7, "EE", {1, 3, 0, 0, 0, 0}}};
    checkEnding(makeContainer(rows, FormatEpoch::DclLegacy), SourceVersion{.major = 10}, true);
    checkEnding(makeClassContainer({SyntheticClassRow{0x00cc, {9, -2, 24, -35, 12, 0x2417, 0, 42, 18, 81, -23, 0}, 7},
                                       SyntheticClassRow{0x00ce, {1, 3, 0, 0, 0, 0}, 7}},
                    ByteOrder::LittleEndian),
        SourceVersion{.major = 12}, true);
}

std::string endingText(const finale_mus_reader::container::ParsedContainer& parsed, std::optional<SourceVersion> version, std::int64_t storedBytes,
    finale_mus_reader::SourcePlatform platform = finale_mus_reader::SourcePlatform::Unknown)
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    profile.version = version;
    profile.platform = platform;
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importRepeatEndingTexts(context);
    const auto text = document->getOthers()->get<RepeatEndingText>(musx::dom::SCORE_PARTID, musx::dom::Cmper(7));
    REQUIRE(text);
    const auto* field = report.findField<RepeatEndingText>("text", musx::dom::SCORE_PARTID, musx::dom::Cmper(7));
    REQUIRE(field);
    CHECK(field->origin == ValueOrigin::LegacyMus);
    CHECK(field->rawValue == storedBytes);
    CHECK(RepeatEndingText::xmlMappingArray().size() == 1);
    return text->text;
}

TEST_CASE("Repeat ending text spans incidences as narrow or UTF-16 characters", "[class][repeat-ending]")
{
    const auto rows = std::vector<SyntheticRow>{{7, "ET", {0x4142, 0x4344, 0x4546, 0x4748, 0x494a, 0x4b4c}}, {7, "ET", {0x4d00, 0x4e4f, 0, 0, 0, 0}}};
    CHECK(endingText(makeContainer(rows, FormatEpoch::UncompressedLegacy), SourceVersion{.major = 5}, 24) == "ABCDEFGHIJKLM");
    CHECK(endingText(
              makeClassContainer({SyntheticClassRow{0x00cd, {0x2e31, 0, 0, 0, 0, 0}, 7}}, ByteOrder::LittleEndian), SourceVersion{.major = 16}, 12)
          == "1.");
    CHECK(endingText(makeClassContainer({SyntheticClassRow{0x00cd, {0x00e9, 0x0031, 0, 0x0032, 0, 0}, 7}}, ByteOrder::LittleEndian),
              SourceVersion{.major = 17}, 12)
          == "\xc3\xa9"
             "1");
}

TEST_CASE("Repeat ending text without an ending font uses the source platform encoding", "[class][repeat-ending]")
{
    const auto rows = std::vector<SyntheticRow>{{7, "ET", {0x6ce5, 0, 0, 0, 0, 0}}};
    const auto parsed = makeContainer(rows, FormatEpoch::DclLegacy);
    CHECK(endingText(parsed, SourceVersion{.major = 6}, 12, finale_mus_reader::SourcePlatform::Windows) == "l\xc3\xa5");
    CHECK(endingText(parsed, SourceVersion{.major = 6}, 12, finale_mus_reader::SourcePlatform::MacOS) == "l\xc3\x82");
}

TEST_CASE("Repeat ending text is stored as narrow text in Finale 1.0 and UTF-16 in Finale 2012", "[class][repeat-ending]")
{
    for (const auto& [fixture, expected] :
        std::array{std::pair{"evidence/F100/F100-rptstart.mus", "dirst"}, std::pair{"evidence/F2012/F2012-rptstart.mus", "first"}}) {
        const auto result = readFixture(fixture);
        const auto text = result.document->getOthers()->get<RepeatEndingText>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(text);
        CHECK(text->text == expected);
        CHECK(result.report.findField<RepeatEndingText>("text", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
    }
}

TEST_CASE("Repeat ending text uses the ending font's Windows character set", "[class][repeat-ending]")
{
    const auto result = readFixture("evidence/F2001/F2001Win-endingtext.mus");
    const auto text = result.document->getOthers()->get<RepeatEndingText>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(text);
    CHECK(text->text == "This is \xc3\xa5 l\xc3\xb6ng ending");
    const auto* field = result.report.findField<RepeatEndingText>("text", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(field);
    CHECK(field->origin == ValueOrigin::LegacyMus);
    CHECK(field->rawValue == 24);
}

TEST_CASE("Finale 1.0 ending separates the compact text offsets", "[class][repeat-ending]")
{
    const auto result = readFixture("evidence/F100/F100-rptstart.mus");
    const auto ending = result.document->getOthers()->get<RepeatEndingStart>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(ending);
    CHECK(ending->targetValue == 2);
    CHECK(ending->textHPos == 24);
    CHECK(ending->textVPos == 24);
    CHECK(ending->jumpAction == musx::dom::others::RepeatActionType::JumpRelative);
    CHECK(ending->trigger == musx::dom::others::RepeatTriggerType::OnPass);
    const auto* textVPos = result.report.findField<RepeatEndingStart>("textVPos", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(textVPos);
    CHECK(textVPos->origin == ValueOrigin::LegacyMus);
    CHECK(textVPos->rawValue == 24);
    const auto passes = result.document->getOthers()->get<RepeatPassList>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(passes);
    CHECK(passes->values == std::vector<int>{1, 2, 3});
}

TEST_CASE("Finale 1.0 ending decodes packed positions and a zero target", "[class][repeat-ending]")
{
    const auto zeroTarget = readFixture("evidence/F100/F100-rptstart-jump0.mus");
    const auto zeroEnding = zeroTarget.document->getOthers()->get<RepeatEndingStart>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(zeroEnding);
    CHECK(zeroEnding->targetValue == 0);
    CHECK(zeroEnding->jumpAction == musx::dom::others::RepeatActionType::JumpRelative);

    const auto moved = readFixture("evidence/F100/F100-rptstart-moved.mus");
    const auto ending = moved.document->getOthers()->get<RepeatEndingStart>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(ending);
    CHECK(ending->textHPos == 31);
    CHECK(ending->textVPos == 4);
    CHECK(ending->leftHPos == -10);
    CHECK(ending->leftVPos == 52);
    CHECK(ending->rightHPos == -1);
    CHECK(ending->rightVPos == -36);
    CHECK(ending->endLineVPos == -36);
    const auto* rightHPos = moved.report.findField<RepeatEndingStart>("rightHPos", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(rightHPos);
    CHECK(rightHPos->origin == ValueOrigin::LegacyMus);
    CHECK(rightHPos->rawValue == -1);
}

TEST_CASE("Pre-2005 ending action follows its era and flag", "[class][repeat-ending]")
{
    struct Case
    {
        const char* path;
        musx::dom::Cmper cmper;
        int targetValue;
        int storedTargetValue;
        musx::dom::others::RepeatActionType action;
    };
    const std::array cases{
        Case{"evidence/F372/F372-openclose.mus", musx::dom::Cmper(2), 0, 0, musx::dom::others::RepeatActionType::JumpRelative},
        Case{"evidence/F2000/F2000-rptjump0.mus", musx::dom::Cmper(1), 0, 0, musx::dom::others::RepeatActionType::JumpAbsolute},
        Case{"evidence/F2000/F2000-rptjump0-stop.mus", musx::dom::Cmper(1), -1, 0, musx::dom::others::RepeatActionType::Stop},
        Case{"evidence/F2002/F2002-jump0.mus", musx::dom::Cmper(1), 0, 0, musx::dom::others::RepeatActionType::JumpAbsolute},
        Case{"evidence/F2002/F2002-jump3.mus", musx::dom::Cmper(1), 3, 3, musx::dom::others::RepeatActionType::JumpAbsolute},
        Case{"evidence/F2002/F2002-jump0-stop.mus", musx::dom::Cmper(1), -1, 0, musx::dom::others::RepeatActionType::Stop},
        Case{"evidence/F2002/F2002-jump3-stop.mus", musx::dom::Cmper(1), -1, 3, musx::dom::others::RepeatActionType::Stop},
    };
    for (const auto& test : cases) {
        const auto result = readFixture(test.path);
        const auto ending = result.document->getOthers()->get<RepeatEndingStart>(musx::dom::SCORE_PARTID, test.cmper);
        REQUIRE(ending);
        CHECK(ending->targetValue == test.targetValue);
        CHECK(ending->jumpAction == test.action);
        CHECK(ending->trigger == musx::dom::others::RepeatTriggerType::OnPass);
        const auto* targetValue = result.report.findField<RepeatEndingStart>("targetValue", musx::dom::SCORE_PARTID, test.cmper);
        REQUIRE(targetValue);
        CHECK(targetValue->rawValue == test.storedTargetValue);
        CHECK(targetValue->origin
              == (test.action == musx::dom::others::RepeatActionType::Stop ? ValueOrigin::LegacyMusAdjusted : ValueOrigin::LegacyMus));
    }
}

TEST_CASE("Finale 3.7.2 ending uses its right line for the end line", "[class][repeat-ending]")
{
    const auto result = readFixture("evidence/F372/F372-openclose-moved.mus");
    const auto ending = result.document->getOthers()->get<RepeatEndingStart>(musx::dom::SCORE_PARTID, musx::dom::Cmper(2));
    REQUIRE(ending);
    CHECK(ending->textHPos == 59);
    CHECK(ending->leftHPos == -20);
    CHECK(ending->leftVPos == 56);
    CHECK(ending->textVPos == -12);
    CHECK(ending->rightVPos == -40);
    CHECK(ending->endLineVPos == -40);
    const auto* endLine = result.report.findField<RepeatEndingStart>("endLineVPos", musx::dom::SCORE_PARTID, musx::dom::Cmper(2));
    REQUIRE(endLine);
    CHECK(endLine->origin == ValueOrigin::LegacyMus);
    CHECK(endLine->rawValue == -40);
}

TEST_CASE("Finale 2005 ending and backward repeat keep distinct staff lists", "[class][repeat-ending]")
{
    const auto result = readFixture("evidence/F2005/F2005-rptstafflists.mus");
    const auto ending = result.document->getOthers()->get<RepeatEndingStart>(musx::dom::SCORE_PARTID, musx::dom::Cmper(2));
    const auto repeatBack = result.document->getOthers()->get<RepeatBack>(musx::dom::SCORE_PARTID, musx::dom::Cmper(2));
    const auto passes = result.document->getOthers()->get<RepeatPassList>(musx::dom::SCORE_PARTID, musx::dom::Cmper(2));
    REQUIRE(ending);
    REQUIRE(repeatBack);
    REQUIRE(passes);
    CHECK(ending->staffList == 1);
    CHECK(repeatBack->staffList == 2);
    CHECK(passes->values == std::vector<int>{1});
    const auto* endingStaffList = result.report.findField<RepeatEndingStart>("staffList", musx::dom::SCORE_PARTID, musx::dom::Cmper(2));
    const auto* backStaffList = result.report.findField<RepeatBack>("staffList", musx::dom::SCORE_PARTID, musx::dom::Cmper(2));
    REQUIRE(endingStaffList);
    REQUIRE(backStaffList);
    CHECK(endingStaffList->origin == ValueOrigin::LegacyMus);
    CHECK(endingStaffList->rawValue == 1);
    CHECK(backStaffList->origin == ValueOrigin::LegacyMus);
    CHECK(backStaffList->rawValue == 2);
}

TEST_CASE("Finale 2012 ending and its part-specific hidden flag", "[class][repeat-ending]")
{
    for (const auto* fixture : {"evidence/F2012/F2012-rptstart.mus", "evidence/F2012/F2012-rptstart-hidden.mus"}) {
        const auto result = readFixture(fixture);
        const auto ending = result.document->getOthers()->get<RepeatEndingStart>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(ending);
        CHECK(ending->targetValue == 3);
        CHECK(ending->textHPos == 24);
        CHECK(ending->textVPos == 24);
        CHECK(ending->individualPlacement == (fixture == std::string_view("evidence/F2012/F2012-rptstart.mus")));
        CHECK(ending->topStaffOnly);
        CHECK(ending->jumpIfIgnoring);
        CHECK_FALSE(ending->hidden);
        CHECK(ending->jumpAction == musx::dom::others::RepeatActionType::JumpRelative);
        CHECK(ending->trigger == musx::dom::others::RepeatTriggerType::OnPass);
        const auto passes = result.document->getOthers()->get<RepeatPassList>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(passes);
        CHECK(passes->values == std::vector<int>{1, 2, 3});
        CHECK(result.report.findField<RepeatEndingStart>("textVPos", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
        if (fixture == std::string_view("evidence/F2012/F2012-rptstart-hidden.mus")) {
            const auto part = result.document->getOthers()->get<RepeatEndingStart>(musx::dom::Cmper(1), musx::dom::Cmper(1));
            REQUIRE(part);
            CHECK(part->hidden);
            CHECK_FALSE(part->individualPlacement);
            const auto* hidden = result.report.findField<RepeatEndingStart>("hidden", musx::dom::Cmper(1), musx::dom::Cmper(1));
            REQUIRE(hidden);
            CHECK(hidden->origin == ValueOrigin::LegacyMus);
            CHECK(hidden->rawValue == 1);
        }
    }
}

} // namespace
} // namespace finale_mus_reader_tests
