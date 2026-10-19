// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

using NamePositionAbbreviated = musx::dom::others::NamePositionAbbreviated;
using NamePositionFull = musx::dom::others::NamePositionFull;
using NamePositionStyleAbbreviated = musx::dom::others::NamePositionStyleAbbreviated;
using NamePositionStyleFull = musx::dom::others::NamePositionStyleFull;

ImportReport namePositionImport(
    const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile, const musx::dom::DocumentPtr& document)
{
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importNamePositionAbbreviated(context);
    finale_mus_reader::others::importNamePositionFull(context);
    finale_mus_reader::others::importNamePositionStyleAbbreviated(context);
    finale_mus_reader::others::importNamePositionStyleFull(context);
    return report;
}

musx::dom::DocumentPtr emptyNamePositionDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    return session.getDocument();
}

template <typename Target>
void expectNamePosition(
    const musx::dom::DocumentPtr& document, const ImportReport& report, musx::dom::Cmper cmper, musx::dom::Evpu horz, bool preFinale37Layout)
{
    const auto target = document->getOthers()->get<Target>(musx::dom::SCORE_PARTID, cmper);
    REQUIRE(target);
    CHECK(target->horzOff == horz);
    CHECK(target->vertOff == -24);
    CHECK(target->justify == musx::dom::AlignJustify::Right);
    CHECK(target->indivPos == !preFinale37Layout);
    CHECK(target->hAlign == (preFinale37Layout ? musx::dom::AlignJustify::Right : musx::dom::AlignJustify::Center));
    CHECK(target->expand);
    CHECK(target->hidden == !preFinale37Layout);
    for (const auto* member : {"horzOff", "vertOff", "justify", "hAlign"}) {
        const auto* field = report.findField<Target>(member, musx::dom::SCORE_PARTID, cmper);
        REQUIRE(field);
        CHECK(field->origin == ValueOrigin::LegacyMus);
    }
    const auto* expand = report.findField<Target>("expand", musx::dom::SCORE_PARTID, cmper);
    REQUIRE(expand);
    CHECK(expand->origin == (preFinale37Layout ? ValueOrigin::LegacyBehavior : ValueOrigin::LegacyMus));
    const auto* individual = report.findField<Target>("indivPos", musx::dom::SCORE_PARTID, cmper);
    REQUIRE(individual);
    CHECK(individual->origin == ValueOrigin::LegacyMus);
    const auto* hidden = report.findField<Target>("hidden", musx::dom::SCORE_PARTID, cmper);
    REQUIRE(hidden);
    CHECK(hidden->origin == (preFinale37Layout ? ValueOrigin::LegacyBehavior : ValueOrigin::LegacyMus));
}

TEST_CASE("Name positioning classes recover their common layout in every epoch")
{
    constexpr std::int16_t flags = static_cast<std::int16_t>(0x8069);
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
            const std::int16_t expandWord = epoch == FormatEpoch::CodaBanner ? 1 : 0;
            const std::int16_t flagsWord = epoch == FormatEpoch::CodaBanner ? 1 : flags;
            const auto parsed = epoch == FormatEpoch::ZlibLegacy ? makeClassContainer(
                                                                       {
                                                                           SyntheticClassRow{0x00b3, {-101, -24, 0, 0, 0, flags}, 11},
                                                                           SyntheticClassRow{0x00b4, {-102, -24, 0, 0, 0, flags}, 12},
                                                                           SyntheticClassRow{0x00b5, {-103, -24, 0, 0, 0, flags}, 13},
                                                                           SyntheticClassRow{0x00b6, {-104, -24, 0, 0, 0, flags}, 14},
                                                                       },
                                                                       byteOrder)
                                                                 : makeContainer(
                                                                       {
                                                                           {11, "ns", {-101, -24, 0, 0, expandWord, flagsWord}},
                                                                           {12, "NY", {-102, -24, 0, 0, expandWord, flagsWord}},
                                                                           {13, "NS", {-103, -24, 0, 0, expandWord, flagsWord}},
                                                                           {14, "Ny", {-104, -24, 0, 0, expandWord, flagsWord}},
                                                                       },
                                                                       epoch, byteOrder);
            const auto document = emptyNamePositionDocument();
            auto profile = SourceProfile(epoch);
            profile.byteOrder = byteOrder;
            const auto report = namePositionImport(parsed, profile, document);
            const auto preFinale37Layout = epoch == FormatEpoch::CodaBanner;
            expectNamePosition<NamePositionAbbreviated>(document, report, 11, -101, preFinale37Layout);
            expectNamePosition<NamePositionStyleAbbreviated>(document, report, 12, -102, preFinale37Layout);
            expectNamePosition<NamePositionFull>(document, report, 13, -103, preFinale37Layout);
            expectNamePosition<NamePositionStyleFull>(document, report, 14, -104, preFinale37Layout);
            CHECK(reportedFieldCount(report) == 4 * NamePositionFull::xmlMappingArray().size());
        }
    }
}

TEST_CASE("Name positioning uses the early layout before Finale 3.7")
{
    struct TestCase
    {
        FormatEpoch epoch;
        SourceVersion version;
        std::int16_t flags;
        bool earlyLayout;
        bool individual;
    };
    const TestCase cases[]{
        {FormatEpoch::CodaBanner, SourceVersion{.major = 2, .minor = 6}, 0x0001, true, false},
        {FormatEpoch::UncompressedLegacy, SourceVersion{.major = 3, .minor = 5}, 0x0005, true, true},
        {FormatEpoch::UncompressedLegacy, SourceVersion{.major = 3, .minor = 7}, 0x0069, false, true},
    };

    for (const auto& testCase : cases) {
        const auto parsed = makeContainer({{1, "NS", {-48, -64, 0, 0, 0, testCase.flags}}}, testCase.epoch, ByteOrder::BigEndian);
        const auto document = emptyNamePositionDocument();
        auto profile = SourceProfile(testCase.epoch);
        profile.byteOrder = ByteOrder::BigEndian;
        profile.version = testCase.version;
        const auto report = namePositionImport(parsed, profile, document);
        const auto target = document->getOthers()->get<NamePositionFull>(musx::dom::SCORE_PARTID, 1);
        REQUIRE(target);
        CHECK(target->justify == musx::dom::AlignJustify::Right);
        CHECK(target->indivPos == testCase.individual);
        CHECK(target->hAlign == (testCase.earlyLayout ? musx::dom::AlignJustify::Right : musx::dom::AlignJustify::Center));
        CHECK(target->expand == testCase.earlyLayout);
        CHECK(target->hidden == !testCase.earlyLayout);
        const auto* field = report.findField<NamePositionFull>("expand", musx::dom::SCORE_PARTID, musx::dom::Cmper{1});
        REQUIRE(field);
        CHECK(field->origin == (testCase.earlyLayout ? ValueOrigin::LegacyBehavior : ValueOrigin::LegacyMus));
    }
}

TEST_CASE("A truncated name positioning class record creates no partial object")
{
    const auto parsed = makeClassContainer({SyntheticClassRow{0x00b5, {-72, -24, 0, 0, 0}, 1}}, ByteOrder::BigEndian);
    const auto document = emptyNamePositionDocument();
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    const auto report = namePositionImport(parsed, profile, document);
    CHECK(document->getOthers()->getAllSources<NamePositionFull>().empty());
    CHECK(report.diagnostics.size() == 1);
}

TEST_CASE("A name-position part continuation overlays its byte and flags masks")
{
    constexpr std::int16_t scoreFlags = static_cast<std::int16_t>(0x802a);
    constexpr std::int16_t partFlags = static_cast<std::int16_t>(0x8022);
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        const auto parsed = makeClassContainer(
            {
                SyntheticClassRow{0x00b6, {123, -45, 0, 0, 0, scoreFlags}, 23},
                SyntheticClassRow{0x00b6, {345, -45, 0, 0, 0, partFlags}, 23, 1, true, {0xffff, 0xffff}, 0, 0x0008},
            },
            byteOrder);
        const auto document = emptyNamePositionDocument();
        auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
        profile.byteOrder = byteOrder;
        namePositionImport(parsed, profile, document);

        const auto part = document->getOthers()->get<NamePositionStyleFull>(1, 23);
        REQUIRE(part);
        CHECK(part->getShareMode() == musx::dom::EnigmaBase::ShareMode::Partial);
        CHECK(part->horzOff == 345);
        CHECK(part->vertOff == -45);
        CHECK(part->justify == musx::dom::AlignJustify::Center);
        CHECK_FALSE(part->indivPos);
        CHECK(part->hAlign == musx::dom::AlignJustify::Center);
        CHECK(part->expand);
    }
}

TEST_CASE("Name positioning classes recover controlled fixed-row and zlib fixtures")
{
    const auto f263Right = readFixture("evidence/F263/F263-namepos.mus");
    const auto f263RightFull = f263Right.document->getOthers()->get<NamePositionFull>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(f263RightFull);
    CHECK(f263RightFull->horzOff == 123);
    CHECK(f263RightFull->vertOff == 45);
    CHECK(f263RightFull->justify == musx::dom::AlignJustify::Right);
    CHECK_FALSE(f263RightFull->indivPos);
    CHECK(f263RightFull->hAlign == musx::dom::AlignJustify::Right);
    CHECK(f263RightFull->expand);
    CHECK_FALSE(f263RightFull->hidden);

    const auto f263Left = readFixture("evidence/F263/F263-namepos-left.mus");
    const auto f263LeftFull = f263Left.document->getOthers()->get<NamePositionFull>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(f263LeftFull);
    CHECK(f263LeftFull->horzOff == 123);
    CHECK(f263LeftFull->vertOff == 45);
    CHECK(f263LeftFull->justify == musx::dom::AlignJustify::Left);
    CHECK_FALSE(f263LeftFull->indivPos);
    CHECK(f263LeftFull->hAlign == musx::dom::AlignJustify::Left);
    CHECK(f263LeftFull->expand);
    CHECK_FALSE(f263LeftFull->hidden);

    const auto f2000 = readFixture("evidence/F2000/F2000-staff-style.mus");
    const auto fullStyle = f2000.document->getOthers()->get<NamePositionStyleFull>(musx::dom::SCORE_PARTID, 1);
    const auto abbreviatedStyle = f2000.document->getOthers()->get<NamePositionStyleAbbreviated>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(fullStyle);
    REQUIRE(abbreviatedStyle);
    CHECK(fullStyle->horzOff == -72);
    CHECK(fullStyle->vertOff == -22);
    CHECK(abbreviatedStyle->horzOff == -72);
    CHECK(abbreviatedStyle->vertOff == -22);

    const auto f2005 = readFixture("evidence/F2005/F2005-staffname-lineperc88.mus");
    const auto full = f2005.document->getOthers()->get<NamePositionFull>(musx::dom::SCORE_PARTID, 1);
    const auto abbreviated = f2005.document->getOthers()->get<NamePositionAbbreviated>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(full);
    REQUIRE(abbreviated);
    CHECK(full->horzOff == -72);
    CHECK(full->vertOff == -22);
    CHECK(abbreviated->horzOff == -72);
    CHECK(abbreviated->vertOff == -22);

    const auto f372 = readFixture("evidence/F372/F372-namepos-indiv.mus");
    const auto individualFull = f372.document->getOthers()->get<NamePositionFull>(musx::dom::SCORE_PARTID, 1);
    const auto individualAbbreviated = f372.document->getOthers()->get<NamePositionAbbreviated>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(individualFull);
    REQUIRE(individualAbbreviated);
    CHECK(individualFull->indivPos);
    CHECK_FALSE(individualAbbreviated->indivPos);

    const auto f2008 = readFixture("evidence/F2008/F2008-namepos-hidden.mus");
    const auto hiddenFull = f2008.document->getOthers()->get<NamePositionFull>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(hiddenFull);
    CHECK(hiddenFull->hidden);
    CHECK(hiddenFull->hAlign == musx::dom::AlignJustify::Right);

    const auto f2011 = readFixture("evidence/F2011/F2011-staffstyle.mus");
    const auto zlibFullStyle = f2011.document->getOthers()->get<NamePositionStyleFull>(musx::dom::SCORE_PARTID, 1);
    const auto zlibAbbreviatedStyle = f2011.document->getOthers()->get<NamePositionStyleAbbreviated>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(zlibFullStyle);
    REQUIRE(zlibAbbreviatedStyle);
    CHECK(zlibFullStyle->horzOff == -72);
    CHECK(zlibFullStyle->vertOff == -22);
    CHECK(zlibAbbreviatedStyle->horzOff == -72);
    CHECK(zlibAbbreviatedStyle->vertOff == -22);
}

} // namespace
} // namespace finale_mus_reader_tests
