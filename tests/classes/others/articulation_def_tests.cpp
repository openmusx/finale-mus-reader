// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <cstdint>
#include <set>
#include <string>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using Definition = musx::dom::others::ArticulationDef;

TEST_CASE("Articulation definitions decode four fixed rows", "[class][articulation-def]")
{
    const auto result = readFixture("evidence/F2001/F2001Win-csharp-artic.mus");
    const auto definition = result.document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(definition);
    CHECK(definition->charMain == U'>');
    CHECK(definition->charAlt == U'>');
    CHECK(definition->fontMain->fontSize == 24);
    CHECK(definition->fontAlt->fontSize == 24);
    CHECK(definition->defVertPos == 24);
    CHECK_FALSE(definition->noPrint);
    CHECK(result.report.findField<Definition>("noPrint", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
    CHECK(definition->copyMode == Definition::CopyMode::None);
    const auto* character = result.report.findField<Definition>("charMain", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(character);
    CHECK(character->origin == ValueOrigin::LegacyMus);
    CHECK(character->rawValue == 62);
    CHECK(result.report.findField<Definition>("autoStack", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::MusxOnly);
    const auto key = finale_mus_reader::instanceKey<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(result.report.fields.contains(key));
    std::set<std::string> actual;
    for (const auto& [name, info] : result.report.fields.at(key)) {
        static_cast<void>(info);
        actual.insert(name);
    }
    CHECK(actual
          == std::set<std::string>{"charMain", "fontMain.fontId", "fontMain.fontSize", "fontMain.bold", "fontMain.italic", "fontMain.underline",
              "fontMain.strikeout", "fontMain.absolute", "fontMain.hidden", "copyMode", "useTopNote", "autoHorz", "autoVert", "autoVertMode",
              "outsideStaff", "aboveSymbolAlt", "belowSymbolAlt", "insideSlur", "noPrint", "autoStack", "centerOnStem", "slurInteractionMode",
              "charAlt", "fontAlt.fontId", "fontAlt.fontSize", "fontAlt.bold", "fontAlt.italic", "fontAlt.underline", "fontAlt.strikeout",
              "fontAlt.absolute", "fontAlt.hidden", "xOffsetMain", "yOffsetMain", "defVertPos", "avoidStaffLines", "isStemSideWhenMultipleLayers",
              "playArtic", "xOffsetAlt", "yOffsetAlt", "mainIsShape", "altIsShape", "mainShape", "altShape", "startTopNoteDelta", "startBotNoteDelta",
              "startTopNotePercent", "startBotNotePercent", "durTopNoteDelta", "durBotNoteDelta", "durTopNotePercent", "durBotNotePercent",
              "ampTopNoteDelta", "ampBotNoteDelta", "ampTopNotePercent", "ampBotNotePercent", "distanceFromStemEnd"});
}

TEST_CASE("Pre-3.3 articulation definitions convert three-row playback", "[class][articulation-def]")
{
    const auto parsed = makeContainer({{1, "IX", {65, 6144, 25, 75, 0, 0x0518}}, {1, "IX", {65, 6144, 2, -3, 14, 1}}, {1, "IX", {0, 0, 4, -5, 0, 0}}},
        FormatEpoch::UncompressedLegacy, ByteOrder::BigEndian);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    const auto index = LegacyRecordIndex::build(parsed);
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    profile.version = SourceVersion{.major = 3, .minor = 2};
    ImportReport report(profile.epoch);
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importArticulationDefs(context);
    const auto definition = document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(definition);
    CHECK(definition->charMain == U'A');
    CHECK(definition->xOffsetMain == 2);
    CHECK(definition->yOffsetMain == -3);
    CHECK(definition->xOffsetAlt == 4);
    CHECK(definition->yOffsetAlt == -5);
    CHECK(definition->playArtic);
    CHECK_FALSE(definition->noPrint);
    CHECK(definition->durTopNotePercent == 75);
    CHECK(definition->durBotNotePercent == 25);
    CHECK(definition->durTopNoteDelta == 0);
    CHECK(definition->startTopNotePercent == 0);
    const auto* top = report.findField<Definition>("durTopNotePercent", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(top);
    CHECK(top->origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(top->rawValue == 75);
    CHECK(report.findField<Definition>("durTopNoteDelta", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyBehavior);
    CHECK(report.findField<Definition>("startTopNotePercent", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyBehavior);
    CHECK(report.findField<Definition>("playArtic", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMusAdjusted);
}

TEST_CASE("Single-row classic articulation definitions are read as four rows", "[class][articulation-def]")
{
    for (const auto minor : {std::uint8_t(2), std::uint8_t(5)}) {
        const bool before33 = minor < 3;
        const auto parsed = makeContainer({{1, "IX", {46, 6144, 0, 0, 0, 0x0018}}}, FormatEpoch::UncompressedLegacy, ByteOrder::BigEndian);
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        auto referenceSession = musx::factory::DocumentFactory::begin();
        const auto reference = std::move(referenceSession).finish();
        const auto index = LegacyRecordIndex::build(parsed);
        SourceProfile profile(parsed.formatEpoch);
        profile.byteOrder = parsed.byteOrder;
        profile.version = SourceVersion{.major = 3, .minor = minor};
        ImportReport report(profile.epoch);
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::others::importArticulationDefs(context);
        CHECK(report.diagnostics.empty());
        const auto definition = document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(definition);
        CHECK(definition->charMain == U'.');
        CHECK(definition->fontMain->fontSize == 24);
        // Before Finale 3.3 the absent second row takes the main symbol and default vertical position.
        CHECK(definition->charAlt == (before33 ? U'.' : U'\0'));
        CHECK(definition->fontAlt->fontSize == (before33 ? 24 : 0));
        CHECK(definition->defVertPos == (before33 ? 24 : 0));
        CHECK(definition->xOffsetAlt == 0);
        const auto* vertical = report.findField<Definition>("defVertPos", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(vertical);
        CHECK(vertical->origin == ValueOrigin::LegacyBehavior);
        CHECK(report.findField<Definition>("charMain", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
    }
}

TEST_CASE("Coda three-row articulation definitions retain placement and convert playback", "[class][articulation-def]")
{
    const auto parsed =
        makeContainer({{1, "IX", {65, 6144, 0x0205, 0, 0, 0x0b18}}, {1, "IX", {66, 6144, 7, -3, 0, 1}}, {1, "IX", {0, 0, 5, -6, 0, 0}}},
            FormatEpoch::CodaBanner, ByteOrder::BigEndian);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    const auto index = LegacyRecordIndex::build(parsed);
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    ImportReport report(profile.epoch);
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importArticulationDefs(context);
    const auto definition = document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(definition);
    CHECK(definition->charMain == U'A');
    CHECK(definition->charAlt == U'B');
    CHECK(definition->autoHorz);
    CHECK(definition->autoVert);
    CHECK(definition->xOffsetMain == 7);
    CHECK(definition->yOffsetMain == -3);
    CHECK(definition->defVertPos == 0);
    CHECK(definition->avoidStaffLines);
    CHECK(definition->xOffsetAlt == 5);
    CHECK(definition->yOffsetAlt == -6);
    CHECK(definition->playArtic);
    CHECK(definition->durTopNotePercent == 40);
    CHECK(definition->durBotNotePercent == 40);
    CHECK_FALSE(definition->noPrint);
    CHECK_FALSE(definition->insideSlur);
    CHECK(report.findField<Definition>("charAlt", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
    CHECK(report.findField<Definition>("defVertPos", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
    CHECK(report.findField<Definition>("durTopNotePercent", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMusAdjusted);
}

TEST_CASE("Short articulation class records retain stored placement", "[class][articulation-def]")
{
    const auto parsed =
        makeClassContainer(0x0079, {65, 6144, 0, 0, 0, 0x0018, 65, 6144, 9, -12, 24, 0, 0, 0, 3, -4, 0, 0}, ByteOrder::LittleEndian, 1);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    const auto index = LegacyRecordIndex::build(parsed);
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    ImportReport report(profile.epoch);
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importArticulationDefs(context);
    const auto definition = document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(definition);
    CHECK(definition->charMain == U'A');
    CHECK(definition->xOffsetMain == 9);
    CHECK(definition->yOffsetMain == -12);
    CHECK(definition->defVertPos == 24);
    CHECK(definition->xOffsetAlt == 3);
    CHECK(definition->yOffsetAlt == -4);
    CHECK_FALSE(definition->playArtic);
    CHECK(report.findField<Definition>("startTopNoteDelta", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyBehavior);
}

TEST_CASE("Finale 2012 articulation symbols use wide code points", "[class][articulation-def]")
{
    const auto result = readFixture("evidence/F2012/F2012-noteartexp.mus");
    const auto definition = result.document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(definition);
    CHECK(definition->charMain == U'>');
    CHECK(definition->charAlt == U'>');
    CHECK(definition->fontMain->fontSize == 24);
    CHECK(definition->fontAlt->fontSize == 24);
    CHECK(definition->autoVert);
    CHECK(definition->autoHorz);
    CHECK(definition->autoVertMode == Definition::AutoVerticalMode::AboveEntry);
    CHECK(definition->outsideStaff);
    CHECK(definition->avoidStaffLines);
    CHECK(definition->slurInteractionMode == Definition::SlurInteractionMode::Ignore);
    CHECK(definition->defVertPos == 24);
    CHECK(result.report.findField<Definition>("charMain", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
    CHECK(result.report.findField<Definition>("copyMode", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
    CHECK(result.report.findField<Definition>("distanceFromStemEnd", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::MusxOnly);
}

TEST_CASE("Finale 2011 class articulation retains the four-incidence layout", "[class][articulation-def]")
{
    const auto result = readFixture("evidence/F2011/F2011-perc-instchange.mus");
    const auto definition = result.document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(definition);
    CHECK(definition->charMain == U'.');
    CHECK(definition->charAlt == U'.');
    CHECK(definition->fontMain->fontSize == 24);
    CHECK(definition->yOffsetMain == -4);
    CHECK(definition->defVertPos == 16);
    CHECK(result.report.findField<Definition>("fontMain.fontSize", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Wide articulation layout keeps 32-bit symbols and independent font words", "[class][articulation-def]")
{
    const auto parsed = makeClassContainer(0x0079,
        {1, -2560, 7, 28, 0x0005, -20532, 1, -2559, 8, -6, 12, 24, 0x007f, 18, 0x0002, -4, 6, 10, 11, -20, 30, 60, 70, 110, 120, 0, 0, 0, 0, 0},
        ByteOrder::LittleEndian, 1);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    const auto index = LegacyRecordIndex::build(parsed);
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    ImportReport report(profile.epoch);
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importArticulationDefs(context);
    const auto definition = document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(definition);
    CHECK(definition->charMain == U'😀');
    CHECK(definition->charAlt == U'😁');
    CHECK(definition->fontMain->fontSize == 28);
    CHECK(definition->fontMain->bold);
    CHECK(definition->fontMain->underline);
    CHECK(definition->fontAlt->fontSize == 18);
    CHECK(definition->fontAlt->italic);
    CHECK(definition->autoVertMode == Definition::AutoVerticalMode::AutoNoteStem);
    CHECK(definition->insideSlur);
    CHECK(definition->noPrint);
    CHECK_FALSE(definition->autoStack);
    CHECK_FALSE(definition->centerOnStem);
    CHECK(definition->slurInteractionMode == Definition::SlurInteractionMode::InsideSlur);
    CHECK(definition->xOffsetMain == -6);
    CHECK(definition->yOffsetMain == 12);
    CHECK(definition->xOffsetAlt == -4);
    CHECK(definition->yOffsetAlt == 6);
    CHECK(definition->mainShape == 10);
    CHECK(definition->altShape == 11);
    CHECK(definition->startTopNotePercent == -20);
    CHECK(definition->ampBotNotePercent == 120);
    const auto* mainChar = report.findField<Definition>("charMain", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(mainChar);
    CHECK(mainChar->rawValue == 0x1f600);
    CHECK(report.findField<Definition>("autoStack", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::MusxOnly);
}

TEST_CASE("Coda articulation definitions copy the main symbol and font to the alternate", "[class][articulation-def]")
{
    const auto result = readFixture("evidence/F263/F263-baseline.mus");
    const auto definition = result.document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(definition);
    CHECK(definition->charMain == U'.');
    CHECK(definition->fontMain->fontSize == 24);
    CHECK(definition->charAlt == definition->charMain);
    REQUIRE(definition->fontAlt);
    CHECK(definition->fontAlt->fontId == definition->fontMain->fontId);
    CHECK(definition->fontAlt->fontSize == definition->fontMain->fontSize);
    CHECK(definition->fontAlt->bold == definition->fontMain->bold);
    CHECK(definition->fontAlt->italic == definition->fontMain->italic);
    CHECK(definition->fontAlt->underline == definition->fontMain->underline);
    CHECK(definition->fontAlt->strikeout == definition->fontMain->strikeout);
    CHECK(definition->fontAlt->absolute == definition->fontMain->absolute);
    CHECK(definition->fontAlt->hidden == definition->fontMain->hidden);
    CHECK(definition->defVertPos == 24);
    CHECK(definition->xOffsetMain == 0);
    CHECK(definition->yOffsetMain == 0);
    CHECK(definition->xOffsetAlt == 0);
    CHECK(definition->yOffsetAlt == 0);
    CHECK(definition->durTopNotePercent == 40);
    CHECK(definition->durBotNotePercent == 40);
    CHECK(definition->ampTopNoteDelta == 0);
    CHECK(definition->ampBotNoteDelta == 0);
    CHECK_FALSE(definition->insideSlur);
    CHECK(definition->slurInteractionMode == Definition::SlurInteractionMode::Ignore);
    CHECK_FALSE(definition->autoVert);
    CHECK_FALSE(definition->noPrint);
    CHECK_FALSE(definition->avoidStaffLines);
    CHECK(result.report.findField<Definition>("charMain", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
    CHECK(result.report.findField<Definition>("copyMode", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
    CHECK(result.report.findField<Definition>("useTopNote", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
    CHECK(result.report.findField<Definition>("insideSlur", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyBehavior);
    CHECK(result.report.findField<Definition>("slurInteractionMode", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin
          == ValueOrigin::LegacyBehavior);
    CHECK(result.report.findField<Definition>("autoVert", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyBehavior);
    CHECK(result.report.findField<Definition>("noPrint", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyBehavior);
    for (const auto* field : {"defVertPos", "xOffsetMain", "yOffsetMain", "xOffsetAlt", "yOffsetAlt", "ampTopNoteDelta", "ampBotNoteDelta"}) {
        const auto* entry = result.report.findField<Definition>(field, musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(entry);
        CHECK(entry->origin == ValueOrigin::LegacyBehavior);
    }
    CHECK(
        result.report.findField<Definition>("avoidStaffLines", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyBehavior);
    CHECK(result.report.findField<Definition>("durTopNotePercent", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin
          == ValueOrigin::LegacyMusAdjusted);
    CHECK(result.report.findField<Definition>("durBotNotePercent", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin
          == ValueOrigin::LegacyMusAdjusted);
    CHECK(definition->playArtic);
    CHECK(result.report.findField<Definition>("playArtic", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
    CHECK(result.report.findField<Definition>("charAlt", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMusAdjusted);
    for (const auto* field : {"fontAlt.fontId", "fontAlt.fontSize", "fontAlt.bold", "fontAlt.italic", "fontAlt.underline", "fontAlt.strikeout",
             "fontAlt.absolute", "fontAlt.hidden"}) {
        const auto* entry = result.report.findField<Definition>(field, musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(entry);
        CHECK(entry->origin == ValueOrigin::LegacyMusAdjusted);
    }
}

TEST_CASE("Coda articulation playback values decode from the first row", "[class][articulation-def]")
{
    const auto baseline = readFixture("evidence/F100/F100-artic.mus");
    const auto changed = readFixture("evidence/F100/F100-artic-playback.mus");
    const auto before = baseline.document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    const auto after = changed.document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(before);
    REQUIRE(after);
    CHECK_FALSE(before->playArtic);
    CHECK(before->ampTopNoteDelta == 0);
    CHECK(before->ampBotNoteDelta == 0);
    CHECK(after->playArtic);
    CHECK(after->ampTopNoteDelta == 5);
    CHECK(after->ampBotNoteDelta == 3);
    CHECK(after->durTopNotePercent == 0);
    CHECK(after->durBotNotePercent == 0);
    const auto* top = changed.report.findField<Definition>("ampTopNoteDelta", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    const auto* bottom = changed.report.findField<Definition>("ampBotNoteDelta", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(top);
    REQUIRE(bottom);
    CHECK(top->origin == ValueOrigin::LegacyMus);
    CHECK(bottom->origin == ValueOrigin::LegacyMus);
    CHECK(top->rawValue == 5);
    CHECK(bottom->rawValue == 3);

    const auto timeBased = readFixture("evidence/F100/F100-artic-playtime.mus");
    const auto timed = timeBased.document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(timed);
    CHECK(timed->playArtic);
    CHECK(timed->startTopNoteDelta == 5);
    CHECK(timed->startBotNoteDelta == 3);
    CHECK(timed->ampTopNoteDelta == 0);
    CHECK(timed->ampBotNoteDelta == 0);
    const auto* timeTop = timeBased.report.findField<Definition>("startTopNoteDelta", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    const auto* timeBottom = timeBased.report.findField<Definition>("startBotNoteDelta", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(timeTop);
    REQUIRE(timeBottom);
    CHECK(timeTop->origin == ValueOrigin::LegacyMus);
    CHECK(timeBottom->origin == ValueOrigin::LegacyMus);
    CHECK(timeTop->rawValue == 5);
    CHECK(timeBottom->rawValue == 3);

    const auto duration = readFixture("evidence/F100/F100-artic-playtime-altdur.mus");
    const auto altered = duration.document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(altered);
    CHECK(altered->playArtic);
    CHECK(altered->durTopNoteDelta == 5);
    CHECK(altered->durBotNoteDelta == 3);
    CHECK(altered->startTopNoteDelta == 0);
    CHECK(altered->startBotNoteDelta == 0);
    const auto* durTop = duration.report.findField<Definition>("durTopNoteDelta", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    const auto* durBottom = duration.report.findField<Definition>("durBotNoteDelta", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(durTop);
    REQUIRE(durBottom);
    CHECK(durTop->origin == ValueOrigin::LegacyMus);
    CHECK(durBottom->origin == ValueOrigin::LegacyMus);
    CHECK(durTop->rawValue == 5);
    CHECK(durBottom->rawValue == 3);

    const auto fraction = readFixture("evidence/F100/F100-artic-playtime-altfrac.mus");
    const auto fractional = fraction.document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(fractional);
    CHECK(fractional->playArtic);
    CHECK(fractional->durTopNotePercent == 43);
    CHECK(fractional->durBotNotePercent == 43);
    CHECK(fractional->durTopNoteDelta == 0);
    CHECK(fractional->durBotNoteDelta == 0);
    const auto* percentTop = fraction.report.findField<Definition>("durTopNotePercent", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    const auto* percentBottom = fraction.report.findField<Definition>("durBotNotePercent", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(percentTop);
    REQUIRE(percentBottom);
    CHECK(percentTop->origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(percentBottom->origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(percentTop->rawValue == 0x0307);
    CHECK(percentBottom->rawValue == 0x0307);

    const auto negative = readFixture("evidence/F100/F100-artic-playneg.mus");
    const auto negativeDef = negative.document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(negativeDef);
    CHECK(negativeDef->playArtic);
    CHECK(negativeDef->ampTopNotePercent == 2500);
    CHECK(negativeDef->ampBotNotePercent == 2500);
    const auto* negativeTop = negative.report.findField<Definition>("ampTopNotePercent", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(negativeTop);
    CHECK(negativeTop->origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(negativeTop->rawValue == 0x7d05);

    const auto invalid = readFixture("evidence/F100/F100-artic-playinf.mus");
    const auto invalidDef = invalid.document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(invalidDef);
    CHECK(invalidDef->playArtic);
    CHECK(invalidDef->ampTopNoteDelta == 7);
    CHECK(invalidDef->ampBotNoteDelta == 32000);
    CHECK(invalidDef->ampTopNotePercent == 0);
    CHECK(invalidDef->ampBotNotePercent == 0);
    const auto* invalidBottom = invalid.report.findField<Definition>("ampBotNoteDelta", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(invalidBottom);
    CHECK(invalidBottom->origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(invalidBottom->rawValue == 0x7d00);
}

TEST_CASE("Coda articulation clone and top-note options decode from their flags", "[class][articulation-def]")
{
    const auto baseline = readFixture("evidence/F100/F100-artic.mus");
    const auto cloned = readFixture("evidence/F100/F100-artic-clone.mus");
    const auto horizontal = readFixture("evidence/F100/F100-artic-clonehorz.mus");
    const auto topNote = readFixture("evidence/F100/F100-artic-topnote.mus");
    const auto before = baseline.document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    const auto withClone = cloned.document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    const auto withHorizontal = horizontal.document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    const auto withTopNote = topNote.document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(before);
    REQUIRE(withClone);
    REQUIRE(withHorizontal);
    REQUIRE(withTopNote);
    CHECK(before->copyMode == Definition::CopyMode::None);
    CHECK_FALSE(before->useTopNote);
    CHECK(withClone->copyMode == Definition::CopyMode::Vertical);
    CHECK_FALSE(withClone->useTopNote);
    CHECK(withHorizontal->copyMode == Definition::CopyMode::Horizontal);
    CHECK_FALSE(withHorizontal->useTopNote);
    CHECK(withTopNote->copyMode == Definition::CopyMode::None);
    CHECK(withTopNote->useTopNote);
}

TEST_CASE("Coda duration fraction with zero denominator uses raw deltas", "[class][articulation-def]")
{
    const auto parsed = makeContainer({{1, "IX", {62, 18176, 32000, 7, 0, 0x0f00}}}, FormatEpoch::CodaBanner, ByteOrder::BigEndian);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    const auto index = LegacyRecordIndex::build(parsed);
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    ImportReport report(profile.epoch);
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importArticulationDefs(context);
    const auto definition = document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(definition);
    CHECK(definition->durTopNoteDelta == 7);
    CHECK(definition->durBotNoteDelta == 32000);
    CHECK(definition->durTopNotePercent == 0);
    CHECK(definition->durBotNotePercent == 0);
    const auto* bottom = report.findField<Definition>("durBotNoteDelta", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(bottom);
    CHECK(bottom->origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(bottom->rawValue == 32000);
}

TEST_CASE("Articulation flags and playback percentages decode from their stored words", "[class][articulation-def]")
{
    const auto parsed = makeContainer({{1, "IX", {0x0458, 0x1802, 0, 0, 0, std::int16_t(0x73fa)}}, {1, "IX", {65, 0x0c00, -7, 11, 24, 0x007f}},
                                          {1, "IX", {0, 0, -3, 4, 9, 10}}, {1, "IX", {-25, 20, 50, 80, 110, 120}}},
        FormatEpoch::UncompressedLegacy, ByteOrder::LittleEndian);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    const auto index = LegacyRecordIndex::build(parsed);
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    ImportReport report(profile.epoch);
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importArticulationDefs(context);
    const auto definition = document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(definition);
    CHECK(definition->charMain == U'X');
    CHECK(definition->fontMain->underline);
    CHECK(definition->copyMode == Definition::CopyMode::Horizontal);
    CHECK(definition->useTopNote);
    CHECK(definition->autoHorz);
    CHECK(definition->autoVert);
    CHECK(definition->insideSlur);
    CHECK(definition->noPrint);
    CHECK(report.findField<Definition>("noPrint", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
    CHECK(definition->slurInteractionMode == Definition::SlurInteractionMode::InsideSlur);
    CHECK(definition->autoVertMode == Definition::AutoVerticalMode::AboveEntry);
    CHECK(definition->outsideStaff);
    CHECK(definition->aboveSymbolAlt);
    CHECK(definition->belowSymbolAlt);
    CHECK(definition->xOffsetMain == -7);
    CHECK(definition->xOffsetAlt == -3);
    CHECK(definition->mainIsShape);
    CHECK(definition->altIsShape);
    CHECK(definition->mainShape == 9);
    CHECK(definition->altShape == 10);
    CHECK(definition->startTopNotePercent == -25);
    CHECK(definition->startTopNoteDelta == 0);
    CHECK(definition->durBotNotePercent == 80);
    CHECK(definition->ampBotNotePercent == 120);
    CHECK(report.findField<Definition>("startTopNoteDelta", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyBehavior);
}

} // namespace
} // namespace finale_mus_reader_tests
