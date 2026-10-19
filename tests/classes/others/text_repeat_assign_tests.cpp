// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <array>
#include <tuple>
#include <vector>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using Target = musx::dom::others::TextRepeatAssign;

void checkModernFlags(const finale_mus_reader::container::ParsedContainer& parsed, SourceVersion version)
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    profile.version = version;
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importTextRepeatAssigns(context);
    const auto assignment = document->getOthers()->get<Target>(musx::dom::SCORE_PARTID, musx::dom::Cmper(7), musx::dom::Inci(0));
    REQUIRE(assignment);
    CHECK(assignment->horzPos == -36);
    CHECK(assignment->vertPos == 48);
    CHECK(assignment->staffList == 9);
    CHECK(assignment->individualPlacement);
    CHECK(assignment->topStaffOnly);
    CHECK(assignment->resetOnAction);
    CHECK(assignment->jumpOnMultiplePasses);
    CHECK(assignment->hidden);
    CHECK(assignment->jumpIfIgnoring);
    CHECK(assignment->jumpAction == musx::dom::others::RepeatActionType::JumpRelative);
    CHECK(assignment->trigger == musx::dom::others::RepeatTriggerType::OnPass);
    CHECK(report.findField<Target>("jumpAction", musx::dom::SCORE_PARTID, musx::dom::Cmper(7), musx::dom::Inci(0))->rawValue == 2);
}

TEST_CASE("Text repeat assignment uses the 2005 repeat flag layout", "[class][text-repeat-assign]")
{
    const auto rows = std::vector<SyntheticRow>{{7, "RU", {-36, 3, -2, 1, 48, 0x642f}}, {7, "RU", {9, 0, 0, 0, 0, 0}}};
    checkModernFlags(makeContainer(rows, FormatEpoch::DclLegacy), SourceVersion{.major = 10});
    checkModernFlags(makeClassContainer({SyntheticClassRow{0x00f3, {-36, 3, -2, 1, 48, 0x642f, 9, 0, 0, 0, 0, 0}, 7}}, ByteOrder::LittleEndian),
        SourceVersion{.major = 12});
    checkModernFlags(makeClassContainer({SyntheticClassRow{0x00f3, {-36, 3, -2, 1, 48, 0x642f, 9, 0, 0, 0, 0, 0}, 7}}, ByteOrder::BigEndian),
        SourceVersion{.major = 12});
}

TEST_CASE("Text repeat assignments preserve distinct incidences in one measure", "[class][text-repeat-assign]")
{
    const auto parsed = makeContainer(
        {{7, "RU", {0, 1, 3, 5, 0, 0x0400}}, {7, "RU", {0, 2, 4, 5, 0, 0x0600}}, {5, "RS", {12, -24, 1, 12, 0, 0}}}, FormatEpoch::UncompressedLegacy);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    profile.version = SourceVersion{.major = 4};
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importTextRepeatAssigns(context);
    const auto first = document->getOthers()->get<Target>(musx::dom::SCORE_PARTID, musx::dom::Cmper(7), musx::dom::Inci(0));
    const auto second = document->getOthers()->get<Target>(musx::dom::SCORE_PARTID, musx::dom::Cmper(7), musx::dom::Inci(1));
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first->passNumber == 1);
    CHECK(second->passNumber == 2);
    CHECK(second->jumpAction == musx::dom::others::RepeatActionType::JumpToMark);
    CHECK(first->horzPos == 12);
    CHECK(second->vertPos == -24);
    CHECK(report.findField<Target>("passNumber", musx::dom::SCORE_PARTID, musx::dom::Cmper(7), musx::dom::Inci(1))->rawValue == 2);
}

TEST_CASE("Pre-2005 DCL assignment still gets positions from its style", "[class][text-repeat-assign]")
{
    const auto parsed = makeContainer({{7, "RU", {0, 2, 4, 5, 0, 0x0400}}, {5, "RS", {12, -24, 1, 12, 0, 0}}}, FormatEpoch::DclLegacy);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    profile.version = SourceVersion{.major = 9};
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importTextRepeatAssigns(context);
    const auto assignment = document->getOthers()->get<Target>(musx::dom::SCORE_PARTID, musx::dom::Cmper(7), musx::dom::Inci(0));
    REQUIRE(assignment);
    CHECK(assignment->horzPos == 12);
    CHECK(assignment->vertPos == -24);
    CHECK(assignment->passNumber == 2);
    CHECK(assignment->staffList == 0);
    CHECK(report.findField<Target>("staffList", musx::dom::SCORE_PARTID, musx::dom::Cmper(7), musx::dom::Inci(0))->origin
          == ValueOrigin::Finale27Default);
}

TEST_CASE("Text repeat assignment reads early positions from its definition style", "[class][text-repeat-assign]")
{
    for (const auto* path : {"evidence/F100/F100-textrpt.mus", "evidence/F263/F263-F100-textrpt.mus", "evidence/F372/F372-F263-F100-textrpt.mus"}) {
        const auto result = readFixture(path);
        const auto assignment = result.document->getOthers()->get<Target>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(0));
        REQUIRE(assignment);
        CHECK(assignment->horzPos == 120);
        CHECK(assignment->vertPos == -196);
        CHECK(assignment->passNumber == 0);
        CHECK(assignment->targetValue == 0);
        CHECK(assignment->textRepeatId == 1);
        CHECK(assignment->jumpAction == musx::dom::others::RepeatActionType::JumpAbsolute);
        CHECK(assignment->trigger == musx::dom::others::RepeatTriggerType::Always);
        CHECK(assignment->resetOnAction);
        CHECK_FALSE(assignment->autoUpdate);
        CHECK(result.report.findField<Target>("autoUpdate", musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(0))->origin
              == ValueOrigin::LegacyBehavior);
        CHECK(result.report.findField<Target>("horzPos", musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(0))->origin
              == ValueOrigin::LegacyMus);
        CHECK(result.report.findField<Target>("vertPos", musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(0))->rawValue == -196);
    }
}

TEST_CASE("Text repeat assignment reads self-contained class positions", "[class][text-repeat-assign]")
{
    const auto result = readFixture("evidence/F2012/F2012-textrpt.mus");
    const auto assignment = result.document->getOthers()->get<Target>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(0));
    REQUIRE(assignment);
    CHECK(assignment->horzPos == -2);
    CHECK(assignment->vertPos == -212);
    CHECK(assignment->passNumber == 2);
    CHECK(assignment->targetValue == 0);
    CHECK(assignment->textRepeatId == 1);
    CHECK(assignment->staffList == 0);
    CHECK(assignment->topStaffOnly);
    CHECK(assignment->jumpAction == musx::dom::others::RepeatActionType::Stop);
    CHECK(assignment->trigger == musx::dom::others::RepeatTriggerType::OnPass);
    const auto* position = result.report.findField<Target>("horzPos", musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(0));
    REQUIRE(position);
    CHECK(position->origin == ValueOrigin::LegacyMus);
    CHECK(position->rawValue == -2);
    CHECK(result.report.findField<Target>("staffList", musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(0))->origin
          == ValueOrigin::LegacyMus);
}

TEST_CASE("Finale 2005 text repeat spans two fixed rows", "[class][text-repeat-assign]")
{
    const auto result = readFixture("evidence/F2005/F2005-textrpt.mus");
    const auto assignment = result.document->getOthers()->get<Target>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(0));
    REQUIRE(assignment);
    CHECK(assignment->horzPos == 152);
    CHECK(assignment->vertPos == -192);
    CHECK(assignment->passNumber == 2);
    CHECK(assignment->targetValue == 0);
    CHECK(assignment->textRepeatId == 1);
    CHECK(assignment->staffList == 0);
    CHECK(assignment->topStaffOnly);
    CHECK(assignment->jumpAction == musx::dom::others::RepeatActionType::Stop);
    CHECK(assignment->trigger == musx::dom::others::RepeatTriggerType::OnPass);
    CHECK_FALSE(assignment->autoUpdate);
    CHECK_FALSE(result.document->getOthers()->get<Target>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(1)));
    CHECK(result.report.findField<Target>("vertPos", musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(0))->origin
          == ValueOrigin::LegacyMus);
    CHECK(result.report.findField<Target>("staffList", musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(0))->origin
          == ValueOrigin::LegacyMus);
}

TEST_CASE("Self-contained text repeats split twelve-word incidences and retain a six-word final incidence", "[class][text-repeat-assign]")
{
    const auto check = [](const finale_mus_reader::container::ParsedContainer& parsed, SourceVersion version) {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        SourceProfile profile(parsed.formatEpoch);
        profile.byteOrder = parsed.byteOrder;
        profile.version = version;
        ImportReport report(profile.epoch);
        const auto index = LegacyRecordIndex::build(parsed);
        auto referenceSession = musx::factory::DocumentFactory::begin();
        const auto reference = std::move(referenceSession).finish();
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::others::importTextRepeatAssigns(context);
        const auto first = document->getOthers()->get<Target>(musx::dom::SCORE_PARTID, musx::dom::Cmper(7), musx::dom::Inci(0));
        const auto second = document->getOthers()->get<Target>(musx::dom::SCORE_PARTID, musx::dom::Cmper(7), musx::dom::Inci(1));
        REQUIRE(first);
        REQUIRE(second);
        CHECK(first->horzPos == 100);
        CHECK(first->staffList == 9);
        CHECK(second->horzPos == 0);
        CHECK(second->staffList == 0);
        CHECK_FALSE(document->getOthers()->get<Target>(musx::dom::SCORE_PARTID, musx::dom::Cmper(7), musx::dom::Inci(2)));
        const auto* firstList = report.findField<Target>("staffList", musx::dom::SCORE_PARTID, musx::dom::Cmper(7), musx::dom::Inci(0));
        const auto* secondList = report.findField<Target>("staffList", musx::dom::SCORE_PARTID, musx::dom::Cmper(7), musx::dom::Inci(1));
        REQUIRE(firstList);
        REQUIRE(secondList);
        CHECK(firstList->origin == ValueOrigin::LegacyMus);
        CHECK(secondList->origin == ValueOrigin::LegacyBehavior);
    };
    check(makeContainer(
              {{7, "RU", {100, 1, 3, 5, 20, 0x0400}}, {7, "RU", {9, 0, 0, 1, 72, 16}}, {7, "RU", {0, 0, 0, 0, 0, 0}}}, FormatEpoch::DclLegacy),
        SourceVersion{.major = 10});
    check(
        makeClassContainer({SyntheticClassRow{0x00f3, {100, 1, 3, 5, 20, 0x0400, 9, 0, 0, 1, 72, 16, 0, 0, 0, 0, 0, 0}, 7}}, ByteOrder::LittleEndian),
        SourceVersion{.major = 12});
}

TEST_CASE("Compact linked-part text repeat assignments inherit both score incidences", "[class][text-repeat-assign]")
{
    const auto parsed = makeClassContainer({SyntheticClassRow{0x00f3, {357, 0, 245, 1, 49, 0x0010, 0, 0, 0, 1, 72, 16, 0, 0, 0, 0, 0, 0}, 229},
                                               SyntheticClassRow{0x00f3, {509, 0, 245, 1, 37, 0x0010}, 229, 1, true}},
        ByteOrder::LittleEndian);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    profile.version = SourceVersion{.major = 12};
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importTextRepeatAssigns(context);
    const auto first = document->getOthers()->get<Target>(1, musx::dom::Cmper(229), musx::dom::Inci(0));
    const auto second = document->getOthers()->get<Target>(1, musx::dom::Cmper(229), musx::dom::Inci(1));
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first->getShareMode() == musx::dom::EnigmaBase::ShareMode::Partial);
    CHECK(second->getShareMode() == musx::dom::EnigmaBase::ShareMode::Partial);
    CHECK(first->horzPos == 357);
    CHECK(first->vertPos == 49);
    CHECK(second->horzPos == 0);
    const auto* position = report.findField<Target>("horzPos", 1, musx::dom::Cmper(229), musx::dom::Inci(0));
    REQUIRE(position);
    CHECK(position->origin == ValueOrigin::LegacyMus);
    CHECK(position->rawValue == 357);
}

TEST_CASE("Short linked-part text repeat edits overlay selected bits", "[class][text-repeat-assign]")
{
    const auto parsed =
        makeClassContainer({SyntheticClassRow{0x00f3, {357, 0, 245, 1, 49, 0x0010, 0, 0, 0, 1, 72, 16, 0, 0, 0, 0, 0, 0}, 229},
                               SyntheticClassRow{0x00f3, {509, 0, 245, 1, 37, 0x0010}, 229, 1, true, {static_cast<std::uint16_t>(0xffff)}}},
            ByteOrder::LittleEndian);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    profile.version = SourceVersion{.major = 12};
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importTextRepeatAssigns(context);
    const auto first = document->getOthers()->get<Target>(1, musx::dom::Cmper(229), musx::dom::Inci(0));
    const auto second = document->getOthers()->get<Target>(1, musx::dom::Cmper(229), musx::dom::Inci(1));
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first->horzPos == 509);
    CHECK(first->vertPos == 49);
    CHECK(second->horzPos == 0);
    CHECK(report.diagnostics.empty());
}

TEST_CASE("Finale 2008 part text repeat masks preserve distinct incidence edits", "[class][text-repeat-assign]")
{
    const auto result = readFixture("evidence/F2008/F2008-parts-jumps.mus");
    const auto scoreFirst = result.document->getOthers()->get<Target>(0, musx::dom::Cmper(1), musx::dom::Inci(0));
    const auto scoreSecond = result.document->getOthers()->get<Target>(0, musx::dom::Cmper(1), musx::dom::Inci(1));
    const auto scoreThird = result.document->getOthers()->get<Target>(0, musx::dom::Cmper(1), musx::dom::Inci(2));
    const auto partOneFirst = result.document->getOthers()->get<Target>(1, musx::dom::Cmper(1), musx::dom::Inci(0));
    const auto partOneSecond = result.document->getOthers()->get<Target>(1, musx::dom::Cmper(1), musx::dom::Inci(1));
    const auto partOneThird = result.document->getOthers()->get<Target>(1, musx::dom::Cmper(1), musx::dom::Inci(2));
    const auto partTwoFirst = result.document->getOthers()->get<Target>(2, musx::dom::Cmper(1), musx::dom::Inci(0));
    const auto partTwoSecond = result.document->getOthers()->get<Target>(2, musx::dom::Cmper(1), musx::dom::Inci(1));
    const auto partTwoThird = result.document->getOthers()->get<Target>(2, musx::dom::Cmper(1), musx::dom::Inci(2));
    REQUIRE(scoreFirst);
    REQUIRE(scoreSecond);
    REQUIRE(scoreThird);
    REQUIRE(partOneFirst);
    REQUIRE(partOneSecond);
    REQUIRE(partOneThird);
    REQUIRE(partTwoFirst);
    REQUIRE(partTwoSecond);
    REQUIRE(partTwoThird);
    CHECK(scoreFirst->horzPos == -20);
    CHECK(scoreFirst->vertPos == -180);
    CHECK(scoreFirst->staffList == 1);
    CHECK(scoreSecond->horzPos == -20);
    CHECK(scoreSecond->vertPos == 32);
    CHECK(scoreThird->horzPos == -188);
    CHECK(scoreThird->vertPos == 36);
    CHECK(scoreThird->textRepeatId == 3);
    CHECK(partOneFirst->horzPos == -84);
    CHECK(partOneFirst->vertPos == -180);
    CHECK(partOneSecond->horzPos == -20);
    CHECK(partOneSecond->vertPos == 32);
    CHECK(partOneThird->horzPos == -188);
    CHECK(partOneThird->vertPos == 36);
    CHECK(partTwoFirst->horzPos == -20);
    CHECK(partTwoSecond->horzPos == -20);
    CHECK(partTwoSecond->vertPos == 32);
    CHECK(partTwoSecond->hidden);
    CHECK(partTwoThird->horzPos == -188);
    CHECK(partTwoThird->vertPos == 36);
    CHECK_FALSE(partTwoThird->hidden);
    CHECK(partOneFirst->getShareMode() == musx::dom::EnigmaBase::ShareMode::Partial);
    CHECK(partTwoSecond->getShareMode() == musx::dom::EnigmaBase::ShareMode::Partial);
    CHECK(partOneThird->getShareMode() == musx::dom::EnigmaBase::ShareMode::Partial);
    CHECK(partTwoThird->getShareMode() == musx::dom::EnigmaBase::ShareMode::Partial);
    const auto* partPosition = result.report.findField<Target>("horzPos", 1, musx::dom::Cmper(1), musx::dom::Inci(0));
    const auto* partHidden = result.report.findField<Target>("hidden", 2, musx::dom::Cmper(1), musx::dom::Inci(1));
    REQUIRE(partPosition);
    REQUIRE(partHidden);
    CHECK(partPosition->rawValue == -84);
    CHECK(partHidden->rawValue == 1);
}

TEST_CASE("Finale 2008 text repeat edits isolate staff list, hidden, and auto update", "[class][text-repeat-assign]")
{
    for (const auto& [path, hidden, autoUpdate] : std::array{
             std::tuple{"evidence/F2008/F2008-textrpt.mus", false, false},
             std::tuple{"evidence/F2008/F2008-textrpt-hide.mus", true, false},
             std::tuple{"evidence/F2008/F2008-textrpt-autoupd.mus", false, true},
         }) {
        const auto result = readFixture(path);
        const auto assignment = result.document->getOthers()->get<Target>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(0));
        REQUIRE(assignment);
        CHECK(assignment->staffList == 1);
        CHECK(assignment->hidden == hidden);
        CHECK(assignment->autoUpdate == autoUpdate);
        const auto* field = result.report.findField<Target>("autoUpdate", musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(0));
        REQUIRE(field);
        CHECK(field->origin == ValueOrigin::LegacyMus);
        CHECK(field->rawValue == (autoUpdate ? 1 : 0));
    }
}

TEST_CASE("Text repeat assignment reports every persisted member", "[class][text-repeat-assign]")
{
    const auto result = readFixture("evidence/F2012/F2012-textrpt.mus");
    constexpr std::array fields{"horzPos", "passNumber", "targetValue", "textRepeatId", "vertPos", "individualPlacement", "topStaffOnly", "hidden",
        "resetOnAction", "jumpOnMultiplePasses", "jumpAction", "autoUpdate", "trigger", "jumpIfIgnoring", "staffList"};
    for (const auto* member : fields) {
        const auto* field = result.report.findField<Target>(member, musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(0));
        REQUIRE(field);
        CHECK(field->origin == ValueOrigin::LegacyMus);
    }
    CHECK(Target::xmlMappingArray().size() == fields.size());
}

} // namespace
} // namespace finale_mus_reader_tests
