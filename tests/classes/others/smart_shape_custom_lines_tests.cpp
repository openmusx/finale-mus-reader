// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

// The pre-Unicode word stream of one custom line style per line style, 36 words each.
// Spelled out rather than derived, so a change to the layout has to be restated here to pass.
const std::vector<std::int16_t> ssLineCharWords{
    2, 199, 6, 17, 3, -1, -83, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const std::vector<std::int16_t> ssLineDashedWords{
    1, 118, 192, 448, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const std::vector<std::int16_t> ssLineCappedWords{
    0, 118, 0, 0, 0, 0, 0, 3, 2, 1984, 0, 2, 0, 5, 1, 2, 3, 4, 5, 11, 13, 17, 19, 23, -29, 31, 37, 41, 43, 47, 61, 59, 61, 53, 0, 0};

// The same three in the Finale 2012 layout. The character occupies words 1 and 2, so only the
// Char record's own later fields move; the Dashed record is untouched and everything from the
// old word 7 on moves for all three.
const std::vector<std::int16_t> ssLineWideCharWords{
    2, 171, 0, 6, 17, 3, -1, -83, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const std::vector<std::int16_t> ssLineWideDashedWords = ssLineDashedWords;
const std::vector<std::int16_t> ssLineWideCappedWords{
    0, 118, 0, 0, 0, 0, 0, 0, 3, 2, 1984, 0, 2, 0, 5, 1, 2, 3, 4, 5, 11, 13, 17, 19, 23, -29, 31, 37, 41, 43, 47, 61, 59, 61, 53, 0};

/// @brief Splits one logical word stream into the six fixed rows a family occupies.
std::vector<SyntheticRow> ssLineFixedRows(std::uint16_t cmper, const std::vector<std::int16_t>& words)
{
    std::vector<SyntheticRow> rows;
    for (std::size_t at = 0; at < words.size(); at += 6) {
        SyntheticRow row{cmper, "ls", {}};
        for (std::size_t slot = 0; slot < 6; ++slot) {
            row.words[slot] = words[at + slot];
        }
        rows.push_back(row);
    }
    return rows;
}

/// @brief A document carrying the two font definitions the character decoding needs.
/// @details Font 6 is an ordinary Mac text font, so a byte stored in it is Mac Roman. Font 0 is
/// the default music font, whose byte is a glyph number whatever its record claims.
musx::dom::DocumentPtr ssLineDocument(musx::factory::DocumentFactory::ConstructionSession& session)
{
    using FontDefinition = musx::dom::others::FontDefinition;
    const auto document = session.getDocument();
    const auto addFont = [&](musx::dom::Cmper cmper, int charsetVal) {
        auto font = std::make_shared<FontDefinition>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, cmper);
        font->charsetBank = FontDefinition::CharacterSetBank::MacOS;
        font->charsetVal = charsetVal;
        document->getOthers()->add(FontDefinition::XmlNodeName, font);
    };
    addFont(0, 4095);
    addFont(6, 0);
    return document;
}

/// @brief Runs the custom-line importer over one synthesized container.
void ssLineImport(const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile, const musx::dom::DocumentPtr& document,
    ImportReport& report)
{
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importSmartShapeCustomLines(context);
}

/// @brief Every field of every line style, in each epoch that carries the record.
/// @details The Coda-banner epoch is in the sweep to assert the opposite: the record does not
/// exist before Finale 2000, so rows carrying its tag must build nothing at all rather than
/// empty objects.
void testSmartShapeCustomLinesAcrossEpochs()
{
    using CustomLine = musx::dom::others::SmartShapeCustomLine;
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        const auto era = " in epoch " + std::to_string(static_cast<int>(epoch));
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = ssLineDocument(session);
        ImportReport report(FormatEpoch::UncompressedLegacy);
        SourceProfile profile(epoch);
        if (epoch == FormatEpoch::ZlibLegacy) {
            // One record per container, so each line style is imported into its own document
            // and then read back from a document of its own.
            for (const auto& record : {std::pair{std::uint16_t{1}, ssLineCharWords}, std::pair{std::uint16_t{2}, ssLineDashedWords},
                     std::pair{std::uint16_t{3}, ssLineCappedWords}}) {
                const auto parsed = makeClassContainer(0x00de, record.second, ByteOrder::BigEndian, record.first);
                profile.byteOrder = parsed.byteOrder;
                ssLineImport(parsed, profile, document, report);
            }
        } else {
            std::vector<SyntheticRow> rows;
            for (const auto& record : {std::pair{std::uint16_t{1}, ssLineCharWords}, std::pair{std::uint16_t{2}, ssLineDashedWords},
                     std::pair{std::uint16_t{3}, ssLineCappedWords}}) {
                const auto family = ssLineFixedRows(record.first, record.second);
                rows.insert(rows.end(), family.begin(), family.end());
            }
            const auto parsed = makeContainer(rows, epoch);
            profile.byteOrder = parsed.byteOrder;
            ssLineImport(parsed, profile, document, report);
        }

        const auto lines = document->getOthers()->getArray<CustomLine>(musx::dom::SCORE_PARTID);
        if (epoch == FormatEpoch::CodaBanner) {
            expectMapping(lines.empty(), "A Coda-banner source built custom line styles from a record that era never"
                                         " wrote"
                                             + era);
            expectMapping(reportedFieldCount(report) == 0, "A Coda-banner source reported custom line style fields it cannot have" + era);
            continue;
        }
        expectMapping(lines.size() == 3, "Not every custom line style was built" + era);

        const auto charLine = document->getOthers()->get<CustomLine>(musx::dom::SCORE_PARTID, 1);
        expectMapping(charLine && charLine->lineStyle == CustomLine::LineStyle::Char, "The Char line style was not recovered" + era);
        expectMapping(charLine->charParams && !charLine->solidParams && !charLine->dashedParams,
            "A Char line kept a parameter block its line style does not select" + era);
        // 199 is Mac Roman in font 6, so the document must carry the code point rather than
        // the byte. The setter musxdom owns turns the stored mask into the two flags.
        expectMapping(charLine->charParams->lineChar == 0x00ab, "The Char line character was not decoded through its own font" + era);
        expectMapping(charLine->charParams->font->fontId == 6 && charLine->charParams->font->fontSize == 17 && charLine->charParams->font->bold
                          && charLine->charParams->font->italic && !charLine->charParams->font->underline,
            "The Char line font tuple was not recovered" + era);
        expectMapping(charLine->charParams->baselineShiftEms == -83, "The Char line baseline shift was not recovered" + era);

        const auto dashed = document->getOthers()->get<CustomLine>(musx::dom::SCORE_PARTID, 2);
        expectMapping(dashed && dashed->dashedParams && !dashed->charParams && dashed->dashedParams->lineWidth == 118
                          && dashed->dashedParams->dashOn == 192 && dashed->dashedParams->dashOff == 448,
            "The Dashed line parameters were not recovered" + era);

        const auto capped = document->getOthers()->get<CustomLine>(musx::dom::SCORE_PARTID, 3);
        expectMapping(capped && capped->solidParams && capped->solidParams->lineWidth == 118, "The Solid line width was not recovered" + era);
        expectMapping(capped->lineCapStartType == CustomLine::LineCapType::Hook && capped->lineCapEndType == CustomLine::LineCapType::ArrowheadCustom
                          && capped->lineCapStartHookLength == 1984 && capped->lineCapEndArrowId == 2 && capped->lineCapStartArrowId == 0
                          && capped->lineCapEndHookLength == 0,
            "A line cap read its shared value slot as the wrong one of the two" + era);
        expectMapping(capped->makeHorz && !capped->lineAfterLeftStartText && capped->lineBeforeRightEndText && !capped->lineAfterLeftContText,
            "The line adjustment flags were not recovered" + era);
        expectMapping(capped->leftStartRawTextId == 1 && capped->leftContRawTextId == 2 && capped->rightEndRawTextId == 3
                          && capped->centerFullRawTextId == 4 && capped->centerAbbrRawTextId == 5,
            "The text anchor comparators were not recovered" + era);
        expectMapping(capped->leftStartX == 11 && capped->leftStartY == 13 && capped->leftContX == 17 && capped->leftContY == 19
                          && capped->rightEndX == 23 && capped->rightEndY == -29 && capped->centerFullX == 31 && capped->centerFullY == 37
                          && capped->centerAbbrX == 41 && capped->centerAbbrY == 43,
            "The anchor offsets did not interleave X with Y" + era);
        expectMapping(
            capped->lineStartX == 47 && capped->lineStartY == 61 && capped->lineEndX == 59 && capped->lineEndY == 61 && capped->lineContX == 53,
            "The line adjustments were not recovered" + era);

        // A field the record does not carry is not reported at all, because the destination
        // does not exist for it. The nested destinations are named with dots, as every other
        // nested report target is.
        expectMapping(!fieldPresent(report, "others.smartShapeCustomLine[2].charParams.lineChar"),
            "A Dashed line reported a Char parameter it cannot have" + era);
        expectMapping(fieldPresent(report, "others.smartShapeCustomLine[3].lineCapStartHookLength")
                          && !fieldPresent(report, "others.smartShapeCustomLine[3].lineCapEndHookLength"),
            "An arrowhead cap reported the hook length it does not use" + era);
        expectMapping(field(report, "others.smartShapeCustomLine[1].charParams.font.fontSize").origin == ValueOrigin::LegacyMus,
            "A nested destination was not reported under its dotted path" + era);
        // The report keeps the byte the source held, not the code point it decoded to.
        expectMapping(field(report, "others.smartShapeCustomLine[1].charParams.lineChar").rawValue == 199,
            "The report did not keep the stored character byte" + era);
    }
}

/// @brief The Finale 2012 layout, where only the Char parameter block moves with the character.
void testSmartShapeCustomLineUnicodeLayout()
{
    using CustomLine = musx::dom::others::SmartShapeCustomLine;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = ssLineDocument(session);
    ImportReport report(FormatEpoch::UncompressedLegacy);
    auto profile = profileFor(17);
    profile.epoch = FormatEpoch::ZlibLegacy;
    profile.byteOrder = ByteOrder::LittleEndian;
    for (const auto& record : {std::pair{std::uint16_t{1}, ssLineWideCharWords}, std::pair{std::uint16_t{2}, ssLineWideDashedWords},
             std::pair{std::uint16_t{3}, ssLineWideCappedWords}}) {
        ssLineImport(makeClassContainer(0x00de, record.second, profile.byteOrder, record.first), profile, document, report);
    }

    const auto charLine = document->getOthers()->get<CustomLine>(musx::dom::SCORE_PARTID, 1);
    expectMapping(charLine && charLine->charParams, "The Unicode-era Char line was not recovered");
    // Already a code point in this era, so nothing is decoded and font 6 is not consulted.
    expectMapping(charLine->charParams->lineChar == 0x00ab, "The Unicode-era character was not read as a code point");
    expectMapping(charLine->charParams->font->fontId == 6 && charLine->charParams->font->fontSize == 17 && charLine->charParams->font->bold
                      && charLine->charParams->font->italic && charLine->charParams->baselineShiftEms == -83,
        "The Char parameter block did not shift with the widened character");

    const auto dashed = document->getOthers()->get<CustomLine>(musx::dom::SCORE_PARTID, 2);
    expectMapping(dashed && dashed->dashedParams && dashed->dashedParams->dashOn == 192 && dashed->dashedParams->dashOff == 448,
        "A Dashed record shifted with a character it does not have");

    const auto capped = document->getOthers()->get<CustomLine>(musx::dom::SCORE_PARTID, 3);
    expectMapping(capped && capped->lineCapStartType == CustomLine::LineCapType::Hook && capped->lineCapStartHookLength == 1984
                      && capped->lineCapEndArrowId == 2,
        "The common part of the record did not shift in the Unicode era");
    expectMapping(capped->leftStartX == 11 && capped->centerAbbrY == 43 && capped->lineStartX == 47 && capped->lineContX == 53,
        "The positions did not shift in the Unicode era");
}

void testSmartShapeCustomLines()
{
    using namespace musx::dom;
    using LineTarget = others::SmartShapeCustomLine;

    // Fixed-row Finale 2000: cmper 1 is a Char line ('~', size 24, baseline -88 EMs), and cmper
    // 2 is a bare Solid line (width 118 Efix).
    const auto fixedRow =
        Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F2000/F2000-baseline.mus");
    const auto charLine = fixedRow.document->getOthers()->get<LineTarget>(SCORE_PARTID, 1);
    REQUIRE(charLine != nullptr);
    CHECK(charLine->lineStyle == LineTarget::LineStyle::Char);
    REQUIRE(charLine->charParams != nullptr);
    CHECK(charLine->charParams->lineChar == U'~');
    CHECK(charLine->charParams->font->fontSize == 24);
    CHECK(charLine->charParams->baselineShiftEms == -88);
    expect(field(fixedRow, "others.smartShapeCustomLine[1].charParams.lineChar").origin == ValueOrigin::LegacyMus
               && field(fixedRow, "others.smartShapeCustomLine[1].charParams.lineChar").rawValue == 126,
        "A recovered Char line style did not report its source raw character");

    const auto solidLine = fixedRow.document->getOthers()->get<LineTarget>(SCORE_PARTID, 2);
    REQUIRE(solidLine != nullptr);
    CHECK(solidLine->lineStyle == LineTarget::LineStyle::Solid);
    REQUIRE(solidLine->solidParams != nullptr);
    CHECK(solidLine->solidParams->lineWidth == 118);

    // The bend-curve tool postdates Finale 2000, so its baseline definition follows the two
    // source-owned lines. Imported defaults retain baseline values rather than Finale's upgrade.
    const auto bendLine = fixedRow.document->getOthers()->get<LineTarget>(SCORE_PARTID, 3);
    REQUIRE(bendLine != nullptr);
    CHECK(bendLine->lineStyle == LineTarget::LineStyle::Solid);
    REQUIRE(bendLine->solidParams != nullptr);
    CHECK(bendLine->solidParams->lineWidth == 115);
    CHECK(bendLine->lineCapEndType == LineTarget::LineCapType::ArrowheadPreset);
    CHECK(bendLine->lineCapEndArrowId == 1);

    // Every text-anchor and line-adjustment offset of one record, each set to its own value
    // so that no two slots can be confused. The five X offsets interleave with the Y offsets
    // they pair with rather than being grouped, and the line adjustments follow in musxdom's
    // own declaration order.
    const auto offsets =
        Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F2000/F2000-ssline-offsets.mus");
    const auto positioned = offsets.document->getOthers()->get<LineTarget>(SCORE_PARTID, 3);
    REQUIRE(positioned != nullptr);
    CHECK(positioned->leftStartRawTextId == 1);
    CHECK(positioned->leftContRawTextId == 2);
    CHECK(positioned->rightEndRawTextId == 3);
    CHECK(positioned->centerFullRawTextId == 4);
    CHECK(positioned->centerAbbrRawTextId == 5);
    CHECK(positioned->leftStartX == 11);
    CHECK(positioned->leftStartY == 13);
    CHECK(positioned->leftContX == 17);
    CHECK(positioned->leftContY == 19);
    CHECK(positioned->rightEndX == 23);
    CHECK(positioned->rightEndY == -29);
    CHECK(positioned->centerFullX == 31);
    CHECK(positioned->centerFullY == 37);
    CHECK(positioned->centerAbbrX == 41);
    CHECK(positioned->centerAbbrY == 43);
    CHECK(positioned->lineStartX == 47);
    CHECK(positioned->lineEndX == 59);
    CHECK(positioned->lineContX == 53);
    // Finale offers one vertical control for the whole line and writes it to both slots, so
    // the two must come out equal from two different words rather than one being copied.
    CHECK(positioned->lineStartY == 61);
    CHECK(positioned->lineEndY == 61);
    expect(field(offsets, "others.smartShapeCustomLine[3].lineEndY").origin == ValueOrigin::LegacyMus,
        "The second half of the synced vertical adjustment was not recovered from the source");

    // The remaining three line styles of the same document, each settling one thing the
    // earlier samples could not distinguish.
    const auto dashed = offsets.document->getOthers()->get<LineTarget>(SCORE_PARTID, 4);
    REQUIRE(dashed != nullptr);
    REQUIRE(dashed->dashedParams != nullptr);
    CHECK(dashed->dashedParams->lineWidth == 118);
    // 3 and 7 EVPU. Unequal at last, so the two slots are told apart rather than assumed.
    CHECK(dashed->dashedParams->dashOn == 192);
    CHECK(dashed->dashedParams->dashOff == 448);

    // A Char line in a named text font, bold and italic. The stored character is a byte in
    // that font's encoding, so Mac Roman 199 is the code point 171 and not the number the
    // file holds; the report keeps the source value while the document carries the code point.
    const auto styled = offsets.document->getOthers()->get<LineTarget>(SCORE_PARTID, 5);
    REQUIRE(styled != nullptr);
    REQUIRE(styled->charParams != nullptr);
    CHECK(styled->charParams->lineChar == U'\u00ab');
    CHECK(styled->charParams->font->fontId == 6);
    CHECK(styled->charParams->font->fontSize == 17);
    CHECK(styled->charParams->font->bold);
    CHECK(styled->charParams->font->italic);
    CHECK_FALSE(styled->charParams->font->underline);
    CHECK(styled->charParams->baselineShiftEms == -83);
    expect(field(offsets, "others.smartShapeCustomLine[5].charParams.lineChar").rawValue == 199,
        "The report did not keep the byte the source actually stored");

    // A hook at one end and a custom arrowhead shape at the other, so each cap's shared value
    // slot is read as the one its own type names.
    const auto capped = offsets.document->getOthers()->get<LineTarget>(SCORE_PARTID, 6);
    REQUIRE(capped != nullptr);
    CHECK(capped->lineCapStartType == LineTarget::LineCapType::Hook);
    CHECK(capped->lineCapEndType == LineTarget::LineCapType::ArrowheadCustom);
    CHECK(capped->lineCapStartHookLength == 1984);
    CHECK(capped->lineCapEndArrowId == 2);
    CHECK(capped->lineCapStartArrowId == 0);
    CHECK(capped->lineCapEndHookLength == 0);

    // No `ls` row occurs before Finale 2000. Those formats receive the baseline glissando,
    // tab-slide, and guitar-bend definitions, in that order, so their option references are
    // usable rather than foreign baseline cmpers.
    const auto pre2000 =
        Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F97/Fin97-baseline.mus");
    const auto pre2000Options = pre2000.document->getOptions()->get<options::SmartShapeOptions>();
    REQUIRE(pre2000Options != nullptr);
    CHECK(pre2000Options->ssLineStyleCmpGlissando == 1);
    CHECK(pre2000Options->ssLineStyleCmpTabSlide == 2);
    CHECK(pre2000Options->ssLineStyleCmpTabBendCurve == 3);
    CHECK(pre2000.document->getOthers()->getArray<LineTarget>(SCORE_PARTID).size() == 3);
    for (musx::dom::Cmper cmper = 1; cmper <= 3; ++cmper) {
        const auto* origin = pre2000.report.findInstanceOrigin(finale_mus_reader::instanceKey<LineTarget>(SCORE_PARTID, cmper));
        REQUIRE(origin != nullptr);
        CHECK(*origin == ValueOrigin::Finale27Default);
    }

    const auto preEngraverSlur = Reader::readWithReport<TestXmlDocument>(
        std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F97/F97-slurtieopts-changed.mus");
    const auto preEngraverSlurOptions = preEngraverSlur.document->getOptions()->get<options::SmartShapeOptions>();
    REQUIRE(preEngraverSlurOptions != nullptr);
    const auto shortContour = preEngraverSlurOptions->slurControlStyles.at(options::SmartShapeOptions::SlurControlStyleType::ShortSpan);
    const auto mediumContour = preEngraverSlurOptions->slurControlStyles.at(options::SmartShapeOptions::SlurControlStyleType::MediumSpan);
    const auto longContour = preEngraverSlurOptions->slurControlStyles.at(options::SmartShapeOptions::SlurControlStyleType::LongSpan);
    const auto extraLongContour = preEngraverSlurOptions->slurControlStyles.at(options::SmartShapeOptions::SlurControlStyleType::ExtraLongSpan);
    CHECK(shortContour->span == 36);
    CHECK(shortContour->inset == 532);
    CHECK(shortContour->height == 13);
    CHECK(mediumContour->span == 288);
    CHECK(mediumContour->inset == 553);
    CHECK(mediumContour->height == 43);
    CHECK(longContour->span == 864);
    CHECK(longContour->inset == 358);
    CHECK(longContour->height == 73);
    CHECK(extraLongContour->span == 1152);
    CHECK(extraLongContour->inset == 358);
    CHECK(extraLongContour->height == 73);
    CHECK(preEngraverSlurOptions->slurAvoidStaffLines);
    CHECK(field(preEngraverSlur, "options.smartShapeOptions.slurControlStyles[3].span").origin == ValueOrigin::Finale27Default);
    CHECK(field(preEngraverSlur, "options.smartShapeOptions.slurControlStyles[3].inset").origin == ValueOrigin::LegacyBehavior);
    CHECK(field(preEngraverSlur, "options.smartShapeOptions.slurControlStyles[3].height").rawValue == 73);
    CHECK(field(preEngraverSlur, "options.smartShapeOptions.slurAvoidStaffLines").origin == ValueOrigin::Finale27Default);
    CHECK(preEngraverSlurOptions->slurConnectStyles.size() == 29);
    CHECK(field(preEngraverSlur, "options.smartShapeOptions.slurConnectStyles[24].connectIndex").origin == ValueOrigin::LegacyMus);
    CHECK(field(preEngraverSlur, "options.smartShapeOptions.slurConnectStyles[25].connectIndex").origin == ValueOrigin::Finale27Default);

    const auto preEngraverThickness =
        Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F2000/F2000-slur-thickness.mus");
    const auto preEngraverThicknessOptions = preEngraverThickness.document->getOptions()->get<options::SmartShapeOptions>();
    REQUIRE(preEngraverThicknessOptions != nullptr);
    CHECK(preEngraverThicknessOptions->slurThicknessCp1Y == 17);
    CHECK(preEngraverThicknessOptions->slurThicknessCp2Y == 17);
    CHECK(field(preEngraverThickness, "options.smartShapeOptions.slurThicknessCp1Y").origin == ValueOrigin::LegacyMus);
    CHECK(field(preEngraverThickness, "options.smartShapeOptions.slurThicknessCp2Y").rawValue == 17);

    const auto coda =
        Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F263/F263-baseline.mus");
    const auto codaOptions = coda.document->getOptions()->get<options::SmartShapeOptions>();
    REQUIRE(codaOptions != nullptr);
    CHECK(codaOptions->ssLineStyleCmpGlissando == 1);
    CHECK(codaOptions->ssLineStyleCmpTabSlide == 2);
    CHECK(codaOptions->ssLineStyleCmpTabBendCurve == 3);
    CHECK(codaOptions->hookLength == 8);
    CHECK(field(coda, "options.smartShapeOptions.hookLength").origin == ValueOrigin::LegacyBehavior);
    CHECK(field(coda, "options.smartShapeOptions.slurConnectStyles[0].connectIndex").origin == ValueOrigin::LegacyMus);
    CHECK(field(coda, "options.smartShapeOptions.slurConnectStyles[1].yOffset").origin == ValueOrigin::LegacyMus);
    CHECK(field(coda, "options.smartShapeOptions.slurConnectStyles[2].xOffset").origin == ValueOrigin::Finale27Default);
    CHECK(coda.document->getOthers()->getArray<LineTarget>(SCORE_PARTID).size() == 3);

    const auto finale100 =
        Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F100/F100-baseline.mus");
    const auto finale100Options = finale100.document->getOptions()->get<options::SmartShapeOptions>();
    REQUIRE(finale100Options != nullptr);
    CHECK(finale100Options->hookLength == 12);

    // Finale 2000 and 2002 already own the glissando and tab-slide definitions but
    // predate the bend-curve tool. Its baseline definition follows the source-owned pool.
    const auto finale2000 =
        Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F2000/F2000-baseline.mus");
    const auto finale2000Options = finale2000.document->getOptions()->get<options::SmartShapeOptions>();
    REQUIRE(finale2000Options != nullptr);
    CHECK(finale2000Options->ssLineStyleCmpGlissando == 1);
    CHECK(finale2000Options->ssLineStyleCmpTabSlide == 2);
    CHECK(finale2000Options->ssLineStyleCmpTabBendCurve == 3);
    CHECK(finale2000.document->getOthers()->getArray<LineTarget>(SCORE_PARTID).size() == 3);
    const auto* finale2000BendOrigin =
        finale2000.report.findInstanceOrigin(finale_mus_reader::instanceKey<LineTarget>(SCORE_PARTID, musx::dom::Cmper(3)));
    REQUIRE(finale2000BendOrigin != nullptr);
    CHECK(*finale2000BendOrigin == ValueOrigin::Finale27Default);
    CHECK(field(finale2000, "options.smartShapeOptions.ssLineStyleCmpTabBendCurve").origin == ValueOrigin::Finale27Default);
    CHECK(finale2000Options->slurConnectStyles.size() == 29);
    CHECK(finale2000Options->tabSlideConnectStyles.size() == 18);
    CHECK(finale2000Options->glissandoConnectStyles.size() == 2);
    CHECK(finale2000Options->bendCurveConnectStyles.size() == 8);
    CHECK(field(finale2000, "options.smartShapeOptions.tabSlideConnectStyles[17].yOffset").origin == ValueOrigin::LegacyMus);
    CHECK(field(finale2000, "options.smartShapeOptions.glissandoConnectStyles[1].xOffset").origin == ValueOrigin::LegacyMus);
    CHECK(field(finale2000, "options.smartShapeOptions.bendCurveConnectStyles[0].xOffset").origin == ValueOrigin::Finale27Default);

    const auto finale2000Offsets =
        Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F2000/F2000-ssline-offsets.mus");
    const auto finale2000OffsetsOptions = finale2000Offsets.document->getOptions()->get<options::SmartShapeOptions>();
    REQUIRE(finale2000OffsetsOptions != nullptr);
    CHECK(finale2000OffsetsOptions->ssLineStyleCmpTabBendCurve == 7);
    CHECK(finale2000Offsets.document->getOthers()->getArray<LineTarget>(SCORE_PARTID).size() == 7);

    const auto finale2002 =
        Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F2002/F2002-baseline.mus");
    const auto finale2002Options = finale2002.document->getOptions()->get<options::SmartShapeOptions>();
    REQUIRE(finale2002Options != nullptr);
    CHECK(finale2002Options->ssLineStyleCmpTabBendCurve == 3);
    CHECK(finale2002.document->getOthers()->getArray<LineTarget>(SCORE_PARTID).size() == 3);
    CHECK(field(finale2002, "options.smartShapeOptions.ssLineStyleCmpTabBendCurve").origin == ValueOrigin::Finale27Default);

    const auto finale2002TipAvoidance = Reader::readWithReport<TestXmlDocument>(
        std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F2002/F2002-tips-avoid-stafflines.mus");
    const auto finale2002TipAvoidanceOptions = finale2002TipAvoidance.document->getOptions()->get<options::SmartShapeOptions>();
    REQUIRE(finale2002TipAvoidanceOptions != nullptr);
    CHECK(finale2002TipAvoidanceOptions->slurAvoidStaffLinesAmt == 17);
    CHECK(field(finale2002TipAvoidance, "options.smartShapeOptions.slurAvoidStaffLinesAmt").origin == ValueOrigin::LegacyMus);
    CHECK(field(finale2002TipAvoidance, "options.smartShapeOptions.slurAvoidStaffLinesAmt").rawValue == 18);

    const auto finale2002Avoidance = Reader::readWithReport<TestXmlDocument>(
        std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F2002/F2002-slursavoid-no-acci.mus");
    const auto finale2002AvoidanceOptions = finale2002Avoidance.document->getOptions()->get<options::SmartShapeOptions>();
    REQUIRE(finale2002AvoidanceOptions != nullptr);
    CHECK_FALSE(finale2002AvoidanceOptions->slurAvoidAccidentals);
    CHECK(finale2002AvoidanceOptions->slurPadding == 37);
    CHECK(finale2002AvoidanceOptions->slurAcciPadding == 37);
    CHECK_FALSE(finale2002AvoidanceOptions->slurDoStretchFirst);
    CHECK(field(finale2002Avoidance, "options.smartShapeOptions.slurAcciPadding").origin == ValueOrigin::LegacyBehavior);
    CHECK(field(finale2002Avoidance, "options.smartShapeOptions.slurAcciPadding").rawValue == 37);
    CHECK(field(finale2002Avoidance, "options.smartShapeOptions.slurDoStretchFirst").origin == ValueOrigin::LegacyBehavior);

    const auto finale2003 =
        Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F2003/F2003-baseline.mus");
    const auto finale2003Options = finale2003.document->getOptions()->get<options::SmartShapeOptions>();
    REQUIRE(finale2003Options != nullptr);
    CHECK(finale2003Options->ssLineStyleCmpTabBendCurve == 3);
    CHECK(finale2003Options->slurAcciPadding == 3);
    CHECK(finale2003Options->slurDoStretchFirst);
    CHECK(finale2003.document->getOthers()->getArray<LineTarget>(SCORE_PARTID).size() == 3);
    CHECK(field(finale2003, "options.smartShapeOptions.ssLineStyleCmpTabBendCurve").origin == ValueOrigin::LegacyMus);
    CHECK(field(finale2003, "options.smartShapeOptions.slurAcciPadding").origin == ValueOrigin::LegacyMus);
    CHECK(field(finale2003, "options.smartShapeOptions.slurDoStretchFirst").origin == ValueOrigin::LegacyMus);
    CHECK(field(finale2003, "options.smartShapeOptions.bendCurveConnectStyles[7].connectIndex").origin == ValueOrigin::LegacyMus);

    for (const auto& test : std::array{std::pair{"F2008-empty.mus", musx::dom::ShapeDirection::Automatic},
             std::pair{"F2008-defss-over.mus", musx::dom::ShapeDirection::Over},
             std::pair{"F2008-defss-under.mus", musx::dom::ShapeDirection::Under}}) {
        const auto imported =
            Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F2008" / test.first);
        const auto importedOptions = imported.document->getOptions()->get<options::SmartShapeOptions>();
        REQUIRE(importedOptions != nullptr);
        CHECK(importedOptions->direction == test.second);
        CHECK(field(imported, "options.smartShapeOptions.direction").origin == ValueOrigin::LegacyMus);
    }

    const auto codaCurve =
        Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F263/F263-curve-opt-2.mus");
    const auto codaCurveOptions = codaCurve.document->getOptions()->get<options::SmartShapeOptions>();
    REQUIRE(codaCurveOptions != nullptr);
    CHECK(codaCurveOptions->slurThicknessCp1X == -13);
    CHECK(codaCurveOptions->slurThicknessCp1Y == -17);
    CHECK(codaCurveOptions->slurThicknessCp2X == -15);
    CHECK(codaCurveOptions->slurThicknessCp2Y == -19);
    CHECK(field(codaCurve, "options.smartShapeOptions.slurThicknessCp1Y").origin == ValueOrigin::LegacyMus);
    CHECK(field(codaCurve, "options.smartShapeOptions.slurThicknessCp2Y").rawValue == 19);

    const auto codaCurveVisual =
        Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F263/F263-curve-opt-3.mus");
    const auto codaCurveVisualOptions = codaCurveVisual.document->getOptions()->get<options::SmartShapeOptions>();
    REQUIRE(codaCurveVisualOptions != nullptr);
    CHECK(codaCurveVisualOptions->slurThicknessCp1X == 131);
    CHECK(codaCurveVisualOptions->slurThicknessCp1Y == 171);
    CHECK(codaCurveVisualOptions->slurThicknessCp2X == -173);
    CHECK(codaCurveVisualOptions->slurThicknessCp2Y == -179);

    // Zlib class 0x00de, Finale 2012: the character slot widens from one word to two and
    // every later field shifts by one word to keep the record 36 words long. cmper 2 here
    // is a bare Solid line (width 224); cmper 3 adds an ArrowheadPreset end cap, which
    // depends on the post-shift cap-type and cap-arrow slots being read correctly.
    const auto zlib =
        Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F2012/F2012-baseline.mus");
    const auto zlibOptions = zlib.document->getOptions()->get<options::SmartShapeOptions>();
    REQUIRE(zlibOptions != nullptr);
    CHECK(zlibOptions->slurConnectStyles.size() == 29);
    CHECK(zlibOptions->tabSlideConnectStyles.size() == 18);
    CHECK(zlibOptions->glissandoConnectStyles.size() == 2);
    CHECK(zlibOptions->bendCurveConnectStyles.size() == 8);
    CHECK(field(zlib, "options.smartShapeOptions.slurConnectStyles[28].yOffset").origin == ValueOrigin::LegacyMus);
    CHECK(field(zlib, "options.smartShapeOptions.bendCurveConnectStyles[7].xOffset").origin == ValueOrigin::LegacyMus);
    const auto zlibChar = zlib.document->getOthers()->get<LineTarget>(SCORE_PARTID, 1);
    REQUIRE(zlibChar != nullptr);
    REQUIRE(zlibChar->charParams != nullptr);
    CHECK(zlibChar->charParams->lineChar == U'~');
    CHECK(zlibChar->charParams->font->fontSize == 24);
    CHECK(zlibChar->charParams->baselineShiftEms == -88);

    // The same six line styles back-saved to Finale 2012. Only the Char parameter block moves
    // with the widened character: a Dashed record keeps its dash lengths where every earlier
    // layout put them, while everything after the parameter block shifts for all three styles.
    const auto wide =
        Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F2012/F2012-ssline-offsets.mus");
    const auto wideDashed = wide.document->getOthers()->get<LineTarget>(SCORE_PARTID, 4);
    REQUIRE(wideDashed != nullptr);
    REQUIRE(wideDashed->dashedParams != nullptr);
    CHECK(wideDashed->dashedParams->dashOn == 192);
    CHECK(wideDashed->dashedParams->dashOff == 448);
    const auto wideStyled = wide.document->getOthers()->get<LineTarget>(SCORE_PARTID, 5);
    REQUIRE(wideStyled != nullptr);
    REQUIRE(wideStyled->charParams != nullptr);
    // Already a code point in this era, so nothing is decoded and the answer is the same one
    // the fixed-row source reaches by decoding.
    CHECK(wideStyled->charParams->lineChar == U'\u00ab');
    CHECK(wideStyled->charParams->font->fontId == 6);
    CHECK(wideStyled->charParams->font->fontSize == 17);
    CHECK(wideStyled->charParams->font->bold);
    CHECK(wideStyled->charParams->font->italic);
    CHECK(wideStyled->charParams->baselineShiftEms == -83);
    const auto wideCapped = wide.document->getOthers()->get<LineTarget>(SCORE_PARTID, 6);
    REQUIRE(wideCapped != nullptr);
    CHECK(wideCapped->lineCapStartHookLength == 1984);
    CHECK(wideCapped->lineCapEndArrowId == 2);
    const auto widePositions = wide.document->getOthers()->get<LineTarget>(SCORE_PARTID, 3);
    REQUIRE(widePositions != nullptr);
    CHECK(widePositions->leftStartX == 11);
    CHECK(widePositions->centerAbbrY == 43);
    CHECK(widePositions->lineContX == 53);

    const auto zlibCapped = zlib.document->getOthers()->get<LineTarget>(SCORE_PARTID, 3);
    REQUIRE(zlibCapped != nullptr);
    CHECK(zlibCapped->lineStyle == LineTarget::LineStyle::Solid);
    REQUIRE(zlibCapped->solidParams != nullptr);
    CHECK(zlibCapped->solidParams->lineWidth == 224);
    CHECK(zlibCapped->lineCapEndType == LineTarget::LineCapType::ArrowheadPreset);
    CHECK(zlibCapped->lineCapEndArrowId == 1);
}

TEST_CASE("Smart shape custom lines", "[class][reader]")
{
    testSmartShapeCustomLines();
}

TEST_CASE("Smart shape custom lines span three epochs", "[class]")
{
    testSmartShapeCustomLinesAcrossEpochs();
}
TEST_CASE("Smart shape custom line Unicode layout", "[class]")
{
    testSmartShapeCustomLineUnicodeLayout();
}

} // namespace
} // namespace finale_mus_reader_tests
