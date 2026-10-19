// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"
#include <catch2/generators/catch_generators.hpp>

namespace finale_mus_reader_tests {
namespace {
using namespace classes;
using TestExpression = musx::dom::others::TextExpressionDef;
using TestExpressionEnclosure = musx::dom::others::TextExpressionEnclosure;

TEST_CASE("Text-expression breakMmRest begins in F2002", "[class][text-expression]")
{
    using namespace finale_mus_reader;
    for (const auto version : {versions::finale2001, versions::finale2002}) {
        auto parsed = makeContainer({{1, "DT", {0, 0, 0, 0, 0, 0x0400}}}, FormatEpoch::DclLegacy);
        auto profile = profileFor(version.major);
        profile.epoch = FormatEpoch::DclLegacy;
        const auto index = LegacyRecordIndex::build(parsed);
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        ImportReport report(profile.epoch);
        PendingReferences pending;
        const ImportContext context{index, profile, noSource, document, document, report, pending, session.getConstructionContext()};
        others::importTextExpressionDefs(context);
        const auto expression = document->getOthers()->get<TestExpression>(0, 1);
        REQUIRE(expression);
        const bool supported = version.major == versions::finale2002.major;
        CHECK(expression->breakMmRest == supported);
        const auto* field = report.findField<TestExpression>("breakMmRest", 0, musx::dom::Cmper(1));
        REQUIRE(field);
        CHECK(field->origin == (supported ? ValueOrigin::LegacyMus : ValueOrigin::LegacyBehavior));
        CHECK(field->rawValue == (supported ? 0x0400 : 0));
    }
}

TEST_CASE("Text expressions decode Swing from the shared playback flags", "[class][text-expression]")
{
    using namespace finale_mus_reader;
    auto parsed =
        makeContainer({{7, "DT", {0, 0, 1024, 0, 0, 0x090e}}, {7, "DT", {0, 0, 0, 0, 0, 0}}, {7, "DT", {0, 0, 0, 0, 0, 0}}}, FormatEpoch::DclLegacy);
    auto profile = profileFor(versions::finale2006.major);
    profile.epoch = FormatEpoch::DclLegacy;
    const auto index = LegacyRecordIndex::build(parsed);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    ImportReport report(profile.epoch);
    PendingReferences pending;
    const ImportContext context{index, profile, noSource, document, document, report, pending, session.getConstructionContext()};
    others::importTextExpressionDefs(context);
    const auto expression = document->getOthers()->get<TestExpression>(musx::dom::SCORE_PARTID, 7);
    REQUIRE(expression);
    CHECK(expression->playbackType == musx::dom::others::PlaybackType::Swing);
    const auto* playback = report.findField<TestExpression>("playbackType", musx::dom::SCORE_PARTID, musx::dom::Cmper(7));
    REQUIRE(playback);
    CHECK(playback->origin == ValueOrigin::LegacyMus);
    CHECK(playback->rawValue == 0x090e);
}

TEST_CASE("Text expression enclosures decode De rows and zlib class records", "[class][text-expression]")
{
    const auto parsed = makeContainer({{7, "De", {-7, 11, 24, 8, 32, static_cast<std::int16_t>(0x9802)}}});
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
    finale_mus_reader::others::importTextExpressionEnclosures(context);
    const auto enclosure = document->getOthers()->get<TestExpressionEnclosure>(musx::dom::SCORE_PARTID, musx::dom::Cmper(7));
    REQUIRE(enclosure);
    CHECK(enclosure->xAdd == -7);
    CHECK(enclosure->yAdd == 11);
    CHECK(enclosure->xMargin == 24);
    CHECK(enclosure->yMargin == 8);
    CHECK(enclosure->lineWidth == 32);
    CHECK(enclosure->shape == TestExpressionEnclosure::Shape::Ellipse);
    CHECK(enclosure->fixedSize);
    CHECK(enclosure->notTall);
    CHECK(enclosure->opaque);
    CHECK(report.findField<TestExpressionEnclosure>("xAdd", musx::dom::SCORE_PARTID, musx::dom::Cmper(7))->origin == ValueOrigin::LegacyMus);
    CHECK(report.findField<TestExpressionEnclosure>("roundCorners", musx::dom::SCORE_PARTID, musx::dom::Cmper(7))->origin == ValueOrigin::MusxOnly);

    const auto zlib = readFixture("evidence/F2011/F2011-perc-instchange.mus");
    const auto zlibEnclosure = zlib.document->getOthers()->get<TestExpressionEnclosure>(musx::dom::SCORE_PARTID, musx::dom::Cmper(25));
    REQUIRE(zlibEnclosure);
    CHECK(zlibEnclosure->xMargin == 9);
    CHECK(zlibEnclosure->shape == TestExpressionEnclosure::Shape::Rectangle);
}

TEST_CASE("Text expressions recover stored playback and synthesize early text", "[class][text-expression]")
{
    for (const auto path : {"evidence/F263/F263-nodrop-64th.mus", "evidence/F2000/F2000-lyropts-align-just.mus"}) {
        const auto result = readFixture(path);
        const auto expression = result.document->getOthers()->get<TestExpression>(musx::dom::SCORE_PARTID, 1);
        REQUIRE(expression);
        CHECK(expression->value == 88);
        CHECK(expression->playbackType == musx::dom::others::PlaybackType::KeyVelocity);
        CHECK(expression->categoryId == 7);
        CHECK(expression->horzMeasExprAlign == musx::dom::others::HorizontalMeasExprAlign::Manual);
        CHECK(expression->vertMeasExprAlign == musx::dom::others::VerticalMeasExprAlign::Manual);
        const auto* horizontal = result.report.findField<TestExpression>("horzMeasExprAlign", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        const auto* vertical = result.report.findField<TestExpression>("vertMeasExprAlign", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(horizontal);
        REQUIRE(vertical);
        CHECK(horizontal->origin == ValueOrigin::LegacyBehavior);
        CHECK(vertical->origin == ValueOrigin::LegacyBehavior);
        const auto block = expression->getTextBlock();
        REQUIRE(block);
        CHECK(block->textType == musx::dom::others::TextBlock::TextType::Expression);
        const auto raw = result.document->getTexts()->get<musx::dom::texts::ExpressionText>(block->textId);
        REQUIRE(raw);
        CHECK(raw->text.ends_with("f"));
        CHECK(raw->text.find(std::string_view(path).find("F263") != std::string_view::npos ? "^size(24)" : "^size(28)") != std::string::npos);
    }
}

TEST_CASE("F2002 expression hiding is limited to bracketed spans", "[class][text-expression]")
{
    const auto result = readFixture("evidence/F2002/F2002-exp-hidepartial.mus");
    const auto expression = result.document->getOthers()->get<TestExpression>(0, 1);
    REQUIRE(expression);
    const auto raw = expression->getRawTextCtx(0).getRawText();
    REQUIRE(raw);
    CHECK(raw->text == "^font(Times)^size(12)^nfx(0)test is ^nfx(128)hidden^nfx(0) text");
    const auto unbalanced = readFixture("evidence/F2002/F2002-exp-unbalanced.mus");
    const auto unbalancedExpression = unbalanced.document->getOthers()->get<TestExpression>(0, 1);
    REQUIRE(unbalancedExpression);
    const auto unbalancedText = unbalancedExpression->getRawTextCtx(0).getRawText();
    REQUIRE(unbalancedText);
    CHECK(unbalancedText->text == "^font(Times)^size(12)^nfx(0)test is ^nfx(128)hidden?");
}

TEST_CASE("Text-expression no-print conversion begins in F97", "[class][text-expression]")
{
    using namespace finale_mus_reader;
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy}) {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        auto font = std::make_shared<musx::dom::others::FontDefinition>(
            document, musx::dom::Cmper(0), musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper(1));
        font->name = "Times";
        document->getOthers()->add(font->XmlNodeName, font);
        auto parsed = makeContainer(
            {{1, "DT", {static_cast<std::int16_t>(epoch == FormatEpoch::CodaBanner ? 0x010c : 0x0c01), 0, 0, 0, 0, 0x0200}}, {1, "DT", {0x4142}}},
            epoch);
        auto profile = epoch == FormatEpoch::CodaBanner ? profileFor(versions::finale3_7.major, versions::finale3_7.minor)
                                                        : profileFor(versions::finale97.major, versions::finale97.minor);
        profile.epoch = epoch;
        const auto index = LegacyRecordIndex::build(parsed);
        ImportReport report(profile.epoch);
        PendingReferences pending;
        const ImportContext context{index, profile, noSource, document, document, report, pending, session.getConstructionContext()};
        others::importTextExpressionDefs(context);
        runDeferredChecks(pending);
        const auto expression = document->getOthers()->get<TestExpression>(0, 1);
        REQUIRE(expression);
        const auto raw = expression->getRawTextCtx(0).getRawText();
        REQUIRE(raw);
        // From F97 the whole text is one hidden span.
        CHECK(raw->text.ends_with(epoch == FormatEpoch::UncompressedLegacy ? "^nfx(0)^nfx(128)AB^nfx(0)" : "^nfx(0)AB"));
    }
}

TEST_CASE("Inline hidden spans preserve surrounding text and effects", "[class][text-expression]")
{
    using namespace finale_mus_reader;
    for (const auto version : {versions::finale2001, versions::finale2002, versions::finale2003}) {
        for (const bool enabled : {false, true}) {
            auto session = musx::factory::DocumentFactory::begin();
            const auto document = session.getDocument();
            auto font = std::make_shared<musx::dom::others::FontDefinition>(
                document, musx::dom::Cmper(0), musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper(1));
            font->name = "Times";
            document->getOthers()->add(font->XmlNodeName, font);
            const std::string plain = "A>x>>B<y<z>^#<open";
            std::vector<SyntheticRow> rows{{1, "DT", {0x0c01, 1, 0, 0, 0, static_cast<std::int16_t>(enabled ? 0xc200 : 0xc000)}}};
            for (std::size_t offset = 0; offset <= plain.size(); offset += 12) {
                SyntheticRow row{1, "DT", {}};
                for (std::size_t i = 0; i < 12 && offset + i < plain.size(); ++i) {
                    row.words[i / 2] |= static_cast<std::uint16_t>(static_cast<unsigned char>(plain[offset + i])) << (i % 2 ? 0 : 8);
                }
                rows.push_back(row);
            }
            auto parsed = makeContainer(rows, FormatEpoch::DclLegacy);
            auto profile = profileFor(version.major);
            profile.epoch = FormatEpoch::DclLegacy;
            const auto index = LegacyRecordIndex::build(parsed);
            ImportReport report(profile.epoch);
            PendingReferences pending;
            const ImportContext context{index, profile, noSource, document, document, report, pending, session.getConstructionContext()};
            others::importTextExpressionDefs(context);
            runDeferredChecks(pending);
            const auto expression = document->getOthers()->get<TestExpression>(0, 1);
            REQUIRE(expression);
            const auto raw = expression->getRawTextCtx(0).getRawText();
            REQUIRE(raw);
            const bool partial = version.major >= versions::finale2002.major;
            // Brackets nest by depth, so the unmatched '>' run leaves every later '<' visible. Before
            // F2002 the text is wrapped in one hidden span with its brackets escaped; an escaped
            // bracket is literal only outside a span, so the first '>' ends the span.
            const std::string body = !enabled  ? "^nfx(1)A>x>>B<y<z>^^#<open"
                                     : partial ? "^nfx(1)AxByz^^#open"
                                               : "^nfx(1)^nfx(129)A^nfx(1)x>>B<y<z>^^#<open";
            CHECK(raw->text == "^font(Times)^size(12)" + body);
        }
    }
}

TEST_CASE("Inline expression placeholders follow the upgrade's pound rule", "[class][text-expression]")
{
    using namespace finale_mus_reader;
    // Pound modes 0-2 insert value, pass, and control; mode 3 inserts nothing.
    const auto mode = GENERATE(0, 1, 2, 3);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto font = std::make_shared<musx::dom::others::FontDefinition>(
        document, musx::dom::Cmper(0), musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper(1));
    font->name = "Times";
    document->getOthers()->add(font->XmlNodeName, font);
    const std::string plain = "a##b#c#d";
    std::vector<SyntheticRow> rows{{1, "DT", {0x0c01, 0, 0, 0, 0, static_cast<std::int16_t>(mode << 14)}}};
    for (std::size_t offset = 0; offset <= plain.size(); offset += 12) {
        SyntheticRow row{1, "DT", {}};
        for (std::size_t i = 0; i < 12 && offset + i < plain.size(); ++i) {
            row.words[i / 2] |= static_cast<std::uint16_t>(static_cast<unsigned char>(plain[offset + i])) << (i % 2 ? 0 : 8);
        }
        rows.push_back(row);
    }
    auto parsed = makeContainer(rows, FormatEpoch::DclLegacy);
    auto profile = profileFor(versions::finale2002.major);
    profile.epoch = FormatEpoch::DclLegacy;
    const auto index = LegacyRecordIndex::build(parsed);
    ImportReport report(profile.epoch);
    PendingReferences pending;
    const ImportContext context{index, profile, noSource, document, document, report, pending, session.getConstructionContext()};
    others::importTextExpressionDefs(context);
    runDeferredChecks(pending);
    const auto expression = document->getOthers()->get<TestExpression>(0, 1);
    REQUIRE(expression);
    const auto raw = expression->getRawTextCtx(0).getRawText();
    REQUIRE(raw);
    // "##" is a literal '#'; only the first single '#' is replaced.
    const char* inserts[] = {"^value()", "^pass()", "^control()", "#"};
    CHECK(raw->text == std::string("^font(Times)^size(12)^nfx(0)a#b") + inserts[mode] + "c#d");
}

TEST_CASE("Text definitions stored before F2004 take the upgrade's positioning", "[class][text-expression]")
{
    using namespace finale_mus_reader;
    for (const auto* path : {"evidence/F263/F263-clef-baseline.mus", "evidence/F98/F98-baseline.mus"}) {
        const auto result = readFixture(path);
        const auto expression = result.document->getOthers()->get<TestExpression>(musx::dom::SCORE_PARTID, 1);
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
            const auto* field = result.report.findField<TestExpression>(member, musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
            REQUIRE(field);
            CHECK(field->origin == ValueOrigin::LegacyBehavior);
        }
    }
}

TEST_CASE("Expression playback recovers smart playback and percussion map selectors", "[class][text-expression]")
{
    using namespace finale_mus_reader;
    using Playback = musx::dom::others::PlaybackType;
    const auto [stored, expected] = GENERATE(std::pair{std::int16_t(0x10), Playback::SmartPlaybackOn},
        std::pair{std::int16_t(0x11), Playback::SmartPlaybackOff}, std::pair{std::int16_t(0x13), Playback::PercussionMidiMap});
    auto parsed =
        makeContainer({{7, "DT", {0, 0, 0, 0, 0, stored}}, {7, "DT", {0, 0, 0, 0, 0, 0}}, {7, "DT", {0, 0, 0, 0, 0, 0}}}, FormatEpoch::DclLegacy);
    auto profile = profileFor(versions::finale2006.major);
    profile.epoch = FormatEpoch::DclLegacy;
    const auto index = LegacyRecordIndex::build(parsed);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    ImportReport report(profile.epoch);
    PendingReferences pending;
    const ImportContext context{index, profile, noSource, document, document, report, pending, session.getConstructionContext()};
    others::importTextExpressionDefs(context);
    const auto expression = document->getOthers()->get<TestExpression>(musx::dom::SCORE_PARTID, 7);
    REQUIRE(expression);
    CHECK(expression->playbackType == expected);
    const auto* playback = report.findField<TestExpression>("playbackType", musx::dom::SCORE_PARTID, musx::dom::Cmper(7));
    REQUIRE(playback);
    CHECK(playback->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Text expressions recover the human playback flag apart from a Coda enclosure nibble", "[class][text-expression]")
{
    using namespace finale_mus_reader;
    const auto epoch = GENERATE(FormatEpoch::CodaBanner, FormatEpoch::DclLegacy);
    const bool enclosureRecord = GENERATE(false, true);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto font = std::make_shared<musx::dom::others::FontDefinition>(
        document, musx::dom::Cmper(0), musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper(1));
    font->name = "Times";
    document->getOthers()->add(font->XmlNodeName, font);
    std::vector<SyntheticRow> rows{
        {1, "DT", {static_cast<std::int16_t>(epoch == FormatEpoch::CodaBanner ? 0x010c : 0x0c01), 0, 0, 0, 0, 0x0100}}, {1, "DT", {0x4142}}};
    if (enclosureRecord) {
        rows.push_back({1, "De", {0, 0, 0, 0, 0, 1}});
    }
    auto parsed = makeContainer(rows, epoch);
    auto profile = epoch == FormatEpoch::CodaBanner ? profileFor(2) : profileFor(versions::finale2002.major);
    profile.epoch = epoch;
    const auto index = LegacyRecordIndex::build(parsed);
    ImportReport report(profile.epoch);
    PendingReferences pending;
    const ImportContext context{index, profile, noSource, document, document, report, pending, session.getConstructionContext()};
    others::importTextExpressionDefs(context);
    runDeferredChecks(pending);
    const auto expression = document->getOthers()->get<TestExpression>(0, 1);
    REQUIRE(expression);
    // In a Coda file the bit names an enclosure when its record exists. Coda enclosures are out of
    // scope, so the definition reports none.
    const bool namesEnclosure = epoch == FormatEpoch::CodaBanner && enclosureRecord;
    CHECK_FALSE(expression->hasEnclosure);
    CHECK(expression->createdByHp == !namesEnclosure);
    const auto* enclosure = report.findField<TestExpression>("hasEnclosure", 0, musx::dom::Cmper(1));
    REQUIRE(enclosure);
    CHECK(enclosure->origin == (namesEnclosure ? ValueOrigin::OutOfScope : ValueOrigin::LegacyMus));
    const auto* field = report.findField<TestExpression>("createdByHp", 0, musx::dom::Cmper(1));
    REQUIRE(field);
    CHECK(field->origin == (namesEnclosure ? ValueOrigin::LegacyMusAdjusted : ValueOrigin::LegacyMus));
}

TEST_CASE("Prime Enigma text expression references survive fixed-row import", "[class][text-expression]")
{
    const auto result = readFixture("evidence/F2006/F2006-text-inserts.mus");
    const auto expression = result.document->getOthers()->get<TestExpression>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(expression);
    CHECK(expression->textIdKey == 19);
    CHECK(expression->categoryId == 7);
    CHECK(expression->horzMeasExprAlign == musx::dom::others::HorizontalMeasExprAlign::Manual);
    CHECK(expression->rehearsalMarkStyle == musx::dom::others::RehearsalMarkStyle::None);
    CHECK(field(result.report, "others.textExprDef[1].rehearsalMarkStyle").origin == ValueOrigin::LegacyBehavior);
    CHECK_FALSE(expression->matchPlayback);
    CHECK(field(result.report, "others.textExprDef[1].matchPlayback").origin == ValueOrigin::LegacyBehavior);
    CHECK(expression->vertMeasExprAlign == musx::dom::others::VerticalMeasExprAlign::Manual);
    CHECK(field(result.report, "others.textExprDef[1].textIdKey").origin == ValueOrigin::LegacyMus);
    const auto described = readFixture("evidence/F2006/F2006-embedded-tiff.mus");
    CHECK(described.document->getOthers()->get<TestExpression>(musx::dom::SCORE_PARTID, 1)->description == "Below Staff (Vel. 127)");
}

TEST_CASE("Unicode era expressions recover category and signed positioning", "[class][text-expression]")
{
    const auto result = readFixture("evidence/F2012/F2012-noteartexp-unlnk-move.mus");
    const auto expression = result.document->getOthers()->get<TestExpression>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(expression);
    CHECK(expression->textIdKey == 2);
    CHECK(expression->categoryId == 1);
    CHECK(expression->horzMeasExprAlign == musx::dom::others::HorizontalMeasExprAlign::LeftOfPrimaryNotehead);
    CHECK(expression->horzExprJustification == musx::dom::AlignJustify::Center);
    CHECK(expression->vertMeasExprAlign == musx::dom::others::VerticalMeasExprAlign::BelowStaffOrEntry);
    CHECK(expression->yAdjustBaseline == 16);
    CHECK(expression->yAdjustEntry == -72);
}

TEST_CASE("SmartMusic playback normalizes with retained source provenance", "[class][text-expression]")
{
    const auto result = readFixture("evidence/F2006/F2006-embedded-tiff.mus");
    for (const auto cmper : {29, 30, 31, 32, 17}) {
        const bool smartMusic = cmper != 17;
        const auto expression = result.document->getOthers()->get<TestExpression>(musx::dom::SCORE_PARTID, musx::dom::Cmper(cmper));
        REQUIRE(expression);
        CHECK(expression->playbackType == (smartMusic ? musx::dom::others::PlaybackType::None : musx::dom::others::PlaybackType::Tempo));
        CHECK(expression->auxData1 == (smartMusic ? 0 : 1024));
        CHECK(expression->useAuxData);
        if (smartMusic) {
            CHECK(expression->hasEnclosure);
            CHECK(expression->breakMmRest);
            CHECK(expression->yAdjustBaseline == 18);
        }
        const auto prefix = "others.textExprDef[" + std::to_string(cmper) + "].";
        const auto& auxiliary = field(result.report, prefix + "auxData1");
        const auto& playback = field(result.report, prefix + "playbackType");
        if (smartMusic) {
            const auto& baseline = field(result.report, prefix + "yAdjustBaseline");
            CHECK(baseline.origin == ValueOrigin::LegacyMus);
            CHECK(baseline.rawValue == 18);
        }
        const auto origin = smartMusic ? ValueOrigin::LegacyMusAdjusted : ValueOrigin::LegacyMus;
        CHECK(auxiliary.origin == origin);
        CHECK(playback.origin == origin);
        CHECK(auxiliary.rawValue == (smartMusic ? (cmper == 30 ? 15 : 14) : 1024));
        CHECK(playback.rawValue == (smartMusic ? 0x1c0f : 0x1001));
        CHECK(auxiliary.sourceIdentity == 0x4454);
        CHECK(playback.decodedOffset == auxiliary.decodedOffset + 4);
        CHECK(field(result.report, prefix + "useAuxData").origin == ValueOrigin::LegacyMus);
    }
}

TEST_CASE("Rehearsal marks independently hide measure numbers and match playback", "[class][text-expression]")
{
    const char* paths[] = {"evidence/F2011/F2011-rehearsal-mark.mus", "evidence/F2011/F2011-rehearsal-mark-hidemeas.mus",
        "evidence/F2011/F2011-rehearsal-mark-matchplay.mus"};
    for (const auto variant : {0, 1, 2}) {
        const bool hidden = variant == 1;
        const bool match = variant == 2;
        const auto result = readFixture(paths[variant]);
        const auto expression = result.document->getOthers()->get<TestExpression>(musx::dom::SCORE_PARTID, 1);
        REQUIRE(expression);
        CHECK(expression->hideMeasureNum == hidden);
        CHECK(expression->matchPlayback == match);
        CHECK(expression->useAuxData == match);
        CHECK(expression->auxData1 == (match ? 1024 : 0));
        CHECK(expression->playbackType == (match ? musx::dom::others::PlaybackType::Tempo : musx::dom::others::PlaybackType::None));
        CHECK(expression->rehearsalMarkStyle == musx::dom::others::RehearsalMarkStyle::MeasureNumber);
        CHECK(expression->breakMmRest);
        const auto& flag = field(result.report, "others.textExprDef[1].hideMeasureNum");
        CHECK(flag.origin == ValueOrigin::LegacyMus);
        CHECK(flag.rawValue == static_cast<std::int16_t>(hidden ? 0x8400 : match ? 0x5401 : 0x0400));
        CHECK(flag.sourceIdentity == 0x00f1);
        CHECK(flag.blockOffset == 0x200);
        CHECK(flag.decodedOffset == 0x23bc + 10);
        const auto& matchFlag = field(result.report, "others.textExprDef[1].matchPlayback");
        CHECK(matchFlag.origin == ValueOrigin::LegacyMus);
        CHECK(matchFlag.rawValue == flag.rawValue);
        CHECK(matchFlag.decodedOffset == flag.decodedOffset);
    }
}

TEST_CASE("Expression UTF-16 descriptions and bounds are independent of byte order", "[class][text-expression]")
{
    for (auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        auto session = musx::factory::DocumentFactory::begin();
        auto document = session.getDocument();
        std::vector<std::int16_t> words(18);
        words[1] = 4;
        words.insert(words.end(), {0x03a9, static_cast<std::int16_t>(0xd834), static_cast<std::int16_t>(0xdd1e), 0});
        auto parsed = makeClassContainer({SyntheticClassRow{0xf1, words, 1}, SyntheticClassRow{0xf1, {1, 2, 3}, 2}}, order);
        auto profile = profileFor(finale_mus_reader::versions::finale2012.major);
        profile.epoch = FormatEpoch::ZlibLegacy;
        profile.byteOrder = order;
        const auto index = LegacyRecordIndex::build(parsed);
        ImportReport report(profile.epoch);
        finale_mus_reader::PendingReferences pending;
        const finale_mus_reader::ImportContext context{
            index, profile, noSource, document, document, report, pending, session.getConstructionContext()};
        finale_mus_reader::others::importTextExpressionDefs(context);
        const auto expression = document->getOthers()->get<TestExpression>(musx::dom::SCORE_PARTID, 1);
        REQUIRE(expression);
        CHECK(expression->description == "Ω𝄞");
        CHECK(expression->rehearsalMarkStyle == musx::dom::others::RehearsalMarkStyle::LettersNumbersLowerCase);
        const auto& style = field(report, "others.textExprDef[1].rehearsalMarkStyle");
        CHECK(style.origin == ValueOrigin::LegacyMus);
        CHECK(style.rawValue == 4);
        CHECK_FALSE(document->getOthers()->get<TestExpression>(musx::dom::SCORE_PARTID, 2));
        CHECK(report.diagnostics.size() == 1);
        CHECK(reportedFieldCount(report) == 23);
        CHECK(TestExpression::xmlMappingArray().size() == reportedFieldCount(report));
        CHECK(field(report, "others.textExprDef[1].createdByHp").origin == ValueOrigin::LegacyMus);
    }
}

TEST_CASE("Unused legacy note selectors do not override measure positioning", "[class][text-expression]")
{
    for (const bool modern : {false, true}) {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        std::vector<std::int16_t> words(18);
        words[6] = 3;
        words[12] = 2;
        words[9] = 4;
        words[14] = 8;
        auto parsed = makeClassContainer({SyntheticClassRow{0xf1, words, 1}}, ByteOrder::LittleEndian);
        auto profile = profileFor(modern ? finale_mus_reader::versions::finale2009.major : finale_mus_reader::versions::finale2008.major);
        profile.epoch = FormatEpoch::ZlibLegacy;
        profile.byteOrder = ByteOrder::LittleEndian;
        const auto index = LegacyRecordIndex::build(parsed);
        ImportReport report(profile.epoch);
        finale_mus_reader::PendingReferences pending;
        const finale_mus_reader::ImportContext context{
            index, profile, noSource, document, document, report, pending, session.getConstructionContext()};
        finale_mus_reader::others::importTextExpressionDefs(context);
        const auto expression = document->getOthers()->get<TestExpression>(0, musx::dom::Cmper(1));
        REQUIRE(expression);
        CHECK(expression->horzMeasExprAlign == musx::dom::others::HorizontalMeasExprAlign::Manual);
        CHECK(expression->vertMeasExprAlign == musx::dom::others::VerticalMeasExprAlign::Manual);
        CHECK(field(report, "others.textExprDef[1].horzMeasExprAlign").rawValue == 3);
        CHECK(field(report, "others.textExprDef[1].vertMeasExprAlign").rawValue == 2);
    }
}
} // namespace
} // namespace finale_mus_reader_tests
