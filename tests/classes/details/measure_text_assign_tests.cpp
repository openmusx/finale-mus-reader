// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <memory>
#include <tuple>
#include <utility>

#include "import/others.h"
#include "import/texts.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using MeasureTextTarget = musx::dom::details::MeasureTextAssign;
using TextBlock = musx::dom::others::TextBlock;

void measureTextImport(const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile,
    const musx::dom::DocumentPtr& document, ImportReport& report, bool importTexts = false)
{
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    if (importTexts) {
        finale_mus_reader::texts::importTexts(context);
        finale_mus_reader::others::importTextBlocks(context);
    }
    finale_mus_reader::details::importMeasureTextAssigns(context);
    finale_mus_reader::runDeferredChecks(pending);
}

const finale_mus_reader::FieldInfo* measureTextField(const ImportReport& report, musx::dom::Cmper staffId, musx::dom::Cmper meas,
    musx::dom::Inci inci, const char* member, std::uint16_t partId = musx::dom::SCORE_PARTID)
{
    return report.findField(finale_mus_reader::instanceKey<MeasureTextTarget>(partId, staffId, inci, meas), member);
}

TEST_CASE("Measure text tuples split the horizontal word by sign from the DCL epoch", "[class][measure-text]")
{
    const std::vector<std::int16_t> words{3, 55, -228, 0, 0, 146, -140, 77, 0, 1};
    for (const auto epoch : {FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            auto session = musx::factory::DocumentFactory::begin();
            const auto document = session.getDocument();
            SourceProfile profile(epoch);
            profile.byteOrder = byteOrder;
            ImportReport report(epoch);
            measureTextImport(epoch == FormatEpoch::ZlibLegacy ? makeDetailClassContainer(4, 23, 0, words, byteOrder, 0x0420)
                                                               : makeDetailContainer(epoch, 4, 23, words, "mt", byteOrder),
                profile, document, report);
            const auto first = document->getDetails()->get<MeasureTextTarget>(musx::dom::SCORE_PARTID, 4, 23, musx::dom::Inci(0));
            const auto second = document->getDetails()->get<MeasureTextTarget>(musx::dom::SCORE_PARTID, 4, 23, musx::dom::Inci(1));
            REQUIRE(first);
            REQUIRE(second);
            CHECK(first->block == 3);
            CHECK(first->xDispEdu == 55);
            CHECK(first->xDispEvpu == 0);
            CHECK(first->yDisp == -228);
            CHECK_FALSE(first->hidden);
            CHECK(second->block == 146);
            CHECK(second->xDispEdu == 0);
            CHECK(second->xDispEvpu == -140);
            CHECK(second->yDisp == 77);
            CHECK(second->hidden);
            CHECK(MeasureTextTarget::xmlMappingArray().size() == 5);
            CHECK(reportedFieldCount(report) == 2 * MeasureTextTarget::xmlMappingArray().size());
            for (const auto* member : {"block", "xDispEdu", "xDispEvpu", "yDisp", "hidden"}) {
                const auto* info = measureTextField(report, 4, 23, 1, member);
                REQUIRE(info);
                CHECK(info->origin == ValueOrigin::LegacyMus);
            }
            CHECK(measureTextField(report, 4, 23, 1, "hidden")->rawValue == 1);
        }
    }
}

TEST_CASE("Measure text before the DCL epoch without its measure keeps its measure-edge offset", "[class][measure-text]")
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    SourceProfile profile(FormatEpoch::UncompressedLegacy);
    profile.version = SourceVersion{.major = 5, .minor = 0};
    profile.byteOrder = ByteOrder::BigEndian;
    ImportReport report(profile.epoch);
    measureTextImport(makeDetailContainer(profile.epoch, 5, 162, {16, 252, -166, 0, 0}, "mt"), profile, document, report);
    const auto assignment = document->getDetails()->get<MeasureTextTarget>(musx::dom::SCORE_PARTID, 5, 162, musx::dom::Inci(0));
    REQUIRE(assignment);
    CHECK(assignment->block == 16);
    CHECK(assignment->xDispEdu == 0);
    CHECK(assignment->xDispEvpu == 252);
    CHECK(assignment->yDisp == -166);
    const auto* evpu = measureTextField(report, 5, 162, 0, "xDispEvpu");
    const auto* edu = measureTextField(report, 5, 162, 0, "xDispEdu");
    REQUIRE(evpu);
    REQUIRE(edu);
    CHECK(evpu->origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(evpu->rawValue == 252);
    CHECK(edu->origin == ValueOrigin::LegacyBehavior);
    CHECK(reportedFieldCount(report) == MeasureTextTarget::xmlMappingArray().size());
}

TEST_CASE("Measure text before the DCL epoch unscales its vertical offset by staff size", "[class][measure-text]")
{
    const auto importAt = [](FormatEpoch epoch, SourceVersion version) {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        auto system = std::make_shared<musx::dom::others::StaffSystem>(
            document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper{28});
        system->startMeas = 121;
        system->endMeas = 126;
        system->hasStaffScaling = true;
        document->getOthers()->add(musx::dom::others::StaffSystem::XmlNodeName, std::move(system));
        auto size = std::make_shared<musx::dom::details::StaffSize>(
            document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper{28}, musx::dom::Cmper{1});
        size->staffPercent = 75;
        document->getDetails()->add(musx::dom::details::StaffSize::XmlNodeName, std::move(size));
        SourceProfile profile(epoch);
        profile.version = version;
        profile.byteOrder = ByteOrder::BigEndian;
        auto report = std::make_unique<ImportReport>(epoch);
        measureTextImport(makeDetailContainer(epoch, 1, 122, {64, 12, 162, 0, 0}, "mt"), profile, document, *report);
        const auto assignment = document->getDetails()->get<MeasureTextTarget>(musx::dom::SCORE_PARTID, 1, 122, musx::dom::Inci(0));
        REQUIRE(assignment);
        const auto* info = measureTextField(*report, 1, 122, 0, "yDisp");
        REQUIRE(info);
        return std::tuple{assignment->yDisp, info->origin, info->rawValue};
    };
    CHECK(importAt(FormatEpoch::UncompressedLegacy, SourceVersion{.major = 5, .minor = 0})
          == std::tuple{musx::dom::Evpu{216}, ValueOrigin::LegacyMusAdjusted, std::int64_t{162}});
    CHECK(importAt(FormatEpoch::UncompressedLegacy, SourceVersion{.major = 3, .minor = 7})
          == std::tuple{musx::dom::Evpu{216}, ValueOrigin::LegacyMusAdjusted, std::int64_t{162}});
    CHECK(importAt(FormatEpoch::UncompressedLegacy, SourceVersion{.major = 3, .minor = 0})
          == std::tuple{musx::dom::Evpu{216}, ValueOrigin::LegacyMusAdjusted, std::int64_t{162}});
    CHECK(importAt(FormatEpoch::CodaBanner, SourceVersion{.major = 2, .minor = 6})
          == std::tuple{musx::dom::Evpu{216}, ValueOrigin::LegacyMusAdjusted, std::int64_t{162}});
    CHECK(importAt(FormatEpoch::DclLegacy, SourceVersion{.major = 6, .minor = 0})
          == std::tuple{musx::dom::Evpu{162}, ValueOrigin::LegacyMus, std::int64_t{162}});
}

TEST_CASE("An incomplete measure text tuple is reported and skipped", "[class][measure-text]")
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    SourceProfile profile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    ImportReport report(profile.epoch);
    measureTextImport(makeDetailClassContainer(1, 2, 0, {2, 10, 20, 0, 0, 3, 4}, profile.byteOrder, 0x0420), profile, document, report);
    CHECK(document->getDetails()->getArray<MeasureTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Cmper{2}).size() == 1);
    CHECK_FALSE(report.diagnostics.empty());
}

TEST_CASE("Coda measure text resolves its PT connector to the numbered block text", "[class][measure-text]")
{
    const auto result = readFixture("evidence/F100/F100-header-meastext.mus");
    const auto assignment = result.document->getDetails()->get<MeasureTextTarget>(musx::dom::SCORE_PARTID, 1, 1, musx::dom::Inci(0));
    REQUIRE(assignment);
    CHECK(assignment->block == 1);
    CHECK(assignment->xDispEdu == 116);
    CHECK(assignment->xDispEvpu == 0);
    CHECK(assignment->yDisp == -200);
    CHECK_FALSE(assignment->hidden);
    const auto block = assignment->getTextBlock();
    REQUIRE(block);
    CHECK(block->textId == 1);
    const auto text = result.document->getTexts()->get<musx::dom::texts::BlockText>(block->textId);
    REQUIRE(text);
    CHECK(text->text.find("Measure Text") != std::string::npos);
    CHECK(field(result, "others.textBlock[1].textId").origin == ValueOrigin::LegacyMus);

    const auto baseline = readFixture("evidence/F100/F100-header.mus");
    CHECK(baseline.document->getDetails()->getArray<MeasureTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Cmper{1}).empty());
}

TEST_CASE("Finale 2001 measure text splits its horizontal position by sign", "[class][measure-text]")
{
    const auto positive = readFixture("evidence/F2001/F2001Win-meastext-pos.mus");
    const auto negative = readFixture("evidence/F2001/F2001Win-meastext-neg.mus");
    const auto right = positive.document->getDetails()->get<MeasureTextTarget>(musx::dom::SCORE_PARTID, 1, 1, musx::dom::Inci(0));
    const auto left = negative.document->getDetails()->get<MeasureTextTarget>(musx::dom::SCORE_PARTID, 1, 1, musx::dom::Inci(0));
    REQUIRE(right);
    REQUIRE(left);
    CHECK(right->block == 1);
    CHECK(right->xDispEdu == 784);
    CHECK(right->xDispEvpu == 0);
    CHECK(right->yDisp == 78);
    CHECK(left->xDispEdu == 0);
    CHECK(left->xDispEvpu == -249);
    CHECK(left->yDisp == 117);
    const auto* origin = measureTextField(positive.report, 1, 1, 0, "xDispEdu");
    REQUIRE(origin);
    CHECK(origin->origin == ValueOrigin::LegacyMus);
    const auto block = right->getTextBlock();
    REQUIRE(block);
    CHECK(block->textId == 1);
}

TEST_CASE("Finale 3.0 measure text moves its offset to the first beat and resolves PT", "[class][measure-text]")
{
    // The music starts 24 EVPU into a 600-EVPU measure: stored 88 EVPU is Edu 455, and stored
    // -340 EVPU is 364 EVPU before the first beat.
    for (const auto& [fixture, stored, edu, evpu, vertical] :
        {std::tuple{"evidence/F300/F300-meatext-pos.mus", 88, 455, 0, -116}, std::tuple{"evidence/F300/F300-meatext-neg.mus", -340, 0, -364, -136}}) {
        const auto result = readFixture(fixture);
        const auto assignment = result.document->getDetails()->get<MeasureTextTarget>(musx::dom::SCORE_PARTID, 1, 1, musx::dom::Inci(0));
        REQUIRE(assignment);
        CHECK(assignment->block == 1);
        CHECK(assignment->xDispEdu == edu);
        CHECK(assignment->xDispEvpu == evpu);
        CHECK(assignment->yDisp == vertical);
        const auto* origin = measureTextField(result.report, 1, 1, 0, edu != 0 ? "xDispEdu" : "xDispEvpu");
        REQUIRE(origin);
        CHECK(origin->origin == ValueOrigin::LegacyMusAdjusted);
        CHECK(origin->rawValue == stored);
        const auto block = assignment->getTextBlock();
        REQUIRE(block);
        CHECK(block->textId == 1);
        CHECK(field(result, "others.textBlock[1].textId").origin == ValueOrigin::LegacyMus);
        const auto text = result.document->getTexts()->get<musx::dom::texts::BlockText>(block->textId);
        REQUIRE(text);
        CHECK(text->text.find("Measure Text") != std::string::npos);
    }
}

TEST_CASE("Finale 3.0 measure text converts its offset through the measure's beat chart", "[class][measure-text]")
{
    // Before the first note, between two notes, and past the barline.
    const auto result = readFixture("evidence/F300/F300-meastext-beatchart.mus");
    for (const auto& [inci, edu, evpu] : {std::tuple{0, 0, -20}, std::tuple{1, 416, 0}, std::tuple{2, 27136, 0}}) {
        CAPTURE(inci);
        const auto assignment = result.document->getDetails()->get<MeasureTextTarget>(musx::dom::SCORE_PARTID, 1, 1, musx::dom::Inci(inci));
        REQUIRE(assignment);
        CHECK(assignment->xDispEdu == edu);
        CHECK(assignment->xDispEvpu == evpu);
    }
}

TEST_CASE("Finale 3.0 measure text on a reduced staff divides its vertical offset", "[class][measure-text]")
{
    const auto result = readFixture("evidence/F300/F300-meatext-pos-staffred.mus");
    const auto assignment = result.document->getDetails()->get<MeasureTextTarget>(musx::dom::SCORE_PARTID, 1, 1, musx::dom::Inci(0));
    REQUIRE(assignment);
    CHECK(assignment->yDisp == -173);
    const auto* info = measureTextField(result.report, 1, 1, 0, "yDisp");
    REQUIRE(info);
    CHECK(info->origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(info->rawValue == -116);
}

TEST_CASE("Finale 1.0 measure text on a reduced staff divides its vertical offset", "[class][measure-text]")
{
    const auto full = readFixture("evidence/F100/F100-meastext2.mus");
    const auto reduced = readFixture("evidence/F100/F100-meastext2-staffred.mus");
    const auto fullAssignment = full.document->getDetails()->get<MeasureTextTarget>(musx::dom::SCORE_PARTID, 1, 1, musx::dom::Inci(0));
    const auto reducedAssignment = reduced.document->getDetails()->get<MeasureTextTarget>(musx::dom::SCORE_PARTID, 1, 1, musx::dom::Inci(0));
    REQUIRE(fullAssignment);
    REQUIRE(reducedAssignment);
    CHECK(fullAssignment->yDisp == -440);
    CHECK(reducedAssignment->yDisp == -772);
    const auto* info = measureTextField(reduced.report, 1, 1, 0, "yDisp");
    REQUIRE(info);
    CHECK(info->origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(info->rawValue == -440);
}

TEST_CASE("Finale 2012 measure text part records overlay unlinked fields", "[class][measure-text]")
{
    using ShareMode = musx::dom::EnigmaBase::ShareMode;
    const auto at = [](const ImportResult& result, musx::dom::Cmper partId, musx::dom::Cmper staffId) {
        return result.document->getDetails()->get<MeasureTextTarget>(partId, staffId, 1, musx::dom::Inci(0));
    };

    const auto linked = readFixture("evidence/F2012/F2012-meastext-part.mus");
    const auto linkedPart = at(linked, 1, 1);
    REQUIRE(linkedPart);
    CHECK(linkedPart->getShareMode() == ShareMode::All);
    CHECK(linkedPart->yDisp == 88);

    // Moving the text in the part unlinks both coordinates; the flag stays linked.
    const auto moved = readFixture("evidence/F2012/F2012-meastext-part-moved.mus");
    const auto movedPart = at(moved, 1, 1);
    REQUIRE(movedPart);
    CHECK(movedPart->getSourcePartId() == 1);
    CHECK(movedPart->getShareMode() == ShareMode::Partial);
    CHECK(movedPart->block == 2);
    CHECK(movedPart->xDispEdu == 51);
    CHECK(movedPart->yDisp == 116);
    CHECK_FALSE(movedPart->hidden);
    CHECK(at(moved, musx::dom::SCORE_PARTID, 1)->yDisp == 88);

    // A linked flag follows the score after the part has unlinked its position.
    const auto scoreHidden = readFixture("evidence/F2012/F2012-meastext-part-moved-score-hidden.mus");
    const auto scoreHiddenPart = at(scoreHidden, 1, 1);
    REQUIRE(scoreHiddenPart);
    CHECK(at(scoreHidden, musx::dom::SCORE_PARTID, 1)->hidden);
    CHECK(scoreHiddenPart->hidden);
    CHECK(scoreHiddenPart->yDisp == 116);

    // A linked position follows the score after the part has unlinked its flag, although the
    // part record still stores the earlier position.
    const auto scoreMoved = readFixture("evidence/F2012/F2012-meastext-part-score-moved.mus");
    const auto scoreMovedScore = at(scoreMoved, musx::dom::SCORE_PARTID, 2);
    const auto scoreMovedPart = at(scoreMoved, 2, 2);
    REQUIRE(scoreMovedScore);
    REQUIRE(scoreMovedPart);
    CHECK(scoreMovedPart->getShareMode() == ShareMode::Partial);
    CHECK_FALSE(scoreMovedScore->hidden);
    CHECK(scoreMovedScore->xDispEvpu == -393);
    CHECK(scoreMovedPart->hidden);
    CHECK(scoreMovedPart->xDispEvpu == -393);
    CHECK(scoreMovedPart->xDispEdu == 0);
    CHECK(scoreMovedPart->yDisp == -212);

    // Zlib sources store the vertical offset unscaled on a reduced staff.
    const auto reduced = readFixture("evidence/F2012/F2012-meastext-part-staff2red.mus");
    const auto reducedScore = at(reduced, musx::dom::SCORE_PARTID, 2);
    REQUIRE(reducedScore);
    CHECK(reducedScore->yDisp == -212);
}

TEST_CASE("HS text blocks do not take a block comparator named by early measure text", "[class][measure-text]")
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    SourceProfile profile(FormatEpoch::UncompressedLegacy);
    profile.version = SourceVersion{.major = 3, .minor = 5};
    profile.byteOrder = ByteOrder::BigEndian;
    ImportReport report(profile.epoch);
    auto parsed = makeContainer(
        {{1, "PT", {7}}, {1, "HS", {7, 11, 268, 0, 0, 0x0082}}, {1, "HT", {0x5469, 0x746c, 0x6500}}, {1, "HT", {}}, {1, "HT", {}}, {1, "HT", {}}},
        profile.epoch);
    parsed.blocks.push_back(makeDetailContainer(profile.epoch, 1, 1, {1, 20, 30, 0, 0}, "MT").blocks.front());
    measureTextImport(parsed, profile, document, report, true);
    const auto assignment = document->getDetails()->get<MeasureTextTarget>(musx::dom::SCORE_PARTID, 1, 1, musx::dom::Inci(0));
    REQUIRE(assignment);
    const auto block = assignment->getTextBlock();
    REQUIRE(block);
    CHECK(block->textId == 7);
    const auto styled = document->getOthers()->get<TextBlock>(musx::dom::SCORE_PARTID, 2);
    REQUIRE(styled);
    CHECK(styled->textId == 1);
}

} // namespace
} // namespace finale_mus_reader_tests
