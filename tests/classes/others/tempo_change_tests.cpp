// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <tuple>
#include <vector>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using TempoChange = musx::dom::others::TempoChange;
constexpr musx::dom::Cmper measure = 7;

ImportReport importTempo(const finale_mus_reader::container::ParsedContainer& parsed, musx::dom::DocumentPtr& document)
{
    const auto index = LegacyRecordIndex::build(parsed);
    auto session = musx::factory::DocumentFactory::begin();
    document = session.getDocument();
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    ImportReport report(parsed.formatEpoch);
    finale_mus_reader::PendingReferences pending;
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importTempoChanges(context);
    return report;
}

std::vector<std::int16_t> tempoWords(ByteOrder order, std::int32_t edu, std::int32_t ratio, std::int16_t unit, std::int16_t flags)
{
    const auto pair = [order](std::int32_t value) {
        const auto first = static_cast<std::int16_t>(static_cast<std::uint32_t>(value) >> 16U);
        const auto second = static_cast<std::int16_t>(value);
        return order == ByteOrder::BigEndian ? std::array{first, second} : std::array{second, first};
    };
    const auto e = pair(edu);
    const auto r = pair(ratio);
    return {e[0], e[1], r[0], r[1], unit, flags};
}

void checkTempo(const musx::dom::DocumentPtr& document, const ImportReport& report, musx::dom::Inci inci, std::int32_t edu, std::int32_t ratio,
    int unit, bool relative)
{
    const auto tempo = document->getOthers()->get<TempoChange>(musx::dom::SCORE_PARTID, measure, inci);
    REQUIRE(tempo);
    CHECK(tempo->eduPosition == edu);
    CHECK(tempo->ratio == ratio);
    CHECK(tempo->unit == unit);
    CHECK(tempo->isRelative == relative);
    const auto key = finale_mus_reader::instanceKey<TempoChange>(musx::dom::SCORE_PARTID, measure, inci);
    REQUIRE(report.fields.contains(key));
    const auto& fields = report.fields.at(key);
    std::set<std::string> names;
    for (const auto& [name, info] : fields) {
        names.insert(name);
        CHECK(info.origin == ValueOrigin::LegacyMus);
    }
    CHECK(names == std::set<std::string>{"eduPosition", "ratio", "unit", "isRelative"});
    CHECK(fields.at("eduPosition").rawValue == edu);
    CHECK(fields.at("ratio").rawValue == ratio);
    CHECK(fields.at("unit").rawValue == unit);
    CHECK(fields.at("isRelative").rawValue == static_cast<std::int64_t>(relative));
}

TEST_CASE("TempoChange recovers fixed rows in every pre-zlib epoch", "[class][tempo-change]")
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            const auto first = tempoWords(order, 65540, 123456, 1000, 0x0002);
            const auto second = tempoWords(order, 1024, -1000, 1000, 0x0003);
            const auto row = [](const std::vector<std::int16_t>& words) {
                std::array<std::int16_t, 6> result{};
                std::copy(words.begin(), words.end(), result.begin());
                return result;
            };
            musx::dom::DocumentPtr document;
            const auto report = importTempo(makeContainer({{measure, "AC", row(first)}, {measure, "AC", row(second)}}, epoch, order), document);
            checkTempo(document, report, 0, 65540, 123456, 1000, false);
            checkTempo(document, report, 1, 1024, -1000, 1000, true);
        }
    }
    CHECK(TempoChange::xmlMappingArray().size() == 4);
}

TEST_CASE("TempoChange splits coalesced zlib elements", "[class][tempo-change]")
{
    for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        auto words = tempoWords(order, 0, 171798, 1000, 0x0002);
        const auto next = tempoWords(order, 2048, 500, 1000, 0x0003);
        words.insert(words.end(), next.begin(), next.end());
        musx::dom::DocumentPtr document;
        const auto report = importTempo(makeClassContainer(0x00f0, words, order, measure), document);
        checkTempo(document, report, 0, 0, 171798, 1000, false);
        checkTempo(document, report, 1, 2048, 500, 1000, true);
    }
}

TEST_CASE("Controlled Coda and DCL tempo changes retain relative source values", "[class][tempo-change]")
{
    for (const auto* baseline : {"evidence/F100/F100-baseline.mus", "evidence/F2002/F2002-baseline.mus"}) {
        const auto result = readFixture(baseline);
        CHECK(result.document->getOthers()->getAllSources<TempoChange>().empty());
    }
    for (const auto& [fixture, edu, ratio, unit] : {
             std::tuple{"evidence/F100/F100-tempo.mus", 2048, 123, 123},
             std::tuple{"evidence/F2002/F2002-tempo.mus", 1024, 1210, 1000},
         }) {
        const auto result = readFixture(fixture);
        const auto tempos = result.document->getOthers()->getArray<TempoChange>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        REQUIRE(tempos.size() == 1);
        const auto& tempo = tempos.front();
        CHECK(tempo->getInci() == musx::dom::Inci(0));
        CHECK(tempo->eduPosition == edu);
        CHECK(tempo->ratio == ratio);
        CHECK(tempo->unit == unit);
        CHECK(tempo->isRelative);
        const auto key = finale_mus_reader::instanceKey<TempoChange>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(0));
        REQUIRE(result.report.fields.contains(key));
        const auto& fields = result.report.fields.at(key);
        CHECK(fields.at("eduPosition").rawValue == edu);
        CHECK(fields.at("ratio").rawValue == ratio);
        CHECK(fields.at("unit").rawValue == unit);
        CHECK(fields.at("isRelative").rawValue == 1);
        for (const auto& [name, field] : fields) {
            static_cast<void>(name);
            CHECK(field.origin == ValueOrigin::LegacyMus);
        }
    }
}

TEST_CASE("Controlled zlib tempo changes split and retain their measure incidences", "[class][tempo-change]")
{
    const auto result = readFixture("evidence/F2012/F2012-jwtempo.mus");
    const auto firstMeasure = result.document->getOthers()->getArray<TempoChange>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    const std::array ratios{
        111848, 118838, 125829, 132819, 139810, 146800, 153791, 160781, 167772, 174762, 181753, 188743, 195734, 202724, 209715, 216705};
    REQUIRE(firstMeasure.size() == ratios.size());
    for (std::size_t i = 0; i < ratios.size(); ++i) {
        const auto& tempo = firstMeasure.at(i);
        CHECK(tempo->getInci() == static_cast<musx::dom::Inci>(i));
        CHECK(tempo->eduPosition == static_cast<int>(256 * i));
        CHECK(tempo->ratio == ratios[i]);
        CHECK(tempo->unit == 1000);
        CHECK_FALSE(tempo->isRelative);
        const auto key = finale_mus_reader::instanceKey<TempoChange>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1), static_cast<musx::dom::Inci>(i));
        REQUIRE(result.report.fields.contains(key));
        CHECK(result.report.fields.at(key).at("ratio").rawValue == ratios[i]);
    }
    const auto secondMeasure = result.document->getOthers()->getArray<TempoChange>(musx::dom::SCORE_PARTID, musx::dom::Cmper(2));
    REQUIRE(secondMeasure.size() == 1);
    CHECK(secondMeasure.front()->eduPosition == 0);
    CHECK(secondMeasure.front()->ratio == 223696);
    CHECK(secondMeasure.front()->unit == 1000);
    CHECK_FALSE(secondMeasure.front()->isRelative);
}

TEST_CASE("TempoChange leaves absent and incomplete records empty", "[class][tempo-change]")
{
    musx::dom::DocumentPtr document;
    const auto absent = importTempo(makeContainer({}), document);
    CHECK(document->getOthers()->getAllSources<TempoChange>().empty());
    CHECK(absent.fields.empty());

    const auto incomplete = importTempo(makeClassContainer(0x00f0, {0, 0, 500, 0, 1000, 1, 42}, ByteOrder::LittleEndian, measure), document);
    CHECK(document->getOthers()->getArray<TempoChange>(musx::dom::SCORE_PARTID, measure).size() == 1);
    CHECK_FALSE(incomplete.diagnostics.empty());
}

} // namespace
} // namespace finale_mus_reader_tests
