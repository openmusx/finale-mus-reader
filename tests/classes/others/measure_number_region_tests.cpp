// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <vector>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using Region = musx::dom::others::MeasureNumberRegion;

TEST_CASE("Measure number regions import the Coda split rows", "[class][measure-number-region]")
{
    const auto result = readFixture("evidence/F263/F263-baseline.mus");
    const auto region = result.document->getOthers()->get<Region>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(region);
    CHECK(region->startMeas == 2);
    CHECK(region->endMeas == 999);
    CHECK(region->numberOffset == 1);
    CHECK(region->scoreData->startFont->fontId == 4);
    CHECK(region->scoreData->startFont->fontSize == 14);
    CHECK(region->scoreData->startFont->italic);
    CHECK(region->partData->mmRestYdisp == 40);
    CHECK_FALSE(region->scoreData->startEnclosure);
    CHECK_FALSE(region->scoreData->useStartEncl);
    CHECK_FALSE(region->scoreData->useMultipleEncl);
    CHECK(region->region == 1);
    const auto& fields = result.report.fields.at(finale_mus_reader::instanceKey<Region>(0, musx::dom::Cmper(1)));
    CHECK(fields.size() == 168);
    CHECK(fields.at("scoreData.startEnclosure.xAdd").origin == ValueOrigin::Unmapped);
    CHECK(fields.at("scoreData.useStartEncl").origin == ValueOrigin::LegacyMus);
    CHECK(fields.at("scoreData.startFont.fontId").origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Measure number regions keep Coda placement and font variants", "[class][measure-number-region]")
{
    const auto result = readFixture("evidence/F100/F100-measnums.mus");
    const auto region = result.document->getOthers()->get<Region>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(region);
    CHECK(region->startMeas == 1);
    CHECK(region->endMeas == 987);
    CHECK(region->startChar == U'B');
    CHECK(region->scoreData->startFont->fontId == 1);
    CHECK(region->scoreData->startFont->fontSize == 12);
    CHECK(region->scoreData->startXdisp == 72);
    CHECK(region->scoreData->startYdisp == -144);
    REQUIRE(region->scoreData->startEnclosure);
    REQUIRE(region->partData->multipleEnclosure);
    CHECK(region->scoreData->startEnclosure->shape == musx::dom::others::Enclosure::Shape::Rectangle);
    CHECK(region->partData->multipleEnclosure->xMargin == 9);
    CHECK(region->scoreData->startEnclosure->yMargin == 9);
    CHECK(region->scoreData->startEnclosure->lineWidth == 224);
    CHECK(region->scoreData->startEnclosure->notTall);
    CHECK_FALSE(region->scoreData->useStartEncl);
    CHECK_FALSE(region->partData->useMultipleEncl);
}

TEST_CASE("Coda measure number prefixes use the last style word", "[class][measure-number-region]")
{
    const auto result = readFixture("evidence/F100/F100-measnums-prefix.mus");
    const auto region = result.document->getOthers()->get<Region>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(region);
    CHECK(region->prefix == "12");
    CHECK(region->suffix.empty());
    CHECK(result.report.findField<Region>("prefix", 0, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Coda measure number enclosure shapes match modern values", "[class][measure-number-region]")
{
    using Shape = musx::dom::others::Enclosure::Shape;
    for (const auto& [name, shape] : std::vector<std::pair<const char*, Shape>>{{"ellipse", Shape::Ellipse}, {"triangle", Shape::Triangle},
             {"diamond", Shape::Diamond}, {"pentagon", Shape::Pentagon}, {"hexagon", Shape::Hexagon}, {"septagon", Shape::Heptagon}}) {
        const auto result = readFixture(std::string("evidence/F100/F100-measnums-") + name + ".mus");
        const auto region = result.document->getOthers()->get<Region>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(region);
        REQUIRE(region->scoreData->startEnclosure);
        REQUIRE(region->scoreData->multipleEnclosure);
        REQUIRE(region->partData->startEnclosure);
        REQUIRE(region->partData->multipleEnclosure);
        CHECK(region->scoreData->startEnclosure->shape == shape);
        CHECK(region->scoreData->multipleEnclosure->shape == shape);
        CHECK(region->partData->startEnclosure->shape == shape);
        CHECK(region->partData->multipleEnclosure->shape == shape);
        CHECK_FALSE(region->scoreData->useStartEncl);
    }
}

TEST_CASE("Coda enclosure selection enables the stored shape", "[class][measure-number-region]")
{
    const auto result = readFixture("evidence/F263/F263-F100-measnums-enclevry.mus");
    const auto region = result.document->getOthers()->get<Region>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(region);
    REQUIRE(region->scoreData->startEnclosure);
    REQUIRE(region->scoreData->multipleEnclosure);
    REQUIRE(region->partData->startEnclosure);
    REQUIRE(region->partData->multipleEnclosure);
    CHECK(region->scoreData->startEnclosure->shape == musx::dom::others::Enclosure::Shape::Rectangle);
    CHECK(region->scoreData->multipleEnclosure->lineWidth == 224);
    CHECK(region->partData->startEnclosure->notTall);
    CHECK(region->scoreData->useStartEncl);
    CHECK(region->scoreData->useMultipleEncl);
    CHECK(region->partData->useStartEncl);
    CHECK(region->partData->useMultipleEncl);
}

TEST_CASE("Old full regions duplicate enclosures into score and part settings", "[class][measure-number-region]")
{
    const auto result = readFixture("evidence/F372/F372-F263-F100-msnm-enclevr.mus");
    const auto region = result.document->getOthers()->get<Region>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(region);
    REQUIRE(region->scoreData);
    REQUIRE(region->partData);
    REQUIRE(region->scoreData->startEnclosure);
    REQUIRE(region->scoreData->multipleEnclosure);
    REQUIRE(region->partData->startEnclosure);
    REQUIRE(region->partData->multipleEnclosure);
    CHECK(region->scoreData->startEnclosure->xMargin == 18);
    CHECK(region->scoreData->startEnclosure->yMargin == 18);
    CHECK(region->scoreData->startEnclosure->lineWidth == 118);
    CHECK(region->scoreData->multipleEnclosure->xMargin == 18);
    CHECK(region->partData->startEnclosure->lineWidth == 118);
    CHECK(region->partData->multipleEnclosure->yMargin == 18);
    CHECK(region->scoreData->useStartEncl);
    CHECK(region->scoreData->useMultipleEncl);
    CHECK(region->partData->useStartEncl);
    CHECK(region->partData->useMultipleEncl);
}

TEST_CASE("Finale 2008 class regions retain the old full layout", "[class][measure-number-region]")
{
    const auto result = readFixture("evidence/F2008/F2008-F372-F263-F100-msnm-enclevr.mus");
    const auto region = result.document->getOthers()->get<Region>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(region);
    CHECK(region->startMeas == 1);
    CHECK(region->endMeas == 987);
    CHECK(region->startChar == U'B');
    CHECK(region->scoreData->startFont->fontId == 1);
    CHECK(region->scoreData->startFont->fontSize == 12);
    CHECK(region->partData->mmRestXdisp == 72);
    CHECK(region->partData->mmRestYdisp == -144);
    REQUIRE(region->scoreData->startEnclosure);
    REQUIRE(region->scoreData->multipleEnclosure);
    REQUIRE(region->partData->startEnclosure);
    REQUIRE(region->partData->multipleEnclosure);
    CHECK(region->scoreData->startEnclosure->xMargin == 18);
    CHECK(region->scoreData->multipleEnclosure->yMargin == 18);
    CHECK(region->partData->startEnclosure->lineWidth == 118);
    CHECK(region->partData->multipleEnclosure->shape == musx::dom::others::Enclosure::Shape::Rectangle);
    CHECK(region->scoreData->useStartEncl);
    CHECK(region->scoreData->useMultipleEncl);
    CHECK(region->partData->useStartEncl);
    CHECK(region->partData->useMultipleEncl);
}

TEST_CASE("Finale 2000 measure ranges use the old flag and bracket slots", "[class][measure-number-region]")
{
    for (const auto& [name, enabled] : std::vector<std::pair<const char*, bool>>{{"F2000-measnums", false}, {"F2000-measnums-mmrest", true}}) {
        const auto result = readFixture(std::string("evidence/F2000/") + name + ".mus");
        const auto region = result.document->getOthers()->get<Region>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(region);
        REQUIRE(region->scoreData);
        REQUIRE(region->partData);
        for (const auto& data : {region->scoreData, region->partData}) {
            CHECK(data->showMmRange == enabled);
            CHECK_FALSE(data->showOnMmRest);
            CHECK(data->leftMmBracketChar == (enabled ? U'[' : 0));
            CHECK(data->rightMmBracketChar == (enabled ? U']' : 0));
        }
        CHECK(result.report.findField<Region>("scoreData.showMmRange", 0, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
        CHECK(result.report.findField<Region>("partData.leftMmBracketChar", 0, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
    }
}

TEST_CASE("Measure number regions accept the old full struct in all later containers", "[class][measure-number-region]")
{
    std::vector<std::int16_t> words(48);
    words[0] = 4;
    words[1] = 14;
    words[2] = 2;
    words[3] = 2;
    words[4] = 999;
    words[5] = 1;
    words[6] = 48;
    words[7] = 10;
    words[8] = 1;
    words[10] = 40;
    words[43] = 1;
    words[44] = 0x408a;
    words[45] = static_cast<std::int16_t>(0xa0cdU);
    for (const auto bits : {1U, 2U, 3U}) {
        const auto alignment = bits == 3U ? musx::dom::AlignJustify::Left : static_cast<musx::dom::AlignJustify>(bits);
        words[41] = static_cast<std::int16_t>(0x8c88U | bits);
        for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            std::vector<SyntheticRow> rows;
            for (std::size_t index = 0; index < words.size(); index += 6) {
                rows.push_back({1, "MN", {words[index], words[index + 1], words[index + 2], words[index + 3], words[index + 4], words[index + 5]}});
            }
            for (const auto& parsed : {makeContainer(rows, FormatEpoch::UncompressedLegacy, order),
                     makeContainer(rows, FormatEpoch::DclLegacy, order), makeClassContainer({SyntheticClassRow{0x00a4, words, 1}}, order)}) {
                auto session = musx::factory::DocumentFactory::begin();
                const auto document = session.getDocument();
                SourceProfile profile(parsed.formatEpoch);
                profile.byteOrder = parsed.byteOrder;
                ImportReport report(profile.epoch);
                const auto index = LegacyRecordIndex::build(parsed);
                auto referenceSession = musx::factory::DocumentFactory::begin();
                const auto reference = std::move(referenceSession).finish();
                finale_mus_reader::PendingReferences pending;
                musx::factory::ConstructionContext construction;
                const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
                finale_mus_reader::others::importMeasureNumberRegions(context);
                const auto region = document->getOthers()->get<Region>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
                REQUIRE(region);
                CHECK(region->startMeas == 2);
                CHECK(region->endMeas == 999);
                CHECK(region->scoreData->startFont->fontId == 4);
                CHECK(region->scoreData->startYdisp == 40);
                CHECK(region->partData->startYdisp == 40);
                CHECK(region->scoreData->showOnStart);
                for (const auto& data : {region->scoreData, region->partData}) {
                    CHECK(data->leftMmBracketChar == 138);
                    CHECK(data->rightMmBracketChar == 205);
                    CHECK(data->showMmRange);
                    CHECK(data->showOnMmRest);
                    CHECK(data->startAlign == alignment);
                    CHECK(data->multipleAlign == alignment);
                    CHECK(data->mmRestAlign == alignment);
                    CHECK(data->startJustify == alignment);
                    CHECK(data->multipleJustify == alignment);
                    CHECK(data->mmRestJustify == alignment);
                }
                CHECK(region->region == 1);
                CHECK(report.fields.at(finale_mus_reader::instanceKey<Region>(0, musx::dom::Cmper(1))).size() == 168);
                CHECK(report.findField<Region>("scoreData.startAlign", 0, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
                CHECK(report.findField<Region>("scoreData.leftMmBracketChar", 0, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
                CHECK(report.findField<Region>("partData.rightMmBracketChar", 0, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
                CHECK(report.findField<Region>("scoreData.showMmRange", 0, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
                CHECK(report.findField<Region>("partData.showOnMmRest", 0, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
                CHECK(report.findField<Region>("partData.mmRestJustify", 0, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
            }
        }
    }
}

TEST_CASE("Old measure number rest controls follow the full layout", "[class][measure-number-region]")
{
    std::vector<std::int16_t> words(48);
    words[0] = 4;
    words[1] = 12;
    words[3] = 1;
    words[4] = 10;
    words[6] = 48;
    words[7] = 10;
    words[41] = static_cast<std::int16_t>(0x8c88U);
    words[43] = 1;
    std::vector<SyntheticRow> rows;
    for (std::size_t index = 0; index < words.size(); index += 6) {
        rows.push_back({1, "MN", {words[index], words[index + 1], words[index + 2], words[index + 3], words[index + 4], words[index + 5]}});
    }
    const auto parsed = makeContainer(rows, FormatEpoch::UncompressedLegacy, ByteOrder::BigEndian);
    for (const auto major : {std::uint8_t(3), std::uint8_t(5)}) {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        SourceProfile profile(parsed.formatEpoch);
        profile.byteOrder = parsed.byteOrder;
        profile.version = finale_mus_reader::SourceVersion{.major = major, .minor = static_cast<std::uint8_t>(major == 3 ? 8 : 0)};
        ImportReport report(profile.epoch);
        const auto index = LegacyRecordIndex::build(parsed);
        auto referenceSession = musx::factory::DocumentFactory::begin();
        const auto reference = std::move(referenceSession).finish();
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::others::importMeasureNumberRegions(context);
        const auto region = document->getOthers()->get<Region>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(region);
        CHECK(region->scoreData->showMmRange);
        CHECK(region->partData->showOnMmRest);
        CHECK(report.findField<Region>("scoreData.showMmRange", 0, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
        CHECK(report.findField<Region>("partData.showOnMmRest", 0, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
    }
}

TEST_CASE("Pre-3.7 regions import the seven-row layout", "[class][measure-number-region]")
{
    std::vector<std::int16_t> words(42);
    words[0] = 4;
    words[1] = 14;
    words[3] = 1;
    words[4] = 999;
    words[5] = 1;
    words[6] = 48;
    words[7] = 10;
    words[10] = 40;
    words[11] = 0x5b00;
    words[23] = 0x5d00;
    words[37] = 18;
    words[38] = 18;
    words[39] = 256;
    words[40] = 0x1001;
    words[41] = static_cast<std::int16_t>(0xec88);
    std::vector<SyntheticRow> rows;
    for (std::size_t index = 0; index < words.size(); index += 6) {
        rows.push_back({1, "MN", {words[index], words[index + 1], words[index + 2], words[index + 3], words[index + 4], words[index + 5]}});
    }
    const auto parsed = makeContainer(rows, FormatEpoch::UncompressedLegacy, ByteOrder::BigEndian);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    profile.version = finale_mus_reader::SourceVersion{.major = 3, .minor = 5};
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importMeasureNumberRegions(context);
    const auto region = document->getOthers()->get<Region>(0, 1);
    REQUIRE(region);
    CHECK(region->prefix == "[");
    CHECK(region->suffix == "]");
    CHECK(region->scoreData->startWith == 0);
    CHECK_FALSE(region->scoreData->showMmRange);
    CHECK_FALSE(region->partData->showOnMmRest);
    CHECK(region->scoreData->startEnclosure->xMargin == 18);
    CHECK(region->scoreData->startEnclosure->lineWidth == 256);
    CHECK(region->region == 1);
    CHECK(report.findField<Region>("scoreData.startWith", 0, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyBehavior);
    CHECK(report.findField<Region>("scoreData.showMmRange", 0, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyBehavior);
    CHECK(report.findField<Region>("prefix", 0, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("An old region enclosure requires a shape and the automatic-use flag", "[class][measure-number-region]")
{
    const auto check = [](std::int16_t enclosureShape, std::int16_t regionFlags, bool expected) {
        std::vector<std::int16_t> words(48);
        words[3] = 1;
        words[4] = 12;
        words[6] = 48;
        words[7] = 10;
        words[40] = enclosureShape;
        words[41] = regionFlags;
        words[43] = 1;
        const auto parsed = makeClassContainer({SyntheticClassRow{0x00a4, words, 1}}, ByteOrder::LittleEndian);
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        SourceProfile profile(parsed.formatEpoch);
        profile.byteOrder = parsed.byteOrder;
        ImportReport report(profile.epoch);
        const auto index = LegacyRecordIndex::build(parsed);
        auto referenceSession = musx::factory::DocumentFactory::begin();
        const auto reference = std::move(referenceSession).finish();
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::others::importMeasureNumberRegions(context);
        const auto region = document->getOthers()->get<Region>(0, 1);
        REQUIRE(region);
        CHECK(region->scoreData->useStartEncl == expected);
        CHECK(region->scoreData->useMultipleEncl == expected);
        CHECK(region->partData->useStartEncl == expected);
        CHECK(report.findField<Region>("scoreData.useStartEncl", 0, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyBehavior);
    };
    check(0, static_cast<std::int16_t>(0xa088), false);
    check(1, static_cast<std::int16_t>(0x8088), false);
    check(1, static_cast<std::int16_t>(0xa088), true);
}

TEST_CASE("Time style bits populate both shared settings", "[class][measure-number-region]")
{
    std::vector<std::int16_t> words(102);
    words[66] = 1;
    words[67] = 10;
    words[68] = 48;
    words[69] = 10;
    words[95] = static_cast<std::int16_t>(0x0070);
    words[96] = 1;
    const auto parsed = makeClassContainer({SyntheticClassRow{0x00a4, words, 1}}, ByteOrder::LittleEndian);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importMeasureNumberRegions(context);
    const auto region = document->getOthers()->get<Region>(0, 1);
    REQUIRE(region);
    CHECK(region->time);
    CHECK(region->includeHours);
    CHECK(region->smpteFrames);
    CHECK(region->noZero);
    CHECK(region->doubleUp);
}

TEST_CASE("Measure number regions import the fixed-row layout", "[class][measure-number-region]")
{
    const auto result = readFixture("evidence/F97/F97-def-measrest.mus");
    const auto region = result.document->getOthers()->get<Region>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(region);
    REQUIRE(region->scoreData);
    REQUIRE(region->partData);
    CHECK(region->startMeas == 2);
    CHECK(region->endMeas == 999);
    CHECK(region->startChar == U'0');
    CHECK(region->base == 10);
    CHECK(region->scoreData->startFont->fontId == 4);
    CHECK(region->scoreData->startFont->fontSize == 14);
    CHECK(region->scoreData->multipleFont->fontId == 4);
    CHECK(region->partData->mmRestFont->fontId == 4);
    CHECK(region->scoreData->startYdisp == 40);
    CHECK(region->scoreData->mmRestYdisp == 40);
    CHECK(region->region == 1);
    CHECK(result.report.fields.at(finale_mus_reader::instanceKey<Region>(0, musx::dom::Cmper(1))).size() == 168);
}

TEST_CASE("Measure number regions import the 2010 score and part layout", "[class][measure-number-region]")
{
    const auto result = readFixture("evidence/F2011/F2011-perc-instchange.mus");
    const auto region = result.document->getOthers()->get<Region>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(region);
    REQUIRE(region->scoreData);
    CHECK(region->startMeas == 1);
    CHECK(region->endMeas == 1000);
    CHECK(region->startChar == U'0');
    CHECK(region->scoreData->startFont->fontSize == 10);
    CHECK(region->scoreData->startXdisp == 6);
    CHECK(region->scoreData->startYdisp == 44);
    CHECK(region->scoreData->showOnStart);
    CHECK(region->scoreData->showOnTop);
    CHECK(region->partData->startXdisp == 6);
    CHECK(region->region == 1);
}

TEST_CASE("Measure number regions import the 2012 Unicode layout", "[class][measure-number-region]")
{
    const auto result = readFixture("evidence/F2012/F2012-upstem-flags.mus");
    const auto region = result.document->getOthers()->get<Region>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(region);
    REQUIRE(region->scoreData);
    CHECK(region->startMeas == 1);
    CHECK(region->endMeas == 999);
    CHECK(region->scoreData->startFont->fontId == 12);
    CHECK(region->scoreData->startFont->fontSize == 10);
    CHECK(region->scoreData->startFont->italic);
    CHECK(region->scoreData->startXdisp == 6);
    CHECK(region->scoreData->startYdisp == 44);
    CHECK(region->scoreData->mmRestYdisp == -160);
    CHECK(region->scoreData->startEnclosure->yAdd == -4);
    CHECK(region->scoreData->startEnclosure->lineWidth == 115);
    CHECK(region->countFromOne);
    CHECK(region->useScoreInfoForPart);
}

TEST_CASE("Modern measure number alignment value three resolves to left", "[class][measure-number-region]")
{
    for (const auto unicode : {false, true}) {
        std::vector<std::int16_t> words(unicode ? 180 : 102);
        const auto dataWords = unicode ? 35U : 33U;
        const auto flagWord = unicode ? 33U : 31U;
        words[flagWord] = 0x003f;
        words[flagWord + 1] = 0x00fc;
        words[dataWords + flagWord] = 0x003f;
        words[dataWords + flagWord + 1] = 0x00fc;
        words[dataWords * 2] = 1;
        words[dataWords * 2 + 1] = 1000;
        words[unicode ? 173 : 96] = 1;
        const auto parsed = makeClassContainer({SyntheticClassRow{0x00a4, words, 1}}, ByteOrder::LittleEndian);
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        SourceProfile profile(parsed.formatEpoch);
        profile.byteOrder = parsed.byteOrder;
        ImportReport report(profile.epoch);
        const auto index = LegacyRecordIndex::build(parsed);
        auto referenceSession = musx::factory::DocumentFactory::begin();
        const auto reference = std::move(referenceSession).finish();
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::others::importMeasureNumberRegions(context);
        const auto region = document->getOthers()->get<Region>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(region);
        for (const auto& data : {region->scoreData, region->partData}) {
            REQUIRE(data);
            CHECK(data->startAlign == musx::dom::AlignJustify::Left);
            CHECK(data->multipleAlign == musx::dom::AlignJustify::Left);
            CHECK(data->mmRestAlign == musx::dom::AlignJustify::Left);
            CHECK(data->startJustify == musx::dom::AlignJustify::Left);
            CHECK(data->multipleJustify == musx::dom::AlignJustify::Left);
            CHECK(data->mmRestJustify == musx::dom::AlignJustify::Left);
        }
    }
}

TEST_CASE("Measure number enclosures retain width words with the extended flag", "[class][measure-number-region]")
{
    std::vector<std::int16_t> words(180);
    words[13] = 256;
    words[14] = 0x66d1;
    words[19] = 111;
    words[20] = static_cast<std::int16_t>(0xe7f1U);
    words[48] = static_cast<std::int16_t>(0xa790U);
    words[49] = static_cast<std::int16_t>(0xbfffU);
    words[54] = 111;
    words[55] = 0x4001;
    words[70] = 1;
    words[71] = 1000;
    words[74] = 10;
    words[173] = 1;
    const auto parsed = makeClassContainer({SyntheticClassRow{0x00a4, words, 1}}, ByteOrder::LittleEndian);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importMeasureNumberRegions(context);
    const auto region = document->getOthers()->get<Region>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(region);
    REQUIRE(region->scoreData->startEnclosure);
    REQUIRE(region->scoreData->multipleEnclosure);
    REQUIRE(region->partData->startEnclosure);
    REQUIRE(region->partData->multipleEnclosure);
    CHECK(region->scoreData->startEnclosure->lineWidth == 256);
    CHECK(region->scoreData->multipleEnclosure->lineWidth == 111);
    CHECK(region->partData->startEnclosure->lineWidth == -22640);
    CHECK(region->partData->multipleEnclosure->lineWidth == 111);
    CHECK(report.findField<Region>("scoreData.startEnclosure.lineWidth", 0, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
}

} // namespace
} // namespace finale_mus_reader_tests
