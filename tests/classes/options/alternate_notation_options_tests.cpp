// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

musx::dom::DocumentPtr makeAlternateNotationOptionsDocument()
{
    using Target = musx::dom::options::AlternateNotationOptions;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<Target>(document);
    options->halfSlashLift = 91;
    options->wholeSlashLift = 92;
    options->dWholeSlashLift = 93;
    options->halfSlashStemLift = 94;
    options->quartSlashStemLift = 95;
    options->quartSlashLift = 96;
    options->twoMeasNumLift = 97;
    document->getOptions()->add(Target::XmlNodeName, options);
    return std::move(session).finish();
}

void testAlternateNotationOptionsAcrossEpochs()
{
    using Target = musx::dom::options::AlternateNotationOptions;
    const auto runImport = [](const finale_mus_reader::container::ParsedContainer& parsed, FormatEpoch epoch, finale_mus_reader::VersionBound version,
                               ImportReport& report) {
        const auto document = makeAlternateNotationOptionsDocument();
        const auto reference = makeAlternateNotationOptionsDocument();
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
        finale_mus_reader::options::importAlternateNotationOptions(context);
        return document->getOptions()->get<Target>();
    };
    const auto verify = [](const auto& options, const ImportReport& report, std::int16_t originShift = 0, std::int16_t twoMeasNumLift = -29,
                            ValueOrigin twoMeasOrigin = ValueOrigin::LegacyMus) {
        const auto slashOrigin = originShift == 0 ? ValueOrigin::LegacyMus : ValueOrigin::LegacyMusAdjusted;
        expectMapping(options->halfSlashLift == -7 - originShift && options->wholeSlashLift == 11 - originShift
                          && options->dWholeSlashLift == -13 - originShift && options->halfSlashStemLift == 17 - originShift
                          && options->quartSlashStemLift == -19 - originShift && options->quartSlashLift == 23 - originShift
                          && options->twoMeasNumLift == twoMeasNumLift,
            "Alternate notation options did not recover all seven numeric fields");
        expectMapping(field(report, "options.alternateNotationOptions.halfSlashLift").origin == slashOrigin
                          && field(report, "options.alternateNotationOptions.halfSlashStemLift").origin == slashOrigin
                          && field(report, "options.alternateNotationOptions.quartSlashStemLift").origin == slashOrigin
                          && field(report, "options.alternateNotationOptions.quartSlashLift").origin == slashOrigin
                          && field(report, "options.alternateNotationOptions.twoMeasNumLift").origin == twoMeasOrigin,
            "Alternate notation options reported an incorrect field origin");
    };

    const std::vector<SyntheticRow> fixedRows{
        {GLOBALS_CMPER, "22", {0, -7, 11, -13, 0, 0}},
        {GLOBALS_CMPER, "43", {0, 0, 0, 17, -19, 23}},
        {GLOBALS_CMPER, "46", {0, 0, 0, 0, 0, -29}},
    };
    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        ImportReport report(epoch);
        const auto version = epoch == FormatEpoch::DclLegacy ? finale_mus_reader::versions::finale2005 : finale_mus_reader::versions::finale2000;
        verify(runImport(makeContainer(fixedRows, epoch), epoch, version, report), report);
    }

    ImportReport finale37Report(FormatEpoch::UncompressedLegacy);
    verify(runImport(makeContainer(fixedRows, FormatEpoch::UncompressedLegacy), FormatEpoch::UncompressedLegacy,
               finale_mus_reader::versions::finale3_7, finale37Report),
        finale37Report, static_cast<std::int16_t>(musx::dom::EVPU_PER_SPACE));

    ImportReport finale97Report(FormatEpoch::UncompressedLegacy);
    verify(runImport(makeContainer(fixedRows, FormatEpoch::UncompressedLegacy), FormatEpoch::UncompressedLegacy,
               finale_mus_reader::versions::finale97, finale97Report),
        finale97Report);

    const std::vector<SyntheticRow> selector43Rows(fixedRows.begin(), fixedRows.end() - 1);
    ImportReport selector43Report(FormatEpoch::CodaBanner);
    const auto selector43 = runImport(
        makeContainer(selector43Rows, FormatEpoch::CodaBanner), FormatEpoch::CodaBanner, finale_mus_reader::versions::finale2_6, selector43Report);
    expectMapping(selector43->halfSlashLift == -24 && selector43->wholeSlashLift == -24 && selector43->dWholeSlashLift == -24
                      && selector43->halfSlashStemLift == -31 && selector43->quartSlashStemLift == -67 && selector43->quartSlashLift == -25
                      && selector43->twoMeasNumLift == 0,
        "Pre-selector-46 alternate notation behavior was not recovered");
    expectMapping(field(selector43Report, "options.alternateNotationOptions.halfSlashLift").origin == ValueOrigin::LegacyBehavior
                      && field(selector43Report, "options.alternateNotationOptions.halfSlashStemLift").origin == ValueOrigin::LegacyMusAdjusted
                      && field(selector43Report, "options.alternateNotationOptions.halfSlashStemLift").rawValue == 17
                      && field(selector43Report, "options.alternateNotationOptions.twoMeasNumLift").origin == ValueOrigin::LegacyBehavior,
        "Pre-selector-46 alternate notation origins were not reported");

    const std::vector<SyntheticRow> noSelector43Rows{fixedRows.front()};
    ImportReport noSelector43Report(FormatEpoch::CodaBanner);
    const auto noSelector43 = runImport(makeContainer(noSelector43Rows, FormatEpoch::CodaBanner), FormatEpoch::CodaBanner,
        finale_mus_reader::versions::finale1_0, noSelector43Report);
    expectMapping(noSelector43->halfSlashLift == -24 && noSelector43->wholeSlashLift == -24 && noSelector43->dWholeSlashLift == -24
                      && noSelector43->halfSlashStemLift == -24 && noSelector43->quartSlashStemLift == -24 && noSelector43->quartSlashLift == -24
                      && noSelector43->twoMeasNumLift == 0,
        "Pre-selector-43 alternate notation behavior was not recovered");
    expectMapping(field(noSelector43Report, "options.alternateNotationOptions.halfSlashLift").origin == ValueOrigin::LegacyBehavior
                      && field(noSelector43Report, "options.alternateNotationOptions.halfSlashStemLift").origin == ValueOrigin::LegacyBehavior
                      && field(noSelector43Report, "options.alternateNotationOptions.twoMeasNumLift").origin == ValueOrigin::LegacyBehavior,
        "Pre-selector-43 alternate notation origins were not reported");

    const std::vector<SyntheticClassRow> classRows{
        {finale_mus_reader::numericGlobalClass(22), {0, -7, 11, -13}},
        {finale_mus_reader::numericGlobalClass(43), {0, 0, 0, 17, -19, 23}},
        {finale_mus_reader::numericGlobalClass(46), {0, 0, 0, 0, 0, -29}},
    };
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        ImportReport report(FormatEpoch::ZlibLegacy);
        verify(runImport(makeClassContainer(classRows, byteOrder), FormatEpoch::ZlibLegacy, finale_mus_reader::versions::finale2007, report), report);
    }
}

TEST_CASE("Alternate notation options span the located epochs", "[class]")
{
    testAlternateNotationOptionsAcrossEpochs();
}

} // namespace
} // namespace finale_mus_reader_tests
