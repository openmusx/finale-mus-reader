// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "shape_note_test_support.h"
#include <set>

namespace finale_mus_reader_tests {
namespace {
using namespace classes;
using ShapeNote = musx::dom::details::ShapeNote;
using ShapeNoteStyle = musx::dom::details::ShapeNoteStyle;

template <typename Target>
void checkShapeNoteManifest(const ImportReport& report, const Target& target)
{
    CHECK(report.fields.at(finale_mus_reader::instanceKey<Target>(target.getSourcePartId(), target.getCmper1(), std::nullopt, musx::dom::Cmper(0)))
              .size()
          == 405);
    CHECK(Target::xmlMappingArray().size() == 2);
    CHECK(musx::dom::details::ShapeNoteBase::NoteShapes::xmlMappingArray().size() == 4);
    REQUIRE(target.noteShapes.size() == 101);
    for (std::size_t index = 0; index < target.noteShapes.size(); ++index) {
        for (const auto* member : {"doubleWhole", "whole", "half", "quarter"}) {
            REQUIRE(report.findField<Target>("noteShapes[" + std::to_string(index) + "]." + member, target.getSourcePartId(), target.getCmper1(),
                std::nullopt, musx::dom::Cmper(0)));
        }
    }
    REQUIRE(report.findField<Target>("arrangedByPitch", target.getSourcePartId(), target.getCmper1(), std::nullopt, musx::dom::Cmper(0)));
}

TEST_CASE("Shape notes assemble duration arrays in every epoch and byte order", "[class][shape-note]")
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            finale_mus_reader::container::ParsedContainer parsed(epoch);
            parsed.byteOrder = order;
            for (bool style : {false, true}) {
                for (std::uint16_t duration = 0; duration < 4; ++duration) {
                    appendShapeNoteRecord(parsed, 7, duration, {std::int16_t(65 + duration), 90, 0, 99, 99}, style);
                }
                appendShapeNoteRecord(parsed, 7, 4, {2, 0, 0, 0, 0}, style);
            }
            auto profile = SourceProfile(epoch);
            profile.byteOrder = order;
            profile.version = SourceVersion{.major = 16};
            const auto document = shapeNoteDocument();
            const auto report = importShapeNoteTest(parsed, profile, document, true);
            const auto item = document->getDetails()->get<ShapeNote>(0, 7, 0);
            const auto style = document->getDetails()->get<ShapeNoteStyle>(0, 7, 0);
            REQUIRE(item);
            REQUIRE(style);
            checkShapeNoteManifest(report, *item);
            checkShapeNoteManifest(report, *style);
            CHECK(item->arrangedByPitch);
            CHECK(style->arrangedByPitch);
            CHECK(item->noteShapes[0]->doubleWhole == 65);
            CHECK(item->noteShapes[0]->whole == 66);
            CHECK(item->noteShapes[0]->half == 67);
            CHECK(item->noteShapes[0]->quarter == 68);
            CHECK(item->noteShapes[1]->quarter == 90);
            CHECK(item->noteShapes[2]->quarter == 113);
            CHECK(style->noteShapes[100]->doubleWhole == 87);
            CHECK(report.findField<ShapeNote>("noteShapes[0].whole", 0, musx::dom::Cmper(7), std::nullopt, musx::dom::Cmper(0))->rawValue == 66);
            CHECK(report.findField<ShapeNote>("noteShapes[2].whole", 0, musx::dom::Cmper(7), std::nullopt, musx::dom::Cmper(0))->origin
                  == ValueOrigin::LegacyBehavior);
        }
    }
}

TEST_CASE("Shape note Unicode width and encoding have distinct boundaries", "[class][shape-note]")
{
    for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        for (const auto build : {std::uint8_t(24), std::uint8_t(25), std::uint8_t(36), std::uint8_t(37)}) {
            finale_mus_reader::container::ParsedContainer parsed(FormatEpoch::ZlibLegacy);
            parsed.byteOrder = order;
            const bool wide = build >= 25;
            appendShapeNoteRecord(
                parsed, 0, 0, wide ? std::vector<std::int16_t>{1, std::int16_t(-11939), 0, 0, 123} : std::vector<std::int16_t>{0x5d, 0, 123});
            auto profile = SourceProfile(parsed.formatEpoch);
            profile.byteOrder = order;
            profile.version = SourceVersion{.major = 17, .devStatus = 2, .build = build};
            const auto document = shapeNoteDocument();
            const auto report = importShapeNoteTest(parsed, profile, document);
            const auto item = document->getDetails()->get<ShapeNote>(0, 0, 0);
            REQUIRE(item);
            CHECK(item->noteShapes[0]->doubleWhole == (build >= 37 ? char32_t(0x1d15d) : char32_t(0x5d)));
            CHECK(item->noteShapes[1]->doubleWhole == 87);
            CHECK_FALSE(item->arrangedByPitch);
            CHECK(report.findField<ShapeNote>("arrangedByPitch", 0, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(0))->origin
                  == ValueOrigin::LegacyBehavior);
        }
    }
}

TEST_CASE("Shape note parts merge each duration independently", "[class][shape-note]")
{
    finale_mus_reader::container::ParsedContainer parsed(FormatEpoch::ZlibLegacy);
    parsed.byteOrder = ByteOrder::LittleEndian;
    appendShapeNoteRecord(parsed, 2, 0, {65, 66, 0, 0, 0});
    appendShapeNoteRecord(parsed, 2, 1, {70, 71, 0, 0, 0});
    appendShapeNoteRecord(parsed, 2, 4, {1, 0, 0, 0, 0});
    appendShapeNoteRecord(parsed, 2, 0, {80, 0, 0, 0, 0}, false, 1, true, {0xffff, 0, 0, 0, 0});
    auto profile = SourceProfile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    profile.version = SourceVersion{.major = 16};
    const auto document = shapeNoteDocument();
    const auto report = importShapeNoteTest(parsed, profile, document);
    const auto score = document->getDetails()->get<ShapeNote>(0, 2, 0);
    const auto part = document->getDetails()->get<ShapeNote>(1, 2, 0);
    REQUIRE(score);
    REQUIRE(part);
    CHECK(part->getShareMode() == musx::dom::EnigmaBase::ShareMode::Partial);
    CHECK(score->noteShapes[0]->doubleWhole == 65);
    CHECK(part->noteShapes[0]->doubleWhole == 80);
    CHECK(part->noteShapes[1]->doubleWhole == 66);
    CHECK(part->noteShapes[0]->whole == 70);
    CHECK(part->arrangedByPitch);
    checkShapeNoteManifest(report, *part);
}

TEST_CASE("Shape note characters use staff and style font overrides", "[class][shape-note]")
{
    const auto document = shapeNoteDocument();
    using Share = musx::dom::EnigmaBase::ShareMode;
    auto font = std::make_shared<musx::dom::others::FontDefinition>(document, musx::dom::Cmper(0), Share::All, musx::dom::Cmper(9));
    font->name = "Test Text";
    font->charsetBank = musx::dom::others::FontDefinition::CharacterSetBank::Windows;
    font->charsetVal = 0;
    document->getOthers()->add(musx::dom::others::FontDefinition::XmlNodeName, font);
    auto staff = std::make_shared<musx::dom::others::Staff>(document, musx::dom::Cmper(0), Share::All, musx::dom::Cmper(7));
    staff->useNoteFont = true;
    staff->noteFont = std::make_shared<musx::dom::FontInfo>(document);
    staff->noteFont->fontId = 9;
    document->getOthers()->add(musx::dom::others::Staff::XmlNodeName, staff);
    auto style = std::make_shared<musx::dom::others::StaffStyle>(document, musx::dom::Cmper(0), Share::All, musx::dom::Cmper(7));
    style->useNoteFont = true;
    style->noteFont = staff->noteFont;
    document->getOthers()->add(musx::dom::others::StaffStyle::XmlNodeName, style);
    finale_mus_reader::container::ParsedContainer parsed(FormatEpoch::DclLegacy);
    parsed.byteOrder = ByteOrder::LittleEndian;
    appendShapeNoteRecord(parsed, 7, 0, {0x80, 0, 0, 0, 0});
    appendShapeNoteRecord(parsed, 7, 0, {0x80, 0, 0, 0, 0}, true);
    auto profile = SourceProfile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    const auto report = importShapeNoteTest(parsed, profile, document);
    CHECK(document->getDetails()->get<ShapeNote>(0, 7, 0)->noteShapes[0]->doubleWhole == 0x20ac);
    CHECK(document->getDetails()->get<ShapeNoteStyle>(0, 7, 0)->noteShapes[0]->doubleWhole == 0x20ac);
    CHECK(report.findField<ShapeNote>("noteShapes[0].doubleWhole", 0, musx::dom::Cmper(7), std::nullopt, musx::dom::Cmper(0))->rawValue == 0x80);
}

TEST_CASE("Shape note arrays cap at 101 slots and ignore incomplete Unicode pairs", "[class][shape-note]")
{
    finale_mus_reader::container::ParsedContainer parsed(FormatEpoch::ZlibLegacy);
    parsed.byteOrder = ByteOrder::LittleEndian;
    std::vector<std::int16_t> words;
    for (int i = 0; i < 103; ++i) {
        words.push_back(1);
        words.push_back(std::int16_t(i + 100));
    }
    appendShapeNoteRecord(parsed, 7, 0, words);
    appendShapeNoteRecord(parsed, 7, 1, {1});
    auto profile = SourceProfile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    profile.version = SourceVersion{.major = 17, .devStatus = 3};
    const auto document = shapeNoteDocument();
    const auto report = importShapeNoteTest(parsed, profile, document);
    const auto item = document->getDetails()->get<ShapeNote>(0, 7, 0);
    REQUIRE(item);
    REQUIRE(item->noteShapes.size() == 101);
    CHECK(item->noteShapes.back()->doubleWhole == 0x100c8);
    CHECK(item->noteShapes.front()->whole == 119);
    checkShapeNoteManifest(report, *item);
}

TEST_CASE("Controlled shape note fixtures retain duration identity and raw glyphs", "[class][shape-note]")
{
    for (const auto* fixture :
        {"evidence/F100/F100-noteshapes-enable.mus", "evidence/F372/F372-noteopts-noshapes.mus", "evidence/F2008/F2008-notehead-colors.mus"}) {
        const auto result = readFixture(fixture);
        const auto item = result.document->getDetails()->get<ShapeNote>(0, 0, 0);
        REQUIRE(item);
        REQUIRE(item->noteShapes.size() == 101);
        CHECK(item->noteShapes[0]->doubleWhole == 87);
        CHECK(item->noteShapes[0]->whole == 119);
        const auto* field =
            result.report.findField<ShapeNote>("noteShapes[0].doubleWhole", 0, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(0));
        REQUIRE(field);
        CHECK(field->rawValue == 87);
        CHECK(field->origin == ValueOrigin::LegacyMus);
    }
    const auto result = readFixture("evidence/F2006/F2006-embedded-tif.mus");
    const auto style = result.document->getDetails()->get<ShapeNoteStyle>(0, 19, 0);
    REQUIRE(style);
    CHECK(style->noteShapes[0]->doubleWhole == 87);
    const auto* field =
        result.report.findField<ShapeNoteStyle>("noteShapes[0].doubleWhole", 0, musx::dom::Cmper(19), std::nullopt, musx::dom::Cmper(0));
    REQUIRE(field);
    CHECK(field->rawValue == 87);
    CHECK(field->sourceIdentity == 0x4859);
}

TEST_CASE("Absent shape note records produce no score content", "[class][shape-note]")
{
    finale_mus_reader::container::ParsedContainer parsed(FormatEpoch::CodaBanner);
    auto profile = SourceProfile(parsed.formatEpoch);
    const auto document = shapeNoteDocument();
    importShapeNoteTest(parsed, profile, document);
    CHECK(document->getDetails()->getAllSources<ShapeNote>().empty());
    CHECK(document->getDetails()->getAllSources<ShapeNoteStyle>().empty());
}
} // namespace
} // namespace finale_mus_reader_tests
