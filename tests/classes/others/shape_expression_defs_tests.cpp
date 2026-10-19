// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {
using namespace classes;
using ShapeExpression = musx::dom::others::ShapeExpressionDef;

TEST_CASE("Shape expressions recover the first DO incidence in early layouts", "[class][shape-expression]")
{
    for (const auto path : {"evidence/F263/F263-clef-baseline.mus", "evidence/F98/F98-baseline.mus"}) {
        const auto result = readFixture(path);
        const auto expression = result.document->getOthers()->get<ShapeExpression>(musx::dom::SCORE_PARTID, 1);
        REQUIRE(expression);
        CHECK(expression->shapeDef == (std::string_view(path).find("F263") != std::string_view::npos ? 2 : 44));
        CHECK(expression->description.empty());
        if (std::string_view(path).find("F263") != std::string_view::npos) {
            CHECK(result.report.formatEpoch == FormatEpoch::CodaBanner);
            CHECK(result.report.findField<ShapeExpression>("horzMeasExprAlign", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin
                  == ValueOrigin::LegacyBehavior);
        } else {
            CHECK(expression->horzMeasExprAlign == musx::dom::others::HorizontalMeasExprAlign::Manual);
            CHECK(result.report.findField<ShapeExpression>("horzMeasExprAlign", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin
                  == ValueOrigin::LegacyBehavior);
        }
        CHECK(result.report.findField<ShapeExpression>("shapeDef", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
        CHECK(result.report.findField<ShapeExpression>("description", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin
              == ValueOrigin::LegacyBehavior);
        CHECK_FALSE(expression->breakMmRest);
        const auto* breakField = result.report.findField<ShapeExpression>("breakMmRest", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(breakField);
        CHECK(breakField->origin == ValueOrigin::LegacyBehavior);
        CHECK(breakField->rawValue == 0);
    }
}

TEST_CASE("Shape definitions stored before F2004 take the upgrade's positioning", "[class][shape-expression]")
{
    for (const auto* path : {"evidence/F263/F263-clef-baseline.mus", "evidence/F98/F98-baseline.mus"}) {
        const auto result = readFixture(path);
        const auto expression = result.document->getOthers()->get<ShapeExpression>(musx::dom::SCORE_PARTID, 1);
        REQUIRE(expression);
        CHECK(expression->horzMeasExprAlign == musx::dom::others::HorizontalMeasExprAlign::Manual);
        CHECK(expression->vertMeasExprAlign == musx::dom::others::VerticalMeasExprAlign::Manual);
        CHECK(expression->horzExprJustification == musx::dom::AlignJustify::Left);
        CHECK(expression->measXAdjust == 0);
        CHECK(expression->yAdjustBaseline == 7);
        CHECK(expression->yAdjustEntry == 0);
        for (const auto* member :
            {"horzMeasExprAlign", "vertMeasExprAlign", "horzExprJustification", "measXAdjust", "yAdjustBaseline", "yAdjustEntry"}) {
            CAPTURE(member);
            const auto* field = result.report.findField<ShapeExpression>(member, musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
            REQUIRE(field);
            CHECK(field->origin == ValueOrigin::LegacyBehavior);
        }
    }
}

TEST_CASE("F100 lock shape survives an F263 resave", "[class][shape-expression]")
{
    for (const auto& [path, locked] :
        {std::pair{"evidence/F100/F100-shapexp.mus", false}, std::pair{"evidence/F100/F100-shapexp-lockshape.mus", true},
            std::pair{"evidence/F263/F263-F100-shapexp.mus", false}, std::pair{"evidence/F263/F263-F100-shapexp-lockshape.mus", true}}) {
        const auto result = readFixture(path);
        const auto expression = result.document->getOthers()->get<ShapeExpression>(musx::dom::SCORE_PARTID, 1);
        REQUIRE(expression);
        CHECK(expression->shapeDef == 1);
        CHECK(expression->noHorzStretch == locked);
        const auto* field = result.report.findField<ShapeExpression>("noHorzStretch", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(field);
        CHECK(field->origin == ValueOrigin::LegacyMus);
        CHECK(field->rawValue == (locked ? 0x0100 : 0));
    }
}

TEST_CASE("F372 shape expression preserves Lock Shape after resave", "[class][shape-expression]")
{
    for (const auto& [path, locked] :
        {std::pair{"evidence/F372/F372-F263-F100-shapexp.mus", false}, std::pair{"evidence/F372/F372-F263-F100-shpexp-lkshp.mus", true}}) {
        const auto result = readFixture(path);
        const auto expression = result.document->getOthers()->get<ShapeExpression>(musx::dom::SCORE_PARTID, 1);
        REQUIRE(expression);
        CHECK(expression->shapeDef == 2);
        CHECK(expression->masterShape);
        CHECK(expression->noHorzStretch == locked);
        const auto* field = result.report.findField<ShapeExpression>("noHorzStretch", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(field);
        CHECK(field->origin == ValueOrigin::LegacyMus);
        CHECK(field->rawValue == (locked ? 0x0900 : 0x0800));
    }
}

TEST_CASE("Shape-expression breakMmRest begins in F2002", "[class][shape-expression]")
{
    using namespace finale_mus_reader;
    for (const auto version : {versions::finale2001, versions::finale2002}) {
        auto parsed = makeContainer({{1, "DO", {4, 0, 0, 0, 0, 0x4000}}}, FormatEpoch::DclLegacy);
        auto profile = profileFor(version.major);
        profile.epoch = FormatEpoch::DclLegacy;
        const auto index = LegacyRecordIndex::build(parsed);
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        ImportReport report(profile.epoch);
        PendingReferences pending;
        const ImportContext context{index, profile, noSource, document, document, report, pending, session.getConstructionContext()};
        others::importShapeExpressionDefs(context);
        const auto expression = document->getOthers()->get<ShapeExpression>(0, 1);
        REQUIRE(expression);
        const bool supported = version.major == versions::finale2002.major;
        CHECK(expression->breakMmRest == supported);
        const auto* field = report.findField<ShapeExpression>("breakMmRest", 0, musx::dom::Cmper(1));
        REQUIRE(field);
        CHECK(field->origin == (supported ? ValueOrigin::LegacyMus : ValueOrigin::LegacyBehavior));
        CHECK(field->rawValue == (supported ? 0x4000 : 0));
    }
}

TEST_CASE("Shape-expression noPrint begins in F97", "[class][shape-expression]")
{
    using namespace finale_mus_reader;
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy}) {
        auto parsed = makeContainer({{1, "DO", {4, 0, 0, 0, 0, 0x0400}}}, epoch);
        auto profile = epoch == FormatEpoch::CodaBanner ? profileFor(versions::finale3_7.major, versions::finale3_7.minor)
                                                        : profileFor(versions::finale97.major, versions::finale97.minor);
        profile.epoch = epoch;
        const auto index = LegacyRecordIndex::build(parsed);
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        ImportReport report(profile.epoch);
        PendingReferences pending;
        const ImportContext context{index, profile, noSource, document, document, report, pending, session.getConstructionContext()};
        others::importShapeExpressionDefs(context);
        const auto expression = document->getOthers()->get<ShapeExpression>(0, 1);
        REQUIRE(expression);
        const bool supported = epoch == FormatEpoch::UncompressedLegacy;
        CHECK(expression->noPrint == supported);
        const auto* field = report.findField<ShapeExpression>("noPrint", 0, musx::dom::Cmper(1));
        REQUIRE(field);
        CHECK(field->origin == (supported ? ValueOrigin::LegacyMus : ValueOrigin::LegacyBehavior));
        CHECK(field->rawValue == (supported ? 0x0400 : 0));
    }
}

TEST_CASE("F2006 shape expressions recover shape flags and positioning", "[class][shape-expression]")
{
    const auto result = readFixture("evidence/F2006/F2006-embedded-tiff.mus");
    const auto expression = result.document->getOthers()->get<ShapeExpression>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(expression);
    CHECK(expression->shapeDef == 6);
    CHECK(expression->masterShape);
    CHECK_FALSE(expression->breakMmRest);
    CHECK_FALSE(expression->noPrint);
    CHECK(expression->horzMeasExprAlign == musx::dom::others::HorizontalMeasExprAlign::Manual);
    CHECK(expression->measXAdjust == 0);
    CHECK(expression->description.empty());
    CHECK(result.report.findField<ShapeExpression>("masterShape", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
    CHECK(result.report.findField<ShapeExpression>("horzMeasExprAlign", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin
          == ValueOrigin::LegacyMus);
    const auto swing = result.document->getOthers()->get<ShapeExpression>(musx::dom::SCORE_PARTID, 7);
    REQUIRE(swing);
    CHECK(swing->playbackType == musx::dom::others::PlaybackType::Swing);
    const auto* playback = result.report.findField<ShapeExpression>("playbackType", musx::dom::SCORE_PARTID, musx::dom::Cmper(7));
    REQUIRE(playback);
    CHECK(playback->origin == ValueOrigin::LegacyMus);
    CHECK(static_cast<std::uint16_t>(playback->rawValue) == 0x090eU);
}

TEST_CASE("Zlib shape expressions use the extended DO layout", "[class][shape-expression]")
{
    const auto result = readFixture("evidence/F2011/F2011-perc-instchange.mus");
    const auto expression = result.document->getOthers()->get<ShapeExpression>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(expression);
    CHECK(expression->shapeDef == 6);
    CHECK(expression->categoryId == 7);
    CHECK(expression->description.empty());
    CHECK(result.report.findField<ShapeExpression>("shapeDef", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
    CHECK(result.report.findField<ShapeExpression>("categoryId", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("F2011 shape expression descriptions decode non-ASCII platform text", "[class][shape-expression]")
{
    const auto result = readFixture("evidence/F2011/F2011-shapexp-desc.mus");
    const auto expression = result.document->getOthers()->get<ShapeExpression>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(expression);
    CHECK(expression->description == "This is a löng déscription for a shape èxpression.");
    const auto* description = result.report.findField<ShapeExpression>("description", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(description);
    CHECK(description->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Shape expression report inventories every persisted field", "[class][shape-expression]")
{
    const auto result = readFixture("evidence/F98/F98-baseline.mus");
    const auto expression = result.document->getOthers()->get<ShapeExpression>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(expression);
    constexpr const char* fields[] = {"shapeDef", "categoryId", "rehearsalMarkStyle", "value", "execShape", "auxData1", "playPass", "breakMmRest",
        "useAuxData", "masterShape", "noPrint", "noHorzStretch", "playbackType", "horzMeasExprAlign", "vertMeasExprAlign", "horzExprJustification",
        "measXAdjust", "yAdjustEntry", "yAdjustBaseline", "useCategoryFonts", "useCategoryPos", "description"};
    CHECK(std::size(fields) == ShapeExpression::xmlMappingArray().size());
    for (const auto* name : fields) {
        CHECK(result.report.findField<ShapeExpression>(name, musx::dom::SCORE_PARTID, musx::dom::Cmper(1)));
    }
}

TEST_CASE("Shape flags and positioning decode independently of text expression flags", "[class][shape-expression]")
{
    using namespace finale_mus_reader;
    auto parsed = makeContainer(
        {{7, "DO", {4, 0, 9, 12, 2, static_cast<std::int16_t>(0x7d01)}}, {7, "DO", {3, 1, -12, 1, 0, 0}}, {7, "DO", {2, -20, 6, 0, 5, -7}}},
        FormatEpoch::DclLegacy);
    auto profile = profileFor(versions::finale2006.major);
    profile.epoch = FormatEpoch::DclLegacy;
    const auto index = LegacyRecordIndex::build(parsed);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    ImportReport report(profile.epoch);
    PendingReferences pending;
    const ImportContext context{index, profile, noSource, document, document, report, pending, session.getConstructionContext()};
    others::importShapeExpressionDefs(context);
    const auto expression = document->getOthers()->get<ShapeExpression>(0, 7);
    REQUIRE(expression);
    CHECK(expression->shapeDef == 4);
    CHECK(expression->execShape == 9);
    CHECK(expression->value == 0);
    CHECK(expression->auxData1 == 12);
    CHECK(expression->playPass == 2);
    CHECK(expression->useAuxData);
    CHECK(expression->masterShape);
    CHECK(expression->breakMmRest);
    CHECK(expression->noPrint);
    CHECK(expression->noHorzStretch);
    CHECK(expression->playbackType == musx::dom::others::PlaybackType::Tempo);
    CHECK(expression->measXAdjust == -12);
    CHECK(expression->yAdjustBaseline == -20);
    CHECK(expression->yAdjustEntry == 5);
    CHECK(expression->description.empty());
    CHECK(report.findField<ShapeExpression>("noPrint", 0, musx::dom::Cmper(7))->rawValue == 0x7d01);
    CHECK(report.findField<ShapeExpression>("breakMmRest", 0, musx::dom::Cmper(7))->rawValue == 0x7d01);
}

TEST_CASE("Zlib shape records decode in either byte order", "[class][shape-expression]")
{
    using namespace finale_mus_reader;
    for (auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        std::vector<std::int16_t> words(18);
        words[0] = 9;
        words[5] = 0x0800;
        words[6] = 3;
        words[12] = 2;
        words[13] = static_cast<std::int16_t>(0xc002);
        words.insert(words.end(), {0x0041, 0x0042});
        auto parsed = makeClassContainer({SyntheticClassRow{0xeb, words, 1}}, order);
        auto profile = profileFor(versions::finale2012.major);
        profile.epoch = FormatEpoch::ZlibLegacy;
        profile.byteOrder = order;
        const auto index = LegacyRecordIndex::build(parsed);
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        ImportReport report(profile.epoch);
        PendingReferences pending;
        const ImportContext context{index, profile, noSource, document, document, report, pending, session.getConstructionContext()};
        others::importShapeExpressionDefs(context);
        const auto expression = document->getOthers()->get<ShapeExpression>(0, 1);
        REQUIRE(expression);
        CHECK(expression->shapeDef == 9);
        CHECK(expression->categoryId == 2);
        CHECK(expression->useCategoryFonts);
        CHECK(expression->useCategoryPos);
        CHECK(expression->description == "AB");
    }
}

TEST_CASE("Zero expression categories resolve to miscellaneous while preserving the category flags", "[class][shape-expression][text-expression]")
{
    using namespace finale_mus_reader;
    for (const bool text : {false, true}) {
        for (const auto flags : {0, 0x4000, 0x8000, 0xc000}) {
            std::vector<std::int16_t> words(18);
            words[13] = static_cast<std::int16_t>(flags);
            auto parsed = makeClassContainer({SyntheticClassRow{static_cast<std::uint16_t>(text ? 0xf1 : 0xeb), words, 1}}, ByteOrder::LittleEndian);
            auto profile = profileFor(versions::finale2012.major);
            profile.epoch = FormatEpoch::ZlibLegacy;
            profile.byteOrder = ByteOrder::LittleEndian;
            const auto index = LegacyRecordIndex::build(parsed);
            auto session = musx::factory::DocumentFactory::begin();
            const auto document = session.getDocument();
            ImportReport report(profile.epoch);
            PendingReferences pending;
            const ImportContext context{index, profile, noSource, document, document, report, pending, session.getConstructionContext()};
            const auto check = [&]<typename Definition>() {
                const auto expression = document->getOthers()->get<Definition>(0, 1);
                REQUIRE(expression);
                CHECK(expression->categoryId == 7);
                CHECK(expression->useCategoryFonts == bool(flags & 0x8000));
                CHECK(expression->useCategoryPos == bool(flags & 0x4000));
                const auto* field = report.findField<Definition>("categoryId", musx::dom::Cmper(0), musx::dom::Cmper(1));
                REQUIRE(field);
                CHECK(field->origin == ValueOrigin::LegacyMusAdjusted);
                CHECK(field->rawValue == static_cast<std::int16_t>(flags));
            };
            if (text) {
                others::importTextExpressionDefs(context);
                check.template operator()<musx::dom::others::TextExpressionDef>();
            } else {
                others::importShapeExpressionDefs(context);
                check.template operator()<ShapeExpression>();
            }
        }
    }
}

TEST_CASE("Shape expression descriptions use platform text before F2012 and UTF-16 afterward", "[class][shape-expression]")
{
    using namespace finale_mus_reader;
    for (const bool unicode : {false, true}) {
        std::vector<std::int16_t> words(18);
        words[0] = 9;
        words[13] = 7;
        if (unicode) {
            words.insert(words.end(), {0x0020, 0x0028, 0x0043, 0x006f, 0x0070, 0x0079, 0x0029, 0});
        } else {
            words.insert(words.end(), {0x2820, 0x6f43, 0x7970, 0x0029});
        }
        auto parsed = makeClassContainer({SyntheticClassRow{0xeb, words, 1}}, ByteOrder::LittleEndian);
        auto profile = profileFor(unicode ? versions::finale2012.major : versions::finale2009.major);
        profile.epoch = FormatEpoch::ZlibLegacy;
        profile.byteOrder = ByteOrder::LittleEndian;
        const auto index = LegacyRecordIndex::build(parsed);
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        ImportReport report(profile.epoch);
        PendingReferences pending;
        const ImportContext context{index, profile, noSource, document, document, report, pending, session.getConstructionContext()};
        others::importShapeExpressionDefs(context);
        const auto expression = document->getOthers()->get<ShapeExpression>(0, 1);
        REQUIRE(expression);
        CHECK(expression->description == " (Copy)");
        const auto* field = report.findField<ShapeExpression>("description", 0, musx::dom::Cmper(1));
        REQUIRE(field);
        CHECK(field->origin == ValueOrigin::LegacyMus);
        CHECK(field->rawValue == (unicode ? 16 : 8));
    }
}

TEST_CASE("Incomplete extended shape expressions are skipped", "[class][shape-expression]")
{
    using namespace finale_mus_reader;
    auto parsed = makeContainer({{3, "DO", {4, 0, 0, 0, 0, 0}}, {3, "DO", {3, 0, 0, 0, 0, 0}}}, FormatEpoch::DclLegacy);
    auto profile = profileFor(versions::finale2006.major);
    profile.epoch = FormatEpoch::DclLegacy;
    const auto index = LegacyRecordIndex::build(parsed);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    ImportReport report(profile.epoch);
    PendingReferences pending;
    const ImportContext context{index, profile, noSource, document, document, report, pending, session.getConstructionContext()};
    others::importShapeExpressionDefs(context);
    CHECK_FALSE(document->getOthers()->get<ShapeExpression>(0, 3));
    CHECK_FALSE(report.diagnostics.empty());
}
} // namespace
} // namespace finale_mus_reader_tests
