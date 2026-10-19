// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using Target = musx::dom::details::ArticulationAssign;

TEST_CASE("Articulation assignment reads the stored words", "[class][articulation-assign]")
{
    const auto result = readFixture("evidence/F2001/F2001Win-csharp-artic.mus");
    const auto assignment = result.document->getDetails()->get<Target>(musx::dom::SCORE_PARTID, 1, musx::dom::Inci(0));
    REQUIRE(assignment);
    CHECK(assignment->articDef == 1);
    CHECK(assignment->horzOffset == 0);
    CHECK(assignment->horzAdd == 0);
    CHECK(assignment->vertOffset == -51);
    CHECK(assignment->vertAdd == 0);
    CHECK(assignment->neverStack);
    CHECK(assignment->numSlursAvoided == -1);
    const auto key = finale_mus_reader::instanceKey<Target>(musx::dom::SCORE_PARTID, musx::dom::Cmper(0), musx::dom::Inci(0), musx::dom::Cmper(1));
    for (const auto* member : {"articDef", "horzOffset", "horzAdd", "vertOffset", "vertAdd"}) {
        const auto* field = result.report.findField(key, member);
        REQUIRE(field);
        CHECK(field->origin == ValueOrigin::LegacyMus);
    }
    for (const auto* member : {"overridePlacement", "aboveEntry", "neverStack", "avoidSlur", "numSlursAvoided"}) {
        const auto* field = result.report.findField(key, member);
        REQUIRE(field);
        CHECK(field->origin == ValueOrigin::LegacyBehavior);
    }
    CHECK(result.report.findField(key, "hide")->origin == ValueOrigin::LegacyBehavior);
    CHECK(Target::xmlMappingArray().size() == 11);
}

TEST_CASE("Zlib articulation assignment keeps its single extended tuple", "[class][articulation-assign]")
{
    const auto result = readFixture("evidence/F2012/F2012-noteartexp.mus");
    const auto assignments = result.document->getDetails()->getArray<Target>(musx::dom::SCORE_PARTID, 5);
    REQUIRE(assignments.size() == 1);
    CHECK(assignments[0]->articDef == 1);
    CHECK(assignments[0]->horzOffset == -3);
    CHECK(assignments[0]->vertOffset == 24);
    CHECK(assignments[0]->neverStack);
    CHECK(assignments[0]->numSlursAvoided == -1);
    const auto key = finale_mus_reader::instanceKey<Target>(musx::dom::SCORE_PARTID, musx::dom::Cmper(0), musx::dom::Inci(0), musx::dom::Cmper(5));
    CHECK(result.report.findField(key, "hide")->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Articulation assignment decodes the expanded hidden flag in either byte order", "[class][articulation-assign]")
{
    for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        for (const std::int16_t flags : {std::int16_t{0}, std::int16_t{1}, std::int16_t{2}, std::int16_t{3}}) {
            const auto parsed = makeDetailClassContainer(0, 27, 0, {7, -9, 4, 21, -6, flags, 0, 0, 0, 0}, order, 0x03f1);
            auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
            profile.byteOrder = order;
            auto session = musx::factory::DocumentFactory::begin();
            const auto document = session.getDocument();
            ImportReport report(profile.epoch);
            const auto index = LegacyRecordIndex::build(parsed);
            auto referenceSession = musx::factory::DocumentFactory::begin();
            const auto reference = std::move(referenceSession).finish();
            finale_mus_reader::PendingReferences pending;
            musx::factory::ConstructionContext construction;
            const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
            finale_mus_reader::details::importArticulationAssigns(context);
            const auto assignment = document->getDetails()->get<Target>(musx::dom::SCORE_PARTID, 27, musx::dom::Inci(0));
            REQUIRE(assignment);
            CHECK(assignment->articDef == 7);
            CHECK(assignment->horzOffset == -9);
            CHECK(assignment->horzAdd == 4);
            CHECK(assignment->vertOffset == 21);
            CHECK(assignment->vertAdd == -6);
            CHECK(assignment->hide == ((flags & 1) != 0));
            CHECK(assignment->neverStack);
            CHECK(assignment->numSlursAvoided == -1);
        }
    }
}

TEST_CASE("An older articulation row without a definition is discarded", "[class][articulation-assign]")
{
    const auto parsed = makeDetailContainer(FormatEpoch::DclLegacy, 0, 27, {0, 1, 2, 3, 4}, "IM", ByteOrder::LittleEndian);
    auto profile = SourceProfile(FormatEpoch::DclLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::details::importArticulationAssigns(context);
    CHECK(document->getDetails()->getAllSources<Target>().empty());
}

TEST_CASE("Before Finale 3.0 an entry without an articulation assignment has its flag cleared", "[class][articulation-assign]")
{
    const auto result = readFixture("evidence/F263/F263-F100-textrpt.mus");
    const auto entry = result.document->getEntries()->get(14);
    REQUIRE(entry);
    CHECK_FALSE(entry->articDetail);
    CHECK(result.document->getDetails()->getArray<Target>(musx::dom::SCORE_PARTID, 14).empty());
    const auto* field =
        result.report.findField<musx::dom::Entry>("articDetail", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(14));
    REQUIRE(field);
    CHECK(field->origin == ValueOrigin::LegacyMusAdjusted);
}

} // namespace
} // namespace finale_mus_reader_tests
