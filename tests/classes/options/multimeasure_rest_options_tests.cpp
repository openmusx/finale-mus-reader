// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

/// @brief A document whose MultimeasureRestOptions carries the pinned baseline's values.
/// @details The three the early era cannot state are what matter: the baseline starts the
/// H-bar 30 Evpu in, ends it 30 Evpu out, and switches automatic updating on.
///
/// `noHorizontalStretch` is seeded **true**, which the pinned baseline is not. That option
/// arrived with Finale 27, so no legacy document can state it and the reader must assert it
/// false rather than take the baseline's word; seeding the baseline's own false would let an
/// implementation that merely inherited pass this test.
musx::dom::DocumentPtr makeMmRestDocument()
{
    using MmRest = musx::dom::options::MultimeasureRestOptions;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<MmRest>(document);
    options->measWidth = 360;
    options->numAdjY = -32;
    options->shapeDef = 3;
    options->numStart = 2;
    options->useSymsThreshold = 9;
    options->symSpacing = 48;
    options->startAdjust = 30;
    options->endAdjust = -30;
    options->autoUpdateMmRests = true;
    options->noHorizontalStretch = true;
    document->getOptions()->add(MmRest::XmlNodeName, options);
    return std::move(session).finish();
}

// Finale 3.5 rewrote the multimeasure-rest record, and the reader decides that boundary from
// the size of the selector-25 family rather than from the version. The boundary falls inside
// the uncompressed epoch, so no epoch gate can express it. The covered releases do not predate
// Finale 3.5, so both shapes are built here, and each is read under versions that would
// contradict the marker if the marker were not what decides.
void testMmRestEarlyLayoutMarker()
{
    using MmRest = musx::dom::options::MultimeasureRestOptions;
    // The shape, when defined, is added after the options importer runs, as the others pool is
    // in a real import, so the H-bar check only finds it if it waits for the deferred phase.
    const auto runImport = [](const std::vector<SyntheticRow>& rows, const SourceProfile& profile, ImportReport& report,
                               std::optional<musx::dom::Cmper> definedShape = std::nullopt) {
        const auto document = makeMmRestDocument();
        const auto reference = makeMmRestDocument();
        const auto index = LegacyRecordIndex::build(makeContainer(rows));
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::options::importMultimeasureRestOptions(context);
        if (definedShape) {
            document->getOthers()->add(
                musx::dom::others::ShapeDef::XmlNodeName, std::make_shared<musx::dom::others::ShapeDef>(document, musx::dom::SCORE_PARTID,
                                                              musx::dom::EnigmaBase::ShareMode::All, *definedShape));
        }
        finale_mus_reader::runDeferredChecks(pending);
        return document->getOptions()->get<MmRest>();
    };
    const auto notesHBar = [](const ImportReport& report) {
        return std::any_of(report.diagnostics.begin(), report.diagnostics.end(), [](const finale_mus_reader::Diagnostic& entry) {
            return entry.level == musx::util::Logger::LogLevel::Verbose && entry.message.find("H-bar") != std::string::npos;
        });
    };

    // The early record: one incidence, the adjustment and shape in slots 4 and 5. Read through
    // the later table these same words would give a shape of 7 and a "start number at" of -20.
    const std::vector<SyntheticRow> early{{GLOBALS_CMPER, "25", {320, 3, 24, 7, -20, 5}}};
    // The later record: two incidences, everything in the framework's places.
    const std::vector<SyntheticRow> later{
        {GLOBALS_CMPER, "25", {216, 0, -20, 5, 3, 11}},
        {GLOBALS_CMPER, "25", {60, -4, 12, -12, 0, 1}},
    };

    auto coda = profileFor(2, 6);
    coda.epoch = FormatEpoch::CodaBanner;
    coda.version.reset();
    // Finale 3.0 and 3.2 by version, one version from well past the boundary, and a
    // Coda-banner file that states no version at all.
    for (const auto& profile : {profileFor(3, 0), profileFor(3, 2), profileFor(15, 0), coda}) {
        ImportReport report(FormatEpoch::UncompressedLegacy);
        const auto options = runImport(early, profile, report);
        expectMapping(options->measWidth == 320 && options->numAdjY == -20 && options->shapeDef == 5,
            "The early multimeasure-rest layout was not read from its own slots");
        expectMapping(options->numStart == 2 && options->useSymsThreshold == 9 && options->symSpacing == 48,
            "The early layout disturbed a field the baseline supplies");
        // The three the baseline gets wrong for this era, plus the Finale 27 option no legacy
        // era has at all.
        expectMapping(options->startAdjust == 0 && options->endAdjust == 0 && !options->autoUpdateMmRests && !options->noHorizontalStretch,
            "An early document inherited a value its era cannot have stated");
        expectMapping(field(report, "options.multimeasureRestOptions.startAdjust").origin == ValueOrigin::LegacyBehavior,
            "An asserted early value was not reported as era behavior");
        expectMapping(field(report, "options.multimeasureRestOptions.symSpacing").origin == ValueOrigin::Finale27Default,
            "An early field the baseline supplies was claimed as read");
        // These documents recover shape 5 and define no shapes at all. The dangling reference is
        // kept and noted, only at Verbose.
        expectMapping(notesHBar(report), "A multimeasure-rest H-bar naming an undefined shape was not noted at Verbose");
        // Once the shape pool defines it, the same reference is not dangling.
        ImportReport definedReport(FormatEpoch::UncompressedLegacy);
        runImport(early, profile, definedReport, musx::dom::Cmper(5));
        expectMapping(!notesHBar(definedReport), "A multimeasure-rest H-bar naming a defined shape was noted as undefined");
    }

    // The same reader on the two-incidence record, including under versions that predate the
    // boundary and under the Coda-banner epoch. Whatever a file claims to be, the record it
    // carries is what decides: the epoch mask says only that these are 16-byte rows.
    for (const auto& profile : {profileFor(3, 0), profileFor(3, 5), profileFor(15, 0), coda}) {
        ImportReport report(FormatEpoch::UncompressedLegacy);
        const auto options = runImport(later, profile, report);
        expectMapping(options->measWidth == 216 && options->numAdjY == -20 && options->shapeDef == 5 && options->numStart == 3
                          && options->useSymsThreshold == 11,
            "The later multimeasure-rest layout was not read from its own slots");
        expectMapping(
            options->symSpacing == 60 && options->numAdjX == -4 && options->startAdjust == 12 && options->endAdjust == -12 && options->useSymbols,
            "The later layout's second incidence was not read");
        expectMapping(field(report, "options.multimeasureRestOptions.startAdjust").origin == ValueOrigin::LegacyMus,
            "A recovered H-bar adjustment was reported as era behavior");
        // Asserted in every era, not only the early one, because no legacy format has it.
        expectMapping(!options->noHorizontalStretch
                          && field(report, "options.multimeasureRestOptions.noHorizontalStretch").origin == ValueOrigin::LegacyBehavior,
            "A later-layout document inherited the baseline's horizontal-stretch setting");
        // Selector 83 is still absent, which is what a Finale 3.5 or 3.7 document looks like.
        expectMapping(
            !options->autoUpdateMmRests && field(report, "options.multimeasureRestOptions.autoUpdateMmRests").origin == ValueOrigin::LegacyBehavior,
            "A document without selector 83 kept the baseline's automatic-update setting");
    }

    // With the selector present, the flag is read rather than asserted, and from word 4.
    ImportReport report(FormatEpoch::UncompressedLegacy);
    auto withUpdate = later;
    withUpdate.push_back({GLOBALS_CMPER, "83", {0, 0, 1, 0, 1, 0}});
    const auto options = runImport(withUpdate, profileFor(5, 0), report);
    expectMapping(options->autoUpdateMmRests && field(report, "options.multimeasureRestOptions.autoUpdateMmRests").origin == ValueOrigin::LegacyMus,
        "The automatic-update word was not read from selector 83");
}

void testMultimeasureRestRecovery()
{
    using MmRest = musx::dom::options::MultimeasureRestOptions;
    const auto read = [](const char* relative) {
        return Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / relative);
    };
    struct Expected
    {
        const char* path;
        const char* era;
        FormatEpoch epoch;
        int measWidth;
        int numAdjY;
        int shapeDef;
        int startAdjust;
        int endAdjust;
        bool useSymbols;
        bool autoUpdate;
    };
    // The early layout first: two Coda-banner documents whose slots 4 and 5 differ from each
    // other, which is what shows the pair is read rather than guessed. Read through the later
    // table, the Finale 2.6.3 fixture would report a shape of 0 and a number adjustment of 24.
    const Expected fixtures[] = {
        {"evidence/F100/F100-baseline.mus", "Finale 1.0.0", FormatEpoch::CodaBanner, 360, 12, 0, 0, 0, false, false},
        {"evidence/F263/F263-baseline.mus", "Finale 2.6.3", FormatEpoch::CodaBanner, 320, -28, 1, 0, 0, false, false},
        // The later fixed-row layout, across both epochs that carry it. Finale 3.7.2 has the
        // two-incidence record but no selector 83 at all, and Finale 97 is the one tracked
        // fixture that sets the character-rest-style flag.
        {"evidence/F372/F372-baseline.mus", "Finale 3.7.2", FormatEpoch::UncompressedLegacy, 360, -12, 1, 0, 0, false, false},
        {"evidence/F97/Fin97-baseline.mus", "Finale 97", FormatEpoch::UncompressedLegacy, 216, -28, 1, 0, 0, true, false},
        {"evidence/F2000/F2000-baseline.mus", "Finale 2000", FormatEpoch::UncompressedLegacy, 360, -12, 1, 0, 0, false, false},
        {"evidence/F2005/F2005-baseline.mus", "Finale 2005", FormatEpoch::DclLegacy, 360, -12, 1, 0, 0, false, false},
        // The zlib era in both byte orders. Finale 2012 is the fixture that exercises the
        // second row through the class encoding: it is the only one whose H-bar adjustments
        // are not zero, and reading them from the wrong offsets would leave them so.
        {"evidence/F2007/F2007-lyric-hyphens.mus", "Finale 2007", FormatEpoch::ZlibLegacy, 360, -12, 1, 0, 0, false, true},
        {"evidence/F2012/F2012-upstem-flags.mus", "Finale 2012", FormatEpoch::ZlibLegacy, 360, -32, 1, 30, -30, false, true},
    };
    for (const auto& fixture : fixtures) {
        const auto result = read(fixture.path);
        expect(result.report.formatEpoch == fixture.epoch, std::string("The fixture for ") + fixture.era + " is not the expected epoch");
        const auto mmRest = result.document->getOptions()->get<MmRest>();
        expect(static_cast<bool>(mmRest), std::string("No multimeasure rest options for ") + fixture.era);
        const auto wrong = [&](const char* name) { return std::string("The multimeasure rest ") + name + " was wrong for " + fixture.era; };
        expect(mmRest->measWidth == fixture.measWidth, wrong("measure width"));
        expect(mmRest->numAdjY == fixture.numAdjY, wrong("vertical number adjustment"));
        expect(mmRest->shapeDef == fixture.shapeDef, wrong("H-bar shape"));
        expect(mmRest->startAdjust == fixture.startAdjust, wrong("start adjustment"));
        expect(mmRest->endAdjust == fixture.endAdjust, wrong("end adjustment"));
        expect(mmRest->useSymbols == fixture.useSymbols, wrong("symbol style flag"));
        expect(mmRest->autoUpdateMmRests == fixture.autoUpdate, wrong("automatic update"));
        // "Stretch Horizontally" arrived with Finale 27, so no legacy era can state it and
        // every fixture must report it as known-false rather than as a synthesized default.
        expect(!mmRest->noHorizontalStretch, wrong("horizontal stretch flag"));
        expect(field(result, "options.multimeasureRestOptions.noHorizontalStretch").origin == ValueOrigin::LegacyBehavior,
            wrong("horizontal stretch provenance"));
        // Three values these fixtures leave at the pinned baseline. The early era does not store
        // them at all, so this also checks that its shorter record leaves them alone rather than
        // reading past it.
        expect(mmRest->numStart == 2 && mmRest->useSymsThreshold == 9 && mmRest->symSpacing == 48 && mmRest->numAdjX == 0, wrong("unvaried group"));
    }

    // Provenance separates the two fixed-row layouts where the values cannot. Finale 3.7.2
    // reads its H-bar adjustments from the record and finds zeros; Finale 2.6.3 has no record
    // to read them from, so they are asserted as era behavior rather than claimed as read or
    // left at the baseline's 30 and -30.
    const auto f263 = read("evidence/F263/F263-baseline.mus");
    for (const char* target : {"options.multimeasureRestOptions.startAdjust", "options.multimeasureRestOptions.endAdjust",
             "options.multimeasureRestOptions.autoUpdateMmRests"}) {
        expect(
            field(f263, target).origin == ValueOrigin::LegacyBehavior, std::string("A Coda-era field the source cannot state was not reported as era "
                                                                                   "behavior: ")
                                                                           + target);
    }
    expect(field(f263, "options.multimeasureRestOptions.numAdjY").origin == ValueOrigin::LegacyMus
               && field(f263, "options.multimeasureRestOptions.numAdjY").rawValue == -28,
        "The Coda-era number adjustment was not reported as read from slot 4");
    expect(field(f263, "options.multimeasureRestOptions.symSpacing").origin == ValueOrigin::Finale27Default,
        "A Coda-era field the source does not store was claimed as read");

    const auto f372 = read("evidence/F372/F372-baseline.mus");
    expect(field(f372, "options.multimeasureRestOptions.startAdjust").origin == ValueOrigin::LegacyMus,
        "The Finale 3.7.2 start adjustment was not read from its second incidence");
    // Selector 83 arrives with Finale 97, so a 3.7.2 document states nothing about automatic
    // updating and must not inherit the baseline's switched-on value.
    expect(field(f372, "options.multimeasureRestOptions.autoUpdateMmRests").origin == ValueOrigin::LegacyBehavior,
        "A Finale 3.7.2 document claimed an automatic-update setting its era has no record for");
    const auto f97 = read("evidence/F97/Fin97-baseline.mus");
    expect(field(f97, "options.multimeasureRestOptions.autoUpdateMmRests").origin == ValueOrigin::LegacyMus,
        "The Finale 97 automatic-update word was not read from selector 83");
    // Word 2 of that record is commonly set in later documents and is not this flag.
    expect(!f97.document->getOptions()->get<MmRest>()->autoUpdateMmRests, "The Finale 97 automatic-update flag was read from the wrong word");
}

TEST_CASE("Multimeasure rest recovery", "[class][reader]")
{
    testMultimeasureRestRecovery();
}

TEST_CASE("Multimeasure rest layout marker", "[class]")
{
    testMmRestEarlyLayoutMarker();
}
} // namespace
} // namespace finale_mus_reader_tests
