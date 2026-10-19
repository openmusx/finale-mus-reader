// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

musx::dom::DocumentPtr makeRepeatOptionsDocument()
{
    using Repeat = musx::dom::options::RepeatOptions;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<Repeat>(document);
    options->maxPasses = 90;
    options->addPeriod = true;
    options->thickLineWidth = 91;
    options->thinLineWidth = 92;
    options->lineSpace = 93;
    options->backToBackStyle = Repeat::BackToBackStyle::Thin;
    options->forwardDotHPos = 94;
    options->backwardDotHPos = 95;
    options->upperDotVPos = 96;
    options->lowerDotVPos = 97;
    options->wingStyle = Repeat::WingStyle::None;
    options->afterClefSpace = 98;
    options->afterKeySpace = 99;
    options->afterTimeSpace = 100;
    options->bracketHeight = 101;
    options->bracketHookLen = 102;
    options->bracketLineWidth = 103;
    options->bracketStartInset = 104;
    options->bracketEndInset = 105;
    options->bracketTextHPos = 106;
    options->bracketTextVPos = 107;
    options->bracketEndHookLen = 108;
    options->bracketEndAnchorThinLine = true;
    options->showOnTopStaffOnly = true;
    options->showOnStaffListNumber = 9;
    document->getOptions()->add(Repeat::XmlNodeName, options);
    return std::move(session).finish();
}

void testRepeatOptionsAcrossEpochs()
{
    using Repeat = musx::dom::options::RepeatOptions;
    const std::vector<SyntheticRow> rows{
        {GLOBALS_CMPER, "05", {0, 0, 0, 14, 0, 0}},
        {GLOBALS_CMPER, "20", {0, 0, 0, 17, 0, 0}},
        {GLOBALS_CMPER, "69", {0, 1, 0, 0, 0, 0}},
        {GLOBALS_CMPER, "70", {111, 22, 333, 2, 44, 55}},
        {GLOBALS_CMPER, "71", {-6, -7, 3, 8, 9, 10}},
        {GLOBALS_CMPER, "72", {11, 12, 13, 14, 15, 16}},
        {GLOBALS_CMPER, "76", {0, 0, 18, 0, 0, 0}},
    };
    const auto runImport = [](const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile, ImportReport& report) {
        const auto document = makeRepeatOptionsDocument();
        const auto reference = makeRepeatOptionsDocument();
        const auto index = LegacyRecordIndex::build(parsed);
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::options::importRepeatOptions(context);
        return document->getOptions()->get<Repeat>();
    };
    const auto expectRecovered = [](const auto& options, const ImportReport& report, const std::string& epoch) {
        expectMapping(options->maxPasses == 17 && options->addPeriod, epoch + " did not recover the repeat pass settings");
        expectMapping(options->thickLineWidth == 111 && options->thinLineWidth == 22 && options->lineSpace == 333
                          && options->backToBackStyle == Repeat::BackToBackStyle::Thick,
            epoch + " did not recover the repeat line settings");
        expectMapping(options->forwardDotHPos == 44 && options->backwardDotHPos == 55 && options->upperDotVPos == -6 && options->lowerDotVPos == -7,
            epoch + " did not recover the repeat dot positions");
        expectMapping(options->wingStyle == Repeat::WingStyle::Curved && options->afterClefSpace == 8 && options->afterKeySpace == 9
                          && options->afterTimeSpace == 10,
            epoch + " did not recover the repeat wing and spacing settings");
        expectMapping(options->bracketHeight == 14 && options->bracketHookLen == 11 && options->bracketLineWidth == 12
                          && options->bracketStartInset == 13 && options->bracketEndInset == 14 && options->bracketTextHPos == 15
                          && options->bracketTextVPos == 16 && options->bracketEndHookLen == 18,
            epoch + " did not recover the ending bracket settings");
        expectMapping(!options->bracketEndAnchorThinLine && options->showOnTopStaffOnly && options->showOnStaffListNumber == 9,
            epoch + " did not assert the legacy anchor behavior or disturbed a staff-list option");
        expectMapping(field(report, "options.repeatOptions.maxPasses").origin == ValueOrigin::LegacyMus
                          && field(report, "options.repeatOptions.bracketEndAnchorThinLine").origin == ValueOrigin::LegacyBehavior,
            epoch + " reported an incorrect repeat-option origin");
    };

    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        auto profile = profileFor(epoch == FormatEpoch::DclLegacy ? 12 : 3, 0);
        profile.epoch = epoch;
        ImportReport report(FormatEpoch::UncompressedLegacy);
        expectRecovered(runImport(makeContainer(rows, epoch), profile, report), report,
            epoch == FormatEpoch::DclLegacy ? "The DCL epoch" : "The uncompressed epoch");
    }

    auto sentinelRows = rows;
    sentinelRows[1].words[3] = 0;
    ImportReport sentinelReport(FormatEpoch::UncompressedLegacy);
    expectMapping(runImport(makeContainer(sentinelRows), profileFor(3, 7), sentinelReport)->maxPasses == 20,
        "The older zero maximum-pass sentinel did not retain its twenty-pass behavior");

    std::vector<SyntheticClassRow> classRows;
    for (const auto& row : rows) {
        const auto selector = static_cast<std::uint16_t>((row.tag[0] - '0') * 10 + row.tag[1] - '0');
        classRows.push_back({finale_mus_reader::numericGlobalClass(selector), std::vector<std::int16_t>(row.words.begin(), row.words.end())});
    }
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        auto profile = profileFor(16, 0);
        profile.epoch = FormatEpoch::ZlibLegacy;
        profile.byteOrder = byteOrder;
        ImportReport report(FormatEpoch::UncompressedLegacy);
        expectRecovered(runImport(makeClassContainer(classRows, byteOrder), profile, report), report,
            byteOrder == ByteOrder::BigEndian ? "The big-endian zlib epoch" : "The little-endian zlib epoch");
    }

    auto coda = profileFor(2, 6);
    coda.epoch = FormatEpoch::CodaBanner;
    coda.version.reset();
    ImportReport codaReport(FormatEpoch::UncompressedLegacy);
    const auto codaOptions = runImport(makeContainer(rows, FormatEpoch::CodaBanner), coda, codaReport);
    expectMapping(codaOptions->maxPasses == 90 && codaOptions->bracketHeight == 101 && !codaOptions->addPeriod && codaOptions->thinLineWidth == 224
                      && codaOptions->upperDotVPos == 0 && codaOptions->lowerDotVPos == 0 && codaOptions->bracketLineWidth == 224
                      && !codaOptions->bracketEndAnchorThinLine && codaOptions->showOnStaffListNumber == 9,
        "The Coda epoch did not apply the pre-layout RepeatOptions behavior");
    expectMapping(field(codaReport, "options.repeatOptions.maxPasses").origin == ValueOrigin::Finale27Default
                      && field(codaReport, "options.repeatOptions.addPeriod").origin == ValueOrigin::LegacyBehavior
                      && field(codaReport, "options.repeatOptions.bracketEndAnchorThinLine").origin == ValueOrigin::LegacyBehavior,
        "The Coda epoch reported an incorrect RepeatOptions origin");

    auto earlyUncompressed = profileFor(3, 0);
    earlyUncompressed.epoch = FormatEpoch::UncompressedLegacy;
    auto rowsWithoutLayoutMarker = rows;
    rowsWithoutLayoutMarker.erase(rowsWithoutLayoutMarker.begin() + 5);
    ImportReport earlyReport(FormatEpoch::UncompressedLegacy);
    const auto earlyOptions = runImport(makeContainer(rowsWithoutLayoutMarker, FormatEpoch::UncompressedLegacy), earlyUncompressed, earlyReport);
    expectMapping(earlyOptions->maxPasses == 90 && earlyOptions->bracketHeight == 101 && earlyOptions->thickLineWidth == 91
                      && !earlyOptions->addPeriod && earlyOptions->thinLineWidth == 224 && earlyOptions->upperDotVPos == 0
                      && earlyOptions->lowerDotVPos == 0 && earlyOptions->bracketLineWidth == 224 && !earlyOptions->bracketEndAnchorThinLine,
        "An uncompressed file without the family did not apply the pre-layout behavior");
    expectMapping(field(earlyReport, "options.repeatOptions.maxPasses").origin == ValueOrigin::Finale27Default
                      && field(earlyReport, "options.repeatOptions.thinLineWidth").origin == ValueOrigin::LegacyBehavior,
        "An uncompressed file without the family reported an incorrect RepeatOptions origin");
}

TEST_CASE("Repeat options span the located epochs", "[class]")
{
    testRepeatOptionsAcrossEpochs();
}

} // namespace
} // namespace finale_mus_reader_tests
