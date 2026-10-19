// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <cmath>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

using LineCurveTarget = musx::dom::options::LineCurveOptions;

musx::dom::DocumentPtr makeLineCurveOptionsDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<LineCurveTarget>(document);
    options->bezierStep = 101;
    options->enclosureWidth = 102;
    options->enclosureRoundCorners = true;
    options->enclosureCornerRadius = 103;
    options->staffLineWidth = 104;
    options->legerLineWidth = 105;
    options->legerFrontLength = 106;
    options->legerBackLength = 107;
    options->restLegerFrontLength = 108;
    options->restLegerBackLength = 109;
    options->psUlDepth = 1.1;
    options->psUlWidth = 1.2;
    options->pathSlurTipWidth = 1.3;
    document->getOptions()->add(LineCurveTarget::XmlNodeName, options);
    return std::move(session).finish();
}

std::shared_ptr<const LineCurveTarget> importLineCurveOptions(
    const finale_mus_reader::container::ParsedContainer& parsed, FormatEpoch epoch, ImportReport& report)
{
    const auto document = makeLineCurveOptionsDocument();
    const auto reference = makeLineCurveOptionsDocument();
    auto profile = profileFor(epoch == FormatEpoch::CodaBanner ? 2 : 7);
    profile.epoch = epoch;
    profile.byteOrder = parsed.byteOrder;
    if (epoch == FormatEpoch::CodaBanner) {
        profile.version.reset();
    }
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{
        LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::options::importLineCurveOptions(context);
    return document->getOptions()->get<LineCurveTarget>();
}

void verifyLaterLineCurveFields(const std::shared_ptr<const LineCurveTarget>& options, const ImportReport& report)
{
    expect(options->bezierStep == 23 && options->enclosureWidth == 24 && options->staffLineWidth == 25 && options->legerLineWidth == 26
               && options->legerFrontLength == 27 && options->legerBackLength == 28 && options->restLegerFrontLength == 29
               && options->restLegerBackLength == 30 && std::abs(options->psUlDepth + 1.25) < 0.00001
               && std::abs(options->psUlWidth - 0.625) < 0.00001 && std::abs(options->pathSlurTipWidth - 0.875) < 0.00001,
        "Line-and-curve options did not recover the later legacy fields");
    expect(!options->enclosureRoundCorners && options->enclosureCornerRadius == 0, "Legacy enclosure-corner behavior was not applied");
    expect(field(report, "options.lineCurveOptions.bezierStep").origin == ValueOrigin::LegacyMus
               && field(report, "options.lineCurveOptions.pathSlurTipWidth").origin == ValueOrigin::LegacyMus
               && field(report, "options.lineCurveOptions.enclosureRoundCorners").origin == ValueOrigin::LegacyBehavior
               && field(report, "options.lineCurveOptions.enclosureCornerRadius").origin == ValueOrigin::LegacyBehavior,
        "Line-and-curve options reported incorrect later field origins");
}

TEST_CASE("Line and curve options recover from fixed and class records", "[class]")
{
    const std::vector<SyntheticRow> fixedRows{
        {GLOBALS_CMPER, "15", {0, 0, 0, 0, 23, 0}},
        {GLOBALS_CMPER, "27", {0, 0, 0, 24, 0, 0}},
        {GLOBALS_CMPER, "58", {0, 0, 0, 0, 0, 25}},
        {GLOBALS_CMPER, "59", {26, 27, 28, 0, 0, 0}},
        {GLOBALS_CMPER, "01", {0, 0, 29, 30, 0, 0}},
        {GLOBALS_CMPER, "62", {-1, -12500, 0, 6250, 0, 0}},
        {GLOBALS_CMPER, "97", {0, 8750, 0, 0, 0, 0}},
    };
    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        ImportReport report(epoch);
        const auto options = importLineCurveOptions(makeContainer(fixedRows, epoch), epoch, report);
        if (epoch == FormatEpoch::DclLegacy) {
            verifyLaterLineCurveFields(options, report);
        } else {
            expect(options->restLegerFrontLength == 108 && options->restLegerBackLength == 109 && std::abs(options->pathSlurTipWidth - 1.3) < 0.00001,
                "An uncompressed record was interpreted with a later line-curve layout");
            expect(field(report, "options.lineCurveOptions.restLegerFrontLength").origin == ValueOrigin::Finale27Default,
                "An unavailable uncompressed line-curve field reported an incorrect origin");
        }
    }

    const std::vector<SyntheticClassRow> classRows{
        {finale_mus_reader::numericGlobalClass(15), {0, 0, 0, 0, 23, 0}},
        {finale_mus_reader::numericGlobalClass(27), {0, 0, 0, 24, 0, 0}},
        {finale_mus_reader::numericGlobalClass(58), {0, 0, 0, 0, 0, 25}},
        {finale_mus_reader::numericGlobalClass(59), {26, 27, 28, 0, 0, 0}},
        {finale_mus_reader::numericGlobalClass(1), {0, 0, 29, 30, 0, 0}},
        {finale_mus_reader::numericGlobalClass(62), {-1, -12500, 0, 6250, 0, 0}},
        {finale_mus_reader::numericGlobalClass(97), {0, 8750, 0, 0, 0, 0}},
    };
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        ImportReport report(FormatEpoch::ZlibLegacy);
        verifyLaterLineCurveFields(importLineCurveOptions(makeClassContainer(classRows, byteOrder), FormatEpoch::ZlibLegacy, report), report);
    }
}

TEST_CASE("Coda line and curve options use only records present in that epoch", "[class]")
{
    const std::vector<SyntheticRow> rows{
        {GLOBALS_CMPER, "01", {0, 0, -29, -30, 0, 0}},
        {GLOBALS_CMPER, "15", {0, 0, 0, 0, 33, 0}},
        {GLOBALS_CMPER, "27", {0, 0, 0, -24, 0, 0}},
        {GLOBALS_CMPER, "52", {-16707, 28836, 15762, 1468, 0, 0}},
        {GLOBALS_CMPER, "62", {-1, -2500, 0, 419, 0, 0}},
    };
    ImportReport report(FormatEpoch::CodaBanner);
    const auto options = importLineCurveOptions(makeContainer(rows, FormatEpoch::CodaBanner), FormatEpoch::CodaBanner, report);
    expect(options->bezierStep == 33 && std::abs(options->psUlDepth + 0.25) < 0.00001 && std::abs(options->psUlWidth - 0.0419) < 0.00001,
        "Coda line-and-curve fields were not recovered");
    expect(options->legerFrontLength == 106 && options->restLegerFrontLength == 108, "A colliding Coda record was interpreted with a later layout");
    expect(options->enclosureWidth == 118 && options->staffLineWidth == 118 && options->legerLineWidth == 118,
        "Coda enclosure-, staff-, and ledger-line behavior was not applied");
    expect(field(report, "options.lineCurveOptions.psUlDepth").origin == ValueOrigin::LegacyMus,
        "A recovered Coda line-and-curve field reported an incorrect origin");
    expect(field(report, "options.lineCurveOptions.enclosureWidth").origin == ValueOrigin::LegacyBehavior
               && field(report, "options.lineCurveOptions.staffLineWidth").origin == ValueOrigin::LegacyBehavior
               && field(report, "options.lineCurveOptions.legerLineWidth").origin == ValueOrigin::LegacyBehavior,
        "Coda enclosure-, staff-, and ledger-line behavior reported incorrect origins");
    expect(field(report, "options.lineCurveOptions.pathSlurTipWidth").origin == ValueOrigin::Finale27Default,
        "An absent Coda line-and-curve field reported an incorrect origin");
}

TEST_CASE("Uncompressed line and curve options apply absent enclosure-width behavior", "[class]")
{
    const std::vector<SyntheticRow> rows{
        {GLOBALS_CMPER, "15", {0, 0, 0, 0, 23, 0}},
        {GLOBALS_CMPER, "58", {0, 0, 0, 0, 0, 25}},
        {GLOBALS_CMPER, "59", {26, 27, 28, 0, 0, 0}},
        {GLOBALS_CMPER, "62", {-1, -12500, 0, 6250, 0, 0}},
    };
    ImportReport report(FormatEpoch::UncompressedLegacy);
    const auto options = importLineCurveOptions(makeContainer(rows, FormatEpoch::UncompressedLegacy), FormatEpoch::UncompressedLegacy, report);

    expect(options->enclosureWidth == 118 && options->staffLineWidth == 25 && options->legerLineWidth == 26,
        "An absent uncompressed enclosure-width selector applied the wrong behavior");
    expect(field(report, "options.lineCurveOptions.enclosureWidth").origin == ValueOrigin::LegacyBehavior
               && field(report, "options.lineCurveOptions.staffLineWidth").origin == ValueOrigin::LegacyMus,
        "Absent uncompressed enclosure-width behavior reported an incorrect origin");
}

TEST_CASE("Controlled Finale 2.6 curve options recover resolution and underline values", "[class]")
{
    const auto baseline = readFixture("evidence/F263/F263-baseline.mus");
    const auto changed = readFixture("evidence/F263/F263-curve-opt.mus");
    const auto changedAgain = readFixture("evidence/F263/F263-curves.mus");
    const auto upgradedLines = readFixture("evidence/F263/F263-from-F100-lines.mus");
    const auto baselineOptions = baseline.document->getOptions()->get<LineCurveTarget>();
    const auto changedOptions = changed.document->getOptions()->get<LineCurveTarget>();
    const auto changedAgainOptions = changedAgain.document->getOptions()->get<LineCurveTarget>();
    const auto upgradedLinesOptions = upgradedLines.document->getOptions()->get<LineCurveTarget>();
    expect(baselineOptions->bezierStep == 16 && changedOptions->bezierStep == 33 && changedAgainOptions->bezierStep == 67
               && std::abs(changedOptions->psUlDepth + 0.25) < 0.00001 && std::abs(changedOptions->psUlWidth - 0.0419) < 0.00001,
        "Controlled Finale 2.6 line-and-curve values were not recovered");
    expect(std::abs(upgradedLinesOptions->psUlDepth + 0.37) < 0.00001 && std::abs(upgradedLinesOptions->psUlWidth - 0.0712) < 0.00001,
        "Finale 2.6 did not recover its fixed-point copy of the Finale 1 underline values");
    expect(field(changed, "options.lineCurveOptions.bezierStep").origin == ValueOrigin::LegacyMus
               && field(changed, "options.lineCurveOptions.psUlDepth").origin == ValueOrigin::LegacyMus
               && field(upgradedLines, "options.lineCurveOptions.psUlWidth").origin == ValueOrigin::LegacyMus,
        "Controlled Finale 2.6 line-and-curve values reported incorrect origins");
}

TEST_CASE("Controlled Finale 1 curve resolution uses its zero sentinel", "[class]")
{
    const auto baseline = readFixture("evidence/F100/F100-baseline.mus");
    const auto changed = readFixture("evidence/F100/F100-lines.mus");
    const auto baselineOptions = baseline.document->getOptions()->get<LineCurveTarget>();
    const auto changedOptions = changed.document->getOptions()->get<LineCurveTarget>();
    expect(baselineOptions->bezierStep == 16, "The Finale 1 curve-resolution sentinel was not recovered");
    expect(std::abs(baselineOptions->psUlDepth + 0.25) < 0.00001 && std::abs(baselineOptions->psUlWidth - 0.0416) < 0.00001,
        "The Finale 1 PostScript underline defaults were not recovered");
    expect(field(changed, "options.lineCurveOptions.psUlDepth").origin == ValueOrigin::LegacyMus,
        "The controlled Finale 1 PostScript underline fields were not mapped");
    expect(std::abs(changedOptions->psUlDepth + 0.37) < 0.00001, "The controlled Finale 1 PostScript underline depth was not recovered");
    expect(std::abs(changedOptions->psUlWidth - 0.0713) < 0.00001, "The controlled Finale 1 PostScript underline width was not recovered");
    expect(field(baseline, "options.lineCurveOptions.bezierStep").origin == ValueOrigin::LegacyBehavior
               && field(changed, "options.lineCurveOptions.psUlDepth").origin == ValueOrigin::LegacyMus
               && field(changed, "options.lineCurveOptions.psUlWidth").origin == ValueOrigin::LegacyMus,
        "The controlled Finale 1 line-and-curve values reported incorrect origins");
}

} // namespace
} // namespace finale_mus_reader_tests
