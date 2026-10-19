// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using EntryAssignTestTarget = musx::dom::details::SmartShapeEntryAssign;
using EntryCmper = musx::dom::Cmper;
using EntryInci = musx::dom::Inci;

TEST_CASE("SmartShape entry class tuple decodes in either byte order", "[class][smart-shape-entry-assign]")
{
    for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        const auto parsed = makeDetailClassContainer(0, 27, 0, {45, 7, 0, 0, 0}, order, 0x0428);
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
        finale_mus_reader::details::importSmartShapeEntryAssigns(context);
        finale_mus_reader::runDeferredChecks(pending);
        const auto assignment = document->getDetails()->get<EntryAssignTestTarget>(musx::dom::SCORE_PARTID, 27, EntryInci(0));
        REQUIRE(assignment);
        CHECK(assignment->shapeNum == 45);
    }
}

TEST_CASE("SmartShape entry assignments retain their shape identifiers", "[class][smart-shape-entry-assign]")
{
    const auto result = readFixture("evidence/F2006/F2006-lyric-vcs.mus");
    const auto assignments = result.document->getDetails()->getArray<EntryAssignTestTarget>(musx::dom::SCORE_PARTID, 27);
    REQUIRE(assignments.size() == 3);
    CHECK(assignments[0]->shapeNum == 4);
    CHECK(assignments[1]->shapeNum == 9);
    CHECK(assignments[2]->shapeNum == 16);
    const auto key = finale_mus_reader::instanceKey<EntryAssignTestTarget>(musx::dom::SCORE_PARTID, EntryCmper(0), EntryInci(1), EntryCmper(27));
    const auto* shapeNum = result.report.findField(key, "shapeNum");
    REQUIRE(shapeNum);
    CHECK(shapeNum->origin == ValueOrigin::LegacyMus);
    CHECK(shapeNum->rawValue == 9);
}

TEST_CASE("Pre-Sx shapes have no entry assignments", "[class][smart-shape-entry-assign]")
{
    const auto result = readFixture("evidence/F263/F263-slur.mus");
    CHECK(result.document->getDetails()->getAllSources<EntryAssignTestTarget>().empty());
}

// Lyric-attached shapes record their entries as lyric marks, which become ordinary entry
// assignments. The fixture's verse lyric assignments sit on the same entries and are not marks.
TEST_CASE("Zlib lyric marks become SmartShape entry assignments", "[class][smart-shape-entry-assign]")
{
    const auto result = readFixture("evidence/F2007/F2007-lyric-hyphens.mus");
    const auto first = result.document->getDetails()->getArray<EntryAssignTestTarget>(musx::dom::SCORE_PARTID, 13);
    const auto second = result.document->getDetails()->getArray<EntryAssignTestTarget>(musx::dom::SCORE_PARTID, 15);
    REQUIRE(first.size() == 1);
    REQUIRE(second.size() == 3);
    CHECK(first[0]->shapeNum == 1);
    CHECK(second[0]->shapeNum == 1);
    CHECK(second[1]->shapeNum == 2);
    CHECK(second[2]->shapeNum == 2);
    const auto key = finale_mus_reader::instanceKey<EntryAssignTestTarget>(musx::dom::SCORE_PARTID, EntryCmper(0), EntryInci(0), EntryCmper(13));
    const auto* field = result.report.findField(key, "shapeNum");
    REQUIRE(field);
    CHECK(field->sourceIdentity == 0x0429);
}

// A lyric mark follows the entry's ordinary assignments.
TEST_CASE("Fixed-row lyric marks follow ordinary SmartShape entry assignments", "[class][smart-shape-entry-assign]")
{
    const auto result = readFixture("evidence/F2006/F2006-lyric-vcs.mus");
    const auto marks = result.document->getDetails()->getArray<EntryAssignTestTarget>(musx::dom::SCORE_PARTID, 17);
    REQUIRE(marks.size() == 3);
    CHECK(marks[0]->shapeNum == 1);
    CHECK(marks[1]->shapeNum == 11);
    CHECK(marks[2]->shapeNum == 14);
    CHECK(result.document->getDetails()->getArray<EntryAssignTestTarget>(musx::dom::SCORE_PARTID, 22).size() == 9);
}

TEST_CASE("Later zlib SmartShape entry class follows moved slur endpoints", "[class][smart-shape-entry-assign]")
{
    const auto baseline = readFixture("evidence/F2012/F2012-slur-part.mus");
    const auto changed = readFixture("evidence/F2012/F2012-slur-diffents.mus");
    const auto baselineStart = baseline.document->getDetails()->get<EntryAssignTestTarget>(musx::dom::SCORE_PARTID, 5, EntryInci(0));
    const auto changedStart = changed.document->getDetails()->get<EntryAssignTestTarget>(musx::dom::SCORE_PARTID, 6, EntryInci(0));
    const auto changedEnd = changed.document->getDetails()->get<EntryAssignTestTarget>(musx::dom::SCORE_PARTID, 7, EntryInci(0));
    REQUIRE(baselineStart);
    REQUIRE(changedStart);
    REQUIRE(changedEnd);
    CHECK(baselineStart->shapeNum == 1);
    CHECK(changedStart->shapeNum == 1);
    CHECK(changedEnd->shapeNum == 1);
    CHECK_FALSE(changed.document->getDetails()->get<EntryAssignTestTarget>(musx::dom::SCORE_PARTID, 5, EntryInci(0)));
    const auto key = finale_mus_reader::instanceKey<EntryAssignTestTarget>(musx::dom::SCORE_PARTID, EntryCmper(0), EntryInci(0), EntryCmper(7));
    const auto* field = changed.report.findField(key, "shapeNum");
    REQUIRE(field);
    CHECK(field->sourceIdentity == 0x0428);
}

// A pre-2011 zlib document holds ordinary assignments and lyric marks side by side.
TEST_CASE("Zlib lyric marks and ordinary assignments occur in one document", "[class][smart-shape-entry-assign]")
{
    const auto result = readFixture("evidence/F2008/F2008-F2006-lyric-vcs.mus");
    const auto lyricMarks = result.document->getDetails()->getArray<EntryAssignTestTarget>(musx::dom::SCORE_PARTID, 17);
    const auto ordinary = result.document->getDetails()->get<EntryAssignTestTarget>(musx::dom::SCORE_PARTID, 27, EntryInci(0));
    REQUIRE(lyricMarks.size() == 3);
    REQUIRE(ordinary);
    CHECK(lyricMarks[0]->shapeNum == 1);
    CHECK(lyricMarks[1]->shapeNum == 11);
    CHECK(lyricMarks[2]->shapeNum == 14);
    CHECK(ordinary->shapeNum == 4);
    const auto lyricKey = finale_mus_reader::instanceKey<EntryAssignTestTarget>(musx::dom::SCORE_PARTID, EntryCmper(0), EntryInci(0), EntryCmper(17));
    const auto ordinaryKey =
        finale_mus_reader::instanceKey<EntryAssignTestTarget>(musx::dom::SCORE_PARTID, EntryCmper(0), EntryInci(0), EntryCmper(27));
    REQUIRE(result.report.findField(lyricKey, "shapeNum"));
    REQUIRE(result.report.findField(ordinaryKey, "shapeNum"));
    CHECK(result.report.findField(lyricKey, "shapeNum")->sourceIdentity == 0x0429);
    CHECK(result.report.findField(ordinaryKey, "shapeNum")->sourceIdentity == 0x0428);
}

TEST_CASE("Entries Finale's conversion marks take the SmartShape flag", "[class][smart-shape-entry-assign]")
{
    for (const char* fixture : {"evidence/F2006/F2006-lyric-vcs.mus", "evidence/F2008/F2008-F2006-lyric-vcs.mus"}) {
        const auto result = readFixture(fixture);
        for (const musx::dom::EntryNumber number : {17, 22}) {
            const auto entry = result.document->getEntries()->get(number);
            REQUIRE(entry);
            CHECK(entry->smartShapeDetail);
            CHECK_FALSE(result.document->getDetails()->getArray<EntryAssignTestTarget>(musx::dom::SCORE_PARTID, number).empty());
            const auto* field = result.report.findField<musx::dom::Entry>(
                "smartShapeDetail", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(number));
            REQUIRE(field);
            CHECK(field->origin == ValueOrigin::LegacyMusAdjusted);
        }
    }
}

} // namespace
} // namespace finale_mus_reader_tests
