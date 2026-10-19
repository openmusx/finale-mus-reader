// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT
#include "class_test_support.h"
#include <array>
#include <catch2/generators/catch_generators.hpp>
#include <map>
#include <optional>
namespace finale_mus_reader_tests {
namespace {
using namespace classes;
using ExprAssignTest = musx::dom::others::MeasureExprAssign;
ImportReport importExpressionTest(
    const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile, const musx::dom::DocumentPtr& document)
{
    auto index = LegacyRecordIndex::build(parsed);
    ImportReport report(profile.epoch);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importMeasureExprAssigns(context);
    finale_mus_reader::runDeferredChecks(pending, finale_mus_reader::DeferredOrder::AsRegistered);
    return report;
}

/// Finale converts legacy expression assignments only in parts that have a definition.
void definePart(const musx::dom::DocumentPtr& document, musx::dom::Cmper part)
{
    using musx::dom::others::PartDefinition;
    if (!document->getOthers()->get<PartDefinition>(musx::dom::SCORE_PARTID, part)) {
        document->getOthers()->add(PartDefinition::XmlNodeName,
            std::make_shared<PartDefinition>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, part));
    }
}

TEST_CASE("Modern measure expression assignment preserves fields and flag groups", "[class][measure-expr-assign]")
{
    for (auto order : {ByteOrder::LittleEndian, ByteOrder::BigEndian}) {
        auto parsed = makeClassContainer(0xb1, {17, 0x2345, -23, 41, -2, static_cast<std::int16_t>(0xef6c), 7, 8, 1, 2, 3, 0}, order, 4);
        auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
        profile.version = SourceVersion{.major = 17};
        profile.byteOrder = order;
        auto session = musx::factory::DocumentFactory::begin();
        auto document = session.getDocument();
        ImportReport report(profile.epoch);
        auto index = LegacyRecordIndex::build(parsed);
        auto referenceSession = musx::factory::DocumentFactory::begin();
        auto reference = std::move(referenceSession).finish();
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::others::importMeasureExprAssigns(context);
        const auto item = document->getOthers()->get<ExprAssignTest>(0, musx::dom::Cmper(4), musx::dom::Inci(0));
        REQUIRE(item);
        CHECK(item->shapeExprId == 17);
        CHECK(item->textExprId == 0);
        CHECK(item->eduPosition == 0x12345);
        CHECK(item->horzEvpuOff == -23);
        CHECK(item->vertEvpuOff == 41);
        CHECK(item->staffAssign == -2);
        CHECK(item->layer == 4);
        CHECK(item->voice2);
        CHECK(item->channelSwitch == ExprAssignTest::ChannelSwitchTarget::ToExpression);
        CHECK(item->dontScaleWithEntry);
        CHECK(item->playbackStart == ExprAssignTest::PlaybackStart::AlignmentPoint);
        CHECK(item->showStaffList == ExprAssignTest::ShowStaffList::ScoreOnly);
        CHECK(item->hidden);
        CHECK(item->createdByHp);
        CHECK(item->staffGroup == 7);
        CHECK(item->staffList == 8);
        CHECK(item->graceNoteIndex == 2);
        CHECK(item->rehearsalMarkOffset == 3);
        REQUIRE(report.fields.size() == 1);
        CHECK(report.fields.begin()->second.size() == 18);
    }
}
TEST_CASE("F2012 entry expression imports from its measure assignment", "[class][measure-expr-assign]")
{
    const auto result = readFixture("evidence/F2012/F2012-noteartexp.mus");
    const auto item = result.document->getOthers()->get<ExprAssignTest>(0, musx::dom::Cmper(1), musx::dom::Inci(0));
    REQUIRE(item);
    CHECK(item->textExprId == 1);
    CHECK(item->staffAssign == 1);
    CHECK(item->layer == 1);
    CHECK(item->playbackStart == ExprAssignTest::PlaybackStart::AlignmentPoint);
}
TEST_CASE("Coda shape expression assignment converts its old object flag and horizontal units", "[class][measure-expr-assign]")
{
    for (const char* fixture : {"evidence/F100/F100-shapexp.mus", "evidence/F263/F263-F100-shapexp.mus"}) {
        const auto result = readFixture(fixture);
        const auto items = result.document->getOthers()->getArray<ExprAssignTest>(0, musx::dom::Cmper(1));
        REQUIRE(items.size() == 1);
        CHECK(items.front()->shapeExprId == 1);
        CHECK(items.front()->textExprId == 0);
        CHECK(items.front()->staffAssign == 1);
        CHECK(items.front()->dontScaleWithEntry);
        CHECK(items.front()->vertEvpuOff == -180);
    }
}

TEST_CASE("Coda expression conversion stores positions before music as EVPU offsets", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        for (const std::int16_t position : std::array<std::int16_t, 3>{1, 32, 64}) {
            auto session = musx::factory::DocumentFactory::begin();
            auto document = session.getDocument();
            auto measure = std::make_shared<others::Measure>(document, Cmper(0), EnigmaBase::ShareMode::All, Cmper(1));
            measure->beats = 1;
            measure->divBeat = 1024;
            measure->width = 96;
            measure->frontSpaceExtra = 32;
            measure->positioningMode = others::Measure::PositioningType::TimeSignature;
            document->getOthers()->add(others::Measure::XmlNodeName, measure);
            const auto parsed = makeContainer({{1, "DY", {1, position, 0, 0, 0, 0x2001}}}, FormatEpoch::CodaBanner, order);
            auto profile = SourceProfile(FormatEpoch::CodaBanner);
            profile.byteOrder = order;
            const auto report = importExpressionTest(parsed, profile, document);
            const auto assignments = document->getOthers()->getArray<ExprAssignTest>(0, Cmper(1));
            REQUIRE(assignments.size() == 1);
            CHECK(assignments.front()->eduPosition == (position == 64 ? 512 : 0));
            CHECK(assignments.front()->horzEvpuOff == (position == 1 ? -31 : 0));
            if (position == 1) {
                const auto* field = report.findField<ExprAssignTest>("horzEvpuOff", Cmper(0), Cmper(1), Inci(0));
                REQUIRE(field);
                CHECK(field->origin == ValueOrigin::LegacyMusAdjusted);
                CHECK(field->rawValue == position);
            }
        }
    }
}

TEST_CASE("Legacy entry assignments split definition positioning and copy enclosures", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    using Definition = others::TextExpressionDef;
    for (bool mixed : {false, true}) {
        auto seed = readFixture("evidence/F2006/F2006-lyric-vcs.mus");
        auto document = seed.document;
        definePart(document, 1);
        const auto frames = document->getDetails()->getArray<details::GFrameHold>(0);
        REQUIRE_FALSE(frames.empty());
        EntryNumber number = 0;
        Cmper measure = 0;
        StaffCmper staff = 0;
        for (const auto& frame : frames) {
            frame->iterateRawEntries([&](const MusxInstance<Entry>& entry, LayerIndex) {
                number = entry->getEntryNumber();
                measure = frame->getMeasure();
                staff = frame->getStaff();
                return false;
            });
            if (number) {
                break;
            }
        }
        REQUIRE(number != 0);
        constexpr Cmper id = 30000;
        auto definition = std::make_shared<Definition>(document, Cmper(0), EnigmaBase::ShareMode::All, id);
        definition->horzMeasExprAlign = others::HorizontalMeasExprAlign::Manual;
        definition->vertMeasExprAlign = others::VerticalMeasExprAlign::Manual;
        definition->measXAdjust = 13;
        definition->yAdjustBaseline = 17;
        document->getOthers()->add(Definition::XmlNodeName, definition);
        auto enclosure = std::make_shared<others::TextExpressionEnclosure>(document, Cmper(0), EnigmaBase::ShareMode::All, id);
        enclosure->lineWidth = 123;
        document->getOthers()->add(others::TextExpressionEnclosure::XmlNodeName, enclosure);
        const auto textNumber = *document->getTexts()->nextFreeCmper<texts::ExpressionText>();
        auto raw = std::make_shared<texts::ExpressionText>(document, Cmper(0), EnigmaBase::ShareMode::All, textNumber);
        raw->text = "^font(Times)^size(12)^nfx(0)cresc.";
        document->getTexts()->add(texts::ExpressionText::XmlNodeName, raw);
        const auto blockNumber = *document->getOthers()->nextFreeCmper<others::TextBlock>(Cmper(0));
        auto block = std::make_shared<others::TextBlock>(document, Cmper(0), EnigmaBase::ShareMode::All, blockNumber);
        block->textId = textNumber;
        block->textType = others::TextBlock::TextType::Expression;
        block->width = 50;
        block->xAdd = 9;
        document->getOthers()->add(others::TextBlock::XmlNodeName, block);
        definition->textIdKey = blockNumber;
        std::vector<SyntheticRow> rows{{id, "DT", {}}, {id, "DT", {3, 0, 13, 4, 2, -29}}, {id, "DT", {2, 17, 4, -31, 37, 0}}};
        if (mixed) {
            rows.push_back({measure, "DY", {id, 512, -11, 0, 0, static_cast<std::int16_t>(0x1000 | staff)}});
        }
        auto parsed = makeContainer(rows, FormatEpoch::DclLegacy);
        auto entries =
            makeDetailContainer(FormatEpoch::DclLegacy, number >> 16U, number & 0xffffU, {id, -23, 41, 0, static_cast<std::int16_t>(0x8100)}, "ED");
        parsed.blocks.push_back(std::move(entries.blocks.front()));
        auto profile = SourceProfile(FormatEpoch::DclLegacy);
        profile.version = SourceVersion{.major = 11};
        profile.byteOrder = ByteOrder::BigEndian;
        auto report = importExpressionTest(parsed, profile, document);
        const auto assignments = document->getOthers()->getArray<ExprAssignTest>(0, measure);
        REQUIRE(assignments.size() == (mixed ? 2 : 1));
        const auto note = assignments.back();
        CHECK(note->staffAssign == staff);
        CHECK(note->layer == 1);
        CHECK(note->vertEvpuOff == 41);
        CHECK(note->playbackStart == ExprAssignTest::PlaybackStart::PositionInMeasure);
        CHECK(note->textExprId == (mixed ? id + 1 : id));
        const auto upgraded = document->getOthers()->get<Definition>(0, note->textExprId);
        REQUIRE(upgraded);
        CHECK(upgraded->horzMeasExprAlign == others::HorizontalMeasExprAlign::CenterAllNoteheads);
        CHECK(upgraded->vertMeasExprAlign == others::VerticalMeasExprAlign::BottomNote);
        CHECK(upgraded->measXAdjust == -29);
        CHECK(upgraded->yAdjustBaseline == -31);
        CHECK(upgraded->yAdjustEntry == 37);
        if (mixed) {
            CHECK(assignments.front()->textExprId == id);
            CHECK(assignments.front()->eduPosition == 512);
            CHECK(definition->horzMeasExprAlign == others::HorizontalMeasExprAlign::Manual);
            CHECK(definition->yAdjustBaseline == 17);
            const auto copied = document->getOthers()->get<others::TextExpressionEnclosure>(0, note->textExprId);
            REQUIRE(copied);
            CHECK(copied->lineWidth == 123);
            // The copy has its own text block and text: frame size from the original, layout from the text defaults.
            CHECK(upgraded->textIdKey != blockNumber);
            const auto copiedBlock = document->getOthers()->get<others::TextBlock>(0, upgraded->textIdKey);
            REQUIRE(copiedBlock);
            const auto textOptions = document->getOptions()->get<options::TextOptions>();
            REQUIRE(textOptions);
            CHECK(copiedBlock->textId != textNumber);
            CHECK(copiedBlock->width == 50);
            CHECK(copiedBlock->xAdd == 0);
            CHECK(copiedBlock->noExpandSingleWord == !textOptions->textExpandSingleWord);
            CHECK(copiedBlock->wordWrap == textOptions->textWordWrap);
            const auto copiedText = document->getTexts()->get<texts::ExpressionText>(copiedBlock->textId);
            REQUIRE(copiedText);
            CHECK(copiedText->text == raw->text);
            CHECK(definition->textIdKey == blockNumber);
            CHECK(report.findField<ExprAssignTest>("textExprId", Cmper(0), measure, Inci(1))->origin == ValueOrigin::LegacyMusAdjusted);
            // The part's copy follows the score's definition, and no part receives a definition or enclosure of its own.
            const auto partNote = document->getOthers()->get<ExprAssignTest>(1, measure, Inci(1));
            REQUIRE(partNote);
            CHECK(partNote->getSourcePartId() == 1);
            CHECK(partNote->textExprId == id + 1);
        }
        for (const auto& item : document->getOthers()->getAllSources<Definition>()) {
            CHECK(item->getSourcePartId() == SCORE_PARTID);
        }
        for (const auto& item : document->getOthers()->getAllSources<others::TextExpressionEnclosure>()) {
            CHECK(item->getSourcePartId() == SCORE_PARTID);
        }
    }
}

TEST_CASE("A definition whose anchors already match its entry positioning is not duplicated", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    using Definition = others::TextExpressionDef;
    auto seed = readFixture("evidence/F2006/F2006-lyric-vcs.mus");
    auto document = seed.document;
    const auto hold = document->getDetails()->getArray<details::GFrameHold>(0).front();
    const auto measure = hold->getMeasure();
    const auto staff = hold->getStaff();
    EntryNumber number = 0;
    hold->iterateRawEntries([&](const MusxInstance<Entry>& entry, LayerIndex) {
        number = entry->getEntryNumber();
        return false;
    });
    REQUIRE(number);
    constexpr Cmper id = 30000;
    // The measure anchors equal the converted note anchors; only the baseline adjustment differs.
    auto definition = std::make_shared<Definition>(document, Cmper(0), EnigmaBase::ShareMode::All, id);
    definition->horzMeasExprAlign = others::HorizontalMeasExprAlign::CenterAllNoteheads;
    definition->vertMeasExprAlign = others::VerticalMeasExprAlign::BottomNote;
    definition->horzExprJustification = AlignJustify::Right;
    definition->measXAdjust = -29;
    definition->yAdjustBaseline = 17;
    document->getOthers()->add(Definition::XmlNodeName, definition);
    std::vector<SyntheticRow> rows{{id, "DT", {}}, {id, "DT", {3, 0, 13, 4, 2, -29}}, {id, "DT", {2, 17, 4, -31, 37, 0}},
        {static_cast<std::uint16_t>(measure), "DY", {id, 512, -11, 0, 0, static_cast<std::int16_t>(0x1000 | staff)}}};
    auto parsed = makeContainer(rows, FormatEpoch::DclLegacy);
    auto entries =
        makeDetailContainer(FormatEpoch::DclLegacy, number >> 16U, number & 0xffffU, {id, -23, 41, 0, static_cast<std::int16_t>(0x8100)}, "ED");
    parsed.blocks.push_back(std::move(entries.blocks.front()));
    auto profile = SourceProfile(FormatEpoch::DclLegacy);
    profile.version = SourceVersion{.major = 11};
    profile.byteOrder = ByteOrder::BigEndian;
    importExpressionTest(parsed, profile, document);
    const auto assignments = document->getOthers()->getArray<ExprAssignTest>(0, measure);
    REQUIRE(assignments.size() == 2);
    CHECK(assignments.back()->textExprId == id);
    CHECK_FALSE(document->getOthers()->get<Definition>(0, id + 1));
    // The definition takes the shared baseline and entry adjustments.
    CHECK(definition->yAdjustBaseline == -31);
    CHECK(definition->yAdjustEntry == 37);
}

TEST_CASE("Invalid text measure assignments do not force a definition copy", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    using Definition = others::TextExpressionDef;
    auto seed = readFixture("evidence/F2006/F2006-lyric-vcs.mus");
    auto document = seed.document;
    const auto hold = document->getDetails()->getArray<details::GFrameHold>(0).front();
    const auto measure = hold->getMeasure();
    const auto staff = hold->getStaff();
    EntryNumber number = 0;
    hold->iterateRawEntries([&](const MusxInstance<Entry>& entry, LayerIndex) {
        number = entry->getEntryNumber();
        return false;
    });
    REQUIRE(number);
    constexpr Cmper id = 30000;
    const bool missingStaff = GENERATE(false, true);
    auto definition = std::make_shared<Definition>(document, Cmper(0), EnigmaBase::ShareMode::All, id);
    definition->horzMeasExprAlign = others::HorizontalMeasExprAlign::Manual;
    definition->vertMeasExprAlign = others::VerticalMeasExprAlign::Manual;
    definition->horzExprJustification = AlignJustify::Right;
    definition->measXAdjust = -29;
    definition->yAdjustBaseline = 17;
    document->getOthers()->add(Definition::XmlNodeName, definition);
    const auto assignmentMeasure = missingStaff ? measure : Cmper(30000);
    const auto assignmentStaff = missingStaff ? StaffCmper(3000) : staff;
    REQUIRE_FALSE(document->getOthers()->get<others::Staff>(0, StaffCmper(3000)));
    REQUIRE_FALSE(document->getOthers()->get<others::Measure>(0, Cmper(30000)));
    std::vector<SyntheticRow> rows{{id, "DT", {}}, {id, "DT", {3, 0, 13, 4, 2, -29}}, {id, "DT", {2, 17, 4, -31, 37, 0}},
        {static_cast<std::uint16_t>(assignmentMeasure), "DY", {id, 512, -11, 0, 0, static_cast<std::int16_t>(0x1000 | assignmentStaff)}}};
    auto parsed = makeContainer(rows, FormatEpoch::DclLegacy);
    auto entries =
        makeDetailContainer(FormatEpoch::DclLegacy, number >> 16U, number & 0xffffU, {id, -23, 41, 0, static_cast<std::int16_t>(0x8100)}, "ED");
    parsed.blocks.push_back(std::move(entries.blocks.front()));
    auto profile = SourceProfile(FormatEpoch::DclLegacy);
    profile.version = SourceVersion{.major = 11};
    profile.byteOrder = ByteOrder::BigEndian;
    importExpressionTest(parsed, profile, document);
    const auto assignments = document->getOthers()->getArray<ExprAssignTest>(0, measure);
    REQUIRE(assignments.size() == (missingStaff ? 2 : 1));
    CHECK(assignments.back()->textExprId == id);
    CHECK_FALSE(document->getOthers()->get<Definition>(0, id + 1));
    CHECK(definition->horzMeasExprAlign == others::HorizontalMeasExprAlign::CenterAllNoteheads);
    CHECK(definition->vertMeasExprAlign == others::VerticalMeasExprAlign::BottomNote);
    CHECK(definition->yAdjustBaseline == -31);
    CHECK(definition->yAdjustEntry == 37);
}

TEST_CASE("Legacy entry assignments naming no definition are discarded", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    auto seed = readFixture("evidence/F2006/F2006-lyric-vcs.mus");
    auto document = seed.document;
    EntryNumber number = 0;
    Cmper measure = 0;
    for (const auto& frame : document->getDetails()->getArray<details::GFrameHold>(0)) {
        frame->iterateRawEntries([&](const MusxInstance<Entry>& entry, LayerIndex) {
            number = entry->getEntryNumber();
            measure = frame->getMeasure();
            return false;
        });
        if (number) {
            break;
        }
    }
    REQUIRE(number != 0);
    REQUIRE(document->getOthers()->getArray<ExprAssignTest>(0, measure).empty());
    constexpr Cmper id = 30000;
    auto parsed = makeContainer(std::vector<SyntheticRow>{}, FormatEpoch::DclLegacy);
    auto entries = makeDetailContainer(FormatEpoch::DclLegacy, number >> 16U, number & 0xffffU,
        {0, -23, 41, 0, static_cast<std::int16_t>(0x8100), id, -23, 41, 0, static_cast<std::int16_t>(0x8100)}, "ED");
    parsed.blocks.push_back(std::move(entries.blocks.front()));
    auto profile = SourceProfile(FormatEpoch::DclLegacy);
    profile.version = SourceVersion{.major = 11};
    profile.byteOrder = ByteOrder::BigEndian;
    importExpressionTest(parsed, profile, document);
    const auto assignments = document->getOthers()->getArray<ExprAssignTest>(0, measure);
    REQUIRE(assignments.size() == 1);
    CHECK(assignments.front()->getInci() == Inci(0));
    CHECK(assignments.front()->textExprId == id);
}

TEST_CASE("Early above and below entry assignments split their shared definition", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    using Definition = others::TextExpressionDef;
    auto seed = readFixture("evidence/F2006/F2006-lyric-vcs.mus");
    auto document = seed.document;
    const auto frames = document->getDetails()->getArray<details::GFrameHold>(0);
    REQUIRE_FALSE(frames.empty());
    EntryNumber number = 0;
    Cmper measure = 0;
    for (const auto& frame : frames) {
        frame->iterateRawEntries([&](const MusxInstance<Entry>& entry, LayerIndex) {
            number = entry->getEntryNumber();
            measure = frame->getMeasure();
            return false;
        });
        if (number) {
            break;
        }
    }
    REQUIRE(number != 0);
    constexpr Cmper id = 30000;
    auto definition = std::make_shared<Definition>(document, Cmper(0), EnigmaBase::ShareMode::All, id);
    definition->horzMeasExprAlign = others::HorizontalMeasExprAlign::Manual;
    definition->vertMeasExprAlign = others::VerticalMeasExprAlign::Manual;
    definition->measXAdjust = 13;
    definition->yAdjustBaseline = 17;
    document->getOthers()->add(Definition::XmlNodeName, definition);
    auto enclosure = std::make_shared<others::TextExpressionEnclosure>(document, Cmper(0), EnigmaBase::ShareMode::All, id);
    enclosure->lineWidth = 123;
    document->getOthers()->add(others::TextExpressionEnclosure::XmlNodeName, enclosure);
    std::vector<SyntheticRow> rows{{id, "DT", {}}, {id, "DT", {3, 0, 13, 4, 2, -29}}, {id, "DT", {2, 17, 4, -31, 37, 0}}};
    auto parsed = makeContainer(rows, FormatEpoch::UncompressedLegacy);
    auto entries = makeDetailContainer(FormatEpoch::UncompressedLegacy, number >> 16U, number & 0xffffU,
        {id, -23, 41, 0, static_cast<std::int16_t>(0x8100), id, -23, 41, 0, static_cast<std::int16_t>(0x8800)}, "ED");
    parsed.blocks.push_back(std::move(entries.blocks.front()));
    auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
    profile.version = SourceVersion{.major = 4};
    profile.byteOrder = ByteOrder::BigEndian;
    auto report = importExpressionTest(parsed, profile, document);
    const auto assignments = document->getOthers()->getArray<ExprAssignTest>(0, measure);
    REQUIRE(assignments.size() == 2);
    CHECK(assignments[0]->textExprId == id);
    CHECK(assignments[1]->textExprId == id + 1);
    const auto above = document->getOthers()->get<Definition>(0, assignments[0]->textExprId);
    const auto below = document->getOthers()->get<Definition>(0, assignments[1]->textExprId);
    REQUIRE(above);
    REQUIRE(below);
    CHECK(above->horzMeasExprAlign == others::HorizontalMeasExprAlign::LeftOfPrimaryNotehead);
    CHECK(above->vertMeasExprAlign == others::VerticalMeasExprAlign::TopNote);
    CHECK(below->vertMeasExprAlign == others::VerticalMeasExprAlign::BottomNote);
    // Early definitions take the upgrade's note defaults: no adjustments, left justified.
    for (const auto& converted : {above, below}) {
        CHECK(converted->horzExprJustification == AlignJustify::Left);
        CHECK(converted->measXAdjust == 0);
        CHECK(converted->yAdjustBaseline == 0);
        CHECK(converted->yAdjustEntry == 0);
    }
    CHECK(assignments[1]->vertEvpuOff == 41);
    CHECK(report.findField<ExprAssignTest>("vertEvpuOff", Cmper(0), measure, Inci(1))->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Legacy measure assignment expands staff lists and applies individual placement", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    auto parsed = makeClassContainer(
        std::vector<SyntheticClassRow>{{0xb1, {19, -12, 23, 7, 4, 0xc00}, 3}, {0xcf, {2, 7, -6, 11, 0, 1}, 3}}, ByteOrder::LittleEndian);
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.version = SourceVersion{.major = 13};
    profile.byteOrder = ByteOrder::LittleEndian;
    auto session = musx::factory::DocumentFactory::begin();
    auto document = session.getDocument();
    for (Cmper id : std::array<Cmper, 2>{1, 2}) {
        document->getOthers()->add(others::Staff::XmlNodeName, std::make_shared<others::Staff>(document, Cmper(0), EnigmaBase::ShareMode::All, id));
        auto used = std::make_shared<others::StaffUsed>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, BASE_SYSTEM_ID, Inci(id - 1));
        used->staffId = id;
        document->getOthers()->add(others::StaffUsed::XmlNodeName, used);
    }
    auto list = std::make_shared<others::StaffListRepeatScore>(document, Cmper(0), EnigmaBase::ShareMode::All, Cmper(4));
    list->values = {1, 2};
    document->getOthers()->add(others::StaffListRepeatScore::XmlNodeName, list);
    auto report = importExpressionTest(parsed, profile, document);
    const auto assignments = document->getOthers()->getArray<ExprAssignTest>(0, Cmper(3));
    REQUIRE(assignments.size() == 2);
    CHECK(assignments[0]->horzEvpuOff == -12);
    CHECK(assignments[0]->vertEvpuOff == 23);
    CHECK_FALSE(assignments[0]->hidden);
    CHECK(assignments[1]->horzEvpuOff == -18);
    CHECK(assignments[1]->vertEvpuOff == 34);
    CHECK(assignments[1]->hidden);
    CHECK(assignments[1]->showStaffList == ExprAssignTest::ShowStaffList::ScoreOnly);
    CHECK(assignments[1]->playbackStart == ExprAssignTest::PlaybackStart::PositionInMeasure);
    CHECK(report.findField<ExprAssignTest>("vertEvpuOff", Cmper(0), Cmper(3), Inci(0))->origin == ValueOrigin::LegacyMus);
    CHECK(report.findField<ExprAssignTest>("vertEvpuOff", Cmper(0), Cmper(3), Inci(1))->origin == ValueOrigin::LegacyMusAdjusted);
}

TEST_CASE("List-zero expressions use the base staff list while direct staff assignments remain recoverable", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    const auto order = GENERATE(ByteOrder::LittleEndian, ByteOrder::BigEndian);
    const auto parsed =
        makeClassContainer(std::vector<SyntheticClassRow>{{0xb1, {19, 0, 0, 0, 0, 0}, 1}, {0xb1, {19, 0, 0, 0, 2, 0x1000}, 2}}, order);
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.version = SourceVersion{.major = 13};
    profile.byteOrder = order;
    auto session = musx::factory::DocumentFactory::begin();
    auto document = session.getDocument();
    for (Cmper id : std::array<Cmper, 2>{1, 2}) {
        document->getOthers()->add(
            others::Staff::XmlNodeName, std::make_shared<others::Staff>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, id));
    }
    auto used = std::make_shared<others::StaffUsed>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, BASE_SYSTEM_ID, Inci(0));
    used->staffId = 1;
    document->getOthers()->add(others::StaffUsed::XmlNodeName, used);

    importExpressionTest(parsed, profile, document);
    const auto expanded = document->getOthers()->getArray<ExprAssignTest>(SCORE_PARTID, Cmper(1));
    REQUIRE(expanded.size() == 1);
    CHECK(expanded.front()->staffAssign == 1);
    const auto direct = document->getOthers()->getArray<ExprAssignTest>(SCORE_PARTID, Cmper(2));
    REQUIRE(direct.size() == 1);
    CHECK(direct.front()->staffAssign == 2);
}

TEST_CASE("Early deleted measure assignments are omitted before Finale 3.3v2", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    for (const auto order : {ByteOrder::LittleEndian, ByteOrder::BigEndian}) {
        const auto parsed = makeContainer({{1, "DY", {19, 0, 0, 0, 0, static_cast<std::int16_t>(0x9000)}}}, FormatEpoch::UncompressedLegacy, order);
        for (const auto& [minor, build] :
            {std::pair{std::uint8_t(2), std::uint8_t(0)}, std::pair{std::uint8_t(3), std::uint8_t(1)}, std::pair{std::uint8_t(3), std::uint8_t(2)}}) {
            auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
            profile.version = SourceVersion{.major = 3, .minor = minor, .build = build};
            profile.byteOrder = order;
            auto session = musx::factory::DocumentFactory::begin();
            auto document = session.getDocument();
            document->getOthers()->add(others::TextExpressionDef::XmlNodeName,
                std::make_shared<others::TextExpressionDef>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, Cmper(19)));
            auto used = std::make_shared<others::StaffUsed>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, BASE_SYSTEM_ID, Inci(0));
            used->staffId = 1;
            document->getOthers()->add(others::StaffUsed::XmlNodeName, used);
            importExpressionTest(parsed, profile, document);
            const auto assignments = document->getOthers()->getArray<ExprAssignTest>(SCORE_PARTID, Cmper(1));
            CHECK(assignments.size() == (minor == 3 && build == 2 ? 1 : 0));
        }
    }
}

TEST_CASE("Coda measure assignment type flag is preserved", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    const auto order = GENERATE(ByteOrder::LittleEndian, ByteOrder::BigEndian);
    const auto parsed = makeContainer({{1, "DY", {19, 0, 0, 0, 0, static_cast<std::int16_t>(0x8000)}}}, FormatEpoch::CodaBanner, order);
    auto profile = SourceProfile(FormatEpoch::CodaBanner);
    profile.byteOrder = order;
    auto session = musx::factory::DocumentFactory::begin();
    auto document = session.getDocument();
    document->getOthers()->add(others::TextExpressionDef::XmlNodeName,
        std::make_shared<others::TextExpressionDef>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, Cmper(19)));
    auto used = std::make_shared<others::StaffUsed>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, BASE_SYSTEM_ID, Inci(0));
    used->staffId = 1;
    document->getOthers()->add(others::StaffUsed::XmlNodeName, used);
    importExpressionTest(parsed, profile, document);
    CHECK(document->getOthers()->getArray<ExprAssignTest>(SCORE_PARTID, Cmper(1)).size() == 1);
}

TEST_CASE("Legacy part measure assignments inherit absent individual placement collections", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    const auto order = GENERATE(ByteOrder::LittleEndian, ByteOrder::BigEndian);
    const auto adjustment = GENERATE(std::int16_t(21), std::int16_t(-21), std::int16_t(-24));
    const auto overrideStaff = GENERATE(std::int16_t(0), std::int16_t(2), std::int16_t(3));
    std::vector<SyntheticClassRow> rows{
        {0xb1, {19, -12, 81, 7, 2, 0x1800}, 3}, {0xb1, {0, 0, 0, 0, 0, 0}, 3, 1, true, {0, 0, 0, 0}, 0, 0}, {0xcf, {2, 7, -6, adjustment, 0, 1}, 3}};
    if (overrideStaff) {
        rows.push_back({0xcf, {overrideStaff, 7, -9, 12, 0, 0}, 3, 1});
    }
    auto parsed = makeClassContainer(rows, order);
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.version = SourceVersion{.major = 13};
    profile.byteOrder = order;
    auto session = musx::factory::DocumentFactory::begin();
    auto document = session.getDocument();
    definePart(document, 1);
    const auto report = importExpressionTest(parsed, profile, document);
    const auto score = document->getOthers()->get<ExprAssignTest>(0, Cmper(3), Inci(0));
    const auto part = document->getOthers()->get<ExprAssignTest>(1, Cmper(3), Inci(0));
    REQUIRE(score);
    REQUIRE(part);
    CHECK(score->vertEvpuOff == 81 + adjustment);
    CHECK(score->horzEvpuOff == -18);
    CHECK(score->hidden);
    CHECK(part->vertEvpuOff == 81 + (overrideStaff == 0 ? adjustment : overrideStaff == 2 ? 12 : 0));
    CHECK(part->horzEvpuOff == (overrideStaff == 0 ? -18 : overrideStaff == 2 ? -21 : -12));
    CHECK(part->hidden == (overrideStaff == 0));
    const auto* field = report.findField<ExprAssignTest>("vertEvpuOff", Cmper(1), Cmper(3), Inci(0));
    REQUIRE(field);
    CHECK(field->origin == (overrideStaff == 3 ? ValueOrigin::LegacyMus : ValueOrigin::LegacyMusAdjusted));
    CHECK(field->rawValue == (overrideStaff == 0 ? adjustment : overrideStaff == 2 ? 12 : 81));
}

TEST_CASE("A part's copy of a score assignment uses the part's individual placement", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    const auto partPlacement = GENERATE(false, true);
    // The part overrides only the first assignment, so its copy of the second comes from the score.
    std::vector<SyntheticClassRow> rows{
        {0xb1, {20, 0, 5, 0, 2, 0x1000, 19, -12, 81, 7, 2, 0x1800}, 3}, {0xb1, {20, 0, 6, 0, 2, 0x1000}, 3, 1}, {0xcf, {2, 7, -6, -24, 0, 1}, 3}};
    if (partPlacement) {
        rows.push_back({0xcf, {2, 7, -9, 12, 0, 0}, 3, 1});
    }
    auto parsed = makeClassContainer(rows, ByteOrder::LittleEndian);
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.version = SourceVersion{.major = 13};
    profile.byteOrder = ByteOrder::LittleEndian;
    auto session = musx::factory::DocumentFactory::begin();
    auto document = session.getDocument();
    definePart(document, 1);
    importExpressionTest(parsed, profile, document);
    const auto score = document->getOthers()->get<ExprAssignTest>(0, Cmper(3), Inci(1));
    const auto copy = document->getOthers()->get<ExprAssignTest>(1, Cmper(3), Inci(1));
    REQUIRE(score);
    REQUIRE(copy);
    CHECK(copy->getSourcePartId() == 1);
    CHECK(score->vertEvpuOff == 57);
    CHECK(score->hidden);
    CHECK(copy->vertEvpuOff == (partPlacement ? 93 : 57));
    CHECK(copy->hidden == !partPlacement);
    CHECK(document->getOthers()->get<ExprAssignTest>(1, Cmper(3), Inci(0))->vertEvpuOff == 6);
}

TEST_CASE("Floating expression assignments do not borrow a real staff's overfilled duration", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    const auto order = GENERATE(ByteOrder::LittleEndian, ByteOrder::BigEndian);
    auto seed = readFixture("evidence/F2006/F2006-lyric-vcs.mus");
    auto document = seed.document;
    const auto holds = document->getDetails()->getArray<details::GFrameHold>(0);
    REQUIRE_FALSE(holds.empty());
    const auto hold = holds.front();
    const auto measureId = hold->getMeasure();
    const auto staffId = hold->getStaff();
    const auto measure = std::const_pointer_cast<others::Measure>(document->getOthers()->get<others::Measure>(0, measureId));
    REQUIRE(measure);
    measure->beats = 1;
    measure->divBeat = 256;
    Edu longest = 0;
    details::GFrameHoldContext frame(document, 0, staffId, measureId);
    frame.iterateEntries([&](const EntryInfoPtr& entry) {
        longest = (std::max)(longest, (entry->elapsedDuration + entry->actualDuration).calcEduDuration());
        return true;
    });
    REQUIRE(longest > 256);
    const auto used = document->getOthers()->getArray<others::StaffUsed>(0, BASE_SYSTEM_ID);
    REQUIRE_FALSE(used.empty());
    for (const auto& item : used) {
        std::const_pointer_cast<others::StaffUsed>(item)->staffId = staffId;
    }
    auto list = std::make_shared<others::StaffListRepeatScore>(document, Cmper(0), EnigmaBase::ShareMode::All, Cmper(30000));
    list->values = {-2, -1, staffId};
    document->getOthers()->add(others::StaffListRepeatScore::XmlNodeName, list);
    const auto parsed = makeClassContainer(0xb1, {30000, 100, 0, 0, 30000, 0}, order, measureId);
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.version = SourceVersion{.major = 13};
    profile.byteOrder = order;
    importExpressionTest(parsed, profile, document);
    const auto assignments = document->getOthers()->getArray<ExprAssignTest>(0, measureId);
    REQUIRE(assignments.size() == 3);
    for (const auto& assignment : assignments) {
        CHECK(assignment->eduPosition == (assignment->staffAssign < 0 ? 100 : 100 * longest / 256));
    }
}

TEST_CASE("Legacy expression visibility follows parts display and extraction modes", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    const auto order = GENERATE(ByteOrder::LittleEndian, ByteOrder::BigEndian);
    const bool showParts = GENERATE(false, true);
    const auto extractionId = GENERATE(Cmper(0), SPECIAL_PART_EXTRACTION_SYSTEM_ID, STAFF_SET_1_SYSTEM_ID);
    const bool inScore = GENERATE(false, true);
    const bool inParts = GENERATE(false, true);
    auto session = musx::factory::DocumentFactory::begin();
    auto document = session.getDocument();
    auto misc = std::make_shared<options::MiscOptions>(document);
    misc->showRepeatsForParts = showParts;
    document->getOptions()->add(options::MiscOptions::XmlNodeName, misc);
    auto globals = std::make_shared<others::PartGlobals>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, MUSX_GLOBALS_CMPER);
    globals->specialPartExtractionIUList = extractionId;
    document->getOthers()->add(others::PartGlobals::XmlNodeName, globals);
    document->getOthers()->add(
        others::Staff::XmlNodeName, std::make_shared<others::Staff>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, Cmper(1)));
    auto used = std::make_shared<others::StaffUsed>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, BASE_SYSTEM_ID, Inci(0));
    used->staffId = 1;
    document->getOthers()->add(others::StaffUsed::XmlNodeName, used);
    auto scoreList = std::make_shared<others::StaffListRepeatScore>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, Cmper(4));
    if (inScore) {
        scoreList->values = {-2, -1, 1};
    }
    document->getOthers()->add(others::StaffListRepeatScore::XmlNodeName, scoreList);
    auto partsList = std::make_shared<others::StaffListRepeatParts>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, Cmper(4));
    if (inParts) {
        partsList->values = {-2, -1, 1};
    }
    document->getOthers()->add(others::StaffListRepeatParts::XmlNodeName, partsList);
    const auto parsed = makeClassContainer(std::vector<SyntheticClassRow>{{0xb1, {1, 0, 0, 0, 4, 0}, 1}, {0xb1, {1, 0, 0, 0, 1, 0x1000}, 2}}, order);
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.version = SourceVersion{.major = 13};
    profile.byteOrder = order;
    importExpressionTest(parsed, profile, document);
    const auto assignments = document->getOthers()->getArray<ExprAssignTest>(SCORE_PARTID, Cmper(1));
    std::size_t expectedCount = 0;
    for (StaffCmper staff : std::array<StaffCmper, 3>{-2, -1, 1}) {
        const bool score = (showParts || (staff > 0 && extractionId == SPECIAL_PART_EXTRACTION_SYSTEM_ID)) ? inParts : inScore;
        if (!score && !inParts) {
            continue;
        }
        const auto found = std::ranges::find_if(assignments, [staff](const auto& item) { return item->staffAssign == staff; });
        REQUIRE(found != assignments.end());
        CHECK((*found)->showStaffList
              == (score && inParts ? ExprAssignTest::ShowStaffList::ScoreAndPart
                  : score          ? ExprAssignTest::ShowStaffList::ScoreOnly
                                   : ExprAssignTest::ShowStaffList::PartOnly));
        ++expectedCount;
    }
    CHECK(assignments.size() == expectedCount);
    const auto direct = document->getOthers()->get<ExprAssignTest>(SCORE_PARTID, Cmper(2), Inci(0));
    REQUIRE(direct);
    CHECK(direct->showStaffList == ExprAssignTest::ShowStaffList::ScoreAndPart);
}

TEST_CASE("A staff no defined part holds is not visible in parts", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    const auto order = GENERATE(ByteOrder::LittleEndian, ByteOrder::BigEndian);
    const bool inScore = GENERATE(false, true);
    auto session = musx::factory::DocumentFactory::begin();
    auto document = session.getDocument();
    definePart(document, 1);
    for (Cmper id : std::array<Cmper, 2>{1, 2}) {
        document->getOthers()->add(
            others::Staff::XmlNodeName, std::make_shared<others::Staff>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, id));
        auto used = std::make_shared<others::StaffUsed>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, BASE_SYSTEM_ID, Inci(id - 1));
        used->staffId = id;
        document->getOthers()->add(others::StaffUsed::XmlNodeName, used);
    }
    // Part 1 holds only staff 1.
    auto partUsed = std::make_shared<others::StaffUsed>(document, Cmper(1), EnigmaBase::ShareMode::None, BASE_SYSTEM_ID, Inci(0));
    partUsed->staffId = 1;
    document->getOthers()->add(others::StaffUsed::XmlNodeName, partUsed);
    auto scoreList = std::make_shared<others::StaffListRepeatScore>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, Cmper(4));
    if (inScore) {
        scoreList->values = {1, 2};
    }
    document->getOthers()->add(others::StaffListRepeatScore::XmlNodeName, scoreList);
    auto partsList = std::make_shared<others::StaffListRepeatParts>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, Cmper(4));
    partsList->values = {1, 2};
    document->getOthers()->add(others::StaffListRepeatParts::XmlNodeName, partsList);
    const auto parsed = makeClassContainer(0xb1, {1, 0, 0, 0, 4, 0}, order, 1);
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.version = SourceVersion{.major = 13};
    profile.byteOrder = order;
    importExpressionTest(parsed, profile, document);
    const auto assignments = document->getOthers()->getArray<ExprAssignTest>(SCORE_PARTID, Cmper(1));
    REQUIRE(assignments.size() == (inScore ? 2 : 1));
    CHECK(assignments[0]->staffAssign == 1);
    CHECK(assignments[0]->showStaffList == (inScore ? ExprAssignTest::ShowStaffList::ScoreAndPart : ExprAssignTest::ShowStaffList::PartOnly));
    if (inScore) {
        CHECK(assignments[1]->staffAssign == 2);
        CHECK(assignments[1]->showStaffList == ExprAssignTest::ShowStaffList::ScoreOnly);
    }
}

TEST_CASE("Legacy measure hidden flag survives conversion unless individual placement overrides it", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    for (const bool baseHidden : {false, true}) {
        for (const int placement : {-1, 0, 1}) {
            const auto flags = static_cast<std::int16_t>(0x1000 | (baseHidden ? 0x4000 : 0) | (placement >= 0 ? 0x800 : 0));
            std::vector<SyntheticClassRow> rows{{0xb1, {19, -12, 23, 7, 2, flags}, 3}};
            if (placement >= 0) {
                rows.push_back({0xcf, {2, 7, 0, 0, 0, static_cast<std::int16_t>(placement)}, 3});
            }
            const auto parsed = makeClassContainer(rows, ByteOrder::LittleEndian);
            auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
            profile.version = SourceVersion{.major = 13};
            profile.byteOrder = ByteOrder::LittleEndian;
            auto session = musx::factory::DocumentFactory::begin();
            auto document = session.getDocument();
            const auto report = importExpressionTest(parsed, profile, document);
            const auto assignment = document->getOthers()->get<ExprAssignTest>(0, Cmper(3), Inci(0));
            REQUIRE(assignment);
            CHECK(assignment->hidden == (placement >= 0 ? placement != 0 : baseHidden));
            const auto* field = report.findField<ExprAssignTest>("hidden", Cmper(0), Cmper(3), Inci(0));
            REQUIRE(field);
            CHECK(field->origin == ValueOrigin::LegacyMus);
            CHECK(field->rawValue == (placement >= 0 ? placement : flags));
        }
    }
}

TEST_CASE("Legacy part measure coordinates clear the inherited alternate unit", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    const auto order = GENERATE(ByteOrder::LittleEndian, ByteOrder::BigEndian);
    const auto scorePosition = GENERATE(std::int16_t(-112), std::int16_t(251));
    const auto partPosition = GENERATE(std::int16_t(-18), std::int16_t(0), std::int16_t(663));
    const auto parsed = makeClassContainer(std::vector<SyntheticClassRow>{{0xb1, {1, scorePosition, 0, 0, 1, 0x1000}, 1},
                                               {0xb1, {1, partPosition, 0, 0, 1, 0x1000}, 1, 1, true, {0, 0xffff}, 0, 0x800}},
        order);
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.version = SourceVersion{.major = 13};
    profile.byteOrder = order;
    auto session = musx::factory::DocumentFactory::begin();
    auto document = session.getDocument();
    definePart(document, 1);
    const auto report = importExpressionTest(parsed, profile, document);
    for (Cmper part : std::array<Cmper, 2>{0, 1}) {
        const auto assignment = document->getOthers()->get<ExprAssignTest>(part, Cmper(1), Inci(0));
        REQUIRE(assignment);
        CHECK(assignment->getSourcePartId() == part);
        const auto position = part ? partPosition : scorePosition;
        CHECK(assignment->eduPosition == (std::max)(0, int(position)));
        CHECK(assignment->horzEvpuOff == (std::min)(0, int(position)));
        const auto* cleared = report.findField<ExprAssignTest>(position < 0 ? "eduPosition" : "horzEvpuOff", part, Cmper(1), Inci(0));
        REQUIRE(cleared);
        CHECK(cleared->origin == ValueOrigin::LegacyBehavior);
        CHECK(cleared->rawValue == 0);
    }
}

TEST_CASE("Part entry overrides preserve the score incidence on inherited frames", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    // Cases: linked, horizontal, vertical, hidden, other flag, definition, internal word.
    const int overrideKind = GENERATE(0, 1, 2, 3, 4, 5, 6);
    const bool unlinkedPlacement = overrideKind >= 1 && overrideKind <= 3;
    const auto order = GENERATE(ByteOrder::LittleEndian, ByteOrder::BigEndian);
    auto seed = readFixture("evidence/F2006/F2006-lyric-vcs.mus");
    auto document = seed.document;
    definePart(document, 1);
    EntryNumber number = 0;
    Cmper measure = 0;
    StaffCmper staff = 0;
    for (const auto& frame : document->getDetails()->getArray<details::GFrameHold>(0)) {
        frame->iterateRawEntries([&](const MusxInstance<Entry>& entry, LayerIndex) {
            number = entry->getEntryNumber();
            measure = frame->getMeasure();
            staff = frame->getStaff();
            return false;
        });
        if (number) {
            break;
        }
    }
    REQUIRE(number);
    for (const auto& system : document->getOthers()->getArray<others::StaffSystem>(0)) {
        std::const_pointer_cast<others::StaffSystem>(system)->hasStaffScaling = true;
        auto size = std::make_shared<details::StaffSize>(document, Cmper(0), EnigmaBase::ShareMode::All, system->getCmper(), staff);
        size->staffPercent = 80;
        document->getDetails()->add(details::StaffSize::XmlNodeName, size);
        auto partSystem = std::make_shared<others::StaffSystem>(document, Cmper(1), EnigmaBase::ShareMode::None, system->getCmper());
        partSystem->startMeas = system->startMeas;
        partSystem->endMeas = system->endMeas;
        document->getOthers()->add(others::StaffSystem::XmlNodeName, partSystem);
    }
    auto parsed = makeClassContainer(0xb1, {1, 0, 11, 0, staff, 0x1000}, order, measure);
    for (std::uint16_t part : std::array<std::uint16_t, 2>{0, 1}) {
        std::vector<std::int16_t> words{2, -95, static_cast<std::int16_t>(part ? 31 : 21), static_cast<std::int16_t>(part ? 17 : 0),
            static_cast<std::int16_t>(part ? 0x8101 : 0x8000)};
        if (!part) {
            words.insert(words.end(), {3, 65, 0, 0, static_cast<std::int16_t>(0x8000)});
        }
        auto entries = makeDetailClassContainer(number >> 16U, number & 0xffffU, part, words, order, 0x423, part != 0,
            {static_cast<std::uint16_t>(overrideKind == 5 ? 0xffff : 0), static_cast<std::uint16_t>(overrideKind == 1 ? 0xffff : 0),
                static_cast<std::uint16_t>(overrideKind == 2 ? 0xffff : 0)});
        if (part) {
            auto& bytes = entries.blocks.front().data;
            const std::uint16_t internalMask = overrideKind == 6 ? 0xffff : 0;
            const std::uint16_t flagMask = overrideKind == 3 ? 1 : overrideKind == 4 ? 0x100 : 0;
            for (std::size_t index = 0; index < 2; ++index) {
                const auto shift = order == ByteOrder::BigEndian ? (1 - index) * 8 : index * 8;
                bytes[bytes.size() - 4 + index] = static_cast<std::uint8_t>(internalMask >> shift);
                bytes[bytes.size() - 2 + index] = static_cast<std::uint8_t>(flagMask >> shift);
            }
        }
        parsed.blocks.push_back(std::move(entries.blocks.front()));
    }
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.version = SourceVersion{.major = 13};
    profile.byteOrder = order;
    const auto report = importExpressionTest(parsed, profile, document);
    const auto score = document->getOthers()->getArray<ExprAssignTest>(0, measure);
    const auto part = document->getOthers()->getArray<ExprAssignTest>(1, measure);
    REQUIRE(score.size() == 3);
    REQUIRE(part.size() == 3);
    CHECK(score[0]->vertEvpuOff == 11);
    CHECK(part[0]->vertEvpuOff == 11);
    CHECK(score[2]->horzEvpuOff == 52);
    CHECK(part[2]->horzEvpuOff == 52);
    CHECK(score[1]->horzEvpuOff == -76);
    CHECK(part[1]->horzEvpuOff == (unlinkedPlacement ? -95 : -76));
    const auto* offsetOrigin = report.findField<ExprAssignTest>("horzEvpuOff", 1, measure, part[1]->getInci());
    REQUIRE(offsetOrigin);
    CHECK(offsetOrigin->origin == (unlinkedPlacement ? ValueOrigin::LegacyMus : ValueOrigin::LegacyMusAdjusted));
    CHECK(score[1]->vertEvpuOff == 21);
    CHECK(part[1]->vertEvpuOff == (overrideKind == 2 ? 31 : 21));
    CHECK(part[1]->hidden == (overrideKind == 3 || score[1]->hidden));
    CHECK(part[1]->getSourcePartId() == 1);
    CHECK(part[1]->getInci() == score[1]->getInci());

    // The part measure stores its own partially linked run with no incidence gap, not just the override.
    std::vector<MusxInstance<ExprAssignTest>> stored;
    for (const auto& assignment : document->getOthers()->getAllSources<ExprAssignTest>()) {
        if (assignment->getSourcePartId() == 1 && assignment->getCmper() == measure) {
            stored.push_back(assignment);
        }
    }
    REQUIRE(stored.size() == score.size());
    for (std::size_t index = 0; index < stored.size(); ++index) {
        CAPTURE(index);
        CHECK(stored[index]->getInci() == Inci(index));
        CHECK(stored[index]->getShareMode() == EnigmaBase::ShareMode::Partial);
        if (index != 1) {
            CHECK(stored[index]->textExprId == score[index]->textExprId);
            CHECK(stored[index]->shapeExprId == score[index]->shapeExprId);
            CHECK(stored[index]->horzEvpuOff == score[index]->horzEvpuOff);
            CHECK(stored[index]->vertEvpuOff == score[index]->vertEvpuOff);
            const auto* copied = report.findField<ExprAssignTest>("vertEvpuOff", 1, measure, Inci(index));
            const auto* original = report.findField<ExprAssignTest>("vertEvpuOff", 0, measure, Inci(index));
            REQUIRE(copied);
            REQUIRE(original);
            CHECK(copied->origin == original->origin);
            CHECK(copied->rawValue == original->rawValue);
        }
    }
}

TEST_CASE("Every defined part converts each score measure assignment in its own context", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    const auto order = GENERATE(ByteOrder::LittleEndian, ByteOrder::BigEndian);
    // Part 1 has its own individual placement, part 2 inherits the score's, and part 3 stores a
    // record but has no definition.
    const auto parsed =
        makeClassContainer(std::vector<SyntheticClassRow>{{0xb1, {19, 100, 23, 7, 2, 0x1800}, 3}, {0xb1, {19, 200, 23, 7, 2, 0x1800}, 3, 3},
                               {0xcf, {2, 7, 10, 5, 0, 0}, 3}, {0xcf, {2, 7, 40, 9, 0, 1}, 3, 1}},
            order);
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.version = SourceVersion{.major = 13};
    profile.byteOrder = order;
    auto session = musx::factory::DocumentFactory::begin();
    auto document = session.getDocument();
    definePart(document, 1);
    definePart(document, 2);
    const auto report = importExpressionTest(parsed, profile, document);
    const auto score = document->getOthers()->get<ExprAssignTest>(SCORE_PARTID, Cmper(3), Inci(0));
    REQUIRE(score);
    CHECK(score->eduPosition == 110);
    CHECK(score->vertEvpuOff == 28);
    CHECK_FALSE(score->hidden);
    const auto own = document->getOthers()->get<ExprAssignTest>(1, Cmper(3), Inci(0));
    REQUIRE(own);
    CHECK(own->getSourcePartId() == 1);
    CHECK(own->eduPosition == 140);
    CHECK(own->vertEvpuOff == 32);
    CHECK(own->hidden);
    const auto* edu = report.findField<ExprAssignTest>("eduPosition", Cmper(1), Cmper(3), Inci(0));
    REQUIRE(edu);
    CHECK(edu->origin == ValueOrigin::LegacyMusAdjusted);
    const auto inherited = document->getOthers()->get<ExprAssignTest>(2, Cmper(3), Inci(0));
    REQUIRE(inherited);
    CHECK(inherited->getSourcePartId() == 2);
    CHECK(inherited->getShareMode() == EnigmaBase::ShareMode::Partial);
    CHECK(inherited->eduPosition == 110);
    CHECK(inherited->vertEvpuOff == 28);
    for (const auto& assignment : document->getOthers()->getAllSources<ExprAssignTest>()) {
        CHECK(assignment->getSourcePartId() != 3);
    }
}

TEST_CASE("A part scales its measure assignment's position through its own measure", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    const auto order = GENERATE(ByteOrder::LittleEndian, ByteOrder::BigEndian);
    auto seed = readFixture("evidence/F2006/F2006-lyric-vcs.mus");
    auto document = seed.document;
    definePart(document, 1);
    const auto hold = document->getDetails()->getArray<details::GFrameHold>(0).front();
    const auto measureId = hold->getMeasure();
    const auto staffId = hold->getStaff();
    const auto measure = std::const_pointer_cast<others::Measure>(document->getOthers()->get<others::Measure>(0, measureId));
    REQUIRE(measure);
    measure->beats = 1;
    measure->divBeat = 256;
    Edu longest = 0;
    details::GFrameHoldContext frame(document, 0, staffId, measureId);
    frame.iterateEntries([&](const EntryInfoPtr& entry) {
        longest = (std::max)(longest, (entry->elapsedDuration + entry->actualDuration).calcEduDuration());
        return true;
    });
    REQUIRE(longest > 256);
    // The part's own measure holds every entry, so its position is not scaled.
    auto partMeasure = std::make_shared<others::Measure>(document, Cmper(1), EnigmaBase::ShareMode::Partial, measureId);
    *partMeasure = *measure;
    partMeasure->beats = static_cast<Cmper>(longest);
    partMeasure->divBeat = 1;
    document->getOthers()->add(others::Measure::XmlNodeName, partMeasure);
    const auto parsed = makeClassContainer(0xb1, {30000, 100, 0, 0, staffId, 0x1000}, order, measureId);
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.version = SourceVersion{.major = 13};
    profile.byteOrder = order;
    importExpressionTest(parsed, profile, document);
    const auto score = document->getOthers()->get<ExprAssignTest>(SCORE_PARTID, measureId, Inci(0));
    const auto part = document->getOthers()->get<ExprAssignTest>(1, measureId, Inci(0));
    REQUIRE(score);
    REQUIRE(part);
    CHECK(score->eduPosition == 100 * longest / 256);
    CHECK(part->getSourcePartId() == 1);
    CHECK(part->eduPosition == 100);
}

TEST_CASE("Measure assignments follow the floating staves and then the score's staff order", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    const auto order = GENERATE(ByteOrder::LittleEndian, ByteOrder::BigEndian);
    auto session = musx::factory::DocumentFactory::begin();
    auto document = session.getDocument();
    Inci slot = 0;
    for (Cmper id : std::array<Cmper, 2>{2, 1}) {
        document->getOthers()->add(others::Staff::XmlNodeName, std::make_shared<others::Staff>(document, Cmper(0), EnigmaBase::ShareMode::All, id));
        auto used = std::make_shared<others::StaffUsed>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, BASE_SYSTEM_ID, slot++);
        used->staffId = id;
        document->getOthers()->add(others::StaffUsed::XmlNodeName, used);
    }
    auto list = std::make_shared<others::StaffListRepeatScore>(document, Cmper(0), EnigmaBase::ShareMode::All, Cmper(4));
    list->values = {-2, -1, 1, 2};
    document->getOthers()->add(others::StaffListRepeatScore::XmlNodeName, list);
    const auto parsed = makeClassContainer(0xb1, {19, 0, 0, 0, 4, 0}, order, 3);
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.version = SourceVersion{.major = 13};
    profile.byteOrder = order;
    importExpressionTest(parsed, profile, document);
    const auto assignments = document->getOthers()->getArray<ExprAssignTest>(SCORE_PARTID, Cmper(3));
    REQUIRE(assignments.size() == 4);
    const std::array<StaffCmper, 4> expected{-1, -2, 2, 1};
    for (std::size_t index = 0; index < expected.size(); ++index) {
        CAPTURE(index);
        CHECK(assignments[index]->getInci() == Inci(index));
        CHECK(assignments[index]->staffAssign == expected[index]);
    }
}

TEST_CASE("Entry assignments follow the score's staff order within a measure", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    const auto order = GENERATE(ByteOrder::LittleEndian, ByteOrder::BigEndian);
    auto seed = readFixture("evidence/F2006/F2006-lyric-vcs.mus");
    auto document = seed.document;
    const auto hold = document->getDetails()->getArray<details::GFrameHold>(0).front();
    const auto measure = hold->getMeasure();
    const auto staff = hold->getStaff();
    EntryNumber number = 0;
    hold->iterateRawEntries([&](const MusxInstance<Entry>& entry, LayerIndex) {
        number = entry->getEntryNumber();
        return false;
    });
    REQUIRE(number);
    // A second staff shows the same frame, and the score lists it before the original staff.
    const StaffCmper added = staff + 1;
    document->getOthers()->add(
        others::Staff::XmlNodeName, std::make_shared<others::Staff>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, added));
    auto addedHold = std::make_shared<details::GFrameHold>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, added, measure);
    addedHold->frames = hold->frames;
    addedHold->clefId = hold->clefId;
    document->getDetails()->add(details::GFrameHold::XmlNodeName, addedHold);
    const auto used = document->getOthers()->getArray<others::StaffUsed>(SCORE_PARTID, BASE_SYSTEM_ID);
    REQUIRE(used.size() == 1);
    std::const_pointer_cast<others::StaffUsed>(used.front())->staffId = added;
    auto original = std::make_shared<others::StaffUsed>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, BASE_SYSTEM_ID, Inci(1));
    original->staffId = staff;
    document->getOthers()->add(others::StaffUsed::XmlNodeName, original);
    const auto parsed = makeDetailClassContainer(number >> 16U, number & 0xffffU, 0, {7, 0, 0, 0, static_cast<std::int16_t>(0x8000)}, order, 0x423);
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.version = SourceVersion{.major = 13};
    profile.byteOrder = order;
    importExpressionTest(parsed, profile, document);
    const auto assignments = document->getOthers()->getArray<ExprAssignTest>(SCORE_PARTID, measure);
    REQUIRE(assignments.size() == 2);
    CHECK(assignments[0]->staffAssign == added);
    CHECK(assignments[1]->staffAssign == staff);
}

TEST_CASE("Entry conversion ignores frames outside the score staff list or measure range", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    const auto order = GENERATE(ByteOrder::LittleEndian, ByteOrder::BigEndian);
    const bool unlistedStaff = GENERATE(false, true);
    auto seed = readFixture("evidence/F2006/F2006-lyric-vcs.mus");
    auto document = seed.document;
    const auto hold = document->getDetails()->getArray<details::GFrameHold>(0).front();
    const auto measure = hold->getMeasure();
    const auto staff = hold->getStaff();
    EntryNumber number = 0;
    hold->iterateRawEntries([&](const MusxInstance<Entry>& entry, LayerIndex) {
        number = entry->getEntryNumber();
        return false;
    });
    REQUIRE(number);
    // A second hold names the same entries but lies outside the score traversal.
    const StaffCmper added = unlistedStaff ? staff + 1 : staff;
    if (unlistedStaff) {
        document->getOthers()->add(
            others::Staff::XmlNodeName, std::make_shared<others::Staff>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, added));
    } else {
        auto outside = std::make_shared<others::Measure>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, Cmper(30000));
        *outside = *document->getOthers()->get<others::Measure>(SCORE_PARTID, measure);
        document->getOthers()->add(others::Measure::XmlNodeName, outside);
    }
    auto addedHold =
        std::make_shared<details::GFrameHold>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, added, unlistedStaff ? measure : MeasCmper(30000));
    addedHold->frames = hold->frames;
    addedHold->clefId = hold->clefId;
    document->getDetails()->add(details::GFrameHold::XmlNodeName, addedHold);
    const auto parsed = makeDetailClassContainer(number >> 16U, number & 0xffffU, 0, {7, 0, 0, 0, static_cast<std::int16_t>(0x8000)}, order, 0x423);
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.version = SourceVersion{.major = 13};
    profile.byteOrder = order;
    importExpressionTest(parsed, profile, document);
    const auto assignments = document->getOthers()->getArray<ExprAssignTest>(SCORE_PARTID, measure);
    REQUIRE(assignments.size() == 1);
    CHECK(assignments[0]->staffAssign == staff);
    CHECK(document->getOthers()->getArray<ExprAssignTest>(SCORE_PARTID, MeasCmper(30000)).empty());
}

TEST_CASE("A part keeps the score's entry assignment unless it unlinks the placement", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    const auto order = GENERATE(ByteOrder::LittleEndian, ByteOrder::BigEndian);
    const bool unlinked = GENERATE(false, true);
    const bool defined = GENERATE(false, true);
    auto seed = readFixture("evidence/F2006/F2006-lyric-vcs.mus");
    auto document = seed.document;
    if (defined) {
        definePart(document, 1);
    }
    EntryNumber number = 0;
    Cmper measure = 0;
    for (const auto& frame : document->getDetails()->getArray<details::GFrameHold>(0)) {
        frame->iterateRawEntries([&](const MusxInstance<Entry>& entry, LayerIndex) {
            number = entry->getEntryNumber();
            measure = frame->getMeasure();
            return false;
        });
        if (number) {
            break;
        }
    }
    REQUIRE(number);
    std::optional<finale_mus_reader::container::ParsedContainer> parsed;
    for (std::uint16_t part : std::array<std::uint16_t, 2>{0, 1}) {
        auto entries = makeDetailClassContainer(number >> 16U, number & 0xffffU, part,
            {2, static_cast<std::int16_t>(part ? -40 : -95), 21, 0, static_cast<std::int16_t>(0x8000)}, order, 0x423, part != 0,
            {0, static_cast<std::uint16_t>(part && unlinked ? 0xffff : 0)});
        if (!parsed) {
            parsed = std::move(entries);
        } else {
            parsed->blocks.push_back(std::move(entries.blocks.front()));
        }
    }
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.version = SourceVersion{.major = 13};
    profile.byteOrder = order;
    importExpressionTest(*parsed, profile, document);
    REQUIRE(document->getOthers()->getArray<ExprAssignTest>(SCORE_PARTID, measure).size() == 1);
    std::vector<MusxInstance<ExprAssignTest>> stored;
    for (const auto& assignment : document->getOthers()->getAllSources<ExprAssignTest>()) {
        if (assignment->getSourcePartId() == 1) {
            stored.push_back(assignment);
        }
    }
    REQUIRE(stored.size() == (unlinked && defined ? 1 : 0));
    if (!stored.empty()) {
        CHECK(stored.front()->horzEvpuOff == -40);
    }
}

TEST_CASE("A linked part measure follows the score's expression flag", "[class][measure-expr-assign]")
{
    using namespace musx::dom;
    const auto order = GENERATE(ByteOrder::LittleEndian, ByteOrder::BigEndian);
    const bool unlinked = GENERATE(false, true);
    auto seed = readFixture("evidence/F2006/F2006-lyric-vcs.mus");
    auto document = seed.document;
    definePart(document, 1);
    const auto hold = document->getDetails()->getArray<details::GFrameHold>(0).front();
    const auto measure = static_cast<std::uint16_t>(hold->getMeasure());
    EntryNumber number = 0;
    hold->iterateRawEntries([&](const MusxInstance<Entry>& entry, LayerIndex) {
        number = entry->getEntryNumber();
        return false;
    });
    REQUIRE(number);
    auto score = std::const_pointer_cast<others::Measure>(document->getOthers()->get<others::Measure>(SCORE_PARTID, measure));
    REQUIRE(score);
    score->hasExpression = false;
    auto partMeasure = std::make_shared<others::Measure>(document, Cmper(1), EnigmaBase::ShareMode::Partial, measure);
    *partMeasure = *score;
    document->getOthers()->add(others::Measure::XmlNodeName, partMeasure);
    auto parsed = makeClassContainer(std::vector<SyntheticClassRow>{{0xb0, std::vector<std::int16_t>(13), measure},
                                         {0xb0, std::vector<std::int16_t>(13), measure, 1, true,
                                             {0, 0, 0, 0, 0, static_cast<std::uint16_t>(unlinked ? 0x4000 : 0), 0, 0, 0, 0, 0, 0, 0}}},
        order);
    auto entries = makeDetailClassContainer(number >> 16U, number & 0xffffU, 0, {7, 0, 0, 0, static_cast<std::int16_t>(0x8000)}, order, 0x423);
    parsed.blocks.push_back(std::move(entries.blocks.front()));
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.version = SourceVersion{.major = 13};
    profile.byteOrder = order;
    importExpressionTest(parsed, profile, document);
    CHECK(score->hasExpression);
    CHECK(partMeasure->hasExpression == !unlinked);
}

TEST_CASE("Modern unknown flag enums stay unmapped and trailing partial tuples are diagnosed", "[class][measure-expr-assign]")
{
    auto parsed = makeClassContainer(0xb1, {1, -1, 0, 0, 1, 0x1877, 0, 0, -1, 0, 0, 0, 12}, ByteOrder::LittleEndian, 1);
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.version = SourceVersion{.major = 17};
    profile.byteOrder = ByteOrder::LittleEndian;
    auto session = musx::factory::DocumentFactory::begin();
    auto document = session.getDocument();
    auto report = importExpressionTest(parsed, profile, document);
    const auto assignments = document->getOthers()->getArray<ExprAssignTest>(0, musx::dom::Cmper(1));
    REQUIRE(assignments.size() == 1);
    CHECK(assignments.front()->eduPosition == -1);
    for (const auto* name : {"layer", "channelSwitch", "showStaffList"}) {
        const auto* field = report.findField<ExprAssignTest>(name, musx::dom::Cmper(0), musx::dom::Cmper(1), musx::dom::Inci(0));
        REQUIRE(field);
        CHECK(field->origin == ValueOrigin::Unmapped);
    }
    REQUIRE(report.diagnostics.size() == 1);
}
} // namespace
} // namespace finale_mus_reader_tests
