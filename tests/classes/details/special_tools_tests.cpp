// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <tuple>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

template <typename Target>
ImportReport importBeamDetail(const finale_mus_reader::container::ParsedContainer& parsed, FormatEpoch epoch, const musx::dom::DocumentPtr& document,
    std::optional<SourceVersion> version = std::nullopt)
{
    ImportReport report(epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    SourceProfile profile(epoch);
    profile.byteOrder = parsed.byteOrder;
    profile.version = version;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    if constexpr (std::is_same_v<Target, musx::dom::details::BeamAlterationsDownStem>) {
        finale_mus_reader::details::importBeamAlterationsDownStem(context);
    } else if constexpr (std::is_same_v<Target, musx::dom::details::BeamAlterationsUpStem>) {
        finale_mus_reader::details::importBeamAlterationsUpStem(context);
    } else if constexpr (std::is_same_v<Target, musx::dom::details::SecondaryBeamAlterationsDownStem>) {
        finale_mus_reader::details::importSecondaryBeamAlterationsDownStem(context);
    } else if constexpr (std::is_same_v<Target, musx::dom::details::BeamExtensionDownStem>) {
        finale_mus_reader::details::importBeamExtensionDownStem(context);
    } else if constexpr (std::is_same_v<Target, musx::dom::details::BeamExtensionUpStem>) {
        finale_mus_reader::details::importBeamExtensionUpStem(context);
    } else if constexpr (std::is_same_v<Target, musx::dom::details::BeamStubDirection>) {
        finale_mus_reader::details::importBeamStubDirection(context);
    } else if constexpr (std::is_same_v<Target, musx::dom::details::SecondaryBeamBreak>) {
        finale_mus_reader::details::importSecondaryBeamBreak(context);
    } else if constexpr (std::is_same_v<Target, musx::dom::details::CustomDownStem>) {
        finale_mus_reader::details::importCustomDownStem(context);
    } else if constexpr (std::is_same_v<Target, musx::dom::details::CustomUpStem>) {
        finale_mus_reader::details::importCustomUpStem(context);
    } else {
        finale_mus_reader::details::importSecondaryBeamAlterationsUpStem(context);
    }
    return report;
}

TEST_CASE("Custom stems decode shape and signed offsets from one tagged incidence", "[class][special-tools]")
{
    using Down = musx::dom::details::CustomDownStem;
    using Up = musx::dom::details::CustomUpStem;
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            auto session = musx::factory::DocumentFactory::begin();
            const auto document = session.getDocument();
            const auto downReport =
                importBeamDetail<Down>(makeDetailContainer(epoch, 0, 42, {27, 99, -12, 35, 88, 0, 0, 0, 0, 0}, "sl", order), epoch, document);
            const auto upReport = importBeamDetail<Up>(makeDetailContainer(epoch, 0, 43, {0, 98, 23, -44, 87}, "SU", order), epoch, document);
            const auto down = document->getDetails()->get<Down>(musx::dom::SCORE_PARTID, 42);
            const auto up = document->getDetails()->get<Up>(musx::dom::SCORE_PARTID, 43);
            REQUIRE(down);
            REQUIRE(up);
            CHECK(down->shapeDef == 27);
            CHECK(down->xOffset == -12);
            CHECK(down->yOffset == 35);
            CHECK(up->shapeDef == 0);
            CHECK(up->xOffset == 23);
            CHECK(up->yOffset == -44);
            for (const auto* name : {"shapeDef", "xOffset", "yOffset"}) {
                const auto* downField =
                    downReport.findField<Down>(name, musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(42));
                const auto* upField = upReport.findField<Up>(name, musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(43));
                REQUIRE(downField);
                REQUIRE(upField);
                CHECK(downField->origin == ValueOrigin::LegacyMus);
                CHECK(upField->origin == ValueOrigin::LegacyMus);
            }
        }
    }
}

TEST_CASE("Custom stems decode both zlib class identities", "[class][special-tools]")
{
    using Down = musx::dom::details::CustomDownStem;
    using Up = musx::dom::details::CustomUpStem;
    for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        const auto downParsed = makeDetailClassContainer(0, 42, musx::dom::SCORE_PARTID, {27, 9, -12, 35, 8}, order, 0x042b);
        const auto upParsed = makeDetailClassContainer(0, 43, musx::dom::SCORE_PARTID, {0, 8, 23, -44, 9}, order, 0x042c);
        importBeamDetail<Down>(downParsed, FormatEpoch::ZlibLegacy, document);
        importBeamDetail<Up>(upParsed, FormatEpoch::ZlibLegacy, document);
        const auto down = document->getDetails()->get<Down>(musx::dom::SCORE_PARTID, 42);
        const auto up = document->getDetails()->get<Up>(musx::dom::SCORE_PARTID, 43);
        REQUIRE(down);
        REQUIRE(up);
        CHECK(down->shapeDef == 27);
        CHECK(down->xOffset == -12);
        CHECK(down->yOffset == 35);
        CHECK(up->shapeDef == 0);
        CHECK(up->xOffset == 23);
        CHECK(up->yOffset == -44);
    }
}

TEST_CASE("Finale 1 custom upstem shape is recovered from a Coda detail", "[class][special-tools]")
{
    using Up = musx::dom::details::CustomUpStem;
    using Down = musx::dom::details::CustomDownStem;
    const auto baseline = readFixture("evidence/F100/F100-beam.mus");
    CHECK(baseline.document->getDetails()->getAllSources<Up>().empty());
    const auto edited = readFixture("evidence/F100/F100-beam-custstem.mus");
    const auto stem = edited.document->getDetails()->get<Up>(musx::dom::SCORE_PARTID, 2);
    REQUIRE(stem);
    CHECK(stem->shapeDef == 1);
    CHECK(stem->xOffset == 0);
    CHECK(stem->yOffset == 0);
    CHECK(edited.document->getDetails()->getAllSources<Down>().empty());
    for (const auto* name : {"shapeDef", "xOffset", "yOffset"}) {
        const auto* field = edited.report.findField<Up>(name, musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(2));
        REQUIRE(field);
        CHECK(field->origin == ValueOrigin::LegacyMus);
    }
}

TEST_CASE("Beam extensions decode both tagged stem directions", "[class][special-tools]")
{
    using Down = musx::dom::details::BeamExtensionDownStem;
    using Up = musx::dom::details::BeamExtensionUpStem;
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            auto session = musx::factory::DocumentFactory::begin();
            const auto document = session.getDocument();
            const auto downReport = importBeamDetail<Down>(makeDetailContainer(epoch, 0, 42, {-60, 664, 0, 0, 2688}, "DE", order), epoch, document);
            const auto upReport = importBeamDetail<Up>(makeDetailContainer(epoch, 0, 43, {12, -36, 0, 0, 2560}, "UE", order), epoch, document);
            const auto down = document->getDetails()->get<Down>(musx::dom::SCORE_PARTID, 42);
            const auto up = document->getDetails()->get<Up>(musx::dom::SCORE_PARTID, 43);
            REQUIRE(down);
            REQUIRE(up);
            CHECK(down->leftOffset == -60);
            CHECK(down->rightOffset == 664);
            CHECK(down->mask == 640);
            CHECK(down->extBeyond8th);
            CHECK(up->leftOffset == 12);
            CHECK(up->rightOffset == -36);
            CHECK(up->mask == 512);
            CHECK(up->extBeyond8th);
            for (const auto* name : {"leftOffset", "rightOffset", "mask", "extBeyond8th"}) {
                const auto* field =
                    downReport.findField<Down>(name, musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(42));
                REQUIRE(field);
                CHECK(field->origin == ValueOrigin::LegacyMus);
                const auto* upField = upReport.findField<Up>(name, musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(43));
                REQUIRE(upField);
                CHECK(upField->origin == ValueOrigin::LegacyMus);
            }
        }
    }
}

TEST_CASE("Entry size reads the second word of one detail incidence", "[class][special-tools]")
{
    using Target = musx::dom::details::EntrySize;
    const auto check = [](const finale_mus_reader::container::ParsedContainer& parsed, FormatEpoch epoch) {
        const auto index = LegacyRecordIndex::build(parsed);
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        auto referenceSession = musx::factory::DocumentFactory::begin();
        const auto reference = std::move(referenceSession).finish();
        ImportReport report(epoch);
        SourceProfile profile(epoch);
        profile.byteOrder = parsed.byteOrder;
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::details::importEntrySize(context);
        const auto size = document->getDetails()->get<Target>(musx::dom::SCORE_PARTID, 42);
        REQUIRE(size);
        CHECK(size->percent == 67);
        const auto* field = report.findField<Target>("percent", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(42));
        REQUIRE(field);
        CHECK(field->origin == ValueOrigin::LegacyMus);
        CHECK(field->rawValue == 67);
    };
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            check(makeDetailContainer(epoch, 0, 42, {9, 67, 8, 7, 6, 0, 99, 0, 0, 0}, "AS", order), epoch);
        }
    }
    check(makeDetailClassContainer(0, 42, musx::dom::SCORE_PARTID, {9, 67, 8, 7, 6}, ByteOrder::LittleEndian, 0x03f0), FormatEpoch::ZlibLegacy);
}

TEST_CASE("Beam stub direction reads only the fifth word", "[class][special-tools]")
{
    using Target = musx::dom::details::BeamStubDirection;
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            const auto parsed = makeDetailContainer(epoch, 0, 42, {11, 22, 33, 44, 0x0180}, "ub", order);
            const auto index = LegacyRecordIndex::build(parsed);
            auto session = musx::factory::DocumentFactory::begin();
            const auto document = session.getDocument();
            auto referenceSession = musx::factory::DocumentFactory::begin();
            const auto reference = std::move(referenceSession).finish();
            ImportReport report(epoch);
            SourceProfile profile(epoch);
            profile.byteOrder = order;
            finale_mus_reader::PendingReferences pending;
            musx::factory::ConstructionContext construction;
            const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
            finale_mus_reader::details::importBeamStubDirection(context);
            const auto detail = document->getDetails()->get<Target>(musx::dom::SCORE_PARTID, 42);
            REQUIRE(detail);
            CHECK(detail->mask == 0x0180);
            const auto* field = report.findField<Target>("mask", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(42));
            REQUIRE(field);
            CHECK(field->origin == ValueOrigin::LegacyMus);
        }
    }
}

TEST_CASE("Beam stub direction ignores bits above the ten beam levels", "[class][special-tools]")
{
    using Target = musx::dom::details::BeamStubDirection;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    const auto report = importBeamDetail<Target>(
        makeDetailContainer(FormatEpoch::UncompressedLegacy, 0, 42, {0, 0, 0, 0, -1}, "ub"), FormatEpoch::UncompressedLegacy, document);
    const auto detail = document->getDetails()->get<Target>(musx::dom::SCORE_PARTID, 42);
    REQUIRE(detail);
    CHECK(detail->mask == 0x03ff);
    const auto* field = report.findField<Target>("mask", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(42));
    REQUIRE(field);
    CHECK(field->rawValue == 0xffff);
}

TEST_CASE("Secondary beam break reads bytes in source order", "[class][special-tools]")
{
    using Target = musx::dom::details::SecondaryBeamBreak;
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            const auto firstWord = static_cast<std::int16_t>(order == ByteOrder::BigEndian ? 0x0100 : 0x0001);
            const auto lastWord = static_cast<std::int16_t>(order == ByteOrder::BigEndian ? 0x0007 : 0x0700);
            const auto parsed = makeDetailContainer(epoch, 0, 42, {firstWord, firstWord, 0, 0, lastWord}, "sB", order);
            const auto index = LegacyRecordIndex::build(parsed);
            auto session = musx::factory::DocumentFactory::begin();
            const auto document = session.getDocument();
            auto referenceSession = musx::factory::DocumentFactory::begin();
            const auto reference = std::move(referenceSession).finish();
            ImportReport report(epoch);
            SourceProfile profile(epoch);
            profile.byteOrder = order;
            finale_mus_reader::PendingReferences pending;
            musx::factory::ConstructionContext construction;
            const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
            finale_mus_reader::details::importSecondaryBeamBreak(context);
            const auto detail = document->getDetails()->get<Target>(musx::dom::SCORE_PARTID, 42);
            REQUIRE(detail);
            CHECK(detail->mask == 0x0140);
            CHECK_FALSE(detail->breakThrough);
            for (const auto* name : {"mask", "breakThrough"}) {
                const auto* field = report.findField<Target>(name, musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(42));
                REQUIRE(field);
                CHECK(field->origin == (std::string_view(name) == "mask" ? ValueOrigin::LegacyMus : ValueOrigin::LegacyBehavior));
            }
        }
    }
}

TEST_CASE("Zlib beam stub and secondary break classes use the tagged payload layouts", "[class][special-tools]")
{
    using Stub = musx::dom::details::BeamStubDirection;
    using Break = musx::dom::details::SecondaryBeamBreak;
    for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        const auto stubReport =
            importBeamDetail<Stub>(makeDetailClassContainer(0, 42, 0, {0, 0, 0, 0, -1}, order, 0x0400), FormatEpoch::ZlibLegacy, document);
        const auto firstWord = static_cast<std::int16_t>(order == ByteOrder::BigEndian ? 0x0001 : 0x0100);
        const auto remainingWord = static_cast<std::int16_t>(0x0101);
        const auto breakReport = importBeamDetail<Break>(
            makeDetailClassContainer(0, 43, 0, {firstWord, remainingWord, remainingWord, remainingWord, remainingWord}, order, 0x0425),
            FormatEpoch::ZlibLegacy, document);
        const auto stub = document->getDetails()->get<Stub>(musx::dom::SCORE_PARTID, 42);
        const auto secondaryBreak = document->getDetails()->get<Break>(musx::dom::SCORE_PARTID, 43);
        REQUIRE(stub);
        REQUIRE(secondaryBreak);
        CHECK(stub->mask == 0x03ff);
        CHECK(secondaryBreak->mask == 0x00ff);
        CHECK(secondaryBreak->breakThrough);
        CHECK(stubReport.findField<Stub>("mask", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(42)));
        CHECK(breakReport.findField<Break>("mask", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(43)));
    }
}

TEST_CASE("F100 and F2003 secondary break edits preserve the byte mask and through synthesis", "[class][special-tools]")
{
    using Target = musx::dom::details::SecondaryBeamBreak;
    for (const auto& [fixture, entry, mask, through, flippedMask] : {std::tuple{"evidence/F100/F100-beam-32brkonly.mus", 6, 0x80u, false, 0x100u},
             {"evidence/F100/F100-beam-32brkthru.mus", 6, 0xffu, true, 0x17fu}, {"evidence/F100/F100-beam-16-64brkonly.mus", 1, 0x140u, false, 0xa0u},
             {"evidence/F2003/F2003-beam-16-64brkonly.mus", 1, 0x140u, false, 0xa0u},
             {"evidence/F2003/F2003-beam-16-64brkonly.mus", 5, 0x140u, false, 0xa0u}}) {
        const auto imported = readFixture(fixture);
        const auto detail = imported.document->getDetails()->get<Target>(musx::dom::SCORE_PARTID, entry);
        REQUIRE(detail);
        CHECK(detail->mask == mask);
        CHECK(detail->breakThrough == through);
        const auto* maskField = imported.report.findField<Target>(
            "mask", musx::dom::SCORE_PARTID, static_cast<musx::dom::Cmper>(entry >> 16U), std::nullopt, static_cast<musx::dom::Cmper>(entry));
        REQUIRE(maskField);
        CHECK(maskField->finaleUpgradeLossValue == flippedMask);
    }
}

TEST_CASE("Finale 1 beam extension edits retain their source mask", "[class][special-tools]")
{
    using Up = musx::dom::details::BeamExtensionUpStem;
    const auto baseline = readFixture("evidence/F100/F100-beam.mus");
    CHECK(baseline.document->getDetails()->getAllSources<Up>().empty());
    for (const auto& [path, left, mask, raw] :
        {std::tuple{"evidence/F100/F100-beam-ext8th.mus", 0, 512U, 2560}, std::tuple{"evidence/F100/F100-beam-extsel.mus", -60, 640U, 2688}}) {
        const auto edited = readFixture(path);
        const auto beam = edited.document->getDetails()->get<Up>(musx::dom::SCORE_PARTID, 1);
        REQUIRE(beam);
        CHECK(beam->leftOffset == left);
        CHECK(beam->rightOffset == 664);
        CHECK(beam->mask == mask);
        CHECK(beam->extBeyond8th);
        const auto* field = edited.report.findField<Up>("mask", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(1));
        REQUIRE(field);
        CHECK(field->rawValue == raw);
        CHECK(field->origin == ValueOrigin::LegacyMus);
    }
}

TEST_CASE("Beam extensions recover zlib class records", "[class][special-tools]")
{
    using Down = musx::dom::details::BeamExtensionDownStem;
    using Up = musx::dom::details::BeamExtensionUpStem;
    for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        const auto downReport =
            importBeamDetail<Down>(makeDetailClassContainer(0, 42, 0, {-60, 664, 0, 0, 2688}, order, 0x03fd), FormatEpoch::ZlibLegacy, document);
        const auto upReport =
            importBeamDetail<Up>(makeDetailClassContainer(0, 43, 0, {33, -68, 0, 0, 2944}, order, 0x03fe), FormatEpoch::ZlibLegacy, document);
        const auto down = document->getDetails()->get<Down>(musx::dom::SCORE_PARTID, 42);
        const auto up = document->getDetails()->get<Up>(musx::dom::SCORE_PARTID, 43);
        REQUIRE(down);
        REQUIRE(up);
        CHECK(down->mask == 640);
        CHECK(up->mask == 896);
        CHECK(up->extBeyond8th);
        const auto* downField = downReport.findField<Down>("mask", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(42));
        const auto* upField = upReport.findField<Up>("mask", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(43));
        REQUIRE(downField);
        REQUIRE(upField);
        CHECK(downField->rawValue == 2688);
        CHECK(upField->rawValue == 2944);
    }
}

TEST_CASE("Beam extensions use incidence zero of a doubled zlib payload", "[class][special-tools]")
{
    using Down = musx::dom::details::BeamExtensionDownStem;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    const auto report =
        importBeamDetail<Down>(makeDetailClassContainer(0, 42, 0, {18, -19, 0, 0, 3008, 0, 0, 0, 0, 2560}, ByteOrder::LittleEndian, 0x03fd),
            FormatEpoch::ZlibLegacy, document);
    const auto down = document->getDetails()->get<Down>(musx::dom::SCORE_PARTID, 42);
    REQUIRE(down);
    CHECK(down->leftOffset == 18);
    CHECK(down->rightOffset == -19);
    CHECK(down->mask == 960);
    CHECK(down->extBeyond8th);
    const auto* field = report.findField<Down>("mask", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(42));
    REQUIRE(field);
    CHECK(field->rawValue == 3008);
}

template <typename Target>
ImportReport importStemDetail(const finale_mus_reader::container::ParsedContainer& parsed, FormatEpoch epoch, const musx::dom::DocumentPtr& document)
{
    ImportReport report(epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    SourceProfile profile(epoch);
    profile.byteOrder = parsed.byteOrder;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    if constexpr (std::is_same_v<Target, musx::dom::details::StemAlterations>) {
        finale_mus_reader::details::importStemAlterations(context);
    } else {
        finale_mus_reader::details::importStemAlterationsUnderBeam(context);
    }
    return report;
}

TEST_CASE("Stem alterations recover signed packed offsets for both selectors", "[class][special-tools]")
{
    using Plain = musx::dom::details::StemAlterations;
    using Beamed = musx::dom::details::StemAlterationsUnderBeam;
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            auto session = musx::factory::DocumentFactory::begin();
            const auto document = session.getDocument();
            const auto plainReport = importStemDetail<Plain>(makeDetailContainer(epoch, 0, 42, {-12, 21, 0, 0, -2809}, "ST", order), epoch, document);
            const auto beamedReport =
                importStemDetail<Beamed>(makeDetailContainer(epoch, 0, 43, {9, -15, 0, 0, std::int16_t(0x0efa)}, "St", order), epoch, document);
            const auto plain = document->getDetails()->get<Plain>(musx::dom::SCORE_PARTID, 42);
            const auto beamed = document->getDetails()->get<Beamed>(musx::dom::SCORE_PARTID, 43);
            REQUIRE(plain);
            REQUIRE(beamed);
            CHECK(plain->upVertAdjust == -12);
            CHECK(plain->downVertAdjust == 21);
            CHECK(plain->upHorzAdjust == -11);
            CHECK(plain->downHorzAdjust == 7);
            CHECK(beamed->upVertAdjust == 9);
            CHECK(beamed->downVertAdjust == -15);
            CHECK(beamed->upHorzAdjust == 14);
            CHECK(beamed->downHorzAdjust == -6);
            for (const auto* name : {"upVertAdjust", "downVertAdjust", "upHorzAdjust", "downHorzAdjust"}) {
                const auto* plainField =
                    plainReport.findField<Plain>(name, musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(42));
                const auto* beamedField =
                    beamedReport.findField<Beamed>(name, musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(43));
                REQUIRE(plainField);
                REQUIRE(beamedField);
                CHECK(plainField->origin == ValueOrigin::LegacyMus);
                CHECK(beamedField->origin == ValueOrigin::LegacyMus);
            }
        }
    }
}

TEST_CASE("Stem alterations recover zlib class records", "[class][special-tools]")
{
    using Plain = musx::dom::details::StemAlterations;
    using Beamed = musx::dom::details::StemAlterationsUnderBeam;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    const auto plainReport =
        importStemDetail<Plain>(makeDetailClassContainer(0, 42, musx::dom::SCORE_PARTID, {-12, 21, 0, 0, -2809}, ByteOrder::LittleEndian, 0x042a),
            FormatEpoch::ZlibLegacy, document);
    const auto beamedReport = importStemDetail<Beamed>(
        makeDetailClassContainer(0, 43, musx::dom::SCORE_PARTID, {9, -15, 0, 0, std::int16_t(0x0efa)}, ByteOrder::LittleEndian, 0x03ff),
        FormatEpoch::ZlibLegacy, document);
    const auto plain = document->getDetails()->get<Plain>(musx::dom::SCORE_PARTID, 42);
    const auto beamed = document->getDetails()->get<Beamed>(musx::dom::SCORE_PARTID, 43);
    REQUIRE(plain);
    REQUIRE(beamed);
    CHECK(plain->upVertAdjust == -12);
    CHECK(plain->downVertAdjust == 21);
    CHECK(plain->upHorzAdjust == -11);
    CHECK(plain->downHorzAdjust == 7);
    CHECK(beamed->upVertAdjust == 9);
    CHECK(beamed->downVertAdjust == -15);
    CHECK(beamed->upHorzAdjust == 14);
    CHECK(beamed->downHorzAdjust == -6);
    const auto* plainField =
        plainReport.findField<Plain>("upHorzAdjust", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(42));
    const auto* beamedField =
        beamedReport.findField<Beamed>("downHorzAdjust", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(43));
    REQUIRE(plainField);
    REQUIRE(beamedField);
    CHECK(plainField->origin == ValueOrigin::LegacyMus);
    CHECK(beamedField->origin == ValueOrigin::LegacyMus);
    CHECK(plainField->rawValue == -11);
    CHECK(beamedField->rawValue == -6);
}

TEST_CASE("Stem alterations use incidence zero when later incidences contain data", "[class][special-tools]")
{
    using Plain = musx::dom::details::StemAlterations;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    const auto report = importStemDetail<Plain>(
        makeDetailContainer(FormatEpoch::UncompressedLegacy, 0, 42, {1, 2, 0, 0, 3, 4, 5, 0, 0, 6}, "ST"), FormatEpoch::UncompressedLegacy, document);
    const auto stem = document->getDetails()->get<Plain>(musx::dom::SCORE_PARTID, 42);
    REQUIRE(stem);
    CHECK(stem->upVertAdjust == 1);
    CHECK(stem->downVertAdjust == 2);
    CHECK(stem->downHorzAdjust == 3);
    CHECK(report.diagnostics.empty());
}

TEST_CASE("Stem alterations retain the first detail before blank trailing rows", "[class][special-tools]")
{
    using Plain = musx::dom::details::StemAlterations;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    const auto report =
        importStemDetail<Plain>(makeDetailContainer(FormatEpoch::UncompressedLegacy, 0, 42, {18, -6, 0, 0, -2809, 0, 0, 0, 0, 0}, "ST"),
            FormatEpoch::UncompressedLegacy, document);
    const auto stem = document->getDetails()->get<Plain>(musx::dom::SCORE_PARTID, 42);
    REQUIRE(stem);
    CHECK(stem->upVertAdjust == 18);
    CHECK(stem->downVertAdjust == -6);
    CHECK(stem->upHorzAdjust == -11);
    CHECK(stem->downHorzAdjust == 7);
    CHECK(report.diagnostics.empty());
}

TEST_CASE("Stem alterations retain zero adjustments only for valid entries", "[class][special-tools]")
{
    using Plain = musx::dom::details::StemAlterations;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    const auto report = importStemDetail<Plain>(
        makeDetailContainer(FormatEpoch::DclLegacy, 0, 3, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, "ST"), FormatEpoch::DclLegacy, document);
    const auto stem = document->getDetails()->get<Plain>(musx::dom::SCORE_PARTID, 3);
    REQUIRE(stem);
    CHECK(stem->upVertAdjust == 0);
    CHECK(stem->downVertAdjust == 0);
    CHECK(report.diagnostics.empty());

    auto zeroSession = musx::factory::DocumentFactory::begin();
    const auto zeroDocument = zeroSession.getDocument();
    const auto zeroReport = importStemDetail<Plain>(
        makeDetailContainer(FormatEpoch::DclLegacy, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, "ST"), FormatEpoch::DclLegacy, zeroDocument);
    CHECK(zeroDocument->getDetails()->getAllSources<Plain>().empty());
    CHECK(zeroReport.diagnostics.empty());

    auto singleZeroSession = musx::factory::DocumentFactory::begin();
    const auto singleZeroDocument = singleZeroSession.getDocument();
    const auto singleZeroReport = importStemDetail<musx::dom::details::StemAlterationsUnderBeam>(
        makeDetailContainer(FormatEpoch::DclLegacy, 0, 0, {8, -4, 0, 0, std::int16_t(0x0102)}, "St"), FormatEpoch::DclLegacy, singleZeroDocument);
    CHECK(singleZeroDocument->getDetails()->getAllSources<musx::dom::details::StemAlterationsUnderBeam>().empty());
    CHECK(singleZeroReport.diagnostics.empty());
}

TEST_CASE("Finale 2002 beamed stem edit recovers vertical and horizontal offsets", "[class][special-tools]")
{
    using Beamed = musx::dom::details::StemAlterationsUnderBeam;
    const auto baseline = readFixture("evidence/F2002/F2002-16ths.mus");
    CHECK(baseline.document->getDetails()->getAllSources<Beamed>().empty());

    const auto edited = readFixture("evidence/F2002/F2002-16ths-mvstem.mus");
    const auto stem = edited.document->getDetails()->get<Beamed>(musx::dom::SCORE_PARTID, 2);
    REQUIRE(stem);
    CHECK(stem->upVertAdjust == 42);
    CHECK(stem->downVertAdjust == 0);
    CHECK(stem->upHorzAdjust == -16);
    CHECK(stem->downHorzAdjust == 0);
    const auto* vertical =
        edited.report.findField<Beamed>("upVertAdjust", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(2));
    const auto* horizontal =
        edited.report.findField<Beamed>("upHorzAdjust", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(2));
    REQUIRE(vertical);
    REQUIRE(horizontal);
    CHECK(vertical->rawValue == 42);
    CHECK(horizontal->rawValue == -16);
    CHECK(vertical->origin == ValueOrigin::LegacyMus);
    CHECK(horizontal->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Primary beam alterations recover both stem directions", "[class][special-tools]")
{
    using Down = musx::dom::details::BeamAlterationsDownStem;
    using Up = musx::dom::details::BeamAlterationsUpStem;
    // The ten-word layout exists from Finale 2000 build 1; an earlier source's rows are separate alterations.
    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        const std::vector<std::int16_t> downWords = {-4, 12, 6, -8, 0, 2, -1, 0, 0, 0};
        const std::vector<std::int16_t> upWords = {3, -5, 7, 9, 0, 1, 64, 0, 0, 0};
        const auto downReport = importBeamDetail<Down>(makeDetailContainer(epoch, 0, 42, downWords, "BL"), epoch, document);
        const auto upReport = importBeamDetail<Up>(makeDetailContainer(epoch, 0, 43, upWords, "BH"), epoch, document);
        const auto down = document->getDetails()->get<Down>(musx::dom::SCORE_PARTID, 42);
        const auto up = document->getDetails()->get<Up>(musx::dom::SCORE_PARTID, 43);
        REQUIRE(down);
        REQUIRE(up);
        CHECK(down->leftOffsetH == -4);
        CHECK(down->leftOffsetY == 12);
        CHECK(down->rightOffsetH == 6);
        CHECK(down->rightOffsetY == -8);
        CHECK(down->dura == 0);
        CHECK(down->flattenStyle == Down::FlattenStyle::OnStandardNote);
        CHECK(down->beamWidth == -1);
        CHECK(up->leftOffsetH == 3);
        CHECK(up->leftOffsetY == -5);
        CHECK(up->rightOffsetH == 7);
        CHECK(up->rightOffsetY == 9);
        CHECK(up->flattenStyle == Up::FlattenStyle::AlwaysFlat);
        CHECK(up->beamWidth == (epoch == FormatEpoch::DclLegacy ? 64 : 4096));
        const auto* downField =
            downReport.findField<Down>("beamWidth", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(42));
        REQUIRE(downField);
        CHECK(downField->origin == ValueOrigin::LegacyMus);
        CHECK(downField->rawValue == -1);
        const auto* upField = upReport.findField<Up>("leftOffsetH", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(43));
        REQUIRE(upField);
        CHECK(upField->rawValue == 3);
    }
}

TEST_CASE("Primary beam alterations keep the last of several logical elements", "[class][special-tools]")
{
    using Down = musx::dom::details::BeamAlterationsDownStem;
    using Up = musx::dom::details::BeamAlterationsUpStem;
    const std::vector<std::int16_t> words = {0, 24, 0, -18, 0, 2, -1, 0, 0, 0, 5, 18, 7, -12, 0, 3, -1, 0, 0, 0};
    const auto checkLast = [](const auto& beam, const ImportReport& report, musx::dom::Cmper entry) {
        using Beam = std::remove_cvref_t<decltype(*beam)>;
        REQUIRE(beam);
        CHECK(beam->leftOffsetH == 5);
        CHECK(beam->leftOffsetY == 18);
        CHECK(beam->rightOffsetH == 7);
        CHECK(beam->rightOffsetY == -12);
        CHECK(beam->flattenStyle == Beam::FlattenStyle::OnExtremeNote);
        CHECK(beam->beamWidth == -1);
        CHECK(report.diagnostics.empty());
        const auto* field = report.findField<Beam>("leftOffsetY", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, entry);
        REQUIRE(field);
        CHECK(field->origin == ValueOrigin::LegacyMus);
        CHECK(field->rawValue == 18);
    };
    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        CAPTURE(epoch);
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        const auto report = importBeamDetail<Down>(makeDetailContainer(epoch, 0, 42, words, "BL"), epoch, document);
        CHECK(document->getDetails()->getAllSources<Down>().size() == 1);
        checkLast(document->getDetails()->get<Down>(musx::dom::SCORE_PARTID, 42), report, 42);
    }
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    const auto report = importBeamDetail<Up>(
        makeDetailClassContainer(0, 43, musx::dom::SCORE_PARTID, words, ByteOrder::LittleEndian, 0x0402), FormatEpoch::ZlibLegacy, document);
    CHECK(document->getDetails()->getAllSources<Up>().size() == 1);
    checkLast(document->getDetails()->get<Up>(musx::dom::SCORE_PARTID, 43), report, 43);
}

TEST_CASE("Beam alterations before Finale 2000 build 1 treat each five-word row as an alteration", "[class][special-tools]")
{
    using Down = musx::dom::details::BeamAlterationsDownStem;
    using Secondary = musx::dom::details::SecondaryBeamAlterationsDownStem;
    const std::vector<std::int16_t> words = {1, -2, 3, -4, 256, 6, -7, 8, -9, 512};
    const std::vector<std::pair<FormatEpoch, std::optional<SourceVersion>>> sources = {{FormatEpoch::CodaBanner, std::nullopt},
        {FormatEpoch::UncompressedLegacy, SourceVersion{.major = 4}}, {FormatEpoch::UncompressedLegacy, SourceVersion{.major = 5, .build = 0}}};
    for (const auto& [epoch, version] : sources) {
        CAPTURE(epoch);
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        const auto primaryReport = importBeamDetail<Down>(makeDetailContainer(epoch, 0, 42, words, "BL"), epoch, document, version);
        const auto secondaryReport = importBeamDetail<Secondary>(makeDetailContainer(epoch, 0, 43, words, "bL"), epoch, document, version);
        CHECK(primaryReport.diagnostics.empty());
        CHECK(secondaryReport.diagnostics.empty());
        const auto primary = document->getDetails()->get<Down>(musx::dom::SCORE_PARTID, 42);
        REQUIRE(primary);
        CHECK(primary->leftOffsetH == 6);
        CHECK(primary->rightOffsetY == -9);
        CHECK(primary->beamWidth == -1);
        const auto first = document->getDetails()->get<Secondary>(musx::dom::SCORE_PARTID, 43, musx::dom::Inci(0));
        const auto second = document->getDetails()->get<Secondary>(musx::dom::SCORE_PARTID, 43, musx::dom::Inci(1));
        REQUIRE(first);
        REQUIRE(second);
        CHECK(first->leftOffsetH == 1);
        CHECK(first->dura == 256);
        CHECK(first->beamWidth == -1);
        CHECK(second->leftOffsetH == 6);
        CHECK(second->dura == 512);
        CHECK(document->getDetails()->getAllSources<Secondary>().size() == 2);
    }

    // From build 1 the same ten words are one alteration.
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    importBeamDetail<Secondary>(makeDetailContainer(FormatEpoch::UncompressedLegacy, 0, 43, words, "bL"), FormatEpoch::UncompressedLegacy, document,
        SourceVersion{.major = 5, .build = 1});
    REQUIRE(document->getDetails()->getAllSources<Secondary>().size() == 1);
    const auto beam = document->getDetails()->get<Secondary>(musx::dom::SCORE_PARTID, 43, musx::dom::Inci(0));
    REQUIRE(beam);
    CHECK(beam->beamWidth == -7);
}

TEST_CASE("Primary beam alteration keeps a trailing half element", "[class][special-tools]")
{
    using Down = musx::dom::details::BeamAlterationsDownStem;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    const auto report =
        importBeamDetail<Down>(makeDetailContainer(FormatEpoch::UncompressedLegacy, 0, 42, {1, 2, 3, 4, 0, 3, 64, 0, 0, 0, 6, -7, 8, -9, 0}, "BL"),
            FormatEpoch::UncompressedLegacy, document);
    const auto beam = document->getDetails()->get<Down>(musx::dom::SCORE_PARTID, 42);
    REQUIRE(beam);
    CHECK(beam->leftOffsetH == 6);
    CHECK(beam->leftOffsetY == -7);
    CHECK(beam->rightOffsetH == 8);
    CHECK(beam->rightOffsetY == -9);
    CHECK(beam->beamWidth == -1);
    CHECK(report.diagnostics.empty());
}

TEST_CASE("Primary beam alteration rejects a record that does not end on an element boundary", "[class][special-tools]")
{
    using Down = musx::dom::details::BeamAlterationsDownStem;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    const auto report = importBeamDetail<Down>(
        makeDetailClassContainer(0, 42, musx::dom::SCORE_PARTID, {1, 2, 3, 4, 0, 3, -1, 0, 0, 0, 1, 2, 3}, ByteOrder::LittleEndian, 0x0401),
        FormatEpoch::ZlibLegacy, document);
    CHECK(document->getDetails()->getAllSources<Down>().empty());
    REQUIRE(report.diagnostics.size() == 1);
    CHECK(report.diagnostics.front().level == musx::util::Logger::LogLevel::Warning);
}

TEST_CASE("Primary beam alterations accept the five-word layout", "[class][special-tools]")
{
    using Down = musx::dom::details::BeamAlterationsDownStem;
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        const auto report = importBeamDetail<Down>(makeDetailContainer(epoch, 0, 42, {1, -2, 3, -4, 0}, "BL"), epoch, document);
        const auto beam = document->getDetails()->get<Down>(musx::dom::SCORE_PARTID, 42);
        REQUIRE(beam);
        CHECK(beam->leftOffsetH == 1);
        CHECK(beam->leftOffsetY == -2);
        CHECK(beam->rightOffsetH == 3);
        CHECK(beam->rightOffsetY == -4);
        CHECK(beam->dura == 0);
        CHECK(beam->beamWidth == -1);
        const auto* mode = report.findField<Down>("flattenStyle", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(42));
        REQUIRE(mode);
        CHECK(mode->origin == ValueOrigin::Finale27Default);
        const auto* width = report.findField<Down>("beamWidth", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(42));
        REQUIRE(width);
        CHECK(width->origin == ValueOrigin::LegacyBehavior);
    }
}

TEST_CASE("Primary beam alterations recover zlib class records", "[class][special-tools]")
{
    using Down = musx::dom::details::BeamAlterationsDownStem;
    using Up = musx::dom::details::BeamAlterationsUpStem;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    const std::vector<std::int16_t> words = {1, -2, 3, -4, 0, 3, -1, 0, 0, 0};
    const auto downReport = importBeamDetail<Down>(
        makeDetailClassContainer(0, 42, musx::dom::SCORE_PARTID, words, ByteOrder::LittleEndian, 0x0401), FormatEpoch::ZlibLegacy, document);
    const auto upReport = importBeamDetail<Up>(
        makeDetailClassContainer(0, 43, musx::dom::SCORE_PARTID, words, ByteOrder::LittleEndian, 0x0402), FormatEpoch::ZlibLegacy, document);
    const auto down = document->getDetails()->get<Down>(musx::dom::SCORE_PARTID, 42);
    const auto up = document->getDetails()->get<Up>(musx::dom::SCORE_PARTID, 43);
    REQUIRE(down);
    REQUIRE(up);
    CHECK(down->leftOffsetH == 1);
    CHECK(down->leftOffsetY == -2);
    CHECK(down->flattenStyle == Down::FlattenStyle::OnExtremeNote);
    CHECK(down->beamWidth == -1);
    CHECK(up->rightOffsetH == 3);
    CHECK(up->rightOffsetY == -4);
    const auto* downField =
        downReport.findField<Down>("flattenStyle", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(42));
    REQUIRE(downField);
    CHECK(downField->origin == ValueOrigin::LegacyMus);
    const auto* upField = upReport.findField<Up>("beamWidth", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(43));
    REQUIRE(upField);
    CHECK(upField->rawValue == -1);
}

TEST_CASE("Primary beam width changes units at Finale 2002", "[class][special-tools]")
{
    using Down = musx::dom::details::BeamAlterationsDownStem;
    for (const auto& [stored, before2002, from2002] : {std::tuple{std::int16_t(12), musx::dom::Efix(768), musx::dom::Efix(12)},
             std::tuple{std::int16_t(0), musx::dom::Efix(-1), musx::dom::Efix(0)},
             std::tuple{std::int16_t(-1), musx::dom::Efix(-1), musx::dom::Efix(-1)},
             std::tuple{std::int16_t(-6), musx::dom::Efix(-6), musx::dom::Efix(-6)}}) {
        for (const auto& [major, expected] : {std::pair{std::uint8_t(6), before2002}, std::pair{std::uint8_t(7), from2002}}) {
            auto session = musx::factory::DocumentFactory::begin();
            const auto document = session.getDocument();
            const auto report = importBeamDetail<Down>(makeDetailContainer(FormatEpoch::DclLegacy, 0, 42, {0, 0, 0, 0, 0, 2, stored, 0, 0, 0}, "BL"),
                FormatEpoch::DclLegacy, document, SourceVersion{.major = major});
            const auto beam = document->getDetails()->get<Down>(musx::dom::SCORE_PARTID, 42);
            REQUIRE(beam);
            CHECK(beam->beamWidth == expected);
            const auto* width = report.findField<Down>("beamWidth", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(42));
            REQUIRE(width);
            CHECK(width->rawValue == stored);
        }
    }
}

TEST_CASE("Finale 2001 primary beam width is stored in whole EVPUs", "[class][special-tools]")
{
    using Down = musx::dom::details::BeamAlterationsDownStem;
    const auto baseline = readFixture("evidence/F2001/F2001Win-16ths.mus");
    CHECK(baseline.document->getDetails()->getAllSources<Down>().empty());

    const auto edited = readFixture("evidence/F2001/F2001Win-16ths-beamwidth.mus");
    const auto beam = edited.document->getDetails()->get<Down>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(beam);
    CHECK(beam->beamWidth == 192);
    const auto* width = edited.report.findField<Down>("beamWidth", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(1));
    REQUIRE(width);
    CHECK(width->rawValue == 3);
    CHECK(width->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Finale 2002 primary beam width is stored in EFIX", "[class][special-tools]")
{
    using Up = musx::dom::details::BeamAlterationsUpStem;
    const auto baseline = readFixture("evidence/F2002/F2002-16ths.mus");
    CHECK(baseline.document->getDetails()->getAllSources<Up>().empty());

    const auto edited = readFixture("evidence/F2002/F2002-16ths-bmwidths.mus");
    const auto beam = edited.document->getDetails()->get<Up>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(beam);
    CHECK(beam->beamWidth == 384);
    const auto* width = edited.report.findField<Up>("beamWidth", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(1));
    REQUIRE(width);
    CHECK(width->rawValue == 384);
    CHECK(width->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Secondary beam alterations retain their width word without primary conversion", "[class][special-tools]")
{
    using Down = musx::dom::details::SecondaryBeamAlterationsDownStem;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    const auto report = importBeamDetail<Down>(makeDetailContainer(FormatEpoch::DclLegacy, 0, 42, {0, 0, 0, 0, 256, 2, -3, 0, 0, 0}, "bL"),
        FormatEpoch::DclLegacy, document, SourceVersion{.major = 6});
    const auto beam = document->getDetails()->get<Down>(musx::dom::SCORE_PARTID, 42, musx::dom::Inci(0));
    REQUIRE(beam);
    CHECK(beam->dura == 256);
    CHECK(beam->beamWidth == -3);
    const auto* width = report.findField<Down>("beamWidth", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), musx::dom::Inci(0), musx::dom::Cmper(42));
    REQUIRE(width);
    CHECK(width->origin == ValueOrigin::LegacyMus);
    CHECK(width->rawValue == -3);
}

TEST_CASE("Secondary beam alterations recover the controlled stem-direction edit", "[class][special-tools]")
{
    using Down = musx::dom::details::SecondaryBeamAlterationsDownStem;
    using Up = musx::dom::details::SecondaryBeamAlterationsUpStem;
    const auto baseline = readFixture("evidence/F2008/F2008-16ths.mus");
    CHECK(baseline.document->getDetails()->getAllSources<Down>().empty());
    CHECK(baseline.document->getDetails()->getAllSources<Up>().empty());

    const auto edited = readFixture("evidence/F2008/F2008-16ths-secdnup.mus");
    const auto down = edited.document->getDetails()->getArray<Down>(musx::dom::SCORE_PARTID, 9);
    const auto up = edited.document->getDetails()->getArray<Up>(musx::dom::SCORE_PARTID, 5);
    REQUIRE(down.size() == 1);
    REQUIRE(up.size() == 1);
    CHECK(down.front()->getInci() == musx::dom::Inci(0));
    CHECK(down.front()->leftOffsetH == 0);
    CHECK(down.front()->leftOffsetY == 6);
    CHECK(down.front()->rightOffsetH == 0);
    CHECK(down.front()->rightOffsetY == -6);
    CHECK(down.front()->dura == 256);
    CHECK(down.front()->beamWidth == -1);
    CHECK(up.front()->getInci() == musx::dom::Inci(0));
    CHECK(up.front()->leftOffsetY == -6);
    CHECK(up.front()->rightOffsetY == -6);
    CHECK(up.front()->dura == 256);
    CHECK(up.front()->beamWidth == -1);
    const auto* downDuration =
        edited.report.findField<Down>("dura", musx::dom::SCORE_PARTID, musx::dom::Cmper(0), musx::dom::Inci(0), musx::dom::Cmper(9));
    REQUIRE(downDuration);
    CHECK(downDuration->rawValue == 256);
    CHECK(downDuration->origin == ValueOrigin::LegacyMus);
}

} // namespace
} // namespace finale_mus_reader_tests
