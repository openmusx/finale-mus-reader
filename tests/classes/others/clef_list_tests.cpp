// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using ClefList = musx::dom::others::ClefList;
using musx::dom::ShowClefMode;
constexpr musx::dom::Cmper clefListCmper = 4;
constexpr musx::dom::Cmper fixtureClefListCmper = 1;
constexpr std::uint16_t clefListClassId = 0x007f;

ImportReport importClefList(const finale_mus_reader::container::ParsedContainer& parsed, musx::dom::DocumentPtr& document)
{
    const auto index = LegacyRecordIndex::build(parsed);
    auto session = musx::factory::DocumentFactory::begin();
    if (!document) {
        document = session.getDocument();
    }
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    ImportReport report(parsed.formatEpoch);
    finale_mus_reader::PendingReferences pending;
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importClefLists(context);
    finale_mus_reader::runDeferredChecks(pending);
    return report;
}

// A barline treble clef, then a hidden bass clef at Edu 1536 moved 12 EVPU down and 20 left,
// then a forced alto clef carrying the vertical-drag and after-barline bits.
constexpr std::array<std::int16_t, 6> barlineClefItem{0, 0, 0, 75, 0, 0};
constexpr std::array<std::int16_t, 6> hiddenClefItem{3, 1536, -12, 75, -20, 0x0002};
constexpr std::array<std::int16_t, 6> forcedClefItem{1, 3072, 0, 60, 0, 0x001c};

void checkClefList(const musx::dom::DocumentPtr& document, const ImportReport& report)
{
    const auto list = document->getOthers()->getArray<ClefList>(musx::dom::SCORE_PARTID, clefListCmper);
    REQUIRE(list.size() == 3);

    CHECK(list[0]->clefIndex == 0);
    CHECK(list[0]->xEduPos == 0);
    CHECK(list[0]->percent == 75);
    CHECK(list[0]->clefMode == ShowClefMode::WhenNeeded);
    CHECK_FALSE(list[0]->unlockVert);
    CHECK_FALSE(list[0]->afterBarline);

    CHECK(list[1]->clefIndex == 3);
    CHECK(list[1]->xEduPos == 1536);
    CHECK(list[1]->yEvpuPos == -12);
    CHECK(list[1]->percent == 75);
    CHECK(list[1]->xEvpuOffset == -20);
    CHECK(list[1]->clefMode == ShowClefMode::Never);
    CHECK_FALSE(list[1]->unlockVert);
    CHECK_FALSE(list[1]->afterBarline);

    CHECK(list[2]->clefIndex == 1);
    CHECK(list[2]->xEduPos == 3072);
    CHECK(list[2]->percent == 60);
    CHECK(list[2]->clefMode == ShowClefMode::Always);
    CHECK(list[2]->unlockVert);
    CHECK(list[2]->afterBarline);

    for (musx::dom::Inci inci = 0; inci < 3; ++inci) {
        const auto key = finale_mus_reader::instanceKey<ClefList>(musx::dom::SCORE_PARTID, clefListCmper, inci);
        REQUIRE(report.fields.contains(key));
        const auto& fields = report.fields.at(key);
        CHECK(fields.size() == ClefList::xmlMappingArray().size());
        for (const auto& [member, info] : fields) {
            INFO(member);
            CHECK(info.origin == ValueOrigin::LegacyMus);
        }
    }
    CHECK(field(report, "others.clefEnum[4,1].clefIndex").rawValue == 3);
    CHECK(field(report, "others.clefEnum[4,1].clefMode").rawValue == 0x0002);
    CHECK(field(report, "others.clefEnum[4,2].afterBarline").rawValue == 0x001c);
}

std::vector<SyntheticRow> fixedClefRows()
{
    return {{clefListCmper, "CE", barlineClefItem}, {clefListCmper, "CE", hiddenClefItem}, {clefListCmper, "CE", forcedClefItem}};
}

std::vector<std::int16_t> clefClassWords()
{
    std::vector<std::int16_t> words;
    for (const auto* item : {&barlineClefItem, &hiddenClefItem, &forcedClefItem}) {
        words.insert(words.end(), item->begin(), item->end());
    }
    return words;
}

TEST_CASE("ClefList recovers one item per incidence in every fixed-row epoch", "[class][clef-list]")
{
    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            musx::dom::DocumentPtr document;
            const auto report = importClefList(makeContainer(fixedClefRows(), epoch, byteOrder), document);
            checkClefList(document, report);
        }
    }
}

TEST_CASE("ClefList splits a zlib payload into six-word items", "[class][clef-list]")
{
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        musx::dom::DocumentPtr document;
        const auto report = importClefList(makeClassContainer(clefListClassId, clefClassWords(), byteOrder, clefListCmper), document);
        checkClefList(document, report);
    }
}

TEST_CASE("ClefList converts a Coda-banner list through the measure named by its frame", "[class][clef-list]")
{
    // Measures 600 EVPU wide whose music starts 36 EVPU in, so a stored 145 EVPU is Edu 792 of a
    // 4096-Edu measure and 318 EVPU is Edu 2048. Frames 1, 3, and 4 name lists 2, 5, and 6; frame 2
    // holds alto clef 4 alone. Stored 30 EVPU precedes the music, so list 6's only clef moves to
    // the barline and leaves no list: frame 4 takes that clef as its single clef. No frame names
    // list 7.
    auto session = musx::factory::DocumentFactory::begin();
    musx::dom::DocumentPtr document = session.getDocument();
    auto spacing = std::make_shared<Spacing>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All);
    spacing->musFront = 36;
    document->getOptions()->add(Spacing::XmlNodeName, std::move(spacing));
    auto staff =
        std::make_shared<musx::dom::others::Staff>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper{1});
    staff->defaultClef = 0;
    document->getOthers()->add(musx::dom::others::Staff::XmlNodeName, std::move(staff));
    for (musx::dom::Cmper meas = 1; meas <= 4; ++meas) {
        auto measure = std::make_shared<musx::dom::others::Measure>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, meas);
        measure->width = 600;
        measure->beats = 4;
        measure->divBeat = 1024;
        document->getOthers()->add(musx::dom::others::Measure::XmlNodeName, std::move(measure));
    }
    // The frame of measure 4 as the GFrameHold importer leaves it, naming list 6.
    auto hold = std::make_shared<musx::dom::details::GFrameHold>(
        document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper{1}, musx::dom::Cmper{4});
    hold->clefListId = 6;
    document->getDetails()->add(musx::dom::details::GFrameHold::XmlNodeName, std::move(hold));
    auto parsed = makeContainer(
        {{2, "CE", {3, 145, -8, 75, 0, 0}}, {5, "CE", {1, 318, 0, 75, 0, 0}}, {6, "CE", {2, 30, 0, 75, 0, 0}}, {7, "CE", {3, 200, 0, 75, 0, 0}}},
        FormatEpoch::CodaBanner);
    for (const auto& [meas, frame] : {std::pair<std::uint16_t, std::vector<std::int16_t>>{1, {91, 2, 0, 0, 0x0400}}, {2, {0, 4, 0, 0, 0}},
             {3, {0, 5, 0, 0, 0x0400}}, {4, {0, 6, 0, 0, 0x0400}}}) {
        parsed.blocks.push_back(std::move(makeDetailContainer(FormatEpoch::CodaBanner, 1, meas, frame, "GF").blocks.front()));
    }
    const auto report = importClefList(parsed, document);

    const auto first = document->getOthers()->getArray<ClefList>(musx::dom::SCORE_PARTID, musx::dom::Cmper{2});
    REQUIRE(first.size() == 2);
    CHECK(first[0]->clefIndex == 0);
    CHECK(first[0]->xEduPos == 0);
    CHECK(first[0]->percent == 0);
    CHECK(first[1]->clefIndex == 3);
    CHECK(first[1]->xEduPos == 792);
    CHECK(first[1]->yEvpuPos == -8);
    CHECK(first[1]->percent == 75);
    const auto second = document->getOthers()->getArray<ClefList>(musx::dom::SCORE_PARTID, musx::dom::Cmper{5});
    REQUIRE(second.size() == 2);
    CHECK(second[0]->clefIndex == 4);
    CHECK(second[1]->clefIndex == 1);
    CHECK(second[1]->xEduPos == 2048);
    CHECK(document->getOthers()->getArray<ClefList>(musx::dom::SCORE_PARTID, musx::dom::Cmper{6}).empty());
    const auto singleClef = document->getDetails()->get<musx::dom::details::GFrameHold>(musx::dom::SCORE_PARTID, 1, 4);
    REQUIRE(singleClef);
    CHECK(singleClef->clefId == 2);
    CHECK(singleClef->clefListId == 0);
    const auto holdKey = finale_mus_reader::instanceKey<musx::dom::details::GFrameHold>(
        musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, std::nullopt, musx::dom::Cmper{4});
    REQUIRE(report.fields.contains(holdKey));
    CHECK(report.fields.at(holdKey).at("clefId").origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(report.fields.at(holdKey).at("clefId").rawValue == 2);
    CHECK(report.fields.at(holdKey).at("clefListId").origin == ValueOrigin::LegacyBehavior);
    CHECK(document->getOthers()->getArray<ClefList>(musx::dom::SCORE_PARTID, musx::dom::Cmper{7}).empty());

    CHECK(field(report, "others.clefEnum[2,0].clefIndex").origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(field(report, "others.clefEnum[2,0].percent").origin == ValueOrigin::LegacyBehavior);
    CHECK(field(report, "others.clefEnum[2,1].xEduPos").origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(field(report, "others.clefEnum[2,1].xEduPos").rawValue == 145);
    CHECK(field(report, "others.clefEnum[2,1].yEvpuPos").origin == ValueOrigin::LegacyMus);
}

TEST_CASE("ClefList rejects an incomplete trailing item", "[class][clef-list]")
{
    std::vector<std::int16_t> words(barlineClefItem.begin(), barlineClefItem.end());
    words.push_back(3);
    musx::dom::DocumentPtr document;
    const auto report = importClefList(makeClassContainer(clefListClassId, words, ByteOrder::LittleEndian, clefListCmper), document);
    CHECK(document->getOthers()->getArray<ClefList>(musx::dom::SCORE_PARTID, clefListCmper).size() == 1);
    CHECK(std::any_of(report.diagnostics.begin(), report.diagnostics.end(),
        [](const auto& diagnostic) { return diagnostic.message.find("incomplete trailing item") != std::string::npos; }));
}

TEST_CASE("ClefList recovers a controlled DCL list", "[class][clef-list][fixture]")
{
    const auto result = readFixture("evidence/F2001/F2001Win-midclef.mus");
    const auto list = result.document->getOthers()->getArray<ClefList>(musx::dom::SCORE_PARTID, fixtureClefListCmper);
    REQUIRE(list.size() == 3);
    CHECK(list[0]->clefIndex == 0);
    CHECK(list[0]->xEduPos == 0);
    CHECK(list[0]->percent == 75);
    CHECK_FALSE(list[0]->unlockVert);
    CHECK_FALSE(list[0]->afterBarline);
    CHECK(list[1]->clefIndex == 3);
    CHECK(list[1]->xEduPos == 1645);
    CHECK(list[1]->yEvpuPos == 12);
    CHECK(list[1]->percent == 75);
    CHECK(list[1]->xEvpuOffset == 0);
    CHECK(list[1]->clefMode == ShowClefMode::WhenNeeded);
    CHECK(list[1]->unlockVert);
    CHECK_FALSE(list[1]->afterBarline);
    CHECK(list[2]->clefIndex == 2);
    CHECK(list[2]->xEduPos == 2912);
    CHECK(list[2]->yEvpuPos == 0);
    CHECK_FALSE(list[2]->unlockVert);
    CHECK(list[2]->afterBarline);
    CHECK(field(result, "others.clefEnum[1,1].unlockVert").rawValue == 0x0004);
    CHECK(field(result, "others.clefEnum[1,2].afterBarline").rawValue == 0x0010);
}

TEST_CASE("ClefList recovers a Coda-banner list upgraded by Finale 3.7.2", "[class][clef-list][fixture]")
{
    const auto result = readFixture("evidence/F372/F372-F263-midclef1.mus");
    const auto list = result.document->getOthers()->getArray<ClefList>(musx::dom::SCORE_PARTID, fixtureClefListCmper);
    REQUIRE(list.size() == 2);
    CHECK(list[0]->clefIndex == 0);
    CHECK(list[0]->xEduPos == 0);
    CHECK(list[0]->percent == 0);
    CHECK(list[1]->clefIndex == 3);
    CHECK(list[1]->xEduPos == 356);
    CHECK(list[1]->yEvpuPos == -16);
    CHECK(list[1]->percent == 75);
}

TEST_CASE("ClefList converts controlled Coda-banner lists", "[class][clef-list][fixture]")
{
    for (const auto& [fixture, position] : {std::pair{"evidence/F100/F100-midclef1.mus", 537}, std::pair{"evidence/F100/F100-midclef2.mus", 1758},
             std::pair{"evidence/F263/F263-midclef1.mus", 356}, std::pair{"evidence/F263/F263-midclef2.mus", 1431}}) {
        CAPTURE(fixture);
        const auto result = readFixture(fixture);
        const auto list = result.document->getOthers()->getArray<ClefList>(musx::dom::SCORE_PARTID, fixtureClefListCmper);
        REQUIRE(list.size() == 2);
        CHECK(list[0]->clefIndex == 0);
        CHECK(list[0]->xEduPos == 0);
        CHECK(list[1]->clefIndex == 3);
        CHECK(list[1]->xEduPos == position);
    }
}

TEST_CASE("ClefList converts Coda-banner lists in measures spaced by beat chart", "[class][clef-list][fixture]")
{
    // Each barline clef is the one in effect: the staff's default, then the clef the previous
    // measure's list ends with.
    const auto result = readFixture("evidence/F263/F263-beatchart.mus");
    for (const auto& [listId, barline, clef, position] :
        {std::tuple{3, 0, 1, 238}, std::tuple{4, 1, 0, 475}, std::tuple{1, 0, 3, 981}, std::tuple{2, 3, 0, 2197}}) {
        CAPTURE(listId);
        const auto list = result.document->getOthers()->getArray<ClefList>(musx::dom::SCORE_PARTID, musx::dom::Cmper(listId));
        REQUIRE(list.size() == 2);
        CHECK(list[0]->clefIndex == barline);
        CHECK(list[1]->clefIndex == clef);
        CHECK(list[1]->xEduPos == position);
    }
}

TEST_CASE("ClefList creates nothing without a stored list", "[class][clef-list][fixture]")
{
    for (const auto* fixture : {"evidence/F100/F100-clef.mus", "evidence/F2005/F2005-clef-baseline.mus"}) {
        CAPTURE(fixture);
        const auto parsed = readFixture(fixture);
        CHECK(parsed.document->getOthers()->getArray<ClefList>(musx::dom::SCORE_PARTID).empty());
        CHECK(std::none_of(parsed.report.diagnostics.begin(), parsed.report.diagnostics.end(), [](const auto& diagnostic) {
            return diagnostic.message.find("clef list") != std::string::npos || diagnostic.message.find("Mid-measure clefs") != std::string::npos;
        }));
    }
}

} // namespace
} // namespace finale_mus_reader_tests
