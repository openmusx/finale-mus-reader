// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <bit>
#include <cstdint>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

using PianoBraceBracketTestTarget = musx::dom::options::PianoBraceBracketOptions;

musx::dom::DocumentPtr makePianoBraceBracketOptionsDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<PianoBraceBracketTestTarget>(document);
    options->defBracketPos = -12;
    options->centerThickness = 9.1;
    options->tipThickness = 9.2;
    options->outerBodyV = 9.3;
    options->innerTipV = 9.4;
    options->innerBodyV = 9.5;
    options->outerTipH = 9.6;
    options->outerTipV = 9.7;
    options->outerBodyH = 9.8;
    options->width = 9.9;
    options->innerTipH = 10.0;
    options->innerBodyH = 10.1;
    document->getOptions()->add(PianoBraceBracketTestTarget::XmlNodeName, options);
    return std::move(session).finish();
}

std::shared_ptr<const PianoBraceBracketTestTarget> importPianoBraceBracketOptions(const finale_mus_reader::container::ParsedContainer& parsed,
    FormatEpoch epoch, ImportReport& report, std::uint8_t sourceMajor = 9, std::uint8_t sourceMinor = 0)
{
    const auto document = makePianoBraceBracketOptionsDocument();
    const auto reference = makePianoBraceBracketOptionsDocument();
    auto profile = profileFor(epoch == FormatEpoch::ZlibLegacy ? 12 : sourceMajor, sourceMinor);
    profile.epoch = epoch;
    profile.byteOrder = parsed.byteOrder;
    if (epoch == FormatEpoch::CodaBanner) {
        profile.version.reset();
    }
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{
        LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::options::importPianoBraceBracketOptions(context);
    return document->getOptions()->get<PianoBraceBracketTestTarget>();
}

void verifyPianoBraceBracketOptions(const PianoBraceBracketTestTarget& options, const ImportReport& report, bool recoversDefaultBracketPosition)
{
    expectMapping(options.defBracketPos == -12 && options.centerThickness == 1.0 && options.tipThickness == 2.0 && options.outerBodyV == 3.0
                      && options.innerTipV == 4.0 && options.innerBodyV == 5.0 && options.outerTipH == 6.0 && options.outerTipV == -1.0
                      && options.outerBodyH == -2.0 && options.width == 7.0 && options.innerTipH == 8.0 && options.innerBodyH == 9.0,
        "PianoBraceBracketOptions did not recover its stored fields");
    expectMapping(field(report, "options.pianoBraceBracketOptions.defBracketPos").origin
                      == (recoversDefaultBracketPosition ? ValueOrigin::LegacyMus : ValueOrigin::Finale27Default),
        "PianoBraceBracketOptions reported an incorrect bracket-position origin");
    for (const auto* member : {"centerThickness", "tipThickness", "outerBodyV", "innerTipV", "innerBodyV", "outerTipH", "outerTipV", "outerBodyH",
             "width", "innerTipH", "innerBodyH"}) {
        expectMapping(field(report, std::string("options.pianoBraceBracketOptions.") + member).origin == ValueOrigin::LegacyMus,
            "PianoBraceBracketOptions reported an incorrect field origin");
    }
}

std::vector<SyntheticRow> pianoBraceBracketFixedRows()
{
    return {
        {GLOBALS_CMPER, "14", {0, 0, 0, -12, 0, 0}},
        {GLOBALS_CMPER, "45", {0, 0, 0, 10000, 0, 20000}},
        {GLOBALS_CMPER, "60", {0, 30000, 0, -25536, 0, -15536}},
        {GLOBALS_CMPER, "61", {0, -5536, -1, -10000, -1, -20000}},
        {GLOBALS_CMPER, "64", {0, 0, 1, 4464, 0, 0}},
        {GLOBALS_CMPER, "65", {0, 0, 1, 14464, 1, 24464}},
    };
}

std::vector<SyntheticClassRow> pianoBraceBracketClassRows()
{
    const auto fixed = pianoBraceBracketFixedRows();
    std::vector<SyntheticClassRow> result;
    result.reserve(fixed.size());
    for (const auto& row : fixed) {
        const auto selector = static_cast<std::uint16_t>((row.tag[0] - '0') * 10 + (row.tag[1] - '0'));
        result.push_back({finale_mus_reader::numericGlobalClass(selector), {row.words.begin(), row.words.end()}});
    }
    return result;
}

TEST_CASE("Piano brace and bracket options recover fixed-row geometry", "[class]")
{
    const auto rows = pianoBraceBracketFixedRows();
    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        ImportReport report(epoch);
        verifyPianoBraceBracketOptions(
            *importPianoBraceBracketOptions(makeContainer(rows, epoch), epoch, report), report, epoch == FormatEpoch::DclLegacy);
    }
}

TEST_CASE("Piano brace default bracket position begins in Finale 2004", "[class]")
{
    auto rows = pianoBraceBracketFixedRows();
    rows.front().words[3] = -17;

    ImportReport beforeReport(FormatEpoch::DclLegacy);
    const auto before = importPianoBraceBracketOptions(makeContainer(rows, FormatEpoch::DclLegacy), FormatEpoch::DclLegacy, beforeReport, 8);
    expectMapping(
        before->defBracketPos == -12 && field(beforeReport, "options.pianoBraceBracketOptions.defBracketPos").origin == ValueOrigin::Finale27Default,
        "Pre-Finale-2004 selector 14 disturbed the seeded bracket position");

    ImportReport finale2004Report(FormatEpoch::DclLegacy);
    const auto finale2004 = importPianoBraceBracketOptions(makeContainer(rows, FormatEpoch::DclLegacy), FormatEpoch::DclLegacy, finale2004Report, 9);
    expectMapping(finale2004->defBracketPos == -17
                      && field(finale2004Report, "options.pianoBraceBracketOptions.defBracketPos").origin == ValueOrigin::LegacyMus,
        "Finale 2004 did not recover its stored bracket position");
}

TEST_CASE("Piano brace thickness options begin with Finale 3.7", "[class]")
{
    const auto rows = pianoBraceBracketFixedRows();

    ImportReport beforeReport(FormatEpoch::UncompressedLegacy);
    const auto before = importPianoBraceBracketOptions(makeContainer(rows), FormatEpoch::UncompressedLegacy, beforeReport, 3, 5);
    expectMapping(before->centerThickness == 2.0 && before->tipThickness == 0.0
                      && field(beforeReport, "options.pianoBraceBracketOptions.centerThickness").origin == ValueOrigin::LegacyBehavior
                      && field(beforeReport, "options.pianoBraceBracketOptions.tipThickness").origin == ValueOrigin::LegacyBehavior,
        "Pre-Finale-3.7 selector 45 disturbed fixed brace-thickness behavior");

    ImportReport finale37Report(FormatEpoch::UncompressedLegacy);
    const auto finale37 = importPianoBraceBracketOptions(makeContainer(rows), FormatEpoch::UncompressedLegacy, finale37Report, 3, 7);
    expectMapping(finale37->centerThickness == 1.0 && finale37->tipThickness == 2.0
                      && field(finale37Report, "options.pianoBraceBracketOptions.centerThickness").origin == ValueOrigin::LegacyMus
                      && field(finale37Report, "options.pianoBraceBracketOptions.tipThickness").origin == ValueOrigin::LegacyMus,
        "Finale 3.7 did not recover its stored brace-thickness options");
}

TEST_CASE("Coda piano brace combines stored parameters with legacy behavior", "[class]")
{
    auto rows = pianoBraceBracketFixedRows();
    rows.push_back({GLOBALS_CMPER, "55", {16128, 0, 16457, 4048, 16429, -1971}});
    ImportReport report(FormatEpoch::CodaBanner);
    const auto options = importPianoBraceBracketOptions(makeContainer(rows), FormatEpoch::CodaBanner, report);
    expectMapping(options->defBracketPos == -12 && options->centerThickness == 2.0 && options->tipThickness == 0.0 && options->outerBodyV == 0.0
                      && options->innerTipV == 0.0 && options->innerBodyV == 0.0 && options->outerTipH == 0.0 && options->outerTipV == 0.0
                      && options->outerBodyH == 0.0 && options->width == 12.0
                      && options->innerTipH == static_cast<double>(std::bit_cast<float>(UINT32_C(0x40490fd0))) * 4.0
                      && options->innerBodyH == static_cast<double>(std::bit_cast<float>(UINT32_C(0x402df84d))) * 4.0,
        "Coda PianoBraceBracketOptions did not recover its stored brace "
        "parameters");
    expectMapping(field(report, "options.pianoBraceBracketOptions.defBracketPos").origin == ValueOrigin::Finale27Default,
        "Coda piano brace default bracket position reported an incorrect origin");
    for (const auto* member :
        {"centerThickness", "tipThickness", "outerBodyV", "innerTipV", "innerBodyV", "outerTipH", "outerTipV", "outerBodyH", "width"}) {
        expectMapping(field(report, std::string("options.pianoBraceBracketOptions.") + member).origin == ValueOrigin::LegacyBehavior,
            "Coda PianoBraceBracketOptions field reported an incorrect origin");
    }
    for (const auto* member : {"innerTipH", "innerBodyH"}) {
        expectMapping(field(report, std::string("options.pianoBraceBracketOptions.") + member).origin == ValueOrigin::LegacyMus,
            "Coda piano brace parameter reported an incorrect origin");
    }
}

TEST_CASE("Later Coda piano brace parameters use the same stored scale", "[class]")
{
    const std::vector<SyntheticRow> rows = {
        {GLOBALS_CMPER, "55", {16128, 0, 16459, 13107, 16465, -26214}},
    };
    ImportReport report(FormatEpoch::CodaBanner);
    const auto options = importPianoBraceBracketOptions(makeContainer(rows), FormatEpoch::CodaBanner, report);

    expectMapping(options->innerTipH == static_cast<double>(3.175F) * 4.0 && options->innerBodyH == static_cast<double>(3.275F) * 4.0,
        "Later Coda PianoBraceBracketOptions did not recover its stored fields");
    for (const auto* member : {"innerTipH", "innerBodyH"}) {
        expectMapping(field(report, std::string("options.pianoBraceBracketOptions.") + member).origin == ValueOrigin::LegacyMus,
            "Later Coda piano brace field reported an incorrect origin");
    }
    expectMapping(field(report, "options.pianoBraceBracketOptions.width").origin == ValueOrigin::LegacyBehavior,
        "Later Coda fixed geometry reported an incorrect origin");
}

TEST_CASE("Coda piano brace uses fixed horizontal parameters without selector 55", "[class]")
{
    ImportReport report(FormatEpoch::CodaBanner);
    const auto options = importPianoBraceBracketOptions(makeContainer({}), FormatEpoch::CodaBanner, report);

    expectMapping(options->innerTipH == 12.0 && options->innerBodyH == 12.0, "Coda piano brace did not apply its absent-selector behavior");
    for (const auto* member : {"innerTipH", "innerBodyH"}) {
        expectMapping(field(report, std::string("options.pianoBraceBracketOptions.") + member).origin == ValueOrigin::LegacyBehavior,
            "Coda absent-selector piano brace field reported an incorrect origin");
    }
}

TEST_CASE("Piano brace and bracket options recover class records in either "
          "byte order",
    "[class]")
{
    const auto rows = pianoBraceBracketClassRows();
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        ImportReport report(FormatEpoch::ZlibLegacy);
        verifyPianoBraceBracketOptions(
            *importPianoBraceBracketOptions(makeClassContainer(rows, byteOrder), FormatEpoch::ZlibLegacy, report), report, true);
    }
}

TEST_CASE("Absent piano brace and bracket records retain seeded defaults", "[class]")
{
    ImportReport report(FormatEpoch::UncompressedLegacy);
    const auto options = importPianoBraceBracketOptions(makeContainer({}), FormatEpoch::UncompressedLegacy, report);
    expectMapping(options->defBracketPos == -12 && options->centerThickness == 9.1 && options->innerBodyH == 10.1,
        "Absent PianoBraceBracketOptions records disturbed seeded defaults");
    expectMapping(field(report, "options.pianoBraceBracketOptions.defBracketPos").origin == ValueOrigin::Finale27Default
                      && field(report, "options.pianoBraceBracketOptions.innerBodyH").origin == ValueOrigin::Finale27Default,
        "Absent PianoBraceBracketOptions records reported incorrect origins");
}

} // namespace
} // namespace finale_mus_reader_tests
