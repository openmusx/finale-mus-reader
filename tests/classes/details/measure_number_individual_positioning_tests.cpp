// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT
#include "class_test_support.h"
#include <array>
#include <set>

namespace finale_mus_reader_tests {
namespace {
using namespace classes;
using IndividualNumber = musx::dom::details::MeasureNumberIndividualPositioning;

ImportReport importIndividualNumbers(const finale_mus_reader::container::ParsedContainer& parsed, const musx::dom::DocumentPtr& document)
{
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::details::importMeasureNumberIndividualPositioning(context);
    finale_mus_reader::runDeferredChecks(pending);
    return report;
}

std::shared_ptr<musx::dom::others::MeasureNumberRegion> addPositionRegion(const musx::dom::DocumentPtr& document, musx::dom::Cmper cmper,
    musx::dom::Cmper region, musx::dom::MeasCmper start = 1, musx::dom::MeasCmper end = 10)
{
    using Region = musx::dom::others::MeasureNumberRegion;
    auto value = std::make_shared<Region>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, cmper);
    value->region = region;
    value->startMeas = start;
    value->endMeas = end;
    value->scoreData = std::make_shared<Region::ScorePartData>(value);
    document->getOthers()->add(Region::XmlNodeName, value);
    return value;
}

void addPositionMeasureAndStaves(const musx::dom::DocumentPtr& document, musx::dom::Cmper measure)
{
    using namespace musx::dom;
    document->getOthers()->add(
        others::Measure::XmlNodeName, std::make_shared<others::Measure>(document, musx::dom::SCORE_PARTID, EnigmaBase::ShareMode::All, measure));
    for (Inci inci = 0; inci < 2; ++inci) {
        auto staff = std::make_shared<others::StaffUsed>(document, musx::dom::SCORE_PARTID, EnigmaBase::ShareMode::All, BASE_SYSTEM_ID, inci);
        staff->staffId = inci + 3;
        document->getOthers()->add(others::StaffUsed::XmlNodeName, staff);
    }
}

TEST_CASE("Early individual numbers select the first region and convert measure visibility", "[class][measure-number-position]")
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy}) {
        for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            auto session = musx::factory::DocumentFactory::begin();
            const auto document = session.getDocument();
            addPositionRegion(document, 1, 17, 1, 5)->time = true;
            addPositionRegion(document, 2, 9, 1, 10);
            addPositionMeasureAndStaves(document, 2);
            const auto parsed = makeContainer({{2, "MI", {3, 99, -101, 24, -35, 0}}, {2, "MI", {4, 99, -21, -32, 43, 0}},
                                                  {3, "MI", {3, 0, 1, 2, 3, 0}}, {4, "MI", {3, 0, 4, 5, 6, 0}}, {10, "MI", {3, 0, 9, 9, 9, 0}},
                                                  {2, "MS", {0, 0, 0, 0, 0x0a00, 0}}, {3, "MS", {0, 0, 0, 0, 0x0800, 0}}},
                epoch, order);
            const auto report = importIndividualNumbers(parsed, document);
            const auto values = document->getDetails()->getAllSources<IndividualNumber>();
            REQUIRE(values.size() == 4);
            const auto value = document->getDetails()->get<IndividualNumber>(0, 3, 2, musx::dom::Inci{0});
            REQUIRE(value);
            CHECK(value->measNumRegion == 17);
            CHECK(value->xOffset == -101);
            CHECK(value->yOffset == 24);
            CHECK(value->xOffset2 == -35);
            CHECK(value->forceVisibility == IndividualNumber::ForceVisibility::Hide);
            CHECK_FALSE(value->useEnclosure);
            CHECK(document->getDetails()->get<IndividualNumber>(0, 3, 3, musx::dom::Inci{0})->forceVisibility
                  == IndividualNumber::ForceVisibility::Show);
            CHECK(document->getDetails()->get<IndividualNumber>(0, 3, 4, musx::dom::Inci{0})->forceVisibility
                  == IndividualNumber::ForceVisibility::None);
            CHECK(document->getOthers()->get<musx::dom::others::Measure>(0, 2)->hasMeasNumbIndivPos);
            const auto& fields =
                report.fields.at(finale_mus_reader::instanceKey<IndividualNumber>(0, musx::dom::Cmper(3), musx::dom::Inci{0}, musx::dom::Cmper(2)));
            CHECK(fields.size() == 19);
            CHECK(fields.at("xOffset").rawValue == -101);
            CHECK(fields.at("forceVisibility").rawValue == 0x0a00);
            CHECK(fields.at("forceVisibility").origin == ValueOrigin::LegacyMusAdjusted);
            CHECK(fields.at("enclosure.shape").origin == ValueOrigin::LegacyBehavior);
        }
    }
}

TEST_CASE("Early measure enclosures populate every region and staff while retaining offsets", "[class][measure-number-position]")
{
    for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        addPositionRegion(document, 1, 17);
        addPositionRegion(document, 2, 9);
        addPositionRegion(document, 3, 5, 3, 10);
        addPositionMeasureAndStaves(document, 2);
        const auto parsed = makeContainer(
            {{2, "MI", {3, 0, -13, 24, -35, 0}}, {2, "Me", {-4, 5, 6, 7, 128, static_cast<std::int16_t>(0xd803)}}, {3, "Me", {1, 2, 3, 4, 5, 2}}},
            FormatEpoch::UncompressedLegacy, order);
        const auto report = importIndividualNumbers(parsed, document);
        const auto values = document->getDetails()->getAllSources<IndividualNumber>();
        REQUIRE(values.size() == 4);
        for (const auto& value : values) {
            CHECK(value->getCmper2() == 2);
            CHECK(value->useEnclosure);
            REQUIRE(value->enclosure);
            CHECK(value->enclosure->xAdd == -4);
            CHECK(value->enclosure->yAdd == 5);
            CHECK(value->enclosure->xMargin == 6);
            CHECK(value->enclosure->yMargin == 7);
            CHECK(value->enclosure->lineWidth == 128);
            CHECK(static_cast<int>(value->enclosure->shape) == 3);
            CHECK(value->enclosure->fixedSize);
            CHECK(value->enclosure->notTall);
            CHECK(value->enclosure->opaque);
            CHECK(value->enclosure->equalAspect);
            const auto& fields =
                report.fields.at(finale_mus_reader::instanceKey<IndividualNumber>(0, value->getCmper1(), value->getInci(), musx::dom::Cmper(2)));
            CHECK(fields.size() == 19);
            CHECK(fields.at("enclosure.xMargin").origin == ValueOrigin::LegacyMus);
        }
        const auto first = document->getDetails()->get<IndividualNumber>(0, 3, 2, musx::dom::Inci{0});
        const auto second = document->getDetails()->get<IndividualNumber>(0, 3, 2, musx::dom::Inci{1});
        REQUIRE(first);
        REQUIRE(second);
        CHECK(first->measNumRegion == 17);
        CHECK(first->xOffset == -13);
        CHECK(first->yOffset == 24);
        CHECK(second->measNumRegion == 9);
        CHECK(second->xOffset == 0);
        CHECK(first->enclosure != second->enclosure);
    }
}

TEST_CASE("Coda positioning preserves the policy of skipping measure enclosures", "[class][measure-number-position]")
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    addPositionRegion(document, 1, 1);
    addPositionMeasureAndStaves(document, 2);
    const auto parsed = makeContainer({{2, "MI", {3, 0, -12, 24, 0, 0}}, {2, "Me", {1, 2, 3, 4, 5, 6}}}, FormatEpoch::CodaBanner);
    importIndividualNumbers(parsed, document);
    const auto values = document->getDetails()->getAllSources<IndividualNumber>();
    REQUIRE(values.size() == 1);
    CHECK(values[0]->xOffset == -12);
    CHECK_FALSE(values[0]->useEnclosure);
}

TEST_CASE("Older number region layouts normalize mid-system horizontal offsets", "[class][measure-number-position]")
{
    using System = musx::dom::others::StaffSystem;
    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto regionWords : {48U, 102U, 180U}) {
            if (epoch != FormatEpoch::ZlibLegacy && regionWords != 48) {
                continue;
            }
            auto session = musx::factory::DocumentFactory::begin();
            const auto document = session.getDocument();
            auto region = addPositionRegion(document, 1, 17);
            region->scoreData->showOnStart = true;
            region->scoreData->startXdisp = 10;
            auto system = std::make_shared<System>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper(1));
            system->startMeas = 1;
            system->endMeas = 5;
            system->horzPercent = 80;
            document->getOthers()->add(System::XmlNodeName, system);
            const auto parsedRegion = epoch == FormatEpoch::ZlibLegacy
                                          ? makeClassContainer(0x00a4, std::vector<std::int16_t>(regionWords), ByteOrder::BigEndian, 1)
                                          : makeContainer({{1, "MN", {}}}, epoch);
            auto parsed = parsedRegion;
            for (const auto measure : std::array<std::uint16_t, 2>{1, 2}) {
                std::vector<std::int16_t> words(15);
                words[0] = 17;
                words[1] = -52;
                const auto detail = epoch == FormatEpoch::ZlibLegacy ? makeDetailClassContainer(3, measure, 0, words, ByteOrder::BigEndian, 0x041f)
                                                                     : makeDetailContainer(epoch, 3, measure, words, "MI");
                parsed.blocks.push_back(detail.blocks.front());
            }
            const auto report = importIndividualNumbers(parsed, document);
            const auto start = document->getDetails()->get<IndividualNumber>(0, 3, 1, musx::dom::Inci{0});
            const auto middle = document->getDetails()->get<IndividualNumber>(0, 3, 2, musx::dom::Inci{0});
            REQUIRE(start);
            REQUIRE(middle);
            CHECK(start->xOffset == -52);
            CHECK(start->xOffset2 == 0);
            CHECK(middle->xOffset == (regionWords == 48 ? -63 : -52));
            CHECK(middle->xOffset2 == (regionWords == 48 ? 3 : 0));
            const auto& fields =
                report.fields.at(finale_mus_reader::instanceKey<IndividualNumber>(0, musx::dom::Cmper(3), musx::dom::Inci{0}, musx::dom::Cmper(2)));
            CHECK(fields.at("xOffset").origin == (regionWords == 48 ? ValueOrigin::LegacyMusAdjusted : ValueOrigin::LegacyMus));
            CHECK(fields.at("xOffset").rawValue == -52);
        }
    }
}

TEST_CASE("Older number region scaling preserves part-owned horizontal offsets", "[class][measure-number-position]")
{
    using System = musx::dom::others::StaffSystem;
    for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        auto region = addPositionRegion(document, 1, 17);
        region->scoreData->showOnStart = true;
        region->scoreData->startXdisp = 10;
        auto parsed = makeClassContainer(0x00a4, std::vector<std::int16_t>(48), order, 1);
        for (const auto partId : std::array<musx::dom::Cmper, 3>{0, 1, 2}) {
            auto system = std::make_shared<System>(document, partId, musx::dom::EnigmaBase::ShareMode::None, musx::dom::Cmper(1));
            system->startMeas = 1;
            system->endMeas = 5;
            system->horzPercent = 80;
            document->getOthers()->add(System::XmlNodeName, system);
            std::vector<std::int16_t> words(15);
            words[0] = 17;
            words[1] = partId == 0 ? -52 : -30;
            words[3] = partId == 0 ? 0 : 14;
            std::vector<std::uint16_t> masks(15);
            masks[1] = 0xffff;
            const auto detail = partId == 2 ? makeDetailClassContainer(3, 2, partId, words, order, 0x041f, true, masks)
                                            : makeDetailClassContainer(3, 2, partId, words, order, 0x041f);
            parsed.blocks.push_back(detail.blocks.front());
        }
        const auto report = importIndividualNumbers(parsed, document);
        for (const auto partId : std::array<musx::dom::Cmper, 3>{0, 1, 2}) {
            const auto value = document->getDetails()->get<IndividualNumber>(partId, 3, 2, musx::dom::Inci{0});
            REQUIRE(value);
            CHECK(value->getSourcePartId() == partId);
            CHECK(value->xOffset == (partId == 0 ? -63 : -30));
            CHECK(value->xOffset2 == (partId == 0 ? 3 : partId == 1 ? 14 : 0));
            const auto& fields = report.fields.at(
                finale_mus_reader::instanceKey<IndividualNumber>(partId, musx::dom::Cmper(3), musx::dom::Inci{0}, musx::dom::Cmper(2)));
            for (const auto* name : {"xOffset", "xOffset2"}) {
                CHECK(fields.at(name).origin == (partId == 0 ? ValueOrigin::LegacyMusAdjusted : ValueOrigin::LegacyMus));
            }
            CHECK(fields.at("xOffset").rawValue == (partId == 0 ? -52 : -30));
            CHECK(fields.at("xOffset2").rawValue == (partId == 1 ? 14 : 0));
        }
    }
}

TEST_CASE("Individual measure numbers decode signed offsets and all enclosure fields", "[class][measure-number-position]")
{
    const std::vector<std::int16_t> words{2, -13, 24, -35, 14, -4, 5, 6, 7, 128, static_cast<std::int16_t>(0xd803), 0, 0, 0, 0};
    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            const auto parsed = epoch == FormatEpoch::ZlibLegacy ? makeDetailClassContainer(3, 7, 0, words, order, 0x041f)
                                                                 : makeDetailContainer(epoch, 3, 7, words, "MI", order);
            auto session = musx::factory::DocumentFactory::begin();
            const auto document = session.getDocument();
            const auto report = importIndividualNumbers(parsed, document);
            const auto value = document->getDetails()->get<IndividualNumber>(0, 3, 7, musx::dom::Inci{0});
            REQUIRE(value);
            CHECK(value->measNumRegion == 2);
            CHECK(value->xOffset == -13);
            CHECK(value->yOffset == 24);
            CHECK(value->xOffset2 == -35);
            CHECK(value->forceVisibility == IndividualNumber::ForceVisibility::Show);
            CHECK(value->isAlternateNumber);
            CHECK(value->useEnclosure);
            REQUIRE(value->enclosure);
            CHECK(value->enclosure->xAdd == -4);
            CHECK(value->enclosure->yAdd == 5);
            CHECK(value->enclosure->xMargin == 6);
            CHECK(value->enclosure->yMargin == 7);
            CHECK(value->enclosure->lineWidth == 128);
            CHECK(static_cast<int>(value->enclosure->shape) == 3);
            CHECK(value->enclosure->fixedSize);
            CHECK(value->enclosure->notTall);
            CHECK(value->enclosure->equalAspect);
            CHECK(value->enclosure->opaque);
            CHECK_FALSE(value->enclosure->roundCorners);
            CHECK(value->enclosure->cornerRadius == 0);
            const auto key = finale_mus_reader::instanceKey<IndividualNumber>(0, musx::dom::Cmper{3}, musx::dom::Inci{0}, musx::dom::Cmper{7});
            const auto& fields = report.fields.at(key);
            std::set<std::string> names;
            for (const auto& [name, field] : fields) {
                names.insert(name);
                CHECK(field.origin
                      == ((name == "enclosure.roundCorners" || name == "enclosure.cornerRadius") ? ValueOrigin::MusxOnly : ValueOrigin::LegacyMus));
            }
            CHECK(names
                  == std::set<std::string>{"measNumRegion", "xOffset", "yOffset", "xOffset2", "forceVisibility", "isAlternateNumber", "useEnclosure",
                      "enclosure.xAdd", "enclosure.yAdd", "enclosure.xMargin", "enclosure.yMargin", "enclosure.lineWidth", "enclosure.shape",
                      "enclosure.fixedSize", "enclosure.notTall", "enclosure.equalAspect", "enclosure.opaque", "enclosure.roundCorners",
                      "enclosure.cornerRadius"});
            CHECK(fields.at("xOffset").rawValue == -13);
            CHECK(fields.at("forceVisibility").rawValue == 14);
            CHECK(IndividualNumber::xmlMappingArray().size() == 8);
        }
    }
}

TEST_CASE("Individual measure numbers preserve repeated items and ignore incomplete tails", "[class][measure-number-position]")
{
    std::vector<std::int16_t> words(35);
    words[0] = 1;
    words[4] = 1;
    words[15] = 2;
    words[19] = 0;
    words[30] = 99;
    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        auto session = musx::factory::DocumentFactory::begin();
        const auto parsed = epoch == FormatEpoch::ZlibLegacy ? makeDetailClassContainer(1, 2, 0, words, ByteOrder::LittleEndian, 0x041f)
                                                             : makeDetailContainer(epoch, 1, 2, words, "MI");
        const auto report = importIndividualNumbers(parsed, session.getDocument());
        const auto values = session.getDocument()->getDetails()->getAllSources<IndividualNumber>();
        REQUIRE(values.size() == 2);
        CHECK(values[0]->forceVisibility == IndividualNumber::ForceVisibility::Hide);
        CHECK(values[1]->forceVisibility == IndividualNumber::ForceVisibility::None);
        CHECK(values[1]->getInci() == 1);
        CHECK(values[1]->measNumRegion == 2);
        CHECK_FALSE(report.diagnostics.empty());
    }
}

TEST_CASE("Individual measure numbers overlay partial part payloads without changing score enclosures", "[class][measure-number-position]")
{
    for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        std::vector<std::int16_t> score(15);
        score[0] = 1;
        score[1] = 10;
        score[5] = 20;
        auto parsed = makeDetailClassContainer(1, 2, 0, score, order, 0x041f);
        std::vector<std::int16_t> part(15);
        part[1] = -30;
        part[5] = -40;
        std::vector<std::uint16_t> masks(15);
        masks[1] = masks[5] = 0xffff;
        const auto partBlock = makeDetailClassContainer(1, 2, 1, part, order, 0x041f, true, masks);
        parsed.blocks.push_back(partBlock.blocks.front());
        auto session = musx::factory::DocumentFactory::begin();
        importIndividualNumbers(parsed, session.getDocument());
        const auto original = session.getDocument()->getDetails()->get<IndividualNumber>(0, 1, 2, musx::dom::Inci{0});
        const auto changed = session.getDocument()->getDetails()->get<IndividualNumber>(1, 1, 2, musx::dom::Inci{0});
        REQUIRE(original);
        REQUIRE(changed);
        CHECK(original->xOffset == 10);
        CHECK(original->enclosure->xAdd == 20);
        CHECK(changed->measNumRegion == 1);
        CHECK(changed->xOffset == -30);
        CHECK(changed->enclosure->xAdd == -40);
        CHECK(changed->getShareMode() == musx::dom::EnigmaBase::ShareMode::Partial);
    }
}

TEST_CASE("Absent individual measure numbers create no objects", "[class][measure-number-position]")
{
    auto session = musx::factory::DocumentFactory::begin();
    importIndividualNumbers(makeContainer({}), session.getDocument());
    CHECK(session.getDocument()->getDetails()->getAllSources<IndividualNumber>().empty());
}
} // namespace
} // namespace finale_mus_reader_tests
