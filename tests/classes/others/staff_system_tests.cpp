// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include <array>
#include <initializer_list>
#include <memory>
#include <set>

#include "class_test_support.h"
#include "staff_system_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

using StaffSystem = musx::dom::others::StaffSystem;
using Measure = musx::dom::others::Measure;

void addStaffSystemTestMeasures(const musx::dom::DocumentPtr& document, int lastMeasure)
{
    for (int measureId = 1; measureId <= lastMeasure; ++measureId) {
        auto measure =
            std::make_shared<Measure>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::None, musx::dom::Cmper(measureId));
        document->getOthers()->add(Measure::XmlNodeName, std::move(measure));
    }
}

void appendExpandedStaffSystemRows(std::vector<SyntheticRow>& rows, std::uint16_t systemId, std::int16_t startMeas, std::int16_t storedEndMeas)
{
    auto words = staffSystemWords(0, 1536, false);
    words[4] = startMeas;
    words[6] = storedEndMeas;
    for (std::size_t at = 0; at < words.size(); at += 6) {
        rows.push_back({systemId, "SS", {words[at], words[at + 1], words[at + 2], words[at + 3], words[at + 4], words[at + 5]}});
    }
}

void checkStaffSystem(const musx::dom::DocumentPtr& document, const ImportReport& report, int expectedTop, int expectedDistance,
    ValueOrigin expectedTopOrigin, int expectedTopRaw, musx::dom::Cmper systemId = musx::dom::Cmper(1), bool extended = true)
{
    const auto system = document->getOthers()->get<StaffSystem>(musx::dom::SCORE_PARTID, systemId);
    REQUIRE(system);
    CHECK(system->top == expectedTop);
    CHECK(system->left == 576);
    CHECK(system->right == -13);
    CHECK(system->bottom == -110);
    CHECK(system->startMeas == 4);
    CHECK(system->endMeas == 9);
    CHECK(system->horzPercent == 120.5);
    CHECK(system->ssysPercent == 70);
    CHECK(system->distanceToPrev == expectedDistance);
    CHECK(system->staffHeight == 6144);
    CHECK(system->noNames);
    CHECK_FALSE(system->hasStaffScaling);
    CHECK(system->placeEndSpaceBeforeBarline);
    CHECK(system->scaleVert);
    CHECK(system->holdMargins);
    CHECK(system->extraStartSystemSpace == (extended ? 24 : 0));
    CHECK(system->extraEndSystemSpace == (extended ? 36 : 0));
    CHECK(reportedFieldCount(report) == staffSystemFieldCount);
    for (const auto* member : {"startMeas", "horzPercent", "ssysPercent", "left", "right", "bottom", "noNames", "hasStaffScaling",
             "placeEndSpaceBeforeBarline", "scaleVert", "holdMargins"}) {
        const auto* source = report.findField<StaffSystem>(member, musx::dom::SCORE_PARTID, systemId);
        REQUIRE(source);
        CHECK(source->origin == ValueOrigin::LegacyMus);
    }
    const auto* endSource = report.findField<StaffSystem>("endMeas", musx::dom::SCORE_PARTID, systemId);
    REQUIRE(endSource);
    CHECK(endSource->origin == ValueOrigin::LegacyMus);
    CHECK(endSource->rawValue == 9);
    const auto* staffScalingSource = report.findField<StaffSystem>("hasStaffScaling", musx::dom::SCORE_PARTID, systemId);
    REQUIRE(staffScalingSource);
    CHECK(staffScalingSource->origin == ValueOrigin::LegacyMus);
    CHECK(staffScalingSource->rawValue == 0);
    const auto* topSource = report.findField<StaffSystem>("top", musx::dom::SCORE_PARTID, systemId);
    REQUIRE(topSource);
    CHECK(topSource->origin == expectedTopOrigin);
    for (const auto* member : {"extraStartSystemSpace", "extraEndSystemSpace"}) {
        const auto* source = report.findField<StaffSystem>(member, musx::dom::SCORE_PARTID, systemId);
        REQUIRE(source);
        CHECK(source->origin == (extended ? ValueOrigin::LegacyMus : ValueOrigin::LegacyBehavior));
    }
    CHECK(report.findField<StaffSystem>("horzPercent", 0, systemId)->rawValue == 12050);
    CHECK(report.findField<StaffSystem>("staffHeight", 0, systemId)->rawValue
          == (report.findField<StaffSystem>("staffHeight", 0, systemId)->origin == ValueOrigin::LegacyMus ? 1536 : 6144));
    CHECK(report.findField<StaffSystem>("top", 0, systemId)->rawValue == expectedTopRaw);
    CHECK(report.findField<StaffSystem>("distanceToPrev", 0, systemId)->rawValue == expectedDistance);
}

TEST_CASE("Fixed-row staff systems recover the base and Finale 2005 layouts", "[class][staff-system]")
{
    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            const auto words = staffSystemWords();
            std::vector<SyntheticRow> rows;
            for (std::size_t at = 0; at < 18; at += 6) {
                rows.push_back({1, "SS", {words[at], words[at + 1], words[at + 2], words[at + 3], words[at + 4], words[at + 5]}});
            }
            const auto document = emptyStaffSystemDocument();
            addStaffSystemTestMeasures(document, 8);
            auto profile = SourceProfile(epoch);
            profile.byteOrder = byteOrder;
            const auto report = importStaffSystems(makeContainer(rows, epoch, byteOrder), profile, document);
            const int expectedTop = -12;
            const int expectedDistance = epoch == FormatEpoch::UncompressedLegacy ? 0 : -490;
            const auto expectedTopOrigin = ValueOrigin::LegacyMus;
            const int expectedTopRaw = -12;
            checkStaffSystem(document, report, expectedTop, expectedDistance, expectedTopOrigin, expectedTopRaw);
        }
    }

    const auto words = staffSystemWords(0x2017, 1536, false);
    const auto parsed = makeContainer({{1, "SS", {words[0], words[1], words[2], words[3], words[4], words[5]}},
        {1, "SS", {words[6], words[7], words[8], words[9], words[10], words[11]}}});
    const auto document = emptyStaffSystemDocument();
    addStaffSystemTestMeasures(document, 8);
    auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    checkStaffSystem(document, importStaffSystems(parsed, profile, document), -12, 0, ValueOrigin::LegacyMus, -12, musx::dom::Cmper(1), false);
}

TEST_CASE("Uncompressed staff-system distance uses the first word after system one", "[class][staff-system]")
{
    const auto words = staffSystemWords(0x2017, 1536, false);
    const auto parsed = makeContainer({{1, "SS", {words[0], words[1], words[2], words[3], words[4], words[5]}},
        {1, "SS", {words[6], words[7], words[8], words[9], words[10], words[11]}}});
    const auto document = emptyStaffSystemDocument();
    addStaffSystemTestMeasures(document, 8);
    auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    checkStaffSystem(document, importStaffSystems(parsed, profile, document), -12, 0, ValueOrigin::LegacyMus, -12, musx::dom::Cmper(1), false);
}

TEST_CASE("Zlib staff systems recover both byte orders and ignore retired flag bits", "[class][staff-system]")
{
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        const auto document = emptyStaffSystemDocument();
        addStaffSystemTestMeasures(document, 8);
        auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
        profile.byteOrder = byteOrder;
        checkStaffSystem(document, importStaffSystems(makeClassContainer(0x00df, staffSystemWords(), byteOrder, 1), profile, document), -12, -490,
            ValueOrigin::LegacyMus, -12);

        const auto ignoredDocument = emptyStaffSystemDocument();
        addStaffSystemTestMeasures(ignoredDocument, 8);
        const auto report =
            importStaffSystems(makeClassContainer(0x00df, staffSystemWords(std::int16_t(-0x4000)), byteOrder, 1), profile, ignoredDocument);
        const auto ignored = ignoredDocument->getOthers()->get<StaffSystem>(0, 1);
        REQUIRE(ignored);
        CHECK_FALSE(ignored->noNames);
        CHECK_FALSE(ignored->hasStaffScaling);
        CHECK_FALSE(ignored->placeEndSpaceBeforeBarline);
        CHECK_FALSE(ignored->scaleVert);
        CHECK_FALSE(ignored->holdMargins);
        CHECK(reportedFieldCount(report) == staffSystemFieldCount);
    }
}

TEST_CASE("A zero legacy staff height takes the standard staff-height behavior", "[class][staff-system]")
{
    const auto document = emptyStaffSystemDocument();
    addStaffSystemTestMeasures(document, 8);
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    const auto report = importStaffSystems(makeClassContainer(0x00df, staffSystemWords(0, 0), ByteOrder::LittleEndian, 1), profile, document);
    const auto system = document->getOthers()->get<StaffSystem>(0, 1);
    REQUIRE(system);
    CHECK(system->staffHeight == 6144);
    const auto* source = report.findField<StaffSystem>("staffHeight", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(source);
    CHECK(source->origin == ValueOrigin::LegacyBehavior);
}

TEST_CASE("Incomplete staff-system layouts are rejected without partial objects", "[class][staff-system]")
{
    for (const auto size : {11, 13}) {
        const auto document = emptyStaffSystemDocument();
        auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
        profile.byteOrder = ByteOrder::LittleEndian;
        const auto report =
            importStaffSystems(makeClassContainer(0x00df, std::vector<std::int16_t>(size), ByteOrder::LittleEndian, 7), profile, document);
        CHECK(document->getOthers()->getAllSources<StaffSystem>().empty());
        CHECK(report.diagnostics.size() == 1);
    }
}

TEST_CASE("Compact staff systems recover independently of their container epoch", "[class][staff-system]")
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy}) {
        const auto document = emptyStaffSystemDocument();
        auto profile = SourceProfile(epoch);
        profile.byteOrder = ByteOrder::BigEndian;
        const auto report = importStaffSystems(
            makeContainer({{1, "SS", {-257, 389, -247, -299, 1, 0}}, {2, "SS", {-481, 517, -499, -521, 3, 128}}}, epoch), profile, document);
        const auto first = document->getOthers()->get<StaffSystem>(0, 1);
        const auto second = document->getOthers()->get<StaffSystem>(0, 2);
        REQUIRE(first);
        REQUIRE(second);
        CHECK(first->top == -257);
        CHECK(first->left == 389);
        CHECK(first->right == -247);
        CHECK(first->bottom == -299);
        CHECK(first->startMeas == 1);
        CHECK(first->endMeas == 0);
        CHECK(first->horzPercent == 0.0);
        CHECK(first->ssysPercent == 100);
        CHECK(first->staffHeight == 6144);
        CHECK_FALSE(first->hasStaffScaling);
        CHECK(first->distanceToPrev == 0);
        CHECK(first->holdMargins);
        CHECK(second->top == 0);
        CHECK(second->left == 517);
        CHECK(second->right == -499);
        CHECK(second->bottom == -521);
        CHECK(second->startMeas == 3);
        CHECK(second->endMeas == 0);
        CHECK(second->distanceToPrev == -481);
        CHECK(reportedFieldCount(report) == 2 * staffSystemFieldCount);

        const auto fieldOrigin = [&](const char* member, musx::dom::Cmper systemId) {
            const auto* field = report.findField<StaffSystem>(member, 0, systemId);
            REQUIRE(field);
            return field->origin;
        };
        CHECK(fieldOrigin("left", 1) == ValueOrigin::LegacyMus);
        CHECK(fieldOrigin("endMeas", 1) == ValueOrigin::LegacyBehavior);
        CHECK(fieldOrigin("top", 1) == ValueOrigin::LegacyMus);
        CHECK(fieldOrigin("horzPercent", 1) == ValueOrigin::LegacyBehavior);
        CHECK(fieldOrigin("ssysPercent", 1) == ValueOrigin::LegacyBehavior);
        CHECK(fieldOrigin("staffHeight", 1) == ValueOrigin::LegacyBehavior);
        CHECK(fieldOrigin("hasStaffScaling", 1) == ValueOrigin::LegacyMus);
        CHECK(fieldOrigin("distanceToPrev", 1) == ValueOrigin::LegacyBehavior);
        CHECK(fieldOrigin("distanceToPrev", 2) == ValueOrigin::LegacyMus);
        CHECK(fieldOrigin("holdMargins", 1) == ValueOrigin::LegacyBehavior);
    }
}

TEST_CASE("Compact staff systems stop at the first start beyond the music", "[class][staff-system]")
{
    for (const auto invalidStart : {0, 3, 7}) {
        const auto document = emptyStaffSystemDocument();
        addStaffSystemTestMeasures(document, 6);
        auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
        profile.byteOrder = ByteOrder::BigEndian;
        const auto report =
            importStaffSystems(makeContainer({{1, "SS", {0, 0, 0, -200, 1, 0}}, {2, "SS", {0, 0, 0, -200, 3, 0}},
                                                 {3, "SS", {0, 0, 0, -200, std::int16_t(invalidStart), 0}}, {4, "SS", {0, 0, 0, -200, 5, 0}}},
                                   FormatEpoch::UncompressedLegacy),
                profile, document);
        const auto first = document->getOthers()->get<StaffSystem>(0, 1);
        const auto second = document->getOthers()->get<StaffSystem>(0, 2);
        REQUIRE(first);
        REQUIRE(second);
        CHECK(second->endMeas == (invalidStart == 7 ? 7 : 0));
        CHECK(bool(document->getOthers()->get<StaffSystem>(0, 3)) == (invalidStart != 7));
        CHECK(bool(document->getOthers()->get<StaffSystem>(0, 4)) == (invalidStart != 7));
        CHECK(reportedFieldCount(report) == (invalidStart == 7 ? 2 : 4) * staffSystemFieldCount);
    }

    const auto document = emptyStaffSystemDocument();
    addStaffSystemTestMeasures(document, 8);
    auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    const auto report = importStaffSystems(
        makeContainer({{1, "SS", {0, 0, 0, -200, 1, 0}}, {3, "SS", {0, 0, 0, -200, 3, 0}}}, FormatEpoch::UncompressedLegacy), profile, document);
    REQUIRE(document->getOthers()->get<StaffSystem>(0, 1));
    CHECK(document->getOthers()->get<StaffSystem>(0, 3));
    CHECK(reportedFieldCount(report) == 2 * staffSystemFieldCount);
    REQUIRE(report.diagnostics.size() == 1);
    CHECK(report.diagnostics.front().level == musx::util::Logger::LogLevel::Warning);
}

TEST_CASE("Expanded staff systems retain interior ranges until a start exceeds the music", "[class][staff-system]")
{
    for (const auto invalidStart : {0, 3, 7}) {
        std::vector<SyntheticRow> rows;
        appendExpandedStaffSystemRows(rows, 1, 1, 4);
        appendExpandedStaffSystemRows(rows, 2, 3, 5);
        appendExpandedStaffSystemRows(rows, 3, std::int16_t(invalidStart), 6);
        appendExpandedStaffSystemRows(rows, 4, 5, 66);

        const auto document = emptyStaffSystemDocument();
        addStaffSystemTestMeasures(document, 6);
        auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
        profile.byteOrder = ByteOrder::BigEndian;
        const auto report = importStaffSystems(makeContainer(rows, FormatEpoch::UncompressedLegacy), profile, document);
        const auto first = document->getOthers()->get<StaffSystem>(0, 1);
        const auto second = document->getOthers()->get<StaffSystem>(0, 2);
        REQUIRE(first);
        REQUIRE(second);
        CHECK(first->endMeas == 4);
        CHECK(second->endMeas == (invalidStart == 7 ? 7 : 5));
        CHECK(bool(document->getOthers()->get<StaffSystem>(0, 3)) == (invalidStart != 7));
        CHECK(bool(document->getOthers()->get<StaffSystem>(0, 4)) == (invalidStart != 7));
        const auto* firstEnd = report.findField<StaffSystem>("endMeas", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        const auto* secondEnd = report.findField<StaffSystem>("endMeas", musx::dom::SCORE_PARTID, musx::dom::Cmper(2));
        REQUIRE(firstEnd);
        REQUIRE(secondEnd);
        CHECK(firstEnd->origin == ValueOrigin::LegacyMus);
        CHECK(secondEnd->origin == (invalidStart == 7 ? ValueOrigin::LegacyMusAdjusted : ValueOrigin::LegacyMus));
        CHECK(firstEnd->rawValue == 4);
        CHECK(secondEnd->rawValue == 5);
        CHECK(reportedFieldCount(report) == (invalidStart == 7 ? 2 : 4) * staffSystemFieldCount);
    }

    std::vector<SyntheticRow> rows;
    appendExpandedStaffSystemRows(rows, 1, 1, 4);
    appendExpandedStaffSystemRows(rows, 3, 3, 5);
    const auto document = emptyStaffSystemDocument();
    addStaffSystemTestMeasures(document, 6);
    auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    const auto report = importStaffSystems(makeContainer(rows, FormatEpoch::UncompressedLegacy), profile, document);
    REQUIRE(document->getOthers()->get<StaffSystem>(0, 1));
    CHECK(document->getOthers()->get<StaffSystem>(0, 3));
    CHECK(reportedFieldCount(report) == 2 * staffSystemFieldCount);
    REQUIRE(report.diagnostics.size() == 1);
    CHECK(report.diagnostics.front().level == musx::util::Logger::LogLevel::Warning);
}

TEST_CASE("System recovery stops before constructing the out-of-range tail in every epoch", "[class][staff-system]")
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            const auto document = emptyStaffSystemDocument();
            addStaffSystemTestMeasures(document, 6);
            auto profile = SourceProfile(epoch);
            profile.byteOrder = order;
            std::vector<SyntheticRow> rows;
            std::vector<SyntheticClassRow> classes;
            const std::array<std::int16_t, 5> starts{1, 4, 7, 0, 2};
            for (std::uint16_t id = 1; id <= starts.size(); ++id) {
                if (epoch == FormatEpoch::CodaBanner) {
                    rows.push_back({id, "SS", {0, 0, 0, 0, starts[id - 1], 0x0080}});
                    rows.push_back({id, "FS", {starts[id - 1], std::int16_t(id == 1 ? 4 : 6), 100, 0, 0, 0}});
                } else if (epoch == FormatEpoch::ZlibLegacy) {
                    auto words = staffSystemWords();
                    words[4] = starts[id - 1];
                    words[6] = id == 1 ? 4 : 6;
                    classes.emplace_back(std::uint16_t(0x00df), words, id);
                } else {
                    appendExpandedStaffSystemRows(rows, id, starts[id - 1], id == 1 ? 4 : 6);
                }
            }
            const auto parsed = epoch == FormatEpoch::ZlibLegacy ? makeClassContainer(classes, order) : makeContainer(rows, epoch, order);
            const auto report = importStaffSystems(parsed, profile, document);
            const auto first = document->getOthers()->get<StaffSystem>(0, 1);
            const auto last = document->getOthers()->get<StaffSystem>(0, 2);
            REQUIRE(first);
            REQUIRE(last);
            CHECK(first->endMeas == 4);
            CHECK(last->startMeas == 4);
            CHECK(last->endMeas == 7);
            for (const auto id : {3, 4, 5}) {
                CHECK_FALSE(document->getOthers()->get<StaffSystem>(0, musx::dom::Cmper(id)));
                CHECK_FALSE(report.findField<StaffSystem>("startMeas", 0, musx::dom::Cmper(id)));
            }
            const auto* end = report.findField<StaffSystem>("endMeas", 0, musx::dom::Cmper(2));
            REQUIRE(end);
            CHECK(end->origin == ValueOrigin::LegacyMusAdjusted);
            CHECK(end->rawValue == 6);
            CHECK(reportedFieldCount(report) == 2 * staffSystemFieldCount);
        }
    }
}

TEST_CASE("Systems stop after covering the final measure even with an in-range tail", "[class][staff-system]")
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            for (const auto storedEnd : {5, 6}) {
                for (const auto tailStart : {0, 2}) {
                    const auto document = emptyStaffSystemDocument();
                    addStaffSystemTestMeasures(document, 4);
                    auto profile = SourceProfile(epoch);
                    profile.byteOrder = order;
                    std::vector<SyntheticRow> rows;
                    std::vector<SyntheticClassRow> classes;
                    for (std::uint16_t id = 1; id <= 3; ++id) {
                        const auto start = std::int16_t(id == 1 ? 1 : id == 2 ? 4 : tailStart);
                        const auto end = std::int16_t(id == 1 ? 4 : id == 2 ? storedEnd : 0);
                        if (epoch == FormatEpoch::CodaBanner) {
                            rows.push_back({id, "SS", {0, 0, 0, 0, start, 0x0080}});
                            rows.push_back({id, "FS", {start, end, 100, 0, 0, 0}});
                        } else if (epoch == FormatEpoch::ZlibLegacy) {
                            auto words = staffSystemWords();
                            words[4] = start;
                            words[6] = end;
                            classes.emplace_back(std::uint16_t(0x00df), words, id);
                        } else {
                            appendExpandedStaffSystemRows(rows, id, start, end);
                        }
                    }
                    const auto parsed = epoch == FormatEpoch::ZlibLegacy ? makeClassContainer(classes, order) : makeContainer(rows, epoch, order);
                    const auto report = importStaffSystems(parsed, profile, document);
                    const auto last = document->getOthers()->get<StaffSystem>(0, 2);
                    REQUIRE(last);
                    CHECK(last->startMeas == 4);
                    CHECK(last->endMeas == 5);
                    CHECK_FALSE(document->getOthers()->get<StaffSystem>(0, 3));
                    CHECK_FALSE(report.findField<StaffSystem>("startMeas", 0, musx::dom::Cmper(3)));
                    const auto* end = report.findField<StaffSystem>("endMeas", 0, musx::dom::Cmper(2));
                    REQUIRE(end);
                    CHECK(end->rawValue == storedEnd);
                    CHECK(end->origin == (storedEnd == 5 ? ValueOrigin::LegacyMus : ValueOrigin::LegacyMusAdjusted));
                }
            }
        }
    }
}

TEST_CASE("Uncalculated final systems keep their end and do not adjust earlier ends", "[class][staff-system]")
{
    for (const auto lastStart : {0, 7}) {
        const auto document = emptyStaffSystemDocument();
        addStaffSystemTestMeasures(document, 6);
        std::vector<SyntheticRow> rows;
        appendExpandedStaffSystemRows(rows, 1, 1, 4);
        appendExpandedStaffSystemRows(rows, 2, 0, 0);
        appendExpandedStaffSystemRows(rows, 3, std::int16_t(lastStart), 0);
        auto profile = SourceProfile(FormatEpoch::DclLegacy);
        profile.byteOrder = ByteOrder::BigEndian;
        const auto report = importStaffSystems(makeContainer(rows, FormatEpoch::DclLegacy), profile, document);
        CHECK(document->getOthers()->get<StaffSystem>(0, 1)->endMeas == 4);
        CHECK(document->getOthers()->get<StaffSystem>(0, 2)->endMeas == 0);
        CHECK(report.findField<StaffSystem>("endMeas", 0, musx::dom::Cmper(2))->origin == ValueOrigin::LegacyMus);
        CHECK(bool(document->getOthers()->get<StaffSystem>(0, 3)) == (lastStart == 0));
    }
}

TEST_CASE("System cutoff is independent for score and part layouts", "[class][staff-system]")
{
    const auto document = emptyStaffSystemDocument();
    addStaffSystemTestMeasures(document, 6);
    std::vector<SyntheticClassRow> rows;
    for (const auto part : std::array<std::uint16_t, 2>{0, 2}) {
        for (std::uint16_t id = 1; id <= 3; ++id) {
            auto words = staffSystemWords();
            words[4] = id == 1 ? 1 : part == 0 ? 7 : id == 2 ? 5 : 9;
            words[6] = id == 1 ? 5 : 9;
            rows.emplace_back(std::uint16_t(0x00df), words, id, part);
        }
    }
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    const auto report = importStaffSystems(makeClassContainer(rows, ByteOrder::LittleEndian), profile, document);
    CHECK(document->getOthers()->get<StaffSystem>(0, 1)->endMeas == 7);
    CHECK_FALSE(document->getOthers()->get<StaffSystem>(0, 2));
    CHECK(document->getOthers()->get<StaffSystem>(2, 2)->endMeas == 7);
    CHECK_FALSE(document->getOthers()->get<StaffSystem>(2, 3));
    CHECK(report.findField<StaffSystem>("endMeas", 2, musx::dom::Cmper(2))->rawValue == 9);
}

TEST_CASE("Coda system scaling rows override the base system options", "[class][staff-system]")
{
    const auto document = emptyStaffSystemDocument();
    auto profile = SourceProfile(FormatEpoch::CodaBanner);
    profile.byteOrder = ByteOrder::BigEndian;
    const auto report = importStaffSystems(
        makeContainer({{1, "SS", {0, 0, 0, -200, 1, 0x0880}}, {2, "SS", {-200, 0, 0, -200, 2, 0x0880}}, {3, "SS", {-200, 0, 0, -200, 3, 0x0880}},
                          {1, "SP", {83, 83, 0, 0, 0, 0}}, {2, "SP", {85, 85, 0, 0, 0, 0x4000}}, {3, "SP", {87, 87, 0, 0, 0, 0x2000}}},
            FormatEpoch::CodaBanner),
        profile, document);

    const auto first = document->getOthers()->get<StaffSystem>(0, 1);
    const auto second = document->getOthers()->get<StaffSystem>(0, 2);
    const auto third = document->getOthers()->get<StaffSystem>(0, 3);
    REQUIRE(first);
    REQUIRE(second);
    REQUIRE(third);
    CHECK(first->ssysPercent == 83);
    CHECK_FALSE(first->holdMargins);
    CHECK_FALSE(first->scaleVert);
    CHECK(second->ssysPercent == 85);
    CHECK(second->holdMargins);
    CHECK_FALSE(second->scaleVert);
    CHECK(third->ssysPercent == 87);
    CHECK_FALSE(third->holdMargins);
    CHECK(third->scaleVert);

    for (const auto systemId : {musx::dom::Cmper(1), musx::dom::Cmper(2), musx::dom::Cmper(3)}) {
        for (const auto* member : {"ssysPercent", "holdMargins", "scaleVert"}) {
            const auto* field = report.findField<StaffSystem>(member, 0, systemId);
            REQUIRE(field);
            CHECK(field->origin == ValueOrigin::LegacyMus);
        }
    }
    CHECK(report.findField<StaffSystem>("ssysPercent", musx::dom::SCORE_PARTID, musx::dom::Cmper(1))->rawValue == 83);
    CHECK(reportedFieldCount(report) == 3 * staffSystemFieldCount);
}

TEST_CASE("Compact layout records supply ranges and horizontal scale only when active", "[class][staff-system]")
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy}) {
        for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            for (const bool modern : {false, true}) {
                const auto document = emptyStaffSystemDocument();
                auto profile = SourceProfile(epoch);
                profile.byteOrder = order;
                const auto tag = modern ? "fS" : "FS";
                const auto layout = modern ? std::array<std::int16_t, 6>{4, 8, 17, -1, -12345, 0} : std::array<std::int16_t, 6>{4, 8, -123, 17, 0, 0};
                const auto report =
                    importStaffSystems(makeContainer({{3, "SS", {-20, 0, 0, 0, 4, 0x2080}}, {3, tag, layout}, {7, "SS", {0, 0, 0, 0, 0, 0}},
                                                         {7, tag, layout}, {9, "SS", {0, 0, 0, 0, 5, 0x0880}}, {9, "SP", {0, 77, 0, 0, 0, 0x2000}}},
                                           epoch, order),
                        profile, document);
                const auto active = document->getOthers()->get<StaffSystem>(0, 3);
                REQUIRE(active);
                CHECK(active->startMeas == 4);
                CHECK(active->endMeas == 8);
                CHECK(active->horzPercent == (modern ? -123.45 : -123.0));
                CHECK(active->noNames);
                CHECK(report.findField<StaffSystem>("endMeas", 0, musx::dom::Cmper(3))->origin == ValueOrigin::LegacyMus);
                const auto inactive = document->getOthers()->get<StaffSystem>(0, 7);
                REQUIRE(inactive);
                CHECK(inactive->endMeas == 0);
                CHECK(inactive->horzPercent == 0);
                const auto missing = document->getOthers()->get<StaffSystem>(0, 9);
                REQUIRE(missing);
                CHECK(missing->endMeas == 0);
                CHECK(missing->ssysPercent == 100);
                CHECK(missing->holdMargins);
                CHECK_FALSE(missing->scaleVert);
                CHECK(reportedFieldCount(report) == 3 * staffSystemFieldCount);
            }
        }
    }
}

TEST_CASE("Expanded system ranges retain score and part ownership", "[class][staff-system]")
{
    for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        const auto document = emptyStaffSystemDocument();
        auto score = staffSystemWords();
        score[4] = 0;
        score[6] = 31;
        auto part = score;
        part[4] = 20;
        part[6] = 25;
        auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
        profile.byteOrder = order;
        const auto report = importStaffSystems(
            makeClassContainer({SyntheticClassRow{0x00df, score, 8}, SyntheticClassRow{0x00df, part, 8, 2}}, order), profile, document);
        const auto scoreSystem = document->getOthers()->get<StaffSystem>(0, 8);
        const auto partSystem = document->getOthers()->get<StaffSystem>(2, 8);
        REQUIRE(scoreSystem);
        REQUIRE(partSystem);
        CHECK(scoreSystem->startMeas == 0);
        CHECK(scoreSystem->endMeas == 31);
        CHECK(partSystem->startMeas == 20);
        CHECK(partSystem->endMeas == 25);
        CHECK(report.findField<StaffSystem>("endMeas", 2, musx::dom::Cmper(8))->rawValue == 25);
    }
}

TEST_CASE("Staff height conversion stops at the Finale 2002 beta boundary", "[class][staff-system]")
{
    for (const auto build : {2, 3}) {
        const auto document = emptyStaffSystemDocument();
        auto profile = SourceProfile(FormatEpoch::DclLegacy);
        profile.byteOrder = ByteOrder::BigEndian;
        profile.version = finale_mus_reader::SourceVersion{.major = 7, .devStatus = 2, .build = std::uint8_t(build)};
        const auto words = staffSystemWords(0, 1000, false);
        const auto report = importStaffSystems(makeContainer({{1, "SS", {words[0], words[1], words[2], words[3], words[4], words[5]}},
                                                                 {1, "SS", {words[6], words[7], words[8], words[9], words[10], words[11]}}},
                                                   FormatEpoch::DclLegacy),
            profile, document);
        const auto system = document->getOthers()->get<StaffSystem>(0, 1);
        REQUIRE(system);
        CHECK(system->staffHeight == (build == 2 ? 6144 : 4000));
        CHECK(report.findField<StaffSystem>("staffHeight", 0, musx::dom::Cmper(1))->origin
              == (build == 2 ? ValueOrigin::LegacyBehavior : ValueOrigin::LegacyMus));
    }
}

TEST_CASE("Part system masks inherit stored score boundaries", "[class][staff-system]")
{
    const auto document = emptyStaffSystemDocument();
    auto score = staffSystemWords();
    auto part = score;
    part[4] = 27;
    part[6] = 30;
    part[1] = 42;
    std::vector<std::uint16_t> mask(score.size(), 0);
    mask[1] = 0xffff;
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    const auto report = importStaffSystems(
        makeClassContainer({SyntheticClassRow{0x00df, score, 1}, SyntheticClassRow{0x00df, part, 1, 2, true, mask}}, ByteOrder::LittleEndian),
        profile, document);
    const auto system = document->getOthers()->get<StaffSystem>(2, 1);
    REQUIRE(system);
    CHECK(system->left == 42);
    CHECK(system->startMeas == 4);
    CHECK(system->endMeas == 9);
    CHECK(report.findField<StaffSystem>("endMeas", 2, musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("StaffSystem recovers the stored staff-scaling flag", "[class][staff-system][reporting]")
{
    const auto document = emptyStaffSystemDocument();
    addStaffSystemTestMeasures(document, 8);
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    const auto report = importStaffSystems(makeClassContainer(0x00df, staffSystemWords(0x2417), ByteOrder::LittleEndian, 1), profile, document);
    const auto system = document->getOthers()->get<StaffSystem>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(system);
    CHECK(system->hasStaffScaling);
    const auto* scaling = report.findField<StaffSystem>("hasStaffScaling", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(scaling);
    CHECK(scaling->origin == ValueOrigin::LegacyMus);
    CHECK(scaling->rawValue == 1);
}

TEST_CASE("Controlled fixtures recover staff systems across supported physical "
          "layouts",
    "[class][staff-system]")
{
    struct FixtureCase
    {
        const char* path;
        int top;
        int left;
        int bottom;
        int endMeas;
        double horzPercent;
    };
    for (const auto& expected : {FixtureCase{"evidence/F97/Fin97-baseline.mus", -260, 288, -144, 2, 111.99},
             FixtureCase{"evidence/F98/F98-baseline.mus", -630, 288, -200, 4, 110.94},
             FixtureCase{"evidence/F2000/F2000-update-layout.mus", -80, 0, -200, 2, 331.5},
             FixtureCase{"evidence/F2006/F2006-empty.mus", -463, 144, -200, 2, 0.0},
             FixtureCase{"evidence/F2007/F2007-lyric-hyphens.mus", -463, 144, -200, 3, 153.25},
             FixtureCase{"evidence/F2012/F2012-baseline.mus", -463, 144, -200, 2, 307.17}}) {
        const auto result = readFixture(expected.path);
        const auto system = result.document->getOthers()->get<StaffSystem>(0, 1);
        REQUIRE(system);
        CHECK(system->top == expected.top);
        CHECK(system->left == expected.left);
        CHECK(system->right == 0);
        CHECK(system->bottom == expected.bottom);
        CHECK(system->startMeas == 1);
        CHECK(system->endMeas == expected.endMeas);
        CHECK(system->horzPercent == expected.horzPercent);
        CHECK(system->ssysPercent == 100);
        CHECK(system->staffHeight == 6144);
    }

    const auto finale97MultipleSystems = readFixture("evidence/F97/F97-altnotation.mus");
    const auto laterSystem = finale97MultipleSystems.document->getOthers()->get<StaffSystem>(0, 2);
    REQUIRE(laterSystem);
    CHECK(laterSystem->top == -188);
    CHECK(laterSystem->distanceToPrev == -72);

    const auto finale372Compact = readFixture("evidence/F372/F372-fileinfo-text.mus");
    const auto finale372System = finale372Compact.document->getOthers()->get<StaffSystem>(0, 1);
    REQUIRE(finale372System);
    CHECK(finale372System->top == -80);
    CHECK(finale372System->left == 0);
    CHECK(finale372System->right == 0);
    CHECK(finale372System->bottom == -200);
    CHECK(finale372System->startMeas == 1);
    CHECK(finale372System->endMeas == 2);
    CHECK(finale372System->distanceToPrev == 0);
    CHECK(finale372System->ssysPercent == 100);
    CHECK(finale372System->staffHeight == 6144);
    CHECK(finale372System->holdMargins);

    const auto finale372Cruft = readFixture("evidence/F372/F372-measure-graphic.mus");
    const auto finale372Last = finale372Cruft.document->getOthers()->get<StaffSystem>(0, 3);
    REQUIRE(finale372Last);
    CHECK(finale372Last->endMeas == 8);
    CHECK_FALSE(finale372Cruft.document->getOthers()->get<StaffSystem>(0, 4));

    const auto finale97Cruft = readFixture("evidence/F97/Fin97-baseline.mus");
    REQUIRE(finale97Cruft.document->getOthers()->get<StaffSystem>(0, 1));
    CHECK_FALSE(finale97Cruft.document->getOthers()->get<StaffSystem>(0, 2));
    CHECK_FALSE(finale97Cruft.document->getOthers()->get<StaffSystem>(0, 3));

    const auto finale98Cruft = readFixture("evidence/F98/F98-altnotation.mus");
    REQUIRE(finale98Cruft.document->getOthers()->get<StaffSystem>(0, 3));
    CHECK_FALSE(finale98Cruft.document->getOthers()->get<StaffSystem>(0, 4));
    CHECK_FALSE(finale98Cruft.document->getOthers()->get<StaffSystem>(0, 15));

    struct CodaFixtureCase
    {
        const char* path;
        int firstLeft;
        int firstRight;
        int firstBottom;
        int firstEndMeas;
        int fifthLeft;
        int fifthRight;
        int fifthBottom;
        int fifthStartMeas;
        int fifthEndMeas;
        int fifthDistance;
        int firstTop;
        int fifthTop;
        double fifthHorzPercent;
    };
    for (const auto& expected :
        {CodaFixtureCase{"evidence/F100/F100-chg-sys.mus", 389, -247, -299, 2, 517, -499, -521, 11, 12, -481, -337, -80, 154.0},
            CodaFixtureCase{"evidence/F263/F263-chg-sys.mus", 173, -420, -207, 4, 641, -34, -244, 16, 19, -151, -349, -188, 107.0}}) {
        const auto result = readFixture(expected.path);
        const auto first = result.document->getOthers()->get<StaffSystem>(0, 1);
        const auto fifth = result.document->getOthers()->get<StaffSystem>(0, 5);
        REQUIRE(first);
        REQUIRE(fifth);
        CHECK(first->top == expected.firstTop);
        CHECK(first->left == expected.firstLeft);
        CHECK(first->right == expected.firstRight);
        CHECK(first->bottom == expected.firstBottom);
        CHECK(first->startMeas == 1);
        CHECK(first->endMeas == expected.firstEndMeas);
        CHECK(first->distanceToPrev == 0);
        CHECK(first->holdMargins);
        CHECK(fifth->top == expected.fifthTop);
        CHECK(fifth->left == expected.fifthLeft);
        CHECK(fifth->right == expected.fifthRight);
        CHECK(fifth->bottom == expected.fifthBottom);
        CHECK(fifth->startMeas == expected.fifthStartMeas);
        CHECK(fifth->endMeas == expected.fifthEndMeas);
        CHECK(fifth->distanceToPrev == expected.fifthDistance);
        CHECK(fifth->horzPercent == expected.fifthHorzPercent);
        CHECK(fifth->ssysPercent == 100);
        CHECK(fifth->staffHeight == 6144);
        CHECK(fifth->holdMargins);
    }

    const auto codaSystemOptions = readFixture("evidence/F263/F263-sysopts.mus");
    const auto first = codaSystemOptions.document->getOthers()->get<StaffSystem>(0, 1);
    const auto second = codaSystemOptions.document->getOthers()->get<StaffSystem>(0, 2);
    const auto third = codaSystemOptions.document->getOthers()->get<StaffSystem>(0, 3);
    REQUIRE(first);
    REQUIRE(second);
    REQUIRE(third);
    CHECK(first->top == -188);
    CHECK(second->top == -188);
    CHECK(third->top == -164);
    CHECK(first->ssysPercent == 83);
    CHECK_FALSE(first->holdMargins);
    CHECK_FALSE(first->scaleVert);
    CHECK(second->ssysPercent == 85);
    CHECK(second->holdMargins);
    CHECK_FALSE(second->scaleVert);
    CHECK(third->ssysPercent == 87);
    CHECK_FALSE(third->holdMargins);
    CHECK(third->scaleVert);
}

TEST_CASE("Early controlled fixtures recover compact StaffSystem layout and scaling flag", "[class][staff-system]")
{
    struct FixtureCase
    {
        const char* baseline;
        const char* scaled;
        std::array<musx::dom::MeasCmper, 4> starts;
    };
    for (const auto& expected : {
             FixtureCase{"evidence/F100/F100-4systems.mus", "evidence/F100/F100-4systems-sys2to73.mus", {1, 3, 6, 9}},
             FixtureCase{"evidence/F372/F372-4systems.mus", "evidence/F372/F372-4systems-sys2to71.mus", {1, 4, 7, 10}},
         }) {
        const auto baseline = readFixture(expected.baseline);
        const auto scaled = readFixture(expected.scaled);
        for (musx::dom::Cmper systemId = 1; systemId <= 4; ++systemId) {
            const auto baselineSystem = baseline.document->getOthers()->get<StaffSystem>(0, systemId);
            const auto scaledSystem = scaled.document->getOthers()->get<StaffSystem>(0, systemId);
            REQUIRE(baselineSystem);
            REQUIRE(scaledSystem);
            CHECK(baselineSystem->startMeas == expected.starts[systemId - 1]);
            CHECK(scaledSystem->startMeas == expected.starts[systemId - 1]);
            CHECK_FALSE(baselineSystem->hasStaffScaling);
            CHECK(scaledSystem->hasStaffScaling == (systemId == 2));
            if (systemId < 4) {
                CHECK(baselineSystem->endMeas == expected.starts[systemId]);
                CHECK(scaledSystem->endMeas == expected.starts[systemId]);
            }
        }
        CHECK_FALSE(baseline.document->getOthers()->get<StaffSystem>(0, 5));
        CHECK_FALSE(scaled.document->getOthers()->get<StaffSystem>(0, 5));

        const auto* baselineFlag = baseline.report.findField<StaffSystem>("hasStaffScaling", musx::dom::SCORE_PARTID, musx::dom::Cmper(2));
        const auto* scaledFlag = scaled.report.findField<StaffSystem>("hasStaffScaling", musx::dom::SCORE_PARTID, musx::dom::Cmper(2));
        REQUIRE(baselineFlag);
        REQUIRE(scaledFlag);
        CHECK(baselineFlag->origin == ValueOrigin::LegacyMus);
        CHECK(scaledFlag->origin == ValueOrigin::LegacyMus);
        CHECK(baselineFlag->rawValue == 0);
        CHECK(scaledFlag->rawValue == 1);
    }
}
}  // namespace
}  // namespace finale_mus_reader_tests
