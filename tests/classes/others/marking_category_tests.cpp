// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using Category = musx::dom::others::MarkingCategory;
using CategoryName = musx::dom::others::MarkingCategoryName;

musx::dom::DocumentPtr emptyMarkingCategoryDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    return session.getDocument();
}

musx::dom::DocumentPtr markingCategoryBaseline()
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    for (musx::dom::Cmper cmper = 1; cmper <= 7; ++cmper) {
        auto category = std::make_shared<Category>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, cmper);
        category->categoryType = static_cast<Category::CategoryType>(cmper);
        category->textFont = std::make_shared<musx::dom::FontInfo>(document);
        category->musicFont = std::make_shared<musx::dom::FontInfo>(document);
        category->numberFont = std::make_shared<musx::dom::FontInfo>(document);
        document->getOthers()->add(Category::XmlNodeName, category);
        auto name = std::make_shared<CategoryName>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, cmper);
        name->name = "Baseline " + std::to_string(cmper);
        document->getOthers()->add(CategoryName::XmlNodeName, name);
    }
    return std::move(session).finish();
}

ImportReport markingCategoryImport(const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile,
    const musx::dom::DocumentPtr& document, const musx::dom::DocumentPtr& reference = markingCategoryBaseline())
{
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importMarkingCategories(context);
    return report;
}

TEST_CASE("Finale 2009 marking categories and byte names recover in both byte orders", "[class]")
{
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        const auto nameWords = byteOrder == ByteOrder::BigEndian ? std::vector<std::int16_t>{static_cast<std::int16_t>(0x8041), 0, 0, 0, 0, 0}
                                                                 : std::vector<std::int16_t>{0x4180, 0, 0, 0, 0, 0};
        const auto parsed = makeClassContainer(
            {SyntheticClassRow{0x012d, {5, 9, 12, 2, 10, 24, 0, 11, 14, 1, 2, 3, -12, 9, 36, -16, static_cast<std::int16_t>(0x04df), 17}, 8},
                SyntheticClassRow{0x012e, nameWords, 8}},
            byteOrder);
        auto profile = profileFor(finale_mus_reader::versions::finale2009.major);
        profile.epoch = FormatEpoch::ZlibLegacy;
        profile.byteOrder = byteOrder;
        profile.platform = finale_mus_reader::SourcePlatform::Windows;
        const auto document = emptyMarkingCategoryDocument();
        const auto report = markingCategoryImport(parsed, profile, document);

        const auto category = document->getOthers()->get<Category>(musx::dom::SCORE_PARTID, 8);
        REQUIRE(category);
        CHECK(category->categoryType == Category::CategoryType::TechniqueText);
        CHECK(category->textFont->fontId == 9);
        CHECK(category->textFont->fontSize == 12);
        CHECK(category->textFont->italic);
        CHECK(category->musicFont->fontId == 10);
        CHECK(category->numberFont->fontId == 11);
        CHECK(category->justification == musx::dom::AlignJustify::Right);
        CHECK(category->horzAlign == musx::dom::others::HorizontalMeasExprAlign::Manual);
        CHECK(category->horzOffset == -12);
        CHECK(category->vertAlign == musx::dom::others::VerticalMeasExprAlign::BelowStaffOrEntry);
        CHECK(category->vertOffsetEntry == 36);
        CHECK(category->vertOffsetBaseline == -16);
        CHECK(category->usesTextFont);
        CHECK(category->usesMusicFont);
        CHECK(category->usesNumberFont);
        CHECK(category->usesPositioning);
        CHECK(category->usesStaffList);
        CHECK(category->usesBreakMmRests);
        CHECK(category->breakMmRest);
        CHECK(category->userCreated);
        CHECK(category->staffList == 17);
        REQUIRE(document->getOthers()->get<CategoryName>(musx::dom::SCORE_PARTID, 8));
        CHECK(category->getName() == "€A");
        CHECK(reportedFieldCount(report) == 41);
    }
}

TEST_CASE("Finale 2012 marking category names are UTF-16 code units", "[class]")
{
    const auto parsed = makeClassContainer(
        {SyntheticClassRow{0x012d, std::vector<std::int16_t>(18), 8}, SyntheticClassRow{0x012e, {static_cast<std::int16_t>(0x03a9), 0x0041, 0}, 8}},
        ByteOrder::LittleEndian);
    auto profile = profileFor(finale_mus_reader::versions::finale2012.major);
    profile.epoch = FormatEpoch::ZlibLegacy;
    profile.byteOrder = ByteOrder::LittleEndian;
    const auto document = emptyMarkingCategoryDocument();
    markingCategoryImport(parsed, profile, document);
    const auto name = document->getOthers()->get<CategoryName>(musx::dom::SCORE_PARTID, 8);
    REQUIRE(name);
    CHECK(name->name == "ΩA");
}

TEST_CASE("A dangling marking-category font receives musxdom's placeholder", "[class]")
{
    std::vector<std::int16_t> words(18);
    words[0] = 1;
    words[1] = 10;
    auto profile = profileFor(finale_mus_reader::versions::finale2011.major);
    profile.epoch = FormatEpoch::ZlibLegacy;
    profile.byteOrder = ByteOrder::LittleEndian;

    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    const auto index = LegacyRecordIndex::build(makeClassContainer({SyntheticClassRow{0x012d, std::move(words), 1}}, ByteOrder::LittleEndian));
    ImportReport report(profile.epoch);
    finale_mus_reader::PendingReferences pending;
    const auto reference = markingCategoryBaseline();
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, session.getConstructionContext()};
    finale_mus_reader::others::importMarkingCategories(context);

    const auto finished = std::move(session).finish();
    const auto font = finished->getOthers()->get<musx::dom::others::FontDefinition>(musx::dom::SCORE_PARTID, 10);
    REQUIRE(font);
    CHECK(font->name == "Missing Font (10)");
}

TEST_CASE("Marking category alignment constants translate to musxdom enums", "[class]")
{
    using H = musx::dom::others::HorizontalMeasExprAlign;
    using V = musx::dom::others::VerticalMeasExprAlign;
    const std::array horizontal{std::pair{0, H::LeftBarline}, std::pair{1, H::StartTimeSig}, std::pair{2, H::AfterClefKeyTime},
        std::pair{3, H::Manual}, std::pair{4, H::CenterOverBarlines}, std::pair{5, H::CenterOverMusic}, std::pair{6, H::RightBarline},
        std::pair{7, H::StartOfMusic}, std::pair{9, H::LeftOfAllNoteheads}, std::pair{10, H::Stem}, std::pair{11, H::CenterPrimaryNotehead},
        std::pair{12, H::CenterAllNoteheads}, std::pair{13, H::LeftOfPrimaryNotehead}, std::pair{14, H::RightOfAllNoteheads}};
    const std::array vertical{std::pair{0, V::AboveStaff}, std::pair{1, V::BelowStaff}, std::pair{2, V::Manual}, std::pair{3, V::RefLine},
        std::pair{4, V::TopNote}, std::pair{5, V::BottomNote}, std::pair{6, V::AboveEntry}, std::pair{7, V::BelowEntry},
        std::pair{8, V::AboveStaffOrEntry}, std::pair{9, V::BelowStaffOrEntry}};
    std::vector<SyntheticClassRow> rows;
    for (std::size_t i = 0; i < horizontal.size(); ++i) {
        std::vector<std::int16_t> words(18);
        words[10] = static_cast<std::int16_t>(i % 3);
        words[11] = static_cast<std::int16_t>(horizontal[i].first);
        words[13] = static_cast<std::int16_t>(vertical[i % vertical.size()].first);
        rows.push_back({0x012d, std::move(words), static_cast<musx::dom::Cmper>(i + 1)});
    }
    auto profile = profileFor(finale_mus_reader::versions::finale2009.major);
    profile.epoch = FormatEpoch::ZlibLegacy;
    profile.byteOrder = ByteOrder::LittleEndian;
    const auto document = emptyMarkingCategoryDocument();
    markingCategoryImport(makeClassContainer(rows, ByteOrder::LittleEndian), profile, document);

    for (std::size_t i = 0; i < horizontal.size(); ++i) {
        const auto category = document->getOthers()->get<Category>(musx::dom::SCORE_PARTID, static_cast<musx::dom::Cmper>(i + 1));
        REQUIRE(category);
        CHECK(category->horzAlign == horizontal[i].second);
        CHECK(category->vertAlign == vertical[i % vertical.size()].second);
        const auto expectedJustification = std::array{musx::dom::AlignJustify::Left, musx::dom::AlignJustify::Center, musx::dom::AlignJustify::Right};
        CHECK(category->justification == expectedJustification[i % 3]);
    }
}

TEST_CASE("Every pre-Finale 2009 epoch receives the seven canned categories", "[class]")
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        const auto document = emptyMarkingCategoryDocument();
        const auto report = markingCategoryImport(makeContainer({}, epoch), SourceProfile(epoch), document);
        const auto categories = document->getOthers()->getArray<Category>(musx::dom::SCORE_PARTID);
        const auto names = document->getOthers()->getArray<CategoryName>(musx::dom::SCORE_PARTID);
        REQUIRE(categories.size() == 7);
        REQUIRE(names.size() == 7);
        for (musx::dom::Cmper cmper = 1; cmper <= 7; ++cmper) {
            const auto* origin = report.findInstanceOrigin(finale_mus_reader::instanceKey<Category>(musx::dom::SCORE_PARTID, cmper));
            REQUIRE(origin);
            CHECK(*origin == ValueOrigin::Finale27Default);
        }
    }

    auto profile = profileFor(finale_mus_reader::versions::finale2008.major);
    profile.epoch = FormatEpoch::ZlibLegacy;
    const auto document = emptyMarkingCategoryDocument();
    markingCategoryImport(makeContainer({}, FormatEpoch::ZlibLegacy), profile, document);
    CHECK(document->getOthers()->getArray<Category>(musx::dom::SCORE_PARTID).size() == 7);
}

TEST_CASE("Finale 2009 and later do not synthesize missing marking categories", "[class]")
{
    for (const auto major : {finale_mus_reader::versions::finale2009.major, finale_mus_reader::versions::finale2012.major}) {
        auto profile = profileFor(major);
        profile.epoch = FormatEpoch::ZlibLegacy;
        const auto document = emptyMarkingCategoryDocument();
        markingCategoryImport(makeContainer({}, FormatEpoch::ZlibLegacy), profile, document);
        CHECK(document->getOthers()->getArray<Category>(musx::dom::SCORE_PARTID).empty());
        CHECK(document->getOthers()->getArray<CategoryName>(musx::dom::SCORE_PARTID).empty());
    }
}

TEST_CASE("A short marking category is rejected before either component is added", "[class]")
{
    auto profile = profileFor(finale_mus_reader::versions::finale2009.major);
    profile.epoch = FormatEpoch::ZlibLegacy;
    profile.byteOrder = ByteOrder::LittleEndian;
    const auto document = emptyMarkingCategoryDocument();
    const auto report = markingCategoryImport(
        makeClassContainer({SyntheticClassRow{0x012d, {1, 2}, 8}, SyntheticClassRow{0x012e, {0x4142, 0}, 8}}, ByteOrder::LittleEndian), profile,
        document);
    CHECK_FALSE(document->getOthers()->get<Category>(musx::dom::SCORE_PARTID, 8));
    CHECK_FALSE(document->getOthers()->get<CategoryName>(musx::dom::SCORE_PARTID, 8));
    REQUIRE(report.diagnostics.size() == 1);
}

} // namespace
} // namespace finale_mus_reader_tests
