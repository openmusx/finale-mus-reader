// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using TieOptions = musx::dom::options::TieOptions;

std::shared_ptr<const TieOptions> tieOptions(const ImportResult& result)
{
    return result.document->getOptions()->get<TieOptions>();
}

void expectCompletePostCodaTieOptions(const ImportResult& result, const char* label)
{
    const auto options = tieOptions(result);
    expect(options && options->tieConnectStyles.size() == 12 && options->tieControlStyles.size() == 4,
        std::string(label).append(" did not recover both complete tie style collections"));
    expect(field(result, "options.tieOptions.thicknessRight").origin == ValueOrigin::LegacyMus
               && field(result, "options.tieOptions.tieConnectStyles[11].offsetY").origin == ValueOrigin::LegacyMus
               && field(result, "options.tieOptions.tieControlStyles[3].cp2.insetFixed").origin == ValueOrigin::LegacyMus,
        std::string(label).append(" reported a recovered tie field with the wrong origin"));
}

void testTieOptionsEpochFixtures()
{
    const auto coda = readFixture("evidence/F263/F263-baseline.mus");
    const auto codaOptions = tieOptions(coda);
    expect(codaOptions && codaOptions->tieConnectStyles.size() == 12 && codaOptions->tieControlStyles.size() == 4,
        "The Coda epoch did not retain the complete pinned TieOptions "
        "collections");
    expect(field(coda, "options.tieOptions.thicknessRight").origin == ValueOrigin::LegacyMusAdjusted
               && field(coda, "options.tieOptions.tieConnectStyles[0].offsetX").origin == ValueOrigin::Finale27Default
               && codaOptions->specialPosMode == TieOptions::SpecialPosMode::None && codaOptions->sysBreakLeftHAdj == 0
               && field(coda, "options.tieOptions.specialPosMode").origin == ValueOrigin::LegacyBehavior
               && field(coda, "options.tieOptions.sysBreakLeftHAdj").origin == ValueOrigin::LegacyBehavior
               && codaOptions->tieControlStyles.at(TieOptions::ControlStyleType::ShortSpan)->cp1->insetRatio == 512
               && field(coda, "options.tieOptions.tieControlStyles[0].cp1.insetRatio").origin == ValueOrigin::LegacyBehavior
               && codaOptions->frontTieSepar == 0 && field(coda, "options.tieOptions.frontTieSepar").origin == ValueOrigin::LegacyBehavior,
        "The Coda TieOptions fields reported the wrong fallback origins");

    const auto finale100 = readFixture("evidence/F100/F100-tieopts.mus");
    const auto finale100Options = tieOptions(finale100);
    expect(finale100Options->thicknessLeft == 9 && finale100Options->thicknessRight == 11
               && finale100Options->tieConnectStyles.at(musx::dom::TieConnectStyleType::OverStartPosInner)->offsetX == 1
               && finale100Options->tieConnectStyles.at(musx::dom::TieConnectStyleType::OverEndPosInner)->offsetX == 3
               && finale100Options->tieConnectStyles.at(musx::dom::TieConnectStyleType::OverHighestNoteStemStartPosOver)->offsetX == 1
               && finale100Options->tieConnectStyles.at(musx::dom::TieConnectStyleType::OverHighestNoteStemEndPosOver)->offsetX == 3,
        "The Finale 1.0 tie scalar and horizontal-placement fields decoded "
        "incorrectly");
    expect(field(finale100, "options.tieOptions.thicknessLeft").origin == ValueOrigin::LegacyMusAdjusted
               && field(finale100, "options.tieOptions.tieConnectStyles[0].offsetX").origin == ValueOrigin::LegacyMus
               && field(finale100, "options.tieOptions.tieConnectStyles[0].offsetY").origin == ValueOrigin::LegacyMusAdjusted
               && field(finale100, "options.tieOptions.frontTieSepar").origin == ValueOrigin::LegacyBehavior,
        "The Finale 1.0 tie fields reported the wrong origins");

    const auto finale37 = readFixture("evidence/F372/F372-activelayer-only.mus");
    expect(field(finale37, "options.tieOptions.thicknessRight").origin == ValueOrigin::Finale27Default,
        "Finale 3.7 synthesized a physically absent tie thickness");
    expect(tieOptions(finale37)->tieControlStyles.at(TieOptions::ControlStyleType::ShortSpan)->span == 48
               && field(finale37, "options.tieOptions.tieControlStyles[0].span").origin == ValueOrigin::LegacyBehavior,
        "Finale 3.7 did not apply its fixed short-tie span");
    expect(tieOptions(finale37)->breakForTimeSigs == false
               && tieOptions(finale37)->mixedStemDirection == TieOptions::MixedStemDirection::OppositeFirst && tieOptions(finale37)->useTieEndCtlStyle
               && field(finale37, "options.tieOptions.breakForTimeSigs").origin == ValueOrigin::LegacyBehavior,
        "Finale 3.7 did not apply its invariant unmapped tie behavior");
    expect(tieOptions(finale37)->frontTieSepar == 0 && field(finale37, "options.tieOptions.frontTieSepar").origin == ValueOrigin::LegacyBehavior,
        "Finale 3.7 did not apply its unstored front-tie separation");

    const auto uncompressed = readFixture("evidence/F2000/F2000-empty.mus");
    expectCompletePostCodaTieOptions(uncompressed, "The uncompressed epoch");
    expect(tieOptions(uncompressed)->thicknessRight == 6 && tieOptions(uncompressed)->frontTieSepar == 0
               && tieOptions(uncompressed)->tieConnectStyles.at(musx::dom::TieConnectStyleType::OverStartPosInner)->offsetX == 8
               && tieOptions(uncompressed)->tieControlStyles.at(TieOptions::ControlStyleType::ShortSpan)->span == 174,
        "The uncompressed TieOptions layout decoded the wrong values");
    expect(field(uncompressed, "options.tieOptions.frontTieSepar").origin == ValueOrigin::LegacyBehavior,
        "The absent uncompressed extra-system field did not report legacy "
        "behavior");

    const auto dcl = readFixture("evidence/F2005/F2005-baseline.mus");
    expectCompletePostCodaTieOptions(dcl, "The DCL epoch");
    expect(tieOptions(dcl)->frontTieSepar == -12 && tieOptions(dcl)->specialPosMode == TieOptions::SpecialPosMode::Avoid
               && tieOptions(dcl)->tieTipWidth == 1.0,
        "The DCL TieOptions scalar transforms decoded incorrectly");

    const auto zlib = readFixture("evidence/F2008/F2008-empty.mus");
    expectCompletePostCodaTieOptions(zlib, "The zlib epoch");
    expect(tieOptions(zlib)->frontTieSepar == -12 && tieOptions(zlib)->tieControlStyles.at(TieOptions::ControlStyleType::LongSpan)->span == 576
               && tieOptions(zlib)->tieTipWidth == 1.0,
        "The zlib TieOptions layout decoded the wrong values");
}

void testControlledTieChanges()
{
    const auto earlyContour = readFixture("evidence/F100/F100-tieheight-inset.mus");
    const auto shortEarlyContour = tieOptions(earlyContour)->tieControlStyles.at(TieOptions::ControlStyleType::ShortSpan);
    const auto mediumEarlyContour = tieOptions(earlyContour)->tieControlStyles.at(TieOptions::ControlStyleType::MediumSpan);
    expect(shortEarlyContour->cp1->height == 4 && shortEarlyContour->cp2->height == 4 && mediumEarlyContour->cp1->height == 9
               && mediumEarlyContour->cp2->height == 9,
        "The controlled Finale 1.0 tie height was not expanded into the early "
        "contours");
    expect(shortEarlyContour->cp1->insetFixed == 8
               && field(earlyContour, "options.tieOptions.tieControlStyles[0].cp1.height").origin == ValueOrigin::LegacyMusAdjusted
               && field(earlyContour, "options.tieOptions.tieControlStyles[0].cp1.insetFixed").origin == ValueOrigin::LegacyMusAdjusted,
        "The controlled Finale 1.0 contour fields reported the wrong origins");
    for (std::size_t index = 0; index < 4; ++index) {
        const auto style = tieOptions(earlyContour)->tieControlStyles.at(static_cast<TieOptions::ControlStyleType>(index));
        expect(style->cp1->insetRatio == 512 && style->cp2->insetRatio == 512 && style->cp1->insetFixed == 8 && style->cp2->insetFixed == 8
                   && field(earlyContour, "options.tieOptions.tieControlStyles[" + std::to_string(index) + "].cp1.insetRatio").origin
                          == ValueOrigin::LegacyBehavior
                   && field(earlyContour, "options.tieOptions.tieControlStyles[" + std::to_string(index) + "].cp2.insetRatio").origin
                          == ValueOrigin::LegacyBehavior
                   && field(earlyContour, "options.tieOptions.tieControlStyles[" + std::to_string(index) + "].cp1.insetFixed").origin
                          == (index < 3 ? ValueOrigin::LegacyMusAdjusted : ValueOrigin::Finale27Default)
                   && field(earlyContour, "options.tieOptions.tieControlStyles[" + std::to_string(index) + "].cp2.insetFixed").origin
                          == (index < 3 ? ValueOrigin::LegacyMusAdjusted : ValueOrigin::Finale27Default),
            "A Coda tie inset did not receive the correct fallback");
    }

    struct EarlyCurveExpected
    {
        const char* fixture;
        int leftThickness;
        int rightThickness;
        int leftHeight;
        int rightHeight;
    };
    constexpr std::array earlyCurveExpectations{
        EarlyCurveExpected{"evidence/F263/F263-curve-opt.mus", 13, 17, 37, 41},
        EarlyCurveExpected{"evidence/F263/F263-curve-opt-2.mus", -17, -19, 12, 10},
        EarlyCurveExpected{"evidence/F263/F263-curve-opt-3.mus", 171, -179, 155, -149},
    };
    for (const auto& expected : earlyCurveExpectations) {
        const auto result = readFixture(expected.fixture);
        const auto options = tieOptions(result);
        const auto medium = options->tieControlStyles.at(TieOptions::ControlStyleType::MediumSpan);
        expect(options->thicknessLeft == expected.leftThickness && options->thicknessRight == expected.rightThickness
                   && medium->cp1->height == expected.leftHeight && medium->cp2->height == expected.rightHeight,
            std::string(expected.fixture).append(" decoded the early curve fields incorrectly"));
    }

    const auto finale37 = readFixture("evidence/F372/F372-tieopts.mus");
    const auto finale37Options = tieOptions(finale37);
    const auto finale37Short = finale37Options->tieControlStyles.at(TieOptions::ControlStyleType::ShortSpan);
    const auto finale37Medium = finale37Options->tieControlStyles.at(TieOptions::ControlStyleType::MediumSpan);
    expect(finale37Options->tieConnectStyles.at(musx::dom::TieConnectStyleType::OverStartPosInner)->offsetY == 5
               && finale37Options->tieConnectStyles.at(musx::dom::TieConnectStyleType::UnderEndPosInner)->offsetY == -5
               && finale37Short->cp1->height == 8 && finale37Short->cp2->height == 8 && finale37Medium->cp1->height == 17
               && finale37Medium->cp2->height == 17,
        "The Finale 3.7 pre-selector-84 tie positions decoded incorrectly");
    expect(field(finale37, "options.tieOptions.tieConnectStyles[0].offsetY").origin == ValueOrigin::LegacyMusAdjusted
               && field(finale37, "options.tieOptions.tieConnectStyles[3].offsetY").origin == ValueOrigin::LegacyMusAdjusted
               && field(finale37, "options.tieOptions.tieControlStyles[0].cp1.height").origin == ValueOrigin::LegacyMusAdjusted
               && finale37Short->cp1->insetFixed == 8
               && field(finale37, "options.tieOptions.tieControlStyles[0].cp1.insetFixed").origin == ValueOrigin::LegacyMusAdjusted,
        "The Finale 3.7 pre-selector-84 tie positions reported the wrong "
        "origins");

    const auto finale37PostScript = readFixture("evidence/F372/F372-tieopts-ps.mus");
    const auto finale37PostScriptOptions = tieOptions(finale37PostScript);
    const auto finale37PostScriptShort = finale37PostScriptOptions->tieControlStyles.at(TieOptions::ControlStyleType::ShortSpan);
    const auto finale37PostScriptMedium = finale37PostScriptOptions->tieControlStyles.at(TieOptions::ControlStyleType::MediumSpan);
    expect(finale37PostScriptOptions->thicknessLeft == 19 && finale37PostScriptOptions->thicknessRight == 21
               && finale37PostScriptOptions->tieConnectStyles.at(musx::dom::TieConnectStyleType::OverStartPosInner)->offsetX == 9
               && finale37PostScriptOptions->tieConnectStyles.at(musx::dom::TieConnectStyleType::OverEndPosInner)->offsetX == 13
               && finale37PostScriptOptions->tieConnectStyles.at(musx::dom::TieConnectStyleType::OverHighestNoteStemStartPosOver)->offsetX == 9
               && finale37PostScriptOptions->tieConnectStyles.at(musx::dom::TieConnectStyleType::UnderEndPosInner)->offsetY == -21
               && finale37PostScriptShort->cp1->height == 18 && finale37PostScriptShort->cp2->height == 19
               && finale37PostScriptMedium->cp1->height == 37 && finale37PostScriptMedium->cp2->height == 39
               && finale37PostScriptMedium->cp1->insetFixed == 8 && finale37PostScriptMedium->cp2->insetFixed == 30,
        "The Finale 3.7 PostScript tie geometry decoded incorrectly");
    expect(field(finale37PostScript, "options.tieOptions.tieControlStyles[1].cp2.insetFixed").origin == ValueOrigin::LegacyMusAdjusted
               && field(finale37PostScript, "options.tieOptions.tieControlStyles[3].cp1.insetFixed").origin == ValueOrigin::Finale27Default,
        "The Finale 3.7 PostScript tie geometry reported the wrong origins");

    const auto changed = readFixture("evidence/F2000/F2000-tieopts-changed.mus");
    expect(
        tieOptions(changed)->thicknessRight == -17 && tieOptions(changed)->thicknessLeft == 11, "The controlled tie thicknesses were not recovered");
    const auto insets = readFixture("evidence/F2000/F2000-tie-insets.mus");
    const auto shortStyle = tieOptions(insets)->tieControlStyles.at(TieOptions::ControlStyleType::ShortSpan);
    expect(tieOptions(insets)->insetStyle == TieOptions::InsetStyle::Fixed && shortStyle->cp1->insetFixed == 17 && shortStyle->cp2->insetFixed == 13,
        "The controlled tie contour insets were not recovered");

    const auto extraSystem = readFixture("evidence/F2005/F2005-tie-xtrasys.mus");
    expect(tieOptions(extraSystem)->frontTieSepar == -11 && field(extraSystem, "options.tieOptions.frontTieSepar").origin == ValueOrigin::LegacyMus,
        "The controlled extra system separation was not recovered");
}

TEST_CASE("TieOptions recover each located layout and preserve absent-family "
          "fallbacks",
    "[class][reader]")
{
    testTieOptionsEpochFixtures();
}

TEST_CASE("TieOptions recover controlled scalar and contour edits", "[class][reader]")
{
    testControlledTieChanges();
}
} // namespace
} // namespace finale_mus_reader_tests
