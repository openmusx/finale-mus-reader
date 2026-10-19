// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

using BeamOptionsTarget = musx::dom::options::BeamOptions;

musx::dom::DocumentPtr makeBeamOptionsDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<BeamOptionsTarget>(document);
    options->beamStubLength = 91;
    options->maxSlope = 92;
    options->beamSepar = 93;
    options->maxFromMiddle = 94;
    options->beamingStyle = BeamOptionsTarget::FlattenStyle::OnStandardNote;
    options->oldFinaleRestBeams = true;
    options->beamFourEighthsInCommonTime = true;
    options->beamWidth = 95;
    document->getOptions()->add(BeamOptionsTarget::XmlNodeName, options);
    return std::move(session).finish();
}

std::shared_ptr<const BeamOptionsTarget> importBeamOptions(const finale_mus_reader::container::ParsedContainer& parsed, FormatEpoch epoch,
    ImportReport& report, finale_mus_reader::VersionBound version = finale_mus_reader::versions::finale2004)
{
    const auto document = makeBeamOptionsDocument();
    const auto reference = makeBeamOptionsDocument();
    auto profile = profileFor(epoch == FormatEpoch::ZlibLegacy ? 12 : version.major, epoch == FormatEpoch::ZlibLegacy ? 0 : version.minor);
    profile.epoch = epoch;
    profile.byteOrder = parsed.byteOrder;
    if (epoch == FormatEpoch::CodaBanner) {
        profile.version.reset();
    }
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{
        LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::options::importBeamOptions(context);
    return document->getOptions()->get<BeamOptionsTarget>();
}

void verifyRecoveredBeamOptions(const BeamOptionsTarget& options, const ImportReport& report)
{
    expectMapping(options.beamStubLength == 17 && options.maxSlope == 23 && options.beamSepar == 29 && options.maxFromMiddle == 31
                      && options.beamingStyle == BeamOptionsTarget::FlattenStyle::AlwaysFlat && options.extendBeamsOverRests
                      && options.incRestsInFourGroups && options.beamFourEighthsInCommonTime && options.beamThreeEighthsInCommonTime
                      && options.dispHalfStemsOnRests && options.oldFinaleRestBeams && options.spanSpace && options.extendSecBeamsOverRests
                      && options.beamWidth == 0x12345,
        "BeamOptions did not recover its stored geometry and behavior");
    expectMapping(field(report, "options.beamOptions.beamingStyle").origin == ValueOrigin::LegacyMus
                      && field(report, "options.beamOptions.beamWidth").origin == ValueOrigin::LegacyMus
                      && field(report, "options.beamOptions.oldFinaleRestBeams").origin == ValueOrigin::LegacyMus,
        "BeamOptions reported an incorrect field origin");
}

TEST_CASE("Beam options recover the packed fixed-row layout", "[class]")
{
    const std::vector<SyntheticRow> rows{
        {GLOBALS_CMPER, "03", {0, 0, 0, 17, 0, 0}},
        {GLOBALS_CMPER, "20", {23, 29, 31, 0, 0, 0}},
        {GLOBALS_CMPER, "41", {1, 0x01fb, 0, 0, 0, 0}},
        {GLOBALS_CMPER, "62", {0, 0, 0, 0, 1, 0x2345}},
    };
    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        ImportReport report(epoch);
        verifyRecoveredBeamOptions(*importBeamOptions(makeContainer(rows, epoch), epoch, report), report);
    }
}

TEST_CASE("Beam options recover class records in either byte order", "[class]")
{
    const std::vector<SyntheticClassRow> rows{
        {finale_mus_reader::numericGlobalClass(3), {0, 0, 0, 17}},
        {finale_mus_reader::numericGlobalClass(20), {23, 29, 31}},
        {finale_mus_reader::numericGlobalClass(41), {1, 0x01fb}},
        {finale_mus_reader::numericGlobalClass(62), {0, 0, 0, 0, 1, 0x2345}},
    };
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        ImportReport report(FormatEpoch::ZlibLegacy);
        verifyRecoveredBeamOptions(*importBeamOptions(makeClassContainer(rows, byteOrder), FormatEpoch::ZlibLegacy, report), report);
    }
}

TEST_CASE("Early beam layouts recover stored fields and assert era behavior", "[class]")
{
    const std::vector<SyntheticRow> laterUnitRows{
        {GLOBALS_CMPER, "03", {0, 0, 0, 17, 0, 0}},
        {GLOBALS_CMPER, "09", {0, 0, 0, 0, 0, 0}},
        {GLOBALS_CMPER, "16", {0, 0, 0, 0, 1, 0}},
        {GLOBALS_CMPER, "20", {23, 29, 31, 0, 0, 0}},
        {GLOBALS_CMPER, "22", {1, 0, 0, 0, 0, 0}},
        {GLOBALS_CMPER, "41", {3, 0, 0, 0, 0, 0}},
        {GLOBALS_CMPER, "62", {0, 0, 0, 0, 1, 0x2345}},
    };
    ImportReport laterReport(FormatEpoch::UncompressedLegacy);
    const auto later = importBeamOptions(makeContainer(laterUnitRows, FormatEpoch::UncompressedLegacy), FormatEpoch::UncompressedLegacy, laterReport,
        finale_mus_reader::versions::finale3_7);
    expectMapping(later->beamStubLength == 17 && later->maxSlope == 23 && later->beamSepar == 29 && later->maxFromMiddle == 31
                      && later->beamingStyle == BeamOptionsTarget::FlattenStyle::OnExtremeNote && later->extendBeamsOverRests
                      && later->extendSecBeamsOverRests && later->dispHalfStemsOnRests && later->beamWidth == 0x12345 && later->oldFinaleRestBeams
                      && later->spanSpace && !later->beamFourEighthsInCommonTime,
        "The early BeamOptions layout did not recover stored fields and behavior");
    expectMapping(field(laterReport, "options.beamOptions.maxSlope").origin == ValueOrigin::LegacyMus
                      && field(laterReport, "options.beamOptions.oldFinaleRestBeams").origin == ValueOrigin::LegacyBehavior
                      && field(laterReport, "options.beamOptions.extendBeamsOverRests").origin == ValueOrigin::LegacyMus
                      && field(laterReport, "options.beamOptions.extendSecBeamsOverRests").origin == ValueOrigin::LegacyMus
                      && field(laterReport, "options.beamOptions.dispHalfStemsOnRests").origin == ValueOrigin::LegacyMus
                      && field(laterReport, "options.beamOptions.beamFourEighthsInCommonTime").origin == ValueOrigin::LegacyMus,
        "The early BeamOptions layout reported incorrect origins");

    auto earlyUnitRows = laterUnitRows;
    earlyUnitRows.push_back({GLOBALS_CMPER, "40", {1, 2, 3, 4, 5, 6}});
    earlyUnitRows[6] = {GLOBALS_CMPER, "62", {0, 0, 0, 0, 0, 30000}};
    ImportReport earlyReport(FormatEpoch::CodaBanner);
    const auto early = importBeamOptions(makeContainer(earlyUnitRows, FormatEpoch::CodaBanner), FormatEpoch::CodaBanner, earlyReport);
    expectMapping(early->maxSlope == 23 * musx::dom::EVPU_PER_STAFF_POSITION && early->maxFromMiddle == 31 * musx::dom::EVPU_PER_STAFF_POSITION
                      && early->beamWidth == 768 && early->oldFinaleRestBeams && early->spanSpace,
        "The early BeamOptions measurements were not converted to musxdom units");
    expectMapping(field(earlyReport, "options.beamOptions.maxSlope").origin == ValueOrigin::LegacyMusAdjusted,
        "The pre-Finale-3.5 BeamOptions conversion reported an incorrect origin");
}

TEST_CASE("Finale 3.7 beam-rest switch populates both modern options", "[class]")
{
    const auto baseline = readFixture("evidence/F372/F372-baseline.mus");
    const auto changed = readFixture("evidence/F372/F372-beams-inclrests.mus");
    const auto baselineOptions = baseline.document->getOptions()->get<BeamOptionsTarget>();
    const auto changedOptions = changed.document->getOptions()->get<BeamOptionsTarget>();

    expect(!baselineOptions->extendBeamsOverRests && !baselineOptions->extendSecBeamsOverRests,
        "The clear Finale 3.7 beam-rest switch was not recovered");
    expect(changedOptions->extendBeamsOverRests && changedOptions->extendSecBeamsOverRests,
        "The set Finale 3.7 beam-rest switch did not populate both modern options");
    expect(field(changed, "options.beamOptions.extendBeamsOverRests").origin == ValueOrigin::LegacyMus
               && field(changed, "options.beamOptions.extendSecBeamsOverRests").origin == ValueOrigin::LegacyMus
               && !baselineOptions->beamFourEighthsInCommonTime
               && field(baseline, "options.beamOptions.beamFourEighthsInCommonTime").origin == ValueOrigin::LegacyMus,
        "The Finale 3.7 beam-rest switch reported an incorrect origin");
}

TEST_CASE("Finale 97 beam-rest switch populates both modern options", "[class]")
{
    const auto baseline = readFixture("evidence/F97/Fin97-baseline.mus");
    const auto changed = readFixture("evidence/F97/F97-beams-inclrests.mus");
    const auto baselineOptions = baseline.document->getOptions()->get<BeamOptionsTarget>();
    const auto changedOptions = changed.document->getOptions()->get<BeamOptionsTarget>();

    expect(!baselineOptions->extendBeamsOverRests && !baselineOptions->extendSecBeamsOverRests,
        "The clear Finale 97 beam-rest switch was not recovered");
    expect(changedOptions->extendBeamsOverRests && changedOptions->extendSecBeamsOverRests && baselineOptions->incRestsInFourGroups
               && changedOptions->incRestsInFourGroups,
        "The set Finale 97 beam-rest switch did not populate both modern options");
    expect(field(changed, "options.beamOptions.extendBeamsOverRests").origin == ValueOrigin::LegacyMus
               && field(changed, "options.beamOptions.extendSecBeamsOverRests").origin == ValueOrigin::LegacyMus,
        "The Finale 97 beam-rest switch reported an incorrect origin");
}

TEST_CASE("Finale 98 ignores its nonpersistent four-groups switch", "[class]")
{
    const auto baseline = readFixture("evidence/F98/F98-baseline.mus");
    const auto includeRests = readFixture("evidence/F98/F98-beams-inclrestsin4.mus");
    const auto noFourEighths = readFixture("evidence/F98/F98-beams-no4-8thcommon.mus");
    const auto baselineOptions = baseline.document->getOptions()->get<BeamOptionsTarget>();
    const auto restOptions = includeRests.document->getOptions()->get<BeamOptionsTarget>();
    const auto fourEighthsOptions = noFourEighths.document->getOptions()->get<BeamOptionsTarget>();

    expect(baselineOptions->beamFourEighthsInCommonTime && !baselineOptions->incRestsInFourGroups && restOptions->beamFourEighthsInCommonTime
               && !restOptions->incRestsInFourGroups && !fourEighthsOptions->beamFourEighthsInCommonTime && !fourEighthsOptions->incRestsInFourGroups,
        "The Finale 98 beam switches did not reproduce reload behavior");
    expect(field(baseline, "options.beamOptions.beamFourEighthsInCommonTime").origin == ValueOrigin::LegacyMus
               && field(baseline, "options.beamOptions.incRestsInFourGroups").origin == ValueOrigin::LegacyBehavior
               && field(includeRests, "options.beamOptions.incRestsInFourGroups").origin == ValueOrigin::LegacyBehavior
               && field(noFourEighths, "options.beamOptions.beamFourEighthsInCommonTime").origin == ValueOrigin::LegacyMus,
        "The Finale 98 beam switches reported incorrect origins");
}

TEST_CASE("Finale 3.7 beamed-rest half-stem switch is source owned", "[class]")
{
    const auto baseline = readFixture("evidence/F372/F372-baseline.mus");
    const auto changed = readFixture("evidence/F372/F372-beams-resthalfstems.mus");
    const auto baselineOptions = baseline.document->getOptions()->get<BeamOptionsTarget>();
    const auto changedOptions = changed.document->getOptions()->get<BeamOptionsTarget>();

    expect(!baselineOptions->dispHalfStemsOnRests, "The clear Finale 3.7 half-stem switch was not recovered");
    expect(changedOptions->dispHalfStemsOnRests, "The set Finale 3.7 half-stem switch was not recovered");
    expect(field(changed, "options.beamOptions.dispHalfStemsOnRests").origin == ValueOrigin::LegacyMus,
        "The Finale 3.7 half-stem switch reported an incorrect origin");
}

TEST_CASE("Finale 2.6.3 stores its two exposed beam options", "[class]")
{
    const auto baseline = readFixture("evidence/F263/F263-baseline.mus");
    const auto includeRests = readFixture("evidence/F263/F263-beams-inclrests.mus");
    const auto flatBeams = readFixture("evidence/F263/F263-flatbams.mus");
    const auto baselineOptions = baseline.document->getOptions()->get<BeamOptionsTarget>();
    const auto restOptions = includeRests.document->getOptions()->get<BeamOptionsTarget>();
    const auto flatOptions = flatBeams.document->getOptions()->get<BeamOptionsTarget>();

    expect(!baselineOptions->extendBeamsOverRests && !baselineOptions->extendSecBeamsOverRests,
        "The clear Finale 2.6.3 beam-rest switch was not recovered");
    expect(restOptions->extendBeamsOverRests && restOptions->extendSecBeamsOverRests,
        "The set Finale 2.6.3 beam-rest switch did not populate both modern options");
    expect(flatOptions->beamingStyle == BeamOptionsTarget::FlattenStyle::AlwaysFlat, "The Finale 2.6.3 flat-beam switch was not recovered");
    expect(baselineOptions->beamFourEighthsInCommonTime && restOptions->beamFourEighthsInCommonTime && flatOptions->beamFourEighthsInCommonTime,
        "Finale 2.6.3 did not retain the accepted later four-eighths default");
    expect(field(includeRests, "options.beamOptions.extendBeamsOverRests").origin == ValueOrigin::LegacyMus
               && field(includeRests, "options.beamOptions.extendSecBeamsOverRests").origin == ValueOrigin::LegacyMus
               && field(flatBeams, "options.beamOptions.beamingStyle").origin == ValueOrigin::LegacyMus
               && field(flatBeams, "options.beamOptions.beamFourEighthsInCommonTime").origin == ValueOrigin::Finale27Default
               && field(flatBeams, "options.beamOptions.dispHalfStemsOnRests").origin == ValueOrigin::Finale27Default,
        "The Finale 2.6.3 beam options reported incorrect origins");
}

} // namespace
} // namespace finale_mus_reader_tests
