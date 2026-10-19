// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <algorithm>
#include <vector>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using CenterShape = musx::dom::details::CenterShape;
struct EarlyShapeCase
{
    musx::dom::Cmper shapeId;
    musx::dom::MeasCmper endMeasure;
};

struct EarlyCenterCase
{
    musx::dom::Cmper shapeId;
    musx::dom::Cmper centerId;
    musx::dom::Cmper measure;
};

ImportReport importCenterShapes(
    const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile, const musx::dom::DocumentPtr& document)
{
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::details::importCenterShapes(context);
    finale_mus_reader::runDeferredChecks(pending);
    return report;
}

TEST_CASE("CenterShape reads the three-incidence detail and class payload", "[class][center-shape]")
{
    const std::vector<std::int16_t> words = {11, -12, 0x4203, -13, 14, 0x4100, 21, -22, -23, 24, 0x4200, 0, 0, 0, 0};
    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            const auto parsed = epoch == FormatEpoch::ZlibLegacy ? makeDetailClassContainer(31, 7, musx::dom::SCORE_PARTID, words, order, 0x0406)
                                                                 : makeDetailContainer(epoch, 31, 7, words, "Cx", order);
            auto profile = SourceProfile(epoch);
            profile.byteOrder = order;
            auto session = musx::factory::DocumentFactory::begin();
            const auto document = session.getDocument();
            const auto report = importCenterShapes(parsed, profile, document);
            const auto shape = document->getDetails()->get<CenterShape>(musx::dom::SCORE_PARTID, 31, 7);
            REQUIRE(shape);
            REQUIRE(shape->startBreakAdj);
            REQUIRE(shape->endBreakAdj);
            REQUIRE(shape->ctlPtAdj);
            CHECK(shape->startBreakAdj->horzOffset == 11);
            CHECK(shape->startBreakAdj->vertOffset == -12);
            CHECK(shape->startBreakAdj->active);
            CHECK(shape->startBreakAdj->contextDir == musx::dom::smartshape::DirectionType::Over);
            CHECK(shape->startBreakAdj->contextEntCnct == musx::dom::smartshape::EntryConnectionType::HeadLeftBottom);
            CHECK(shape->endBreakAdj->horzOffset == -13);
            CHECK(shape->endBreakAdj->vertOffset == 14);
            CHECK(shape->endBreakAdj->active);
            CHECK(shape->endBreakAdj->contextDir == musx::dom::smartshape::DirectionType::Under);
            CHECK(shape->ctlPtAdj->startCtlPtX == 21);
            CHECK(shape->ctlPtAdj->startCtlPtY == -22);
            CHECK(shape->ctlPtAdj->endCtlPtX == -23);
            CHECK(shape->ctlPtAdj->endCtlPtY == 24);
            CHECK(shape->ctlPtAdj->active);
            CHECK(shape->ctlPtAdj->contextDir == musx::dom::smartshape::DirectionType::Over);
            const auto* field = report.findField<CenterShape>(
                "ctlPtAdj.startCtlPtX", musx::dom::SCORE_PARTID, musx::dom::Cmper(31), std::nullopt, musx::dom::Cmper(7));
            REQUIRE(field);
            CHECK(field->origin == ValueOrigin::LegacyMus);
            CHECK(field->rawValue == 21);
        }
    }
}

struct SyntheticDetail
{
    const char* tag{};
    std::uint16_t cmper1{};
    std::uint16_t cmper2{};
    std::vector<std::int16_t> words;
};

// Builds early shapes, their measure assignments, and their centers from the given rows.
musx::dom::DocumentPtr importEarlyShapes(const std::vector<SyntheticRow>& others, const std::vector<SyntheticDetail>& details)
{
    auto parsed = makeContainer(others);
    finale_mus_reader::container::DecodedBlock detailBlock;
    detailBlock.info.type = 0x0002;
    for (const auto& detail : details) {
        auto single = makeDetailContainer(FormatEpoch::UncompressedLegacy, detail.cmper1, detail.cmper2, detail.words, detail.tag);
        auto& data = single.blocks.front().data;
        detailBlock.data.insert(detailBlock.data.end(), data.begin(), data.end());
    }
    detailBlock.info.decodedSize = detailBlock.data.size();
    parsed.blocks.push_back(std::move(detailBlock));
    SourceProfile profile(FormatEpoch::UncompressedLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importSmartShapes(context);
    finale_mus_reader::others::importSmartShapeMeasureAssigns(context);
    finale_mus_reader::details::importCenterShapes(context);
    finale_mus_reader::runDeferredChecks(pending);
    return document;
}

TEST_CASE("Early spanning SmartShapes build their centers from endpoint and center rows", "[class][center-shape]")
{
    using MeasureAssign = musx::dom::others::SmartShapeMeasureAssign;
    const auto flags = [](std::uint16_t value) { return static_cast<std::int16_t>(value); };
    // A crescendo and a down slur span measures 3 to 6. The slur has a center row without details
    // in measure 4 and one with break and curvature details in its end measure.
    const auto document = importEarlyShapes(
        {{1, "sX", {6, 3, 0, 0, 0, flags(0x8200)}}, {1, "eX", {3, 3, 0, 0, 0, flags(0x8200)}}, {2, "sX", {6, 0, 0, 0, 0, flags(0x8200)}},
            {2, "eX", {3, 0, 0, 0, 0, flags(0x8200)}}, {3, "DY", {1, 0, -30, 0, 0, flags(0x8001)}}, {3, "DY", {2, 0, -50, 0, 0, flags(0x8001)}},
            {4, "DY", {2, 0, -7, 0, 0, flags(0xa001)}}, {6, "DY", {1, 900, -30, 0, 0, flags(0xc001)}}, {6, "DY", {2, 1, -9, 0, 0, flags(0xa001)}},
            {6, "DY", {2, 900, -50, 0, 0, flags(0xc001)}}},
        {{"bX", 2, 1, {5, 6, 7, 8, 0}}, {"dX", 2, 1, {10, 20, 0, 4, 0}}});
    const auto center = [&](musx::dom::Cmper shapeId, musx::dom::Cmper centerId) {
        const auto found = document->getDetails()->get<CenterShape>(musx::dom::SCORE_PARTID, shapeId, centerId);
        REQUIRE(found);
        CHECK(found->startBreakAdj->active);
        CHECK(found->endBreakAdj->active);
        return found;
    };
    for (const auto centerId : {musx::dom::Cmper(1), musx::dom::Cmper(2)}) {
        const auto hairpin = center(1, centerId);
        CHECK(hairpin->startBreakAdj->horzOffset == 0);
        CHECK(hairpin->startBreakAdj->vertOffset == -30);
        CHECK(hairpin->endBreakAdj->vertOffset == -30);
        CHECK(hairpin->ctlPtAdj->active);
        CHECK(hairpin->ctlPtAdj->startCtlPtY == 24);
    }
    CHECK_FALSE(document->getDetails()->get<CenterShape>(musx::dom::SCORE_PARTID, 1, 3));

    const auto fromRow = center(2, 1);
    CHECK(fromRow->startBreakAdj->vertOffset == -7);
    CHECK(fromRow->endBreakAdj->vertOffset == -7);
    CHECK(fromRow->ctlPtAdj->startCtlPtX == 171);
    CHECK(fromRow->ctlPtAdj->startCtlPtY == -24);
    const auto fromStart = center(2, 2);
    CHECK(fromStart->startBreakAdj->vertOffset == -50);
    const auto fromDetails = center(2, 3);
    CHECK(fromDetails->startBreakAdj->horzOffset == 5);
    CHECK(fromDetails->startBreakAdj->vertOffset == -3);
    CHECK(fromDetails->endBreakAdj->horzOffset == 7);
    CHECK(fromDetails->endBreakAdj->vertOffset == -1);
    CHECK(fromDetails->ctlPtAdj->startCtlPtX == 384);
    CHECK(fromDetails->ctlPtAdj->endCtlPtX == -242);
    CHECK(fromDetails->ctlPtAdj->startCtlPtY == -28);
    CHECK(fromDetails->ctlPtAdj->endCtlPtY == -28);

    const auto assignedCenter = [&](musx::dom::Cmper measure, musx::dom::Cmper shapeId) {
        const auto assignments = document->getOthers()->getArray<MeasureAssign>(musx::dom::SCORE_PARTID, measure);
        const auto found = std::find_if(assignments.begin(), assignments.end(), [shapeId](const auto& value) { return value->shapeNum == shapeId; });
        REQUIRE(found != assignments.end());
        return (*found)->centerShapeNum;
    };
    CHECK(assignedCenter(3, 1) == 0);
    CHECK(assignedCenter(4, 1) == 1);
    CHECK(assignedCenter(5, 1) == 2);
    CHECK(assignedCenter(6, 1) == 0);
    CHECK(assignedCenter(4, 2) == 1);
    CHECK(assignedCenter(5, 2) == 2);
    CHECK(assignedCenter(6, 2) == 3);
}

TEST_CASE("Early 8va centers take the start row's vertical offset", "[class][center-shape]")
{
    const auto result = readFixture("evidence/F263/F263-11bars-8va-3to8.mus");
    using MeasureAssign = musx::dom::others::SmartShapeMeasureAssign;
    const auto centers = result.document->getDetails()->getAllSources<CenterShape>();
    REQUIRE(centers.size() == 4);
    for (int centerId = 1; centerId <= 4; ++centerId) {
        const auto center = result.document->getDetails()->get<CenterShape>(musx::dom::SCORE_PARTID, 1, musx::dom::Cmper(centerId));
        REQUIRE(center);
        CHECK(center->startBreakAdj->active);
        CHECK(center->startBreakAdj->vertOffset == 40);
        CHECK(center->endBreakAdj->active);
        CHECK(center->endBreakAdj->vertOffset == 40);
        CHECK_FALSE(center->ctlPtAdj->active);
        const auto assignments = result.document->getOthers()->getArray<MeasureAssign>(musx::dom::SCORE_PARTID, musx::dom::Cmper(centerId + 3));
        REQUIRE(assignments.size() == 1);
        CHECK(assignments.front()->shapeNum == 1);
        CHECK(assignments.front()->centerShapeNum == centerId);
    }
    const auto* field = result.report.findField<CenterShape>(
        "startBreakAdj.vertOffset", musx::dom::SCORE_PARTID, musx::dom::Cmper(1), std::nullopt, musx::dom::Cmper(1));
    REQUIRE(field);
    CHECK(field->origin == ValueOrigin::LegacyMus);
    CHECK(field->sourceIdentity == finale_mus_reader::records::packTag("DY"));
    CHECK(field->rawValue == 40);
}

TEST_CASE("CenterShape rejects an incomplete class record", "[class][center-shape]")
{
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    const auto report = importCenterShapes(
        makeDetailClassContainer(31, 7, musx::dom::SCORE_PARTID, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, ByteOrder::LittleEndian, 0x0406), profile,
        document);
    CHECK(document->getDetails()->getAllSources<CenterShape>().empty());
    REQUIRE(report.diagnostics.size() == 1);
}

} // namespace
} // namespace finale_mus_reader_tests
