// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"
#include "staff_style_assign_test_support.h"

#include <tuple>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

using StaffStyleAssign = musx::dom::others::StaffStyleAssign;
using Staff = musx::dom::others::Staff;
using StaffStyle = musx::dom::others::StaffStyle;

void checkStaffStyleAssign(const StaffStyleAssignImportResult& result)
{
    const auto assign = result.document->getOthers()->get<StaffStyleAssign>(musx::dom::SCORE_PARTID, musx::dom::Cmper{4}, musx::dom::Inci{0});
    REQUIRE(assign);
    CHECK(assign->styleId == 7);
    CHECK(assign->startMeas == 3);
    CHECK(assign->startEdu == 0x00010203);
    CHECK(assign->endMeas == 8);
    CHECK(assign->endEdu == (std::numeric_limits<musx::dom::Edu>::max)());
    const auto staff = result.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, musx::dom::Cmper{4});
    REQUIRE(staff);
    CHECK(staff->hasStyles);
    CHECK(reportedFieldCount(result.report) == StaffStyleAssign::xmlMappingArray().size() + 1);
    for (const auto* member : {"styleId", "startMeas", "startEdu", "endMeas", "endEdu"}) {
        const auto* field = result.report.findField<StaffStyleAssign>(member, musx::dom::SCORE_PARTID, musx::dom::Cmper{4}, musx::dom::Inci{0});
        REQUIRE(field);
        CHECK(field->origin == ValueOrigin::LegacyMus);
    }
    const auto* hasStyles = result.report.findField<Staff>("hasStyles", musx::dom::SCORE_PARTID, musx::dom::Cmper{4});
    REQUIRE(hasStyles);
    CHECK(hasStyles->origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(result.report.staffStyleAssignmentAuditComplete);
    const auto* audit = result.report.findStaffStyleAssignmentAudit(musx::dom::SCORE_PARTID, musx::dom::Cmper{4});
    REQUIRE(audit);
    CHECK(audit->expectedAssignments == 1);
    CHECK(audit->constructedAssignments == 1);
    CHECK_FALSE(audit->malformedSource);
}

TEST_CASE("StaffStyleAssign recovers fixed-row and class-record layouts")
{
    const std::vector<SyntheticRow> bigEndianRows{
        {4, "Sy", {7, 0, 0, 0, 0, 0}},
        {4, "Sy", {3, 1, 0x0203, 8, 0x7fff, -1}},
    };
    checkStaffStyleAssign(importStaffStyleAssigns(makeContainer(bigEndianRows, FormatEpoch::UncompressedLegacy, ByteOrder::BigEndian)));

    const std::vector<SyntheticRow> littleEndianRows{
        {4, "Sy", {7, 0, 0, 0, 0, 0}},
        {4, "Sy", {3, 0x0203, 1, 8, -1, 0x7fff}},
    };
    checkStaffStyleAssign(importStaffStyleAssigns(makeContainer(littleEndianRows, FormatEpoch::DclLegacy, ByteOrder::LittleEndian)));

    const std::vector<std::int16_t> classWords{7, 0, 0, 0, 0, 0, 3, 0x0203, 1, 8, -1, 0x7fff};
    checkStaffStyleAssign(importStaffStyleAssigns(makeClassContainer(0x00e9, classWords, ByteOrder::LittleEndian, 4)));
}

TEST_CASE("StaffStyleAssign rejects incomplete tuples and refreshes absent "
          "assignment state")
{
    const std::vector<SyntheticRow> rows{{4, "Sy", {7, 0, 0, 0, 0, 0}}};
    const auto incomplete = importStaffStyleAssigns(makeContainer(rows, FormatEpoch::UncompressedLegacy));
    CHECK(incomplete.document->getOthers()->getAllSources<StaffStyleAssign>().empty());
    CHECK(incomplete.report.diagnostics.size() == 1);
    const auto staff = incomplete.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, musx::dom::Cmper{4});
    REQUIRE(staff);
    CHECK_FALSE(staff->hasStyles);
    CHECK(incomplete.report.staffStyleAssignmentAuditComplete);
    const auto* incompleteAudit = incomplete.report.findStaffStyleAssignmentAudit(musx::dom::SCORE_PARTID, musx::dom::Cmper{4});
    REQUIRE(incompleteAudit);
    CHECK(incompleteAudit->expectedAssignments == 0);
    CHECK(incompleteAudit->constructedAssignments == 0);
    CHECK(incompleteAudit->malformedSource);

    const auto absent = importStaffStyleAssigns(makeContainer({}, FormatEpoch::CodaBanner));
    const auto absentStaff = absent.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, musx::dom::Cmper{4});
    REQUIRE(absentStaff);
    CHECK_FALSE(absentStaff->hasStyles);
    const auto* hasStyles = absent.report.findField<Staff>("hasStyles", musx::dom::SCORE_PARTID, musx::dom::Cmper{4});
    REQUIRE(hasStyles);
    CHECK(hasStyles->origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(absent.report.staffStyleAssignmentAuditComplete);
    CHECK_FALSE(absent.report.findStaffStyleAssignmentAudit(musx::dom::SCORE_PARTID, musx::dom::Cmper{4}));
}

TEST_CASE("StaffStyleAssign instrumentation rejects an unreported assignment producer")
{
    try {
        static_cast<void>(importStaffStyleAssigns(makeContainer({}, FormatEpoch::DclLegacy), true));
        FAIL("Expected the assignment completeness audit to fail");
    } catch (const std::logic_error& error) {
        CHECK(std::string_view(error.what()) == "A staff-style assignment importer did not report its source structure");
    }
}

TEST_CASE("Controlled StaffStyleAssign fixtures recover their exact ranges")
{
    const auto finale2000 = readFixture("evidence/F2000/F2000-staffstyle-applied.mus");
    const auto assign2000 = finale2000.document->getOthers()->get<StaffStyleAssign>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{0});
    REQUIRE(assign2000);
    CHECK(assign2000->styleId == 1);
    CHECK(assign2000->startMeas == 1);
    CHECK(assign2000->startEdu == 1024);
    CHECK(assign2000->endMeas == 2);
    CHECK(assign2000->endEdu == 2047);

    const auto finale2003 = readFixture("evidence/F2003/F2003-staffstyle-assigned.mus");
    const auto assign2003 = finale2003.document->getOthers()->get<StaffStyleAssign>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{0});
    REQUIRE(assign2003);
    CHECK(assign2003->startMeas == 1);
    CHECK(assign2003->startEdu == 1024);
    CHECK(assign2003->endMeas == 2);
    CHECK(assign2003->endEdu == 2047);

    const auto finale2011 = readFixture("evidence/F2011/F2011-staffstyle-assigned.mus");
    const auto assign2011 = finale2011.document->getOthers()->get<StaffStyleAssign>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{0});
    REQUIRE(assign2011);
    CHECK(assign2011->startMeas == 1);
    CHECK(assign2011->startEdu == 1024);
    CHECK(assign2011->endMeas == 2);
    CHECK(assign2011->endEdu == 2047);
}

TEST_CASE("Pre-Finale-2000 alternate notation ranges synthesize Staff Styles and assignments")
{
    using Notation = Staff::AlternateNotation;
    struct Expected
    {
        const char* fixture;
        musx::dom::Cmper styleId;
        Notation notation;
        const char* styleName;
        musx::dom::MeasCmper endMeas;
        bool hidesAttachedItems;
    };

    for (const auto& expected : {
             Expected{"evidence/F98/F98-altnotation-partial.mus", musx::dom::Cmper(2), Notation::SlashBeats, "Slash Notation",
                 musx::dom::MeasCmper(3), false},
             Expected{"evidence/F98/F98-altnotation-full.mus", musx::dom::Cmper(5), Notation::TwoBarRepeat, "Two Bar Repeats",
                 musx::dom::MeasCmper(44), true},
         }) {
        const auto result = readFixture(expected.fixture);
        const auto style = result.document->getOthers()->get<StaffStyle>(musx::dom::SCORE_PARTID, expected.styleId);
        REQUIRE(style);
        REQUIRE(style->masks);
        CHECK(style->styleName == expected.styleName);
        CHECK(style->altNotation == expected.notation);
        CHECK(style->copyable);
        CHECK(style->addToMenu);
        CHECK(style->masks->altNotation);
        CHECK(style->masks->hideChords);
        CHECK(style->masks->hideFretboards);
        CHECK(style->hideChords == expected.hidesAttachedItems);
        CHECK(style->hideFretboards == expected.hidesAttachedItems);

        const auto assignment = result.document->getOthers()->get<StaffStyleAssign>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{0});
        REQUIRE(assignment);
        CHECK(assignment->styleId == expected.styleId);
        CHECK(assignment->startMeas == 1);
        CHECK(assignment->startEdu == 0);
        CHECK(assignment->endMeas == expected.endMeas);
        CHECK(assignment->endEdu == (std::numeric_limits<musx::dom::Edu>::max)());
        CHECK(result.document->getOthers()->getArray<StaffStyleAssign>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}).size() == 1);
        CHECK(result.document->getOthers()->getAllSources<StaffStyle>().size() == 6);

        const auto staff = result.document->getOthers()->get<Staff>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1});
        REQUIRE(staff);
        CHECK(staff->hasStyles);

        const auto* styleType = result.report.findField<StaffStyle>("altNotation", musx::dom::SCORE_PARTID, expected.styleId);
        REQUIRE(styleType);
        CHECK(styleType->origin == ValueOrigin::LegacyMusAdjusted);
        CHECK(styleType->sourceIdentity == finale_mus_reader::records::packTag("GF"));
        const auto* assignmentStyle =
            result.report.findField<StaffStyleAssign>("styleId", musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{0});
        REQUIRE(assignmentStyle);
        CHECK(assignmentStyle->origin == ValueOrigin::LegacyMusAdjusted);
        CHECK(assignmentStyle->sourceIdentity == finale_mus_reader::records::packTag("GF"));
    }

    const auto baseline = readFixture("evidence/F98/F98-baseline.mus");
    constexpr std::pair<std::string_view, Notation> canonicalStyles[]{
        {"Normal Notation", Notation::Normal},
        {"Slash Notation", Notation::SlashBeats},
        {"Rhythmic Notation", Notation::Rhythmic},
        {"One Bar Repeats", Notation::OneBarRepeat},
        {"Two Bar Repeats", Notation::TwoBarRepeat},
        {"Blank Notation", Notation::Blank},
    };
    const auto styles = baseline.document->getOthers()->getAllSources<StaffStyle>();
    REQUIRE(styles.size() == std::size(canonicalStyles));
    for (std::size_t index = 0; index < std::size(canonicalStyles); ++index) {
        const auto style = baseline.document->getOthers()->get<StaffStyle>(musx::dom::SCORE_PARTID, static_cast<musx::dom::Cmper>(index + 1));
        REQUIRE(style);
        CHECK(style->styleName == canonicalStyles[index].first);
        CHECK(style->altNotation == canonicalStyles[index].second);
        CHECK(style->instUuid == musx::dom::uuid::BlankStaff);
        CHECK(style->botRepeatDotOff == -5);
        CHECK(style->topRepeatDotOff == -3);
        CHECK(style->dwRestOffset == -4);
        CHECK(style->wRestOffset == -4);
        CHECK(style->hRestOffset == -4);
        CHECK(style->otherRestOffset == -4);
        CHECK(style->lineSpace == 24);
        REQUIRE(style->noteFont);
        CHECK(style->noteFont->fontSize == 24);
        CHECK(style->stemReversal == -4);
        REQUIRE(style->masks);
        CHECK(style->masks->altNotation);
        const auto* styleType =
            baseline.report.findField<StaffStyle>("altNotation", musx::dom::SCORE_PARTID, static_cast<musx::dom::Cmper>(index + 1));
        REQUIRE(styleType);
        CHECK(styleType->origin == ValueOrigin::LegacyBehavior);
        const auto* styleUuid = baseline.report.findField<StaffStyle>("instUuid", musx::dom::SCORE_PARTID, static_cast<musx::dom::Cmper>(index + 1));
        REQUIRE(styleUuid);
        CHECK(styleUuid->origin == ValueOrigin::LegacyBehavior);
        const auto* lineSpace = baseline.report.findField<StaffStyle>("lineSpace", musx::dom::SCORE_PARTID, static_cast<musx::dom::Cmper>(index + 1));
        REQUIRE(lineSpace);
        CHECK(lineSpace->origin == ValueOrigin::Finale27Default);
    }
    CHECK(baseline.document->getOthers()->getAllSources<StaffStyleAssign>().empty());
}

TEST_CASE("Controlled GFrameHolds recover every legacy alternate notation")
{
    constexpr std::tuple<musx::dom::Cmper, musx::dom::MeasCmper, musx::dom::MeasCmper> expectedAssignments[]{
        {musx::dom::Cmper(2), musx::dom::MeasCmper(2), musx::dom::MeasCmper(2)},
        {musx::dom::Cmper(3), musx::dom::MeasCmper(3), musx::dom::MeasCmper(3)},
        {musx::dom::Cmper(4), musx::dom::MeasCmper(4), musx::dom::MeasCmper(4)},
        {musx::dom::Cmper(5), musx::dom::MeasCmper(5), musx::dom::MeasCmper(6)},
        {musx::dom::Cmper(6), musx::dom::MeasCmper(7), musx::dom::MeasCmper(7)},
    };
    constexpr std::pair<musx::dom::Cmper, std::int64_t> expectedRawTypes[]{
        {musx::dom::Cmper(2), std::int64_t(1)},
        {musx::dom::Cmper(3), std::int64_t(2)},
        {musx::dom::Cmper(4), std::int64_t(3)},
        {musx::dom::Cmper(6), std::int64_t(6)},
    };

    for (const auto fixture : {"evidence/F263/F263-altnotation.mus", "evidence/F372/F372-altnotation.mus", "evidence/F97/F97-altnotation.mus",
             "evidence/F98/F98-altnotation.mus"}) {
        const auto result = readFixture(fixture);
        const auto assignments = result.document->getOthers()->getArray<StaffStyleAssign>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1});
        REQUIRE(assignments.size() == std::size(expectedAssignments));
        for (std::size_t index = 0; index < std::size(expectedAssignments); ++index) {
            const auto& [styleId, startMeas, endMeas] = expectedAssignments[index];
            CHECK(assignments[index]->styleId == styleId);
            CHECK(assignments[index]->startMeas == startMeas);
            CHECK(assignments[index]->startEdu == 0);
            CHECK(assignments[index]->endMeas == endMeas);
            CHECK(assignments[index]->endEdu == (std::numeric_limits<musx::dom::Edu>::max)());
        }
        for (const auto& [styleId, rawType] : expectedRawTypes) {
            const auto* field = result.report.findField<StaffStyle>("altNotation", musx::dom::SCORE_PARTID, styleId);
            REQUIRE(field);
            CHECK(field->origin == ValueOrigin::LegacyMusAdjusted);
            CHECK(field->rawValue == rawType);
            CHECK(field->sourceIdentity == finale_mus_reader::records::packTag("GF"));
        }
    }
}

TEST_CASE("Pre-Finale-2000 alternate notation selects presumed GFrameHold flags at Finale 98")
{
    const auto shortGFrameHold = importStaffStyleAssigns(makeDetailContainer(FormatEpoch::CodaBanner, 4, 3, {9, 0, 0, 0, 0x1231}, "GF"));
    const auto shortAssignment =
        shortGFrameHold.document->getOthers()->get<StaffStyleAssign>(musx::dom::SCORE_PARTID, musx::dom::Cmper{4}, musx::dom::Inci{0});
    REQUIRE(shortAssignment);
    CHECK(shortAssignment->styleId == 2);
    CHECK(shortAssignment->startMeas == 3);
    CHECK(shortAssignment->endMeas == 3);

    const auto repeatedCodaGFrameHold =
        importStaffStyleAssigns(makeDetailContainer(FormatEpoch::CodaBanner, 4, 3, {9, 0x1231, 0, 0, 0, 9, 0x1231, 0, 0, 0}, "GF"));
    CHECK(repeatedCodaGFrameHold.document->getOthers()->getAllSources<StaffStyleAssign>().empty());

    const auto finale97GFrameHold =
        importStaffStyleAssigns(makeDetailContainer(FormatEpoch::UncompressedLegacy, 4, 3, {9, 0x1232, 0, 0, 0x1231}, "GF"), false,
            SourceVersion{.major = finale_mus_reader::versions::finale97.major});
    const auto finale97Assignment =
        finale97GFrameHold.document->getOthers()->get<StaffStyleAssign>(musx::dom::SCORE_PARTID, musx::dom::Cmper{4}, musx::dom::Inci{0});
    REQUIRE(finale97Assignment);
    CHECK(finale97Assignment->styleId == 2);

    const auto finale98GFrameHold =
        importStaffStyleAssigns(makeDetailContainer(FormatEpoch::UncompressedLegacy, 4, 3, {9, 0x1231, 0, 0, 0x1232}, "GF"), false,
            SourceVersion{.major = finale_mus_reader::versions::finale98.major});
    const auto finale98Assignment =
        finale98GFrameHold.document->getOthers()->get<StaffStyleAssign>(musx::dom::SCORE_PARTID, musx::dom::Cmper{4}, musx::dom::Inci{0});
    REQUIRE(finale98Assignment);
    CHECK(finale98Assignment->styleId == 2);

    for (const auto& [storedType, styleId, notation] : {
             std::tuple{2, musx::dom::Cmper{3}, Staff::AlternateNotation::Rhythmic},
             std::tuple{3, musx::dom::Cmper{4}, Staff::AlternateNotation::OneBarRepeat},
             std::tuple{6, musx::dom::Cmper{6}, Staff::AlternateNotation::Blank},
         }) {
        const auto imported = importStaffStyleAssigns(
            makeDetailContainer(FormatEpoch::CodaBanner, 4, 3, {9, 0, 0, 0, static_cast<std::int16_t>(0x1230 | storedType)}, "GF"));
        const auto style = imported.document->getOthers()->get<StaffStyle>(musx::dom::SCORE_PARTID, styleId);
        REQUIRE(style);
        CHECK(style->altNotation == notation);
        const auto storedAssignment =
            imported.document->getOthers()->get<StaffStyleAssign>(musx::dom::SCORE_PARTID, musx::dom::Cmper{4}, musx::dom::Inci{0});
        REQUIRE(storedAssignment);
        CHECK(storedAssignment->styleId == styleId);
    }
}
} // namespace
} // namespace finale_mus_reader_tests
