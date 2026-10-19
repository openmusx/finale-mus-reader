// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

void testFretClassesAreSourceOwned()
{
    using FretInstrument = musx::dom::others::FretInstrument;
    using FretboardGroup = musx::dom::others::FretboardGroup;
    using FretboardStyle = musx::dom::others::FretboardStyle;
    using FretboardDiagram = musx::dom::details::FretboardDiagram;

    std::vector<std::int16_t> instrumentWords(36);
    instrumentWords[0] = 0x000a;
    instrumentWords[2] = 21;
    instrumentWords[3] = 2;
    instrumentWords[4] = 5;
    instrumentWords[6] = 0x4869;
    instrumentWords[30] = 0x4000;
    instrumentWords[31] = 0x3b00;
    std::vector<std::int16_t> groupWords(30);
    groupWords[0] = 7;
    groupWords[6] = 0x4772;
    std::vector<std::int16_t> styleWords(78);
    styleWords[0] = 1;
    styleWords[3] = 11;
    styleWords[4] = 12;
    styleWords[5] = 13;
    styleWords[6] = 14;
    styleWords[7] = 15;
    styleWords[8] = 4;
    styleWords[10] = 900;
    styleWords[12] = 1404;
    styleWords[29] = 2;
    styleWords[30] = 9;
    styleWords[32] = 3;
    styleWords[33] = 5;
    styleWords[38] = 80;
    styleWords[42] = 0x5374;
    styleWords[66] = 0x6672;
    styleWords[67] = 0x2e00;

    const auto parsed = makeClassContainer({{0x0094, groupWords, 9}, {0x0095, instrumentWords, 7}, {0x0097, styleWords, 3}}, ByteOrder::BigEndian);
    const auto index = LegacyRecordIndex::build(parsed);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    ImportReport report(FormatEpoch::UncompressedLegacy);
    finale_mus_reader::PendingReferences pending;
    SourceProfile profile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importFretInstruments(context);
    finale_mus_reader::others::importFretboardGroups(context);
    finale_mus_reader::others::importFretboardStyles(context);

    const auto instrument = document->getOthers()->get<FretInstrument>(musx::dom::SCORE_PARTID, 7);
    const auto group = document->getOthers()->get<FretboardGroup>(musx::dom::SCORE_PARTID, 9, musx::dom::Inci(0));
    const auto style = document->getOthers()->get<FretboardStyle>(musx::dom::SCORE_PARTID, 3);
    expectMapping(instrument && instrument->numFrets == 21 && instrument->numStrings == 2 && instrument->speedyClef == 5 && instrument->name == "Hi"
                      && instrument->strings.size() == 2 && instrument->strings[0]->pitch == 64 && instrument->strings[1]->pitch == 59
                      && instrument->fretSteps == std::vector<int>{2, 4},
        "A zlib FretInstrument field failed to decode");
    expectMapping(group && group->fretInstId == 7 && group->name == "Gr", "A zlib FretboardGroup tuple failed to decode");
    expectMapping(style && style->showLastFret && style->fingStrShapeId == 11 && style->openStrShapeId == 12 && style->muteStrShapeId == 13
                      && style->barreShapeId == 14 && style->customShapeId == 15 && style->defNumFrets == 4 && style->stringGap == 900
                      && style->fretGap == 1404 && style->fretNumFont->fontId == 2 && style->fretNumFont->fontSize == 9
                      && style->fingNumFont->fontId == 3 && style->fingNumFont->fontSize == 5 && style->vertFingNumOff == 80 && style->name == "St"
                      && style->fretNumText == "fr.",
        "A zlib FretboardStyle field failed to decode");

    const std::vector<std::int16_t> diagramWords{
        4, 2, 5, 3, 1, 0x2002, 1, 0x1803, static_cast<std::int16_t>(0xa001), 0, 0x2800, 2, 0, 0, 0, 1, 0x0105, 0, 0, 0};
    const auto detailParsed = makeDetailClassContainer(9, 2, 0, diagramWords, ByteOrder::BigEndian, 0x0413);
    const auto detailIndex = LegacyRecordIndex::build(detailParsed);
    ImportReport detailReport(FormatEpoch::UncompressedLegacy);
    finale_mus_reader::PendingReferences detailPending;
    musx::factory::ConstructionContext detailConstruction;
    const finale_mus_reader::ImportContext detailContext{
        detailIndex, profile, noSource, document, reference, detailReport, detailPending, detailConstruction};
    finale_mus_reader::details::importFretboardDiagrams(detailContext);
    const auto diagram = document->getDetails()->get<FretboardDiagram>(musx::dom::SCORE_PARTID, 9, 2);
    expectMapping(diagram && diagram->numFrets == 4 && diagram->fretboardNum == 2 && diagram->lock && diagram->showNum && diagram->cells.size() == 3
                      && diagram->cells[0]->string == 4 && diagram->cells[0]->fret == 2 && diagram->cells[0]->shape == FretboardDiagram::Shape::Closed
                      && diagram->cells[1]->fingerNum == 5 && diagram->cells[2]->shape == FretboardDiagram::Shape::Open && diagram->barres.size() == 1
                      && diagram->barres[0]->fret == 1 && diagram->barres[0]->startString == 1 && diagram->barres[0]->endString == 5,
        "A zlib FretboardDiagram field or padded item array failed to decode");

    std::vector<SyntheticRow> fixedRows;
    const auto appendFixedOther = [&](std::uint16_t cmper, const char* tag, const std::vector<std::int16_t>& words) {
        for (std::size_t at = 0; at < words.size(); at += 6) {
            std::array<std::int16_t, 6> incidence{};
            std::copy_n(words.begin() + static_cast<std::ptrdiff_t>(at), 6, incidence.begin());
            fixedRows.push_back({cmper, tag, incidence});
        }
    };
    appendFixedOther(9, "fg", groupWords);
    appendFixedOther(7, "fI", instrumentWords);
    appendFixedOther(3, "ft", styleWords);
    const auto fixedParsed = makeContainer(fixedRows, FormatEpoch::DclLegacy);
    const auto fixedIndex = LegacyRecordIndex::build(fixedParsed);
    auto fixedSession = musx::factory::DocumentFactory::begin();
    const auto fixedDocument = fixedSession.getDocument();
    ImportReport fixedReport(FormatEpoch::UncompressedLegacy);
    finale_mus_reader::PendingReferences fixedPending;
    SourceProfile fixedProfile(FormatEpoch::DclLegacy);
    fixedProfile.byteOrder = ByteOrder::BigEndian;
    musx::factory::ConstructionContext fixedConstruction;
    const finale_mus_reader::ImportContext fixedContext{
        fixedIndex, fixedProfile, noSource, fixedDocument, reference, fixedReport, fixedPending, fixedConstruction};
    finale_mus_reader::others::importFretInstruments(fixedContext);
    finale_mus_reader::others::importFretboardGroups(fixedContext);
    finale_mus_reader::others::importFretboardStyles(fixedContext);
    const auto fixedInstrument = fixedDocument->getOthers()->get<FretInstrument>(musx::dom::SCORE_PARTID, 7);
    const auto fixedGroup = fixedDocument->getOthers()->get<FretboardGroup>(musx::dom::SCORE_PARTID, 9, musx::dom::Inci(0));
    const auto fixedStyle = fixedDocument->getOthers()->get<FretboardStyle>(musx::dom::SCORE_PARTID, 3);
    expectMapping(fixedInstrument && fixedInstrument->numFrets == 21 && fixedInstrument->strings.size() == 2 && fixedInstrument->name == "Hi"
                      && fixedGroup && fixedGroup->fretInstId == 7 && fixedGroup->name == "Gr" && fixedStyle && fixedStyle->stringGap == 900
                      && fixedStyle->name == "St" && fixedStyle->fretNumText == "fr.",
        "A fixed-row fret others record failed to decode");

    const auto fixedDetailParsed = makeDetailContainer(FormatEpoch::DclLegacy, 9, 2, diagramWords, "fb");
    const auto fixedDetailIndex = LegacyRecordIndex::build(fixedDetailParsed);
    ImportReport fixedDetailReport(FormatEpoch::UncompressedLegacy);
    finale_mus_reader::PendingReferences fixedDetailPending;
    musx::factory::ConstructionContext fixedDetailConstruction;
    const finale_mus_reader::ImportContext fixedDetailContext{
        fixedDetailIndex, fixedProfile, noSource, fixedDocument, reference, fixedDetailReport, fixedDetailPending, fixedDetailConstruction};
    finale_mus_reader::details::importFretboardDiagrams(fixedDetailContext);
    const auto fixedDiagram = fixedDocument->getDetails()->get<FretboardDiagram>(musx::dom::SCORE_PARTID, 9, 2);
    expectMapping(fixedDiagram && fixedDiagram->cells.size() == 3 && fixedDiagram->barres.size() == 1 && fixedDiagram->showNum,
        "A fixed-row fretboard diagram failed to decode");

    const auto emptyIndex = LegacyRecordIndex::build(makeClassContainer(std::vector<SyntheticClassRow>{}, ByteOrder::LittleEndian));
    auto emptySession = musx::factory::DocumentFactory::begin();
    const auto emptyDocument = emptySession.getDocument();
    ImportReport emptyReport(FormatEpoch::UncompressedLegacy);
    finale_mus_reader::PendingReferences emptyPending;
    musx::factory::ConstructionContext emptyConstruction;
    const finale_mus_reader::ImportContext emptyContext{
        emptyIndex, profile, noSource, emptyDocument, reference, emptyReport, emptyPending, emptyConstruction};
    finale_mus_reader::others::importFretInstruments(emptyContext);
    finale_mus_reader::others::importFretboardGroups(emptyContext);
    finale_mus_reader::others::importFretboardStyles(emptyContext);
    finale_mus_reader::details::importFretboardDiagrams(emptyContext);
    expectMapping(emptyDocument->getOthers()->getArray<FretInstrument>(musx::dom::SCORE_PARTID).empty()
                      && emptyDocument->getOthers()->getArray<FretboardGroup>(musx::dom::SCORE_PARTID).empty()
                      && emptyDocument->getOthers()->getArray<FretboardStyle>(musx::dom::SCORE_PARTID).empty()
                      && emptyDocument->getDetails()->getArray<FretboardDiagram>(musx::dom::SCORE_PARTID).empty(),
        "An absent source fret record synthesized an instance");

    for (const auto emptyEpoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        const auto emptyFixedIndex = LegacyRecordIndex::build(makeContainer({}, emptyEpoch));
        auto emptyFixedSession = musx::factory::DocumentFactory::begin();
        const auto emptyFixedDocument = emptyFixedSession.getDocument();
        ImportReport emptyFixedReport(FormatEpoch::UncompressedLegacy);
        finale_mus_reader::PendingReferences emptyFixedPending;
        SourceProfile emptyFixedProfile(emptyEpoch);
        emptyFixedProfile.byteOrder = ByteOrder::BigEndian;
        musx::factory::ConstructionContext emptyFixedConstruction;
        const finale_mus_reader::ImportContext emptyFixedContext{
            emptyFixedIndex, emptyFixedProfile, noSource, emptyFixedDocument, reference, emptyFixedReport, emptyFixedPending, emptyFixedConstruction};
        finale_mus_reader::others::importFretInstruments(emptyFixedContext);
        finale_mus_reader::others::importFretboardGroups(emptyFixedContext);
        finale_mus_reader::others::importFretboardStyles(emptyFixedContext);
        finale_mus_reader::details::importFretboardDiagrams(emptyFixedContext);
        expectMapping(emptyFixedDocument->getOthers()->getArray<FretInstrument>(musx::dom::SCORE_PARTID).empty()
                          && emptyFixedDocument->getOthers()->getArray<FretboardGroup>(musx::dom::SCORE_PARTID).empty()
                          && emptyFixedDocument->getOthers()->getArray<FretboardStyle>(musx::dom::SCORE_PARTID).empty()
                          && emptyFixedDocument->getDetails()->getArray<FretboardDiagram>(musx::dom::SCORE_PARTID).empty(),
            "An empty fixed-row epoch synthesized a fret instance");
    }
}

void testFretboardGroupUnicodeLayout()
{
    using FretboardGroup = musx::dom::others::FretboardGroup;
    constexpr std::size_t tupleWords = 102;
    std::vector<std::int16_t> words(tupleWords * 2);
    words[0] = 2;
    const std::u16string firstName = u"Simple Major Triad";
    std::copy(firstName.begin(), firstName.end(), words.begin() + 6);
    words[tupleWords] = 2;
    const std::u16string secondName = u"Major   (copy)";
    std::copy(secondName.begin(), secondName.end(), words.begin() + tupleWords + 6);

    const auto index = LegacyRecordIndex::build(makeClassContainer(0x0094, words, ByteOrder::LittleEndian, 1));
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    ImportReport report(FormatEpoch::UncompressedLegacy);
    finale_mus_reader::PendingReferences pending;
    auto profile = profileFor(17);
    profile.epoch = FormatEpoch::ZlibLegacy;
    profile.byteOrder = ByteOrder::LittleEndian;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importFretboardGroups(context);

    const auto first = document->getOthers()->get<FretboardGroup>(musx::dom::SCORE_PARTID, 1, musx::dom::Inci(0));
    const auto second = document->getOthers()->get<FretboardGroup>(musx::dom::SCORE_PARTID, 1, musx::dom::Inci(1));
    const auto nonexistent = document->getOthers()->get<FretboardGroup>(musx::dom::SCORE_PARTID, 1, musx::dom::Inci(2));
    expectMapping(first && first->fretInstId == 2 && first->name == "Simple Major Triad" && second && second->fretInstId == 2
                      && second->name == "Major   (copy)" && !nonexistent,
        "A Finale 2012 FretboardGroup did not use its 204-byte UTF-16LE tuple");
}

TEST_CASE("Fret classes are source owned", "[class]")
{
    testFretClassesAreSourceOwned();
}
TEST_CASE("Fretboard group Unicode layout", "[class]")
{
    testFretboardGroupUnicodeLayout();
}

} // namespace
} // namespace finale_mus_reader_tests
