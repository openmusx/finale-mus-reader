// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <tuple>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

using BarlineTarget = musx::dom::options::BarlineOptions;

musx::dom::DocumentPtr makeBarlineOptionsDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<BarlineTarget>(document);
    options->drawBarlines = false;
    options->drawCloseSystemBarline = false;
    options->drawCloseFinalBarline = false;
    options->drawFinalBarlineOnLastMeas = false;
    options->drawDoubleBarlineBeforeKeyChanges = true;
    options->drawLeftBarlineSingleStaff = false;
    options->drawLeftBarlineMultipleStaves = true;
    options->leftBarlineUsePrevStyle = false;
    options->thickBarlineWidth = 91;
    options->barlineWidth = 92;
    options->doubleBarlineSpace = 93;
    options->finalBarlineSpace = 94;
    options->barlineDashOn = 95;
    options->barlineDashOff = 96;
    document->getOptions()->add(BarlineTarget::XmlNodeName, options);
    return std::move(session).finish();
}

std::shared_ptr<const BarlineTarget> importBarlineOptions(const finale_mus_reader::container::ParsedContainer& parsed, FormatEpoch epoch,
    ImportReport& report, std::uint8_t sourceMajor = 0, std::uint8_t sourceMinor = 0, bool hasSourceVersion = true)
{
    const auto document = makeBarlineOptionsDocument();
    const auto reference = makeBarlineOptionsDocument();
    auto profile = profileFor(sourceMajor ? sourceMajor : epoch == FormatEpoch::ZlibLegacy ? 12 : 9, sourceMinor);
    profile.epoch = epoch;
    profile.byteOrder = parsed.byteOrder;
    if (epoch == FormatEpoch::CodaBanner || !hasSourceVersion) {
        profile.version.reset();
    }
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{
        LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::options::importBarlineOptions(context);
    return document->getOptions()->get<BarlineTarget>();
}

void verifyRecoveredBarlineOptions(const BarlineTarget& options, const ImportReport& report, bool closeBarlines, bool finalBarlineAtEnd,
    bool singleStaffLeftBarline, bool previousBarlineStyle)
{
    expectMapping(options.drawBarlines && options.drawCloseSystemBarline == closeBarlines && options.drawCloseFinalBarline == closeBarlines
                      && options.drawFinalBarlineOnLastMeas == finalBarlineAtEnd && options.leftBarlineUsePrevStyle == previousBarlineStyle
                      && options.drawLeftBarlineSingleStaff == singleStaffLeftBarline && !options.drawLeftBarlineMultipleStaves,
        "BarlineOptions did not recover its stored display switches");
    expectMapping(options.barlineWidth == 321 && options.thickBarlineWidth == 654 && options.doubleBarlineSpace == 765
                      && options.finalBarlineSpace == 876 && options.barlineDashOn == 0x12345 && options.barlineDashOff == 0x23456,
        "BarlineOptions did not recover its stored geometry");
    expectMapping(!options.drawDoubleBarlineBeforeKeyChanges, "BarlineOptions did not apply the fixed legacy key-change behavior");
    expectMapping(field(report, "options.barlineOptions.barlineDashOff").origin == ValueOrigin::LegacyMus
                      && field(report, "options.barlineOptions.drawFinalBarlineOnLastMeas").origin
                             == (finalBarlineAtEnd ? ValueOrigin::LegacyMus : ValueOrigin::LegacyBehavior)
                      && field(report, "options.barlineOptions.leftBarlineUsePrevStyle").origin
                             == (previousBarlineStyle ? ValueOrigin::LegacyMus : ValueOrigin::Finale27Default)
                      && field(report, "options.barlineOptions.drawDoubleBarlineBeforeKeyChanges").origin == ValueOrigin::LegacyBehavior,
        "BarlineOptions reported an incorrect field origin");
}

TEST_CASE("The expanded barline family splits the two left-barline switches", "[class]")
{
    const std::vector<SyntheticRow> unifiedRows{
        {GLOBALS_CMPER, "36", {0, 0, 0, 1, 0, 0}},
    };
    ImportReport unifiedReport(FormatEpoch::UncompressedLegacy);
    const auto unified = importBarlineOptions(
        makeContainer(unifiedRows, FormatEpoch::UncompressedLegacy), FormatEpoch::UncompressedLegacy, unifiedReport, 3, 5, false);
    expectMapping(unified->drawLeftBarlineSingleStaff && unified->drawLeftBarlineMultipleStaves
                      && field(unifiedReport, "options.barlineOptions.drawLeftBarlineSingleStaff").origin == ValueOrigin::LegacyMus,
        "The unified layout did not fan its stored switch out to both fields");

    const std::vector<SyntheticRow> splitRows{
        {GLOBALS_CMPER, "36", {0, 0, 1, 0, 0, 0}},
        {GLOBALS_CMPER, "67", {0, 0, 0, 0, 0, 0}},
    };
    ImportReport splitReport(FormatEpoch::UncompressedLegacy);
    const auto split =
        importBarlineOptions(makeContainer(splitRows, FormatEpoch::UncompressedLegacy), FormatEpoch::UncompressedLegacy, splitReport, 3, 2, false);
    expectMapping(split->drawLeftBarlineSingleStaff && !split->drawLeftBarlineMultipleStaves
                      && field(splitReport, "options.barlineOptions.drawLeftBarlineSingleStaff").origin == ValueOrigin::LegacyMus,
        "The expanded barline family did not select the split layout");
}

TEST_CASE("The Finale 1 barline switches map independently", "[class]")
{
    const auto baseline = readFixture("evidence/F100/F100-baseline.mus");
    const auto noLeftBarline = readFixture("evidence/F100/F100-dont-leftbarline.mus");
    const auto noBarlines = readFixture("evidence/F100/F100-dont-barline.mus");
    const auto baselineOptions = baseline.document->getOptions()->get<BarlineTarget>();
    const auto noLeftOptions = noLeftBarline.document->getOptions()->get<BarlineTarget>();
    const auto noBarlinesOptions = noBarlines.document->getOptions()->get<BarlineTarget>();

    expectMapping(baselineOptions->drawLeftBarlineSingleStaff && baselineOptions->drawLeftBarlineMultipleStaves && baselineOptions->drawBarlines
                      && !noLeftOptions->drawLeftBarlineSingleStaff && !noLeftOptions->drawLeftBarlineMultipleStaves && noLeftOptions->drawBarlines
                      && noBarlinesOptions->drawLeftBarlineSingleStaff && noBarlinesOptions->drawLeftBarlineMultipleStaves
                      && !noBarlinesOptions->drawBarlines && baselineOptions->barlineWidth == 224 && noLeftOptions->barlineWidth == 224
                      && noBarlinesOptions->barlineWidth == 224,
        "The Coda-era barline switches did not map independently");
    for (const auto* fixture : {&baseline, &noLeftBarline, &noBarlines}) {
        expectMapping(field(*fixture, "options.barlineOptions.drawLeftBarlineSingleStaff").origin == ValueOrigin::LegacyMus
                          && field(*fixture, "options.barlineOptions.drawLeftBarlineMultipleStaves").origin == ValueOrigin::LegacyMus
                          && field(*fixture, "options.barlineOptions.drawBarlines").origin == ValueOrigin::LegacyMus
                          && field(*fixture, "options.barlineOptions.barlineWidth").origin == ValueOrigin::LegacyBehavior,
            "The Coda-era barline fields did not report stored provenance");
    }
}

TEST_CASE("Previous-style left barlines begin with Finale 2000", "[class]")
{
    const std::vector<SyntheticRow> rows{
        {GLOBALS_CMPER, "36", {0, 1, 0, 0, 0, 0}},
    };
    for (const auto& [major, expected, origin] :
        {std::tuple{std::uint8_t{4}, false, ValueOrigin::Finale27Default}, std::tuple{std::uint8_t{5}, true, ValueOrigin::LegacyMus}}) {
        ImportReport report(FormatEpoch::UncompressedLegacy);
        const auto options =
            importBarlineOptions(makeContainer(rows, FormatEpoch::UncompressedLegacy), FormatEpoch::UncompressedLegacy, report, major);
        expectMapping(
            options->leftBarlineUsePrevStyle == expected && field(report, "options.barlineOptions.leftBarlineUsePrevStyle").origin == origin,
            "The previous-style left-barline gate selected the wrong layout or origin");
    }

    ImportReport unknownVersionReport(FormatEpoch::UncompressedLegacy);
    const auto unknownVersion = importBarlineOptions(
        makeContainer(rows, FormatEpoch::UncompressedLegacy), FormatEpoch::UncompressedLegacy, unknownVersionReport, 5, 0, false);
    expectMapping(!unknownVersion->leftBarlineUsePrevStyle
                      && field(unknownVersionReport, "options.barlineOptions.leftBarlineUsePrevStyle").origin == ValueOrigin::Finale27Default,
        "An unknown uncompressed version did not fail the Finale 2000 gate closed");

    const auto finale97 = readFixture("evidence/F97/Fin97-baseline.mus");
    const auto finale2000 = readFixture("evidence/F2000/F2000-baseline.mus");
    expectMapping(!finale97.document->getOptions()->get<BarlineTarget>()->leftBarlineUsePrevStyle
                      && field(finale97, "options.barlineOptions.leftBarlineUsePrevStyle").origin == ValueOrigin::Finale27Default
                      && !finale2000.document->getOptions()->get<BarlineTarget>()->leftBarlineUsePrevStyle
                      && field(finale2000, "options.barlineOptions.leftBarlineUsePrevStyle").origin == ValueOrigin::LegacyMus,
        "The tracked Finale 97/2000 pair did not exercise the introduction boundary");
}

TEST_CASE("Barline options recover the located fixed-row fields", "[class]")
{
    const std::vector<SyntheticRow> rows{
        {GLOBALS_CMPER, "03", {0, 0, 0, 0, 1, 1}},
        {GLOBALS_CMPER, "09", {0, 0, 0, 0, 0, 1}},
        {GLOBALS_CMPER, "36", {0, 1, 1, 0, 1, 0}},
        {GLOBALS_CMPER, "58", {0, 0, 0, 0, 321, 0}},
        {GLOBALS_CMPER, "67", {0, 0, 654, 765, 876, 0}},
        {GLOBALS_CMPER, "68", {0, 0, 1, 0x2345, 2, 0x3456}},
    };
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        ImportReport report(epoch);
        const bool closeBarlines = epoch != FormatEpoch::CodaBanner;
        const bool finalBarlineAtEnd = epoch == FormatEpoch::DclLegacy;
        const bool singleStaffLeftBarline = epoch != FormatEpoch::CodaBanner;
        const bool previousBarlineStyle = epoch != FormatEpoch::CodaBanner;
        verifyRecoveredBarlineOptions(*importBarlineOptions(makeContainer(rows, epoch), epoch, report), report, closeBarlines, finalBarlineAtEnd,
            singleStaffLeftBarline, previousBarlineStyle);
    }
}

TEST_CASE("Automatic final barlines begin in the DCL epoch", "[class][reader]")
{
    const auto baseline = readFixture("evidence/F2001/F2001Win-empty.mus");
    const auto disabled = readFixture("evidence/F2001/F2001Win-finalbarline-toggle.mus");
    const auto baselineOptions = baseline.document->getOptions()->get<BarlineTarget>();
    const auto disabledOptions = disabled.document->getOptions()->get<BarlineTarget>();

    expectMapping(baseline.report.formatEpoch == FormatEpoch::DclLegacy && baseline.report.byteOrder == ByteOrder::LittleEndian
                      && baselineOptions->drawFinalBarlineOnLastMeas && !disabledOptions->drawFinalBarlineOnLastMeas
                      && field(disabled, "options.barlineOptions.drawFinalBarlineOnLastMeas").origin == ValueOrigin::LegacyMus,
        "The Windows Finale 2001 final-barline switch was not recovered");
}

TEST_CASE("Barline options recover class records in either byte order", "[class]")
{
    const std::vector<SyntheticClassRow> rows{
        {finale_mus_reader::numericGlobalClass(3), {0, 0, 0, 0, 1, 1}},
        {finale_mus_reader::numericGlobalClass(9), {0, 0, 0, 0, 0, 1}},
        {finale_mus_reader::numericGlobalClass(36), {0, 1, 1, 0, 1, 0}},
        {finale_mus_reader::numericGlobalClass(58), {0, 0, 0, 0, 321, 0}},
        {finale_mus_reader::numericGlobalClass(67), {0, 0, 654, 765, 876, 0}},
        {finale_mus_reader::numericGlobalClass(68), {0, 0, 1, 0x2345, 2, 0x3456}},
    };
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        ImportReport report(FormatEpoch::ZlibLegacy);
        verifyRecoveredBarlineOptions(
            *importBarlineOptions(makeClassContainer(rows, byteOrder), FormatEpoch::ZlibLegacy, report), report, true, true, true, true);
    }
}

TEST_CASE("Absent barline records retain mapped defaults and legacy behavior", "[class]")
{
    ImportReport report(FormatEpoch::CodaBanner);
    const auto options = importBarlineOptions(makeContainer({}, FormatEpoch::CodaBanner), FormatEpoch::CodaBanner, report);
    expectMapping(
        options->barlineWidth == 224 && options->drawLeftBarlineMultipleStaves, "Absent BarlineOptions records disturbed the seeded mapped fields");
    expectMapping(!options->drawDoubleBarlineBeforeKeyChanges, "Absent BarlineOptions records retained a post-legacy baseline behavior");
    expectMapping(field(report, "options.barlineOptions.drawDoubleBarlineBeforeKeyChanges").origin == ValueOrigin::LegacyBehavior
                      && field(report, "options.barlineOptions.barlineWidth").origin == ValueOrigin::LegacyBehavior,
        "Absent BarlineOptions records reported incorrect origins");
}

} // namespace
} // namespace finale_mus_reader_tests
