// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <algorithm>
#include <map>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

using PageFormatOptionsTestTarget = musx::dom::options::PageFormatOptions;
using PageFormatTestTarget = PageFormatOptionsTestTarget::PageFormat;

musx::dom::DocumentPtr makePageFormatOptionsDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<PageFormatOptionsTestTarget>(document);
    options->adjustPageScope = PageFormatOptionsTestTarget::AdjustPageScope::PageRange;
    options->avoidSystemMarginCollisions = true;
    options->pageFormatScore = std::make_shared<PageFormatTestTarget>();
    options->pageFormatParts = std::make_shared<PageFormatTestTarget>();
    options->pageFormatScore->pageHeight = 901;
    options->pageFormatParts->pageHeight = 902;
    options->pageFormatScore->sysPercent = 100;
    options->pageFormatParts->sysPercent = 100;
    options->pageFormatScore->rawStaffHeight = 903;
    options->pageFormatParts->rawStaffHeight = 904;
    document->getOptions()->add(PageFormatOptionsTestTarget::XmlNodeName, options);
    return std::move(session).finish();
}

std::shared_ptr<const PageFormatOptionsTestTarget> importPageFormatOptions(const finale_mus_reader::container::ParsedContainer& parsed,
    FormatEpoch epoch, ImportReport& report, std::optional<finale_mus_reader::VersionBound> version = std::nullopt)
{
    const auto document = makePageFormatOptionsDocument();
    const auto reference = makePageFormatOptionsDocument();
    auto profile = version ? profileFor(version->major, version->minor, version->maint) : profileFor(epoch == FormatEpoch::ZlibLegacy ? 12 : 9);
    profile.epoch = epoch;
    profile.byteOrder = parsed.byteOrder;
    if (epoch == FormatEpoch::CodaBanner) {
        profile.version.reset();
    }
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{
        LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::options::importPageFormatOptions(context);
    return document->getOptions()->get<PageFormatOptionsTestTarget>();
}

std::vector<SyntheticRow> pageFormatFixedRows(ByteOrder byteOrder = ByteOrder::BigEndian)
{
    std::vector<SyntheticRow> result{
        {GLOBALS_CMPER, "01", {0, 0, 0, 0, 18, 19}},
        {GLOBALS_CMPER, "02", {31, 32, 33, 34, 1, 35}},
        {GLOBALS_CMPER, "03", {36, 37, 0, 0, 0, 0}},
        {GLOBALS_CMPER, "13", {0, 0, 0, 0, 1, 1}},
        {GLOBALS_CMPER, "14", {0, 3001, 0, 0, 0, 0}},
        {GLOBALS_CMPER, "15", {0, 0, 0, 2001, 0, 0}},
        {GLOBALS_CMPER, "16", {21, 22, 23, 24, 0, 0}},
        {GLOBALS_CMPER, "17", {11, 12, 13, 14, 0, 0}},
        {GLOBALS_CMPER, "39", {0, 0, 91, 0, 0, 0}},
        {GLOBALS_CMPER, "76", {0, 0, 0, 81, 82, 0}},
        {GLOBALS_CMPER, "77", {0, 3101, 0, 2101, 92, 41}},
        {GLOBALS_CMPER, "77", {42, 43, 44, 45, 46, 47}},
        {GLOBALS_CMPER, "77", {48, 51, 52, 53, 54, 55}},
        {GLOBALS_CMPER, "77", {1, 1, 1, 56, 57, 58}},
        {GLOBALS_CMPER, "77", {59, 0, 0, 0, 0, 0}},
        {GLOBALS_CMPER, "93", {0, 0, 1600, 0, 1700, 0}},
    };
    if (byteOrder == ByteOrder::LittleEndian) {
        auto& dimensions = *std::ranges::find_if(result, [](const SyntheticRow& row) { return std::string_view(row.tag) == "77"; });
        std::swap(dimensions.words[0], dimensions.words[1]);
        std::swap(dimensions.words[2], dimensions.words[3]);
    }
    return result;
}

std::vector<SyntheticClassRow> pageFormatClassRows(ByteOrder byteOrder)
{
    std::map<std::uint16_t, std::vector<std::int16_t>> payloads;
    for (const auto& row : pageFormatFixedRows()) {
        const auto selector = static_cast<std::uint16_t>((row.tag[0] - '0') * 10 + (row.tag[1] - '0'));
        auto& payload = payloads[selector];
        payload.insert(payload.end(), row.words.begin(), row.words.end());
    }
    if (byteOrder == ByteOrder::LittleEndian) {
        auto& parts = payloads[77];
        std::swap(parts[0], parts[1]);
        std::swap(parts[2], parts[3]);
    }
    std::vector<SyntheticClassRow> result;
    for (auto& [selector, payload] : payloads) {
        result.push_back({finale_mus_reader::numericGlobalClass(selector), std::move(payload)});
    }
    result.push_back({0x008d, {64, 0, -32768, 0, 0, 0}, 10});
    return result;
}

void verifyScorePageFormat(const PageFormatTestTarget& format)
{
    expectMapping(format.pageHeight == 3001 && format.pageWidth == 2001 && format.pagePercent == 91 && format.sysPercent == 81
                      && format.rawStaffHeight == 1600 && format.leftPageMarginTop == 21 && format.leftPageMarginLeft == 22
                      && format.leftPageMarginBottom == 23 && format.leftPageMarginRight == 24 && format.rightPageMarginTop == 31
                      && format.rightPageMarginLeft == 32 && format.rightPageMarginBottom == 33 && format.rightPageMarginRight == 34
                      && format.sysMarginTop == 11 && format.sysMarginLeft == 12 && format.sysMarginBottom == 14 && format.sysMarginRight == 13
                      && format.sysDistanceBetween == 18 && format.firstPageMarginTop == 35 && format.firstSysMarginTop == 36
                      && format.firstSysMarginLeft == 37 && format.firstSysMarginDistance == 19 && format.facingPages
                      && format.differentFirstSysMargin && format.differentFirstPageMargin,
        "PageFormatOptions did not recover the score page-format fields");
}

void verifyPartsPageFormat(const PageFormatTestTarget& format)
{
    expectMapping(format.pageHeight == 3101 && format.pageWidth == 2101 && format.pagePercent == 92 && format.sysPercent == 82
                      && format.rawStaffHeight == 1700 && format.leftPageMarginTop == 41 && format.leftPageMarginLeft == 42
                      && format.leftPageMarginBottom == 43 && format.leftPageMarginRight == 44 && format.rightPageMarginTop == 45
                      && format.rightPageMarginLeft == 46 && format.rightPageMarginBottom == 47 && format.rightPageMarginRight == 48
                      && format.sysMarginTop == 51 && format.sysMarginLeft == 52 && format.sysMarginBottom == 53 && format.sysMarginRight == 54
                      && format.sysDistanceBetween == 55 && format.firstPageMarginTop == 56 && format.firstSysMarginTop == 57
                      && format.firstSysMarginLeft == 58 && format.firstSysMarginDistance == 59 && format.facingPages
                      && format.differentFirstSysMargin && format.differentFirstPageMargin,
        "PageFormatOptions did not recover the parts page-format fields");
}

void verifyRecoveredPageFormats(const PageFormatOptionsTestTarget& options, const ImportReport& report)
{
    verifyScorePageFormat(*options.pageFormatScore);
    verifyPartsPageFormat(*options.pageFormatParts);
    for (const auto* member :
        {"pageFormatScore.pageHeight", "pageFormatScore.sysPercent", "pageFormatParts.pageHeight", "pageFormatParts.firstSysMarginDistance"}) {
        expectMapping(field(report, std::string("options.pageFormatOptions.") + member).origin == ValueOrigin::LegacyMus,
            "PageFormatOptions reported an incorrect recovered-field origin");
    }
    expectMapping(options.avoidSystemMarginCollisions
                      && field(report, "options.pageFormatOptions.adjustPageScope").origin == ValueOrigin::Finale27Default
                      && field(report, "options.pageFormatOptions.avoidSystemMarginCollisions").origin == ValueOrigin::LegacyMus,
        "PageFormatOptions reported an incorrect outer-field origin");
}

TEST_CASE("Page format options recover score and parts fixed rows", "[class]")
{
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        auto rows = pageFormatFixedRows(byteOrder);
        rows.push_back({10, "FI", {64, 0, -32768, 0, 0, 0}});
        rows.push_back({0, "IU",
            byteOrder == ByteOrder::BigEndian ? std::array<std::int16_t, 6>{1, 0, 0, 0, -2, -4464}
                                              : std::array<std::int16_t, 6>{1, 0, 0, 0, -4464, -2}});
        ImportReport report(FormatEpoch::DclLegacy);
        verifyRecoveredPageFormats(
            *importPageFormatOptions(makeContainer(rows, FormatEpoch::DclLegacy, byteOrder), FormatEpoch::DclLegacy, report), report);
    }
}

TEST_CASE("Page format options recover score and parts class records in either byte order", "[class]")
{
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        const auto rows = pageFormatClassRows(byteOrder);
        ImportReport report(FormatEpoch::ZlibLegacy);
        verifyRecoveredPageFormats(*importPageFormatOptions(makeClassContainer(rows, byteOrder), FormatEpoch::ZlibLegacy, report), report);
    }
}

TEST_CASE("Finale 1 page format populates both modern destinations", "[class]")
{
    const auto fixture = readFixture("evidence/F100/F100-pageformat.mus");
    const auto options = fixture.document->getOptions()->get<PageFormatOptionsTestTarget>();
    for (const auto& format : {options->pageFormatScore, options->pageFormatParts}) {
        expectMapping(format->pageHeight == 3167 && format->pageWidth == 2449 && format->pagePercent == 91 && format->sysPercent == 100
                          && format->rawStaffHeight == 1536 && format->leftPageMarginTop == -141 && format->leftPageMarginLeft == 142
                          && format->leftPageMarginBottom == 143 && format->leftPageMarginRight == -145 && format->rightPageMarginTop == -141
                          && format->rightPageMarginLeft == 142 && format->rightPageMarginBottom == 143 && format->rightPageMarginRight == -145
                          && format->sysMarginTop == -80 && format->sysMarginLeft == 9 && format->sysMarginBottom == -201
                          && format->sysMarginRight == -11 && format->sysDistanceBetween == 7 && format->firstPageMarginTop == -141
                          && format->firstSysMarginTop == -73 && format->firstSysMarginLeft == 9 && format->firstSysMarginDistance == 0
                          && !format->facingPages && format->differentFirstSysMargin && !format->differentFirstPageMargin,
            "The Finale 1 page-format fixture did not recover its exact edited values");
    }
    expectMapping(field(fixture, "options.pageFormatOptions.pageFormatScore.rawStaffHeight").origin == ValueOrigin::LegacyBehavior
                      && field(fixture, "options.pageFormatOptions.pageFormatParts.firstSysMarginTop").origin == ValueOrigin::LegacyMusAdjusted
                      && field(fixture, "options.pageFormatOptions.pageFormatScore.sysPercent").origin == ValueOrigin::Finale27Default,
        "The Finale 1 transformed and absent page-format fields reported incorrect origins");
    expectMapping(!options->avoidSystemMarginCollisions
                      && field(fixture, "options.pageFormatOptions.avoidSystemMarginCollisions").origin == ValueOrigin::LegacyBehavior,
        "The Finale 1 collision behavior was not applied");
}

TEST_CASE("System scaling and absolute page-format staff height begin with Finale 2002", "[class]")
{
    const auto rows = pageFormatFixedRows();
    ImportReport finale2001Report(FormatEpoch::DclLegacy);
    const auto finale2001 = importPageFormatOptions(
        makeContainer(rows, FormatEpoch::DclLegacy), FormatEpoch::DclLegacy, finale2001Report, finale_mus_reader::versions::finale2001);
    expectMapping(finale2001->pageFormatScore->sysPercent == 100 && finale2001->pageFormatParts->sysPercent == 100
                      && finale2001->pageFormatScore->rawStaffHeight == 1536 && finale2001->pageFormatParts->rawStaffHeight == 1536
                      && field(finale2001Report, "options.pageFormatOptions.pageFormatScore.sysPercent").origin == ValueOrigin::Finale27Default
                      && field(finale2001Report, "options.pageFormatOptions.pageFormatParts.sysPercent").origin == ValueOrigin::Finale27Default
                      && field(finale2001Report, "options.pageFormatOptions.pageFormatScore.rawStaffHeight").origin == ValueOrigin::LegacyBehavior
                      && field(finale2001Report, "options.pageFormatOptions.pageFormatParts.rawStaffHeight").origin == ValueOrigin::LegacyBehavior,
        "Finale 2001 incorrectly supplied a later Page Format preference");

    ImportReport finale2002Report(FormatEpoch::DclLegacy);
    const auto finale2002 = importPageFormatOptions(
        makeContainer(rows, FormatEpoch::DclLegacy), FormatEpoch::DclLegacy, finale2002Report, finale_mus_reader::versions::finale2002);
    expectMapping(finale2002->pageFormatScore->sysPercent == 81 && finale2002->pageFormatParts->sysPercent == 82
                      && finale2002->pageFormatScore->rawStaffHeight == 1600 && finale2002->pageFormatParts->rawStaffHeight == 1700
                      && field(finale2002Report, "options.pageFormatOptions.pageFormatScore.sysPercent").origin == ValueOrigin::LegacyMus
                      && field(finale2002Report, "options.pageFormatOptions.pageFormatParts.sysPercent").origin == ValueOrigin::LegacyMus
                      && field(finale2002Report, "options.pageFormatOptions.pageFormatScore.rawStaffHeight").origin == ValueOrigin::LegacyMus
                      && field(finale2002Report, "options.pageFormatOptions.pageFormatParts.rawStaffHeight").origin == ValueOrigin::LegacyMus,
        "Finale 2002 did not recover its Page Format preferences");
}

TEST_CASE("Finale 2001 retains baseline system scaling", "[class]")
{
    const auto fixture = readFixture("evidence/F2001/F2001Win-empty.mus");
    const auto options = fixture.document->getOptions()->get<PageFormatOptionsTestTarget>();
    expectMapping(options->pageFormatScore->sysPercent == 100 && options->pageFormatParts->sysPercent == 100
                      && field(fixture, "options.pageFormatOptions.pageFormatScore.sysPercent").origin == ValueOrigin::Finale27Default
                      && field(fixture, "options.pageFormatOptions.pageFormatParts.sysPercent").origin == ValueOrigin::Finale27Default,
        "Finale 2001 selector placeholders disturbed baseline system scaling");
}

TEST_CASE("An uncompressed document without selector 77 shares its score page format", "[class]")
{
    auto rows = pageFormatFixedRows();
    std::erase_if(rows, [](const SyntheticRow& row) { return std::string_view(row.tag) == "77"; });
    ImportReport report(FormatEpoch::UncompressedLegacy);
    const auto options = importPageFormatOptions(makeContainer(rows, FormatEpoch::UncompressedLegacy), FormatEpoch::UncompressedLegacy, report);
    const auto& score = *options->pageFormatScore;
    const auto& parts = *options->pageFormatParts;
    expectMapping(parts.pageHeight == score.pageHeight && parts.pageWidth == score.pageWidth && parts.leftPageMarginLeft == score.leftPageMarginLeft
                      && parts.rightPageMarginRight == score.rightPageMarginRight && parts.firstSysMarginTop == score.firstSysMarginTop
                      && parts.facingPages == score.facingPages,
        "The single-set uncompressed page format was not shared with parts");
    expectMapping(field(report, "options.pageFormatOptions.pageFormatParts.pageHeight").origin == ValueOrigin::LegacyBehavior
                      && field(report, "options.pageFormatOptions.pageFormatParts.facingPages").origin == ValueOrigin::LegacyBehavior,
        "The shared parts page format reported incorrect origins");
}

TEST_CASE("Pre-Finale 3.5 page format derives later fields before sharing with parts", "[class]")
{
    auto rows = pageFormatFixedRows();
    std::erase_if(rows, [](const SyntheticRow& row) { return std::string_view(row.tag) == "77"; });
    rows.push_back({0, "IU", {1, 1, -188, 2, 1, -452}});
    const auto parsed = makeContainer(rows, FormatEpoch::UncompressedLegacy);
    const auto storedSystemTop = LegacyRecordIndex::build(parsed).word(finale_mus_reader::records::packTag("IU"), 0, 2);
    expectMapping(storedSystemTop && storedSystemTop->value == -188, "The synthetic pre-Finale 3.5 current-system row was not indexed");
    ImportReport report(FormatEpoch::UncompressedLegacy);
    const auto options = importPageFormatOptions(parsed, FormatEpoch::UncompressedLegacy, report, finale_mus_reader::versions::finale3_2);
    const auto& score = *options->pageFormatScore;
    const auto& parts = *options->pageFormatParts;
    expectMapping(score.sysMarginTop == -188 && score.firstSysMarginTop == -177 && score.rightPageMarginTop == score.leftPageMarginTop
                      && score.rightPageMarginLeft == score.leftPageMarginLeft && score.rightPageMarginBottom == score.leftPageMarginBottom
                      && score.rightPageMarginRight == score.leftPageMarginRight && score.firstSysMarginLeft == score.sysMarginLeft
                      && parts.pageHeight == score.pageHeight && parts.rightPageMarginTop == score.rightPageMarginTop
                      && parts.rightPageMarginLeft == score.rightPageMarginLeft && parts.rightPageMarginBottom == score.rightPageMarginBottom
                      && parts.rightPageMarginRight == score.rightPageMarginRight && parts.firstSysMarginLeft == score.firstSysMarginLeft,
        "The pre-Finale 3.5 page format did not derive and share its single setting set");
    for (const auto* member : {"pageFormatScore.rightPageMarginTop", "pageFormatScore.rightPageMarginLeft", "pageFormatScore.rightPageMarginBottom",
             "pageFormatScore.rightPageMarginRight", "pageFormatScore.firstSysMarginLeft", "pageFormatParts.rightPageMarginTop",
             "pageFormatParts.firstSysMarginLeft"}) {
        expectMapping(field(report, std::string("options.pageFormatOptions.") + member).origin == ValueOrigin::LegacyBehavior,
            "A derived pre-Finale 3.5 page-format field reported an incorrect origin");
    }
}

TEST_CASE("Finale 3.5 page format uses the expanded current-system layout", "[class]")
{
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        for (const auto* tag : {"IU", "Iu"}) {
            auto rows = pageFormatFixedRows(byteOrder);
            std::erase_if(rows, [](const SyntheticRow& row) { return std::string_view(row.tag) == "77"; });
            rows.push_back({GLOBALS_CMPER, "75", {0, 0, 0, 0, 0, 0}});
            const std::array<std::int16_t, 6> currentSystem = byteOrder == ByteOrder::BigEndian ? std::array<std::int16_t, 6>{1, 0, 0, 0, -2, -4464}
                                                                                                : std::array<std::int16_t, 6>{1, 0, 0, 0, -4464, -2};
            rows.push_back({0, tag, currentSystem});
            const auto parsed = makeContainer(rows, FormatEpoch::UncompressedLegacy, byteOrder);
            ImportReport report(FormatEpoch::UncompressedLegacy);
            const auto options = importPageFormatOptions(parsed, FormatEpoch::UncompressedLegacy, report, finale_mus_reader::versions::finale3_5);
            const auto& score = *options->pageFormatScore;
            expectMapping(
                score.sysMarginTop == -70000 && score.firstSysMarginTop == -69953 && score.rightPageMarginTop == 31 && score.firstSysMarginLeft == 49,
                "The expanded page-format layout did not recover its signed system-top long");
        }
    }
}

TEST_CASE("Pre-Finale 3.5 collision avoidance is a scalar flag", "[class]")
{
    for (const auto& [stored, expected] : {std::pair<std::int16_t, bool>{std::int16_t(0), false}, {std::int16_t(1), true}}) {
        const auto parsed = makeContainer({{10, "FI", {64, 0, stored, 0, 0, 4}}}, FormatEpoch::UncompressedLegacy);
        ImportReport report(FormatEpoch::UncompressedLegacy);
        const auto options = importPageFormatOptions(parsed, FormatEpoch::UncompressedLegacy, report, finale_mus_reader::versions::finale3_2);
        expectMapping(options->avoidSystemMarginCollisions == expected
                          && field(report, "options.pageFormatOptions.avoidSystemMarginCollisions").origin == ValueOrigin::LegacyMus,
            "The pre-Finale 3.5 scalar collision-avoidance flag was not recovered");
    }
}

TEST_CASE("Uncompressed parts first-page top follows the recovered page top", "[class]")
{
    const auto rows = pageFormatFixedRows();
    ImportReport report(FormatEpoch::UncompressedLegacy);
    const auto options = importPageFormatOptions(
        makeContainer(rows, FormatEpoch::UncompressedLegacy), FormatEpoch::UncompressedLegacy, report, finale_mus_reader::versions::finale97);
    const auto& parts = *options->pageFormatParts;
    expectMapping(parts.leftPageMarginTop == 41 && parts.rightPageMarginTop == 45 && parts.firstPageMarginTop == parts.leftPageMarginTop
                      && field(report, "options.pageFormatOptions.pageFormatParts.firstPageMarginTop").origin == ValueOrigin::LegacyBehavior,
        "The uncompressed parts first-page top did not follow its recovered page top");
}

TEST_CASE("Finale 3.7 collision avoidance is recovered from its stored flag", "[class]")
{
    const auto baseline = readFixture("evidence/F372/F372-baseline.mus");
    const auto edited = readFixture("evidence/F372/F372-noavoid-margincoll.mus");
    const auto baselineOptions = baseline.document->getOptions()->get<PageFormatOptionsTestTarget>();
    const auto editedOptions = edited.document->getOptions()->get<PageFormatOptionsTestTarget>();
    expectMapping(baselineOptions->avoidSystemMarginCollisions && !editedOptions->avoidSystemMarginCollisions,
        "The Finale 3.7 collision-avoidance flag was not recovered");
    expectMapping(field(edited, "options.pageFormatOptions.avoidSystemMarginCollisions").origin == ValueOrigin::LegacyMus,
        "The collision-avoidance flag reported an incorrect origin");
}

TEST_CASE("Uncompressed parts facing pages survives through Finale 2000", "[class]")
{
    for (const auto* path : {"evidence/F372/F372-facingpages-parts.mus", "evidence/F2000/F2000-F372-facingpages-parts.mus"}) {
        const auto fixture = readFixture(path);
        const auto options = fixture.document->getOptions()->get<PageFormatOptionsTestTarget>();
        expectMapping(!options->pageFormatScore->facingPages && options->pageFormatParts->facingPages
                          && field(fixture, "options.pageFormatOptions.pageFormatParts.facingPages").origin == ValueOrigin::LegacyMus,
            "The uncompressed parts-facing-pages flag was not recovered");
    }
}

TEST_CASE("Finale 2.6 collision avoidance is recovered from its Coda flag", "[class]")
{
    const auto baseline = readFixture("evidence/F263/F263-baseline.mus");
    const auto edited = readFixture("evidence/F263/F263-noavoid-margincoll.mus");
    const auto baselineOptions = baseline.document->getOptions()->get<PageFormatOptionsTestTarget>();
    const auto editedOptions = edited.document->getOptions()->get<PageFormatOptionsTestTarget>();
    expectMapping(baselineOptions->avoidSystemMarginCollisions && !editedOptions->avoidSystemMarginCollisions,
        "The Finale 2.6 collision-avoidance flag was not recovered");
    expectMapping(field(edited, "options.pageFormatOptions.avoidSystemMarginCollisions").origin == ValueOrigin::LegacyMus,
        "The Coda collision-avoidance flag reported an incorrect origin");
}

void verifyTrackedDefaultPageFormat(const PageFormatTestTarget& format, bool dclOrLater)
{
    expectMapping(format.pageHeight == 3168 && format.pageWidth == 2448 && format.pagePercent == 100 && format.sysPercent == 100
                      && format.rawStaffHeight == 1536 && format.leftPageMarginTop == -144 && format.leftPageMarginLeft == 144
                      && format.leftPageMarginBottom == 144 && format.leftPageMarginRight == -144 && format.rightPageMarginTop == -144
                      && format.rightPageMarginLeft == 144 && format.rightPageMarginBottom == 144 && format.rightPageMarginRight == -144
                      && format.sysMarginTop == -80 && format.sysMarginLeft == 0 && format.sysMarginBottom == -200 && format.sysMarginRight == 0
                      && format.sysDistanceBetween == (dclOrLater ? -80 : 0) && format.firstPageMarginTop == -144
                      && format.firstSysMarginTop == (dclOrLater ? -463 : -80) && format.firstSysMarginLeft == (dclOrLater ? 144 : 0)
                      && format.firstSysMarginDistance == 0 && !format.facingPages && format.differentFirstSysMargin
                      && !format.differentFirstPageMargin,
        "A tracked default fixture did not recover its exact page-format values");
}

TEST_CASE("Tracked later epochs recover their exact page formats", "[class]")
{
    for (const auto& [path, dclOrLater] : {std::pair{"evidence/F2000/F2000-empty.mus", false}, std::pair{"evidence/F2002/F2002-empty.mus", true},
             std::pair{"evidence/F2008/F2008-empty.mus", true}}) {
        const auto fixture = readFixture(path);
        const auto options = fixture.document->getOptions()->get<PageFormatOptionsTestTarget>();
        verifyTrackedDefaultPageFormat(*options->pageFormatScore, dclOrLater);
        verifyTrackedDefaultPageFormat(*options->pageFormatParts, dclOrLater);
        const auto partsOrigin = field(fixture, "options.pageFormatOptions.pageFormatParts.pageHeight").origin;
        const auto staffOrigin = field(fixture, "options.pageFormatOptions.pageFormatScore.rawStaffHeight").origin;
        const auto expectedStaffOrigin = dclOrLater ? ValueOrigin::LegacyMus : ValueOrigin::LegacyBehavior;
        CAPTURE(path, expectedStaffOrigin, partsOrigin, staffOrigin);
        expectMapping(partsOrigin == ValueOrigin::LegacyMus, "A tracked fixture reported an incorrect parts page-height origin");
        expectMapping(staffOrigin == expectedStaffOrigin, "A tracked fixture reported an incorrect score staff-height origin");
    }
}

TEST_CASE("Uncompressed system records select their own page-format layout", "[class]")
{
    const auto finale97 = readFixture("evidence/F97/F97-fileinfo-long.mus");
    const auto finale97Options = finale97.document->getOptions()->get<PageFormatOptionsTestTarget>();
    expectMapping(finale97Options->pageFormatScore->sysMarginTop == -188 && finale97Options->pageFormatScore->sysDistanceBetween == -48
                      && finale97Options->pageFormatScore->firstSysMarginTop == -236 && finale97Options->pageFormatScore->firstSysMarginLeft == 522
                      && finale97Options->pageFormatParts->leftPageMarginLeft == 432 && finale97Options->pageFormatParts->sysMarginTop == -188
                      && finale97Options->pageFormatParts->sysDistanceBetween == -72 && finale97Options->pageFormatParts->firstSysMarginTop == -260
                      && finale97Options->pageFormatParts->firstSysMarginLeft == 288,
        "The six-word uncompressed system layout did not recover its exact values");

    const auto finale98 = readFixture("evidence/F98/F98-beams-inclrestsin4.mus");
    const auto finale98Options = finale98.document->getOptions()->get<PageFormatOptionsTestTarget>();
    expectMapping(finale98Options->pageFormatScore->sysMarginTop == -80 && finale98Options->pageFormatScore->sysDistanceBetween == -118
                      && finale98Options->pageFormatScore->firstSysMarginTop == -630 && finale98Options->pageFormatParts->leftPageMarginLeft == 213
                      && finale98Options->pageFormatParts->sysDistanceBetween == -118 && finale98Options->pageFormatParts->firstSysMarginTop == -630,
        "The offset-bearing uncompressed system layout did not recover its exact values");
}

TEST_CASE("Absent page format records retain the seeded values", "[class]")
{
    ImportReport report(FormatEpoch::DclLegacy);
    const auto options = importPageFormatOptions(makeContainer({}, FormatEpoch::DclLegacy), FormatEpoch::DclLegacy, report);
    expectMapping(options->pageFormatScore->pageHeight == 901 && options->pageFormatParts->pageHeight == 902
                      && options->adjustPageScope == PageFormatOptionsTestTarget::AdjustPageScope::PageRange && options->avoidSystemMarginCollisions,
        "Absent page-format records disturbed the seeded options");
    expectMapping(field(report, "options.pageFormatOptions.pageFormatScore.pageHeight").origin == ValueOrigin::Finale27Default
                      && field(report, "options.pageFormatOptions.adjustPageScope").origin == ValueOrigin::Finale27Default,
        "Absent PageFormatOptions records reported incorrect origins");
}
} // namespace
} // namespace finale_mus_reader_tests
