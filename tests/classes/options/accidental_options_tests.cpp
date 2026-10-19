// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

musx::dom::DocumentPtr makeAccidentalOptionsDocument()
{
    using Target = musx::dom::options::AccidentalOptions;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<Target>(document);
    options->minOverlap = 91;
    options->multiCharSpace = 92;
    options->crossLayerPositioning = false;
    options->startMeasureSepar = 93;
    options->acciNoteSpace = 94;
    options->acciAcciSpace = 95;
    document->getOptions()->add(Target::XmlNodeName, options);
    return std::move(session).finish();
}

void testAccidentalOptionsAcrossEpochs()
{
    using Target = musx::dom::options::AccidentalOptions;
    const auto runImport = [](const finale_mus_reader::container::ParsedContainer& parsed, FormatEpoch epoch, finale_mus_reader::VersionBound version,
                               ImportReport& report) {
        const auto document = makeAccidentalOptionsDocument();
        const auto reference = makeAccidentalOptionsDocument();
        auto profile = profileFor(version.major, version.minor);
        profile.epoch = epoch;
        profile.byteOrder = parsed.byteOrder;
        if (epoch == FormatEpoch::CodaBanner) {
            profile.version.reset();
        }
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{
            LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::options::importAccidentalOptions(context);
        return document->getOptions()->get<Target>();
    };
    const auto verify = [](const auto& options, const ImportReport& report, bool startMeasureStored, ValueOrigin crossLayerOrigin) {
        expectMapping(options->minOverlap == 7 && options->multiCharSpace == -11 && options->acciNoteSpace == 13 && options->acciAcciSpace == 17,
            "Accidental options did not recover the four located numeric fields");
        expectMapping(!options->crossLayerPositioning && options->startMeasureSepar == (startMeasureStored ? 27 : 93),
            "Accidental options decoded a located field incorrectly or disturbed an unavailable field");
        expectMapping(field(report, "options.accidentalOptions.minOverlap").origin == ValueOrigin::LegacyMus
                          && field(report, "options.accidentalOptions.crossLayerPositioning").origin == crossLayerOrigin
                          && field(report, "options.accidentalOptions.startMeasureSepar").origin
                                 == (startMeasureStored ? ValueOrigin::LegacyMus : ValueOrigin::Finale27Default),
            "Accidental options reported an incorrect field origin");
    };

    const std::vector<SyntheticRow> fixedRows{
        {GLOBALS_CMPER, "21", {0, 0, 0, 7, 0, -11}},
        {GLOBALS_CMPER, "22", {0, 0, 0, 0, 0, 0}},
        {GLOBALS_CMPER, "59", {0, 0, 0, 13, 17, 0}},
        {GLOBALS_CMPER, "41", {0, 2, 0, 0, 0, 0}},
        {GLOBALS_CMPER, "41", {0, 0, 0, 0, 0, 0}},
        {GLOBALS_CMPER, "41", {0, 0, 27, 0, 0, 0}},
    };
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        ImportReport report(epoch);
        const auto version = epoch == FormatEpoch::CodaBanner  ? finale_mus_reader::versions::finale2_6
                             : epoch == FormatEpoch::DclLegacy ? finale_mus_reader::versions::finale2005
                                                               : finale_mus_reader::versions::finale2000;
        verify(runImport(makeContainer(fixedRows, epoch), epoch, version, report), report, true,
            epoch == FormatEpoch::DclLegacy ? ValueOrigin::LegacyMus : ValueOrigin::LegacyBehavior);
    }

    {
        auto rowsWithoutStartMeasure = fixedRows;
        rowsWithoutStartMeasure.resize(4);
        ImportReport report(FormatEpoch::UncompressedLegacy);
        verify(runImport(makeContainer(rowsWithoutStartMeasure, FormatEpoch::UncompressedLegacy), FormatEpoch::UncompressedLegacy,
                   finale_mus_reader::versions::finale2000, report),
            report, false, ValueOrigin::LegacyBehavior);
    }

    const std::vector<SyntheticClassRow> classRows{
        {finale_mus_reader::numericGlobalClass(21), {0, 0, 0, 7, 0, -11}},
        {finale_mus_reader::numericGlobalClass(22), {0, 0, 0, 0, 0, 0}},
        {finale_mus_reader::numericGlobalClass(59), {0, 0, 0, 13, 17, 0}},
        {finale_mus_reader::numericGlobalClass(41), {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 27}},
    };
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        ImportReport report(FormatEpoch::ZlibLegacy);
        verify(runImport(makeClassContainer(classRows, byteOrder), FormatEpoch::ZlibLegacy, finale_mus_reader::versions::finale2007, report), report,
            true, ValueOrigin::LegacyMus);
    }

    auto enabledRows = fixedRows;
    enabledRows[1].words[0] = 1;
    ImportReport enabledReport(FormatEpoch::DclLegacy);
    expectMapping(
        runImport(makeContainer(enabledRows, FormatEpoch::DclLegacy), FormatEpoch::DclLegacy, finale_mus_reader::versions::finale2004, enabledReport)
            ->crossLayerPositioning,
        "Accidental options did not recover the enabled cross-layer flag");

    ImportReport preFinale2004Report(FormatEpoch::DclLegacy);
    const auto preFinale2004 = runImport(
        makeContainer(enabledRows, FormatEpoch::DclLegacy), FormatEpoch::DclLegacy, finale_mus_reader::versions::finale2003, preFinale2004Report);
    expectMapping(!preFinale2004->crossLayerPositioning
                      && field(preFinale2004Report, "options.accidentalOptions.crossLayerPositioning").origin == ValueOrigin::LegacyBehavior,
        "The pre-Finale-2004 selector word was mistaken for cross-layer positioning");

    auto earlyRows = fixedRows;
    earlyRows[3].words[1] = 0;
    earlyRows[2].words[3] = 0;
    earlyRows[2].words[4] = 0;
    ImportReport earlyReport(FormatEpoch::UncompressedLegacy);
    const auto early = runImport(makeContainer(earlyRows, FormatEpoch::UncompressedLegacy), FormatEpoch::UncompressedLegacy,
        finale_mus_reader::versions::finale3_2, earlyReport);
    expectMapping(
        early->acciNoteSpace == 8 && early->acciAcciSpace == 8, "Finale 3.2 zero accidental spacing did not apply its hard-coded legacy value");
    expectMapping(field(earlyReport, "options.accidentalOptions.acciNoteSpace").origin == ValueOrigin::LegacyBehavior
                      && field(earlyReport, "options.accidentalOptions.acciAcciSpace").origin == ValueOrigin::LegacyBehavior,
        "Finale 3.2 zero accidental spacing reported the wrong origin");

    ImportReport earlyNonzeroReport(FormatEpoch::UncompressedLegacy);
    const auto earlyNonzero = runImport(makeContainer(fixedRows, FormatEpoch::UncompressedLegacy), FormatEpoch::UncompressedLegacy,
        finale_mus_reader::versions::finale3_0, earlyNonzeroReport);
    expectMapping(earlyNonzero->acciNoteSpace == 13 && earlyNonzero->acciAcciSpace == 17
                      && field(earlyNonzeroReport, "options.accidentalOptions.acciNoteSpace").origin == ValueOrigin::LegacyMus,
        "Finale 3.0 nonzero accidental spacing was mistaken for legacy behavior");

    ImportReport finale35Report(FormatEpoch::UncompressedLegacy);
    const auto finale35 = runImport(makeContainer(earlyRows, FormatEpoch::UncompressedLegacy), FormatEpoch::UncompressedLegacy,
        finale_mus_reader::versions::finale3_5, finale35Report);
    expectMapping(finale35->acciNoteSpace == 0 && finale35->acciAcciSpace == 0
                      && field(finale35Report, "options.accidentalOptions.acciNoteSpace").origin == ValueOrigin::LegacyMus,
        "Finale 3.5 accidental spacing crossed the legacy-behavior boundary incorrectly");
}

TEST_CASE("Accidental options span the located epochs", "[class]")
{
    testAccidentalOptionsAcrossEpochs();
}

TEST_CASE("Finale 2004 stores accidental cross-layer positioning", "[class]")
{
    using Target = musx::dom::options::AccidentalOptions;
    const auto baseline = readFixture("evidence/F2004/F2004-baseline.mus");
    const auto disabled = readFixture("evidence/F2004/F2004-no-xlayer-accis.mus");

    expect(baseline.document->getOptions()->get<Target>()->crossLayerPositioning, "The enabled Finale 2004 cross-layer option was not recovered");
    expect(!disabled.document->getOptions()->get<Target>()->crossLayerPositioning, "The disabled Finale 2004 cross-layer option was not recovered");
    expect(field(baseline, "options.accidentalOptions.crossLayerPositioning").origin == ValueOrigin::LegacyMus
               && field(disabled, "options.accidentalOptions.crossLayerPositioning").origin == ValueOrigin::LegacyMus,
        "The Finale 2004 cross-layer option reported an incorrect origin");
}

} // namespace
} // namespace finale_mus_reader_tests
