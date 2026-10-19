// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <cstdint>
#include <vector>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using Definition = musx::dom::others::TextRepeatDef;
using Enclosure = musx::dom::others::TextRepeatEnclosure;
using Text = musx::dom::others::TextRepeatText;

TEST_CASE("Text repeat definitions recover font and substitution from fixed rows", "[class][text-repeat]")
{
    const auto result = readFixture("evidence/F2006/F2006-embedded-tif.mus");
    const auto first = result.document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    const auto third = result.document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(3));
    const auto fifth = result.document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(5));
    REQUIRE(first);
    REQUIRE(third);
    REQUIRE(fifth);
    CHECK(first->font->fontId == 1);
    CHECK(first->font->fontSize == 12);
    CHECK(first->font->italic);
    CHECK(first->justification == musx::dom::AlignJustify::Right);
    CHECK(third->poundReplace == Definition::PoundReplaceOption::MeasureNumber);
    CHECK(fifth->poundReplace == Definition::PoundReplaceOption::RepeatID);
    const auto firstText = result.document->getOthers()->get<Text>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(firstText);
    CHECK(firstText->text == "D.C. al Fine");
    const auto* fontId = result.report.findField<Definition>("font.fontId", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(fontId);
    CHECK(fontId->origin == ValueOrigin::LegacyMus);
    CHECK(fontId->rawValue == 1);
    CHECK(result.report.findField<Definition>("useThisFont", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Text repeat text uses its definition font before Finale 2012", "[class][text-repeat]")
{
    const auto result = readFixture("evidence/F98/F98-baseline.mus");
    const auto definition = result.document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(6));
    const auto text = result.document->getOthers()->get<Text>(musx::dom::SCORE_PARTID, musx::dom::Cmper(6));
    REQUIRE(definition);
    REQUIRE(text);
    CHECK(definition->font->fontId == 0);
    CHECK(text->text == "\xef\xac\x81");
}

TEST_CASE("Finale 1 text repeat recovers definition and text but leaves enclosure out of scope", "[class][text-repeat]")
{
    const auto result = readFixture("evidence/F100/F100-textrpt.mus");
    const auto definition = result.document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    const auto enclosure = result.document->getOthers()->get<Enclosure>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    const auto text = result.document->getOthers()->get<Text>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(definition);
    CHECK_FALSE(enclosure);
    REQUIRE(text);
    CHECK(definition->font->fontId == 1);
    CHECK(definition->font->fontSize == 12);
    CHECK(definition->justification == musx::dom::AlignJustify::Center);
    CHECK(definition->poundReplace == Definition::PoundReplaceOption::MeasureNumber);
    CHECK(definition->hasEnclosure);
    CHECK(text->text == "D.C. al Coda #");
}

TEST_CASE("Coda text repeat enclosures are out of scope but Finale 3.7 recovers the same row", "[class][text-repeat]")
{
    const auto checkShape = [](const char* path, bool expectedEnclosure) {
        const auto result = readFixture(path);
        const auto definition = result.document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        const auto enclosure = result.document->getOthers()->get<Enclosure>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(definition);
        CHECK(definition->hasEnclosure);
        if (!expectedEnclosure) {
            CHECK_FALSE(enclosure);
            return;
        }
        REQUIRE(enclosure);
        CHECK(enclosure->shape == Enclosure::Shape::Diamond);
        CHECK(enclosure->xMargin == 18);
        CHECK(enclosure->yMargin == 11);
        CHECK(enclosure->lineWidth == 41);
    };
    checkShape("evidence/F263/F263-F100-textrpt.mus", false);
    checkShape("evidence/F372/F372-F263-F100-textrpt.mus", true);
}

TEST_CASE("Finale 2012 text repeat recovers class enclosure and Unicode text", "[class][text-repeat]")
{
    const auto result = readFixture("evidence/F2012/F2012-textrpt.mus");
    const auto definition = result.document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    const auto enclosure = result.document->getOthers()->get<Enclosure>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    const auto text = result.document->getOthers()->get<Text>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(definition);
    REQUIRE(enclosure);
    REQUIRE(text);
    CHECK(definition->font->fontId == 12);
    CHECK(definition->font->fontSize == 14);
    CHECK(definition->font->bold);
    CHECK(definition->font->italic);
    CHECK(definition->hasEnclosure);
    CHECK(definition->justification == musx::dom::AlignJustify::Right);
    CHECK(enclosure->xMargin == 9);
    CHECK(enclosure->yMargin == 9);
    CHECK(enclosure->lineWidth == 224);
    CHECK(enclosure->shape == Enclosure::Shape::Rectangle);
    CHECK(enclosure->notTall);
    CHECK(text->text == "D.S. \xc3\xa4l Fine #");
}

TEST_CASE("Text repeat text spans incidences and decodes UTF-16 after Finale 2012", "[class][text-repeat]")
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    const auto parsed =
        makeClassContainer({SyntheticClassRow{0x00f4, {0, 0, 1, 12, 2, 1}, 7}, SyntheticClassRow{0x00f6, {0x00e9, 0x0031, 0, 0x0032, 0, 0}, 7}},
            ByteOrder::LittleEndian);
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    profile.version = SourceVersion{.major = 17};
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importTextRepeatDefs(context);
    finale_mus_reader::others::importTextRepeatTexts(context);
    const auto text = document->getOthers()->get<Text>(musx::dom::SCORE_PARTID, musx::dom::Cmper(7));
    REQUIRE(text);
    CHECK(text->text
          == "\xc3\xa9"
             "1");
}

TEST_CASE("Text repeat definition and enclosure decode their extended fixed rows", "[class][text-repeat]")
{
    const auto parsed = makeContainer(
        {{7, "RS", {17, -9, 3, 18, 3, 0x0862}}, {7, "RS", {2, 1, 3, 0, 0, 0}}, {7, "Rx", {-7, 11, 24, 8, 32, static_cast<std::int16_t>(0x9802)}},
            {8, "RS", {0, 0, 2, 12, 0, 3}}, {8, "Rx", {1, 2, 3, 4, 5, 9}}, {9, "Rx", {0, 0, 6, 7, 37, 0x2001}}});
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
    finale_mus_reader::others::importTextRepeatDefs(context);
    finale_mus_reader::others::importTextRepeatEnclosures(context);
    const auto definition = document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(7));
    const auto enclosure = document->getOthers()->get<Enclosure>(musx::dom::SCORE_PARTID, musx::dom::Cmper(7));
    REQUIRE(definition);
    REQUIRE(enclosure);
    CHECK(definition->font->fontId == 3);
    CHECK(definition->font->fontSize == 18);
    CHECK(definition->font->bold);
    CHECK(definition->font->italic);
    CHECK(definition->hasEnclosure);
    CHECK(definition->useThisFont);
    CHECK(definition->justification == musx::dom::AlignJustify::Center);
    CHECK(definition->poundReplace == Definition::PoundReplaceOption::RepeatID);
    CHECK(definition->passList == std::vector<int>{1, 3});
    CHECK(enclosure->xAdd == -7);
    CHECK(enclosure->yAdd == 11);
    CHECK(enclosure->xMargin == 24);
    CHECK(enclosure->yMargin == 8);
    CHECK(enclosure->lineWidth == 32);
    CHECK(enclosure->shape == Enclosure::Shape::Ellipse);
    CHECK(enclosure->fixedSize);
    CHECK(enclosure->notTall);
    CHECK(enclosure->opaque);
    CHECK_FALSE(enclosure->equalAspect);
    CHECK_FALSE(enclosure->roundCorners);
    CHECK(enclosure->cornerRadius == 0);
    CHECK(report.findField<Definition>("passList[1]", musx::dom::SCORE_PARTID, musx::dom::Cmper(7))->rawValue == 3);
    CHECK(report.findField<Enclosure>("xAdd", musx::dom::SCORE_PARTID, musx::dom::Cmper(7))->rawValue == -7);
    CHECK(report.findField<Enclosure>("cornerRadius", musx::dom::SCORE_PARTID, musx::dom::Cmper(7))->origin == ValueOrigin::MusxOnly);
    const auto unsupportedDefinition = document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(8));
    const auto unsupportedEnclosure = document->getOthers()->get<Enclosure>(musx::dom::SCORE_PARTID, musx::dom::Cmper(8));
    REQUIRE(unsupportedDefinition);
    REQUIRE(unsupportedEnclosure);
    CHECK(unsupportedDefinition->font->fontId == 2);
    CHECK(unsupportedEnclosure->xAdd == 1);
    CHECK(unsupportedEnclosure->shape == Enclosure::Shape::NoEnclosure);
    const auto* justification = report.findField<Definition>("justification", musx::dom::SCORE_PARTID, musx::dom::Cmper(8));
    const auto* shape = report.findField<Enclosure>("shape", musx::dom::SCORE_PARTID, musx::dom::Cmper(8));
    REQUIRE(justification);
    REQUIRE(shape);
    CHECK(justification->origin == ValueOrigin::Unmapped);
    CHECK(justification->rawValue == 3);
    CHECK(shape->origin == ValueOrigin::Unmapped);
    CHECK(shape->rawValue == 9);
    const auto flaggedEnclosure = document->getOthers()->get<Enclosure>(musx::dom::SCORE_PARTID, musx::dom::Cmper(9));
    REQUIRE(flaggedEnclosure);
    CHECK(flaggedEnclosure->lineWidth == 37);
    CHECK(flaggedEnclosure->shape == Enclosure::Shape::Rectangle);
    const auto* width = report.findField<Enclosure>("lineWidth", musx::dom::SCORE_PARTID, musx::dom::Cmper(9));
    REQUIRE(width);
    CHECK(width->origin == ValueOrigin::LegacyMus);
    CHECK(width->rawValue == 37);
}

} // namespace
} // namespace finale_mus_reader_tests
