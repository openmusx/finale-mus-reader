// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {
using namespace classes;

std::shared_ptr<const Spacing> importSpacingForTest(
    const finale_mus_reader::container::ParsedContainer& parsed, SourceProfile profile, ImportReport& report)
{
    Spacing* spacing = nullptr;
    const auto document = makeDocument(&spacing);
    profile.byteOrder = parsed.byteOrder;
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const auto index = LegacyRecordIndex::build(parsed);
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, document, report, pending, construction};
    finale_mus_reader::options::importMusicSpacingOptions(context);
    return document->getOptions()->get<Spacing>();
}

TEST_CASE("Spacing tail starts at the precise 2005 format boundary", "[class]")
{
    for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        const auto parsed = makeContainer({{GLOBALS_CMPER, "94", {0, 222, 1234, 37, 25, 0}}, {GLOBALS_CMPER, "94", {0, 1024, 0, 84, 0, 16180}},
                                              {GLOBALS_CMPER, "94", {0, 117, 9, 1, 1, 0}}},
            FormatEpoch::DclLegacy, order);
        for (const auto build : {3, 4}) {
            auto profile = profileFor(10);
            profile.epoch = FormatEpoch::DclLegacy;
            profile.version->build = static_cast<std::uint8_t>(build);
            ImportReport report(profile.epoch);
            const auto spacing = importSpacingForTest(parsed, profile, report);
            CHECK(spacing->minDistance == 37);
            CHECK(spacing->defaultAllotment == (build == 3 ? 84 : 117));
            CHECK(spacing->minDistGrace == (build == 3 ? 12 : 65545));
            CHECK(spacing->graceNoteSpacing == (build == 3 ? Spacing::GraceNoteSpacing::Automatic : Spacing::GraceNoteSpacing::KeepCurrent));
        }
    }
}

TEST_CASE("Older spacing family wins without borrowing the newer tail", "[class]")
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            const std::vector<SyntheticRow> rows{
                {GLOBALS_CMPER, "89", {0, 222, 1234, 37, 25, 0}},
                {GLOBALS_CMPER, "89", {0, 1024, 0, 84, 0, 16180}},
                {GLOBALS_CMPER, "89", {0, 0, 0, 0, 0, 0}},
                {GLOBALS_CMPER, "94", {0, 444, 1234, 55, 25, 0}},
                {GLOBALS_CMPER, "94", {0, 1024, 0, 84, 0, 16180}},
                {GLOBALS_CMPER, "94", {0, 117, 9, 1, 1, 0}},
            };
            const auto parsed =
                epoch == FormatEpoch::ZlibLegacy
                    ? makeClassContainer(
                          {{finale_mus_reader::numericGlobalClass(89), {0, 222, 1234, 37, 25, 0, 0, 1024, 0, 84, 0, 16180, 0}},
                              {finale_mus_reader::numericGlobalClass(94), {0, 444, 1234, 55, 25, 0, 0, 1024, 0, 84, 0, 16180, 0, 117, 9, 1, 1}}},
                          order)
                    : makeContainer(rows, epoch, order);
            auto profile = profileFor(12);
            profile.epoch = epoch;
            ImportReport report(epoch);
            const auto spacing = importSpacingForTest(parsed, profile, report);
            CHECK(spacing->minWidth == 222);
            CHECK(spacing->minDistance == 37);
            CHECK(spacing->defaultAllotment == 84);
            CHECK(field(report, "options.musicSpacing.defaultAllotment").origin == ValueOrigin::LegacyBehavior);
            CHECK(spacing->minDistGrace == 12);
            CHECK(spacing->graceNoteSpacing == Spacing::GraceNoteSpacing::Automatic);
        }
    }
}

TEST_CASE("Music spacing reads older settings and the later allotment tail", "[class]")
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            for (const auto selector : {89, 94}) {
                if (selector == 94 && (epoch == FormatEpoch::CodaBanner || epoch == FormatEpoch::UncompressedLegacy)) {
                    continue;
                }
                for (int repair = 0; repair < (selector == 89 ? 3 : 1); ++repair) {
                    std::vector<std::int16_t> words{0, 222, 1234, 17, 25, 0x20e1, 2, 2048, 0, 99, 0, 15000, 0, 117, 9, 1, 1};
                    if (repair != 0) {
                        words[7] = 0;
                        words[8] = repair == 2 ? 2048 : 0;
                        words[9] = 0;
                        words[10] = repair == 2 ? 99 : 0;
                        words[11] = 0;
                        words[12] = 0;
                    }
                    std::vector<SyntheticRow> rows;
                    for (std::size_t first = 0; first < words.size(); first += 6) {
                        std::array<std::int16_t, 6> chunk{};
                        for (std::size_t i = 0; i < chunk.size() && first + i < words.size(); ++i) {
                            chunk[i] = words[first + i];
                        }
                        rows.push_back({GLOBALS_CMPER, selector == 89 ? "89" : "94", chunk});
                    }
                    const auto parsed =
                        epoch == FormatEpoch::ZlibLegacy
                            ? makeClassContainer({{finale_mus_reader::numericGlobalClass(static_cast<std::uint16_t>(selector)), words}}, order)
                            : makeContainer(rows, epoch, order);
                    Spacing* spacing = nullptr;
                    const auto document = makeDocument(&spacing);
                    auto profile = profileFor(12);
                    profile.epoch = epoch;
                    profile.byteOrder = order;
                    ImportReport report(epoch);
                    finale_mus_reader::PendingReferences pending;
                    musx::factory::ConstructionContext construction;
                    const auto index = LegacyRecordIndex::build(parsed);
                    const finale_mus_reader::ImportContext context{index, profile, noSource, document, document, report, pending, construction};
                    finale_mus_reader::options::importMusicSpacingOptions(context);
                    CHECK(spacing->minWidth == 222);
                    CHECK(spacing->maxWidth == 1234);
                    CHECK(spacing->useAllottmentTables);
                    CHECK(spacing->referenceDuration == (repair == 1 ? 1024 : 2048));
                    CHECK(spacing->referenceWidth == (repair == 1 ? 84 : 99));
                    CHECK(spacing->scalingFactor == (repair == 0 ? 1.5 : 1.618));
                    CHECK(spacing->defaultAllotment == (selector == 89 ? 84 : 117));
                    CHECK(spacing->minDistGrace == (selector == 89 ? 12 : 65545));
                    CHECK(field(report, "options.musicSpacing.minWidth").origin == ValueOrigin::LegacyMus);
                }
            }
        }
    }
}
} // namespace
} // namespace finale_mus_reader_tests
