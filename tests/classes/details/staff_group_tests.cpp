// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using StaffGroup = musx::dom::details::StaffGroup;

ImportReport importStaffGroups(
    const finale_mus_reader::container::ParsedContainer& parsed, SourceProfile profile, const musx::dom::DocumentPtr& document)
{
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    ImportReport report(profile.epoch);
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importStaffUsed(context);
    finale_mus_reader::details::importStaffGroups(context);
    finale_mus_reader::runDeferredChecks(pending);
    return report;
}

musx::dom::DocumentPtr emptyStaffGroupDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    return session.getDocument();
}

TEST_CASE("StaffGroup recovers the Finale 2011 range layout", "[class][staff-group]")
{
    constexpr std::uint16_t packedFlags = 0x8000 | 0x4000 | 0x2000 | 0x1000 | 0x0800 | (0x0e << 6) | (1 << 3) | 2;
    constexpr std::uint16_t packedAux = 0x8000 | 0x4000 | (2 << 2) | 1;
    const std::vector<std::int16_t> words{
        4, 7, 11, -48, 12, 3, -24, 1, 2, 1, static_cast<std::int16_t>(packedFlags), 12, -20, 4, static_cast<std::int16_t>(packedAux), 44, 2, 9, 0, 0};
    const auto parsed = makeDetailClassContainer(0, 6, 0, words, ByteOrder::LittleEndian, 0x0421);
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    profile.version = SourceVersion{.major = finale_mus_reader::versions::finale2011.major};
    const auto document = emptyStaffGroupDocument();
    const auto report = importStaffGroups(parsed, profile, document);

    const auto group = document->getDetails()->get<StaffGroup>(musx::dom::SCORE_PARTID, musx::dom::BASE_SYSTEM_ID, 6);
    REQUIRE(group);
    REQUIRE(group->bracket);
    CHECK(group->startInst == 4);
    CHECK(group->endInst == 7);
    CHECK(group->startMeas == 2);
    CHECK(group->endMeas == 9);
    CHECK(group->fullNameId == 11);
    CHECK(group->fullNameXadj == -48);
    CHECK(group->fullNameYadj == 12);
    CHECK(group->bracket->style == musx::dom::details::Bracket::BracketStyle::PianoBrace);
    CHECK(group->bracket->horzAdjLeft == -24);
    CHECK(group->bracket->vertAdjTop == 1);
    CHECK(group->bracket->vertAdjBot == 2);
    CHECK(group->bracket->showOnSingleStaff);
    CHECK(group->barlineType == StaffGroup::BarlineType::Custom);
    CHECK(group->fullNameJustify == musx::dom::AlignJustify::Center);
    CHECK(group->abbrvNameJustify == musx::dom::AlignJustify::Right);
    CHECK(group->drawBarlines == StaffGroup::DrawBarlineStyle::Mensurstriche);
    CHECK(group->ownBarline);
    CHECK(group->fullNameIndivPos);
    CHECK(group->abbrvNameIndivPos);
    CHECK(group->hideName);
    CHECK(group->abbrvNameId == 12);
    CHECK(group->abbrvNameXadj == -20);
    CHECK(group->abbrvNameYadj == 4);
    CHECK(group->fullNameAlign == musx::dom::AlignJustify::Right);
    CHECK(group->abbrvNameAlign == musx::dom::AlignJustify::Center);
    CHECK(group->fullNameExpand);
    CHECK(group->abbrvNameExpand);
    CHECK(group->customBarShape == 44);
    CHECK(group->hideStaves == StaffGroup::HideStaves::Normally);

    const auto key =
        finale_mus_reader::instanceKey<StaffGroup>(musx::dom::SCORE_PARTID, musx::dom::BASE_SYSTEM_ID, std::nullopt, musx::dom::Cmper(6));
    REQUIRE(report.fields.contains(key));
    CHECK(report.fields.at(key).size() == 29);
    CHECK(report.fields.at(key).at("startMeas").origin == ValueOrigin::LegacyMus);
    CHECK(report.fields.at(key).at("customBarShape").rawValue == 44);
    CHECK(report.fields.at(key).at("hideStaves").origin == ValueOrigin::LegacyMus);
    CHECK(report.fields.at(key).at("barlineType").rawValue == static_cast<std::int16_t>(packedFlags));
}

TEST_CASE("Finale 2011 StaffGroup preserves both stored comparators", "[class][staff-group]")
{
    constexpr musx::dom::Cmper sourceCmper1 = 9;
    constexpr musx::dom::Cmper sourceCmper2 = 6;
    const std::vector<std::int16_t> words{1, 2, 0, 0, 0, 3, -12, 0, 0, 1, 0x0400, 0, 0, 0, 0, 0, 2, 9, 0, 0};
    const auto parsed = makeDetailClassContainer(sourceCmper1, sourceCmper2, 0, words, ByteOrder::LittleEndian, 0x0421);
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    profile.version = SourceVersion{.major = finale_mus_reader::versions::finale2011.major};
    const auto document = emptyStaffGroupDocument();
    const auto report = importStaffGroups(parsed, profile, document);

    const auto group = document->getDetails()->get<StaffGroup>(musx::dom::SCORE_PARTID, sourceCmper1, sourceCmper2);
    REQUIRE(group);
    CHECK(group->getCmper1() == sourceCmper1);
    CHECK(group->getCmper2() == sourceCmper2);
    const auto key = finale_mus_reader::instanceKey<StaffGroup>(musx::dom::SCORE_PARTID, sourceCmper1, std::nullopt, sourceCmper2);
    CHECK(report.fields.contains(key));
}

TEST_CASE("Pre-Finale-2011 StaffGroup imports only the base system for all measures", "[class][staff-group]")
{
    const std::vector<std::int16_t> words{1, 2, 0, 0, 0, 3, -12, 0, 0, 1, 0x0440, 0, 0, 0, 0};
    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        const auto paddedWords =
            epoch == FormatEpoch::DclLegacy ? std::vector<std::int16_t>{1, 2, 0, 0, 0, 3, -12, 0, 0, 1, 0x0440, 0, 0, 0, 0, 0, 0, 0, 0, 0} : words;
        const auto parsed = makeDetailContainer(epoch, musx::dom::BASE_SYSTEM_ID, 3, paddedWords, "NG", ByteOrder::BigEndian);
        auto profile = SourceProfile(epoch);
        profile.byteOrder = ByteOrder::BigEndian;
        const auto document = emptyStaffGroupDocument();
        const auto report = importStaffGroups(parsed, profile, document);
        const auto group = document->getDetails()->get<StaffGroup>(musx::dom::SCORE_PARTID, musx::dom::BASE_SYSTEM_ID, 3);
        REQUIRE(group);
        CHECK(group->startMeas == 1);
        CHECK(group->endMeas == (std::numeric_limits<musx::dom::MeasCmper>::max)());
        CHECK(group->isAllMeasures());
        const auto key =
            finale_mus_reader::instanceKey<StaffGroup>(musx::dom::SCORE_PARTID, musx::dom::BASE_SYSTEM_ID, std::nullopt, musx::dom::Cmper(3));
        REQUIRE(report.fields.contains(key));
        CHECK(report.fields.at(key).at("startMeas").origin == ValueOrigin::LegacyBehavior);
        CHECK(report.fields.at(key).at("endMeas").origin == ValueOrigin::LegacyBehavior);
        CHECK(
            report.fields.at(key).at("customBarShape").origin == (epoch == FormatEpoch::DclLegacy ? ValueOrigin::LegacyMus : ValueOrigin::Unmapped));
    }
}

TEST_CASE("StaffGroup hide-staves auxiliary flags activate in Finale 2003", "[class][staff-group]")
{
    const auto import = [](finale_mus_reader::VersionBound version, std::uint16_t hideFlags) {
        std::vector<std::int16_t> words{1, 2, 0, 0, 0, 3, -12, 0, 0, 1, 0x0440, 0, 0, 0, static_cast<std::int16_t>(hideFlags), 0, 0, 0, 0, 0};
        const auto parsed = makeDetailContainer(FormatEpoch::DclLegacy, musx::dom::BASE_SYSTEM_ID, 3, words, "NG", ByteOrder::BigEndian);
        auto profile = SourceProfile(FormatEpoch::DclLegacy);
        profile.byteOrder = ByteOrder::BigEndian;
        profile.version = SourceVersion{.major = version.major};
        const auto document = emptyStaffGroupDocument();
        auto report = importStaffGroups(parsed, profile, document);
        return std::pair{document->getDetails()->get<StaffGroup>(musx::dom::SCORE_PARTID, musx::dom::BASE_SYSTEM_ID, 3), std::move(report)};
    };

    const auto [asGroup, asGroupReport] = import(finale_mus_reader::versions::finale2003, 0x0800);
    REQUIRE(asGroup);
    CHECK(asGroup->hideStaves == StaffGroup::HideStaves::AsGroup);
    const auto key =
        finale_mus_reader::instanceKey<StaffGroup>(musx::dom::SCORE_PARTID, musx::dom::BASE_SYSTEM_ID, std::nullopt, musx::dom::Cmper(3));
    CHECK(asGroupReport.fields.at(key).at("hideStaves").origin == ValueOrigin::LegacyMus);

    const auto [never, neverReport] = import(finale_mus_reader::versions::finale2003, 0x1000);
    REQUIRE(never);
    CHECK(never->hideStaves == StaffGroup::HideStaves::None);
    CHECK(neverReport.fields.at(key).at("hideStaves").origin == ValueOrigin::LegacyMus);

    const auto [inactive, inactiveReport] = import(finale_mus_reader::versions::finale2002, 0x0800);
    REQUIRE(inactive);
    CHECK(inactive->hideStaves == StaffGroup::HideStaves::Normally);
    CHECK(inactiveReport.fields.at(key).at("hideStaves").origin == ValueOrigin::Finale27Default);

    const auto [invalid, invalidReport] = import(finale_mus_reader::versions::finale2003, 0x1800);
    REQUIRE(invalid);
    CHECK(invalid->hideStaves == StaffGroup::HideStaves::Normally);
    CHECK(invalidReport.fields.at(key).at("hideStaves").origin == ValueOrigin::LegacyMusAdjusted);
    REQUIRE(invalidReport.diagnostics.size() == 1);
    CHECK(invalidReport.diagnostics.front().level == musx::util::Logger::LogLevel::Info);
    CHECK(invalidReport.diagnostics.front().message.find("both hide-staves flags") != std::string::npos);
}

TEST_CASE("Coda-banner StaffGroup is absent without a GS record", "[class][staff-group][reader]")
{
    const auto result = readFixture("evidence/F263/F263-baseline.mus");
    CHECK(result.document->getDetails()->getAllSources<StaffGroup>().empty());
}

TEST_CASE("GS StaffGroup takes its staff span from the compact Scroll View list in either container", "[class][staff-group]")
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy}) {
        CAPTURE(epoch);
        const auto parsed =
            makeContainer({{0, "IU", {4, 2, -184, 12, 2, -456}}, {0, "IU", {7, 2, -724, 8, 3, -1016}}, {0, "IU", {9, 3, -1108, 0, 0, 0}},
                              {2, "GS", {2, -20, 0, 0, 0, 0}}, {2, "GS", {3, -40, 0, 0, 0, 0}}, {3, "GS", {3, -20, 0, 0, 0, 0}}},
                epoch, ByteOrder::BigEndian);
        const auto document = emptyStaffGroupDocument();
        auto profile = SourceProfile(epoch);
        profile.byteOrder = ByteOrder::BigEndian;
        const auto report = importStaffGroups(parsed, profile, document);

        const auto bracket = document->getDetails()->get<StaffGroup>(musx::dom::SCORE_PARTID, musx::dom::BASE_SYSTEM_ID, 1);
        REQUIRE(bracket);
        CHECK(bracket->startInst == 4);
        CHECK(bracket->endInst == 7);
        const auto nestedBrace = document->getDetails()->get<StaffGroup>(musx::dom::SCORE_PARTID, musx::dom::BASE_SYSTEM_ID, 2);
        REQUIRE(nestedBrace);
        CHECK(nestedBrace->startInst == 4);
        CHECK(nestedBrace->endInst == 7);
        REQUIRE(nestedBrace->bracket);
        CHECK(nestedBrace->bracket->horzAdjLeft == -40);
        const auto brace = document->getDetails()->get<StaffGroup>(musx::dom::SCORE_PARTID, musx::dom::BASE_SYSTEM_ID, 3);
        REQUIRE(brace);
        CHECK(brace->startInst == 8);
        CHECK(brace->endInst == 9);

        const auto key =
            finale_mus_reader::instanceKey<StaffGroup>(musx::dom::SCORE_PARTID, musx::dom::BASE_SYSTEM_ID, std::nullopt, musx::dom::Cmper(1));
        CHECK(report.fields.at(key).at("startInst").origin == ValueOrigin::LegacyMus);
        CHECK(report.fields.at(key).at("startInst").rawValue == 4);
        CHECK(report.fields.at(key).at("endInst").origin == ValueOrigin::LegacyMus);
        CHECK(report.fields.at(key).at("endInst").rawValue == 7);
    }
}

TEST_CASE("GS StaffGroup recovers the curved-hook bracket", "[class][staff-group]")
{
    const auto parsed = makeContainer(
        {{0, "IU", {15, 1, -184, 19, 1, -456}}, {1, "GS", {6, -20, 0, 0, 0, 0}}}, FormatEpoch::UncompressedLegacy, ByteOrder::BigEndian);
    const auto document = emptyStaffGroupDocument();
    auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    const auto report = importStaffGroups(parsed, profile, document);

    const auto group = document->getDetails()->get<StaffGroup>(musx::dom::SCORE_PARTID, musx::dom::BASE_SYSTEM_ID, 1);
    REQUIRE(group);
    REQUIRE(group->bracket);
    CHECK(group->bracket->style == musx::dom::details::Bracket::BracketStyle::BracketCurvedHooks);
    CHECK(group->startInst == 15);
    CHECK(group->endInst == 19);
    CHECK(report.diagnostics.empty());
    const auto key =
        finale_mus_reader::instanceKey<StaffGroup>(musx::dom::SCORE_PARTID, musx::dom::BASE_SYSTEM_ID, std::nullopt, musx::dom::Cmper(1));
    CHECK(report.fields.at(key).at("bracket.style").origin == ValueOrigin::LegacyMus);
    CHECK(report.fields.at(key).at("bracket.style").rawValue == 6);
}

TEST_CASE("Controlled Finale 2.6.3 StaffGroups recover the three Coda bracket styles", "[class][staff-group][reader]")
{
    struct Sample
    {
        const char* path;
        std::int16_t rawStyle;
        musx::dom::details::Bracket::BracketStyle style;
    };
    constexpr Sample samples[]{
        {"evidence/F263/F263-bracket-thickbar.mus", 1, musx::dom::details::Bracket::BracketStyle::ThickLine},
        {"evidence/F263/F263-bracket.mus", 2, musx::dom::details::Bracket::BracketStyle::BracketStraightHooks},
        {"evidence/F263/F263-bracket-brace.mus", 3, musx::dom::details::Bracket::BracketStyle::PianoBrace},
    };

    for (const auto& sample : samples) {
        CAPTURE(sample.path);
        const auto result = readFixture(sample.path);
        const auto group = result.document->getDetails()->get<StaffGroup>(musx::dom::SCORE_PARTID, musx::dom::BASE_SYSTEM_ID, 1);
        REQUIRE(group);
        REQUIRE(group->bracket);
        CHECK(group->startInst == 1);
        CHECK(group->endInst == 1);
        CHECK(group->startMeas == 1);
        CHECK(group->endMeas == (std::numeric_limits<musx::dom::MeasCmper>::max)());
        CHECK(group->bracket->style == sample.style);
        CHECK(group->bracket->horzAdjLeft == -20);
        CHECK(group->bracket->vertAdjTop == 24);
        CHECK(group->bracket->vertAdjBot == -304);
        CHECK(group->bracket->showOnSingleStaff);
        CHECK(group->barlineType == StaffGroup::BarlineType::Normal);
        CHECK(group->drawBarlines == StaffGroup::DrawBarlineStyle::ThroughStaves);

        const auto key =
            finale_mus_reader::instanceKey<StaffGroup>(musx::dom::SCORE_PARTID, musx::dom::BASE_SYSTEM_ID, std::nullopt, musx::dom::Cmper(1));
        REQUIRE(result.report.fields.contains(key));
        CHECK(result.report.fields.at(key).size() == 29);
        CHECK(result.report.fields.at(key).at("bracket.style").origin == ValueOrigin::LegacyMus);
        CHECK(result.report.fields.at(key).at("bracket.style").rawValue == sample.rawStyle);
        CHECK(result.report.fields.at(key).at("startInst").origin == ValueOrigin::LegacyMus);
        CHECK(result.report.fields.at(key).at("endInst").origin == ValueOrigin::LegacyMus);
        CHECK(result.report.fields.at(key).at("customBarShape").origin == ValueOrigin::Unmapped);
    }
}

TEST_CASE("Controlled Finale 2012 StaffGroup has the all-measures range", "[class][staff-group][reader]")
{
    const auto result = readFixture("evidence/F2012/F2012-piano.mus");
    const auto group = result.document->getDetails()->get<StaffGroup>(musx::dom::SCORE_PARTID, musx::dom::BASE_SYSTEM_ID, 1);
    REQUIRE(group);
    CHECK(group->startInst == 1);
    CHECK(group->endInst == 2);
    CHECK(group->startMeas == 1);
    CHECK(group->endMeas == (std::numeric_limits<musx::dom::MeasCmper>::max)());
    CHECK(group->isAllMeasures());
    REQUIRE(group->bracket);
    CHECK(group->bracket->style == musx::dom::details::Bracket::BracketStyle::PianoBrace);
    CHECK(group->customBarShape == 0);
    CHECK(group->hideStaves == StaffGroup::HideStaves::AsGroup);
}

TEST_CASE("Controlled Finale 2000 StaffGroup recovers a custom barline shape", "[class][staff-group][reader]")
{
    const auto result = readFixture("evidence/F2000/F2000-group-custbar.mus");
    const auto group = result.document->getDetails()->get<StaffGroup>(musx::dom::SCORE_PARTID, musx::dom::BASE_SYSTEM_ID, 1);
    REQUIRE(group);
    CHECK(group->customBarShape == 2);
    CHECK(group->barlineType == StaffGroup::BarlineType::Custom);
    CHECK(group->startMeas == 1);
    CHECK(group->endMeas == (std::numeric_limits<musx::dom::MeasCmper>::max)());

    const auto key =
        finale_mus_reader::instanceKey<StaffGroup>(musx::dom::SCORE_PARTID, musx::dom::BASE_SYSTEM_ID, std::nullopt, musx::dom::Cmper(1));
    REQUIRE(result.report.fields.contains(key));
    CHECK(result.report.fields.at(key).at("customBarShape").origin == ValueOrigin::LegacyMus);
    CHECK(result.report.fields.at(key).at("customBarShape").rawValue == 2);
}

} // namespace
} // namespace finale_mus_reader_tests
