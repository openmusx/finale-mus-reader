// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

using CategoryParts = musx::dom::others::StaffListCategoryParts;
using CategoryScore = musx::dom::others::StaffListCategoryScore;
using RepeatName = musx::dom::others::StaffListRepeatName;
using StaffListCategoryName = musx::dom::others::StaffListCategoryName;
using RepeatParts = musx::dom::others::StaffListRepeatParts;
using RepeatPartsForced = musx::dom::others::StaffListRepeatPartsForced;
using RepeatScore = musx::dom::others::StaffListRepeatScore;
using RepeatScoreForced = musx::dom::others::StaffListRepeatScoreForced;

musx::dom::DocumentPtr emptyDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    return session.getDocument();
}

musx::dom::DocumentPtr categoryBaseline()
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    for (musx::dom::Cmper cmper = 1; cmper <= 8; ++cmper) {
        auto parts = std::make_shared<CategoryParts>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, cmper);
        parts->values = {-1};
        document->getOthers()->add(CategoryParts::XmlNodeName, parts);
        auto score = std::make_shared<CategoryScore>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, cmper);
        score->values = {-1};
        document->getOthers()->add(CategoryScore::XmlNodeName, score);
    }
    return std::move(session).finish();
}

musx::dom::DocumentPtr repeatBaseline()
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto name = std::make_shared<RepeatName>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper(1));
    name->name = "Baseline repeat list";
    document->getOthers()->add(RepeatName::XmlNodeName, name);
    return std::move(session).finish();
}

ImportReport importStaffLists(const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile,
    const musx::dom::DocumentPtr& document, const musx::dom::DocumentPtr& reference = emptyDocument())
{
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importStaffLists(context);
    return report;
}

TEST_CASE("Finale 2009 category staff lists and platform names recover", "[class]")
{
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        const auto nameWords = byteOrder == ByteOrder::BigEndian ? std::vector<std::int16_t>{static_cast<std::int16_t>(0x8041), 0, 0, 0, 0, 0}
                                                                 : std::vector<std::int16_t>{0x4180, 0, 0, 0, 0, 0};
        const auto parsed = makeClassContainer(
            {
                SyntheticClassRow{0x012f, nameWords, 3},
                SyntheticClassRow{0x0130, {-1, 2, 7, 0, 0, 0}, 3},
                SyntheticClassRow{0x0132, {-2, 9, 0, 0, 0, 0}, 3},
            },
            byteOrder);
        auto profile = profileFor(finale_mus_reader::versions::finale2009.major);
        profile.epoch = FormatEpoch::ZlibLegacy;
        profile.byteOrder = byteOrder;
        profile.platform = finale_mus_reader::SourcePlatform::Windows;
        const auto document = emptyDocument();
        const auto report = importStaffLists(parsed, profile, document);

        const auto name = document->getOthers()->get<StaffListCategoryName>(musx::dom::SCORE_PARTID, 3);
        const auto score = document->getOthers()->get<CategoryScore>(musx::dom::SCORE_PARTID, 3);
        const auto parts = document->getOthers()->get<CategoryParts>(musx::dom::SCORE_PARTID, 3);
        REQUIRE(name);
        CHECK(name->name == "€A");
        REQUIRE(score);
        CHECK(score->values == std::vector<musx::dom::StaffCmper>{-2, 9});
        REQUIRE(parts);
        CHECK(parts->values == std::vector<musx::dom::StaffCmper>{-1, 2, 7});
        CHECK(reportedFieldCount(report) == 5);
        const auto* nameOrigin =
            report.findInstanceOrigin(finale_mus_reader::instanceKey<StaffListCategoryName>(musx::dom::SCORE_PARTID, musx::dom::Cmper(3)));
        REQUIRE(nameOrigin);
        CHECK(*nameOrigin == ValueOrigin::LegacyMus);
    }
}

TEST_CASE("Finale 2012 category names remain one-byte platform text", "[class]")
{
    auto profile = profileFor(finale_mus_reader::versions::finale2012.major);
    profile.epoch = FormatEpoch::ZlibLegacy;
    profile.byteOrder = ByteOrder::LittleEndian;
    profile.platform = finale_mus_reader::SourcePlatform::Windows;
    const auto document = emptyDocument();
    const auto report =
        importStaffLists(makeClassContainer(0x012f, {0x4180, 0, 0, 0, 0, 0}, ByteOrder::LittleEndian, 4), profile, document, categoryBaseline());
    const auto name = document->getOthers()->get<StaffListCategoryName>(musx::dom::SCORE_PARTID, 4);
    REQUIRE(name);
    CHECK(name->name == "€A");
    CHECK_FALSE(document->getOthers()->get<CategoryParts>(musx::dom::SCORE_PARTID, 4));
    CHECK_FALSE(document->getOthers()->get<CategoryScore>(musx::dom::SCORE_PARTID, 4));
    CHECK(document->getOthers()->get<CategoryParts>(musx::dom::SCORE_PARTID, 3));
    CHECK(document->getOthers()->get<CategoryScore>(musx::dom::SCORE_PARTID, 5));
    CHECK(report.findInstanceOrigin(finale_mus_reader::instanceKey<StaffListCategoryName>(musx::dom::SCORE_PARTID, musx::dom::Cmper(4))) != nullptr);
}

TEST_CASE("Pre-Finale 2009 files receive the pinned category staff lists", "[class]")
{
    const auto result = readFixture("evidence/F100/F100-baseline.mus");
    const auto parts = result.document->getOthers()->getArray<CategoryParts>(musx::dom::SCORE_PARTID);
    const auto score = result.document->getOthers()->getArray<CategoryScore>(musx::dom::SCORE_PARTID);
    CHECK(result.document->getOthers()->getArray<StaffListCategoryName>(musx::dom::SCORE_PARTID).empty());
    REQUIRE(parts.size() == 8);
    REQUIRE(score.size() == 8);
    for (std::size_t index = 0; index < 8; ++index) {
        CHECK(parts[index]->getCmper() == static_cast<musx::dom::Cmper>(index + 1));
        CHECK(parts[index]->values == std::vector<musx::dom::StaffCmper>{-1});
        CHECK(score[index]->getCmper() == static_cast<musx::dom::Cmper>(index + 1));
        CHECK(score[index]->values == std::vector<musx::dom::StaffCmper>{-1});
        const auto* partsOrigin = result.report.findInstanceOrigin(
            finale_mus_reader::instanceKey<CategoryParts>(musx::dom::SCORE_PARTID, static_cast<musx::dom::Cmper>(index + 1)));
        const auto* scoreOrigin = result.report.findInstanceOrigin(
            finale_mus_reader::instanceKey<CategoryScore>(musx::dom::SCORE_PARTID, static_cast<musx::dom::Cmper>(index + 1)));
        REQUIRE(partsOrigin);
        REQUIRE(scoreOrigin);
        CHECK(*partsOrigin == ValueOrigin::Finale27Default);
        CHECK(*scoreOrigin == ValueOrigin::Finale27Default);
    }
}

TEST_CASE("Finale 2009 category lists are filled from four through eight", "[class]")
{
    auto profile = profileFor(finale_mus_reader::versions::finale2009.major);
    profile.epoch = FormatEpoch::ZlibLegacy;
    profile.byteOrder = ByteOrder::LittleEndian;
    const auto document = emptyDocument();
    std::vector<SyntheticClassRow> rows;
    for (musx::dom::Cmper cmper = 1; cmper <= 4; ++cmper) {
        rows.emplace_back(std::uint16_t(0x0130), std::vector<std::int16_t>{static_cast<std::int16_t>(cmper), 0, 0, 0, 0, 0}, cmper);
        rows.emplace_back(std::uint16_t(0x0132), std::vector<std::int16_t>{-2, 0, 0, 0, 0, 0}, cmper);
    }
    const auto report = importStaffLists(makeClassContainer(rows, ByteOrder::LittleEndian), profile, document, categoryBaseline());
    CHECK(document->getOthers()->getAllSources<StaffListCategoryName>().empty());
    const auto parts = document->getOthers()->getArray<CategoryParts>(musx::dom::SCORE_PARTID);
    const auto score = document->getOthers()->getArray<CategoryScore>(musx::dom::SCORE_PARTID);
    REQUIRE(parts.size() == 8);
    REQUIRE(score.size() == 8);
    for (std::size_t index = 0; index < 4; ++index) {
        CHECK(score[index]->values == std::vector<musx::dom::StaffCmper>{-2});
        CHECK(parts[index]->values == std::vector<musx::dom::StaffCmper>{static_cast<musx::dom::StaffCmper>(index + 1)});
    }
    for (std::size_t index = 4; index < 8; ++index) {
        CHECK(parts[index]->values == std::vector<musx::dom::StaffCmper>{-1});
        CHECK(score[index]->values == std::vector<musx::dom::StaffCmper>{-1});
        const auto* origin = report.findInstanceOrigin(
            finale_mus_reader::instanceKey<CategoryScore>(musx::dom::SCORE_PARTID, static_cast<musx::dom::Cmper>(index + 1)));
        REQUIRE(origin);
        CHECK(*origin == ValueOrigin::Finale27Default);
    }
    CHECK(reportedFieldCount(report) == 8);
}

TEST_CASE("Category staff-list overrides are reported and ignored", "[class]")
{
    auto profile = profileFor(finale_mus_reader::versions::finale2009.major);
    profile.epoch = FormatEpoch::ZlibLegacy;
    profile.byteOrder = ByteOrder::LittleEndian;
    const auto document = emptyDocument();
    const auto report = importStaffLists(makeClassContainer(
                                             {
                                                 SyntheticClassRow{0x0131, {-1, 0, 0, 0, 0, 0}, 2},
                                                 SyntheticClassRow{0x0133, {-2, 0, 0, 0, 0, 0}, 5, 3},
                                             },
                                             ByteOrder::LittleEndian),
        profile, document);
    CHECK(document->getOthers()->getAllSources<StaffListCategoryName>().empty());
    CHECK(document->getOthers()->getAllSources<CategoryParts>().empty());
    CHECK(document->getOthers()->getAllSources<CategoryScore>().empty());
    REQUIRE(report.diagnostics.size() == 2);
    CHECK(report.diagnostics[0].level == musx::util::Logger::LogLevel::Info);
    CHECK(report.diagnostics[0].message.find("score staff-list override 2") != std::string::npos);
    CHECK(report.diagnostics[1].level == musx::util::Logger::LogLevel::Info);
    CHECK(report.diagnostics[1].message.find("parts staff-list override 5 for part 3") != std::string::npos);
}

TEST_CASE("Fixed-row repeat staff lists and forced arrays recover", "[class]")
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        auto profile = profileFor(finale_mus_reader::versions::finale2005.major);
        profile.epoch = epoch;
        profile.byteOrder = ByteOrder::BigEndian;
        profile.platform = finale_mus_reader::SourcePlatform::MacOS;
        const auto document = emptyDocument();
        const auto report = importStaffLists(makeContainer(
                                                 {
                                                     SyntheticRow{7, "Dc", {0x5265, 0x7065, 0x6174, 0x2041, 0, 0}},
                                                     SyntheticRow{7, "DC", {-1, 2, 0, 0, 0, 0}},
                                                     SyntheticRow{7, "dc", {-2, 3, 0, 0, 0, 0}},
                                                     SyntheticRow{7, "dc", {4, 5, 0, 0, 0, 0}},
                                                     SyntheticRow{7, "IO", {-1, 0, 0, 0, 0, 0}},
                                                     SyntheticRow{7, "io", {3, 0, 0, 0, 0, 0}},
                                                 },
                                                 epoch),
            profile, document);
        const auto name = document->getOthers()->get<RepeatName>(musx::dom::SCORE_PARTID, 7);
        const auto score = document->getOthers()->get<RepeatScore>(musx::dom::SCORE_PARTID, 7);
        const auto parts = document->getOthers()->get<RepeatParts>(musx::dom::SCORE_PARTID, 7);
        const auto scoreForced = document->getOthers()->get<RepeatScoreForced>(musx::dom::SCORE_PARTID, 7);
        const auto partsForced = document->getOthers()->get<RepeatPartsForced>(musx::dom::SCORE_PARTID, 7);
        REQUIRE(name);
        CHECK(name->name == "Repeat A");
        REQUIRE(score);
        CHECK(score->values == std::vector<musx::dom::StaffCmper>{-1, 2});
        REQUIRE(parts);
        CHECK(parts->values == std::vector<musx::dom::StaffCmper>{-2, 3, 4, 5});
        REQUIRE(scoreForced);
        CHECK(scoreForced->values == std::vector<musx::dom::StaffCmper>{-1});
        REQUIRE(partsForced);
        CHECK(partsForced->values == std::vector<musx::dom::StaffCmper>{3});
        CHECK(reportedFieldCount(report) == 8);
    }
}

TEST_CASE("Class-record repeat staff lists retain part scope", "[class]")
{
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        auto profile = profileFor(finale_mus_reader::versions::finale2012.major);
        profile.epoch = FormatEpoch::ZlibLegacy;
        profile.byteOrder = byteOrder;
        profile.platform = finale_mus_reader::SourcePlatform::Windows;
        const auto document = emptyDocument();
        const auto nameWords =
            byteOrder == ByteOrder::BigEndian ? std::vector<std::int16_t>{0x5769, 0x6465, 0} : std::vector<std::int16_t>{0x6957, 0x6564, 0};
        const auto report = importStaffLists(makeClassContainer(
                                                 {
                                                     SyntheticClassRow{0x00e1, nameWords, 11, 3},
                                                     SyntheticClassRow{0x00e4, {-1, -2, 0}, 11, 3},
                                                     SyntheticClassRow{0x00e2, {-3, 4, 0, 0, 0, 0, 5, 6, 0, 0, 0, 0}, 11, 3},
                                                     SyntheticClassRow{0x00e5, {-2, 0}, 11, 3},
                                                     SyntheticClassRow{0x00e3, {4, 0}, 11, 3},
                                                     SyntheticClassRow{0x00e3, {0, 0}, 12, 3},
                                                 },
                                                 byteOrder),
            profile, document);
        const auto name = document->getOthers()->get<RepeatName>(3, 11);
        const auto score = document->getOthers()->get<RepeatScore>(3, 11);
        const auto parts = document->getOthers()->get<RepeatParts>(3, 11);
        const auto scoreForced = document->getOthers()->get<RepeatScoreForced>(3, 11);
        const auto partsForced = document->getOthers()->get<RepeatPartsForced>(3, 11);
        REQUIRE(name);
        CHECK(name->name == "Wide");
        REQUIRE(score);
        CHECK(score->values == std::vector<musx::dom::StaffCmper>{-1, -2});
        REQUIRE(parts);
        CHECK(parts->values == std::vector<musx::dom::StaffCmper>{-3, 4, 5, 6});
        REQUIRE(scoreForced);
        CHECK(scoreForced->values == std::vector<musx::dom::StaffCmper>{-2});
        REQUIRE(partsForced);
        CHECK(partsForced->values == std::vector<musx::dom::StaffCmper>{4});
        CHECK_FALSE(document->getOthers()->get<RepeatPartsForced>(3, 12));
        CHECK(document->getOthers()->getAllSources<RepeatName>().size() == 1);
        CHECK(reportedFieldCount(report) == 8);
    }
}

TEST_CASE("Controlled Finale 2005 and 2012 repeat staff lists recover", "[class]")
{
    const auto finale2005 = readFixture("evidence/F2005/F2005-rptopts-stafflist.mus");
    const auto name2005 = finale2005.document->getOthers()->get<RepeatName>(musx::dom::SCORE_PARTID, 1);
    const auto score2005 = finale2005.document->getOthers()->get<RepeatScore>(musx::dom::SCORE_PARTID, 1);
    const auto parts2005 = finale2005.document->getOthers()->get<RepeatParts>(musx::dom::SCORE_PARTID, 1);
    const auto forced2005 = finale2005.document->getOthers()->get<RepeatPartsForced>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(name2005);
    CHECK(name2005->name == "Repeat Staves");
    REQUIRE(score2005);
    CHECK(score2005->values == std::vector<musx::dom::StaffCmper>{-1});
    REQUIRE(parts2005);
    CHECK(parts2005->values == std::vector<musx::dom::StaffCmper>{-3});
    REQUIRE(forced2005);
    CHECK(forced2005->values == std::vector<musx::dom::StaffCmper>{-3});

    const auto finale2012 = readFixture("evidence/F2012/F2012-rptopts-3lists-sel2.mus");
    const auto names2012 = finale2012.document->getOthers()->getArray<RepeatName>(musx::dom::SCORE_PARTID);
    REQUIRE(names2012.size() == 3);
    const auto forced2012 = finale2012.document->getOthers()->get<RepeatScoreForced>(musx::dom::SCORE_PARTID, 2);
    REQUIRE(forced2012);
    CHECK(forced2012->values == std::vector<musx::dom::StaffCmper>{-2});
}

TEST_CASE("Absent repeat staff lists are not copied from the baseline", "[class]")
{
    auto profile = profileFor(finale_mus_reader::versions::finale2012.major);
    profile.epoch = FormatEpoch::ZlibLegacy;
    profile.byteOrder = ByteOrder::LittleEndian;
    const auto document = emptyDocument();
    static_cast<void>(importStaffLists(makeClassContainer({}, ByteOrder::LittleEndian), profile, document, repeatBaseline()));
    CHECK(document->getOthers()->getAllSources<RepeatName>().empty());
    CHECK(document->getOthers()->getAllSources<RepeatParts>().empty());
    CHECK(document->getOthers()->getAllSources<RepeatScore>().empty());
}

} // namespace
} // namespace finale_mus_reader_tests
