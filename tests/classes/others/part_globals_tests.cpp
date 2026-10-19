// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

using PartGlobals = musx::dom::others::PartGlobals;

ImportReport partGlobalsImport(
    const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile, const musx::dom::DocumentPtr& document)
{
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importPartGlobals(context);
    return report;
}

musx::dom::DocumentPtr emptyPartGlobalsDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    return session.getDocument();
}

TEST_CASE("Pre-zlib part globals recover their scattered option words in every "
          "epoch")
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        const auto parsed = makeContainer({{GLOBALS_CMPER, "12", {0, 0, 0, 0, 0, 0}}, {GLOBALS_CMPER, "23", {0, 0, 0, 0, 17, 0}}}, epoch);
        const auto document = emptyPartGlobalsDocument();
        const auto report = partGlobalsImport(parsed, SourceProfile(epoch), document);
        const auto globals = document->getOthers()->get<PartGlobals>(musx::dom::SCORE_PARTID, musx::dom::MUSX_GLOBALS_CMPER);
        expect(globals != nullptr, "The pre-zlib score part globals were not constructed");
        expect(!globals->showTransposed, "Display Concert Pitch did not clear showTransposed");
        expect(globals->scrollViewIUlist == musx::dom::BASE_SYSTEM_ID && globals->studioViewIUlist == musx::dom::STUDIO_VIEW_SYSTEM_ID,
            "A pre-zlib view did not take its era behavior");
        expect(globals->specialPartExtractionIUList == 17, "The pre-zlib extraction staff-list comparator was not recovered");
        expect(globals->getShareMode() == musx::dom::EnigmaBase::ShareMode::All, "The pre-zlib score instance was not shared to its score");
        expect(field(report, "others.partGlobals[65534].showTransposed").origin == ValueOrigin::LegacyMus,
            "The recovered concert-pitch setting has the wrong origin");
        expect(field(report, "others.partGlobals[65534].specialPartExtractionIUList").origin == ValueOrigin::LegacyMus,
            "The recovered extraction list has the wrong origin");
        for (const auto* member : {"scrollViewIUlist", "studioViewIUlist"}) {
            expect(field(report, std::string("others.partGlobals[65534].") + member).origin == ValueOrigin::LegacyBehavior,
                std::string("A pre-zlib view did not report legacy behavior: ") + member);
        }
        expect(reportedFieldCount(report) == 4, "The pre-zlib report does not exhaust the PartGlobals field manifest");
    }
}

TEST_CASE("Zlib part globals recover one complete record per score and linked part")
{
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        const auto parsed = makeClassContainer(
            {SyntheticClassRow{0x0120, {1, 0, -136, 0, 0, 0}, GLOBALS_CMPER, 0}, SyntheticClassRow{0x0120, {0, 7, -136, 9, 0, 0}, GLOBALS_CMPER, 3}},
            byteOrder);
        const auto document = emptyPartGlobalsDocument();
        SourceProfile profile(FormatEpoch::ZlibLegacy);
        profile.byteOrder = byteOrder;
        const auto report = partGlobalsImport(parsed, profile, document);

        const auto score = document->getOthers()->get<PartGlobals>(musx::dom::SCORE_PARTID, GLOBALS_CMPER);
        const auto part = document->getOthers()->get<PartGlobals>(3, GLOBALS_CMPER);
        expect(score && score->showTransposed && score->scrollViewIUlist == 0 && score->studioViewIUlist == musx::dom::STUDIO_VIEW_SYSTEM_ID
                   && score->specialPartExtractionIUList == 0,
            "The zlib score record was not decoded in field order");
        expect(part && !part->showTransposed && part->scrollViewIUlist == 7 && part->studioViewIUlist == musx::dom::STUDIO_VIEW_SYSTEM_ID
                   && part->specialPartExtractionIUList == 9,
            "The zlib linked-part record was not decoded in field order");
        expect(score->getShareMode() == musx::dom::EnigmaBase::ShareMode::All && part->getShareMode() == musx::dom::EnigmaBase::ShareMode::None,
            "PartGlobals sharing was not taken from the source record");
        for (const auto* member : {"showTransposed", "scrollViewIUlist", "studioViewIUlist", "specialPartExtractionIUList"}) {
            const auto* reported = report.findField<PartGlobals>(member, 3, GLOBALS_CMPER);
            expect(reported && reported->origin == ValueOrigin::LegacyMus, std::string("A zlib part-global member has the wrong origin: ") + member);
        }
        expect(reportedFieldCount(report) == 8, "The zlib report does not exhaust both PartGlobals field manifests");
    }
}

TEST_CASE("A short zlib part-globals record is rejected without a partial object")
{
    const auto parsed = makeClassContainer({SyntheticClassRow{0x0120, {1, 2, 3, 4, 5}, GLOBALS_CMPER, 0}}, ByteOrder::BigEndian);
    const auto document = emptyPartGlobalsDocument();
    SourceProfile profile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    const auto report = partGlobalsImport(parsed, profile, document);
    expect(document->getOthers()->getAllSources<PartGlobals>().empty(), "A truncated PartGlobals record created a partial object");
    expect(report.diagnostics.size() == 1, "A truncated PartGlobals record was not diagnosed once");
}

TEST_CASE("Coda part globals do not infer the current Staff Set from an authored list")
{
    const auto parsed =
        makeContainer({{GLOBALS_CMPER, "12", {1, 0, 0, 0, 0, 0}}, {GLOBALS_CMPER, "23", {0, 0, 0, 0, 0, 0}}, {65531, "IU", {2, 0, -80, 0, 0, 0}}},
            FormatEpoch::CodaBanner);
    const auto document = emptyPartGlobalsDocument();
    const auto report = partGlobalsImport(parsed, SourceProfile(FormatEpoch::CodaBanner), document);
    const auto globals = document->getOthers()->get<PartGlobals>(musx::dom::SCORE_PARTID, GLOBALS_CMPER);
    expect(globals && globals->scrollViewIUlist == musx::dom::BASE_SYSTEM_ID, "A Coda Staff Set list was mistaken for the current view selection");
    const auto& source = field(report, "others.partGlobals[65534].scrollViewIUlist");
    expect(source.origin == ValueOrigin::LegacyBehavior, "The absent Coda view selection was reported as recovered content");
}

TEST_CASE("Part globals recover from controlled fixtures across every physical "
          "layout")
{
    for (const auto* relative : {"evidence/F263/F263-baseline.mus", "evidence/F372/F372-baseline.mus", "evidence/F2002/F2002-empty.mus",
             "evidence/F2007/F2007-lyric-hyphens.mus", "evidence/F2012/F2012-baseline.mus"}) {
        const auto result = readFixture(relative);
        const auto globals = result.document->getOthers()->get<PartGlobals>(musx::dom::SCORE_PARTID, GLOBALS_CMPER);
        expect(globals && globals->showTransposed && globals->scrollViewIUlist == musx::dom::BASE_SYSTEM_ID
                   && globals->studioViewIUlist == musx::dom::STUDIO_VIEW_SYSTEM_ID && globals->specialPartExtractionIUList == 0,
            std::string("PartGlobals disagrees with the fixture companion: ") + relative);
    }

    const auto concert = readFixture("evidence/F263/F263-concert-pitch.mus");
    const auto concertGlobals = concert.document->getOthers()->get<PartGlobals>(musx::dom::SCORE_PARTID, GLOBALS_CMPER);
    expect(concertGlobals && !concertGlobals->showTransposed, "The controlled Display Concert Pitch edit was not recovered");

    const auto quartet = readFixture("evidence/F100/F100-quartet.mus");
    const auto quartetGlobals = quartet.document->getOthers()->get<PartGlobals>(musx::dom::SCORE_PARTID, GLOBALS_CMPER);
    expect(quartetGlobals && quartetGlobals->scrollViewIUlist == musx::dom::BASE_SYSTEM_ID,
        "The Coda quartet baseline did not use the full Scroll View staff list");

    const auto oboeView = readFixture("evidence/F100/F100-quartet-oboeview.mus");
    const auto oboeViewGlobals = oboeView.document->getOthers()->get<PartGlobals>(musx::dom::SCORE_PARTID, GLOBALS_CMPER);
    expect(oboeViewGlobals && oboeViewGlobals->scrollViewIUlist == musx::dom::BASE_SYSTEM_ID,
        "The Coda quartet's authored Staff Set affected the current view selection");
    const auto* oboeViewSource = oboeView.report.findField<PartGlobals>("scrollViewIUlist", musx::dom::SCORE_PARTID, GLOBALS_CMPER);
    expect(oboeViewSource && oboeViewSource->origin == ValueOrigin::LegacyBehavior, "The absent Coda view selection has the wrong origin");

    const auto linked = readFixture("evidence/F2012/F2012-noteartexp.mus");
    const auto instances = linked.document->getOthers()->getAllSources<PartGlobals>();
    expect(instances.size() == 2, "The linked-part fixture did not recover score "
                                  "and part PartGlobals records");
    const auto part = linked.document->getOthers()->get<PartGlobals>(1, GLOBALS_CMPER);
    expect(part && part->showTransposed && part->getShareMode() == musx::dom::EnigmaBase::ShareMode::None,
        "The linked part did not synthesize its unshared PartGlobals defaults");
}

} // namespace
} // namespace finale_mus_reader_tests
