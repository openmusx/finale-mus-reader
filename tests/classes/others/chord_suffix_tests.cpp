// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

using ChordSuffixElement = musx::dom::others::ChordSuffixElement;
using ChordSuffixPlayback = musx::dom::others::ChordSuffixPlayback;

ImportReport importChordSuffixes(
    const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile, const musx::dom::DocumentPtr& document)
{
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importChordSuffixElements(context);
    finale_mus_reader::others::importChordSuffixPlayback(context);
    return report;
}

musx::dom::DocumentPtr emptyChordSuffixDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    return session.getDocument();
}

TEST_CASE("Pre-2012 chord suffix records recover in every fixed-row epoch", "[class]")
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            const auto parsed = makeContainer(
                {
                    {11, "IV", {0x00ab, -12, 34, 0x1207, 0x00e7, 0x0840}},
                    {11, "IV", {0x0029, 8, -9, 0x0a03, 0x0020, 0x0010}},
                    {11, "IK", {4, 0, 7, -5, 0, 0}},
                },
                epoch, byteOrder);
            auto profile = SourceProfile(epoch);
            profile.byteOrder = byteOrder;
            const auto document = emptyChordSuffixDocument();
            const auto report = importChordSuffixes(parsed, profile, document);

            const auto first = document->getOthers()->get<ChordSuffixElement>(musx::dom::SCORE_PARTID, 11, musx::dom::Inci(0));
            const auto second = document->getOthers()->get<ChordSuffixElement>(musx::dom::SCORE_PARTID, 11, musx::dom::Inci(1));
            const auto playback = document->getOthers()->get<ChordSuffixPlayback>(musx::dom::SCORE_PARTID, 11);
            REQUIRE(first);
            CHECK(first->symbol == char32_t{0x00ab});
            CHECK(first->xdisp == -12);
            CHECK(first->ydisp == 34);
            CHECK(first->font->fontId == 7);
            CHECK(first->font->fontSize == 18);
            CHECK(first->font->bold);
            CHECK(first->font->italic);
            CHECK(first->font->underline);
            CHECK(first->font->strikeout);
            CHECK(first->font->absolute);
            CHECK(first->font->hidden);
            CHECK(first->isNumber);
            CHECK(first->prefix == ChordSuffixElement::Prefix::Sharp);
            REQUIRE(second);
            CHECK(second->prefix == ChordSuffixElement::Prefix::Minus);
            REQUIRE(playback);
            CHECK(playback->values == std::vector<std::int16_t>{4, 0, 7, -5, 0, 0});
            CHECK(reportedFieldCount(report) == 32);
            CHECK(field(report, "others.chordSuffix[11].font.fontId").origin == ValueOrigin::LegacyMus);
            CHECK(field(report, "others.chordSuffix[11].prefix").origin == ValueOrigin::LegacyMus);
        }
    }
}

TEST_CASE("Zlib chord suffix records select narrow and Finale 2012 layouts", "[class]")
{
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        auto narrowProfile = profileFor(16);
        narrowProfile.epoch = FormatEpoch::ZlibLegacy;
        narrowProfile.byteOrder = byteOrder;
        const auto narrowDocument = emptyChordSuffixDocument();
        const auto narrowReport = importChordSuffixes(makeClassContainer(
                                                          {
                                                              SyntheticClassRow{0x007d, {0x002b, -3, 5, 0x0f09, 0x0003, 0x0820}, 17},
                                                              SyntheticClassRow{0x007e, {3, 8, 10, -2, 0, 0}, 17},
                                                          },
                                                          byteOrder),
            narrowProfile, narrowDocument);
        const auto narrow = narrowDocument->getOthers()->get<ChordSuffixElement>(musx::dom::SCORE_PARTID, 17, musx::dom::Inci(0));
        const auto narrowPlayback = narrowDocument->getOthers()->get<ChordSuffixPlayback>(musx::dom::SCORE_PARTID, 17);
        REQUIRE(narrow);
        CHECK(narrow->symbol == U'+');
        CHECK(narrow->font->fontId == 9);
        CHECK(narrow->font->fontSize == 15);
        CHECK(narrow->prefix == ChordSuffixElement::Prefix::Plus);
        REQUIRE(narrowPlayback);
        CHECK(narrowPlayback->values == std::vector<std::int16_t>{3, 8, 10, -2, 0, 0});
        CHECK(reportedFieldCount(narrowReport) == 19);

        auto wideProfile = profileFor(17);
        wideProfile.epoch = FormatEpoch::ZlibLegacy;
        wideProfile.byteOrder = byteOrder;
        const auto wideDocument = emptyChordSuffixDocument();
        const auto wideReport = importChordSuffixes(
            makeClassContainer(
                {SyntheticClassRow{0x007d, {0x0001, static_cast<std::int16_t>(0xf642), -14, 21, 300, 18, 0x00a1, 0x0880, 0, 0, 0, 0}, 23}},
                byteOrder),
            wideProfile, wideDocument);
        const auto wide = wideDocument->getOthers()->get<ChordSuffixElement>(musx::dom::SCORE_PARTID, 23, musx::dom::Inci(0));
        REQUIRE(wide);
        CHECK(wide->symbol == U'\U0001f642');
        CHECK(wide->xdisp == -14);
        CHECK(wide->ydisp == 21);
        CHECK(wide->font->fontId == 300);
        CHECK(wide->font->fontSize == 18);
        CHECK(wide->font->bold);
        CHECK(wide->font->strikeout);
        CHECK(wide->font->hidden);
        CHECK(wide->isNumber);
        CHECK(wide->prefix == ChordSuffixElement::Prefix::Flat);
        CHECK(reportedFieldCount(wideReport) == 13);
    }
}

TEST_CASE("Controlled Finale 1.0 chord suffixes retain playback zeroes", "[class][reader]")
{
    const auto result = readFixture("evidence/F100/F100-chordsuffs.mus");
    const auto first = result.document->getOthers()->get<ChordSuffixElement>(musx::dom::SCORE_PARTID, 1, musx::dom::Inci(0));
    const auto second = result.document->getOthers()->get<ChordSuffixElement>(musx::dom::SCORE_PARTID, 1, musx::dom::Inci(1));
    const auto numeric = result.document->getOthers()->get<ChordSuffixElement>(musx::dom::SCORE_PARTID, 2, musx::dom::Inci(0));
    const auto playback = result.document->getOthers()->get<ChordSuffixPlayback>(musx::dom::SCORE_PARTID, 1);

    REQUIRE(first);
    CHECK(first->symbol == U'm');
    CHECK(first->font->fontId == 2);
    CHECK(first->font->fontSize == 12);
    REQUIRE(second);
    CHECK(second->symbol == U'7');
    CHECK(second->xdisp == 40);
    REQUIRE(numeric);
    CHECK(numeric->isNumber);
    CHECK(numeric->prefix == ChordSuffixElement::Prefix::Flat);
    REQUIRE(playback);
    CHECK(playback->values == std::vector<std::int16_t>{3, 7, 10, 0, 0, 0});
}

TEST_CASE("Incomplete and absent chord suffix records do not create partial objects", "[class]")
{
    auto profile = profileFor(17);
    profile.epoch = FormatEpoch::ZlibLegacy;
    profile.byteOrder = ByteOrder::LittleEndian;
    const auto document = emptyChordSuffixDocument();
    const auto report = importChordSuffixes(makeClassContainer(0x007d, {0, 65, 1, 2, 3, 12, 0, 0}, ByteOrder::LittleEndian, 41), profile, document);
    CHECK(document->getOthers()->getAllSources<ChordSuffixElement>().empty());
    CHECK(document->getOthers()->getAllSources<ChordSuffixPlayback>().empty());
    REQUIRE(report.diagnostics.size() == 1);
}

} // namespace
} // namespace finale_mus_reader_tests
