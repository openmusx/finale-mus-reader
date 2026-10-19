// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using BeatChartElement = musx::dom::others::BeatChartElement;
constexpr musx::dom::Cmper syntheticCmper = 7;
constexpr musx::dom::Cmper fixtureCmper = 1;

using Justification = musx::dom::others::Measure::BeatChartJustification;

ImportReport importBeatChart(const finale_mus_reader::container::ParsedContainer& parsed, musx::dom::DocumentPtr& document,
    std::optional<Justification> expectedJustification = std::nullopt)
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
    finale_mus_reader::others::importBeatChartElements(context);
    if (expectedJustification) {
        const musx::dom::others::Measure measure(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, syntheticCmper);
        CHECK(finale_mus_reader::others::beatChartJustification(context, measure) == *expectedJustification);
    }
    return report;
}

void checkBeatChart(const musx::dom::DocumentPtr& document, const ImportReport& report)
{
    const auto control = document->getOthers()->get<BeatChartElement>(musx::dom::SCORE_PARTID, syntheticCmper, musx::dom::Inci(0));
    const auto element = document->getOthers()->get<BeatChartElement>(musx::dom::SCORE_PARTID, syntheticCmper, musx::dom::Inci(1));
    REQUIRE(control);
    REQUIRE(control->control);
    CHECK(control->control->totalDur == 4096);
    CHECK(control->control->totalWidth == 304);
    CHECK(control->control->minWidth == 155);
    CHECK(control->control->allotWidth == 189);
    REQUIRE(element);
    CHECK_FALSE(element->control);
    CHECK(element->dur == 1024);
    CHECK(element->pos == -84);
    CHECK(element->endPos == 168);
    CHECK(element->minPos == -59);

    const auto controlKey = finale_mus_reader::instanceKey<BeatChartElement>(musx::dom::SCORE_PARTID, syntheticCmper, musx::dom::Inci(0));
    const auto elementKey = finale_mus_reader::instanceKey<BeatChartElement>(musx::dom::SCORE_PARTID, syntheticCmper, musx::dom::Inci(1));
    REQUIRE(report.fields.contains(controlKey));
    REQUIRE(report.fields.contains(elementKey));
    CHECK(report.fields.at(controlKey).size() == BeatChartElement::Control::xmlMappingArray().size());
    CHECK(report.fields.at(elementKey).size() == BeatChartElement::xmlMappingArray().size() - 1);
    CHECK(report.fields.at(controlKey).at("control.totalDur").origin == ValueOrigin::LegacyMus);
    CHECK(report.fields.at(elementKey).at("minPos").origin == ValueOrigin::LegacyMus);
}

TEST_CASE("BeatChartElement recovers its row tuple in every epoch", "[class][beat-chart-element]")
{
    const std::vector<SyntheticRow> fixedRows{
        {syntheticCmper, "BC", {0, 4096, 304, 155, 189, 1}}, {syntheticCmper, "BC", {0, 1024, -84, 168, -59, 0}}};
    const std::vector<std::int16_t> classWords{4096, 0, 304, 155, 189, 1, 1024, 0, -84, 168, -59, 0};
    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        musx::dom::DocumentPtr document;
        const auto report = importBeatChart(makeContainer(fixedRows, epoch), document);
        checkBeatChart(document, report);
    }
    musx::dom::DocumentPtr document;
    const auto report = importBeatChart(makeClassContainer(0x007a, classWords, ByteOrder::LittleEndian, syntheticCmper), document);
    checkBeatChart(document, report);
}

TEST_CASE("Beat chart justification is a conversion input without a DOM field", "[class][beat-chart-element]")
{
    for (const std::int16_t flag : {std::int16_t(0), std::int16_t(1), std::int16_t(2), std::int16_t(3)}) {
        const auto expected = flag & 1 ? Justification::Modern : Justification::Legacy;
        for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::CodaBanner}) {
            musx::dom::DocumentPtr document;
            const std::vector<SyntheticRow> rows{{syntheticCmper, "BC", {0, 4096, 304, 155, 189, flag}}};
            importBeatChart(makeContainer(rows, epoch), document, epoch == FormatEpoch::CodaBanner ? Justification::Legacy : expected);
        }
        musx::dom::DocumentPtr document;
        const std::vector<std::int16_t> words{4096, 0, 304, 155, 189, flag};
        importBeatChart(makeClassContainer(0x007a, words, ByteOrder::LittleEndian, syntheticCmper), document, expected);
    }
}

TEST_CASE("Coda BeatChartElement separates the control and first positioned element", "[class][beat-chart-element]")
{
    const std::vector<SyntheticRow> rows{{syntheticCmper, "BC", {0, 4096, 44, 92, 839, 0}}, {syntheticCmper, "BC", {0, 256, 147, 195, 588, 0}}};
    musx::dom::DocumentPtr document;
    const auto report = importBeatChart(makeContainer(rows, FormatEpoch::CodaBanner), document);
    const auto control = document->getOthers()->get<BeatChartElement>(musx::dom::SCORE_PARTID, syntheticCmper, musx::dom::Inci(0));
    const auto first = document->getOthers()->get<BeatChartElement>(musx::dom::SCORE_PARTID, syntheticCmper, musx::dom::Inci(1));
    const auto second = document->getOthers()->get<BeatChartElement>(musx::dom::SCORE_PARTID, syntheticCmper, musx::dom::Inci(2));
    REQUIRE(control);
    REQUIRE(control->control);
    CHECK(control->control->totalDur == 4096);
    CHECK(control->control->totalWidth == 839);
    CHECK(control->control->minWidth == 0);
    CHECK(control->control->allotWidth == 0);
    REQUIRE(first);
    CHECK(first->dur == 0);
    CHECK(first->pos == 44);
    CHECK(first->endPos == 92);
    CHECK(first->minPos == 0);
    REQUIRE(second);
    CHECK(second->dur == 256);
    CHECK(second->pos == 147);
    CHECK(second->endPos == 195);
    CHECK(second->minPos == 0);
    CHECK(field(report, "others.beatChart[7,0].control.minWidth").origin == ValueOrigin::Finale27Default);
    CHECK(field(report, "others.beatChart[7,1].dur").origin == ValueOrigin::Finale27Default);
    CHECK(field(report, "others.beatChart[7,2].dur").origin == ValueOrigin::LegacyMus);
}

TEST_CASE("BeatChartElement recovers tracked fixed-row fixtures", "[class][beat-chart-element][fixture]")
{
    const auto uncompressed = readFixture("evidence/F97/F97-disptime.mus");
    const auto first = uncompressed.document->getOthers()->get<BeatChartElement>(musx::dom::SCORE_PARTID, fixtureCmper, musx::dom::Inci(0));
    const auto last = uncompressed.document->getOthers()->get<BeatChartElement>(musx::dom::SCORE_PARTID, fixtureCmper, musx::dom::Inci(12));
    REQUIRE(first);
    REQUIRE(first->control);
    CHECK(first->control->totalDur == 4096);
    CHECK(first->control->totalWidth == 624);
    CHECK(first->control->minWidth == 660);
    CHECK(first->control->allotWidth == 1066);
    REQUIRE(last);
    CHECK(last->dur == 3584);
    CHECK(last->pos == 564);
    CHECK(last->endPos == 624);
    CHECK(last->minPos == 578);

    const auto dcl = readFixture("evidence/F2004/F2004-brakpos-17.mus");
    const auto dclControl = dcl.document->getOthers()->get<BeatChartElement>(musx::dom::SCORE_PARTID, fixtureCmper, musx::dom::Inci(0));
    const auto dclElement = dcl.document->getOthers()->get<BeatChartElement>(musx::dom::SCORE_PARTID, fixtureCmper, musx::dom::Inci(2));
    REQUIRE(dclControl);
    REQUIRE(dclControl->control);
    CHECK(dclControl->control->totalDur == 4096);
    CHECK(dclControl->control->allotWidth == 189);
    REQUIRE(dclElement);
    CHECK(dclElement->dur == 1024);
    CHECK(dclElement->pos == 84);
    CHECK(dclElement->endPos == 168);
    CHECK(dclElement->minPos == 59);
}

TEST_CASE("BeatChartElement rejects an incomplete trailing tuple", "[class][beat-chart-element]")
{
    musx::dom::DocumentPtr document;
    const auto report =
        importBeatChart(makeClassContainer(0x007a, {4096, 0, 304, 155, 189, 1, 99}, ByteOrder::LittleEndian, syntheticCmper), document);
    CHECK(document->getOthers()->get<BeatChartElement>(musx::dom::SCORE_PARTID, syntheticCmper, musx::dom::Inci(0)));
    CHECK_FALSE(document->getOthers()->get<BeatChartElement>(musx::dom::SCORE_PARTID, syntheticCmper, musx::dom::Inci(1)));
    CHECK(std::any_of(report.diagnostics.begin(), report.diagnostics.end(),
        [](const auto& diagnostic) { return diagnostic.message.find("incomplete trailing element") != std::string::npos; }));
}

} // namespace
} // namespace finale_mus_reader_tests
