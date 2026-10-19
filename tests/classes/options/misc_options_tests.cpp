// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

using MiscOptionsTarget = musx::dom::options::MiscOptions;

musx::dom::DocumentPtr makeMiscOptionsDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<MiscOptionsTarget>(document);
    options->showRepeatsForParts = false;
    options->pickupValue = 101;
    options->keepWrittenOctaveInConcertPitch = true;
    options->showActiveLayerOnly = false;
    options->consolidateRestsAcrossLayers = true;
    options->shapeDesignerDashLength = 102;
    options->shapeDesignerDashSpace = 103;
    options->restWidthAdjust = 104;
    options->dblWholeVertAdjust = 105;
    options->alignMeasureNumbersWithBarlines = true;
    document->getOptions()->add(MiscOptionsTarget::XmlNodeName, options);
    return std::move(session).finish();
}

std::shared_ptr<const MiscOptionsTarget> importMiscOptions(
    const finale_mus_reader::container::ParsedContainer& parsed, FormatEpoch epoch, ImportReport& report, std::uint8_t sourceMajor = 0)
{
    const auto document = makeMiscOptionsDocument();
    const auto reference = makeMiscOptionsDocument();
    const std::uint8_t defaultMajor = epoch == FormatEpoch::ZlibLegacy ? 17 : 9;
    auto profile = profileFor(sourceMajor ? sourceMajor : defaultMajor);
    profile.epoch = epoch;
    profile.byteOrder = parsed.byteOrder;
    if (epoch == FormatEpoch::CodaBanner) {
        profile.version.reset();
    }
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{
        LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::options::importMiscOptions(context);
    return document->getOptions()->get<MiscOptionsTarget>();
}

void verifyMiscOptions(const MiscOptionsTarget& options, const ImportReport& report, bool expectedShowRepeatsForParts,
    ValueOrigin expectedShowRepeatsForPartsOrigin, bool expectedShowActiveLayerOnly, ValueOrigin expectedShowActiveLayerOnlyOrigin)
{
    expect(options.pickupValue == 2048, "MiscOptions did not recover its located pickup value");
    expect(options.showActiveLayerOnly == expectedShowActiveLayerOnly, "MiscOptions did not apply the epoch's active-layer behavior");
    expect(options.showRepeatsForParts == expectedShowRepeatsForParts, "MiscOptions did not apply the epoch's repeat-part behavior");
    expect(options.shapeDesignerDashLength == 102 && options.shapeDesignerDashSpace == 103 && options.restWidthAdjust == 104
               && options.dblWholeVertAdjust == 105,
        "MiscOptions disturbed an unresolved seeded field");
    expect(!options.consolidateRestsAcrossLayers && !options.alignMeasureNumbersWithBarlines, "MiscOptions did not apply fixed legacy behavior");
    expect(options.keepWrittenOctaveInConcertPitch, "MiscOptions disturbed the seeded concert-pitch octave behavior");
    expect(field(report, "options.miscOptions.pickupValue").origin == ValueOrigin::LegacyMus,
        "MiscOptions did not report the pickup value as legacy MUS data");
    expect(field(report, "options.miscOptions.showActiveLayerOnly").origin == expectedShowActiveLayerOnlyOrigin,
        "MiscOptions did not report the active-layer value with its epoch "
        "origin");
    expect(field(report, "options.miscOptions.showRepeatsForParts").origin == expectedShowRepeatsForPartsOrigin,
        "MiscOptions did not report the repeat-part value with its epoch "
        "origin");
    for (const auto* member :
        {"shapeDesignerDashLength", "shapeDesignerDashSpace", "restWidthAdjust", "dblWholeVertAdjust", "keepWrittenOctaveInConcertPitch"}) {
        expect(field(report, std::string("options.miscOptions.") + member).origin == ValueOrigin::Finale27Default,
            std::string("MiscOptions did not retain the Finale 27 fallback for ") + member);
    }
    for (const auto* member : {"consolidateRestsAcrossLayers", "alignMeasureNumbersWithBarlines"}) {
        expect(field(report, std::string("options.miscOptions.") + member).origin == ValueOrigin::LegacyBehavior,
            std::string("MiscOptions did not report fixed legacy behavior for ") + member);
    }
    expect(reportedFieldCount(report) == 10, "MiscOptions report does not exhaust the musxdom field manifest");
}

TEST_CASE("MiscOptions recovers fixed-row layouts and reports every field")
{
    const std::vector<SyntheticRow> rows{
        {GLOBALS_CMPER, "16", {0, 0, 0, 0, 0, 1}},
        {GLOBALS_CMPER, "17", {0, 0, 0, 0, 0, 2048}},
        {10, "FI", {0, 0, 0, 0, 1, 0}},
    };
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        ImportReport report(epoch);
        const auto options = importMiscOptions(makeContainer(rows, epoch), epoch, report);
        const auto isCoda = epoch == FormatEpoch::CodaBanner;
        verifyMiscOptions(*options, report, true, ValueOrigin::LegacyMus, !isCoda, isCoda ? ValueOrigin::Finale27Default : ValueOrigin::LegacyMus);
    }
}

TEST_CASE("Coda repeat-part display option recovers both states and byte orders")
{
    for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        for (const auto value : {std::int16_t{0}, std::int16_t{1}}) {
            ImportReport report(FormatEpoch::CodaBanner);
            const auto parsed = makeContainer({{GLOBALS_CMPER, "16", {0, 0, 0, 0, 0, value}}}, FormatEpoch::CodaBanner, order);
            const auto options = importMiscOptions(parsed, FormatEpoch::CodaBanner, report);
            CHECK(options->showRepeatsForParts == (value != 0));
            const auto& origin = field(report, "options.miscOptions.showRepeatsForParts");
            CHECK(origin.origin == ValueOrigin::LegacyMus);
            CHECK(origin.rawValue == value);
        }
    }
}

TEST_CASE("MiscOptions recovers zlib class records in both byte orders")
{
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        ImportReport report(FormatEpoch::ZlibLegacy);
        const auto parsed = makeClassContainer(
            {
                SyntheticClassRow{finale_mus_reader::numericGlobalClass(16), {0, 0, 0, 0, 0, 1}, GLOBALS_CMPER},
                SyntheticClassRow{finale_mus_reader::numericGlobalClass(17), {0, 0, 0, 0, 0, 2048}, GLOBALS_CMPER},
                SyntheticClassRow{0x008d, {0, 0, 0, 0, 1, 0}, 10},
            },
            byteOrder);
        const auto options = importMiscOptions(parsed, FormatEpoch::ZlibLegacy, report);
        verifyMiscOptions(*options, report, true, ValueOrigin::LegacyMus, true, ValueOrigin::LegacyMus);
    }
}

TEST_CASE("Controlled repeat-part fixtures preserve the setting across layouts")
{
    for (const auto* path : {
             "evidence/F2005/F2005-rpts-forparts.mus",
             "evidence/F2012/F2012-F2005-rpts-forparts.mus",
         }) {
        const auto result = readFixture(path);
        const auto options = result.document->getOptions()->get<MiscOptionsTarget>();
        expect(options->showRepeatsForParts, "The controlled fixture did not recover repeat display for parts");
        expect(field(result, "options.miscOptions.showRepeatsForParts").origin == ValueOrigin::LegacyMus,
            "The controlled repeat-part fixture reported an incorrect origin");
    }
}

TEST_CASE("Controlled Coda pickup fixture recovers selector 17 word 5")
{
    const auto baseline = readFixture("evidence/F100/F100-baseline.mus");
    const auto changed = readFixture("evidence/F100/F100-pickup-512.mus");
    const auto baselineOptions = baseline.document->getOptions()->get<MiscOptionsTarget>();
    const auto changedOptions = changed.document->getOptions()->get<MiscOptionsTarget>();

    expect(baselineOptions->pickupValue == 0 && changedOptions->pickupValue == 512, "The controlled Coda pickup duration was not recovered");
    const auto& changedField = field(changed, "options.miscOptions.pickupValue");
    expect(changedField.origin == ValueOrigin::LegacyMus && changedField.rawValue == 512 && changedField.blockOffset == 0x208
               && changedField.decodedOffset == 0x550,
        "The controlled Coda pickup duration reported incorrect provenance");
}

TEST_CASE("Controlled uncompressed active-layer fixture recovers FI word 4")
{
    const auto baseline = readFixture("evidence/F372/F372-baseline.mus");
    const auto changed = readFixture("evidence/F372/F372-activelayer-only.mus");
    const auto baselineOptions = baseline.document->getOptions()->get<MiscOptionsTarget>();
    const auto changedOptions = changed.document->getOptions()->get<MiscOptionsTarget>();

    expect(!baselineOptions->showActiveLayerOnly && changedOptions->showActiveLayerOnly, "The controlled active-layer setting was not recovered");
    const auto& changedField = field(changed, "options.miscOptions.showActiveLayerOnly");
    expect(changedField.origin == ValueOrigin::LegacyMus && changedField.rawValue == 1 && changedField.blockOffset == 0x200
               && changedField.decodedOffset == 0x12c0,
        "The controlled active-layer setting reported incorrect provenance");
}

TEST_CASE("Controlled active-layer migration recovers the zlib class")
{
    const auto result = readFixture("evidence/F2012/F2012-F372-activelayer-only.mus");
    const auto options = result.document->getOptions()->get<MiscOptionsTarget>();

    expect(options->showActiveLayerOnly, "The Finale 2012 migration lost the active-layer setting");
    const auto& source = field(result, "options.miscOptions.showActiveLayerOnly");
    expect(source.origin == ValueOrigin::LegacyMus && source.rawValue == 1 && source.blockOffset == 0x200 && source.decodedOffset == 0x189e,
        "The migrated active-layer setting reported incorrect provenance");
}

TEST_CASE("MiscOptions retains defaults when source records are absent")
{
    ImportReport report(FormatEpoch::UncompressedLegacy);
    const auto options = importMiscOptions(makeContainer({}), FormatEpoch::UncompressedLegacy, report);
    expect(options->pickupValue == 101 && !options->showActiveLayerOnly, "MiscOptions did not retain seeded defaults for absent records");
    expect(field(report, "options.miscOptions.pickupValue").origin == ValueOrigin::Finale27Default
               && field(report, "options.miscOptions.showActiveLayerOnly").origin == ValueOrigin::Finale27Default,
        "MiscOptions did not report absent located fields as defaults");
}

} // namespace
} // namespace finale_mus_reader_tests
