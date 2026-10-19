// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

void testMeasureGraphicAssignmentsAcrossEpochs()
{
    using Target = musx::dom::details::MeasureGraphicAssign;
    const std::vector<std::int16_t> tuple{0x100, 120, -324, 336, 168, 1, 0, 1, 393, 0, 0, 1, 336, 168, 0, 0, 0, 1, 0, 0};
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        const auto parsed = epoch == FormatEpoch::ZlibLegacy ? makeDetailClassContainer(1, 2, 0, tuple, ByteOrder::LittleEndian)
                                                             : makeDetailContainer(epoch, 1, 2, tuple);
        const auto index = LegacyRecordIndex::build(parsed);
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        auto referenceSession = musx::factory::DocumentFactory::begin();
        const auto reference = std::move(referenceSession).finish();
        ImportReport report(FormatEpoch::UncompressedLegacy);
        finale_mus_reader::PendingReferences pending;
        SourceProfile profile(epoch);
        profile.byteOrder = parsed.byteOrder;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::details::importMeasureGraphicAssignments(context);
        const auto assignment = document->getDetails()->get<Target>(musx::dom::SCORE_PARTID, 1, 2, musx::dom::Inci(0));
        expectMapping(assignment && assignment->version == 0x100 && assignment->left == 120 && assignment->bottom == -324 && assignment->width == 336
                          && assignment->height == 168 && assignment->fDescId == 1 && !assignment->hidden
                          && assignment->hAlign == Target::HorizontalAlignment::Left && assignment->vAlign == Target::VerticalAlignment::Top
                          && assignment->posFrom == Target::PositionFrom::PageEdge && assignment->fixedPerc && assignment->savedRecord
                          && assignment->origWidth == 336 && assignment->origHeight == 168 && assignment->graphicCmper == 1,
            "A MeasureGraphicAssign field or comparator failed in epoch " + std::to_string(static_cast<int>(epoch)));
        expectMapping(reportedFieldCount(report) == 15, "A MeasureGraphicAssign did not report every persisted field");
        const auto instance =
            finale_mus_reader::instanceKey<Target>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1), musx::dom::Inci(0), musx::dom::Cmper(2));
        for (const auto* member : {"hAlign", "vAlign", "posFrom", "fixedPerc"}) {
            const auto* info = report.findField(instance, member);
            expectMapping(info && info->origin == ValueOrigin::LegacyMus && info->rawValue == 393,
                std::string("MeasureGraphicAssign did not recover ") + member + " from the packed positioning word");
        }
    }
    const auto bigEndian = makeDetailClassContainer(7, 12, 2, tuple, ByteOrder::BigEndian);
    const auto bigEndianIndex = LegacyRecordIndex::build(bigEndian);
    const auto bigEndianRows = bigEndianIndex.getClassDetails().getArray(0x041d, 7, 12, 2);
    expectMapping(bigEndianRows.size() == 1 && bigEndianRows.front().partId == 2 && bigEndianRows.front().inci == 0
                      && bigEndianIndex.getClassDetails().get(0x041d, 7, 12, 0) == nullptr
                      && bigEndianIndex.getClassDetails().get(0x041d, 7, 12, 0, 2) == &bigEndianRows.front()
                      && bigEndianIndex.getClassDetails().payloadOf(bigEndianRows.front()).size() == 40,
        "A big-endian zlib detail did not preserve cmper2, part id, and payload length");

    auto partSession = musx::factory::DocumentFactory::begin();
    const auto partDocument = partSession.getDocument();
    auto partReferenceSession = musx::factory::DocumentFactory::begin();
    const auto partReference = std::move(partReferenceSession).finish();
    ImportReport partReport(FormatEpoch::UncompressedLegacy);
    finale_mus_reader::PendingReferences partPending;
    SourceProfile partProfile(FormatEpoch::ZlibLegacy);
    partProfile.byteOrder = ByteOrder::BigEndian;
    musx::factory::ConstructionContext partConstruction;
    const finale_mus_reader::ImportContext partContext{
        bigEndianIndex, partProfile, noSource, partDocument, partReference, partReport, partPending, partConstruction};
    finale_mus_reader::details::importMeasureGraphicAssignments(partContext);
    const auto partAssignment = partDocument->getDetails()->get<Target>(2, 7, 12, musx::dom::Inci(0));
    expectMapping(partAssignment && partAssignment->getSourcePartId() == 2 && partAssignment->getShareMode() == musx::dom::EnigmaBase::ShareMode::None
                      && !partDocument->getDetails()->get<Target>(musx::dom::SCORE_PARTID, 7, 12, musx::dom::Inci(0)),
        "A standalone part-owned zlib detail did not retain its "
        "identity and sharing mode");

    auto partialTuple = tuple;
    partialTuple[1] = 777;
    partialTuple[2] = 888;
    partialTuple[3] = 999;
    partialTuple[7] = 0x11;
    std::vector<std::uint16_t> partialMasks(18);
    partialMasks[1] = 0xffff;
    partialMasks[7] = 0x0010;
    auto partialParsed = makeDetailClassContainer(7, 12, 0, tuple, ByteOrder::BigEndian);
    auto partialPart = makeDetailClassContainer(7, 12, 2, partialTuple, ByteOrder::BigEndian, 0x041d, true, partialMasks);
    auto& partialData = partialParsed.blocks.front().data;
    partialData.insert(partialData.end(), partialPart.blocks.front().data.begin(), partialPart.blocks.front().data.end());
    partialParsed.blocks.front().info.decodedSize = partialData.size();
    const auto partialIndex = LegacyRecordIndex::build(partialParsed);
    auto partialSession = musx::factory::DocumentFactory::begin();
    const auto partialDocument = partialSession.getDocument();
    ImportReport partialReport(FormatEpoch::ZlibLegacy);
    const finale_mus_reader::ImportContext partialContext{
        partialIndex, partProfile, noSource, partialDocument, partReference, partialReport, partPending, partConstruction};
    finale_mus_reader::details::importMeasureGraphicAssignments(partialContext);
    const auto partialAssignment = partialDocument->getDetails()->get<Target>(2, 7, 12, musx::dom::Inci(0));
    expectMapping(partialAssignment && partialAssignment->left == 777 && partialAssignment->bottom == -324 && partialAssignment->width == 336
                      && partialAssignment->hidden,
        "A continued measure graphic did not overlay only its unlinked fields");

    const auto continued = makeDetailClassContainer(7, 12, 2, tuple, ByteOrder::BigEndian, 0x041d, true);
    const auto continuedIndex = LegacyRecordIndex::build(continued);
    const auto continuedRows = continuedIndex.getClassDetails().getArray(0x041d, 7, 12, 2);
    const finale_mus_reader::RecordFamilySource continuedSource{&continuedIndex.getClassDetails(), 0x041d, true, true, {}};
    expectMapping(continuedRows.size() == 1
                      && finale_mus_reader::recordShareMode(continuedSource, continuedRows.front()) == musx::dom::EnigmaBase::ShareMode::Partial,
        "A continued detail record did not select partial sharing");
}

void testFinale372MeasureGraphic()
{
    const auto result =
        Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F372/F372-measure-graphic.mus");
    const auto assignment =
        result.document->getDetails()->get<musx::dom::details::MeasureGraphicAssign>(musx::dom::SCORE_PARTID, 1, 3, musx::dom::Inci(0));
    expect(assignment && assignment->version == 0x100 && assignment->left == 116 && assignment->bottom == -348 && assignment->width == 336
               && assignment->height == 168 && assignment->fDescId == 1 && assignment->savedRecord && assignment->origWidth == 336
               && assignment->origHeight == 168 && assignment->graphicCmper == 0,
        "The Finale 3.7.2 linked measure graphic was not recovered");
    expect(result.document->getEmbeddedGraphics().empty(), "A Finale 3.7.2 linked graphic was mistaken for an embedded payload");
}

TEST_CASE("Finale 3.7.2 measure graphic", "[class][reader]")
{
    testFinale372MeasureGraphic();
}

TEST_CASE("Measure graphic assignments span four epochs", "[class]")
{
    testMeasureGraphicAssignmentsAcrossEpochs();
}

} // namespace
} // namespace finale_mus_reader_tests
