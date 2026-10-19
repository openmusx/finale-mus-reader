// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using MeasureAssignTestTarget = musx::dom::others::SmartShapeMeasureAssign;
using MeasureCmper = musx::dom::Cmper;
using MeasureInci = musx::dom::Inci;

TEST_CASE("SmartShape measure tuple keeps a nonzero center reference", "[class][smart-shape-measure-assign]")
{
    for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        const auto parsed = makeClassContainer(0x00da, {45, 7, 0, 0, 0, 0}, order, 4);
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
        finale_mus_reader::others::importSmartShapeMeasureAssigns(context);
        finale_mus_reader::runDeferredChecks(pending);
        const auto assignment = document->getOthers()->get<MeasureAssignTestTarget>(musx::dom::SCORE_PARTID, MeasureCmper(4), MeasureInci(0));
        REQUIRE(assignment);
        CHECK(assignment->shapeNum == 45);
        CHECK(assignment->centerShapeNum == 7);
        const auto* field = report.findField<MeasureAssignTestTarget>("centerShapeNum", musx::dom::SCORE_PARTID, MeasureCmper(4), MeasureInci(0));
        REQUIRE(field);
        CHECK(field->rawValue == 7);
    }
}

TEST_CASE("Stored SmartShape measure assignments retain shape and center identifiers", "[class][smart-shape-measure-assign]")
{
    const auto result = readFixture("evidence/F2006/F2006-lyric-vcs.mus");
    const auto assignments = result.document->getOthers()->getArray<MeasureAssignTestTarget>(musx::dom::SCORE_PARTID, MeasureCmper(2));
    REQUIRE(assignments.size() == 9);
    CHECK(assignments[0]->shapeNum == 1);
    CHECK(assignments[0]->centerShapeNum == 0);
    CHECK(assignments[1]->shapeNum == 3);
    const auto* shapeNum = result.report.findField<MeasureAssignTestTarget>("shapeNum", musx::dom::SCORE_PARTID, MeasureCmper(2), MeasureInci(1));
    const auto* centerNum =
        result.report.findField<MeasureAssignTestTarget>("centerShapeNum", musx::dom::SCORE_PARTID, MeasureCmper(2), MeasureInci(1));
    REQUIRE(shapeNum);
    REQUIRE(centerNum);
    CHECK(shapeNum->origin == ValueOrigin::LegacyMus);
    CHECK(shapeNum->rawValue == 3);
    CHECK(centerNum->rawValue == 0);
}

TEST_CASE("Uncompressed SmartShape measure assignment follows the stored Mx record", "[class][smart-shape-measure-assign]")
{
    const auto result = readFixture("evidence/F97/F97-F300-slur.mus");
    const auto assignments = result.document->getOthers()->getArray<MeasureAssignTestTarget>(musx::dom::SCORE_PARTID, MeasureCmper(1));
    REQUIRE(assignments.size() == 1);
    CHECK(assignments.front()->shapeNum == 1);
    CHECK(assignments.front()->centerShapeNum == 0);
}

TEST_CASE("Early SmartShape endpoint assignments occur once per measure and shape", "[class][smart-shape-measure-assign]")
{
    const auto result = readFixture("evidence/F263/F263-cresc-dim.mus");
    const auto assignments = result.document->getOthers()->getArray<MeasureAssignTestTarget>(musx::dom::SCORE_PARTID, MeasureCmper(1));
    REQUIRE(assignments.size() == 2);
    CHECK(assignments[0]->shapeNum == 1);
    CHECK(assignments[1]->shapeNum == 2);
    CHECK(assignments[0]->centerShapeNum == 0);
    const auto* centerNum =
        result.report.findField<MeasureAssignTestTarget>("centerShapeNum", musx::dom::SCORE_PARTID, MeasureCmper(1), MeasureInci(0));
    REQUIRE(centerNum);
    CHECK(centerNum->origin == ValueOrigin::LegacyBehavior);
}

TEST_CASE("Zlib SmartShape measure assignments decode concatenated tuples", "[class][smart-shape-measure-assign]")
{
    const auto result = readFixture("evidence/F2007/F2007-lyric-hyphens.mus");
    const auto assignments = result.document->getOthers()->getArray<MeasureAssignTestTarget>(musx::dom::SCORE_PARTID, MeasureCmper(2));
    REQUIRE(assignments.size() == 2);
    CHECK(assignments[0]->shapeNum == 1);
    CHECK(assignments[1]->shapeNum == 2);
    CHECK(assignments[1]->centerShapeNum == 0);
    const auto* field = result.report.findField<MeasureAssignTestTarget>("shapeNum", musx::dom::SCORE_PARTID, MeasureCmper(2), MeasureInci(1));
    REQUIRE(field);
    CHECK(field->sourceIdentity == 0x00da);
}

} // namespace
} // namespace finale_mus_reader_tests
