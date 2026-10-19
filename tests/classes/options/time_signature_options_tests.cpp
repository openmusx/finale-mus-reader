// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using TimeSignatureOptionsTarget = musx::dom::options::TimeSignatureOptions;

musx::dom::DocumentPtr makeTimeSignatureOptionsDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<TimeSignatureOptionsTarget>(document);
    options->timeUpperLift = 101;
    options->timeFront = 102;
    options->timeBack = 103;
    options->timeFrontParts = 104;
    options->timeBackParts = 105;
    options->timeUpperLiftParts = 106;
    options->timeLowerLiftParts = 107;
    options->timeAbrvLiftParts = 108;
    options->timeSigDoAbrvCommon = false;
    options->timeSigDoAbrvCut = false;
    options->numCompositeDecimalPlaces = 109;
    options->cautionaryTimeChanges = false;
    options->timeLowerLift = 110;
    options->timeAbrvLift = 111;
    document->getOptions()->add(TimeSignatureOptionsTarget::XmlNodeName, options);
    return std::move(session).finish();
}

std::shared_ptr<const TimeSignatureOptionsTarget> importTimeSignatureOptions(
    const finale_mus_reader::container::ParsedContainer& parsed, FormatEpoch epoch, ImportReport& report)
{
    const auto document = makeTimeSignatureOptionsDocument();
    const auto reference = makeTimeSignatureOptionsDocument();
    auto profile = profileFor(epoch == FormatEpoch::ZlibLegacy ? 12 : 5, 0);
    profile.epoch = epoch;
    profile.byteOrder = parsed.byteOrder;
    if (epoch == FormatEpoch::CodaBanner) {
        profile.version.reset();
    }
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{
        LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::options::importTimeSignatureOptions(context);
    return document->getOptions()->get<TimeSignatureOptionsTarget>();
}

constexpr const char* timeSignatureMembers[] = {"timeUpperLift", "timeFront", "timeBack", "timeFrontParts", "timeBackParts", "timeUpperLiftParts",
    "timeLowerLiftParts", "timeAbrvLiftParts", "timeSigDoAbrvCommon", "timeSigDoAbrvCut", "numCompositeDecimalPlaces", "cautionaryTimeChanges",
    "timeLowerLift", "timeAbrvLift"};

void expectCompleteManifest(const ImportReport& report)
{
    const auto found = report.fields.find(finale_mus_reader::instanceKey<TimeSignatureOptionsTarget>());
    expectMapping(found != report.fields.end() && found->second.size() == 14, "TimeSignatureOptions did not report exactly its fourteen "
                                                                              "musxdom fields");
    for (const auto* member : timeSignatureMembers) {
        expectMapping(
            fieldPresent(report, std::string("options.timeSignatureOptions.") + member), std::string("TimeSignatureOptions omitted field ") + member);
    }
}

void expectRecovered(const TimeSignatureOptionsTarget& options, const ImportReport& report)
{
    expectMapping(options.timeUpperLift == 15 && options.timeFront == 13 && options.timeBack == 14 && options.timeFrontParts == 23
                      && options.timeBackParts == 24 && options.timeUpperLiftParts == 20 && options.timeLowerLiftParts == 21
                      && options.timeAbrvLiftParts == 22 && options.timeSigDoAbrvCommon && options.timeSigDoAbrvCut
                      && options.numCompositeDecimalPlaces == 7 && options.cautionaryTimeChanges && options.timeLowerLift == -31
                      && options.timeAbrvLift == 32,
        "TimeSignatureOptions did not recover every stored field");
    expectCompleteManifest(report);
    for (const auto* member : timeSignatureMembers) {
        expectMapping(field(report, std::string("options.timeSignatureOptions.") + member).origin == ValueOrigin::LegacyMus,
            std::string("TimeSignatureOptions reported an incorrect origin for ") + member);
    }
}

const std::vector<SyntheticRow> fixedTimeSignatureRows{
    {GLOBALS_CMPER, "18", {0, 0, 0, 13, 14, 15}},
    {GLOBALS_CMPER, "18", {20, 21, 22, 23, 24, 0}},
    {GLOBALS_CMPER, "19", {0, 0, 1, 1, 0, 0}},
    {GLOBALS_CMPER, "23", {0, 7, 0, 0, 0, 0}},
    {GLOBALS_CMPER, "44", {0, 0, 0, 2, 0, 0}},
    {GLOBALS_CMPER, "67", {-31, 32, 0, 0, 0, 0}},
};

const std::vector<SyntheticClassRow> classTimeSignatureRows{
    {finale_mus_reader::numericGlobalClass(18), {0, 0, 0, 13, 14, 15, 20, 21, 22, 23, 24}},
    {finale_mus_reader::numericGlobalClass(19), {0, 0, 1, 1}},
    {finale_mus_reader::numericGlobalClass(23), {0, 7}},
    {finale_mus_reader::numericGlobalClass(44), {0, 0, 0, 2}},
    {finale_mus_reader::numericGlobalClass(67), {-31, 32}},
};

TEST_CASE("Time-signature options recover fixed rows in both epochs", "[class]")
{
    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        ImportReport report(epoch);
        const auto options = importTimeSignatureOptions(makeContainer(fixedTimeSignatureRows, epoch), epoch, report);
        expectRecovered(*options, report);
    }
}

TEST_CASE("Time-signature options recover zlib classes in either byte order", "[class]")
{
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        ImportReport report(FormatEpoch::ZlibLegacy);
        const auto options = importTimeSignatureOptions(makeClassContainer(classTimeSignatureRows, byteOrder), FormatEpoch::ZlibLegacy, report);
        expectRecovered(*options, report);
    }
}

TEST_CASE("A short zlib time-signature class does not imitate the shared fixed layout", "[class]")
{
    ImportReport report(FormatEpoch::ZlibLegacy);
    const auto options =
        importTimeSignatureOptions(makeClassContainer({{finale_mus_reader::numericGlobalClass(18), {0, 0, 0, 13, 14, 15}}}, ByteOrder::BigEndian),
            FormatEpoch::ZlibLegacy, report);

    expectMapping(options->timeFront == 13 && options->timeBack == 14 && options->timeUpperLift == 15 && options->timeFrontParts == 104
                      && options->timeBackParts == 105 && options->timeUpperLiftParts == 106,
        "A short zlib class was mistaken for the shared fixed-row layout");
    expectMapping(field(report, "options.timeSignatureOptions.timeFrontParts").origin == ValueOrigin::Finale27Default,
        "A short zlib class reported a synthesized parts field as shared behavior");
}

TEST_CASE("Coda time-signature options retain defaults for later fields", "[class]")
{
    ImportReport report(FormatEpoch::CodaBanner);
    const auto options = importTimeSignatureOptions(
        makeContainer(
            {{GLOBALS_CMPER, "18", {0, 0, 0, 13, 14, 15}}, {GLOBALS_CMPER, "19", {0, 0, 1, 1, 0, 0}}, {GLOBALS_CMPER, "23", {0, 7, 0, 0, 0, 0}}},
            FormatEpoch::CodaBanner),
        FormatEpoch::CodaBanner, report);

    expectMapping(options->timeUpperLift == 15 && options->timeFront == 13 && options->timeBack == 14 && options->timeSigDoAbrvCommon
                      && options->timeSigDoAbrvCut && options->numCompositeDecimalPlaces == 7 && options->timeFrontParts == 13
                      && options->timeBackParts == 14 && options->timeUpperLiftParts == 15 && options->timeLowerLiftParts == 110
                      && options->timeAbrvLiftParts == 111 && options->timeLowerLift == 110 && !options->cautionaryTimeChanges,
        "The Coda time-signature layout did not share its score distances with parts");
    expectCompleteManifest(report);
    expectMapping(field(report, "options.timeSignatureOptions.timeUpperLift").origin == ValueOrigin::LegacyMus
                      && field(report, "options.timeSignatureOptions.timeFrontParts").origin == ValueOrigin::LegacyBehavior
                      && field(report, "options.timeSignatureOptions.timeAbrvLiftParts").origin == ValueOrigin::LegacyBehavior,
        "Coda time-signature origins were incorrect");
}

TEST_CASE("Missing time-signature records retain seeded defaults", "[class]")
{
    ImportReport report(FormatEpoch::UncompressedLegacy);
    const auto options = importTimeSignatureOptions(makeContainer({}, FormatEpoch::UncompressedLegacy), FormatEpoch::UncompressedLegacy, report);
    expectMapping(options->timeUpperLift == 101 && options->timeFrontParts == 104 && options->timeLowerLift == 110 && !options->timeSigDoAbrvCut
                      && !options->cautionaryTimeChanges,
        "Missing time-signature records did not retain seeded values");
    expectCompleteManifest(report);
}

TEST_CASE("Controlled Finale 1 score spacing and lift are recovered", "[class]")
{
    const auto baseline = readFixture("evidence/F100/F100-baseline.mus");
    const auto changed = readFixture("evidence/F100/F100-cleftime-separs.mus");
    const auto baselineOptions = baseline.document->getOptions()->get<TimeSignatureOptionsTarget>();
    const auto changedOptions = changed.document->getOptions()->get<TimeSignatureOptionsTarget>();

    expectMapping(baselineOptions->timeFront == 24 && baselineOptions->timeBack == 12 && baselineOptions->timeUpperLift == 0
                      && changedOptions->timeFront == 23 && changedOptions->timeBack == 11 && changedOptions->timeUpperLift == 1
                      && changedOptions->timeFrontParts == 23 && changedOptions->timeBackParts == 11 && changedOptions->timeUpperLiftParts == 1,
        "The controlled Finale 1 time-signature edit was not recovered");
    for (const auto* member : {"timeFront", "timeBack", "timeUpperLift"}) {
        expectMapping(field(changed, std::string("options.timeSignatureOptions.") + member).origin == ValueOrigin::LegacyMus,
            std::string("The controlled Finale 1 edit reported an incorrect origin for ") + member);
    }
    for (const auto* member : {"timeFrontParts", "timeBackParts", "timeUpperLiftParts", "timeLowerLiftParts", "timeAbrvLiftParts"}) {
        expectMapping(field(changed, std::string("options.timeSignatureOptions.") + member).origin == ValueOrigin::LegacyBehavior,
            std::string("The controlled Finale 1 shared field reported an incorrect origin for ") + member);
    }
}

TEST_CASE("Finale 2008 carries independent linked-parts distances", "[class]")
{
    const auto result = readFixture("evidence/F2008/F2008-empty.mus");
    const auto options = result.document->getOptions()->get<TimeSignatureOptionsTarget>();
    expectMapping(options->timeFront == 24 && options->timeBack == 12 && options->timeFrontParts == 24 && options->timeBackParts == 12
                      && options->timeUpperLiftParts == 0 && options->timeLowerLiftParts == 0 && options->timeAbrvLiftParts == 0,
        "The Finale 2008 score and linked-parts distances were not recovered "
        "independently");
    for (const auto* member : {"timeFrontParts", "timeBackParts", "timeUpperLiftParts", "timeLowerLiftParts", "timeAbrvLiftParts"}) {
        expectMapping(field(result, std::string("options.timeSignatureOptions.") + member).origin == ValueOrigin::LegacyMus,
            std::string("The Finale 2008 parts field reported an incorrect origin for ") + member);
    }
}

} // namespace
} // namespace finale_mus_reader_tests
