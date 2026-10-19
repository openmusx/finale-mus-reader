// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using SplitMeasure = musx::dom::others::SplitMeasure;

TEST_CASE("Split measure imports both stored positions", "[class][split-measure]")
{
    for (const auto& [fixture, first, second] : {
             std::tuple{"evidence/F100/F100-splitpoints.mus", 168, 392},
             std::tuple{"evidence/F2002/F2002-splitpoints.mus", 234, 396},
         }) {
        const auto result = readFixture(fixture);
        const auto split = result.document->getOthers()->get<SplitMeasure>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(split);
        REQUIRE(split->values.size() == 2);
        CHECK(split->values[0] == first);
        CHECK(split->values[1] == second);
        for (std::size_t index = 0; index < 2; ++index) {
            const auto* field =
                result.report.findField<SplitMeasure>("values[" + std::to_string(index) + "]", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
            REQUIRE(field);
            CHECK(field->origin == ValueOrigin::LegacyMus);
            CHECK(field->rawValue == (index == 0 ? first : second));
            CHECK(field->sourceIdentity == finale_mus_reader::records::packTag("SM"));
        }
    }
    CHECK(SplitMeasure::xmlMappingArray().size() == 1);
}

TEST_CASE("Split measure is absent when no position record is stored", "[class][split-measure]")
{
    for (const auto* fixture : {"evidence/F100/F100-baseline.mus", "evidence/F2002/F2002-baseline.mus", "evidence/F2012/F2012-baseline.mus"}) {
        const auto result = readFixture(fixture);
        CHECK(result.document->getOthers()->getAllSources<SplitMeasure>().empty());
    }
}

TEST_CASE("Split measure imports the Finale 2012 class record", "[class][split-measure]")
{
    const auto result = readFixture("evidence/F2012/F2012-splitpoints.mus");
    const auto split = result.document->getOthers()->get<SplitMeasure>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(split);
    CHECK(split->values == std::vector<musx::dom::Evpu>{328});
    const auto* field = result.report.findField<SplitMeasure>("values[0]", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(field);
    CHECK(field->origin == ValueOrigin::LegacyMus);
    CHECK(field->rawValue == 328);
    CHECK(field->sourceIdentity == 0x00dd);
}

TEST_CASE("A score SplitMeasure is shared with its linked part", "[class][split-measure]")
{
    const auto result = readFixture("evidence/F2012/F2012-splitpoints-parts.mus");
    const auto score = result.document->getOthers()->get<SplitMeasure>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    const auto part = result.document->getOthers()->get<SplitMeasure>(musx::dom::Cmper(1), musx::dom::Cmper(1));
    REQUIRE(score);
    REQUIRE(part);
    CHECK(score->getShareMode() == musx::dom::EnigmaBase::ShareMode::All);
    CHECK(part->getShareMode() == musx::dom::EnigmaBase::ShareMode::All);
    CHECK(score->getSourcePartId() == musx::dom::SCORE_PARTID);
    CHECK(part->getSourcePartId() == musx::dom::SCORE_PARTID);
    CHECK(score->values == std::vector<musx::dom::Evpu>{221});
    CHECK(part->values == std::vector<musx::dom::Evpu>{221});
    CHECK(result.document->getOthers()->getAllSources<SplitMeasure>().size() == 1);
    const auto* field = result.report.findField<SplitMeasure>("values[0]", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(field);
    CHECK(field->origin == ValueOrigin::LegacyMus);
    CHECK(field->rawValue == 221);
}

TEST_CASE("Split measure uses the fixed-row layout in the uncompressed epoch", "[class][split-measure]")
{
    const auto parsed = makeContainer({{1, "SM", {112, 368, 0, 0, 0, 0}}}, FormatEpoch::UncompressedLegacy);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    SourceProfile profile(FormatEpoch::UncompressedLegacy);
    profile.byteOrder = parsed.byteOrder;
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importSplitMeasures(context);
    const auto split = document->getOthers()->get<SplitMeasure>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(split);
    CHECK(split->values == std::vector<musx::dom::Evpu>{112, 368});
}

} // namespace
} // namespace finale_mus_reader_tests
