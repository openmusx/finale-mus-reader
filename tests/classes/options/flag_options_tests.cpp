// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

using FlagOptionsTarget = musx::dom::options::FlagOptions;

musx::dom::DocumentPtr makeFlagOptionsDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<FlagOptionsTarget>(document);
    options->straightFlags = false;
    options->upHAdj = 101;
    options->downHAdj = 102;
    options->upHAdj2 = 103;
    options->downHAdj2 = 104;
    options->upHAdj16 = 105;
    options->downHAdj16 = 106;
    options->eighthFlagHoist = 107;
    options->stUpHAdj = 108;
    options->stDownHAdj = 109;
    options->upVAdj = 110;
    options->downVAdj = 111;
    options->upVAdj2 = 112;
    options->downVAdj2 = 113;
    options->upVAdj16 = 114;
    options->downVAdj16 = 115;
    options->stUpVAdj = 116;
    options->stDownVAdj = 117;
    options->flagSpacing = 118;
    options->secondaryGroupAdj = 119;
    document->getOptions()->add(FlagOptionsTarget::XmlNodeName, options);
    return std::move(session).finish();
}

std::shared_ptr<const FlagOptionsTarget> importFlagOptions(
    const finale_mus_reader::container::ParsedContainer& parsed, SourceProfile profile, ImportReport& report)
{
    const auto document = makeFlagOptionsDocument();
    const auto reference = makeFlagOptionsDocument();
    profile.byteOrder = parsed.byteOrder;
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{
        LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::options::importFlagOptions(context);
    return document->getOptions()->get<FlagOptionsTarget>();
}

std::shared_ptr<const FlagOptionsTarget> importFlagOptions(
    const finale_mus_reader::container::ParsedContainer& parsed, FormatEpoch epoch, ImportReport& report)
{
    auto profile = profileFor(epoch == FormatEpoch::ZlibLegacy ? 12 : 9, 0);
    profile.epoch = epoch;
    if (epoch == FormatEpoch::CodaBanner) {
        profile.version.reset();
    }
    return importFlagOptions(parsed, profile, report);
}

void verifyRecoveredFlagOptions(const FlagOptionsTarget& options, const ImportReport& report)
{
    expectMapping(options.upHAdj == 1 && options.downHAdj == -2 && options.upHAdj2 == 3 && options.downHAdj2 == -4 && options.upHAdj16 == 5
                      && options.downHAdj16 == -6 && options.upVAdj == 7 && options.downVAdj == -8 && options.upVAdj2 == 9 && options.downVAdj2 == -10
                      && options.upVAdj16 == 11 && options.downVAdj16 == -12 && options.stUpHAdj == 13 && options.stDownHAdj == -14
                      && options.stUpVAdj == 15 && options.stDownVAdj == -16 && options.flagSpacing == 17 && options.secondaryGroupAdj == 18,
        "FlagOptions did not recover its located geometry");
    expectMapping(options.straightFlags && options.eighthFlagHoist == 107, "FlagOptions disturbed fields whose legacy locations are not established");
    expectMapping(field(report, "options.flagOptions.straightFlags").origin == ValueOrigin::LegacyMus
                      && field(report, "options.flagOptions.upHAdj").origin == ValueOrigin::LegacyMus
                      && field(report, "options.flagOptions.downVAdj16").origin == ValueOrigin::LegacyMus
                      && field(report, "options.flagOptions.secondaryGroupAdj").origin == ValueOrigin::LegacyMus
                      && field(report, "options.flagOptions.eighthFlagHoist").origin == ValueOrigin::Finale27Default,
        "FlagOptions reported an incorrect field origin");
}

TEST_CASE("Flag options recover the fixed-row layout", "[class]")
{
    const std::vector<SyntheticRow> rows{
        {GLOBALS_CMPER, "05", {0, 0, 1, 0, 0, 0}},
        {GLOBALS_CMPER, "73", {1, -2, 3, -4, 5, -6}},
        {GLOBALS_CMPER, "74", {7, -8, 9, -10, 11, -12}},
        {GLOBALS_CMPER, "75", {90, 91, 13, -14, 15, -16}},
        {GLOBALS_CMPER, "76", {17, 18, 0, 0, 0, 0}},
    };
    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        ImportReport report(epoch);
        verifyRecoveredFlagOptions(*importFlagOptions(makeContainer(rows, epoch), epoch, report), report);
    }
}

TEST_CASE("Flag options recover class records in either byte order", "[class]")
{
    const std::vector<SyntheticClassRow> rows{
        {finale_mus_reader::numericGlobalClass(5), {0, 0, 1}},
        {finale_mus_reader::numericGlobalClass(73), {1, -2, 3, -4, 5, -6}},
        {finale_mus_reader::numericGlobalClass(74), {7, -8, 9, -10, 11, -12}},
        {finale_mus_reader::numericGlobalClass(75), {90, 91, 13, -14, 15, -16}},
        {finale_mus_reader::numericGlobalClass(76), {17, 18}},
    };
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        ImportReport report(FormatEpoch::ZlibLegacy);
        verifyRecoveredFlagOptions(*importFlagOptions(makeClassContainer(rows, byteOrder), FormatEpoch::ZlibLegacy, report), report);
    }
}

TEST_CASE("Coda flag options recover the vertical choice layout", "[class]")
{
    const std::vector<SyntheticRow> rows{
        {GLOBALS_CMPER, "10", {0, 0, 0, 11, 1, 1}},
        {GLOBALS_CMPER, "36", {0, 0, 0, 0, 0, 1}},
    };
    ImportReport report(FormatEpoch::CodaBanner);
    const auto options = importFlagOptions(makeContainer(rows, FormatEpoch::CodaBanner), FormatEpoch::CodaBanner, report);
    expectMapping(!options->straightFlags && options->upHAdj == 101 && options->upHAdj2 == 103 && options->upHAdj16 == 105 && options->downHAdj == 0
                      && options->downHAdj2 == 0 && options->downHAdj16 == 0 && options->upVAdj == 1536 && options->upVAdj2 == 1536
                      && options->upVAdj16 == 1536 && options->downVAdj == -1536 && options->downVAdj2 == -1536 && options->downVAdj16 == -1536
                      && options->stUpHAdj == 0 && options->stDownHAdj == 0 && options->stUpVAdj == 1536 && options->stDownVAdj == -1536
                      && options->flagSpacing == 24 && options->secondaryGroupAdj == 0,
        "Coda FlagOptions did not recover its vertical choices");
    expectMapping(field(report, "options.flagOptions.straightFlags").origin == ValueOrigin::Finale27Default
                      && field(report, "options.flagOptions.upHAdj").origin == ValueOrigin::Finale27Default
                      && field(report, "options.flagOptions.stUpVAdj").origin == ValueOrigin::LegacyBehavior,
        "Coda FlagOptions reported an incorrect field origin");
}

TEST_CASE("The editable flag family structurally gates straight flags", "[class]")
{
    const std::vector<SyntheticRow> rows{
        {GLOBALS_CMPER, "05", {0, 0, 1, 0, 0, 0}},
    };
    auto profile = profileFor(3, 5);
    profile.epoch = FormatEpoch::UncompressedLegacy;
    ImportReport report(FormatEpoch::UncompressedLegacy);
    const auto options = importFlagOptions(makeContainer(rows, FormatEpoch::UncompressedLegacy), profile, report);
    expectMapping(!options->straightFlags && field(report, "options.flagOptions.straightFlags").origin == ValueOrigin::Finale27Default,
        "Selector 05 supplied straight flags without the editable flag family");
}

TEST_CASE("Coda flag origin switches expand independently", "[class]")
{
    struct Case
    {
        std::int16_t up;
        std::int16_t down;
        musx::dom::Efix upExpected;
        musx::dom::Efix downExpected;
        musx::dom::Efix down16Expected;
    };
    for (const auto& item : {Case{0, 0, 5376, -5376, -5655}, Case{1, 0, 1536, -5376, -5655}, Case{0, 1, 5376, -1536, -1536}}) {
        const std::vector<SyntheticRow> rows{
            {GLOBALS_CMPER, "10", {0, 0, 0, 7, item.up, item.down}},
            {GLOBALS_CMPER, "36", {0, 0, 0, 0, 0, 0}},
        };
        ImportReport report(FormatEpoch::CodaBanner);
        const auto options = importFlagOptions(makeContainer(rows, FormatEpoch::CodaBanner), FormatEpoch::CodaBanner, report);
        expectMapping(options->upHAdj == 101 && options->upVAdj == item.upExpected && options->upVAdj16 == item.upExpected
                          && options->downVAdj == item.downExpected && options->downVAdj16 == item.down16Expected,
            "Coda FlagOptions coupled independent origin choices");
    }
}

TEST_CASE("Controlled Coda flag fixtures recover vertical source choices", "[class][reader]")
{
    const auto all = readFixture("evidence/F100/F100-flag-options.mus");
    const auto allOptions = all.document->getOptions()->get<FlagOptionsTarget>();
    expect(allOptions && !allOptions->straightFlags && allOptions->upHAdj == 0 && allOptions->upHAdj2 == 0 && allOptions->upHAdj16 == 0,
        "The Finale 1.0 flag fixture did not retain its horizontal defaults");

    const auto offset = readFixture("evidence/F100/F100-flagoff-neg3.mus");
    const auto offsetOptions = offset.document->getOptions()->get<FlagOptionsTarget>();
    expect(offsetOptions && offsetOptions->upHAdj == 0 && offsetOptions->upHAdj2 == 0 && offsetOptions->upHAdj16 == 0
               && field(offset.report, "options.flagOptions.upHAdj").origin == ValueOrigin::Finale27Default,
        "The Finale 1.0 flag-offset fixture did not retain its horizontal defaults");

    const auto offset263 = readFixture("evidence/F263/F263-flagoff-neg3.mus");
    const auto offset263Options = offset263.document->getOptions()->get<FlagOptionsTarget>();
    expect(offset263Options && offset263Options->upHAdj == 0 && offset263Options->upHAdj2 == 0 && offset263Options->upHAdj16 == 0
               && field(offset263.report, "options.flagOptions.upHAdj").origin == ValueOrigin::Finale27Default,
        "The Finale 2.6 flag-offset fixture did not retain its horizontal defaults");

    const auto up = readFixture("evidence/F263/F263-flagup-origy.mus");
    const auto upOptions = up.document->getOptions()->get<FlagOptionsTarget>();
    expect(upOptions && upOptions->upVAdj == 1536 && upOptions->upVAdj16 == 1536 && upOptions->downVAdj == -5376 && upOptions->downVAdj16 == -5655,
        "The Finale 2.6 up-origin fixture did not recover independently");

    const auto down = readFixture("evidence/F263/F263-flagdn-origy.mus");
    const auto downOptions = down.document->getOptions()->get<FlagOptionsTarget>();
    expect(downOptions && downOptions->upVAdj == 5376 && downOptions->upVAdj16 == 5376 && downOptions->downVAdj == -1536
               && downOptions->downVAdj16 == -1536,
        "The Finale 2.6 down-origin fixture did not recover independently");
}

TEST_CASE("Finale 2012 flag options recover the class-record geometry", "[class][reader]")
{
    const auto result = readFixture("evidence/F2012/F2012-upstem-flags.mus");
    const auto options = result.document->getOptions()->get<FlagOptionsTarget>();
    expect(options && options->upHAdj == 0 && options->downHAdj == 0 && options->upHAdj2 == 0 && options->downHAdj2 == 0 && options->upHAdj16 == 0
               && options->downHAdj16 == 0 && options->upVAdj == 1536 && options->downVAdj == -1536 && options->upVAdj2 == 1920
               && options->downVAdj2 == -2176 && options->upVAdj16 == 1216 && options->downVAdj16 == -1280 && options->stUpVAdj == 1536
               && options->stDownVAdj == 768 && options->flagSpacing == 20 && options->secondaryGroupAdj == 6,
        "The Finale 2012 fixture did not recover FlagOptions geometry");
    expect(field(result, "options.flagOptions.upVAdj").origin == ValueOrigin::LegacyMus
               && field(result, "options.flagOptions.stDownVAdj").origin == ValueOrigin::LegacyMus
               && field(result, "options.flagOptions.flagSpacing").origin == ValueOrigin::LegacyMus,
        "The Finale 2012 FlagOptions fixture reported incorrect origins");
}

TEST_CASE("Finale 2012 flag options recover every edited class-record value", "[class][reader]")
{
    const auto result = readFixture("evidence/F2012/F2012-flagopts.mus");
    const auto options = result.document->getOptions()->get<FlagOptionsTarget>();
    expect(options && options->straightFlags && options->upHAdj == 64 && options->downHAdj == -64 && options->upHAdj2 == 192
               && options->downHAdj2 == -192 && options->upHAdj16 == 448 && options->downHAdj16 == -448 && options->upVAdj == 128
               && options->downVAdj == -128 && options->upVAdj2 == 320 && options->downVAdj2 == -320 && options->upVAdj16 == 576
               && options->downVAdj16 == -576 && options->stUpHAdj == 704 && options->stDownHAdj == -704 && options->stUpVAdj == 832
               && options->stDownVAdj == -832 && options->flagSpacing == 19 && options->secondaryGroupAdj == 23,
        "The Finale 2012 FlagOptions fixture did not recover all edited values");
    expect(field(result, "options.flagOptions.straightFlags").origin == ValueOrigin::LegacyMus
               && field(result, "options.flagOptions.stUpHAdj").origin == ValueOrigin::LegacyMus
               && field(result, "options.flagOptions.secondaryGroupAdj").origin == ValueOrigin::LegacyMus,
        "The complete Finale 2012 FlagOptions fixture reported incorrect origins");
}

TEST_CASE("Finale 2003 flag options recover the DCL fixed-row layout", "[class][reader]")
{
    const auto result = readFixture("evidence/F2003/F2003-flagopts.mus");
    const auto options = result.document->getOptions()->get<FlagOptionsTarget>();
    expect(options && options->straightFlags && options->upHAdj == 64 && options->downHAdj == -64 && options->upHAdj2 == 192
               && options->downHAdj2 == -192 && options->upHAdj16 == 448 && options->downHAdj16 == -448 && options->upVAdj == 128
               && options->downVAdj == -128 && options->upVAdj2 == 320 && options->downVAdj2 == -320 && options->upVAdj16 == 576
               && options->downVAdj16 == -576 && options->stUpHAdj == 704 && options->stDownHAdj == -704 && options->stUpVAdj == 832
               && options->stDownVAdj == -832 && options->flagSpacing == 17 && options->secondaryGroupAdj == 19,
        "The Finale 2003 FlagOptions fixture did not recover all edited values");
    expect(field(result, "options.flagOptions.straightFlags").origin == ValueOrigin::LegacyMus
               && field(result, "options.flagOptions.secondaryGroupAdj").origin == ValueOrigin::LegacyMus,
        "The Finale 2003 FlagOptions fixture reported incorrect origins");
}

TEST_CASE("Finale 3.7 flag options recover straight-flag geometry", "[class][reader]")
{
    const auto result = readFixture("evidence/F372/F372-flagopts.mus");
    const auto options = result.document->getOptions()->get<FlagOptionsTarget>();
    expect(options && options->straightFlags && options->upHAdj == 64 && options->downHAdj == -64 && options->upHAdj2 == 192
               && options->downHAdj2 == -192 && options->upHAdj16 == 448 && options->downHAdj16 == -448 && options->upVAdj == 128
               && options->downVAdj == -128 && options->upVAdj2 == 320 && options->downVAdj2 == -320 && options->upVAdj16 == 576
               && options->downVAdj16 == -576 && options->stUpHAdj == 704 && options->stDownHAdj == -704 && options->stUpVAdj == 832
               && options->stDownVAdj == -832 && options->flagSpacing == 19 && options->secondaryGroupAdj == 23,
        "The Finale 3.7 fixture did not recover all edited flag options");
    expect(field(result, "options.flagOptions.straightFlags").origin == ValueOrigin::LegacyMus
               && field(result, "options.flagOptions.stUpHAdj").origin == ValueOrigin::LegacyMus
               && field(result, "options.flagOptions.stDownHAdj").origin == ValueOrigin::LegacyMus,
        "The Finale 3.7 FlagOptions fixture reported incorrect origins");
}

} // namespace
} // namespace finale_mus_reader_tests
