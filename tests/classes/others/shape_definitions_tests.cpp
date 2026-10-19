// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

void testShapeDefinitions()
{
    using namespace musx::dom;
    const auto readShapeFixture = [](std::string_view relative) {
        return Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / relative);
    };

    const auto verifyModern = [&](std::string_view path, const std::string& what) {
        const auto result = readShapeFixture(path);
        const auto shape = result.document->getOthers()->get<others::ShapeDef>(SCORE_PARTID, 1);
        const auto instructions = result.document->getOthers()->get<others::ShapeInstructionList>(SCORE_PARTID, 1);
        const auto data = result.document->getOthers()->get<others::ShapeData>(SCORE_PARTID, 1);
        expect(shape && shape->instructionList == 1 && shape->dataList == 1 && shape->shapeType == others::ShapeDef::ShapeType::Other,
            what + " did not recover the shape definition");
        // The fixed rows contain 21 slots, but the last is the zero terminator/padding.
        expect(instructions && instructions->instructions.size() == 20, what + " did not recover the complete instruction list");
        expect(instructions->instructions[0]->type == ShapeDefInstructionType::StartGroup && instructions->instructions[0]->numData == 11
                   && instructions->instructions[1]->type == ShapeDefInstructionType::StartObject
                   && instructions->instructions.back()->type == ShapeDefInstructionType::EndGroup,
            what + " mistranslated packed instruction tags");
        expect(data && data->values.size() == 66 && data->values[0] == 0 && data->values[2] == (std::numeric_limits<int>::max)()
                   && data->values[3] == (std::numeric_limits<int>::min)(),
            what + " did not recover signed 32-bit shape data");
        expect(field(result, "others.shapeDef[1].instructionList").origin == ValueOrigin::LegacyMus
                   && field(result, "others.shapeList[1].instructions[0].numData").rawValue == 11
                   && field(result, "others.shapeData[1].values[3]").rawValue == (std::numeric_limits<int>::min)(),
            what + " did not report shape provenance and raw values");
    };

    // Same logical record in the fixed-row and both zlib byte orders.
    verifyModern("evidence/F2002/F2002-baseline.mus", "Finale 2002");
    verifyModern("evidence/F2007/F2007-lyric-hyphens.mus", "Finale 2007");
    verifyModern("evidence/F2012/F2012-upstem-flags.mus", "Finale 2012");

    const auto zlibTypes = readShapeFixture("evidence/F2012/F2012-graphics-types.mus");
    expect(zlibTypes.document->getOthers()->get<others::ShapeDef>(SCORE_PARTID, 2)->shapeType == others::ShapeDef::ShapeType::Clef
               && zlibTypes.document->getOthers()->get<others::ShapeDef>(SCORE_PARTID, 3)->shapeType == others::ShapeDef::ShapeType::Clef
               && zlibTypes.document->getOthers()->get<others::ShapeDef>(SCORE_PARTID, 4)->shapeType == others::ShapeDef::ShapeType::Expression,
        "Zlib ShapeDef word 2 was not recovered as shapeType");
    // Its external-graphic instructions name ShapeGraphicAssign records another importer fills.
    expect(std::any_of(zlibTypes.report.diagnostics.begin(), zlibTypes.report.diagnostics.end(),
               [](const auto& d) {
                   return d.level == musx::util::Logger::LogLevel::Verbose
                          && d.message == "3 shape external-graphic instruction(s) were checked; 0 do not resolve to a ShapeGraphicAssign.";
               }),
        "External-graphic instructions did not resolve to their shape graphic assignments");

    // Shape list 2 has two stale packed instructions after its zero terminator. Decoding stops
    // at the terminator.
    const auto terminated = readShapeFixture("evidence/F2002/F2002-baseline.mus");
    const auto terminatedList = terminated.document->getOthers()->get<others::ShapeInstructionList>(SCORE_PARTID, 2);
    const auto terminatedData = terminated.document->getOthers()->get<others::ShapeData>(SCORE_PARTID, 2);
    expect(terminatedList && terminatedList->instructions.size() == 12 && terminatedData && terminatedData->values.size() == 45,
        "A zero shape-instruction terminator did not suppress stale trailing instructions");

    const auto early = readShapeFixture("evidence/F263/F263-baseline.mus");
    const auto earlyShape = early.document->getOthers()->get<others::ShapeDef>(SCORE_PARTID, 1);
    const auto earlyInstructions = early.document->getOthers()->get<others::ShapeInstructionList>(SCORE_PARTID, 1);
    const auto earlyData = early.document->getOthers()->get<others::ShapeData>(SCORE_PARTID, 1);
    expect(earlyShape && earlyShape->instructionList == 1 && earlyShape->dataList == 1 && earlyShape->shapeType == others::ShapeDef::ShapeType::Other,
        "Finale 2.6 bounding words were mistaken for a modern shape type");
    expect(earlyInstructions && earlyInstructions->instructions.size() == 9
               && earlyInstructions->instructions[0]->type == ShapeDefInstructionType::LineWidth && earlyInstructions->instructions[0]->numData == 1,
        "Finale 2.6 revision-1 instructions were not translated");
    expect(earlyData && earlyData->values.size() == 15 && earlyData->values[0] == 1024,
        "Finale 2.6 hundredths-of-a-point line width was not converted to Efix (size " + std::to_string(earlyData ? earlyData->values.size() : 0)
            + ", first " + std::to_string(earlyData && !earlyData->values.empty() ? earlyData->values.front() : 0) + ")");
    expect(fieldFor<others::ShapeData>(early, "others.shapeData[1].values[0]").rawValue == 400,
        "The converted Finale 2.6 line width did not retain its source value in the report");

    // This file stores a physical shape list at comparator 0, which no shape can name.
    const auto reserved = readShapeFixture("evidence/F100/F100-C4.mus");
    expect(!reserved.document->getOthers()->get<others::ShapeInstructionList>(SCORE_PARTID, 0), "The reserved shape-list comparator 0 was imported");
    expect(std::any_of(reserved.report.diagnostics.begin(), reserved.report.diagnostics.end(),
               [](const auto& d) {
                   return d.level == musx::util::Logger::LogLevel::Verbose && d.message.starts_with("Skipped ShapeInstructionList comparator 0");
               }),
        "The skipped reserved shape list was not reported at Verbose");
}

TEST_CASE("Shape definitions", "[class][reader]")
{
    testShapeDefinitions();
}

} // namespace
} // namespace finale_mus_reader_tests
