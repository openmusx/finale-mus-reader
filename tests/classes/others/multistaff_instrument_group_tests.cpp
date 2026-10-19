// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <cstdint>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

using MultiStaffGroupIdTarget = musx::dom::others::MultiStaffGroupId;
using MultiStaffInstrumentGroupTarget = musx::dom::others::MultiStaffInstrumentGroup;

constexpr std::uint8_t finale2012Major = 17;
constexpr std::uint16_t multiStaffInstrumentGroupClass = 0x0142;
constexpr std::uint16_t multiStaffGroupIdClass = 0x0143;

ImportReport importMultiStaffRecords(
    const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile, const musx::dom::DocumentPtr& document)
{
    ImportReport report(profile.epoch);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{
        LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importMultiStaffInstrumentGroups(context);
    finale_mus_reader::others::importMultiStaffGroupIds(context);
    return report;
}

TEST_CASE("Multi-staff class records split staff membership from the staff-group reference", "[class]")
{
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        auto profile = profileFor(finale2012Major);
        profile.epoch = FormatEpoch::ZlibLegacy;
        profile.byteOrder = byteOrder;
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        constexpr musx::dom::Cmper objectCmper = 7;
        const auto parsed = makeClassContainer(
            {{multiStaffInstrumentGroupClass, {5, 8, 13, 0, 0, 0}, objectCmper}, {multiStaffGroupIdClass, {21, 0, 0, 0, 0, 0}, objectCmper}},
            byteOrder);

        const auto report = importMultiStaffRecords(parsed, profile, document);
        const auto instrument = document->getOthers()->get<MultiStaffInstrumentGroupTarget>(musx::dom::SCORE_PARTID, objectCmper);
        const auto groupId = document->getOthers()->get<MultiStaffGroupIdTarget>(musx::dom::SCORE_PARTID, objectCmper);

        REQUIRE(instrument);
        CHECK(instrument->staffNums == std::vector<musx::dom::StaffCmper>{5, 8, 13});
        REQUIRE(groupId);
        CHECK(groupId->staffGroupId == 21);
        CHECK(reportedFieldCount(report) == 4);
        CHECK(fieldFor<MultiStaffInstrumentGroupTarget>(report, "others.multiStaffInstGroup[7].staffNums[0]").origin == ValueOrigin::LegacyMus);
        CHECK(fieldFor<MultiStaffInstrumentGroupTarget>(report, "others.multiStaffInstGroup[7].staffNums[2]").rawValue == 13);
        CHECK(fieldFor<MultiStaffGroupIdTarget>(report, "others.multiStaffGroupID[7].staffGroupId").rawValue == 21);
    }
}

TEST_CASE("Multi-staff records omit empty staff slots and retain part-specific group ids", "[class]")
{
    auto profile = profileFor(finale2012Major);
    profile.epoch = FormatEpoch::ZlibLegacy;
    profile.byteOrder = ByteOrder::LittleEndian;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    constexpr musx::dom::Cmper objectCmper = 1;
    constexpr musx::dom::Cmper partId = 2;
    const auto parsed = makeClassContainer(
        {{multiStaffInstrumentGroupClass, {1, 2, 0, 0, 0, 0}, objectCmper}, {multiStaffGroupIdClass, {4, 0, 0, 0, 0, 0}, objectCmper},
            {multiStaffGroupIdClass, {9, 0, 0, 0, 0, 0}, objectCmper, partId}},
        ByteOrder::LittleEndian);

    const auto report = importMultiStaffRecords(parsed, profile, document);
    const auto instrument = document->getOthers()->get<MultiStaffInstrumentGroupTarget>(musx::dom::SCORE_PARTID, objectCmper);
    const auto scoreGroup = document->getOthers()->get<MultiStaffGroupIdTarget>(musx::dom::SCORE_PARTID, objectCmper);
    const auto partGroup = document->getOthers()->get<MultiStaffGroupIdTarget>(partId, objectCmper);

    REQUIRE(instrument);
    CHECK(instrument->staffNums == std::vector<musx::dom::StaffCmper>{1, 2});
    REQUIRE(scoreGroup);
    CHECK(scoreGroup->staffGroupId == 4);
    REQUIRE(partGroup);
    CHECK(partGroup->staffGroupId == 9);
    CHECK(reportedFieldCount(report) == 4);
}

TEST_CASE("Incomplete multi-staff class records do not create objects", "[class]")
{
    auto profile = profileFor(finale2012Major);
    profile.epoch = FormatEpoch::ZlibLegacy;
    profile.byteOrder = ByteOrder::LittleEndian;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    constexpr musx::dom::Cmper objectCmper = 1;
    const auto report = importMultiStaffRecords(
        makeClassContainer(
            {{multiStaffInstrumentGroupClass, {1, 2}, objectCmper}, {multiStaffGroupIdClass, {1}, objectCmper}}, ByteOrder::LittleEndian),
        profile, document);

    CHECK(document->getOthers()->getArray<MultiStaffInstrumentGroupTarget>(musx::dom::SCORE_PARTID).empty());
    CHECK(document->getOthers()->getArray<MultiStaffGroupIdTarget>(musx::dom::SCORE_PARTID).empty());
    CHECK(reportedFieldCount(report) == 0);
}

TEST_CASE("Multi-staff class ids are ignored before Finale 2012", "[class]")
{
    auto profile = profileFor(std::uint8_t(16));
    profile.epoch = FormatEpoch::ZlibLegacy;
    profile.byteOrder = ByteOrder::LittleEndian;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    constexpr musx::dom::Cmper objectCmper = 1;
    const auto report = importMultiStaffRecords(makeClassContainer({{multiStaffInstrumentGroupClass, {1, 2, 0, 0, 0, 0}, objectCmper},
                                                                       {multiStaffGroupIdClass, {4, 0, 0, 0, 0, 0}, objectCmper}},
                                                    ByteOrder::LittleEndian),
        profile, document);

    CHECK(document->getOthers()->getArray<MultiStaffInstrumentGroupTarget>(musx::dom::SCORE_PARTID).empty());
    CHECK(document->getOthers()->getArray<MultiStaffGroupIdTarget>(musx::dom::SCORE_PARTID).empty());
    CHECK(reportedFieldCount(report) == 0);
}

TEST_CASE("Controlled Finale 2012 piano recovers its multi-staff records", "[class][reader]")
{
    const auto baseline = readFixture("evidence/F2012/F2012-baseline.mus");
    CHECK(baseline.document->getOthers()->getArray<MultiStaffInstrumentGroupTarget>(musx::dom::SCORE_PARTID).empty());
    CHECK(baseline.document->getOthers()->getArray<MultiStaffGroupIdTarget>(musx::dom::SCORE_PARTID).empty());

    const auto piano = readFixture("evidence/F2012/F2012-piano.mus");
    constexpr musx::dom::Cmper objectCmper = 1;
    const auto instrument = piano.document->getOthers()->get<MultiStaffInstrumentGroupTarget>(musx::dom::SCORE_PARTID, objectCmper);
    const auto groupId = piano.document->getOthers()->get<MultiStaffGroupIdTarget>(musx::dom::SCORE_PARTID, objectCmper);
    REQUIRE(instrument);
    CHECK(instrument->staffNums == std::vector<musx::dom::StaffCmper>{1, 2});
    REQUIRE(groupId);
    CHECK(groupId->staffGroupId == 1);
    CHECK(fieldFor<MultiStaffInstrumentGroupTarget>(piano, "others.multiStaffInstGroup[1].staffNums[1]").origin == ValueOrigin::LegacyMus);
    CHECK(fieldFor<MultiStaffGroupIdTarget>(piano, "others.multiStaffGroupID[1].staffGroupId").origin == ValueOrigin::LegacyMus);
}

} // namespace
} // namespace finale_mus_reader_tests
