// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using TimeUpper = musx::dom::others::TimeCompositeUpper;
using TimeLower = musx::dom::others::TimeCompositeLower;
using musx::util::Fraction;

struct UpperItem
{
    int beats;
    Fraction fraction;
    bool startGroup;
};

struct LowerItem
{
    int unit;
    bool startGroup;
};

ImportReport importComposites(
    const finale_mus_reader::container::ParsedContainer& parsed, musx::dom::DocumentPtr& document, std::optional<SourceVersion> version = {})
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
    profile.version = version;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importTimeCompositeLowers(context);
    finale_mus_reader::others::importTimeCompositeUppers(context);
    return report;
}

void checkUpper(const musx::dom::DocumentPtr& document, const ImportReport& report, musx::dom::Cmper cmper, const std::vector<UpperItem>& expected,
    finale_mus_reader::records::LegacyTag identity, bool storesGroups = true)
{
    const auto upper = document->getOthers()->get<TimeUpper>(musx::dom::SCORE_PARTID, cmper);
    REQUIRE(upper);
    REQUIRE(upper->items.size() == expected.size());
    const auto key = finale_mus_reader::instanceKey<TimeUpper>(musx::dom::SCORE_PARTID, cmper);
    REQUIRE(report.fields.contains(key));
    const auto& fields = report.fields.at(key);
    CHECK(fields.size() == 3 * expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        CHECK(upper->items[i]->beats == expected[i].beats);
        CHECK(upper->items[i]->fraction == expected[i].fraction);
        CHECK(upper->items[i]->startGroup == expected[i].startGroup);
        const auto prefix = "items[" + std::to_string(i) + "].";
        CHECK(fields.at(prefix + "beats").rawValue == expected[i].beats);
        CHECK(fields.at(prefix + "startGroup").rawValue == (expected[i].startGroup ? 1 : 0));
        for (const auto* member : {"beats", "fraction"}) {
            CHECK(fields.at(prefix + member).origin == ValueOrigin::LegacyMus);
            CHECK(fields.at(prefix + member).sourceIdentity == identity);
        }
        CHECK(fields.at(prefix + "startGroup").origin == (storesGroups ? ValueOrigin::LegacyMus : ValueOrigin::LegacyBehavior));
    }
}

void checkLower(const musx::dom::DocumentPtr& document, const ImportReport& report, musx::dom::Cmper cmper, const std::vector<LowerItem>& expected,
    finale_mus_reader::records::LegacyTag identity, bool storesGroups = true)
{
    const auto lower = document->getOthers()->get<TimeLower>(musx::dom::SCORE_PARTID, cmper);
    REQUIRE(lower);
    REQUIRE(lower->items.size() == expected.size());
    const auto key = finale_mus_reader::instanceKey<TimeLower>(musx::dom::SCORE_PARTID, cmper);
    REQUIRE(report.fields.contains(key));
    const auto& fields = report.fields.at(key);
    CHECK(fields.size() == 2 * expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        CHECK(lower->items[i]->unit == expected[i].unit);
        CHECK(lower->items[i]->startGroup == expected[i].startGroup);
        const auto prefix = "items[" + std::to_string(i) + "].";
        CHECK(fields.at(prefix + "unit").rawValue == expected[i].unit);
        CHECK(fields.at(prefix + "startGroup").rawValue == (expected[i].startGroup ? 1 : 0));
        for (const auto* member : {"unit"}) {
            CHECK(fields.at(prefix + member).origin == ValueOrigin::LegacyMus);
            CHECK(fields.at(prefix + member).sourceIdentity == identity);
        }
        CHECK(fields.at(prefix + "startGroup").origin == (storesGroups ? ValueOrigin::LegacyMus : ValueOrigin::LegacyBehavior));
    }
}

TEST_CASE("Composite time signature lists persist one collection of fixed items", "[class][time-composite]")
{
    CHECK(TimeUpper::xmlMappingArray().size() == 1);
    CHECK(TimeUpper::CompositeItem::xmlMappingArray().size() == 3);
    CHECK(TimeLower::xmlMappingArray().size() == 1);
    CHECK(TimeLower::CompositeItem::xmlMappingArray().size() == 2);
}

TEST_CASE("Composite upper lists import from DCL rows and zlib class records", "[class][time-composite]")
{
    for (const auto& [fixture, identity] : {
             std::tuple{"evidence/F2006/F2006-embedded-tif.mus", finale_mus_reader::records::packTag("TU")},
             std::tuple{"evidence/F2011/F2011-perc-instchange.mus", finale_mus_reader::records::LegacyTag(0x00ee)},
         }) {
        const auto result = readFixture(fixture);
        checkUpper(result.document, result.report, 3, {{3, Fraction(0), true}, {2, Fraction(0), false}}, identity);
        checkUpper(result.document, result.report, 4, {{4, Fraction(0), true}, {3, Fraction(0), false}}, identity);
        CHECK(result.document->getOthers()->getAllSources<TimeUpper>().size() == 2);
        CHECK(result.document->getOthers()->getAllSources<TimeLower>().empty());
    }
}

TEST_CASE("Composite lists are absent when no list is stored", "[class][time-composite]")
{
    for (const auto* fixture : {"evidence/F2002/F2002-baseline.mus", "evidence/F2012/F2012-baseline.mus"}) {
        const auto result = readFixture(fixture);
        CHECK(result.document->getOthers()->getAllSources<TimeUpper>().empty());
        CHECK(result.document->getOthers()->getAllSources<TimeLower>().empty());
    }
}

TEST_CASE("Composite items continue across fixed-row incidences and skip zero padding", "[class][time-composite]")
{
    // Upper: (2 1/4, start), (3), (5, start) across two incidences; the fourth item is padding.
    // Lower: (1024, start), (768); the third item is padding.
    const auto parsed = makeContainer({
        {1, "TU", {2, 0x0104, 1, 3, 0, 0}},
        {1, "TU", {5, 0, 1, 0, 0, 0}},
        {2, "TL", {1024, 1, 768, 0, 0, 0}},
    });
    musx::dom::DocumentPtr document;
    const auto report = importComposites(parsed, document, SourceVersion{.major = 5});
    checkUpper(
        document, report, 1, {{2, Fraction(1, 4), true}, {3, Fraction(0), false}, {5, Fraction(0), true}}, finale_mus_reader::records::packTag("TU"));
    checkLower(document, report, 2, {{1024, true}, {768, false}}, finale_mus_reader::records::packTag("TL"));

    const auto& upperFields = report.fields.at(finale_mus_reader::instanceKey<TimeUpper>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1)));
    CHECK(upperFields.at("items[0].fraction").rawValue == 0x0104);
    CHECK(upperFields.at("items[2].beats").decodedOffset == upperFields.at("items[0].beats").decodedOffset + 16);
}

TEST_CASE("Composite lists import from little-endian zlib records and report incomplete items", "[class][time-composite]")
{
    musx::dom::DocumentPtr document;
    const auto report = importComposites(makeClassContainer(
                                             {
                                                 SyntheticClassRow{0x00ed, {1024, 1, 768, 1, 0, 0}, 4},
                                                 SyntheticClassRow{0x00ee, {2, 0, 1, 3, 0, 0, 3, 0}, 5},
                                             },
                                             ByteOrder::LittleEndian),
        document);
    checkLower(document, report, 4, {{1024, true}, {768, true}}, finale_mus_reader::records::LegacyTag(0x00ed));
    checkUpper(document, report, 5, {{2, Fraction(0), true}, {3, Fraction(0), false}}, finale_mus_reader::records::LegacyTag(0x00ee));
    CHECK_FALSE(report.diagnostics.empty());
}

TEST_CASE("Composite lists import from the uncompressed epoch", "[class][time-composite]")
{
    // The upper list's third item continues into incidence 1, with the fraction stored as 60/120.
    for (const auto& [fixture, upperCmper, lowerCmper] : {
             std::tuple{"evidence/F372/F372-timecomp.mus", musx::dom::Cmper(3), musx::dom::Cmper(4)},
             std::tuple{"evidence/F97/F97-timecomp.mus", musx::dom::Cmper(3), musx::dom::Cmper(3)},
         }) {
        const auto result = readFixture(fixture);
        checkUpper(result.document, result.report, upperCmper, {{3, Fraction(0), true}, {1, Fraction(0), false}, {2, Fraction(1, 2), false}},
            finale_mus_reader::records::packTag("TU"));
        checkLower(result.document, result.report, lowerCmper, {{1024, true}, {512, false}, {256, false}}, finale_mus_reader::records::packTag("TL"));
        const auto& fields = result.report.fields.at(finale_mus_reader::instanceKey<TimeUpper>(musx::dom::SCORE_PARTID, upperCmper));
        CHECK(fields.at("items[2].fraction").rawValue == 0x3c78);
    }
}

TEST_CASE("Composite lists import a second group from DCL rows and zlib class records", "[class][time-composite]")
{
    // The second group starts on a later item of each list; the upper list spans three incidences
    // or one 36-byte payload.
    for (const auto& [fixture, cmper, secondBeats, upperIdentity, lowerIdentity] : {
             std::tuple{"evidence/F2005/F2005-timecomp.mus", musx::dom::Cmper(3), 1, finale_mus_reader::records::packTag("TU"),
                 finale_mus_reader::records::packTag("TL")},
             std::tuple{"evidence/F2012/F2012-timecomp.mus", musx::dom::Cmper(8), 4, finale_mus_reader::records::LegacyTag(0x00ee),
                 finale_mus_reader::records::LegacyTag(0x00ed)},
         }) {
        CAPTURE(fixture);
        const auto result = readFixture(fixture);
        checkUpper(result.document, result.report, cmper,
            {{3, Fraction(0), true}, {1, Fraction(0), false}, {2, Fraction(1, 2), false}, {secondBeats, Fraction(0), true}, {2, Fraction(0), false}},
            upperIdentity);
        checkLower(result.document, result.report, cmper, {{512, true}, {256, false}, {1024, true}, {256, false}}, lowerIdentity);
    }
}

TEST_CASE("Coda-banner composite lists store no groups and form one", "[class][time-composite]")
{
    const auto frac = readFixture("evidence/F100/F100-timecomp-frac.mus");
    checkUpper(frac.document, frac.report, 1, {{3, Fraction(0), true}, {1, Fraction(0), false}, {2, Fraction(1, 2), false}},
        finale_mus_reader::records::packTag("TU"), false);
    checkLower(frac.document, frac.report, 1, {{1024, true}, {512, false}, {256, false}}, finale_mus_reader::records::packTag("TL"), false);
    const auto& fields = frac.report.fields.at(finale_mus_reader::instanceKey<TimeUpper>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1)));
    CHECK(fields.at("items[2].fraction").rawValue == 0x0102);

    const auto twoItems = readFixture("evidence/F263/F263-timecomp.mus");
    checkUpper(
        twoItems.document, twoItems.report, 1, {{2, Fraction(0), true}, {3, Fraction(0), false}}, finale_mus_reader::records::packTag("TU"), false);
    checkLower(twoItems.document, twoItems.report, 1, {{1024, true}, {2048, false}}, finale_mus_reader::records::packTag("TL"), false);
}

TEST_CASE("Uncompressed composite lists store groups from Finale 3.5", "[class][time-composite]")
{
    const auto parsed = makeContainer({{1, "TU", {3, 0, 1, 0x0102, 0, 0}}, {2, "TL", {1024, 1, 512, 0, 0, 0}}});
    const auto tu = finale_mus_reader::records::packTag("TU");
    const auto tl = finale_mus_reader::records::packTag("TL");
    musx::dom::DocumentPtr document;

    const auto early = importComposites(parsed, document, SourceVersion{.major = 3, .minor = 2});
    checkUpper(document, early, 1, {{3, Fraction(0), true}, {1, Fraction(1, 2), false}}, tu, false);
    checkLower(document, early, 2, {{1024, true}, {1, false}, {512, false}}, tl, false);

    const auto later = importComposites(parsed, document, SourceVersion{.major = 3, .minor = 5});
    checkUpper(document, later, 1, {{3, Fraction(0), true}, {0x0102, Fraction(0), false}}, tu);
    checkLower(document, later, 2, {{1024, true}, {512, false}}, tl);
}

} // namespace
} // namespace finale_mus_reader_tests
