// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"
#include "import/details.h"
#include "import/shared/skipped_smart_shapes.h"

#include <array>
#include <tuple>
#include <utility>
#include <vector>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using Shape = musx::dom::others::SmartShape;

struct SyntheticShapes
{
    musx::dom::DocumentPtr document;
    ImportReport report;
};

SyntheticShapes importSyntheticShapes(const std::vector<SyntheticRow>& rows, FormatEpoch epoch = FormatEpoch::UncompressedLegacy)
{
    const auto parsed = makeContainer(rows, epoch);
    SourceProfile profile(epoch);
    profile.byteOrder = ByteOrder::BigEndian;
    auto session = musx::factory::DocumentFactory::begin();
    SyntheticShapes result{session.getDocument(), ImportReport(profile.epoch)};
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, result.document, reference, result.report, pending, construction};
    finale_mus_reader::others::importSmartShapes(context);
    finale_mus_reader::runDeferredChecks(pending);
    return result;
}

constexpr auto flags(std::uint16_t value)
{
    return static_cast<std::int16_t>(value);
}

TEST_CASE("Finale 2007 lyric shapes recover their source endpoints", "[class][smart-shape]")
{
    const auto result = readFixture("evidence/F2007/F2007-lyric-hyphens.mus");
    const auto first = result.document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    const auto second = result.document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(2));
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first->shapeType == Shape::ShapeType::Hyphen);
    CHECK(second->shapeType == Shape::ShapeType::WordExtension);
    CHECK(first->startTermSeg->endPoint->staffId == 1);
    CHECK(first->startTermSeg->endPoint->measId == 1);
    CHECK_FALSE(first->entryBased);
    CHECK(first->startTermSeg->endPoint->entryNumber == 13);
    CHECK(first->endTermSeg->endPoint->measId == 2);
    CHECK(first->endTermSeg->endPoint->entryNumber == 15);
    CHECK(first->startLyricNum == 1);
    CHECK(first->startLyricType == musx::dom::LyricTextType::Verse);
    CHECK(first->endLyricType == musx::dom::LyricTextType::Verse);
    CHECK(result.report.findField<Shape>("shapeType", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
    CHECK(result.report.findField<Shape>("startLyricType", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Inactive part SmartShape adjustments retain score attachment context", "[class][smart-shape]")
{
    std::vector<std::int16_t> scoreWords(48);
    scoreWords[0] = 15;
    scoreWords[1] = static_cast<std::int16_t>(0x8211);
    scoreWords[3] = 1;
    scoreWords[4] = 14;
    scoreWords[8] = 6;
    scoreWords[9] = 0x4205;
    scoreWords[17] = 0x4201;
    scoreWords[18] = 1;
    scoreWords[19] = 14;
    scoreWords[24] = 0x4201;
    scoreWords[37] = 0x4200;
    auto partWords = scoreWords;
    partWords[1] = static_cast<std::int16_t>(0x8201);
    partWords[8] = 0;
    partWords[9] = 0;
    partWords[17] = 0x4103;
    partWords[24] = 0;
    partWords[37] = 0;
    const auto parsed = makeClassContainer(
        {{0x00d9, scoreWords, 72}, {0x00d9, partWords, 72, 1, true, std::vector<std::uint16_t>(46, 0xffff)}}, ByteOrder::LittleEndian);
    SourceProfile profile(FormatEpoch::ZlibLegacy);
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
    finale_mus_reader::others::importSmartShapes(context);
    finale_mus_reader::runDeferredChecks(pending);

    const auto score = document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(72));
    const auto part = document->getOthers()->get<Shape>(musx::dom::Cmper(1), musx::dom::Cmper(72));
    REQUIRE(score);
    REQUIRE(part);
    CHECK(score->startTermSeg->endPointAdj->contextDir == musx::dom::smartshape::DirectionType::Over);
    CHECK_FALSE(part->startTermSeg->endPointAdj->active);
    CHECK(part->startTermSeg->endPointAdj->vertOffset == 0);
    CHECK(part->startTermSeg->endPointAdj->contextDir == musx::dom::smartshape::DirectionType::Over);
    CHECK(part->startTermSeg->endPointAdj->contextEntCnct == musx::dom::smartshape::EntryConnectionType::StemRightTop);
    CHECK(part->endTermSeg->endPointAdj->contextDir == musx::dom::smartshape::DirectionType::Over);
    CHECK(part->fullCtlPtAdj->contextDir == musx::dom::smartshape::DirectionType::Over);
    CHECK(part->startTermSeg->breakAdj->contextDir == musx::dom::smartshape::DirectionType::Under);
    CHECK(part->startTermSeg->breakAdj->contextEntCnct == musx::dom::smartshape::EntryConnectionType::HeadLeftBottom);
    CHECK(part->engraverSlurState == Shape::EngraverSlurState::Off);
    const auto* adjusted = report.findField<Shape>("startTermSeg.endPointAdj.contextDir", musx::dom::Cmper(1), musx::dom::Cmper(72));
    REQUIRE(adjusted);
    CHECK(adjusted->origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(adjusted->rawValue == 0);
}

TEST_CASE("Active part SmartShape adjustments retain score attachment context", "[class][smart-shape]")
{
    std::vector<std::int16_t> scoreWords(48);
    scoreWords[0] = 15;
    scoreWords[1] = static_cast<std::int16_t>(0x8201);
    scoreWords[3] = 1;
    scoreWords[4] = 14;
    scoreWords[17] = 0x4205;
    scoreWords[18] = 1;
    scoreWords[19] = 14;
    scoreWords[30] = 2;
    scoreWords[31] = 40;
    scoreWords[32] = 0x4205;
    auto partWords = scoreWords;
    // The part edited both break offsets and stored them with no context.
    partWords[17] = 0x4000;
    partWords[30] = -42;
    partWords[31] = 5;
    partWords[32] = 0x4103;
    const auto parsed = makeClassContainer(
        {{0x00d9, scoreWords, 34}, {0x00d9, partWords, 34, 1, true, std::vector<std::uint16_t>(46, 0xffff)}}, ByteOrder::LittleEndian);
    SourceProfile profile(FormatEpoch::ZlibLegacy);
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
    finale_mus_reader::others::importSmartShapes(context);
    finale_mus_reader::runDeferredChecks(pending);

    const auto part = document->getOthers()->get<Shape>(musx::dom::Cmper(1), musx::dom::Cmper(34));
    REQUIRE(part);
    const auto& start = *part->startTermSeg->breakAdj;
    CHECK(start.active);
    CHECK(start.contextDir == musx::dom::smartshape::DirectionType::Over);
    CHECK(start.contextEntCnct == musx::dom::smartshape::EntryConnectionType::StemRightTop);
    const auto& end = *part->endTermSeg->breakAdj;
    CHECK(end.active);
    CHECK(end.horzOffset == -42);
    CHECK(end.vertOffset == 5);
    CHECK(end.contextDir == musx::dom::smartshape::DirectionType::Under);
    CHECK(end.contextEntCnct == musx::dom::smartshape::EntryConnectionType::HeadLeftBottom);
    const auto* adjusted = report.findField<Shape>("startTermSeg.breakAdj.contextEntCnct", musx::dom::Cmper(1), musx::dom::Cmper(34));
    REQUIRE(adjusted);
    CHECK(adjusted->origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(adjusted->rawValue == 0);
    CHECK(report.findField<Shape>("endTermSeg.breakAdj.contextEntCnct", musx::dom::Cmper(1), musx::dom::Cmper(34))->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Part slur edit retains linked shape identity and inactive adjustments", "[class][smart-shape]")
{
    const auto linked = readFixture("evidence/F2012/F2012-slur-part.mus");
    const auto edited = readFixture("evidence/F2012/F2012-slur-part-edited.mus");
    const auto linkedScore = linked.document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    const auto linkedPart = linked.document->getOthers()->get<Shape>(musx::dom::Cmper(1), musx::dom::Cmper(1));
    const auto editedScore = edited.document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    const auto editedPart = edited.document->getOthers()->get<Shape>(musx::dom::Cmper(1), musx::dom::Cmper(1));
    REQUIRE(linkedScore);
    REQUIRE(linkedPart);
    REQUIRE(editedScore);
    REQUIRE(editedPart);
    CHECK(linked.document->getOthers()->getAllSources<Shape>().size() == 1);
    CHECK(linkedPart->shapeType == Shape::ShapeType::SlurAuto);
    CHECK(editedPart != editedScore);
    CHECK(edited.document->getOthers()->getAllSources<Shape>().size() == 2);
    CHECK(editedPart->getShareMode() == Shape::ShareMode::Partial);
    CHECK(editedScore->shapeType == Shape::ShapeType::SlurAuto);
    CHECK(editedPart->shapeType == Shape::ShapeType::SlurDown);
    const auto* partType = edited.report.findField<Shape>("shapeType", musx::dom::Cmper(1), musx::dom::Cmper(1));
    REQUIRE(partType);
    CHECK(partType->origin == ValueOrigin::LegacyMus);
    CHECK(partType->rawValue == 0);
    CHECK_FALSE(editedPart->startTermSeg->endPointAdj->active);
    CHECK_FALSE(editedPart->endTermSeg->endPointAdj->active);
}

TEST_CASE("Seven-incidence SmartShapes retain endpoints through saved upgrades", "[class][smart-shape]")
{
    for (const char* fixture : {"evidence/F372/F372-F300-slur.mus", "evidence/F97/F97-F300-slur.mus", "evidence/F2000/F2000-F300-slur.mus"}) {
        const auto result = readFixture(fixture);
        const auto shape = result.document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(shape);
        CHECK(shape->shapeType == Shape::ShapeType::SlurDown);
        CHECK_FALSE(shape->entryBased);
        CHECK(shape->startTermSeg->endPoint->staffId == 1);
        CHECK(shape->startTermSeg->endPoint->measId == 1);
        CHECK(shape->startTermSeg->endPoint->eduPosition == 142);
        CHECK(shape->startTermSeg->endPoint->entryNumber == 0);
        CHECK(shape->endTermSeg->endPoint->eduPosition == 1138);
        CHECK(shape->startTermSeg->endPointAdj->vertOffset == -100);
        CHECK(shape->endTermSeg->endPointAdj->vertOffset == -80);
        CHECK(shape->startTermSeg->ctlPtAdj->startCtlPtX == 171);
        CHECK(shape->startTermSeg->ctlPtAdj->startCtlPtY == -72);
        CHECK(shape->startTermSeg->endPointAdj->active);
        CHECK(shape->fullCtlPtAdj->active);
        CHECK(result.report.findField<Shape>("startTermSeg.endPoint.eduPosition", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin
              == ValueOrigin::LegacyMus);
    }
}

TEST_CASE("SmartShape comparator zero does not create a shape", "[class][smart-shape]")
{
    const std::vector<std::array<std::int16_t, 6>> payload{{3, static_cast<std::int16_t>(0x8200), 0, 1, 504, 3368}, {0, 0, -184, 0x4000, 0, 0},
        {0, 0, 0, 0, 0, 0}, {1, 504, 4096, 0, 0, -184}, {0x4000, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}};
    std::vector<SyntheticRow> rows;
    for (const auto cmper : {0, 36}) {
        for (const auto& words : payload) {
            rows.push_back({static_cast<std::uint16_t>(cmper), "Sx", words});
        }
    }
    const auto parsed = makeContainer(rows);
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
    finale_mus_reader::runDeferredChecks(pending);

    CHECK_FALSE(document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(0)));
    const auto valid = document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(36));
    REQUIRE(valid);
    CHECK(valid->shapeType == Shape::ShapeType::Crescendo);
    CHECK(valid->startTermSeg->endPoint->measId == 504);
    CHECK(valid->startTermSeg->endPoint->staffId == 1);
    CHECK(valid->startTermSeg->endPoint->eduPosition == 3368);
    CHECK(valid->endTermSeg->endPoint->eduPosition == 4096);
    CHECK(document->getOthers()->getAllSources<Shape>().size() == 1);
}

TEST_CASE("Entry-based SmartShapes naming an entry the file does not store are skipped with their objects", "[class][smart-shape]")
{
    // Seven rows: shape type and flags, then the start endpoint at words 3-6 and the end at 18-21.
    const auto shapeRows = [](std::uint16_t cmper, std::int16_t highFlags, std::int16_t start, std::int16_t end) {
        const std::vector<std::array<std::int16_t, 6>> payload{{static_cast<std::int16_t>(Shape::ShapeType::SlurDown), 0, highFlags, 1, 1, start},
            {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {1, 1, end, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}};
        std::vector<SyntheticRow> rows;
        for (const auto& words : payload) {
            rows.push_back({cmper, "Sx", words});
        }
        return rows;
    };
    constexpr std::int16_t entryBased = 0x4000;
    std::vector<SyntheticRow> rows;
    for (const auto& [cmper, highFlags, end] : {std::tuple{36, entryBased, 2}, std::tuple{37, entryBased, 9}, std::tuple{38, std::int16_t{0}, 9}}) {
        const auto shape = shapeRows(static_cast<std::uint16_t>(cmper), highFlags, 1, static_cast<std::int16_t>(end));
        rows.insert(rows.end(), shape.begin(), shape.end());
    }
    // Measure 1 assigns all three shapes, the skipped one first.
    for (const auto shapeNum : std::array<std::int16_t, 3>{37, 36, 38}) {
        rows.push_back({1, "Mx", {shapeNum, 0, 0, 0, 0, 0}});
    }
    auto parsed = makeContainer(rows);
    // Entry 1 assigns the kept shape and missing entry 9 the skipped one, and each has a center.
    finale_mus_reader::container::DecodedBlock details;
    details.info.type = 0x0002;
    const auto appendDetail = [&](std::uint16_t cmper1, std::uint16_t cmper2, const std::vector<std::int16_t>& words, const char* tag) {
        const auto single = makeDetailContainer(FormatEpoch::UncompressedLegacy, cmper1, cmper2, words, tag);
        details.data.insert(details.data.end(), single.blocks.front().data.begin(), single.blocks.front().data.end());
    };
    appendDetail(0, 1, {36, 0, 0, 0, 0}, "Ex");
    appendDetail(0, 9, {37, 0, 0, 0, 0}, "Ex");
    for (const auto shapeNum : std::array<std::uint16_t, 2>{37, 36}) {
        appendDetail(shapeNum, 1, std::vector<std::int16_t>(15, 0), "Cx");
    }
    details.info.decodedSize = details.data.size();
    parsed.blocks.push_back(std::move(details));
    finale_mus_reader::container::DecodedBlock entries;
    entries.info.type = 0x0003;
    for (const auto number : std::array<std::uint8_t, 2>{1, 2}) {
        std::array<std::uint8_t, 38> entry{};
        entry[3] = number;
        entries.data.insert(entries.data.end(), entry.begin(), entry.end());
    }
    entries.info.decodedSize = entries.data.size();
    parsed.blocks.push_back(std::move(entries));
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
    // The objects that name a shape are imported first: the decision does not depend on order.
    finale_mus_reader::others::importSmartShapeMeasureAssigns(context);
    finale_mus_reader::details::importSmartShapeEntryAssigns(context);
    finale_mus_reader::details::importCenterShapes(context);
    finale_mus_reader::others::importSmartShapes(context);
    finale_mus_reader::runDeferredChecks(pending);

    const auto kept = document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(36));
    REQUIRE(kept);
    CHECK(kept->entryBased);
    CHECK(kept->startTermSeg->endPoint->entryNumber == 1);
    CHECK(kept->endTermSeg->endPoint->entryNumber == 2);
    CHECK_FALSE(document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(37)));
    // A beat-based shape stores positions, not entries.
    const auto beatBased = document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(38));
    REQUIRE(beatBased);
    CHECK(beatBased->endTermSeg->endPoint->eduPosition == 9);

    // Only the skipped shape's objects are dropped; the others close the gap it leaves.
    const auto measureAssigns =
        document->getOthers()->getArray<musx::dom::others::SmartShapeMeasureAssign>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1});
    REQUIRE(measureAssigns.size() == 2);
    CHECK(measureAssigns[0]->shapeNum == 36);
    CHECK(measureAssigns[0]->getInci() == 0);
    CHECK(measureAssigns[1]->shapeNum == 38);
    CHECK(measureAssigns[1]->getInci() == 1);
    const auto entryAssigns = document->getDetails()->getArray<musx::dom::details::SmartShapeEntryAssign>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(entryAssigns.size() == 1);
    CHECK(entryAssigns[0]->shapeNum == 36);
    CHECK(document->getDetails()->getArray<musx::dom::details::SmartShapeEntryAssign>(musx::dom::SCORE_PARTID, 9).empty());
    CHECK(document->getDetails()->get<musx::dom::details::CenterShape>(musx::dom::SCORE_PARTID, 36, 1));
    CHECK_FALSE(document->getDetails()->get<musx::dom::details::CenterShape>(musx::dom::SCORE_PARTID, 37, 1));
}

TEST_CASE("A SmartShape naming a missing entry is kept as Finale keeps it", "[class][smart-shape]")
{
    // Every shape ends at entry 9, which is not stored, and has a mark there. Shape 40 has a
    // mark on stored entry 1 as well, so Finale keeps it; shape 41 has none. Shape 44 has one on
    // entry 2, but stores staff 2 for an entry that lies in staff 1, so Finale drops it.
    const auto shapeRows = [](std::uint16_t cmper, std::int16_t staff, std::int16_t start) {
        const std::vector<std::array<std::int16_t, 6>> payload{{static_cast<std::int16_t>(Shape::ShapeType::SlurDown), 0, 0x4000, staff, 1, start},
            {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {staff, 1, 9, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}};
        std::vector<SyntheticRow> rows;
        for (const auto& words : payload) {
            rows.push_back({cmper, "Sx", words});
        }
        return rows;
    };
    std::vector<SyntheticRow> rows;
    for (const auto& [cmper, staff, start] :
        {std::tuple{40, 1, 1}, std::tuple{41, 1, 12}, std::tuple{42, 1, 2}, std::tuple{43, 1, 13}, std::tuple{44, 2, 2}}) {
        const auto shape = shapeRows(static_cast<std::uint16_t>(cmper), static_cast<std::int16_t>(staff), static_cast<std::int16_t>(start));
        rows.insert(rows.end(), shape.begin(), shape.end());
    }
    for (const auto shapeNum : std::array<std::int16_t, 2>{41, 40}) {
        rows.push_back({1, "Mx", {shapeNum, 0, 0, 0, 0, 0}});
    }
    auto parsed = makeContainer(rows);
    finale_mus_reader::container::DecodedBlock details;
    details.info.type = 0x0002;
    const auto appendMark = [&](std::uint16_t entry, std::int16_t shapeNum) {
        const auto single = makeDetailContainer(FormatEpoch::UncompressedLegacy, 0, entry, {shapeNum, 0, 0, 0, 0}, "Ex");
        details.data.insert(details.data.end(), single.blocks.front().data.begin(), single.blocks.front().data.end());
    };
    appendMark(1, 40);
    appendMark(9, 40);
    appendMark(12, 41);
    appendMark(9, 41);
    // Finale judges a missing entry by its first mark only: anchored shape 42 comes first on entry
    // 10, so unanchored shape 43 survives with it.
    appendMark(2, 42);
    appendMark(10, 42);
    appendMark(10, 43);
    appendMark(2, 44);
    appendMark(11, 44);
    details.info.decodedSize = details.data.size();
    parsed.blocks.push_back(std::move(details));
    finale_mus_reader::container::DecodedBlock entryBlock;
    entryBlock.info.type = 0x0003;
    for (const auto number : std::array<std::uint8_t, 2>{1, 2}) {
        std::array<std::uint8_t, 38> entry{};
        entry[3] = number;
        entryBlock.data.insert(entryBlock.data.end(), entry.begin(), entry.end());
    }
    entryBlock.info.decodedSize = entryBlock.data.size();
    parsed.blocks.push_back(std::move(entryBlock));
    SourceProfile profile(FormatEpoch::UncompressedLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();

    // Entries 1 and 2 fill the frame of staff 1 in measure 1, as the frame and entry importers
    // leave them.
    using musx::dom::EnigmaBase;
    auto staff = std::make_shared<musx::dom::others::Staff>(document, musx::dom::SCORE_PARTID, EnigmaBase::ShareMode::All, musx::dom::Cmper{1});
    document->getOthers()->add(musx::dom::others::Staff::XmlNodeName, std::move(staff));
    auto measure = std::make_shared<musx::dom::others::Measure>(document, musx::dom::SCORE_PARTID, EnigmaBase::ShareMode::All, musx::dom::Cmper{1});
    measure->integrityCheck(measure);
    measure->beats = 4;
    measure->divBeat = 1024;
    document->getOthers()->add(musx::dom::others::Measure::XmlNodeName, std::move(measure));
    auto hold = std::make_shared<musx::dom::details::GFrameHold>(
        document, musx::dom::SCORE_PARTID, EnigmaBase::ShareMode::All, musx::dom::Cmper{1}, musx::dom::Cmper{1});
    hold->frames[0] = 1;
    document->getDetails()->add(musx::dom::details::GFrameHold::XmlNodeName, std::move(hold));
    auto frame = std::make_shared<musx::dom::others::Frame>(document, musx::dom::SCORE_PARTID, EnigmaBase::ShareMode::All, musx::dom::Cmper{1});
    frame->startEntry = 1;
    frame->endEntry = 2;
    document->getOthers()->add(musx::dom::others::Frame::XmlNodeName, std::move(frame));
    for (const auto& [number, previous, next] : {std::tuple{1, 0, 2}, std::tuple{2, 1, 0}}) {
        auto entry = std::make_shared<musx::dom::Entry>(document, musx::dom::SCORE_PARTID, EnigmaBase::ShareMode::All, number, previous, next);
        entry->duration = 1024;
        entry->isValid = true;
        document->getEntries()->add(number, std::move(entry));
    }

    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importSmartShapes(context);
    finale_mus_reader::others::importSmartShapeMeasureAssigns(context);
    finale_mus_reader::details::importSmartShapeEntryAssigns(context);
    finale_mus_reader::runDeferredChecks(pending);

    const auto kept = document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(40));
    REQUIRE(kept);
    CHECK(kept->endTermSeg->endPoint->entryNumber == 9);
    CHECK_FALSE(document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(41)));
    CHECK(document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(42)));
    CHECK(document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(43)));
    CHECK_FALSE(document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(44)));
    const auto measureAssigns =
        document->getOthers()->getArray<musx::dom::others::SmartShapeMeasureAssign>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1});
    REQUIRE(measureAssigns.size() == 1);
    CHECK(measureAssigns[0]->shapeNum == 40);
    // A kept shape keeps its marks, including the one on its missing entry.
    using EntryMark = musx::dom::details::SmartShapeEntryAssign;
    REQUIRE(document->getDetails()->getArray<EntryMark>(musx::dom::SCORE_PARTID, 1).size() == 1);
    const auto missingEntryMarks = document->getDetails()->getArray<EntryMark>(musx::dom::SCORE_PARTID, 9);
    REQUIRE(missingEntryMarks.size() == 1);
    CHECK(missingEntryMarks[0]->shapeNum == 40);
    CHECK(document->getDetails()->getArray<EntryMark>(musx::dom::SCORE_PARTID, 12).empty());
    const auto entryTwoMarks = document->getDetails()->getArray<EntryMark>(musx::dom::SCORE_PARTID, 2);
    REQUIRE(entryTwoMarks.size() == 1);
    CHECK(entryTwoMarks[0]->shapeNum == 42);
}

TEST_CASE("Only objects whose own shape is skipped name a skipped shape", "[class][smart-shape]")
{
    const finale_mus_reader::SmartShapeKeys skipped{{musx::dom::Cmper{3}, musx::dom::Cmper{40}}, {musx::dom::SCORE_PARTID, musx::dom::Cmper{41}}};
    // A part's skipped copy leaves the score's shape and another part's in place.
    CHECK(finale_mus_reader::namesSkippedSmartShape(skipped, 3, 40));
    CHECK_FALSE(finale_mus_reader::namesSkippedSmartShape(skipped, musx::dom::SCORE_PARTID, 40));
    CHECK_FALSE(finale_mus_reader::namesSkippedSmartShape(skipped, 4, 40));
    // A skipped score shape is gone from every part.
    CHECK(finale_mus_reader::namesSkippedSmartShape(skipped, musx::dom::SCORE_PARTID, 41));
    CHECK(finale_mus_reader::namesSkippedSmartShape(skipped, 3, 41));
    CHECK_FALSE(finale_mus_reader::namesSkippedSmartShape(skipped, 3, 42));
}

TEST_CASE("Only lyric-based SmartShapes read the lyric tail", "[class][smart-shape]")
{
    std::vector<SyntheticRow> rows;
    for (const auto& [cmper, type, lyricFlag, lyricNum] : {std::tuple{1, 15, 0x6000, 81}, std::tuple{2, 38, 0, 1}}) {
        std::array<std::array<std::int16_t, 6>, 8> payload{};
        payload[0] = {static_cast<std::int16_t>(type), static_cast<std::int16_t>(0x8200), static_cast<std::int16_t>(lyricFlag), 1, 1, 0};
        payload[7] = {static_cast<std::int16_t>(lyricNum), static_cast<std::int16_t>(cmper == 1 ? 0x7000 : 1), 0x7665, 0x7665, 0, 0};
        for (const auto& words : payload) {
            rows.push_back({static_cast<std::uint16_t>(cmper), "Sx", words});
        }
    }
    const auto parsed = makeContainer(rows);
    SourceProfile profile(FormatEpoch::DclLegacy);
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
    finale_mus_reader::runDeferredChecks(pending);

    const auto slur = document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    const auto hyphen = document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(2));
    REQUIRE(slur);
    REQUIRE(hyphen);
    CHECK(slur->startLyricNum == 0);
    CHECK(slur->endLyricNum == 0);
    CHECK_FALSE(slur->startLyricType);
    CHECK_FALSE(slur->endLyricType);
    CHECK(hyphen->startLyricNum == 1);
    CHECK(hyphen->endLyricNum == 1);
    CHECK(hyphen->startLyricType == musx::dom::LyricTextType::Verse);
    CHECK(hyphen->endLyricType == musx::dom::LyricTextType::Verse);
    CHECK(report.findField<Shape>("startLyricNum", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyBehavior);
    CHECK(report.findField<Shape>("startLyricNum", musx::dom::SCORE_PARTID, musx::dom::Cmper(2))->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Sources without a verified SmartShape record create no shapes", "[class][smart-shape]")
{
    for (const char* fixture :
        {"evidence/F100/F100-shapexp.mus", "evidence/F2000/F2000-empty.mus", "evidence/F2004/F2004-lyropts-nosmart-wext.mus"}) {
        const auto result = readFixture(fixture);
        CHECK(result.document->getOthers()->getArray<Shape>(musx::dom::SCORE_PARTID).empty());
    }
}

TEST_CASE("Early SmartShapes pair endpoint rows by shape comparator", "[class][smart-shape]")
{
    const auto result = readFixture("evidence/F263/F263-cresc-dim.mus");
    const auto cresc = result.document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    const auto dim = result.document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(2));
    REQUIRE(cresc);
    REQUIRE(dim);
    CHECK(cresc->shapeType == Shape::ShapeType::Crescendo);
    CHECK_FALSE(cresc->makeHorz);
    CHECK_FALSE(cresc->noPushEndStart);
    CHECK(cresc->startTermSeg->endPoint->eduPosition == 0);
    CHECK(cresc->endTermSeg->endPoint->eduPosition == 987);
    CHECK(cresc->startTermSeg->endPointAdj->vertOffset == -148);
    CHECK(cresc->endTermSeg->endPointAdj->vertOffset == -152);
    CHECK(dim->shapeType == Shape::ShapeType::Decrescendo);
    CHECK(dim->startTermSeg->endPoint->staffId == 1);
    CHECK(dim->startTermSeg->endPoint->measId == 1);
    CHECK(dim->startTermSeg->endPoint->eduPosition == 1888);
    CHECK(dim->endTermSeg->endPoint->eduPosition == 3456);
    CHECK(dim->startTermSeg->endPointAdj->vertOffset == -140);
    CHECK(dim->endTermSeg->endPointAdj->vertOffset == -192);
    CHECK(cresc->startTermSeg->endPointAdj->active);
    CHECK(dim->endTermSeg->breakAdj->active);
    CHECK(result.report.findField<Shape>("endTermSeg.endPoint.eduPosition", musx::dom::SCORE_PARTID, musx::dom::Cmper(2))->origin
          == ValueOrigin::LegacyMus);
    const auto* noPush = result.report.findField<Shape>("noPushEndStart", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    const auto* makeHorz = result.report.findField<Shape>("makeHorz", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(noPush);
    REQUIRE(makeHorz);
    CHECK(noPush->origin == ValueOrigin::LegacyMus);
    CHECK(makeHorz->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Early SmartShape offset row populates endpoint and break adjustments", "[class][smart-shape]")
{
    const auto [document, report] = importSyntheticShapes({{1, "sX", {1, 3, 0, 0, 12, flags(0x8200)}}, {1, "eX", {1, 3, 0, 0, 12, flags(0x8200)}},
        {1, "DY", {1, 100, -40, 0, 0, flags(0x8001)}}, {1, "DY", {1, 900, -60, 0, 0, flags(0xc001)}}, {1, "oX", {-11, 6, -22, -19, 12, -30}}});

    const auto shape = document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(shape);
    CHECK(shape->startTermSeg->endPointAdj->horzOffset == -11);
    CHECK(shape->startTermSeg->breakAdj->horzOffset == 6);
    CHECK(shape->startTermSeg->breakAdj->vertOffset == -22);
    CHECK(shape->endTermSeg->endPointAdj->horzOffset == -19);
    CHECK(shape->endTermSeg->breakAdj->horzOffset == 12);
    CHECK(shape->endTermSeg->breakAdj->vertOffset == -30);
    const auto* startOffset = report.findField<Shape>("startTermSeg.endPointAdj.horzOffset", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    const auto* endBreak = report.findField<Shape>("endTermSeg.breakAdj.vertOffset", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(startOffset);
    REQUIRE(endBreak);
    CHECK(startOffset->origin == ValueOrigin::LegacyMus);
    CHECK(startOffset->sourceIdentity == finale_mus_reader::records::packTag("oX"));
    CHECK(startOffset->rawValue == -11);
    CHECK(endBreak->origin == ValueOrigin::LegacyMus);
    CHECK(endBreak->rawValue == -30);
    CHECK(endBreak->decodedOffset == startOffset->decodedOffset + 10);
}

TEST_CASE("Early SmartShape without an offset row keeps active zero adjustments", "[class][smart-shape]")
{
    const auto [document, report] = importSyntheticShapes({{1, "sX", {1, 3, 0, 0, 12, flags(0x8200)}}, {1, "eX", {1, 3, 0, 0, 12, flags(0x8200)}},
        {1, "DY", {1, 100, -40, 0, 0, flags(0x8001)}}, {1, "DY", {1, 900, -60, 0, 0, flags(0xc001)}}});

    const auto shape = document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(shape);
    for (const auto& adjustment :
        {shape->startTermSeg->endPointAdj, shape->startTermSeg->breakAdj, shape->endTermSeg->endPointAdj, shape->endTermSeg->breakAdj}) {
        CHECK(adjustment->active);
    }
    CHECK(shape->startTermSeg->endPointAdj->horzOffset == 0);
    CHECK(shape->endTermSeg->breakAdj->vertOffset == 0);
    for (const char* member : {"startTermSeg.endPointAdj.active", "startTermSeg.breakAdj.active", "startTermSeg.endPointAdj.horzOffset",
             "endTermSeg.breakAdj.vertOffset", "endTermSeg.breakAdj.contextEntCnct"}) {
        INFO(member);
        const auto* field = report.findField<Shape>(member, musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(field);
        CHECK(field->origin == ValueOrigin::LegacyBehavior);
    }
}

TEST_CASE("Early SmartShape endpoints are the first rows of their own kind", "[class][smart-shape]")
{
    // The start measure holds an unrelated expression, a center row, a second shape's start, and then
    // this shape's start; the end measure holds a start-kind row before the end row.
    const auto [document, report] = importSyntheticShapes(
        {{2, "sX", {3, 1, 0, 0, 10, flags(0x8200)}}, {2, "eX", {1, 1, 0, 0, 20, flags(0x8200)}}, {1, "DY", {2, 10, -1, 0, 0, flags(0x0002)}},
            {1, "DY", {2, 20, -2, 0, 0, flags(0xa002)}}, {1, "DY", {7, 30, -3, 0, 0, flags(0x8002)}}, {1, "DY", {2, 40, -4, 0, 0, flags(0x8002)}},
            {1, "DY", {2, 50, -5, 0, 0, flags(0x8002)}}, {3, "DY", {2, 60, -6, 0, 0, flags(0x8003)}}, {3, "DY", {2, 70, -7, 0, 0, flags(0xc003)}}});

    const auto shape = document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(2));
    REQUIRE(shape);
    CHECK(shape->shapeType == Shape::ShapeType::SlurUp);
    CHECK(shape->startTermSeg->endPoint->measId == 1);
    CHECK(shape->startTermSeg->endPoint->eduPosition == 40);
    CHECK(shape->startTermSeg->endPointAdj->vertOffset == -4);
    CHECK(shape->endTermSeg->endPoint->measId == 3);
    CHECK(shape->endTermSeg->endPoint->eduPosition == 70);
    // Both endpoints take the start row's staff.
    CHECK(shape->startTermSeg->endPoint->staffId == 2);
    CHECK(shape->endTermSeg->endPoint->staffId == 2);
    CHECK(report.findField<Shape>("endTermSeg.endPoint.staffId", musx::dom::SCORE_PARTID, musx::dom::Cmper(2))->rawValue == 2);
    // An up slur's control points lie above it.
    CHECK(shape->fullCtlPtAdj->startCtlPtY == 34);
    CHECK(shape->endTermSeg->ctlPtAdj->endCtlPtY == 44);
}

TEST_CASE("Early SmartShapes need a start no later than their end", "[class][smart-shape]")
{
    for (const auto& [start, end] : {std::pair{3, 2}, std::pair{1, 0}}) {
        INFO(start << " to " << end);
        const auto [document, report] = importSyntheticShapes({{1, "sX", {static_cast<std::int16_t>(end), 0, 0, 0, 10, flags(0x8200)}},
            {1, "eX", {static_cast<std::int16_t>(start), 0, 0, 0, 10, flags(0x8200)}},
            {static_cast<std::uint16_t>(start), "DY", {1, 0, 0, 0, 0, flags(0x8001)}},
            {static_cast<std::uint16_t>(end), "DY", {1, 0, 0, 0, 0, flags(0xc001)}}});
        CHECK(document->getOthers()->getArray<Shape>(musx::dom::SCORE_PARTID).empty());
    }
}

TEST_CASE("Coda SmartShape endpoints pair by staff", "[class][smart-shape]")
{
    const std::vector<SyntheticRow> markers{{1, "sX", {2, 2, 0, 0, 12, flags(0x8200)}}, {1, "eX", {1, 2, 0, 0, 12, flags(0x8200)}}};
    SECTION("The first start with an end on its staff pairs")
    {
        // The staff-2 start has no end on its staff, and the unmarked row is an expression.
        auto rows = markers;
        rows.insert(rows.end(),
            {{1, "DY", {1, 10, -1, 0, 0, flags(0xc002)}}, {1, "DY", {1, 20, -2, 0, 0, flags(0x8001)}}, {1, "DY", {1, 30, -3, 0, 0, flags(0xc001)}},
                {2, "DY", {1, 40, -4, 0, 0, flags(0x4001)}}, {2, "DY", {1, 50, -5, 0, 0, flags(0x4001)}}});
        const auto [document, report] = importSyntheticShapes(rows, FormatEpoch::CodaBanner);
        const auto shape = document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(shape);
        CHECK(shape->startTermSeg->endPoint->staffId == 1);
        CHECK(shape->startTermSeg->endPoint->eduPosition == 30);
        CHECK(shape->endTermSeg->endPoint->eduPosition == 40);
    }
    SECTION("A center row on the start's staff leaves no end")
    {
        auto rows = markers;
        rows.insert(rows.end(),
            {{1, "DY", {1, 20, -2, 0, 0, flags(0xc001)}}, {2, "DY", {1, 40, -4, 0, 0, flags(0x5001)}}, {2, "DY", {1, 50, -5, 0, 0, flags(0x4001)}}});
        const auto [document, report] = importSyntheticShapes(rows, FormatEpoch::CodaBanner);
        CHECK(document->getOthers()->getArray<Shape>(musx::dom::SCORE_PARTID).empty());
    }
    SECTION("No end on the start's staff leaves no shape")
    {
        auto rows = markers;
        rows.insert(rows.end(), {{1, "DY", {1, 20, -2, 0, 0, flags(0xc001)}}, {2, "DY", {1, 40, -4, 0, 0, flags(0x4002)}}});
        const auto [document, report] = importSyntheticShapes(rows, FormatEpoch::CodaBanner);
        CHECK(document->getOthers()->getArray<Shape>(musx::dom::SCORE_PARTID).empty());
    }
}

TEST_CASE("Early hairpins open by their stored height", "[class][smart-shape]")
{
    const auto result = readFixture("evidence/F263/F263-cresc-dim.mus");
    const auto cresc = result.document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(cresc);
    for (const auto& adjustment : {cresc->fullCtlPtAdj, cresc->startTermSeg->ctlPtAdj, cresc->endTermSeg->ctlPtAdj}) {
        CHECK(adjustment->active);
        CHECK(adjustment->startCtlPtX == 0);
        CHECK(adjustment->startCtlPtY == 48);
        CHECK(adjustment->endCtlPtX == 0);
        CHECK(adjustment->endCtlPtY == 0);
    }
    const auto* opening = result.report.findField<Shape>("fullCtlPtAdj.startCtlPtY", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    const auto* horizontal = result.report.findField<Shape>("fullCtlPtAdj.startCtlPtX", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(opening);
    REQUIRE(horizontal);
    CHECK(opening->origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(opening->sourceIdentity == finale_mus_reader::records::packTag("sX"));
    CHECK(opening->rawValue == 12);
    CHECK(horizontal->origin == ValueOrigin::LegacyBehavior);
}

TEST_CASE("Early slur curvature becomes control points", "[class][smart-shape]")
{
    for (const auto& [fixture, inset, x] :
        {std::tuple{"evidence/F300/F300-slur.mus", 0, 171}, std::tuple{"evidence/F300/F300-slur-movecps.mus", 64, 626}}) {
        INFO(fixture);
        const auto result = readFixture(fixture);
        const auto shape = result.document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(shape);
        CHECK(shape->fullCtlPtAdj->active);
        CHECK(shape->fullCtlPtAdj->startCtlPtX == x);
        CHECK(shape->fullCtlPtAdj->startCtlPtY == -72);
        CHECK(shape->fullCtlPtAdj->endCtlPtX == -x);
        CHECK(shape->fullCtlPtAdj->endCtlPtY == -72);
        CHECK(shape->fullCtlPtAdj->contextDir == musx::dom::smartshape::DirectionType::None);
        for (const auto& adjustment : {shape->startTermSeg->ctlPtAdj, shape->endTermSeg->ctlPtAdj}) {
            CHECK(adjustment->active);
            CHECK(adjustment->startCtlPtX == 171);
            CHECK(adjustment->endCtlPtX == -171);
            CHECK(adjustment->endCtlPtY == -72);
        }
        const auto* curvature = result.report.findField<Shape>("fullCtlPtAdj.endCtlPtX", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(curvature);
        CHECK(curvature->origin == ValueOrigin::LegacyMusAdjusted);
        CHECK(curvature->sourceIdentity == finale_mus_reader::records::packTag("aX"));
        CHECK(curvature->rawValue == inset);
    }
}

TEST_CASE("Early shapes without stored curvature have inactive control points", "[class][smart-shape]")
{
    const auto [document, report] = importSyntheticShapes({{1, "sX", {1, 5, 0, 0, 12, flags(0x8200)}}, {1, "eX", {1, 5, 0, 0, 12, flags(0x8200)}},
        {1, "DY", {1, 100, -40, 0, 0, flags(0x8001)}}, {1, "DY", {1, 900, -60, 0, 0, flags(0xc001)}}, {1, "aX", {5, 6, 7, 8, 9, 10}}});

    const auto shape = document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(shape);
    CHECK(shape->shapeType == Shape::ShapeType::OctaveUp);
    for (const auto& adjustment : {shape->fullCtlPtAdj, shape->startTermSeg->ctlPtAdj, shape->endTermSeg->ctlPtAdj}) {
        CHECK_FALSE(adjustment->active);
        CHECK(adjustment->startCtlPtX == 0);
        CHECK(adjustment->startCtlPtY == 0);
    }
    const auto* point = report.findField<Shape>("fullCtlPtAdj.startCtlPtX", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(point);
    CHECK(point->origin == ValueOrigin::LegacyBehavior);
}

TEST_CASE("Eight-incidence lyric shapes decode text-kind tags", "[class][smart-shape]")
{
    const auto result = readFixture("evidence/F2005/F2005-breakwexts.mus");
    const auto shape = result.document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(6));
    REQUIRE(shape);
    CHECK(shape->startLyricType == musx::dom::LyricTextType::Verse);
    CHECK(result.report.findField<Shape>("startLyricType", musx::dom::SCORE_PARTID, musx::dom::Cmper(6))->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Lyric shape tags retain their text kinds across saves", "[class][smart-shape]")
{
    for (const char* fixture :
        {"evidence/F2006/F2006-lyric-vcs.mus", "evidence/F2008/F2008-F2006-lyric-vcs.mus", "evidence/F2012/F2012-F2006-lyric-vcs.mus"}) {
        INFO(fixture);
        const auto result = readFixture(fixture);
        for (const auto& [cmper, expectedType] : {std::pair{1, musx::dom::LyricTextType::Verse}, std::pair{8, musx::dom::LyricTextType::Chorus},
                 std::pair{11, musx::dom::LyricTextType::Section}}) {
            INFO(cmper);
            const auto shape = result.document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(cmper));
            REQUIRE(shape);
            CHECK(shape->startLyricType == expectedType);
            CHECK(shape->endLyricType == expectedType);
            CHECK(
                result.report.findField<Shape>("startLyricType", musx::dom::SCORE_PARTID, musx::dom::Cmper(cmper))->origin == ValueOrigin::LegacyMus);
        }
        const auto extension = result.document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(9));
        REQUIRE(extension);
        CHECK(extension->startLyricType == musx::dom::LyricTextType::Chorus);
        CHECK_FALSE(extension->endLyricType.has_value());
    }
}

TEST_CASE("Early slurs retain their own endpoints", "[class][smart-shape]")
{
    for (const auto& [fixture, start, end] :
        {std::tuple{"evidence/F263/F263-slur.mus", 58, 1016}, std::tuple{"evidence/F300/F300-slur.mus", 142, 1138}}) {
        const auto result = readFixture(fixture);
        const auto shape = result.document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(shape);
        CHECK(shape->shapeType == Shape::ShapeType::SlurDown);
        CHECK_FALSE(shape->makeHorz);
        CHECK_FALSE(shape->noPushEndStart);
        CHECK(shape->startTermSeg->endPoint->eduPosition == start);
        CHECK(shape->endTermSeg->endPoint->eduPosition == end);
        CHECK(shape->startTermSeg->endPointAdj->active);
        CHECK(shape->startTermSeg->breakAdj->active);
        CHECK(shape->endTermSeg->endPointAdj->active);
        CHECK(shape->endTermSeg->breakAdj->active);
        const auto* activation = result.report.findField<Shape>("startTermSeg.endPointAdj.active", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(activation);
        CHECK(activation->origin == ValueOrigin::LegacyBehavior);
        const auto* noPush = result.report.findField<Shape>("noPushEndStart", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        const auto* makeHorz = result.report.findField<Shape>("makeHorz", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(noPush);
        REQUIRE(makeHorz);
        CHECK(noPush->origin == ValueOrigin::LegacyMus);
        CHECK(makeHorz->origin == ValueOrigin::LegacyMus);
    }
}

TEST_CASE("Early eX names the start measure and sX the end", "[class][smart-shape]")
{
    const auto result = readFixture("evidence/F263/F263-11bars-8va-3to8.mus");
    const auto shape = result.document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(shape);
    CHECK(shape->shapeType == Shape::ShapeType::OctaveUp);
    CHECK(shape->startTermSeg->endPoint->measId == 3);
    CHECK(shape->endTermSeg->endPoint->measId == 8);
    CHECK(shape->startTermSeg->endPoint->eduPosition == 493);
    CHECK(shape->endTermSeg->endPoint->eduPosition == 1859);
    CHECK(shape->makeHorz);
    CHECK_FALSE(shape->noPushEndStart);
    const auto* startMeasure = result.report.findField<Shape>("startTermSeg.endPoint.measId", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(startMeasure);
    CHECK(startMeasure->sourceIdentity == finale_mus_reader::records::packTag("eX"));
}

TEST_CASE("Early SmartShapes refresh the measure flags their conversion changes", "[class][smart-shape]")
{
    for (const char* fixture : {"evidence/F300/F300-slur.mus", "evidence/F263/F263-cresc-1to2.mus"}) {
        const auto result = readFixture(fixture);
        const auto measure = result.document->getOthers()->get<musx::dom::others::Measure>(musx::dom::SCORE_PARTID, 1);
        REQUIRE(measure);
        CHECK(measure->hasSmartShape);
        const auto* field = result.report.findField<musx::dom::others::Measure>("hasSmartShape", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(field);
        CHECK(field->origin == ValueOrigin::LegacyMusAdjusted);
    }
    // The slur's endpoint rows were the measure's only expression rows.
    const auto result = readFixture("evidence/F300/F300-slur.mus");
    CHECK_FALSE(result.document->getOthers()->get<musx::dom::others::Measure>(musx::dom::SCORE_PARTID, 1)->hasExpression);
    const auto* field = result.report.findField<musx::dom::others::Measure>("hasExpression", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(field);
    CHECK(field->origin == ValueOrigin::LegacyMusAdjusted);
}

} // namespace
} // namespace finale_mus_reader_tests
