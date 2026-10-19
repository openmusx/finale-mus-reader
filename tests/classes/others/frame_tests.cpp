// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using Frame = musx::dom::others::Frame;

TEST_CASE("Frame recovers a pickup spacer and entry range in both fixed-row byte orders", "[class][frame]")
{
    for (const auto& [fixture, startTime] : {
             std::pair{"evidence/F100/F100-8th-pickup.mus", musx::dom::Edu(3584)},
             std::pair{"evidence/F2001/F2001Win-legacypickup.mus", musx::dom::Edu(3072)},
         }) {
        const auto result = readFixture(fixture);
        const auto spacer = result.document->getOthers()->get<Frame>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(0));
        const auto entries = result.document->getOthers()->get<Frame>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(1));
        REQUIRE(spacer);
        REQUIRE(entries);
        CHECK(spacer->startTime == startTime);
        CHECK(spacer->startEntry == 0);
        CHECK(spacer->endEntry == 0);
        CHECK(entries->startTime == 0);
        CHECK(entries->startEntry == 1);
        CHECK(entries->endEntry == 1);
        const auto spacerKey = finale_mus_reader::instanceKey<Frame>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(0));
        const auto entryKey = finale_mus_reader::instanceKey<Frame>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(1));
        CHECK(result.report.fields.at(spacerKey).size() == Frame::xmlMappingArray().size());
        CHECK(result.report.fields.at(entryKey).size() == Frame::xmlMappingArray().size());
        CHECK(result.report.fields.at(spacerKey).at("startTime").origin == ValueOrigin::LegacyMus);
        CHECK(result.report.fields.at(entryKey).at("startEntry").origin == ValueOrigin::LegacyMus);
    }
}

TEST_CASE("Frame recovers zlib entry range records", "[class][frame]")
{
    const auto result = readFixture("evidence/F2012/F2012-mirrorfromFin14.mus");
    const auto frame = result.document->getOthers()->get<Frame>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(0));
    REQUIRE(frame);
    CHECK(frame->startEntry == 1);
    CHECK(frame->endEntry == 4);
    CHECK(frame->startTime == 0);
    const auto key = finale_mus_reader::instanceKey<Frame>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(0));
    CHECK(result.report.fields.at(key).size() == Frame::xmlMappingArray().size());
    CHECK(result.report.fields.at(key).at("startEntry").sourceIdentity == 0x0092);
}

TEST_CASE("Frame separates a zlib pickup spacer from its entry range", "[class][frame]")
{
    const auto result = readFixture("evidence/F2008/F2008-16thpickup.mus");
    const auto spacer = result.document->getOthers()->get<Frame>(musx::dom::SCORE_PARTID, musx::dom::Cmper(2), musx::dom::Inci(0));
    const auto entries = result.document->getOthers()->get<Frame>(musx::dom::SCORE_PARTID, musx::dom::Cmper(2), musx::dom::Inci(1));
    REQUIRE(spacer);
    REQUIRE(entries);
    CHECK(spacer->startTime == 3840);
    CHECK(spacer->startEntry == 0);
    CHECK(spacer->endEntry == 0);
    CHECK(entries->startTime == 0);
    CHECK(entries->startEntry == 5);
    CHECK(entries->endEntry == 5);
    const auto spacerKey = finale_mus_reader::instanceKey<Frame>(musx::dom::SCORE_PARTID, musx::dom::Cmper(2), musx::dom::Inci(0));
    const auto entryKey = finale_mus_reader::instanceKey<Frame>(musx::dom::SCORE_PARTID, musx::dom::Cmper(2), musx::dom::Inci(1));
    CHECK(result.report.fields.at(spacerKey).at("startTime").rawValue == 3840);
    CHECK(result.report.fields.at(entryKey).at("startEntry").rawValue == 5);
}

TEST_CASE("Frame identifies an uncompressed spacer independently of incidence order", "[class][frame]")
{
    const auto parsed = makeContainer({{7, "FR", {0, 8, 0, 9, 0, 0}}, {7, "FR", {0, 3072, 0, 0, 0, 2048}}, {8, "FR", {0, 10, 0, 11, 0, 64}}});
    const auto index = LegacyRecordIndex::build(parsed);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    ImportReport report(parsed.formatEpoch);
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importFrames(context);
    const auto entries = document->getOthers()->get<Frame>(musx::dom::SCORE_PARTID, musx::dom::Cmper(7), musx::dom::Inci(0));
    const auto spacer = document->getOthers()->get<Frame>(musx::dom::SCORE_PARTID, musx::dom::Cmper(7), musx::dom::Inci(1));
    REQUIRE(entries);
    REQUIRE(spacer);
    CHECK(entries->startEntry == 8);
    CHECK(entries->endEntry == 9);
    CHECK(spacer->startTime == 3072);
    CHECK_FALSE(document->getOthers()->get<Frame>(musx::dom::SCORE_PARTID, musx::dom::Cmper(8), musx::dom::Inci(0)));
    CHECK_FALSE(report.diagnostics.empty());
}

} // namespace
} // namespace finale_mus_reader_tests
