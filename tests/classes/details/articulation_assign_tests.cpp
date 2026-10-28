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

TEST_CASE("Zlib articulation assignment maintains its vertical placement relative to the definition", "[class][articulation-assign]")
{
    const auto result = readFixture("evidence/F2012/F2012-noteartexp.mus");
    const auto assignments = result.document->getDetails()->getArray<Target>(musx::dom::SCORE_PARTID, 5);
    REQUIRE(assignments.size() == 1);
    CHECK(assignments[0]->articDef == 1);
    CHECK(assignments[0]->horzOffset == -3);
    CHECK(assignments[0]->vertOffset == 0);
    CHECK(assignments[0]->neverStack);
    CHECK(assignments[0]->numSlursAvoided == -1);
    const auto key = finale_mus_reader::instanceKey<Target>(musx::dom::SCORE_PARTID, musx::dom::Cmper(0), musx::dom::Inci(0), musx::dom::Cmper(5));
    CHECK(result.report.findField(key, "hide")->origin == ValueOrigin::LegacyMus);
    const auto* vertical = result.report.findField(key, "vertOffset");
    REQUIRE(vertical);
    CHECK(vertical->origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(vertical->rawValue == 24);
}

TEST_CASE("Articulation maintained-placement offsets normalize after definitions in every epoch", "[class][articulation-assign]")
{
    struct PositionCase
    {
        std::int16_t flags;
        std::int16_t stored;
        std::int16_t distance;
        std::int16_t expected;
    };
    const PositionCase cases[] = {
        {0, 30, 24, 30},
        {1, 30, 24, 30},
        {2, 30, 24, 30},
        {3, 30, 24, -30},
        {4, 30, 24, 30},
        {5, 30, 24, 30},
        {8, 30, 24, 6},
        {9, 30, 24, 6},
        {10, 30, 24, 6},
        {11, 30, 24, -54},
        {12, 30, 24, 6},
        {13, 30, 24, 6},
        {3, -30, 24, 30},
        {11, -30, 24, 6},
        {10, -30, -24, -6},
        {11, 30, -24, -6},
        {3, 0, 24, 0},
        {10, 0, 24, -24},
        {10, 30, 0, 30},
        {11, -12, 24, -12},
        {3, -32768, 24, -32768},
        {11, -32768, 24, 32744},
        {10, 32767, -1, -32768},
        {10, -32768, 1, 32767},
    };
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            for (const bool definitionFirst : {false, true}) {
                for (const auto& position : cases) {
                    CAPTURE(epoch, order, definitionFirst, position.flags, position.stored, position.distance);
                    auto parsed =
                        epoch == FormatEpoch::ZlibLegacy
                            ? makeClassContainer(0x0079,
                                  {62, 6144, 0, 0, 0, position.flags, 62, 6144, 0, 0, position.distance, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                  order, 1)
                            : makeContainer({{1, "IX", {62, 6144, 0, 0, 0, position.flags}}, {1, "IX", {62, 6144, 0, 0, position.distance, 0}},
                                                {1, "IX", {0, 0, 0, 0, 0, 0}}, {1, "IX", {0, 0, 0, 0, 0, 0}}},
                                  epoch, order);
                    auto details = epoch == FormatEpoch::ZlibLegacy
                                       ? makeDetailClassContainer(2, 27, 0, {1, -9, 4, position.stored, -6, 1, 0, 0, 0, 0}, order, 0x03f1)
                                       : makeDetailContainer(epoch, 2, 27, {1, -9, 4, position.stored, -6}, "IM", order);
                    parsed.blocks.push_back(std::move(details.blocks.front()));
                    auto session = musx::factory::DocumentFactory::begin();
                    const auto document = session.getDocument();
                    auto referenceSession = musx::factory::DocumentFactory::begin();
                    const auto reference = std::move(referenceSession).finish();
                    const auto index = LegacyRecordIndex::build(parsed);
                    SourceProfile profile(epoch);
                    profile.byteOrder = order;
                    ImportReport report(epoch);
                    finale_mus_reader::PendingReferences pending;
                    musx::factory::ConstructionContext construction;
                    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
                    if (definitionFirst) {
                        finale_mus_reader::others::importArticulationDefs(context);
                    }
                    finale_mus_reader::details::importArticulationAssigns(context);
                    if (!definitionFirst) {
                        finale_mus_reader::others::importArticulationDefs(context);
                    }
                    const auto assignment = document->getDetails()->get<Target>(0, 0x2001b, musx::dom::Inci(0));
                    REQUIRE(assignment);
                    CHECK(assignment->vertOffset == position.stored);
                    const auto key = finale_mus_reader::instanceKey<Target>(0, musx::dom::Cmper(2), musx::dom::Inci(0), musx::dom::Cmper(27));
                    const auto* vertical = report.findField(key, "vertOffset");
                    REQUIRE(vertical);
                    const auto before = *vertical;
                    finale_mus_reader::runDeferredChecks(pending,
                        definitionFirst ? finale_mus_reader::DeferredOrder::AsRegistered : finale_mus_reader::DeferredOrder::ReversedWithinStage);
                    CHECK(assignment->vertOffset == position.expected);
                    CHECK(assignment->horzOffset == -9);
                    CHECK(assignment->horzAdd == 4);
                    CHECK(assignment->vertAdd == -6);
                    CHECK(assignment->hide == (epoch == FormatEpoch::ZlibLegacy));
                    CHECK_FALSE(assignment->overridePlacement);
                    CHECK_FALSE(assignment->aboveEntry);
                    CHECK(assignment->neverStack);
                    CHECK(vertical->rawValue == position.stored);
                    CHECK(vertical->blockOffset == before.blockOffset);
                    CHECK(vertical->decodedOffset == before.decodedOffset);
                    CHECK(vertical->sourceIdentity == before.sourceIdentity);
                    CHECK(vertical->origin == (position.expected == position.stored ? ValueOrigin::LegacyMus : ValueOrigin::LegacyMusAdjusted));
                }
            }
        }
    }
}

TEST_CASE("Articulation vertical normalization propagates only through linked part bits", "[class][articulation-assign]")
{
    using Definition = musx::dom::others::ArticulationDef;
    for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        auto parsed = makeDetailClassContainer(0, 27, 0, {1, -9, 4, 30, -6, 0, 0, 0, 0, 0, 1, 7, 8, -12, 9, 0, 0, 0, 0, 0}, order, 0x03f1);
        auto part = makeDetailClassContainer(0, 27, 1, {1, -9, 4, 42, -6, 0, 0, 0, 0, 0, 1, 7, 8, 12, 9, 0, 0, 0, 0, 0}, order, 0x03f1, true,
            {0, 0, 0, 0xffff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xffff});
        auto& block = parsed.blocks.front();
        block.data.insert(block.data.end(), part.blocks.front().data.begin(), part.blocks.front().data.end());
        auto linkedPart = makeDetailClassContainer(0, 27, 3, {1, 11, 4, 99, -6, 0, 0, 0, 0, 0, 1, 13, 8, 3, 9, 0, 0, 0, 0, 0}, order, 0x03f1, true,
            {0, 0xffff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xffff, 0, 0x000f});
        block.data.insert(block.data.end(), linkedPart.blocks.front().data.begin(), linkedPart.blocks.front().data.end());
        auto standalone = makeDetailClassContainer(0, 27, 4, {1, 11, 4, 99, -6, 0, 0, 0, 0, 0}, order, 0x03f1);
        block.data.insert(block.data.end(), standalone.blocks.front().data.begin(), standalone.blocks.front().data.end());
        block.info.decodedSize = block.data.size();
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        auto referenceSession = musx::factory::DocumentFactory::begin();
        const auto reference = std::move(referenceSession).finish();
        const auto index = LegacyRecordIndex::build(parsed);
        SourceProfile profile(parsed.formatEpoch);
        profile.byteOrder = order;
        ImportReport report(profile.epoch);
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::details::importArticulationAssigns(context);
        auto definition = std::make_shared<Definition>(document, musx::dom::Cmper(0), musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper(1));
        definition->autoVert = true;
        definition->defVertPos = 24;
        definition->autoVertMode = Definition::AutoVerticalMode::AboveEntry;
        document->getOthers()->add(Definition::XmlNodeName, definition);
        SECTION("Part assignments inherit the score definition") {}
        SECTION("Part assignments use a part-specific definition")
        {
            auto partDefinition =
                std::make_shared<Definition>(document, musx::dom::Cmper(1), musx::dom::EnigmaBase::ShareMode::None, musx::dom::Cmper(1));
            partDefinition->autoVert = true;
            partDefinition->defVertPos = 12;
            partDefinition->autoVertMode = Definition::AutoVerticalMode::BelowEntry;
            document->getOthers()->add(Definition::XmlNodeName, partDefinition);
        }
        finale_mus_reader::runDeferredChecks(pending, finale_mus_reader::DeferredOrder::ReversedWithinStage);
        const auto score = document->getDetails()->getArray<Target>(0, 27);
        const auto unlinked = document->getDetails()->getArray<Target>(1, 27);
        const auto linked = document->getDetails()->getArray<Target>(2, 27);
        REQUIRE(score.size() == 2);
        REQUIRE(unlinked.size() == 2);
        REQUIRE(linked.size() == 2);
        CHECK(score[0]->vertOffset == 6);
        CHECK(score[1]->vertOffset == -36);
        CHECK(unlinked[0]->vertOffset == 42);
        CHECK(unlinked[1]->vertOffset == 12);
        CHECK(linked[0]->getSourcePartId() == 0);
        CHECK(linked[1]->getSourcePartId() == 0);
        CHECK(linked[0]->vertOffset == 6);
        CHECK(linked[1]->vertOffset == -36);
        const auto partlyLinked = document->getDetails()->getArray<Target>(3, 27);
        REQUIRE(partlyLinked.size() == 2);
        CHECK(partlyLinked[0]->vertOffset == 6);
        CHECK(partlyLinked[0]->horzOffset == 11);
        CHECK(partlyLinked[1]->vertOffset == -45);
        CHECK(partlyLinked[1]->horzOffset == 13);
        const auto independent = document->getDetails()->get<Target>(4, 27, musx::dom::Inci(0));
        REQUIRE(independent);
        CHECK(independent->vertOffset == 99);
        CHECK(document->getDetails()->getAllSources<Target>().size() == 7);
        const auto* vertical = report.findField<Target>("vertOffset", 1, musx::dom::Cmper(0), musx::dom::Inci(1), musx::dom::Cmper(27));
        REQUIRE(vertical);
        CHECK(vertical->rawValue == 12);
        CHECK(vertical->origin == ValueOrigin::LegacyMus);
        const auto* inherited = report.findField<Target>("vertOffset", 3, musx::dom::Cmper(0), musx::dom::Inci(0), musx::dom::Cmper(27));
        REQUIRE(inherited);
        CHECK(inherited->rawValue == 30);
        CHECK(inherited->origin == ValueOrigin::LegacyMusAdjusted);
    }
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
            finale_mus_reader::runDeferredChecks(pending);
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
            CHECK(report.findField<Target>("vertOffset", 0, musx::dom::Cmper(0), musx::dom::Inci(0), musx::dom::Cmper(27))->origin
                  == ValueOrigin::LegacyMus);
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
