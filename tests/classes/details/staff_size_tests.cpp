// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include <set>

#include "class_test_support.h"
#include "staff_size_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

using StaffSize = musx::dom::details::StaffSize;
using StaffSystem = musx::dom::others::StaffSystem;

TEST_CASE("StaffSize recovers one detail incidence without deriving StaffSystem state", "[class][staff-size]")
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            constexpr musx::dom::Cmper systemId = 7;
            constexpr musx::dom::Cmper staffId = 12;
            constexpr std::int16_t staffPercent = 83;
            const auto parsed =
                epoch == FormatEpoch::ZlibLegacy
                    ? makeDetailClassContainer(systemId, staffId, musx::dom::SCORE_PARTID, {staffPercent, 1, 2, 3, 4}, byteOrder, 0x0410)
                    : makeDetailContainer(epoch, systemId, staffId, {staffPercent, 1, 2, 3, 4}, "LP", byteOrder);
            auto profile = SourceProfile(epoch);
            profile.byteOrder = byteOrder;
            const auto document = staffSizeDocument(systemId);
            const auto report = importStaffSizes(parsed, profile, document);

            const auto staffSize = document->getDetails()->get<StaffSize>(musx::dom::SCORE_PARTID, systemId, staffId);
            REQUIRE(staffSize);
            CHECK(staffSize->staffPercent == staffPercent);
            const auto system = document->getOthers()->get<StaffSystem>(musx::dom::SCORE_PARTID, systemId);
            REQUIRE(system);
            CHECK_FALSE(system->hasStaffScaling);

            const auto* percent = report.findField<StaffSize>("staffPercent", musx::dom::SCORE_PARTID, systemId, std::nullopt, staffId);
            REQUIRE(percent);
            CHECK(percent->origin == ValueOrigin::LegacyMus);
            CHECK(percent->rawValue == staffPercent);
            CHECK_FALSE(report.findField<StaffSystem>("hasStaffScaling", musx::dom::SCORE_PARTID, systemId));
        }
    }
}

TEST_CASE("StaffSize preserves records without a retained StaffSystem", "[class][staff-size]")
{
    constexpr musx::dom::Cmper retainedSystemId = 7;
    constexpr musx::dom::Cmper staleSystemId = 8;
    const auto document = staffSizeDocument(retainedSystemId);
    auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    const auto report = importStaffSizes(
        makeDetailContainer(FormatEpoch::UncompressedLegacy, staleSystemId, 12, {75, 0, 0, 0, 0}, "LP", ByteOrder::BigEndian), profile, document);

    const auto stale = document->getDetails()->get<StaffSize>(musx::dom::SCORE_PARTID, staleSystemId, 12);
    REQUIRE(stale);
    CHECK(stale->staffPercent == 75);
    const auto system = document->getOthers()->get<StaffSystem>(musx::dom::SCORE_PARTID, retainedSystemId);
    REQUIRE(system);
    CHECK_FALSE(system->hasStaffScaling);
    CHECK_FALSE(report.findField<StaffSystem>("hasStaffScaling", musx::dom::SCORE_PARTID, retainedSystemId));
}

TEST_CASE("StaffSize rejects a truncated class payload", "[class][staff-size]")
{
    constexpr musx::dom::Cmper systemId = 7;
    constexpr musx::dom::Cmper staffId = 12;
    const auto document = staffSizeDocument(systemId);
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    const auto report = importStaffSizes(
        makeDetailClassContainer(systemId, staffId, musx::dom::SCORE_PARTID, {}, ByteOrder::LittleEndian, 0x0410), profile, document, false);

    CHECK(document->getDetails()->getAllSources<StaffSize>().empty());
    CHECK(report.diagnostics.size() == 1);
    const auto system = document->getOthers()->get<StaffSystem>(musx::dom::SCORE_PARTID, systemId);
    REQUIRE(system);
    CHECK_FALSE(system->hasStaffScaling);
}

TEST_CASE("Early controlled fixtures recover the compact StaffSize layout", "[class][staff-size]")
{
    struct FixtureCase
    {
        const char* baseline;
        const char* scaled;
        int percent;
    };
    for (const auto& expected : {
             FixtureCase{"evidence/F100/F100-4systems.mus", "evidence/F100/F100-4systems-sys2to73.mus", 73},
             FixtureCase{"evidence/F372/F372-4systems.mus", "evidence/F372/F372-4systems-sys2to71.mus", 71},
         }) {
        const auto baseline = readFixture(expected.baseline);
        const auto scaled = readFixture(expected.scaled);
        CHECK(baseline.document->getDetails()->getAllSources<StaffSize>().empty());
        const auto staffSizes = scaled.document->getDetails()->getAllSources<StaffSize>();
        REQUIRE(staffSizes.size() == 1);
        const auto staffSize = scaled.document->getDetails()->get<StaffSize>(0, 2, 1);
        REQUIRE(staffSize);
        CHECK(staffSize->staffPercent == expected.percent);
        CHECK(staffSize->getSourcePartId() == musx::dom::SCORE_PARTID);
        CHECK(staffSize->getShareMode() == musx::dom::EnigmaBase::ShareMode::All);
    }
}
} // namespace
} // namespace finale_mus_reader_tests
