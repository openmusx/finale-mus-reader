// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using TupletOptionsTarget = musx::dom::options::TupletOptions;

musx::dom::DocumentPtr makeTupletOptionsDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<TupletOptionsTarget>(document);
    options->displayNumber = 101;
    options->displayDuration = 102;
    options->referenceNumber = 103;
    options->referenceDuration = 104;
    options->alwaysFlat = false;
    options->fullDura = false;
    options->metricCenter = false;
    options->avoidStaff = false;
    options->autoBracketStyle = TupletOptionsTarget::AutoBracketStyle::Always;
    options->tupOffX = 105;
    options->tupOffY = 106;
    options->brackOffX = 107;
    options->brackOffY = 108;
    options->numStyle = TupletOptionsTarget::NumberStyle::Nothing;
    options->posStyle = TupletOptionsTarget::PositioningStyle::Manual;
    options->allowHorz = false;
    options->ignoreHorzNumOffset = false;
    options->breakBracket = false;
    options->matchHooks = false;
    options->useBottomNote = false;
    options->brackStyle = TupletOptionsTarget::BracketStyle::Nothing;
    options->smartTuplet = false;
    options->leftHookLen = 109;
    options->leftHookExt = 110;
    options->rightHookLen = 111;
    options->rightHookExt = 112;
    options->manualSlopeAdj = 113;
    options->tupMaxSlope = 114;
    options->tupLineWidth = 115;
    options->tupNUpstemOffset = 116;
    options->tupNDownstemOffset = 117;
    document->getOptions()->add(TupletOptionsTarget::XmlNodeName, options);
    return std::move(session).finish();
}

std::shared_ptr<const TupletOptionsTarget> importTupletOptions(
    const finale_mus_reader::container::ParsedContainer& parsed, SourceProfile profile, ImportReport& report)
{
    const auto document = makeTupletOptionsDocument();
    const auto reference = makeTupletOptionsDocument();
    profile.byteOrder = parsed.byteOrder;
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{
        LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::options::importTupletOptions(context);
    return document->getOptions()->get<TupletOptionsTarget>();
}

constexpr const char* tupletMembers[] = {"displayNumber", "displayDuration", "referenceNumber", "referenceDuration", "alwaysFlat", "fullDura",
    "metricCenter", "avoidStaff", "autoBracketStyle", "tupOffX", "tupOffY", "brackOffX", "brackOffY", "numStyle", "posStyle", "allowHorz",
    "ignoreHorzNumOffset", "breakBracket", "matchHooks", "useBottomNote", "brackStyle", "smartTuplet", "leftHookLen", "leftHookExt", "rightHookLen",
    "rightHookExt", "manualSlopeAdj", "tupMaxSlope", "tupLineWidth", "tupNUpstemOffset", "tupNDownstemOffset"};

void expectCompleteTupletManifest(const ImportReport& report)
{
    const auto found = report.fields.find(finale_mus_reader::instanceKey<TupletOptionsTarget>());
    expectMapping(found != report.fields.end() && found->second.size() == 31, "TupletOptions did not report exactly its thirty-one musxdom fields");
    for (const auto* member : tupletMembers) {
        expectMapping(fieldPresent(report, std::string("options.tupletOptions.") + member), std::string("TupletOptions omitted field ") + member);
    }
}

const std::vector<SyntheticRow> fixedTupletRows{
    {GLOBALS_CMPER, "56", {3, 512, 2, 512, 0x001f, 11}},
    {GLOBALS_CMPER, "56", {12, 13, 14, static_cast<std::int16_t>(0xdfc3), -15, 16}},
    {GLOBALS_CMPER, "56", {-17, 18, 19, 0, 0, 0}},
    {GLOBALS_CMPER, "14", {0, 0, 0, 0, 20, -21}},
    {GLOBALS_CMPER, "23", {0, 0, 0, 0, 0, 22}},
    {GLOBALS_CMPER, "69", {23, 0, 0, 0, 0, 0}},
};

const std::vector<SyntheticRow> shortUncompressedTupletRows{
    {GLOBALS_CMPER, "56", {32, 4, 18, 4, 0x0303, 8}},
};

const std::vector<SyntheticClassRow> classTupletRows{
    {finale_mus_reader::numericGlobalClass(56), {3, 512, 2, 512, 0x001f, 11, 12, 13, 14, static_cast<std::int16_t>(0xdfc3), -15, 16, -17, 18, 19}},
    {finale_mus_reader::numericGlobalClass(14), {0, 0, 0, 0, 20, -21}},
    {finale_mus_reader::numericGlobalClass(23), {0, 0, 0, 0, 0, 22}},
    {finale_mus_reader::numericGlobalClass(69), {23}},
};

void expectFinale2005Tuplets(const TupletOptionsTarget& options, const ImportReport& report)
{
    expectMapping(
        options.displayNumber == 3 && options.displayDuration == 512 && options.referenceNumber == 2 && options.referenceDuration == 512
            && options.alwaysFlat && options.fullDura && options.metricCenter && options.avoidStaff
            && options.autoBracketStyle == TupletOptionsTarget::AutoBracketStyle::NeverBeamSide && options.tupOffX == 11 && options.tupOffY == 12
            && options.brackOffX == 13 && options.brackOffY == 14 && options.numStyle == TupletOptionsTarget::NumberStyle::RatioPlusBothNotes
            && options.posStyle == TupletOptionsTarget::PositioningStyle::Below && options.allowHorz && options.ignoreHorzNumOffset
            && options.breakBracket && options.matchHooks && options.useBottomNote && options.brackStyle == TupletOptionsTarget::BracketStyle::Slur
            && options.smartTuplet && options.leftHookLen == -15 && options.leftHookExt == 16 && options.rightHookLen == -17
            && options.rightHookExt == 18 && options.manualSlopeAdj == 19 && options.tupMaxSlope == 22 && options.tupLineWidth == 23
            && options.tupNUpstemOffset == 20 && options.tupNDownstemOffset == -21,
        "TupletOptions did not recover the complete Finale 2005 layout");
    expectCompleteTupletManifest(report);
    for (const auto* member : tupletMembers) {
        expectMapping(field(report, std::string("options.tupletOptions.") + member).origin == ValueOrigin::LegacyMus,
            std::string("TupletOptions reported an incorrect origin for ") + member);
    }
}

TEST_CASE("Tuplet options recover the Finale 2005 fixed layout", "[class]")
{
    auto profile = profileFor(10, 0);
    profile.epoch = FormatEpoch::DclLegacy;
    ImportReport report(profile.epoch);
    expectFinale2005Tuplets(*importTupletOptions(makeContainer(fixedTupletRows, profile.epoch), profile, report), report);
}

TEST_CASE("Tuplet options recover class records in either byte order", "[class]")
{
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        auto profile = profileFor(12, 0);
        profile.epoch = FormatEpoch::ZlibLegacy;
        ImportReport report(profile.epoch);
        expectFinale2005Tuplets(*importTupletOptions(makeClassContainer(classTupletRows, byteOrder), profile, report), report);
    }
}

TEST_CASE("Pre-2005 tuplets do not interpret the obsolete word as flags", "[class]")
{
    auto profile = profileFor(5, 0);
    profile.epoch = FormatEpoch::UncompressedLegacy;
    ImportReport report(profile.epoch);
    const auto options = importTupletOptions(makeContainer(fixedTupletRows, profile.epoch), profile, report);

    expectMapping(options->displayNumber == 3 && options->tupOffY == 12 && !options->alwaysFlat && !options->fullDura && !options->metricCenter
                      && !options->avoidStaff && options->autoBracketStyle == TupletOptionsTarget::AutoBracketStyle::UnbeamedOnly
                      && options->smartTuplet && options->tupMaxSlope == 114 && options->tupLineWidth == 23 && options->tupNUpstemOffset == 20
                      && options->tupNDownstemOffset == -21,
        "A pre-2005 tuplet layout used the obsolete word as flags or lost stored fields");
    expectCompleteTupletManifest(report);
    for (const auto* member : {"alwaysFlat", "fullDura", "metricCenter", "tupMaxSlope"}) {
        expectMapping(field(report, std::string("options.tupletOptions.") + member).origin == ValueOrigin::Finale27Default,
            std::string("A pre-2005-only TupletOptions field had the wrong origin: ") + member);
    }
    expectMapping(field(report, "options.tupletOptions.avoidStaff").origin == ValueOrigin::LegacyBehavior,
        "Pre-2005 staff avoidance was not reported as legacy behavior");
    for (const auto* member : {"tupLineWidth", "tupNUpstemOffset", "tupNDownstemOffset"}) {
        expectMapping(field(report, std::string("options.tupletOptions.") + member).origin == ValueOrigin::LegacyMus,
            std::string("A pre-2005 stored TupletOptions field had the wrong origin: ") + member);
    }
}

TEST_CASE("Pre-expanded uncompressed tuplets combine stored flags with era behavior", "[class]")
{
    auto profile = profileFor(3, 0);
    profile.epoch = FormatEpoch::UncompressedLegacy;
    ImportReport report(profile.epoch);
    const auto options = importTupletOptions(makeContainer(shortUncompressedTupletRows, profile.epoch), profile, report);

    expectMapping(options->displayNumber == 32 && options->displayDuration == 4 && options->referenceNumber == 18 && options->referenceDuration == 4
                      && options->alwaysFlat && options->fullDura && options->tupOffX == 8,
        "The pre-expanded uncompressed layout did not recover its stored fields");
    expectMapping(options->autoBracketStyle == TupletOptionsTarget::AutoBracketStyle::Always && options->tupOffY == 0
                      && options->numStyle == TupletOptionsTarget::NumberStyle::Nothing
                      && options->posStyle == TupletOptionsTarget::PositioningStyle::Manual && !options->breakBracket && !options->matchHooks
                      && options->brackStyle == TupletOptionsTarget::BracketStyle::Nothing && !options->smartTuplet && options->leftHookLen == 0
                      && options->rightHookLen == 0 && options->tupLineWidth == 224,
        "The pre-expanded uncompressed layout did not apply its unstored behavior");
    expectCompleteTupletManifest(report);
    for (const auto* member : {"alwaysFlat", "fullDura"}) {
        expectMapping(field(report, std::string("options.tupletOptions.") + member).origin == ValueOrigin::LegacyMus,
            std::string("A stored pre-expanded TupletOptions flag had the wrong origin: ") + member);
    }
    for (const auto* member : {"autoBracketStyle", "tupOffY", "numStyle", "posStyle", "breakBracket", "matchHooks", "brackStyle", "smartTuplet",
             "leftHookLen", "rightHookLen", "tupLineWidth"}) {
        expectMapping(field(report, std::string("options.tupletOptions.") + member).origin == ValueOrigin::LegacyBehavior,
            std::string("An unstored pre-expanded TupletOptions field had the wrong origin: ") + member);
    }
}

TEST_CASE("An incomplete DCL tuplet family does not imitate the pre-expanded layout", "[class]")
{
    auto profile = profileFor(8, 0);
    profile.epoch = FormatEpoch::DclLegacy;
    ImportReport report(profile.epoch);
    const auto options = importTupletOptions(makeContainer(shortUncompressedTupletRows, profile.epoch), profile, report);

    expectMapping(options->displayNumber == 32 && options->tupOffX == 8 && !options->alwaysFlat && !options->fullDura && options->tupOffY == 106
                      && options->leftHookLen == 109 && options->tupLineWidth == 115,
        "An incomplete DCL family crossed into the pre-expanded tuplet layout");
    expectCompleteTupletManifest(report);
}

TEST_CASE("The short Coda tuplet layout recovers only its stored prefix", "[class]")
{
    auto profile = profileFor(2, 6);
    profile.epoch = FormatEpoch::CodaBanner;
    ImportReport report(profile.epoch);
    const auto options = importTupletOptions(makeContainer({fixedTupletRows.front()}, profile.epoch), profile, report);

    expectMapping(options->displayNumber == 3 && options->displayDuration == 512 && options->referenceNumber == 2 && options->referenceDuration == 512
                      && options->tupOffX == 11,
        "The short Coda tuplet layout did not recover its stored prefix");
    expectMapping(options->alwaysFlat && !options->fullDura && options->tupOffY == 0
                      && options->autoBracketStyle == TupletOptionsTarget::AutoBracketStyle::Always,
        "The short Coda tuplet layout crossed its structural boundary");
    expectCompleteTupletManifest(report);
    expectMapping(field(report, "options.tupletOptions.displayNumber").origin == ValueOrigin::LegacyMus
                      && field(report, "options.tupletOptions.tupOffY").origin == ValueOrigin::LegacyBehavior,
        "The short Coda tuplet layout reported incorrect origins");
}

TEST_CASE("Controlled Finale 2005 tuplet options are recovered", "[class]")
{
    const auto fixture = readFixture("evidence/F2005/F2005-baseline.mus");
    const auto options = fixture.document->getOptions()->get<TupletOptionsTarget>();
    expectMapping(options && !options->alwaysFlat && !options->fullDura && options->metricCenter && options->avoidStaff
                      && options->autoBracketStyle == TupletOptionsTarget::AutoBracketStyle::NeverBeamSide && options->tupOffX == 0
                      && options->tupOffY == 24 && options->numStyle == TupletOptionsTarget::NumberStyle::Number
                      && options->posStyle == TupletOptionsTarget::PositioningStyle::BeamSide && !options->allowHorz && options->ignoreHorzNumOffset
                      && options->breakBracket && options->matchHooks && !options->useBottomNote
                      && options->brackStyle == TupletOptionsTarget::BracketStyle::Bracket && options->smartTuplet && options->leftHookLen == -12
                      && options->rightHookLen == -12 && options->tupMaxSlope == 100 && options->tupLineWidth == 224
                      && options->tupNUpstemOffset == 12 && options->tupNDownstemOffset == 0,
        "The controlled Finale 2005 tuplet preferences were not recovered exactly");
}

} // namespace
} // namespace finale_mus_reader_tests
