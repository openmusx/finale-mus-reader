// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <array>
#include <cstdint>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using RepeatBack = musx::dom::others::RepeatBack;

void checkRepeatBack(const finale_mus_reader::container::ParsedContainer& parsed, std::optional<SourceVersion> version, bool modernFlags,
    musx::dom::others::RepeatActionType action = musx::dom::others::RepeatActionType::JumpRelative,
    musx::dom::others::RepeatTriggerType trigger = musx::dom::others::RepeatTriggerType::OnPass)
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
    finale_mus_reader::others::importRepeatBacks(context);
    const auto repeat = document->getOthers()->get<RepeatBack>(musx::dom::SCORE_PARTID, musx::dom::Cmper(7));
    REQUIRE(repeat);
    CHECK(repeat->passNumber == 3);
    CHECK(repeat->targetValue == -2);
    CHECK(repeat->leftHPos == -35);
    CHECK(repeat->leftVPos == 12);
    CHECK(repeat->individualPlacement);
    CHECK(repeat->resetOnAction);
    CHECK(repeat->topStaffOnly == modernFlags);
    CHECK(repeat->jumpAction == action);
    CHECK(repeat->trigger == trigger);
    CHECK(repeat->staffList == (modernFlags ? 9 : 0));
    CHECK(repeat->rightHPos == 81);
    CHECK(repeat->rightVPos == -23);
    const auto fields = {"passNumber", "targetValue", "leftHPos", "leftVPos", "individualPlacement", "topStaffOnly", "hidden", "resetOnAction",
        "jumpAction", "trigger", "staffList", "rightHPos", "rightVPos"};
    for (const auto* member : fields) {
        const auto* field = report.findField<RepeatBack>(member, musx::dom::SCORE_PARTID, musx::dom::Cmper(7));
        REQUIRE(field);
        const auto expected = member == std::string_view("hidden") && !modernFlags         ? ValueOrigin::LegacyBehavior
                              : member == std::string_view("topStaffOnly") && !modernFlags ? ValueOrigin::LegacyBehavior
                              : member == std::string_view("staffList") && !modernFlags    ? ValueOrigin::Finale27Default
                                                                                           : ValueOrigin::LegacyMus;
        CHECK(field->origin == expected);
    }
    CHECK(RepeatBack::xmlMappingArray().size() == 13);
}

TEST_CASE("RepeatBack decodes the twelve word layout in early epochs", "[class][repeat-back]")
{
    const auto earlyRows = std::vector<SyntheticRow>{{7, "BR", {99, 3, -2, -35, 12, 0x1841}}, {7, "BR", {0, 0, 0, 81, -23, 0}}};
    checkRepeatBack(makeContainer(earlyRows, FormatEpoch::CodaBanner), SourceVersion{.major = 1}, false);
    checkRepeatBack(makeContainer(earlyRows, FormatEpoch::UncompressedLegacy), SourceVersion{.major = 4}, false);
    const auto dclRows = std::vector<SyntheticRow>{{7, "BR", {99, 3, -2, -35, 12, 0x1441}}, {7, "BR", {0, 0, 0, 81, -23, 0}}};
    checkRepeatBack(makeContainer(dclRows, FormatEpoch::DclLegacy), SourceVersion{.major = 9}, false);
}

TEST_CASE("RepeatBack decodes the later flag layout with stable word positions", "[class][repeat-back]")
{
    const auto rows = std::vector<SyntheticRow>{{7, "BR", {17, 3, -2, -35, 12, 0x0427}}, {7, "BR", {9, 0, 0, 81, -23, 0}}};
    checkRepeatBack(makeContainer(rows, FormatEpoch::DclLegacy), SourceVersion{.major = 10}, true);
    checkRepeatBack(makeClassContainer({SyntheticClassRow{0x00cb, {17, 3, -2, -35, 12, 0x0427, 9, 0, 0, 81, -23, 0}, 7}}, ByteOrder::LittleEndian),
        SourceVersion{.major = 12}, true);
    checkRepeatBack(makeClassContainer({SyntheticClassRow{0x00cb, {17, 3, -2, -35, 12, 0x0417, 9, 0, 0, 81, -23, 0}, 7}}, ByteOrder::LittleEndian),
        SourceVersion{.major = 12}, true, musx::dom::others::RepeatActionType::JumpAbsolute);
    checkRepeatBack(makeClassContainer({SyntheticClassRow{0x00cb, {17, 3, -2, -35, 12, 0x0407, 9, 0, 0, 81, -23, 0}, 7}}, ByteOrder::LittleEndian),
        SourceVersion{.major = 12}, true, musx::dom::others::RepeatActionType::JumpAuto);
}

TEST_CASE("RepeatBack decodes every later action and trigger value", "[class][repeat-back]")
{
    using Action = musx::dom::others::RepeatActionType;
    using Trigger = musx::dom::others::RepeatTriggerType;
    const auto actions = std::array{Action::JumpAuto, Action::JumpAbsolute, Action::JumpRelative, Action::JumpToMark, Action::Stop, Action::NoJump};
    const auto triggers = std::array{Trigger::Always, Trigger::OnPass, Trigger::UntilPass};
    for (std::uint16_t action = 0; action < actions.size(); ++action) {
        for (std::uint16_t trigger = 0; trigger < triggers.size(); ++trigger) {
            const auto flags = static_cast<std::int16_t>(0x0007 | (action << 4) | (trigger << 10));
            const auto parsed =
                makeClassContainer({SyntheticClassRow{0x00cb, {17, 3, -2, -35, 12, flags, 9, 0, 0, 81, -23, 0}, 7}}, ByteOrder::LittleEndian);
            checkRepeatBack(parsed, SourceVersion{.major = 12}, true, actions[action], triggers[trigger]);
        }
    }
}

TEST_CASE("RepeatBack reads the controlled Coda and zlib records", "[class][repeat-back]")
{
    const auto early = readFixture("evidence/F100/F100-rptback.mus");
    const auto first = early.document->getOthers()->get<RepeatBack>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(first);
    CHECK(first->passNumber == 2);
    CHECK(first->targetValue == 1);
    CHECK(first->resetOnAction);
    CHECK(first->jumpAction == musx::dom::others::RepeatActionType::JumpAbsolute);
    CHECK(first->trigger == musx::dom::others::RepeatTriggerType::OnPass);
    CHECK(early.report.findField<RepeatBack>("rightHPos", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::Finale27Default);

    const auto earlySecond = readFixture("evidence/F100/F100-rptback2.mus");
    const auto firstVariant = earlySecond.document->getOthers()->get<RepeatBack>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(firstVariant);
    CHECK(firstVariant->passNumber == 2);
    CHECK(firstVariant->individualPlacement);
    CHECK_FALSE(firstVariant->resetOnAction);
    CHECK(firstVariant->trigger == musx::dom::others::RepeatTriggerType::UntilPass);

    const auto late = readFixture("evidence/F2012/F2012-rptback.mus");
    const auto second = late.document->getOthers()->get<RepeatBack>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(second);
    CHECK(second->passNumber == 3);
    CHECK(second->targetValue == 1);
    CHECK(second->resetOnAction);
    CHECK(second->jumpAction == musx::dom::others::RepeatActionType::JumpAuto);
    CHECK(second->trigger == musx::dom::others::RepeatTriggerType::OnPass);
    CHECK(second->staffList == 0);
    CHECK(late.report.findField<RepeatBack>("topStaffOnly", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
    CHECK(late.report.findField<RepeatBack>("staffList", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("RepeatBack reads the Finale 2005 action bits", "[class][repeat-back]")
{
    const auto result = readFixture("evidence/F2005/F2005-rptback.mus");
    const auto repeat = result.document->getOthers()->get<RepeatBack>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(repeat);
    CHECK(repeat->passNumber == 2);
    CHECK(repeat->targetValue == -1);
    CHECK(repeat->jumpAction == musx::dom::others::RepeatActionType::JumpRelative);
    CHECK(repeat->trigger == musx::dom::others::RepeatTriggerType::Always);
    CHECK(result.report.findField<RepeatBack>("topStaffOnly", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("RepeatBack rejects incomplete rows and unresolved compact boundaries", "[class][repeat-back]")
{
    for (const auto& [rows, epoch, version] : {
             std::tuple{std::vector<SyntheticRow>{{7, "BR", {0, 3, 2, 0, 0, 0}}}, FormatEpoch::UncompressedLegacy,
                 std::optional<SourceVersion>{SourceVersion{.major = 9}}},
             std::tuple{std::vector<SyntheticRow>{{7, "BR", {3, 2, 0, 0, 0, 0}}, {7, "BR", {0, 0, 0, 0, 0, 0}}}, FormatEpoch::DclLegacy,
                 std::optional<SourceVersion>{}},
         }) {
        const auto parsed = makeContainer(rows, epoch);
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        SourceProfile profile(epoch);
        profile.version = version;
        profile.byteOrder = parsed.byteOrder;
        ImportReport report(epoch);
        const auto index = LegacyRecordIndex::build(parsed);
        auto referenceSession = musx::factory::DocumentFactory::begin();
        const auto reference = std::move(referenceSession).finish();
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::others::importRepeatBacks(context);
        CHECK(document->getOthers()->getAllSources<RepeatBack>().empty());
        CHECK(report.diagnostics.size() == 1);
    }
}

} // namespace
} // namespace finale_mus_reader_tests
