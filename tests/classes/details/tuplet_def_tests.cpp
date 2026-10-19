// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <set>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using TupletTestTarget = musx::dom::details::TupletDef;

void checkTupletRecord(const finale_mus_reader::container::ParsedContainer& parsed, SourceProfile profile, bool shortForm, bool modernFlags)
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    ImportReport report(profile.epoch);
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::details::importTupletDefs(context);

    const auto tuplet = document->getDetails()->get<TupletTestTarget>(musx::dom::SCORE_PARTID, 27, musx::dom::Inci(0));
    REQUIRE(tuplet);
    CHECK(tuplet->displayNumber == 3);
    CHECK(tuplet->displayDuration == 512);
    CHECK(tuplet->referenceNumber == 2);
    CHECK(tuplet->referenceDuration == 512);
    const auto key = finale_mus_reader::instanceKey<TupletTestTarget>(0, musx::dom::Cmper{0}, musx::dom::Inci(0), musx::dom::Cmper{27});
    const auto& fields = report.fields.at(key);
    CHECK(fields.size() == TupletTestTarget::xmlMappingArray().size());
    CHECK(fields.at("displayNumber").origin == ValueOrigin::LegacyMus);
    CHECK(fields.at("displayNumber").rawValue == 3);
    CHECK(fields.at("hidden").origin == ValueOrigin::Unmapped);

    if (shortForm) {
        CHECK(tuplet->tupOffX == 0);
        CHECK(tuplet->matchHooks);
        CHECK_FALSE(tuplet->smartTuplet);
        CHECK(fields.at("tupOffX").origin == ValueOrigin::LegacyBehavior);
        CHECK(fields.at("matchHooks").origin == ValueOrigin::LegacyBehavior);
        CHECK(fields.at("smartTuplet").origin == ValueOrigin::LegacyBehavior);
        return;
    }
    CHECK(tuplet->tupOffX == -12);
    CHECK(tuplet->tupOffY == 24);
    CHECK(tuplet->brackOffX == -3);
    CHECK(tuplet->brackOffY == 5);
    CHECK(tuplet->leftHookLen == -12);
    CHECK(tuplet->leftHookExt == 4);
    CHECK(tuplet->rightHookLen == -11);
    CHECK(tuplet->rightHookExt == 6);
    CHECK(tuplet->manualSlopeAdj == -7);
    CHECK(tuplet->numStyle == TupletTestTarget::NumberStyle::RatioPlusBothNotes);
    CHECK(tuplet->posStyle == TupletTestTarget::PositioningStyle::BeamSide);
    CHECK(tuplet->brackStyle == TupletTestTarget::BracketStyle::Bracket);
    CHECK(tuplet->smartTuplet == modernFlags);
    CHECK(tuplet->allowHorz);
    CHECK(tuplet->ignoreHorzNumOffset);
    CHECK(tuplet->breakBracket);
    CHECK(tuplet->matchHooks);
    CHECK(tuplet->useBottomNote);
    CHECK(tuplet->alwaysFlat == modernFlags);
    CHECK(tuplet->fullDura == modernFlags);
    CHECK(tuplet->metricCenter == modernFlags);
    CHECK(tuplet->avoidStaff == modernFlags);
    CHECK(tuplet->autoBracketStyle
          == (modernFlags ? TupletTestTarget::AutoBracketStyle::NeverBeamSide : TupletTestTarget::AutoBracketStyle::UnbeamedOnly));
    CHECK(fields.at("alwaysFlat").origin == (modernFlags ? ValueOrigin::LegacyMus : ValueOrigin::LegacyBehavior));
    CHECK(fields.at("smartTuplet").origin == (modernFlags ? ValueOrigin::LegacyMus : ValueOrigin::LegacyBehavior));
    CHECK(fields.at("leftHookLen").rawValue == static_cast<std::uint16_t>(-12));
}

TEST_CASE("Tuplet records decode each supported layout and byte order", "[class][tuplet-def]")
{
    const std::vector<std::int16_t> full{
        3, 512, 2, 512, 0x001f, -12, 24, -3, 5, static_cast<std::int16_t>(0x8000 | 0x4000 | 0x2000 | 0x0f80 | 0x0010 | 0x0003), -12, 4, -11, 6, -7};
    for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        auto coda = SourceProfile(FormatEpoch::CodaBanner);
        coda.byteOrder = order;
        checkTupletRecord(makeDetailContainer(coda.epoch, 0, 27, {3, 512, 2, 512, 9}, "TP", order), coda, true, false);
        auto uncompressed = SourceProfile(FormatEpoch::UncompressedLegacy);
        uncompressed.byteOrder = order;
        checkTupletRecord(makeDetailContainer(uncompressed.epoch, 0, 27, full, "TP", order), uncompressed, false, false);
        auto earlyUncompressed = SourceProfile(FormatEpoch::UncompressedLegacy);
        earlyUncompressed.byteOrder = order;
        checkTupletRecord(makeDetailContainer(earlyUncompressed.epoch, 0, 27, {3, 512, 2, 512, 9}, "TP", order), earlyUncompressed, true, false);
        for (const auto major : {9, 10}) {
            auto dcl = SourceProfile(FormatEpoch::DclLegacy);
            dcl.byteOrder = order;
            dcl.version = SourceVersion{.major = static_cast<std::uint8_t>(major)};
            checkTupletRecord(makeDetailContainer(dcl.epoch, 0, 27, full, "TP", order), dcl, false, major == 10);
        }
        auto zlib = SourceProfile(FormatEpoch::ZlibLegacy);
        zlib.byteOrder = order;
        checkTupletRecord(makeDetailClassContainer(0, 27, 0, full, order, 0x0430), zlib, false, true);
    }
}

TEST_CASE("Early tuplet appearance detail follows the legacy conversion", "[class][tuplet-def]")
{
    for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy}) {
            auto parsed = makeDetailContainer(epoch, 0, 27, {3, 512, 2, 512, 9}, "TP", order);
            auto appearance = makeDetailContainer(epoch, 0, 27, {34, 0, 12, 16, 0x4000 | 515}, "TN", order);
            parsed.blocks.push_back(std::move(appearance.blocks.front()));
            auto session = musx::factory::DocumentFactory::begin();
            const auto document = session.getDocument();
            const auto index = LegacyRecordIndex::build(parsed);
            auto referenceSession = musx::factory::DocumentFactory::begin();
            const auto reference = std::move(referenceSession).finish();
            ImportReport report(epoch);
            finale_mus_reader::PendingReferences pending;
            musx::factory::ConstructionContext construction;
            auto profile = SourceProfile(epoch);
            profile.byteOrder = order;
            if (epoch == FormatEpoch::UncompressedLegacy) {
                profile.version = SourceVersion{.major = 3, .minor = 2};
            }
            const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
            finale_mus_reader::details::importTupletDefs(context);

            const auto tuplet = document->getDetails()->get<TupletTestTarget>(0, 27, musx::dom::Inci(0));
            REQUIRE(tuplet);
            CHECK(tuplet->tupOffY == 34);
            CHECK(tuplet->brackOffY == 12);
            CHECK(tuplet->leftHookLen == -12);
            CHECK(tuplet->rightHookLen == -12);
            CHECK(tuplet->manualSlopeAdj == 16);
            CHECK(tuplet->matchHooks);
            CHECK_FALSE(tuplet->smartTuplet);
            CHECK(tuplet->numStyle == TupletTestTarget::NumberStyle::RatioPlusDenominatorNote);
            CHECK(tuplet->brackStyle == TupletTestTarget::BracketStyle::Bracket);
            const auto key = finale_mus_reader::instanceKey<TupletTestTarget>(0, musx::dom::Cmper{0}, musx::dom::Inci(0), musx::dom::Cmper{27});
            const auto& fields = report.fields.at(key);
            CHECK(fields.at("tupOffY").origin == ValueOrigin::LegacyMus);
            CHECK(fields.at("leftHookLen").origin == ValueOrigin::LegacyMusAdjusted);
            CHECK(fields.at("matchHooks").origin == ValueOrigin::LegacyBehavior);
            CHECK(fields.at("smartTuplet").origin == ValueOrigin::LegacyBehavior);
        }
    }
}

TEST_CASE("Tracked early tuplet source keeps each entry's ratio", "[class][tuplet-def]")
{
    const auto result = readFixture("evidence/F263/F263-beatchart.mus");
    for (const auto entry : {musx::dom::Cmper{49}, musx::dom::Cmper{51}}) {
        const auto tuplet = result.document->getDetails()->get<TupletTestTarget>(0, entry, musx::dom::Inci(0));
        REQUIRE(tuplet);
        CHECK(tuplet->displayNumber == 3);
        CHECK(tuplet->displayDuration == 1024);
        CHECK(tuplet->referenceNumber == 2);
        CHECK(tuplet->referenceDuration == 1024);
        CHECK(tuplet->tupOffY == 24);
        CHECK(tuplet->matchHooks);
        CHECK(tuplet->numStyle == TupletTestTarget::NumberStyle::Number);
        const auto key = finale_mus_reader::instanceKey<TupletTestTarget>(0, musx::dom::Cmper{0}, musx::dom::Inci(0), entry);
        CHECK(result.report.fields.at(key).at("tupOffY").origin == ValueOrigin::LegacyMus);
    }
}

} // namespace
} // namespace finale_mus_reader_tests
