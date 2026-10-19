// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using SystemLock = musx::dom::others::SystemLock;

TEST_CASE("System locks recover fixed-row source measure spans", "[class][system-lock]")
{
    for (const auto& [fixture, start, end] : {
             std::tuple{"evidence/F372/F372-measure-graphic.mus", musx::dom::Cmper(2), musx::dom::MeasCmper(6)},
             std::tuple{"evidence/F2002/F2002-fileinfo-text.mus", musx::dom::Cmper(111), musx::dom::MeasCmper(118)},
         }) {
        const auto result = readFixture(fixture);
        const auto lock = result.document->getOthers()->get<SystemLock>(musx::dom::SCORE_PARTID, start);
        REQUIRE(lock);
        CHECK(lock->endMeas == end);
        CHECK(lock->getShareMode() == musx::dom::EnigmaBase::ShareMode::All);
        const auto* field = result.report.findField<SystemLock>("endMeas", musx::dom::SCORE_PARTID, start);
        REQUIRE(field);
        CHECK(field->origin == ValueOrigin::LegacyMus);
        CHECK(field->rawValue == end);
    }
    CHECK(SystemLock::xmlMappingArray().size() == 1);
}

TEST_CASE("System lock word one and filler do not affect the recovered span", "[class][system-lock]")
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        const auto parsed = makeContainer({{2, "FM", {6, 1, 2, 3, 4, 5}}}, epoch);
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        SourceProfile profile(epoch);
        profile.byteOrder = parsed.byteOrder;
        ImportReport report(profile.epoch);
        const auto index = LegacyRecordIndex::build(parsed);
        auto referenceSession = musx::factory::DocumentFactory::begin();
        const auto reference = std::move(referenceSession).finish();
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::others::importSystemLocks(context);
        const auto lock = document->getOthers()->get<SystemLock>(musx::dom::SCORE_PARTID, musx::dom::Cmper(2));
        REQUIRE(lock);
        CHECK(lock->endMeas == 6);
    }
}

TEST_CASE("System lock recovers the Finale 2008 class record", "[class][system-lock]")
{
    const auto result = readFixture("evidence/F2008/F2008-syslock.mus");
    const auto lock = result.document->getOthers()->get<SystemLock>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(lock);
    CHECK(lock->endMeas == 2);
    const auto* field = result.report.findField<SystemLock>("endMeas", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(field);
    CHECK(field->origin == ValueOrigin::LegacyMus);
    CHECK(field->rawValue == 2);
    CHECK(field->sourceIdentity == 0x0093);
}

TEST_CASE("Finale 1.0 system lock spans both measures of the controlled edit", "[class][system-lock]")
{
    const auto baseline = readFixture("evidence/F100/F100-baseline.mus");
    CHECK(baseline.document->getOthers()->getAllSources<SystemLock>().empty());
    CHECK(baseline.document->getOthers()->getAllSources<musx::dom::others::Measure>().size() == 1);

    const auto edited = readFixture("evidence/F100/F100-syslock.mus");
    CHECK(edited.document->getOthers()->getAllSources<musx::dom::others::Measure>().size() == 2);
    const auto lock = edited.document->getOthers()->get<SystemLock>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(lock);
    CHECK(lock->endMeas == 3);
    const auto* field = edited.report.findField<SystemLock>("endMeas", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(field);
    CHECK(field->origin == ValueOrigin::LegacyMus);
    CHECK(field->rawValue == 3);
    CHECK(field->sourceIdentity == finale_mus_reader::records::packTag("FM"));
}

TEST_CASE("Documents without a SystemLock record have no lock object", "[class][system-lock]")
{
    for (const auto* fixture : {"evidence/F263/F263-baseline.mus", "evidence/F2008/F2008-parts.mus"}) {
        const auto result = readFixture(fixture);
        CHECK(result.document->getOthers()->getAllSources<SystemLock>().empty());
    }
}

} // namespace
} // namespace finale_mus_reader_tests
