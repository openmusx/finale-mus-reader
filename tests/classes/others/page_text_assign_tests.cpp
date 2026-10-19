// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <memory>
#include <optional>
#include <utility>

#include "import/texts.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using PageTextTarget = musx::dom::others::PageTextAssign;

void pageTextImport(const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile, const musx::dom::DocumentPtr& document,
    ImportReport& report, std::optional<musx::dom::Cmper> pairedBlock = std::nullopt, bool importTexts = false)
{
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    if (pairedBlock) {
        pending.codaTextBlockByStyle.emplace(std::pair{musx::dom::Cmper{1}, musx::dom::Inci{0}}, *pairedBlock);
    }
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    if (importTexts) {
        finale_mus_reader::texts::importTexts(context);
        finale_mus_reader::others::importTextBlocks(context);
    }
    finale_mus_reader::others::importPageTextAssigns(context);
    finale_mus_reader::runDeferredChecks(pending);
}

TEST_CASE("Early HS text size and lower handle use the uncompressed layout", "[class][page-text]")
{
    using BlockText = musx::dom::texts::BlockText;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    SourceProfile profile(FormatEpoch::UncompressedLegacy);
    profile.version = SourceVersion{.major = 3, .minor = 5};
    profile.byteOrder = ByteOrder::BigEndian;
    ImportReport report(profile.epoch);
    pageTextImport(makeContainer({{1, "HS", {0, 516, 0x0e04, 0, 0, 0x0080}}, {1, "HS", {0, 96, 0x0e04, 0, 0, 0}}, {1, "HT", {0x4100}}, {1, "HT", {}},
                                     {1, "HT", {}}, {1, "HT", {}}, {1, "HT", {0x4200}}, {1, "HT", {}}, {1, "HT", {}}, {1, "HT", {}}},
                       profile.epoch),
        profile, document, report, std::nullopt, true);

    const auto top = document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{0});
    const auto lower = document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{1});
    REQUIRE(top);
    REQUIRE(lower);
    CHECK(top->yDisp == -474);
    CHECK(top->vPos == PageTextTarget::VerticalAlignment::Top);
    CHECK(lower->yDisp == 84);
    CHECK(lower->vPos == PageTextTarget::VerticalAlignment::Bottom);
    const auto firstText = document->getTexts()->get<BlockText>(1);
    const auto secondText = document->getTexts()->get<BlockText>(2);
    REQUIRE(firstText);
    REQUIRE(secondText);
    CHECK(firstText->text.find("^fontid(4)^size(14)") == 0);
    CHECK(secondText->text.find("^fontid(4)^size(14)") == 0);
}

TEST_CASE("Page text fields and incidence are decoded from fixed rows", "[class][page-text]")
{
    const std::vector<std::int16_t> first{7, -12, 34, 1, 0, 0x0ed9, -24, 48, 0, 0, 0, 0};
    const std::vector<std::int16_t> second{9, 10, -20, 1, 1, 0x0128, 0, 0, 0, 0, 0, 0};
    std::vector<SyntheticRow> rows;
    for (const auto& words : {first, second}) {
        for (std::size_t at = 0; at < words.size(); at += 6) {
            SyntheticRow row{1, "pT", {}};
            for (std::size_t slot = 0; slot < 6; ++slot) {
                row.words[slot] = words[at + slot];
            }
            rows.push_back(row);
        }
    }
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    ImportReport report(FormatEpoch::UncompressedLegacy);
    SourceProfile profile(FormatEpoch::UncompressedLegacy);
    profile.version = SourceVersion{.major = 3, .minor = 7};
    profile.byteOrder = ByteOrder::BigEndian;
    pageTextImport(makeContainer(rows), profile, document, report);
    const auto left = document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{0});
    const auto right = document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{1});
    REQUIRE(left);
    REQUIRE(right);
    CHECK(left->block == 7);
    CHECK(left->xDisp == -12);
    CHECK(left->yDisp == 34);
    CHECK(left->endPage == 0);
    CHECK(left->oddEven == PageTextTarget::PageAssignType::Odd);
    CHECK(left->hPosLp == PageTextTarget::HorizontalAlignment::Center);
    CHECK(left->hPosRp == PageTextTarget::HorizontalAlignment::Right);
    CHECK(left->hidden);
    CHECK(left->vPos == PageTextTarget::VerticalAlignment::Center);
    CHECK(left->hPosPageEdge);
    CHECK(left->vPosPageEdge);
    CHECK(left->indRpPos);
    CHECK(left->rightPgXDisp == -24);
    CHECK(left->rightPgYDisp == 48);
    CHECK(right->block == 9);
    CHECK(field(report, "others.pageTextAssign[1,0].block").origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Finale 97 page text references its stored TextBlock", "[class][page-text]")
{
    const auto result = readFixture("evidence/F97/Fin97-baseline.mus");
    const auto assignment = result.document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{0}, musx::dom::Inci{0});
    REQUIRE(assignment);
    CHECK(assignment->block == 2);
    const auto block = assignment->getTextBlock();
    REQUIRE(block);
    CHECK(block->getCmper() == 2);
    CHECK(field(result.report, "others.pageTextAssign[0,0].block").origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Coda pT and HS use distinct numbered and HT block texts", "[class][page-text]")
{
    using BlockText = musx::dom::texts::BlockText;
    auto parsed =
        makeContainer({{5, "pT", {2, 0, 0, 0, 0, 0}}, {9, "pT", {6, 0, 0, 0, 0, 0}}, {2, "PT", {2}}, {6, "PT", {6}},
                          {0, "HS", {0, 0, 0x010c, 0, 0, 0x0080}}, {0, "HT", {0x4865, 0x6164, 0x6572}}, {0, "HT", {}}, {0, "HT", {}}, {0, "HT", {}}},
            FormatEpoch::CodaBanner);
    const std::string textRegion = "^text()^block(2)First paragraph^block(6)Second paragraph";
    const auto length = static_cast<std::uint32_t>(textRegion.size());
    for (unsigned shift : {24U, 16U, 8U, 0U}) {
        parsed.textRegion.push_back(static_cast<std::uint8_t>(length >> shift));
    }
    parsed.textRegion.insert(parsed.textRegion.end(), textRegion.begin(), textRegion.end());
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    SourceProfile profile(FormatEpoch::CodaBanner);
    profile.byteOrder = ByteOrder::BigEndian;
    ImportReport report(profile.epoch);
    pageTextImport(parsed, profile, document, report, std::nullopt, true);

    const auto first = document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{5}, musx::dom::Inci{0});
    const auto second = document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{9}, musx::dom::Inci{0});
    const auto header = document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{0}, musx::dom::Inci{0});
    REQUIRE(first);
    REQUIRE(second);
    REQUIRE(header);
    CHECK(first->block == 2);
    CHECK(second->block == 6);
    CHECK(header->block == 7);
    const auto firstText = document->getTexts()->get<BlockText>(2);
    const auto secondText = document->getTexts()->get<BlockText>(6);
    const auto headerText = document->getTexts()->get<BlockText>(7);
    const auto firstBlock = first->getTextBlock();
    const auto secondBlock = second->getTextBlock();
    const auto headerBlock = header->getTextBlock();
    REQUIRE(firstText);
    REQUIRE(secondText);
    REQUIRE(headerText);
    REQUIRE(firstBlock);
    REQUIRE(secondBlock);
    REQUIRE(headerBlock);
    CHECK(firstText->text.find("First paragraph") != std::string::npos);
    CHECK(secondText->text.find("Second paragraph") != std::string::npos);
    CHECK(headerText->text.find("Header") != std::string::npos);
    CHECK(firstBlock->textId == 2);
    CHECK(secondBlock->textId == 6);
    CHECK(headerBlock->textId == 7);
    CHECK_FALSE(firstBlock->showShape);
    CHECK_FALSE(secondBlock->showShape);
    CHECK(field(report, "others.textBlock[2].showShape").origin == ValueOrigin::LegacyBehavior);
}

TEST_CASE("Finale 3.7 page text uses fixed pT tuples", "[class][page-text]")
{
    const auto result = readFixture("evidence/F372/F372-fileinfo-text.mus");
    const auto first = result.document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{0});
    const auto second = result.document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{1});
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first->block == 1);
    CHECK(first->xDisp == 96);
    CHECK(first->yDisp == -336);
    CHECK(second->block == 3);
    CHECK(second->yDisp == -496);
    CHECK(field(result.report, "others.pageTextAssign[1,0].yDisp").origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Early pT and HS assignments coexist while later files use pT", "[class][page-text]")
{
    const SyntheticRow legacy{1, "HS", {7, 11, 0x0c01, 0, 0, 0x0080}};
    const SyntheticRow legacyText{1, "HT", {0x4100}};
    const SyntheticRow fixedFirst{1, "pT", {9, 7, -11, 1, 1, 0}};
    const SyntheticRow fixedSecond{1, "pT", {0, 0, 0, 0, 0, 0}};
    const auto read = [&](const std::vector<SyntheticRow>& rows, FormatEpoch epoch, SourceVersion version) {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        ImportReport report(epoch);
        SourceProfile profile(epoch);
        profile.version = version;
        profile.byteOrder = ByteOrder::BigEndian;
        pageTextImport(makeContainer(rows, epoch), profile, document, report, musx::dom::Cmper{7});
        return document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{0});
    };

    const auto early = read({legacy, legacyText}, FormatEpoch::UncompressedLegacy, SourceVersion{.major = 3, .minor = 5});
    REQUIRE(early);
    CHECK(early->block == 7);
    CHECK(early->yDisp == 25);

    const auto mixedEarly =
        read({legacy, legacyText, fixedFirst, fixedSecond}, FormatEpoch::UncompressedLegacy, SourceVersion{.major = 3, .minor = 5});
    REQUIRE(mixedEarly);
    CHECK(mixedEarly->block == 9);
    CHECK(mixedEarly->xDisp == 7);
    CHECK(mixedEarly->yDisp == -11);

    const auto earlyPt = read({fixedFirst, fixedSecond}, FormatEpoch::UncompressedLegacy, SourceVersion{.major = 3, .minor = 5});
    REQUIRE(earlyPt);
    CHECK(earlyPt->block == 9);

    const auto mixedModern =
        read({legacy, legacyText, fixedFirst, fixedSecond}, FormatEpoch::UncompressedLegacy, SourceVersion{.major = 3, .minor = 7});
    REQUIRE(mixedModern);
    CHECK(mixedModern->block == 9);
    CHECK(mixedModern->yDisp == -11);

    CHECK_FALSE(read({legacy, legacyText}, FormatEpoch::UncompressedLegacy, SourceVersion{.major = 3, .minor = 7}));
    CHECK_FALSE(read({legacy, legacyText}, FormatEpoch::CodaBanner, SourceVersion{.major = 3, .minor = 7}));
}

TEST_CASE("Early pT and HS assignments link distinct TextBlocks", "[class][page-text]")
{
    using BlockText = musx::dom::texts::BlockText;
    using TextBlock = musx::dom::others::TextBlock;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto text = std::make_shared<BlockText>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper{1});
    text->text = "Allegretto";
    document->getTexts()->add(BlockText::XmlNodeName, std::move(text));
    SourceProfile profile(FormatEpoch::UncompressedLegacy);
    profile.version = SourceVersion{.major = 3, .minor = 5};
    profile.byteOrder = ByteOrder::BigEndian;
    ImportReport report(profile.epoch);
    pageTextImport(makeContainer({{1, "pT", {2, 429, -173, 0, 0, 0}}, {1, "PT", {0}}, {2, "PT", {1}}, {1, "HS", {7, 11, 268, 0, 0, 0x0082}},
                                     {1, "HT", {0x5469, 0x746c, 0x6500}}, {1, "HT", {}}, {1, "HT", {}}, {1, "HT", {}}},
                       profile.epoch),
        profile, document, report, std::nullopt, true);
    const auto first = document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{0});
    const auto second = document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{1});
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first->block == 2);
    CHECK(first->xDisp == 429);
    CHECK(first->rightPgXDisp == 0);
    CHECK(first->rightPgYDisp == 0);
    CHECK(field(report, "others.pageTextAssign[1,0].rightPgXDisp").origin == ValueOrigin::LegacyBehavior);
    CHECK(field(report, "others.pageTextAssign[1,0].rightPgYDisp").origin == ValueOrigin::LegacyBehavior);
    CHECK(first->hPosLp == PageTextTarget::HorizontalAlignment::Left);
    CHECK(second->block == 3);
    CHECK(second->hPosLp == PageTextTarget::HorizontalAlignment::Center);
    const auto firstBlock = first->getTextBlock();
    const auto secondBlock = second->getTextBlock();
    REQUIRE(firstBlock);
    REQUIRE(secondBlock);
    CHECK(firstBlock->textId == 1);
    CHECK(firstBlock->justify == TextBlock::TextJustify::Left);
    CHECK(secondBlock->textId == 2);
    CHECK(secondBlock->justify == TextBlock::TextJustify::Center);
    CHECK(firstBlock->showShape);
    CHECK(secondBlock->showShape);
    CHECK(field(report, "others.textBlock[2].textId").origin == ValueOrigin::LegacyMus);
}

TEST_CASE("The PT globals record does not take an early HS block's comparator", "[class][page-text]")
{
    using BlockText = musx::dom::texts::BlockText;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto text = std::make_shared<BlockText>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper{1});
    text->text = "Allegretto";
    document->getTexts()->add(BlockText::XmlNodeName, std::move(text));
    SourceProfile profile(FormatEpoch::UncompressedLegacy);
    profile.version = SourceVersion{.major = 3, .minor = 5};
    profile.byteOrder = ByteOrder::BigEndian;
    ImportReport report(profile.epoch);
    // Two styled blocks: numbering from the globals comparator leaves room for only one.
    pageTextImport(
        makeContainer({{1, "pT", {2, 429, -173, 0, 0, 0}}, {1, "PT", {0}}, {2, "PT", {1}}, {65534, "PT", {0}}, {1, "HS", {7, 11, 268, 0, 0, 0x0082}},
                          {1, "HS", {7, 11, 300, 0, 0, 0x0082}}, {1, "HT", {0x5469, 0x746c, 0x6500}}, {1, "HT", {}}, {1, "HT", {}}, {1, "HT", {}},
                          {1, "HT", {0x4f6e, 0x6500}}, {1, "HT", {}}, {1, "HT", {}}, {1, "HT", {}}},
            profile.epoch),
        profile, document, report, std::nullopt, true);
    const auto connected = document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{0});
    const auto styled = document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{1});
    const auto second = document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{2});
    REQUIRE(connected);
    REQUIRE(styled);
    REQUIRE(second);
    CHECK(connected->block == 2);
    REQUIRE(styled->getTextBlock());
    REQUIRE(second->getTextBlock());
    CHECK(styled->block != connected->block);
    CHECK(second->block != styled->block);
    CHECK(second->block != connected->block);
    CHECK(std::none_of(report.diagnostics.begin(), report.diagnostics.end(),
        [](const auto& d) { return d.message.find("exceeds the available comparators") != std::string::npos; }));
}

TEST_CASE("Early pT resolves its PT connector before the 3.7 gate", "[class][page-text]")
{
    using BlockText = musx::dom::texts::BlockText;
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy}) {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        auto text = std::make_shared<BlockText>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper{1});
        text->text = "Copyright";
        document->getTexts()->add(BlockText::XmlNodeName, std::move(text));
        SourceProfile profile(epoch);
        profile.version = epoch == FormatEpoch::CodaBanner ? SourceVersion{.major = 2, .minor = 6} : SourceVersion{.major = 3, .minor = 5};
        profile.byteOrder = ByteOrder::BigEndian;
        ImportReport report(epoch);
        pageTextImport(makeContainer({{1, "pT", {2, 0, 0, 0, 0, 0}}, {2, "PT", {1}}}, epoch), profile, document, report);
        const auto assignment = document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{0});
        REQUIRE(assignment);
        CHECK(assignment->block == 2);
        const auto block = assignment->getTextBlock();
        REQUIRE(block);
        CHECK(block->textId == 1);
        CHECK(field(report, "others.textBlock[2].textId").origin == ValueOrigin::LegacyMus);
    }
}

TEST_CASE("Page text tuple decoding covers DCL and zlib framing", "[class][page-text]")
{
    const std::vector<std::int16_t> words{7, -12, 34, 1, 1, 0x0121, 0, 0, 0, 0, 0, 0};
    for (const auto epoch : {FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        ImportReport report(epoch);
        SourceProfile profile(epoch);
        profile.byteOrder = ByteOrder::BigEndian;
        if (epoch == FormatEpoch::ZlibLegacy) {
            pageTextImport(makeClassContainer(0x00c2, words, profile.byteOrder, 1), profile, document, report);
        } else {
            SyntheticRow first{1, "pT", {}};
            SyntheticRow second{1, "pT", {}};
            for (std::size_t slot = 0; slot < 6; ++slot) {
                first.words[slot] = words[slot];
                second.words[slot] = words[slot + 6];
            }
            pageTextImport(makeContainer({first, second}, epoch), profile, document, report);
        }
        const auto assignment = document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{0});
        REQUIRE(assignment);
        CHECK(assignment->block == 7);
        CHECK(assignment->oddEven == PageTextTarget::PageAssignType::Odd);
        CHECK(field(report, "others.pageTextAssign[1,0].block").origin == ValueOrigin::LegacyMus);
    }
}

TEST_CASE("Rejected page text tuples do not leave output incidence gaps", "[class][page-text]")
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    SourceProfile profile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    ImportReport report(profile.epoch);
    pageTextImport(makeClassContainer(0x00c2, {7, 0, 0, 1, 1, 3, 0, 0, 0, 0, 0, 0, 9, 10, -20, 1, 1, 0x0121, 0, 0, 0, 0, 0, 0}, profile.byteOrder, 1),
        profile, document, report);
    const auto assignment = document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{0});
    REQUIRE(assignment);
    CHECK(assignment->block == 9);
    CHECK(assignment->xDisp == 10);
    CHECK_FALSE(document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{1}));
    CHECK(field(report, "others.pageTextAssign[1,0].block").origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Finale 2012 page text part records overlay unlinked fields", "[class][page-text]")
{
    const auto linked = readFixture("evidence/F2012/F2012-pagetext-part.mus");
    const auto linkedLeft = linked.document->getOthers()->get<PageTextTarget>(musx::dom::Cmper{1}, musx::dom::Cmper{1}, musx::dom::Inci{0});
    const auto linkedRight = linked.document->getOthers()->get<PageTextTarget>(musx::dom::Cmper{1}, musx::dom::Cmper{1}, musx::dom::Inci{1});
    REQUIRE(linkedLeft);
    REQUIRE(linkedRight);
    CHECK(linkedLeft->getSourcePartId() == musx::dom::SCORE_PARTID);
    CHECK(linkedRight->getSourcePartId() == musx::dom::SCORE_PARTID);

    const auto unlinked = readFixture("evidence/F2012/F2012-pagetext-part-unlinked.mus");
    const auto scoreLeft = unlinked.document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{0});
    const auto scoreRight = unlinked.document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{1});
    const auto partLeft = unlinked.document->getOthers()->get<PageTextTarget>(musx::dom::Cmper{1}, musx::dom::Cmper{1}, musx::dom::Inci{0});
    const auto partRight = unlinked.document->getOthers()->get<PageTextTarget>(musx::dom::Cmper{1}, musx::dom::Cmper{1}, musx::dom::Inci{1});
    REQUIRE(scoreLeft);
    REQUIRE(scoreRight);
    REQUIRE(partLeft);
    REQUIRE(partRight);
    CHECK(partLeft->getSourcePartId() == 1);
    CHECK(partRight->getSourcePartId() == 1);
    CHECK(partLeft->getShareMode() == musx::dom::EnigmaBase::ShareMode::Partial);
    CHECK(partRight->getShareMode() == musx::dom::EnigmaBase::ShareMode::Partial);
    CHECK_FALSE(scoreLeft->hidden);
    CHECK(partLeft->hidden);
    CHECK(partLeft->block == scoreLeft->block);
    CHECK(partLeft->xDisp == scoreLeft->xDisp);
    CHECK(partLeft->yDisp == scoreLeft->yDisp);
    CHECK(partRight->block == scoreRight->block);
    CHECK(partRight->xDisp == -28);
    CHECK(partRight->yDisp == -892);
    CHECK(scoreRight->xDisp == -196);
    CHECK(scoreRight->yDisp == -780);
}

TEST_CASE("Coda-banner page text resolves its synthesized TextBlock", "[class][page-text]")
{
    const auto result = readFixture("evidence/F100/F100-pagetitle.mus");
    const auto assignment = result.document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{0});
    REQUIRE(assignment);
    CHECK(assignment->getTextBlock());
    CHECK(assignment->block == assignment->getTextBlock()->getCmper());
    CHECK(assignment->xDisp == 420);
    CHECK(assignment->yDisp == -517);
    CHECK(assignment->startPage == 1);
    CHECK(assignment->endPage == 1);
    CHECK(assignment->hPosLp == PageTextTarget::HorizontalAlignment::Center);
    CHECK(field(result.report, "others.pageTextAssign[1,0].block").origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(field(result.report, "others.pageTextAssign[1,0].yDisp").origin == ValueOrigin::LegacyMusAdjusted);
    CHECK(field(result.report, "others.pageTextAssign[1,0].yDisp").rawValue == 556);
    CHECK(field(result.report, "others.pageTextAssign[1,0].rightPgXDisp").origin == ValueOrigin::Unmapped);

    const auto older = readFixture("evidence/F263/F263-staffopts.mus");
    const auto range = older.document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{0}, musx::dom::Inci{0});
    REQUIRE(range);
    CHECK(range->getTextBlock());
    CHECK(range->startPage == 1);
    CHECK(range->endPage == 0);
    CHECK(range->oddEven == PageTextTarget::PageAssignType::Even);
    CHECK(range->yDisp == 42);
    CHECK(older.document->getOthers()->getArray<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}).size() == 7);

    const auto moved = readFixture("evidence/F100/F100-pagetitle-ydisp.mus");
    const auto movedAssignment = moved.document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{0});
    REQUIRE(movedAssignment);
    CHECK(movedAssignment->getTextBlock());
    CHECK(movedAssignment->yDisp == -557);
    CHECK(field(moved.report, "others.pageTextAssign[1,0].yDisp").rawValue == 596);

    const auto upgraded = readFixture("evidence/F263/F263-F100-pagetitle-ydisp.mus");
    const auto upgradedAssignment =
        upgraded.document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{1}, musx::dom::Inci{0});
    REQUIRE(upgradedAssignment);
    CHECK(upgradedAssignment->getTextBlock());
    CHECK(upgradedAssignment->yDisp == movedAssignment->yDisp);

    const auto header = readFixture("evidence/F100/F100-header-x7-y11.mus");
    const auto headerAssignment = header.document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{0}, musx::dom::Inci{0});
    REQUIRE(headerAssignment);
    CHECK(headerAssignment->getTextBlock());
    CHECK(headerAssignment->xDisp == 7);
    CHECK(headerAssignment->yDisp == 25);
    CHECK(headerAssignment->oddEven == PageTextTarget::PageAssignType::Odd);
    CHECK(headerAssignment->startPage == 1);
    CHECK(headerAssignment->hPosLp == PageTextTarget::HorizontalAlignment::Right);
    CHECK(headerAssignment->vPos == PageTextTarget::VerticalAlignment::Top);
    CHECK(field(header.report, "others.pageTextAssign[0,0].yDisp").rawValue == 11);

    const auto footer = readFixture("evidence/F100/F100-footer-x7-y11.mus");
    const auto footerAssignment = footer.document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{0}, musx::dom::Inci{0});
    REQUIRE(footerAssignment);
    CHECK(footerAssignment->getTextBlock());
    CHECK(footerAssignment->xDisp == 7);
    CHECK(footerAssignment->yDisp == -1);
    CHECK(footerAssignment->oddEven == PageTextTarget::PageAssignType::Even);
    CHECK(footerAssignment->startPage == 1);
    CHECK(footerAssignment->hPosLp == PageTextTarget::HorizontalAlignment::Left);
    CHECK(footerAssignment->vPos == PageTextTarget::VerticalAlignment::Bottom);
    CHECK(field(footer.report, "others.pageTextAssign[0,0].yDisp").rawValue == 11);
}

TEST_CASE("An empty Coda text slot does not consume a page assignment incidence", "[class][page-text]")
{
    std::vector<SyntheticRow> rows{
        {0, "HS", {10, 0, 268, 0, 0, 0x0080}}, {0, "HS", {20, 0, 268, 0, 0, 0x0080}}, {0, "HS", {30, 0, 268, 0, 0, 0x0080}}};
    for (const auto firstWord : {std::int16_t{0x4100}, std::int16_t{0}, std::int16_t{0x4200}}) {
        rows.push_back({0, "HT", {firstWord}});
        for (int continuation = 0; continuation < 3; ++continuation) {
            rows.push_back({0, "HT", {}});
        }
    }

    const auto index = LegacyRecordIndex::build(makeContainer(rows, FormatEpoch::CodaBanner));
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    ImportReport report(FormatEpoch::CodaBanner);
    SourceProfile profile(FormatEpoch::CodaBanner);
    profile.version = SourceVersion{.major = 2, .minor = 6};
    profile.byteOrder = ByteOrder::BigEndian;
    finale_mus_reader::PendingReferences pending;
    pending.codaTextBlockByStyle.emplace(std::pair{musx::dom::Cmper{0}, musx::dom::Inci{0}}, musx::dom::Cmper{11});
    pending.codaTextBlockByStyle.emplace(std::pair{musx::dom::Cmper{0}, musx::dom::Inci{1}}, musx::dom::Cmper{22});
    pending.codaTextBlockByStyle.emplace(std::pair{musx::dom::Cmper{0}, musx::dom::Inci{2}}, musx::dom::Cmper{33});
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importPageTextAssigns(context);
    finale_mus_reader::runDeferredChecks(pending);

    const auto first = document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{0}, musx::dom::Inci{0});
    const auto second = document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{0}, musx::dom::Inci{1});
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first->block == 11);
    CHECK(first->xDisp == 10);
    CHECK(second->block == 33);
    CHECK(second->xDisp == 30);
    CHECK_FALSE(document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{0}, musx::dom::Inci{2}));
    CHECK(field(report, "others.pageTextAssign[0,1].block").rawValue == 2);
}

TEST_CASE("Adding Coda measure text leaves page text assignments unchanged", "[class][page-text]")
{
    const auto header = readFixture("evidence/F100/F100-header.mus");
    const auto withMeasureText = readFixture("evidence/F100/F100-header-meastext.mus");
    const auto first = header.document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{0}, musx::dom::Inci{0});
    const auto second = withMeasureText.document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{0}, musx::dom::Inci{0});
    REQUIRE(first);
    REQUIRE(second);
    const auto firstBlock = first->getTextBlock();
    const auto secondBlock = second->getTextBlock();
    REQUIRE(firstBlock);
    REQUIRE(secondBlock);
    const auto firstText = header.document->getTexts()->get<musx::dom::texts::BlockText>(firstBlock->textId);
    const auto secondText = withMeasureText.document->getTexts()->get<musx::dom::texts::BlockText>(secondBlock->textId);
    REQUIRE(firstText);
    REQUIRE(secondText);
    CHECK(secondText->text == firstText->text);
    CHECK(second->xDisp == first->xDisp);
    CHECK(second->yDisp == first->yDisp);
    CHECK(withMeasureText.document->getOthers()->getArray<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{0}).size() == 1);
}

TEST_CASE("HS Except Page 1 starts a repeating page text assignment on page two", "[class][page-text]")
{
    const auto baseline = readFixture("evidence/F263/F263-F100-header.mus");
    const auto exceptFirst = readFixture("evidence/F263/F263-F100-header-excp1.mus");
    const auto ordinary = baseline.document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{0}, musx::dom::Inci{0});
    const auto excluded = exceptFirst.document->getOthers()->get<PageTextTarget>(musx::dom::SCORE_PARTID, musx::dom::Cmper{0}, musx::dom::Inci{0});
    REQUIRE(ordinary);
    REQUIRE(excluded);
    CHECK(ordinary->startPage == 1);
    CHECK(excluded->startPage == 2);
    CHECK(ordinary->endPage == 0);
    CHECK(excluded->endPage == 0);
    CHECK(ordinary->oddEven == PageTextTarget::PageAssignType::Odd);
    CHECK(excluded->oddEven == ordinary->oddEven);
    CHECK(excluded->block == ordinary->block);
    CHECK(field(baseline.report, "others.pageTextAssign[0,0].startPage").origin == ValueOrigin::LegacyBehavior);
    CHECK(field(exceptFirst.report, "others.pageTextAssign[0,0].startPage").origin == ValueOrigin::LegacyBehavior);
}

} // namespace
} // namespace finale_mus_reader_tests
