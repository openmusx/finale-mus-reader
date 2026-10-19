// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <initializer_list>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

template <typename Target>
void checkPlacement(const musx::dom::DocumentPtr& document, const ImportReport& report, musx::dom::Cmper cmper, bool hidden, bool textRepeat)
{
    const auto target = document->getOthers()->get<Target>(musx::dom::SCORE_PARTID, cmper, musx::dom::Inci(0));
    REQUIRE(target);
    CHECK(target->staffId == 3);
    CHECK(target->measureId == (textRepeat ? 42 : 0));
    CHECK(target->hidden == hidden);
    CHECK(target->x1add == -12);
    CHECK(target->y1add == 24);
    CHECK(target->x2add == (textRepeat ? 0 : -36));
    CHECK(target->y2add == (textRepeat ? 0 : 48));
    const auto* field = report.findField<Target>("staffId", musx::dom::SCORE_PARTID, cmper, musx::dom::Inci(0));
    REQUIRE(field);
    CHECK(field->origin == ValueOrigin::LegacyMus);
    const auto* hiddenField = report.findField<Target>("hidden", musx::dom::SCORE_PARTID, cmper, musx::dom::Inci(0));
    REQUIRE(hiddenField);
    CHECK(hiddenField->origin == (hidden ? ValueOrigin::LegacyMus : ValueOrigin::LegacyBehavior));
}

TEST_CASE("Individual repeat placements decode the shared row in fixed and class records", "[individual-positioning]")
{
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
            const auto parsed =
                epoch == FormatEpoch::ZlibLegacy
                    ? makeClassContainer(
                          {SyntheticClassRow{0x00d1, {3, 1, -12, 24, -36, 48}, 11}, SyntheticClassRow{0x00d2, {3, 1, -12, 24, -36, 48}, 12},
                              SyntheticClassRow{0x00d3, {3, 0, -12, 24, -36, 48}, 13}, SyntheticClassRow{0x00d4, {3, 42, -12, 24, 0, 1}, 14}},
                          byteOrder)
                    : makeContainer({{11, "BI", {3, 1, -12, 24, -36, 48}}, {12, "EI", {3, 1, -12, 24, -36, 48}}, {13, "LI", {3, 0, -12, 24, -36, 48}},
                                        {14, "RI", {3, 42, -12, 24, 0, 1}}},
                          epoch, byteOrder);
            auto session = musx::factory::DocumentFactory::begin();
            auto document = session.getDocument();
            auto profile = SourceProfile(epoch);
            profile.byteOrder = byteOrder;
            if (epoch == FormatEpoch::DclLegacy) {
                profile.version = SourceVersion{.major = 10};
            }
            ImportReport report(epoch);
            const auto index = LegacyRecordIndex::build(parsed);
            auto referenceSession = musx::factory::DocumentFactory::begin();
            const auto reference = std::move(referenceSession).finish();
            finale_mus_reader::PendingReferences pending;
            musx::factory::ConstructionContext construction;
            const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
            finale_mus_reader::others::importRepeatBackIndividualPositioning(context);
            finale_mus_reader::others::importRepeatEndingStartIndividualPositioning(context);
            finale_mus_reader::others::importRepeatEndingTextIndividualPositioning(context);
            finale_mus_reader::others::importTextRepeatIndividualPositioning(context);
            const bool hidden = epoch == FormatEpoch::ZlibLegacy;
            checkPlacement<musx::dom::others::RepeatBackIndividualPositioning>(document, report, 11, hidden, false);
            checkPlacement<musx::dom::others::RepeatEndingStartIndividualPositioning>(document, report, 12, hidden, false);
            checkPlacement<musx::dom::others::RepeatEndingTextIndividualPositioning>(document, report, 13, false, false);
            checkPlacement<musx::dom::others::TextRepeatIndividualPositioning>(document, report, 14, hidden, true);
            CHECK(reportedFieldCount(report) == 4 * musx::dom::others::RepeatIndividualPositioning::xmlMappingArray().size());
        }
    }
}

TEST_CASE("Finale 1.0 imports all four individual repeat placements", "[individual-positioning]")
{
    const auto result = readFixture("evidence/F100/F100-rptindiv.mus");
    using Back = musx::dom::others::RepeatBackIndividualPositioning;
    using Line = musx::dom::others::RepeatEndingStartIndividualPositioning;
    using Text = musx::dom::others::RepeatEndingTextIndividualPositioning;
    using Repeat = musx::dom::others::TextRepeatIndividualPositioning;
    const auto back = result.document->getOthers()->get<Back>(musx::dom::SCORE_PARTID, musx::dom::Cmper(2), musx::dom::Inci(0));
    const auto line = result.document->getOthers()->get<Line>(musx::dom::SCORE_PARTID, musx::dom::Cmper(2), musx::dom::Inci(0));
    const auto text = result.document->getOthers()->get<Text>(musx::dom::SCORE_PARTID, musx::dom::Cmper(2), musx::dom::Inci(0));
    const auto text3 = result.document->getOthers()->get<Text>(musx::dom::SCORE_PARTID, musx::dom::Cmper(3), musx::dom::Inci(0));
    const auto repeat0 = result.document->getOthers()->get<Repeat>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(0));
    const auto repeat1 = result.document->getOthers()->get<Repeat>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(1));
    REQUIRE(back);
    REQUIRE(line);
    REQUIRE(text);
    REQUIRE(text3);
    REQUIRE(repeat0);
    REQUIRE(repeat1);
    CHECK(back->staffId == 2);
    CHECK(back->x1add == -28);
    CHECK(back->y1add == 56);
    CHECK(line->x1add == -4);
    CHECK(line->y1add == 28);
    CHECK(line->x2add == -76);
    CHECK(line->y2add == -4);
    CHECK(text->x1add == 52);
    CHECK(text->y1add == 44);
    CHECK(text3->staffId == 1);
    CHECK(text3->x1add == 68);
    CHECK(text3->y1add == 28);
    CHECK(repeat0->staffId == 1);
    CHECK(repeat0->measureId == 1);
    CHECK(repeat0->x1add == -92);
    CHECK(repeat0->y1add == 128);
    CHECK(repeat1->staffId == 2);
    CHECK(repeat1->measureId == 1);
    CHECK(repeat1->x1add == 48);
    CHECK(repeat1->y1add == 124);
    const auto* field = result.report.findField<Repeat>("x1add", musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(1));
    REQUIRE(field);
    CHECK(field->origin == ValueOrigin::LegacyMus);
    CHECK(field->rawValue == 48);
}

TEST_CASE("Finale 3.7 imports all four uncompressed individual repeat placements", "[individual-positioning]")
{
    const auto result = readFixture("evidence/F372/F372-rptindiv.mus");
    using Back = musx::dom::others::RepeatBackIndividualPositioning;
    using Line = musx::dom::others::RepeatEndingStartIndividualPositioning;
    using Text = musx::dom::others::RepeatEndingTextIndividualPositioning;
    using Repeat = musx::dom::others::TextRepeatIndividualPositioning;
    const auto back = result.document->getOthers()->get<Back>(0, musx::dom::Cmper(2), musx::dom::Inci(0));
    const auto line = result.document->getOthers()->get<Line>(0, musx::dom::Cmper(3), musx::dom::Inci(0));
    const auto text = result.document->getOthers()->get<Text>(0, musx::dom::Cmper(2), musx::dom::Inci(0));
    const auto text3 = result.document->getOthers()->get<Text>(0, musx::dom::Cmper(3), musx::dom::Inci(0));
    const auto repeat0 = result.document->getOthers()->get<Repeat>(0, musx::dom::Cmper(1), musx::dom::Inci(0));
    const auto repeat1 = result.document->getOthers()->get<Repeat>(0, musx::dom::Cmper(1), musx::dom::Inci(1));
    REQUIRE(back);
    REQUIRE(line);
    REQUIRE(text);
    REQUIRE(text3);
    REQUIRE(repeat0);
    REQUIRE(repeat1);
    CHECK(result.document->getOthers()->getArray<Line>(0).size() == 1);
    CHECK(back->staffId == 2);
    CHECK(back->x1add == -92);
    CHECK(back->y1add == 4);
    CHECK(back->x2add == 96);
    CHECK(back->y2add == -12);
    CHECK_FALSE(back->hidden);
    CHECK(line->staffId == 2);
    CHECK(line->x1add == -64);
    CHECK(line->y1add == 32);
    CHECK(line->x2add == 4);
    CHECK(line->y2add == -20);
    CHECK_FALSE(line->hidden);
    CHECK(text->staffId == 2);
    CHECK(text->x1add == 76);
    CHECK(text->y1add == 40);
    CHECK_FALSE(text->hidden);
    CHECK(text3->staffId == 2);
    CHECK(text3->x1add == 28);
    CHECK(text3->y1add == -60);
    CHECK(repeat0->staffId == 1);
    CHECK(repeat0->measureId == 1);
    CHECK(repeat0->x1add == 72);
    CHECK(repeat1->staffId == 2);
    CHECK(repeat1->measureId == 1);
    CHECK(repeat1->x1add == 20);
    CHECK(repeat1->y1add == -196);
    CHECK_FALSE(repeat1->hidden);
    const auto* backPosition = result.report.findField<Back>("x1add", 0, musx::dom::Cmper(2), musx::dom::Inci(0));
    REQUIRE(backPosition);
    CHECK(backPosition->origin == ValueOrigin::LegacyMus);
    CHECK(backPosition->rawValue == -92);
    const auto* linePosition = result.report.findField<Line>("x1add", 0, musx::dom::Cmper(3), musx::dom::Inci(0));
    REQUIRE(linePosition);
    CHECK(linePosition->origin == ValueOrigin::LegacyMus);
    CHECK(linePosition->rawValue == -64);
}

TEST_CASE("Finale 2002 preserves every individual repeat placement and signed offsets", "[individual-positioning]")
{
    const auto result = readFixture("evidence/F2002/F2002-rptindiv.mus");
    struct Expected
    {
        int cmper;
        int inci;
        int staff;
        int measure;
        int x1;
        int y1;
        int x2;
        int y2;
    };
    const auto check = [&]<typename Target>(std::initializer_list<Expected> expected) {
        for (const auto& row : expected) {
            const auto cmper = musx::dom::Cmper(row.cmper);
            const auto inci = musx::dom::Inci(row.inci);
            const auto target = result.document->getOthers()->get<Target>(musx::dom::SCORE_PARTID, cmper, inci);
            REQUIRE(target);
            CHECK(target->staffId == row.staff);
            CHECK(target->measureId == row.measure);
            CHECK_FALSE(target->hidden);
            CHECK(target->x1add == row.x1);
            CHECK(target->y1add == row.y1);
            CHECK(target->x2add == row.x2);
            CHECK(target->y2add == row.y2);
            const auto* hidden = result.report.findField<Target>("hidden", musx::dom::SCORE_PARTID, cmper, inci);
            REQUIRE(hidden);
            CHECK(hidden->origin == ValueOrigin::LegacyBehavior);
        }
        CHECK(result.document->getOthers()->getArray<Target>(musx::dom::SCORE_PARTID).size() == expected.size());
    };
    check.template operator()<musx::dom::others::RepeatBackIndividualPositioning>({{2, 0, 1, 0, 0, 0, 132, -54}, {2, 1, 2, 0, -48, 36, 54, -24}});
    check.template operator()<musx::dom::others::RepeatEndingStartIndividualPositioning>(
        {{2, 0, 1, 0, 0, 0, -188, -74}, {2, 1, 2, 0, 0, 0, -120, 0}, {3, 0, 1, 0, 0, 0, 0, -70}, {3, 1, 2, 0, 0, 0, 0, 0}});
    check.template operator()<musx::dom::others::RepeatEndingTextIndividualPositioning>(
        {{2, 0, 1, 0, 6, 0, 0, -108}, {2, 1, 2, 0, 0, 0, 0, -64}, {3, 0, 2, 0, 102, 34, 0, -72}});
    check.template operator()<musx::dom::others::TextRepeatIndividualPositioning>({{1, 0, 2, 1, 228, -194, 0, 0}});
}

TEST_CASE("Finale 2005 retains the older individual placement fields without hidden flags", "[individual-positioning]")
{
    const auto result = readFixture("evidence/F2005/F2005-rptindiv.mus");
    using Back = musx::dom::others::RepeatBackIndividualPositioning;
    using Line = musx::dom::others::RepeatEndingStartIndividualPositioning;
    using Text = musx::dom::others::RepeatEndingTextIndividualPositioning;
    using Repeat = musx::dom::others::TextRepeatIndividualPositioning;
    CHECK(result.document->getOthers()->getArray<Back>(musx::dom::SCORE_PARTID).size() == 3);
    CHECK(result.document->getOthers()->getArray<Line>(musx::dom::SCORE_PARTID).size() == 4);
    CHECK(result.document->getOthers()->getArray<Text>(musx::dom::SCORE_PARTID).size() == 4);
    CHECK(result.document->getOthers()->getArray<Repeat>(musx::dom::SCORE_PARTID).size() == 2);
    const auto back = result.document->getOthers()->get<Back>(musx::dom::SCORE_PARTID, musx::dom::Cmper(3), musx::dom::Inci(0));
    const auto line = result.document->getOthers()->get<Line>(musx::dom::SCORE_PARTID, musx::dom::Cmper(3), musx::dom::Inci(1));
    const auto text = result.document->getOthers()->get<Text>(musx::dom::SCORE_PARTID, musx::dom::Cmper(2), musx::dom::Inci(1));
    const auto repeat = result.document->getOthers()->get<Repeat>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(1));
    REQUIRE(back);
    REQUIRE(line);
    REQUIRE(text);
    REQUIRE(repeat);
    CHECK(back->staffId == 2);
    CHECK_FALSE(back->hidden);
    CHECK(back->x2add == 32);
    CHECK(back->y2add == 72);
    CHECK_FALSE(line->hidden);
    CHECK(line->x1add == -32);
    CHECK(line->y1add == 12);
    CHECK(text->y1add == -16);
    CHECK(text->y2add == -28);
    CHECK(repeat->measureId == 1);
    CHECK_FALSE(repeat->hidden);
    CHECK(repeat->x1add == -44);
    const auto* hidden = result.report.findField<Back>("hidden", musx::dom::SCORE_PARTID, musx::dom::Cmper(3), musx::dom::Inci(0));
    REQUIRE(hidden);
    CHECK(hidden->origin == ValueOrigin::LegacyBehavior);
}

TEST_CASE("Finale 2008 imports hidden and per-part individual placements", "[individual-positioning]")
{
    const auto result = readFixture("evidence/F2008/F2008-rptindiv.mus");
    using Back = musx::dom::others::RepeatBackIndividualPositioning;
    using Line = musx::dom::others::RepeatEndingStartIndividualPositioning;
    using Text = musx::dom::others::RepeatEndingTextIndividualPositioning;
    using Repeat = musx::dom::others::TextRepeatIndividualPositioning;
    for (const auto partId : std::initializer_list<musx::dom::Cmper>{0, 1, 2}) {
        CHECK(result.document->getOthers()->getArray<Back>(partId).size() == 2);
        CHECK(result.document->getOthers()->getArray<Line>(partId).size() == 4);
        CHECK(result.document->getOthers()->getArray<Text>(partId).size() == 4);
        CHECK(result.document->getOthers()->getArray<Repeat>(partId).size() == 2);
    }
    const auto scoreBack = result.document->getOthers()->get<Back>(0, musx::dom::Cmper(2), musx::dom::Inci(0));
    const auto scoreBackSecond = result.document->getOthers()->get<Back>(0, musx::dom::Cmper(2), musx::dom::Inci(1));
    const auto partTwoBack = result.document->getOthers()->get<Back>(2, musx::dom::Cmper(2), musx::dom::Inci(1));
    const auto scoreLine = result.document->getOthers()->get<Line>(0, musx::dom::Cmper(2), musx::dom::Inci(0));
    const auto partOneLine = result.document->getOthers()->get<Line>(1, musx::dom::Cmper(2), musx::dom::Inci(0));
    const auto partTwoLine = result.document->getOthers()->get<Line>(2, musx::dom::Cmper(2), musx::dom::Inci(1));
    const auto partOneHiddenLine = result.document->getOthers()->get<Line>(1, musx::dom::Cmper(3), musx::dom::Inci(1));
    const auto scoreText = result.document->getOthers()->get<Text>(0, musx::dom::Cmper(3), musx::dom::Inci(0));
    const auto partOneText = result.document->getOthers()->get<Text>(1, musx::dom::Cmper(3), musx::dom::Inci(1));
    const auto partTwoText = result.document->getOthers()->get<Text>(2, musx::dom::Cmper(2), musx::dom::Inci(1));
    const auto scoreRepeat = result.document->getOthers()->get<Repeat>(0, musx::dom::Cmper(1), musx::dom::Inci(0));
    const auto partTwoRepeat = result.document->getOthers()->get<Repeat>(2, musx::dom::Cmper(1), musx::dom::Inci(0));
    REQUIRE(scoreBack);
    REQUIRE(scoreBackSecond);
    REQUIRE(partTwoBack);
    REQUIRE(scoreLine);
    REQUIRE(partOneLine);
    REQUIRE(partTwoLine);
    REQUIRE(partOneHiddenLine);
    REQUIRE(scoreText);
    REQUIRE(partOneText);
    REQUIRE(partTwoText);
    REQUIRE(scoreRepeat);
    REQUIRE(partTwoRepeat);
    CHECK(scoreBack->staffId == 2);
    CHECK(scoreBack->x1add == -72);
    CHECK(scoreBackSecond->staffId == 1);
    CHECK(scoreBackSecond->y1add == 98);
    CHECK(scoreBackSecond->x2add == 161);
    CHECK(scoreBackSecond->y2add == 21);
    CHECK(scoreBackSecond->hidden);
    CHECK(scoreLine->hidden);
    CHECK(scoreLine->x2add == -144);
    CHECK(partOneLine->hidden);
    CHECK(partOneLine->x1add == -56);
    CHECK(partOneLine->y1add == 28);
    CHECK(partOneLine->x2add == -89);
    CHECK(partOneLine->getShareMode() == musx::dom::EnigmaBase::ShareMode::Partial);
    CHECK_FALSE(partTwoLine->hidden);
    CHECK(partTwoLine->y1add == 17);
    CHECK(partOneHiddenLine->hidden);
    CHECK(scoreText->y2add == -35);
    CHECK_FALSE(scoreText->hidden);
    CHECK(partOneText->y2add == 120);
    CHECK_FALSE(partOneText->hidden);
    CHECK(partTwoText->y1add == -17);
    CHECK(partOneText->getShareMode() == musx::dom::EnigmaBase::ShareMode::Partial);
    CHECK(scoreRepeat->x1add == 104);
    CHECK(scoreRepeat->y1add == 4);
    CHECK(partTwoRepeat->x1add == 22);
    CHECK(partTwoRepeat->y1add == -12);
    CHECK(partTwoRepeat->hidden);
    CHECK(partTwoRepeat->getShareMode() == musx::dom::EnigmaBase::ShareMode::Partial);
    const auto* hidden = result.report.findField<Line>("hidden", 1, musx::dom::Cmper(3), musx::dom::Inci(1));
    REQUIRE(hidden);
    CHECK(hidden->origin == ValueOrigin::LegacyMus);
    CHECK(hidden->rawValue == 1);
    const auto* position = result.report.findField<Repeat>("x1add", 2, musx::dom::Cmper(1), musx::dom::Inci(0));
    REQUIRE(position);
    CHECK(position->origin == ValueOrigin::LegacyMus);
    CHECK(position->rawValue == 22);
    const auto* repeatHidden = result.report.findField<Repeat>("hidden", 2, musx::dom::Cmper(1), musx::dom::Inci(0));
    REQUIRE(repeatHidden);
    CHECK(repeatHidden->origin == ValueOrigin::LegacyMus);
    CHECK(repeatHidden->rawValue == 1);
    const auto* backHidden = result.report.findField<Back>("hidden", 0, musx::dom::Cmper(2), musx::dom::Inci(1));
    REQUIRE(backHidden);
    CHECK(backHidden->origin == ValueOrigin::LegacyMus);
    CHECK(backHidden->rawValue == 1);
    const auto* textHidden = result.report.findField<Text>("hidden", 1, musx::dom::Cmper(3), musx::dom::Inci(1));
    REQUIRE(textHidden);
    CHECK(textHidden->origin == ValueOrigin::LegacyBehavior);
}

TEST_CASE("A DCL placement with no source version uses the pre-class hidden behavior", "[individual-positioning]")
{
    const auto parsed = makeContainer({{11, "BI", {3, 1, -12, 24, -36, 48}}}, FormatEpoch::DclLegacy);
    auto session = musx::factory::DocumentFactory::begin();
    auto document = session.getDocument();
    auto profile = SourceProfile(FormatEpoch::DclLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importRepeatBackIndividualPositioning(context);
    const auto target = document->getOthers()->get<musx::dom::others::RepeatBackIndividualPositioning>(
        musx::dom::SCORE_PARTID, musx::dom::Cmper(11), musx::dom::Inci(0));
    REQUIRE(target);
    CHECK(target->x1add == -12);
    const auto* hiddenField = report.findField<musx::dom::others::RepeatBackIndividualPositioning>(
        "hidden", musx::dom::SCORE_PARTID, musx::dom::Cmper(11), musx::dom::Inci(0));
    REQUIRE(hiddenField);
    CHECK_FALSE(target->hidden);
    CHECK(hiddenField->origin == ValueOrigin::LegacyBehavior);
}

TEST_CASE("A Finale 2012 ending placement retains its staff identity", "[individual-positioning]")
{
    const auto result = readFixture("evidence/F2012/F2012-rptstart-hidden.mus");
    const auto target = result.document->getOthers()->get<musx::dom::others::RepeatEndingStartIndividualPositioning>(
        musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(0));
    REQUIRE(target);
    CHECK(target->staffId == 1);
    CHECK(target->measureId == 0);
}

} // namespace
} // namespace finale_mus_reader_tests
