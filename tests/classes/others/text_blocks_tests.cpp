// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include "import/texts.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

std::vector<SyntheticRow> textBlockFixedRows(std::uint16_t cmper, const std::vector<std::int16_t>& words)
{
    std::vector<SyntheticRow> rows;
    for (std::size_t at = 0; at < words.size(); at += 6) {
        SyntheticRow row{cmper, "TX", {}};
        for (std::size_t slot = 0; slot < 6; ++slot) {
            row.words[slot] = words[at + slot];
        }
        rows.push_back(row);
    }
    return rows;
}

void textBlockImport(const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile,
    const musx::dom::DocumentPtr& document, ImportReport& report, bool importTexts = false)
{
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    if (importTexts) {
        finale_mus_reader::texts::importTexts(context);
    }
    finale_mus_reader::others::importTextBlocks(context);
    // The Coda pass is deferred, because it pairs its rows against the imported text pool. A test
    // driving one importer alone still has to close the phase the registry closes.
    finale_mus_reader::runDeferredChecks(pending);
}

void testStoredTextBlocksAcrossEpochs()
{
    using Target = musx::dom::others::TextBlock;
    const std::vector<std::int16_t> words{9, 480, 240, 3, 125, -12, 34, 0x1e09, -1, -32, 0, 64, 2004, 0, 0, 0, 0, 0};
    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        ImportReport report(FormatEpoch::UncompressedLegacy);
        SourceProfile profile(epoch);
        profile.byteOrder = ByteOrder::BigEndian;
        if (epoch == FormatEpoch::ZlibLegacy) {
            textBlockImport(makeClassContainer(0x00b7, words, profile.byteOrder, 7), profile, document, report);
        } else {
            textBlockImport(makeContainer(textBlockFixedRows(7, words), epoch), profile, document, report);
        }
        const auto block = document->getOthers()->get<Target>(musx::dom::SCORE_PARTID, 7);
        expectMapping(block && block->textId == 9 && block->width == 480 && block->height == 240 && block->shapeId == 3
                          && block->lineSpacingPercentage == 125 && !block->lineSpacingEvpu && block->xAdd == -12 && block->yAdd == 34,
            "The stored TextBlock scalar fields were not recovered");
        expectMapping(
            block->justify == Target::TextJustify::Right && block->newPos36 && block->showShape && block->noExpandSingleWord && block->wordWrap,
            "The stored TextBlock flags were not recovered");
        expectMapping(block->inset == -32 && block->stdLineThickness == 64, "The stored TextBlock Efix values were not read high-word first");
        expectMapping(block->textType == Target::TextType::Block, "The stored TextBlock block-family discriminator was not recovered");
        expectMapping(!block->roundCorners && block->cornerRadius == 0
                          && field(report, "others.textBlock[7].roundCorners").origin == ValueOrigin::LegacyBehavior
                          && field(report, "others.textBlock[7].cornerRadius").origin == ValueOrigin::LegacyBehavior,
            "The legacy TextBlock corner behavior was not reported");
        expectMapping(field(report, "others.textBlock[7].justify").rawValue == 1 && field(report, "others.textBlock[7].textType").rawValue == 2004
                          && field(report, "others.textBlock[7].lineSpacingPercentage").rawValue == 125,
            "The TextBlock report did not retain its stored values");
    }

    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    ImportReport report(FormatEpoch::UncompressedLegacy);
    SourceProfile profile(FormatEpoch::UncompressedLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    auto absolute = words;
    absolute[4] = 72;
    absolute[7] = 2;
    textBlockImport(makeContainer(textBlockFixedRows(8, absolute)), profile, document, report);
    const auto block = document->getOthers()->get<Target>(musx::dom::SCORE_PARTID, 8);
    expectMapping(block && block->lineSpacingEvpu == 72 && !block->lineSpacingPercentage && block->justify == Target::TextJustify::Center,
        "The absolute TextBlock spacing or center justification was not recovered");

    auto zeroSpacingSession = musx::factory::DocumentFactory::begin();
    const auto zeroSpacingDocument = zeroSpacingSession.getDocument();
    ImportReport zeroSpacingReport(FormatEpoch::UncompressedLegacy);
    auto zeroSpacing = absolute;
    zeroSpacing[4] = 0;
    textBlockImport(makeContainer(textBlockFixedRows(10, zeroSpacing)), profile, zeroSpacingDocument, zeroSpacingReport);
    const auto zeroSpacingBlock = zeroSpacingDocument->getOthers()->get<Target>(musx::dom::SCORE_PARTID, 10);
    expectMapping(zeroSpacingBlock && zeroSpacingBlock->lineSpacingEvpu == 0 && !zeroSpacingBlock->lineSpacingPercentage
                      && field(zeroSpacingReport, "others.textBlock[10].lineSpacingEvpu").rawValue == 0,
        "Zero-EVPU TextBlock spacing was not preserved as the stored setting");

    auto zeroPercentSession = musx::factory::DocumentFactory::begin();
    const auto zeroPercentDocument = zeroPercentSession.getDocument();
    ImportReport zeroPercentReport(FormatEpoch::UncompressedLegacy);
    auto zeroPercent = words;
    zeroPercent[4] = 0;
    textBlockImport(makeContainer(textBlockFixedRows(11, zeroPercent)), profile, zeroPercentDocument, zeroPercentReport);
    const auto zeroPercentBlock = zeroPercentDocument->getOthers()->get<Target>(musx::dom::SCORE_PARTID, 11);
    expectMapping(zeroPercentBlock && zeroPercentBlock->lineSpacingEvpu == 0 && !zeroPercentBlock->lineSpacingPercentage
                      && field(zeroPercentReport, "others.textBlock[11].lineSpacingPercentage").rawValue == 0
                      && field(zeroPercentReport, "others.textBlock[11].lineSpacingEvpu").origin == ValueOrigin::LegacyBehavior,
        "Zero-percent TextBlock spacing was not upgraded to zero EVPU");

    for (const std::int16_t discriminator : {static_cast<std::int16_t>(finale_mus_reader::records::packTag("xp")), std::int16_t(2006)}) {
        auto expressionSession = musx::factory::DocumentFactory::begin();
        const auto expressionDocument = expressionSession.getDocument();
        ImportReport expressionReport(FormatEpoch::UncompressedLegacy);
        auto expression = words;
        expression[12] = discriminator;
        textBlockImport(makeContainer(textBlockFixedRows(9, expression), FormatEpoch::DclLegacy), profile, expressionDocument, expressionReport);
        const auto expressionBlock = expressionDocument->getOthers()->get<Target>(musx::dom::SCORE_PARTID, 9);
        expectMapping(expressionBlock && expressionBlock->textType == Target::TextType::Expression
                          && field(expressionReport, "others.textBlock[9].textType").rawValue == discriminator,
            "A stored TextBlock expression-family discriminator was not recovered");
    }

    auto taggedBlockSession = musx::factory::DocumentFactory::begin();
    const auto taggedBlockDocument = taggedBlockSession.getDocument();
    ImportReport taggedBlockReport(FormatEpoch::UncompressedLegacy);
    auto taggedBlockWords = words;
    taggedBlockWords[12] = static_cast<std::int16_t>(finale_mus_reader::records::packTag("bl"));
    textBlockImport(makeContainer(textBlockFixedRows(10, taggedBlockWords)), profile, taggedBlockDocument, taggedBlockReport);
    const auto taggedBlock = taggedBlockDocument->getOthers()->get<Target>(musx::dom::SCORE_PARTID, 10);
    expectMapping(taggedBlock && taggedBlock->textType == Target::TextType::Block
                      && field(taggedBlockReport, "others.textBlock[10].textType").rawValue == taggedBlockWords[12],
        "The packed block-family TextBlock discriminator was not recovered");

    auto oldSession = musx::factory::DocumentFactory::begin();
    const auto oldDocument = oldSession.getDocument();
    ImportReport oldReport(FormatEpoch::UncompressedLegacy);
    auto oldWords = words;
    oldWords[12] = 0;
    textBlockImport(makeContainer(textBlockFixedRows(11, oldWords)), profile, oldDocument, oldReport);
    const auto oldBlock = oldDocument->getOthers()->get<Target>(musx::dom::SCORE_PARTID, 11);
    expectMapping(
        oldBlock && oldBlock->textType == Target::TextType::Block, "A TextBlock without a family discriminator did not retain the block default");
}

void testCodaTextBlockSynthesis()
{
    using Target = musx::dom::others::TextBlock;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    const auto parsed =
        makeContainer({{0, "HS", {0, 0, 1036, 0, 0, 0x0080}}, {0, "HS", {0, 0, 1036, 0, 0, 0x0081}}, {0, "HT", {0x4100}}, {0, "HT", {}},
                          {0, "HT", {}}, {0, "HT", {}}, {0, "HT", {0x4200}}, {0, "HT", {}}, {0, "HT", {}}, {0, "HT", {}}},
            FormatEpoch::CodaBanner);
    ImportReport report(FormatEpoch::UncompressedLegacy);
    SourceProfile profile(FormatEpoch::CodaBanner);
    profile.byteOrder = ByteOrder::BigEndian;
    textBlockImport(parsed, profile, document, report, true);

    const auto first = document->getOthers()->get<Target>(musx::dom::SCORE_PARTID, 1);
    const auto second = document->getOthers()->get<Target>(musx::dom::SCORE_PARTID, 2);
    expectMapping(first && first->textId == 1 && first->justify == Target::TextJustify::Left && second && second->textId == 2
                      && second->justify == Target::TextJustify::Right,
        "Coda TextBlocks did not follow the HS/HT structural order");
    expectMapping(first->lineSpacingPercentage == 100 && !first->newPos36 && first->shapeId == 0 && !first->showShape && first->wordWrap
                      && !first->noExpandSingleWord && !second->noExpandSingleWord && !first->roundCorners && first->cornerRadius == 0,
        "Coda TextBlock behavior was not synthesized");
    expectMapping(field(report, "others.textBlock[1].textId").origin == ValueOrigin::LegacyMus
                      && field(report, "others.textBlock[1].newPos36").origin == ValueOrigin::LegacyBehavior
                      && field(report, "others.textBlock[1].shapeId").origin == ValueOrigin::LegacyBehavior
                      && field(report, "others.textBlock[1].showShape").origin == ValueOrigin::LegacyBehavior
                      && field(report, "others.textBlock[1].noExpandSingleWord").origin == ValueOrigin::LegacyBehavior
                      && field(report, "others.textBlock[1].wordWrap").origin == ValueOrigin::LegacyBehavior
                      && field(report, "others.textBlock[1].roundCorners").origin == ValueOrigin::LegacyBehavior
                      && field(report, "others.textBlock[1].cornerRadius").origin == ValueOrigin::LegacyBehavior,
        "Coda stored identity and era behavior were not reported separately");
}

TEST_CASE("Stored text blocks span three epochs", "[class]")
{
    testStoredTextBlocksAcrossEpochs();
}

TEST_CASE("Coda text blocks are assembled from text structure", "[class]")
{
    testCodaTextBlockSynthesis();
}
} // namespace
} // namespace finale_mus_reader_tests
