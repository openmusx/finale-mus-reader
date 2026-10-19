// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"
#include "import/shared/gframe_records.h"

#include <array>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <utility>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using GFrameHoldTarget = musx::dom::details::GFrameHold;

auto gframeKey(musx::dom::Cmper measure)
{
    return finale_mus_reader::instanceKey<GFrameHoldTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, std::nullopt, measure);
}

std::pair<musx::dom::DocumentPtr, ImportReport> importSynthetic(
    const finale_mus_reader::container::ParsedContainer& parsed, int clefChangePercent = 75)
{
    const auto index = LegacyRecordIndex::build(parsed);
    auto session = musx::factory::DocumentFactory::begin();
    auto document = session.getDocument();
    auto clefOptions = std::make_shared<musx::dom::options::ClefOptions>(document);
    clefOptions->clefChangePercent = clefChangePercent;
    document->getOptions()->add(musx::dom::options::ClefOptions::XmlNodeName, std::move(clefOptions));
    // Frames are imported only for measures the score has; these tests address measure 1.
    document->getOthers()->add(musx::dom::others::Measure::XmlNodeName,
        std::make_shared<musx::dom::others::Measure>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper{1}));
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    ImportReport report(parsed.formatEpoch);
    finale_mus_reader::PendingReferences pending;
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::details::importGFrameHolds(context);
    return {document, std::move(report)};
}

std::pair<musx::dom::DocumentPtr, ImportReport> importFlags(std::uint16_t flags)
{
    return importSynthetic(
        makeDetailClassContainer(1, 1, musx::dom::SCORE_PARTID, {2, static_cast<std::int16_t>(flags), 75, 0, 0, 0, 0}, ByteOrder::BigEndian, 0x0414));
}

TEST_CASE("Early GFrameHold layer links resolve their LL rows", "[class][gframe-hold]")
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy}) {
        auto parsed = makeDetailContainer(epoch, 1, 1, {1013, 0, 0, 19, 0}, "GF");
        for (const auto& [layer, frame] : {std::pair{std::uint16_t{1}, std::uint16_t{1362}}, {std::uint16_t{2}, std::uint16_t{1404}}}) {
            const auto link = makeDetailContainer(epoch, 19, layer, {static_cast<std::int16_t>(frame), 0, 0, 19, 0}, "LL");
            const auto& bytes = link.blocks.front().data;
            parsed.blocks.front().data.insert(parsed.blocks.front().data.end(), bytes.begin(), bytes.end());
        }
        parsed.blocks.front().info.decodedSize = parsed.blocks.front().data.size();
        const auto [document, report] = importSynthetic(parsed);
        const auto hold = document->getDetails()->get<GFrameHoldTarget>(musx::dom::SCORE_PARTID, 1, 1);
        REQUIRE(hold);
        CHECK(hold->frames == std::vector<musx::dom::Cmper>{1013, 1362, 1404, 0});
        const auto key = gframeKey(musx::dom::Cmper{1});
        REQUIRE(report.fields.contains(key));
        CHECK(report.fields.at(key).at("frame2").origin == ValueOrigin::LegacyMus);
        CHECK(report.fields.at(key).at("frame3").rawValue == 1404);
    }
}

TEST_CASE("GFrameHold drops frames past the score's last measure", "[class][gframe-hold]")
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        CAPTURE(epoch);
        auto parsed = makeDetailContainer(epoch, 1, 1, {0, 0, 0, 0, 0}, "GF");
        const auto beyond = makeDetailContainer(epoch, 1, 2, {0, 0, 0, 0, 0}, "GF");
        const auto& bytes = beyond.blocks.front().data;
        parsed.blocks.front().data.insert(parsed.blocks.front().data.end(), bytes.begin(), bytes.end());
        parsed.blocks.front().info.decodedSize = parsed.blocks.front().data.size();
        const auto [document, report] = importSynthetic(parsed);
        CHECK(document->getDetails()->get<GFrameHoldTarget>(musx::dom::SCORE_PARTID, 1, 1));
        CHECK_FALSE(document->getDetails()->get<GFrameHoldTarget>(musx::dom::SCORE_PARTID, 1, 2));
        CHECK_FALSE(report.fields.contains(gframeKey(musx::dom::Cmper{2})));
    }
}

TEST_CASE("GFrameHold decodes the three clef display states without guessing the fourth", "[class][gframe-hold]")
{
    constexpr std::array flags{std::uint16_t{0}, std::uint16_t{0x0010}, std::uint16_t{0x0020}, std::uint16_t{0x0030}};
    constexpr std::array modes{
        musx::dom::ShowClefMode::WhenNeeded, musx::dom::ShowClefMode::Never, musx::dom::ShowClefMode::Always, musx::dom::ShowClefMode::WhenNeeded};
    for (std::size_t i = 0; i < flags.size(); ++i) {
        const auto [document, report] = importFlags(flags[i] | 0x0001);
        const auto hold = document->getDetails()->get<GFrameHoldTarget>(musx::dom::SCORE_PARTID, 1, 1);
        REQUIRE(hold);
        CHECK(hold->clefId == 2);
        CHECK(hold->showClefMode == modes[i]);
        CHECK(hold->clefAfterBarline);
        const auto key = gframeKey(musx::dom::Cmper{1});
        REQUIRE(report.fields.contains(key));
        CHECK(report.fields.at(key).at("showClefMode").origin == (i == 3 ? ValueOrigin::Unmapped : ValueOrigin::LegacyMus));
    }
}

TEST_CASE("GFrameHold preserves a stored clef list ID without its referent", "[class][gframe-hold]")
{
    constexpr musx::dom::Cmper listId{47};
    const auto result = importSynthetic(makeDetailContainer(FormatEpoch::CodaBanner, 1, 1,
        {0, static_cast<std::int16_t>(listId), 0, 0, static_cast<std::int16_t>(finale_mus_reader::details::gframe::clefListBit)}, "GF"));
    CHECK_FALSE(result.first->getOthers()->get<musx::dom::others::ClefList>(musx::dom::SCORE_PARTID, listId, musx::dom::Inci{0}));
    const auto hold = result.first->getDetails()->get<GFrameHoldTarget>(musx::dom::SCORE_PARTID, 1, 1);
    REQUIRE(hold);
    CHECK(hold->clefListId == listId);
    CHECK_FALSE(hold->clefId.has_value());
    const auto key = gframeKey(musx::dom::Cmper{1});
    REQUIRE(result.second.fields.contains(key));
    CHECK(result.second.fields.at(key).at("clefListId").origin == ValueOrigin::LegacyMus);
    CHECK(result.second.fields.at(key).at("clefListId").rawValue == listId);
}

TEST_CASE("GFrameHold leaves a zero clef list comparator unresolved", "[class][gframe-hold]")
{
    const auto result = importSynthetic(makeDetailContainer(
        FormatEpoch::CodaBanner, 1, 1, {0, 0, 0, 0, static_cast<std::int16_t>(finale_mus_reader::details::gframe::clefListBit)}, "GF"));
    const auto hold = result.first->getDetails()->get<GFrameHoldTarget>(musx::dom::SCORE_PARTID, 1, 1);
    REQUIRE(hold);
    CHECK(hold->clefListId == 0);
    CHECK_FALSE(hold->clefId.has_value());
    const auto key = gframeKey(musx::dom::Cmper{1});
    REQUIRE(result.second.fields.contains(key));
    CHECK(result.second.fields.at(key).at("clefId").origin == ValueOrigin::Unmapped);
    CHECK(result.second.fields.at(key).at("clefListId").origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Zlib GFrameHold carries its mirror flag", "[class][gframe-hold]")
{
    const auto [document, report] = importFlags(0x0040);
    const auto hold = document->getDetails()->get<GFrameHoldTarget>(musx::dom::SCORE_PARTID, 1, 1);
    REQUIRE(hold);
    CHECK(hold->mirrorFrame);
    const auto key = gframeKey(musx::dom::Cmper{1});
    REQUIRE(report.fields.contains(key));
    CHECK(report.fields.at(key).at("mirrorFrame").origin == ValueOrigin::LegacyMus);
    CHECK(report.fields.at(key).at("mirrorFrame").rawValue == 0x0040);
}

TEST_CASE("Fixed-row GFrameHold layouts carry the mirror flag", "[class][gframe-hold]")
{
    for (const auto& parsed : {makeDetailContainer(FormatEpoch::CodaBanner, 1, 1, {0, 0, 0, 0, 0x0040}, "GF"),
             makeDetailContainer(FormatEpoch::UncompressedLegacy, 1, 1, {0, 0, 0, 0, 0x0040}, "GF"),
             makeDetailContainer(FormatEpoch::DclLegacy, 1, 1, {0, 0x0040, 0, 0, 0, 0, 0}, "GF")}) {
        const auto [document, report] = importSynthetic(parsed);
        const auto hold = document->getDetails()->get<GFrameHoldTarget>(musx::dom::SCORE_PARTID, 1, 1);
        REQUIRE(hold);
        CHECK(hold->mirrorFrame);
        const auto key = gframeKey(musx::dom::Cmper{1});
        REQUIRE(report.fields.contains(key));
        CHECK(report.fields.at(key).at("mirrorFrame").origin == ValueOrigin::LegacyMus);
        CHECK(report.fields.at(key).at("mirrorFrame").rawValue == 0x0040);
    }
}

TEST_CASE("GFrameHold imports the four frame slots and clef percent", "[class][gframe-hold]")
{
    const auto before = readFixture("evidence/F2000/F2000-multilayer.mus");
    const auto first = before.document->getDetails()->get<GFrameHoldTarget>(musx::dom::SCORE_PARTID, 1, 1);
    const auto second = before.document->getDetails()->get<GFrameHoldTarget>(musx::dom::SCORE_PARTID, 1, 2);
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first->frames == std::vector<musx::dom::Cmper>{1, 0, 0, 0});
    CHECK(second->frames == std::vector<musx::dom::Cmper>{0, 2, 0, 0});
    CHECK(first->clefId == 0);
    CHECK(first->clefPercent == 75);
    const auto earlyKey = gframeKey(musx::dom::Cmper{1});
    REQUIRE(before.report.fields.contains(earlyKey));
    CHECK(before.report.fields.at(earlyKey).at("clefPercent").origin == ValueOrigin::LegacyMus);
    CHECK(before.report.fields.at(earlyKey).at("clefPercent").rawValue == 75);

    const auto after = readFixture("evidence/F2004/F2004-baseline.mus");
    const auto modern = after.document->getDetails()->get<GFrameHoldTarget>(musx::dom::SCORE_PARTID, 1, 1);
    REQUIRE(modern);
    CHECK(modern->clefId == 0);
    CHECK(modern->clefPercent == 75);
    CHECK(modern->frames == std::vector<musx::dom::Cmper>{1, 0, 0, 0});
    const auto key = gframeKey(musx::dom::Cmper{1});
    REQUIRE(after.report.fields.contains(key));
    const auto& reported = after.report.fields.at(key);
    CHECK(GFrameHoldTarget::xmlMappingArray().size() == 10);
    CHECK(reported.size() == 10);
    CHECK(reported.at("clefPercent").origin == ValueOrigin::LegacyMus);
    CHECK(reported.at("clefPercent").rawValue == 75);
    CHECK(reported.at("mirrorFrame").origin == ValueOrigin::LegacyMus);
    CHECK(reported.at("frame4").origin == ValueOrigin::LegacyMus);
}

TEST_CASE("GFrameHold reads a pre-2004 clef percentage from the second row", "[class][gframe-hold]")
{
    const auto early = readFixture("evidence/F98/F98-altnotation.mus");
    const auto earlyHold = early.document->getDetails()->get<GFrameHoldTarget>(musx::dom::SCORE_PARTID, 1, 2);
    REQUIRE(earlyHold);
    CHECK(earlyHold->clefPercent == 75);
    const auto earlyKey = gframeKey(musx::dom::Cmper{2});
    REQUIRE(early.report.fields.contains(earlyKey));
    CHECK(early.report.fields.at(earlyKey).at("clefPercent").origin == ValueOrigin::LegacyBehavior);

    const auto edited = readFixture("evidence/F2001/F2001Win-clef87.mus");
    const auto hold = edited.document->getDetails()->get<GFrameHoldTarget>(musx::dom::SCORE_PARTID, 1, 1);
    REQUIRE(hold);
    CHECK(hold->clefId == 3);
    CHECK(hold->clefPercent == 87);
    const auto key = gframeKey(musx::dom::Cmper{1});
    REQUIRE(edited.report.fields.contains(key));
    CHECK(edited.report.fields.at(key).at("clefPercent").origin == ValueOrigin::LegacyMus);
    CHECK(edited.report.fields.at(key).at("clefPercent").rawValue == 87);
}

TEST_CASE("GFrameHold inherits the clef change percentage when its own percentage is absent or zero", "[class][gframe-hold]")
{
    const auto fixture = readFixture("evidence/F2002/F2002-defclef73.mus");
    const auto fixtureOptions = fixture.document->getOptions()->get<musx::dom::options::ClefOptions>();
    REQUIRE(fixtureOptions);
    REQUIRE(fixtureOptions->clefChangePercent == 73);
    const auto fixtureHold = fixture.document->getDetails()->get<GFrameHoldTarget>(musx::dom::SCORE_PARTID, 1, 1);
    REQUIRE(fixtureHold);
    CHECK(fixtureHold->clefPercent == 73);
    const auto fixtureKey = gframeKey(musx::dom::Cmper{1});
    CHECK(fixture.report.fields.at(fixtureKey).at("clefPercent").origin == ValueOrigin::LegacyMus);

    const auto early = importSynthetic(makeDetailContainer(FormatEpoch::UncompressedLegacy, 1, 1, {0, 0, 0, 0, 0}, "GF"), 73);
    const auto earlyHold = early.first->getDetails()->get<GFrameHoldTarget>(musx::dom::SCORE_PARTID, 1, 1);
    REQUIRE(earlyHold);
    CHECK(earlyHold->clefPercent == 73);
    const auto earlyKey = gframeKey(musx::dom::Cmper{1});
    CHECK(early.second.fields.at(earlyKey).at("clefPercent").origin == ValueOrigin::LegacyBehavior);

    const auto modern =
        importSynthetic(makeDetailClassContainer(1, 1, musx::dom::SCORE_PARTID, {0, 0, 0, 1, 0, 0, 0}, ByteOrder::BigEndian, 0x0414), 73);
    const auto modernHold = modern.first->getDetails()->get<GFrameHoldTarget>(musx::dom::SCORE_PARTID, 1, 1);
    REQUIRE(modernHold);
    CHECK(modernHold->clefPercent == 73);
    const auto modernKey = gframeKey(musx::dom::Cmper{1});
    CHECK(modern.second.fields.at(modernKey).at("clefPercent").origin == ValueOrigin::LegacyBehavior);
}

TEST_CASE("GFrameHold retains the early single frame slot", "[class][gframe-hold]")
{
    const auto result = readFixture("evidence/F263/F263-altnotation.mus");
    const auto hold = result.document->getDetails()->get<GFrameHoldTarget>(musx::dom::SCORE_PARTID, 1, 3);
    REQUIRE(hold);
    CHECK(hold->frames[0] == 1);
    const auto key = gframeKey(musx::dom::Cmper{3});
    REQUIRE(result.report.fields.contains(key));
    CHECK(result.report.fields.at(key).at("frame2").origin == ValueOrigin::LegacyBehavior);
}

} // namespace
} // namespace finale_mus_reader_tests
