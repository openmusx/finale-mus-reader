// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

void appendFixedRows(std::vector<SyntheticRow>& rows, std::uint16_t cmper, const char* tag, const std::vector<std::int16_t>& words)
{
    for (std::size_t at = 0; at < words.size(); at += 6) {
        SyntheticRow row{cmper, tag, {}};
        const auto count = std::min<std::size_t>(6, words.size() - at);
        std::copy_n(words.begin() + static_cast<std::ptrdiff_t>(at), count, row.words.begin());
        rows.push_back(row);
    }
}

std::vector<std::int16_t> customKeyMapWords(ByteOrder byteOrder)
{
    constexpr std::array<std::pair<std::int16_t, std::int16_t>, 6> steps{{
        {std::int16_t(0), std::int16_t(-32768)},
        {std::int16_t(0), std::int16_t(0)},
        {std::int16_t(1), std::int16_t(-32768)},
        {std::int16_t(1), std::int16_t(0)},
        {std::int16_t(2), std::int16_t(-32768)},
        {std::int16_t(2), std::int16_t(0)},
    }};
    std::vector<std::int16_t> result;
    result.reserve(steps.size() * 2);
    for (const auto& [hlevel, flag] : steps) {
        if (byteOrder == ByteOrder::LittleEndian) {
            result.insert(result.end(), {hlevel, flag});
        } else {
            result.insert(result.end(), {flag, hlevel});
        }
    }
    return result;
}

std::vector<SyntheticRow> customKeyFixedRows(ByteOrder byteOrder)
{
    std::vector<SyntheticRow> rows;
    appendFixedRows(rows, 17, "An", {-7, -6, -5, -4, -3, -2, -1, 91, 92, 93, 94, 95});
    appendFixedRows(rows, 17, "Ap", {1, 2, 3, 4, 5, 6, 7, 91, 92, 93, 94, 95});
    appendFixedRows(rows, 17, "On", {6, 5, 4, 3, 2, 1, 0, 91, 92, 93, 94, 95});
    appendFixedRows(rows, 17, "Op", {0, 1, 2, 3, 4, 5, 6, 91, 92, 93, 94, 95});
    appendFixedRows(rows, 17, "Tn", {7, 6, 5, 4, 3, 2, 1, 0, 91, 92, 93, 94});
    appendFixedRows(rows, 17, "Tp", {0, 1, 2, 3, 4, 5, 6, 7, 91, 92, 93, 94});
    appendFixedRows(rows, 17, "KF", {5, 7, 81, 82, 83, 84});
    appendFixedRows(rows, 17, "KA", {3, 60, 9, 1, 12, 1});
    appendFixedRows(rows, 17, "KM", customKeyMapWords(byteOrder));
    return rows;
}

std::vector<SyntheticClassRow> customKeyClassRows(ByteOrder byteOrder)
{
    return {
        {0x0072, {-7, -6, -5, -4, -3, -2, -1, 91, 92, 93, 94, 95}, 17},
        {0x0073, {1, 2, 3, 4, 5, 6, 7, 91, 92, 93, 94, 95}, 17},
        {0x0074, {6, 5, 4, 3, 2, 1, 0, 91, 92, 93, 94, 95}, 17},
        {0x0075, {0, 1, 2, 3, 4, 5, 6, 91, 92, 93, 94, 95}, 17},
        {0x009a, {7, 6, 5, 4, 3, 2, 1, 0, 91, 92, 93, 94}, 17},
        {0x009b, {0, 1, 2, 3, 4, 5, 6, 7, 91, 92, 93, 94}, 17},
        {0x00a0, {5, 7, 81, 82, 83, 84}, 17},
        {0x00a1, customKeyMapWords(byteOrder), 17},
        {0x00a2, {3, 60, 9, 1, 12, 1}, 17},
    };
}

void importOtherCustomKeys(const finale_mus_reader::ImportContext& context)
{
    finale_mus_reader::others::importAcciAmountFlats(context);
    finale_mus_reader::others::importAcciAmountSharps(context);
    finale_mus_reader::others::importAcciOrderFlats(context);
    finale_mus_reader::others::importAcciOrderSharps(context);
    finale_mus_reader::others::importKeyAttributes(context);
    finale_mus_reader::others::importKeyFormats(context);
    finale_mus_reader::others::importKeyMapArrays(context);
    finale_mus_reader::others::importTonalCenterFlats(context);
    finale_mus_reader::others::importTonalCenterSharps(context);
}

template <typename Target, typename Element, std::size_t Size>
bool hasValues(const musx::dom::DocumentPtr& document, const std::array<Element, Size>& expected)
{
    const auto value = document->getOthers()->get<Target>(musx::dom::SCORE_PARTID, 17);
    return value && std::equal(value->values.begin(), value->values.end(), expected.begin(), expected.end());
}

void verifyOtherCustomKeys(const musx::dom::DocumentPtr& document)
{
    using namespace musx::dom::others;
    expectMapping(hasValues<AcciAmountFlats>(document, std::array<int, 7>{-7, -6, -5, -4, -3, -2, -1})
                      && hasValues<AcciAmountSharps>(document, std::array<int, 7>{1, 2, 3, 4, 5, 6, 7})
                      && hasValues<AcciOrderFlats>(document, std::array<unsigned, 7>{6, 5, 4, 3, 2, 1, 0})
                      && hasValues<AcciOrderSharps>(document, std::array<unsigned, 7>{0, 1, 2, 3, 4, 5, 6})
                      && hasValues<TonalCenterFlats>(document, std::array<unsigned, 8>{7, 6, 5, 4, 3, 2, 1, 0})
                      && hasValues<TonalCenterSharps>(document, std::array<unsigned, 8>{0, 1, 2, 3, 4, 5, 6, 7}),
        "A custom-key array was not recovered or was not capped to its DOM size");

    const auto format = document->getOthers()->get<KeyFormat>(musx::dom::SCORE_PARTID, 17);
    const auto attributes = document->getOthers()->get<KeyAttributes>(musx::dom::SCORE_PARTID, 17);
    const auto map = document->getOthers()->get<KeyMapArray>(musx::dom::SCORE_PARTID, 17);
    expectMapping(format && format->semitones == 5 && format->scaleTones == 7 && attributes && attributes->harmRefer == 3
                      && attributes->middleCKey == 60 && attributes->fontSym == 9 && attributes->gotoKey == 1 && attributes->symbolList == 12
                      && attributes->hasClefOctv && map && map->steps.size() == 5 && map->steps[0]->diatonic && map->steps[0]->hlevel == 0
                      && !map->steps[1]->diatonic && map->steps[1]->hlevel == 0 && map->steps[2]->diatonic && map->steps[2]->hlevel == 1
                      && !map->steps[3]->diatonic && map->steps[3]->hlevel == 1 && map->steps[4]->diatonic && map->steps[4]->hlevel == 2,
        "A scalar custom-key record or epoch-specific key-map step "
        "failed to decode");
}

void testOtherCustomKeysAcrossEpochs()
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            const auto parsed = epoch == FormatEpoch::ZlibLegacy ? makeClassContainer(customKeyClassRows(byteOrder), byteOrder)
                                                                 : makeContainer(customKeyFixedRows(byteOrder), epoch, byteOrder);
            const auto index = LegacyRecordIndex::build(parsed);
            auto session = musx::factory::DocumentFactory::begin();
            const auto document = session.getDocument();
            auto referenceSession = musx::factory::DocumentFactory::begin();
            const auto reference = std::move(referenceSession).finish();
            ImportReport report(epoch);
            finale_mus_reader::PendingReferences pending;
            SourceProfile profile(epoch);
            profile.byteOrder = parsed.byteOrder;
            musx::factory::ConstructionContext construction;
            const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
            importOtherCustomKeys(context);
            finale_mus_reader::runDeferredChecks(pending);
            verifyOtherCustomKeys(document);
            expectMapping(reportedFieldCount(report) == 62, "The custom-key others import did not report every retained field");
        }
    }
}

void importAndVerifyDetail(FormatEpoch epoch, const char* tag, std::uint16_t classId, bool flats)
{
    const std::vector<std::int16_t> words{-3, -2, -1, 0, 1, 2, 3, 91, 92, 93};
    const auto parsed = epoch == FormatEpoch::ZlibLegacy ? makeDetailClassContainer(17, 4, 0, words, ByteOrder::LittleEndian, classId)
                                                         : makeDetailContainer(epoch, 17, 4, words, tag);
    const auto index = LegacyRecordIndex::build(parsed);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    ImportReport report(epoch);
    finale_mus_reader::PendingReferences pending;
    SourceProfile profile(epoch);
    profile.byteOrder = parsed.byteOrder;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    if (flats) {
        finale_mus_reader::details::importClefOctaveFlats(context);
        const auto value = document->getDetails()->get<musx::dom::details::ClefOctaveFlats>(musx::dom::SCORE_PARTID, 17, 4);
        expectMapping(value && value->values == std::vector<int>{-3, -2, -1, 0, 1, 2, 3},
            "ClefOctaveFlats was not recovered or capped to seven values in "
            "epoch "
                + std::to_string(static_cast<int>(epoch))
                + "; source rows=" + std::to_string(index.getDetails().getArray(finale_mus_reader::records::packTag(tag), 17, 4).size())
                + "; target=" + (value ? std::to_string(value->values.size()) : "missing"));
    } else {
        finale_mus_reader::details::importClefOctaveSharps(context);
        const auto value = document->getDetails()->get<musx::dom::details::ClefOctaveSharps>(musx::dom::SCORE_PARTID, 17, 4);
        expectMapping(value && value->values == std::vector<int>{-3, -2, -1, 0, 1, 2, 3}, "ClefOctaveSharps was not recovered or capped to seven "
                                                                                          "values in epoch "
                                                                                              + std::to_string(static_cast<int>(epoch)));
    }
    expectMapping(reportedFieldCount(report) == 7, "A custom-key clef octave import did not report every value");
}

void testClefOctaveArraysAcrossEpochs()
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        importAndVerifyDetail(epoch, "Cn", 0x0408, true);
        importAndVerifyDetail(epoch, "Cp", 0x0409, false);
    }
}

void testClefOctaveFlagNormalization()
{
    using Attributes = musx::dom::others::KeyAttributes;
    constexpr std::uint16_t keyCmper = 17;
    constexpr std::uint16_t clefId = 4;
    const std::vector<std::int16_t> octaveWords{-3, -2, -1, 0, 1, 2, 3, 91, 92, 93};
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto flats : {false, true}) {
            auto parsed = epoch == FormatEpoch::ZlibLegacy ? makeClassContainer({{0x00a2, {3, 60, 9, 1, 12, 0}, keyCmper}}, ByteOrder::LittleEndian)
                                                           : makeContainer({{keyCmper, "KA", {3, 60, 9, 1, 12, 0}}}, epoch);
            auto detailParsed = epoch == FormatEpoch::ZlibLegacy
                                    ? makeDetailClassContainer(keyCmper, clefId, 0, octaveWords, ByteOrder::LittleEndian, flats ? 0x0408 : 0x0409)
                                    : makeDetailContainer(epoch, keyCmper, clefId, octaveWords, flats ? "Cn" : "Cp");
            parsed.blocks.push_back(std::move(detailParsed.blocks.front()));

            const auto index = LegacyRecordIndex::build(parsed);
            auto session = musx::factory::DocumentFactory::begin();
            const auto document = session.getDocument();
            auto referenceSession = musx::factory::DocumentFactory::begin();
            const auto reference = std::move(referenceSession).finish();
            ImportReport report(epoch);
            finale_mus_reader::PendingReferences pending;
            SourceProfile profile(epoch);
            profile.byteOrder = parsed.byteOrder;
            musx::factory::ConstructionContext construction;
            const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
            finale_mus_reader::others::importKeyAttributes(context);
            if (flats) {
                finale_mus_reader::details::importClefOctaveFlats(context);
            } else {
                finale_mus_reader::details::importClefOctaveSharps(context);
            }
            finale_mus_reader::runDeferredChecks(pending);

            const auto attributes = document->getOthers()->get<Attributes>(musx::dom::SCORE_PARTID, keyCmper);
            const auto instance = finale_mus_reader::instanceKey<Attributes>(musx::dom::SCORE_PARTID, keyCmper);
            const auto* info = report.findField(instance, "hasClefOctv");
            expectMapping(attributes && attributes->hasClefOctv && info && info->origin == ValueOrigin::LegacyMusAdjusted && info->rawValue == 0,
                "Clef-octave details did not normalize a false source "
                "flag while preserving its provenance");
        }
    }
}

void testKeySymbolListAcrossEpochs()
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            const auto firstWord =
                epoch == FormatEpoch::CodaBanner || byteOrder == ByteOrder::LittleEndian ? std::int16_t{0x0062} : std::int16_t{0x6200};
            const std::vector<std::int16_t> words{firstWord, 0x7172, 0x7374, 0x7576, 0x7778};
            const auto parsed = epoch == FormatEpoch::ZlibLegacy ? makeDetailClassContainer(12, 0xfffe, 0, words, byteOrder, 0x0416)
                                                                 : makeDetailContainer(epoch, 12, 0xfffe, words, "KS", byteOrder);
            const auto index = LegacyRecordIndex::build(parsed);
            auto session = musx::factory::DocumentFactory::begin();
            const auto document = session.getDocument();
            auto referenceSession = musx::factory::DocumentFactory::begin();
            const auto reference = std::move(referenceSession).finish();
            ImportReport report(epoch);
            finale_mus_reader::PendingReferences pending;
            SourceProfile profile(epoch);
            profile.byteOrder = parsed.byteOrder;
            musx::factory::ConstructionContext construction;
            const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
            finale_mus_reader::details::importKeySymbolListElements(context);
            finale_mus_reader::runDeferredChecks(pending);
            const auto value = document->getDetails()->get<musx::dom::details::KeySymbolListElement>(musx::dom::SCORE_PARTID, 12, 0xfffe);
            expectMapping(value && value->getAlterationValue() == -2 && value->accidentalString == "b",
                "A key-symbol list element did not preserve its signed slot or bounded "
                "string in epoch "
                    + std::to_string(static_cast<int>(epoch))
                    + "; source rows=" + std::to_string(index.getDetails().getArray(finale_mus_reader::records::packTag("KS"), 12, 0xfffe).size())
                    + "; target=" + (value ? value->accidentalString : "missing"));
            expectMapping(reportedFieldCount(report) == 1, "A key-symbol list import did not report its string field");
        }
    }
}

TEST_CASE("Other custom-key classes across epochs", "[class]")
{
    testOtherCustomKeysAcrossEpochs();
}

TEST_CASE("Custom-key clef octave arrays across epochs", "[class]")
{
    testClefOctaveArraysAcrossEpochs();
}

TEST_CASE("Custom-key clef octave presence normalizes attributes", "[class]")
{
    testClefOctaveFlagNormalization();
}

TEST_CASE("Custom-key symbol list across epochs", "[class]")
{
    testKeySymbolListAcrossEpochs();
}
} // namespace
} // namespace finale_mus_reader_tests
