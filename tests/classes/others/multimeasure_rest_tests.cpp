// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

using MmRest = musx::dom::others::MultimeasureRest;
using MmRestOptions = musx::dom::options::MultimeasureRestOptions;

constexpr musx::dom::Evpu optionsSymbolSpacing = 37;

/// @brief A bare document carrying multimeasure-rest options whose symbol spacing is recognizable.
musx::dom::DocumentPtr mmRestDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    auto document = session.getDocument();
    auto options = std::make_shared<MmRestOptions>(document);
    options->symSpacing = optionsSymbolSpacing;
    document->getOptions()->add(MmRestOptions::XmlNodeName, options);
    return document;
}

/// @brief Runs the importer alone, including the deferred phase that follows every importer.
ImportReport importMmRests(const finale_mus_reader::container::ParsedContainer& parsed, FormatEpoch epoch, const musx::dom::DocumentPtr& document)
{
    SourceProfile profile(epoch);
    profile.byteOrder = parsed.byteOrder;
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importMultimeasureRests(context);
    finale_mus_reader::runDeferredChecks(pending);
    return report;
}

const FieldInfo* mmRestField(
    const ImportReport& report, const char* member, musx::dom::Cmper cmper, musx::dom::Cmper partId = musx::dom::SCORE_PARTID)
{
    return report.findField<MmRest>(member, partId, cmper);
}

TEST_CASE("Two-row multimeasure rest records recover every legacy field", "[class][mmrest]")
{
    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            const auto parsed = makeContainer({{5, "XI", {216, 9, -28, 1, 2, 9}}, {5, "XI", {36, -24, 12, -48, 0, 1}}}, epoch, byteOrder);
            const auto document = mmRestDocument();
            const auto report = importMmRests(parsed, epoch, document);
            const auto rest = document->getOthers()->get<MmRest>(musx::dom::SCORE_PARTID, 5);
            REQUIRE(rest);
            CHECK(rest->measWidth == 216);
            CHECK(rest->nextMeas == 9);
            CHECK(rest->numVertAdj == -28);
            CHECK(rest->shapeDef == 1);
            CHECK(rest->numStart == 2);
            CHECK(rest->symbolThreshold == 9);
            CHECK(rest->symbolSpacing == 36);
            CHECK(rest->numHorzAdj == -24);
            CHECK(rest->shapeStartAdjust == 12);
            CHECK(rest->shapeEndAdjust == -48);
            CHECK(rest->useSymbols);
            CHECK_FALSE(rest->noHorizontalStretch);
            CHECK(rest->calcNumberOfMeasures() == 4);
            for (const auto* member : {"measWidth", "nextMeas", "numVertAdj", "shapeDef", "numStart", "symbolThreshold", "symbolSpacing",
                     "numHorzAdj", "shapeStartAdjust", "shapeEndAdjust", "useSymbols"}) {
                const auto* field = mmRestField(report, member, 5);
                REQUIRE(field);
                CHECK(field->origin == ValueOrigin::LegacyMus);
                CHECK(field->sourceIdentity == finale_mus_reader::records::packTag("XI"));
            }
            const auto* stretch = mmRestField(report, "noHorizontalStretch", 5);
            REQUIRE(stretch);
            CHECK(stretch->origin == ValueOrigin::MusxOnly);
            CHECK(stretch->rawValue == 0);
            CHECK(reportedFieldCount(report) == MmRest::xmlMappingArray().size());
        }
    }
}

TEST_CASE("Only bit 0 of the multimeasure rest flags word selects symbols", "[class][mmrest]")
{
    const auto parsed = makeContainer({{5, "XI", {216, 9, -28, 1, 2, 9}}, {5, "XI", {48, 0, 0, 0, 0, static_cast<std::int16_t>(0xfffe)}}});
    const auto document = mmRestDocument();
    importMmRests(parsed, FormatEpoch::UncompressedLegacy, document);
    const auto rest = document->getOthers()->get<MmRest>(musx::dom::SCORE_PARTID, 5);
    REQUIRE(rest);
    CHECK_FALSE(rest->useSymbols);
    CHECK_FALSE(rest->noHorizontalStretch);
}

TEST_CASE("A one-row multimeasure rest record takes era behavior for the second row", "[class][mmrest]")
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy}) {
        const auto parsed = makeContainer({{3, "XI", {320, 7, -28, 19, 0, 0}}}, epoch);
        const auto document = mmRestDocument();
        const auto report = importMmRests(parsed, epoch, document);
        const auto rest = document->getOthers()->get<MmRest>(musx::dom::SCORE_PARTID, 3);
        REQUIRE(rest);
        CHECK(rest->measWidth == 320);
        CHECK(rest->nextMeas == 7);
        CHECK(rest->numVertAdj == -28);
        CHECK(rest->shapeDef == 19);
        CHECK(rest->numStart == 0);
        CHECK(rest->symbolThreshold == 0);
        CHECK(rest->symbolSpacing == optionsSymbolSpacing);
        CHECK(rest->numHorzAdj == 0);
        CHECK(rest->shapeStartAdjust == 0);
        CHECK(rest->shapeEndAdjust == 0);
        CHECK_FALSE(rest->useSymbols);
        for (const auto* member : {"measWidth", "nextMeas", "numVertAdj", "shapeDef", "numStart", "symbolThreshold"}) {
            const auto* field = mmRestField(report, member, 3);
            REQUIRE(field);
            CHECK(field->origin == ValueOrigin::LegacyMus);
        }
        for (const auto* member : {"symbolSpacing", "numHorzAdj", "shapeStartAdjust", "shapeEndAdjust", "useSymbols"}) {
            const auto* field = mmRestField(report, member, 3);
            REQUIRE(field);
            CHECK(field->origin == ValueOrigin::LegacyBehavior);
        }
        const auto* spacing = mmRestField(report, "symbolSpacing", 3);
        REQUIRE(spacing);
        CHECK(spacing->rawValue == optionsSymbolSpacing);
        CHECK(reportedFieldCount(report) == MmRest::xmlMappingArray().size());
    }
}

TEST_CASE("One-row and two-row multimeasure rest records coexist in one document", "[class][mmrest]")
{
    const auto parsed = makeContainer({
        {3, "XI", {320, 7, -28, 1, 0, 0}},
        {12, "XI", {216, 20, -28, 1, 2, 9}},
        {12, "XI", {24, 0, 0, 0, 0, 1}},
    });
    const auto document = mmRestDocument();
    const auto report = importMmRests(parsed, FormatEpoch::UncompressedLegacy, document);
    const auto early = document->getOthers()->get<MmRest>(musx::dom::SCORE_PARTID, 3);
    const auto later = document->getOthers()->get<MmRest>(musx::dom::SCORE_PARTID, 12);
    REQUIRE(early);
    REQUIRE(later);
    CHECK(early->symbolSpacing == optionsSymbolSpacing);
    CHECK_FALSE(early->useSymbols);
    CHECK(later->symbolSpacing == 24);
    CHECK(later->useSymbols);
    const auto* earlySpacing = mmRestField(report, "symbolSpacing", 3);
    const auto* laterSpacing = mmRestField(report, "symbolSpacing", 12);
    REQUIRE(earlySpacing);
    REQUIRE(laterSpacing);
    CHECK(earlySpacing->origin == ValueOrigin::LegacyBehavior);
    CHECK(laterSpacing->origin == ValueOrigin::LegacyMus);
    CHECK(reportedFieldCount(report) == 2 * MmRest::xmlMappingArray().size());
}

TEST_CASE("Multimeasure rest class records recover score and part rests", "[class][mmrest]")
{
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        const auto parsed = makeClassContainer(
            {
                SyntheticClassRow{0x00af, {216, 64, -28, 1, 2, 9, 48, 0, 0, 0, 0, 1}, 1, 0},
                SyntheticClassRow{0x00af, {180, 64, -30, 123, 3, 8, 30, 6, -6, 12, 0, 0}, 1, 2},
            },
            byteOrder);
        const auto document = mmRestDocument();
        const auto report = importMmRests(parsed, FormatEpoch::ZlibLegacy, document);
        const auto score = document->getOthers()->get<MmRest>(musx::dom::SCORE_PARTID, 1);
        REQUIRE(score);
        CHECK(score->measWidth == 216);
        CHECK(score->useSymbols);
        const auto part = document->getOthers()->get<MmRest>(2, 1);
        REQUIRE(part);
        CHECK(part->getSourcePartId() == 2);
        CHECK(part->getShareMode() == musx::dom::EnigmaBase::ShareMode::None);
        CHECK(part->measWidth == 180);
        CHECK(part->nextMeas == 64);
        CHECK(part->numVertAdj == -30);
        CHECK(part->shapeDef == 123);
        CHECK(part->numStart == 3);
        CHECK(part->symbolThreshold == 8);
        CHECK(part->symbolSpacing == 30);
        CHECK(part->numHorzAdj == 6);
        CHECK(part->shapeStartAdjust == -6);
        CHECK(part->shapeEndAdjust == 12);
        CHECK_FALSE(part->useSymbols);
        const auto* field = mmRestField(report, "shapeEndAdjust", 1, 2);
        REQUIRE(field);
        CHECK(field->origin == ValueOrigin::LegacyMus);
        CHECK(field->sourceIdentity == 0x00af);
        CHECK(reportedFieldCount(report) == 2 * MmRest::xmlMappingArray().size());
    }
}

TEST_CASE("Documents without multimeasure rest records have no rest objects", "[class][mmrest]")
{
    const auto parsed = makeContainer({{2, "FM", {6, 0, 0, 0, 0, 0}}});
    const auto document = mmRestDocument();
    const auto report = importMmRests(parsed, FormatEpoch::UncompressedLegacy, document);
    CHECK(document->getOthers()->getAllSources<MmRest>().empty());
    CHECK(reportedFieldCount(report) == 0);
}

TEST_CASE("Finale 1.0 multimeasure rests recover their one-row records", "[class][mmrest]")
{
    const auto without = readFixture("evidence/F100/F100-nommrests.mus");
    CHECK(without.document->getOthers()->getAllSources<MmRest>().empty());

    const auto result = readFixture("evidence/F100/F100-mmrests.mus");
    const auto rests = result.document->getOthers()->getAllSources<MmRest>();
    REQUIRE(rests.size() == 3);
    for (const auto& [start, next] : {std::pair{1, 4}, std::pair{5, 8}, std::pair{9, 12}}) {
        const auto rest = result.document->getOthers()->get<MmRest>(musx::dom::SCORE_PARTID, musx::dom::Cmper(start));
        REQUIRE(rest);
        CHECK(rest->measWidth == 360);
        CHECK(rest->nextMeas == next);
        CHECK(rest->numVertAdj == 12);
        CHECK(rest->shapeDef == 1);
        CHECK(rest->numStart == 0);
        CHECK(rest->symbolThreshold == 0);
        CHECK(rest->numHorzAdj == 0);
        CHECK(rest->shapeStartAdjust == 0);
        CHECK(rest->shapeEndAdjust == 0);
        CHECK_FALSE(rest->useSymbols);
        CHECK_FALSE(rest->noHorizontalStretch);
        const auto* nextField = result.report.findField<MmRest>("nextMeas", musx::dom::SCORE_PARTID, musx::dom::Cmper(start));
        REQUIRE(nextField);
        CHECK(nextField->origin == ValueOrigin::LegacyMus);
        CHECK(nextField->rawValue == next);
        CHECK(nextField->sourceIdentity == finale_mus_reader::records::packTag("XI"));
        const auto* spacing = result.report.findField<MmRest>("symbolSpacing", musx::dom::SCORE_PARTID, musx::dom::Cmper(start));
        REQUIRE(spacing);
        CHECK(spacing->origin == ValueOrigin::LegacyBehavior);
        CHECK(rest->symbolSpacing == result.document->getOptions()->get<MmRestOptions>()->symSpacing);
    }
}

TEST_CASE("Finale 3.0 multimeasure rests store only the first row", "[class][mmrest]")
{
    const auto without = readFixture("evidence/F300/F300-nommrest.mus");
    CHECK(without.document->getOthers()->getAllSources<MmRest>().empty());

    const auto result = readFixture("evidence/F300/F300-mmrest.mus");
    REQUIRE(result.document->getOthers()->getAllSources<MmRest>().size() == 3);
    for (const auto& [start, next] : {std::pair{1, 4}, std::pair{5, 8}, std::pair{9, 12}}) {
        const auto rest = result.document->getOthers()->get<MmRest>(musx::dom::SCORE_PARTID, musx::dom::Cmper(start));
        REQUIRE(rest);
        CHECK(rest->measWidth == 361);
        CHECK(rest->nextMeas == next);
        CHECK(rest->numVertAdj == 7);
        CHECK(rest->shapeDef == 1);
        CHECK_FALSE(rest->useSymbols);
        const auto* spacing = result.report.findField<MmRest>("symbolSpacing", musx::dom::SCORE_PARTID, musx::dom::Cmper(start));
        REQUIRE(spacing);
        CHECK(spacing->origin == ValueOrigin::LegacyBehavior);
    }
}

TEST_CASE("Finale 3.7 multimeasure rests recover their stored second row", "[class][mmrest]")
{
    const auto without = readFixture("evidence/F372/F372-nommrest.mus");
    CHECK(without.document->getOthers()->getAllSources<MmRest>().empty());

    const auto result = readFixture("evidence/F372/F372-mmrest-syms.mus");
    REQUIRE(result.document->getOthers()->getAllSources<MmRest>().size() == 3);
    for (const auto& [start, next] : {std::pair{1, 4}, std::pair{5, 8}, std::pair{9, 12}}) {
        const auto rest = result.document->getOthers()->get<MmRest>(musx::dom::SCORE_PARTID, musx::dom::Cmper(start));
        REQUIRE(rest);
        CHECK(rest->measWidth == 369);
        CHECK(rest->nextMeas == next);
        CHECK(rest->numVertAdj == -7);
        CHECK(rest->shapeDef == 1);
        CHECK(rest->numStart == 3);
        CHECK(rest->symbolThreshold == 11);
        CHECK(rest->symbolSpacing == 47);
        CHECK(rest->numHorzAdj == 5);
        CHECK(rest->shapeStartAdjust == 21);
        CHECK(rest->shapeEndAdjust == -29);
        CHECK(rest->useSymbols);
        CHECK(rest->calcUsesSymbols());
        for (const auto* member : {"symbolSpacing", "numHorzAdj", "shapeStartAdjust", "shapeEndAdjust", "useSymbols"}) {
            const auto* field = result.report.findField<MmRest>(member, musx::dom::SCORE_PARTID, musx::dom::Cmper(start));
            REQUIRE(field);
            CHECK(field->origin == ValueOrigin::LegacyMus);
        }
    }
}

} // namespace
} // namespace finale_mus_reader_tests
