// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <fstream>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

void testClassRecordContinuationSegment()
{
    const auto verify = [](ByteOrder byteOrder) {
        finale_mus_reader::container::ParsedContainer parsed(FormatEpoch::ZlibLegacy);
        parsed.byteOrder = byteOrder;

        finale_mus_reader::container::DecodedBlock block;
        block.info.type = 0x001a;
        const auto push16 = [&](std::uint16_t value) {
            if (byteOrder == ByteOrder::BigEndian) {
                block.data.push_back(static_cast<std::uint8_t>(value >> 8U));
                block.data.push_back(static_cast<std::uint8_t>(value));
            } else {
                block.data.push_back(static_cast<std::uint8_t>(value));
                block.data.push_back(static_cast<std::uint8_t>(value >> 8U));
            }
        };
        const auto push32 = [&](std::uint32_t value) {
            if (byteOrder == ByteOrder::BigEndian) {
                push16(static_cast<std::uint16_t>(value >> 16U));
                push16(static_cast<std::uint16_t>(value));
            } else {
                push16(static_cast<std::uint16_t>(value));
                push16(static_cast<std::uint16_t>(value >> 16U));
            }
        };
        const auto appendHeader = [&](std::uint16_t classId, std::uint16_t cmper, std::uint16_t partId, std::uint32_t length) {
            push16(classId);
            push16(cmper);
            push16(partId);
            push32(length);
        };

        appendHeader(0x00b1, 1, 1, 12);
        block.data.insert(block.data.end(), {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
        // The continuation occupies the declared byte count including its repeated-size
        // prefix. It belongs to the row, not to another searchable incidence.
        push32(12);
        block.data.insert(block.data.end(), {21, 22, 23, 24, 25, 26, 27, 28});
        push16(0);
        push16(0x1234);

        appendHeader(0x00d6, 3, 0, 4);
        block.data.insert(block.data.end(), {31, 32, 33, 34});
        block.data.insert(block.data.end(), 4, 0);
        block.info.decodedSize = block.data.size();
        parsed.blocks.push_back(std::move(block));

        const auto index = LegacyRecordIndex::build(parsed);
        expectMapping(
            index.getClassOthers().cmpersForTag(0x00b1).empty() && index.getClassOthers().cmpersForTag(0x00b1, 1) == std::vector<std::uint16_t>{1},
            "A part-owned class record leaked into the score comparator list");
        const auto first = index.getClassOthers().get(0x00b1, 1, 0, 0, 1);
        const auto firstPayload = first ? index.getClassOthers().payloadOf(*first) : std::span<const std::uint8_t>{};
        expectMapping(firstPayload.size() == 12 && firstPayload.front() == 1 && firstPayload.back() == 12,
            "A class-record continuation replaced the primary payload");
        const auto continuation = first ? index.getClassOthers().continuationOf(*first) : std::span<const std::uint8_t>{};
        expectMapping(continuation.size() == 12 && continuation[4] == 21 && continuation.back() == 28 && first->trailerFirst == 0
                          && first->trailerSecond == 0x1234,
            "Class-record continuation or terminal words were not retained");
        expectMapping(index.getClassOthers().partIdsForTag(0x00b1) == std::vector<std::uint16_t>{1}, "Class-record source parts were not enumerated");
        const auto following = index.getClassOthers().get(0x00d6, 3, 0, 0);
        const auto followingPayload = following ? index.getClassOthers().payloadOf(*following) : std::span<const std::uint8_t>{};
        expectMapping(followingPayload.size() == 4 && followingPayload.front() == 31 && followingPayload.back() == 34,
            "A class-record continuation stopped the remaining pool");
    };

    verify(ByteOrder::BigEndian);
    verify(ByteOrder::LittleEndian);
}

void testDetailClassRecordContinuationMetadata()
{
    const auto verify = [](ByteOrder byteOrder) {
        finale_mus_reader::container::ParsedContainer parsed(FormatEpoch::ZlibLegacy);
        parsed.byteOrder = byteOrder;

        finale_mus_reader::container::DecodedBlock block;
        block.info.type = 0x001b;
        const auto push16 = [&](std::uint16_t value) {
            if (byteOrder == ByteOrder::BigEndian) {
                block.data.push_back(static_cast<std::uint8_t>(value >> 8U));
                block.data.push_back(static_cast<std::uint8_t>(value));
            } else {
                block.data.push_back(static_cast<std::uint8_t>(value));
                block.data.push_back(static_cast<std::uint8_t>(value >> 8U));
            }
        };
        const auto push32 = [&](std::uint32_t value) {
            if (byteOrder == ByteOrder::BigEndian) {
                push16(static_cast<std::uint16_t>(value >> 16U));
                push16(static_cast<std::uint16_t>(value));
            } else {
                push16(static_cast<std::uint16_t>(value));
                push16(static_cast<std::uint16_t>(value >> 16U));
            }
        };
        const auto appendHeader = [&](std::uint16_t classId, std::uint16_t cmper1, std::uint16_t cmper2, std::uint16_t partId, std::uint32_t length) {
            push16(classId);
            push16(cmper1);
            push16(cmper2);
            push16(partId);
            push32(length);
        };

        appendHeader(0x041c, 1, 2, 2, 12);
        block.data.insert(block.data.end(), {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
        push32(12);
        block.data.insert(block.data.end(), {21, 22, 23, 24, 25, 26, 27, 28});
        push16(0);
        push16(0);

        appendHeader(0x041c, 2, 3, 2, 12);
        block.data.insert(block.data.end(), {41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52});
        push32(12);
        block.data.insert(block.data.end(), {61, 62, 63, 64, 65, 66, 67, 68});
        push16(1);
        push16(0);

        appendHeader(0x0444, 3, 4, 0, 10);
        block.data.insert(block.data.end(), {31, 32, 33, 34, 35, 36, 37, 38, 39, 40});
        block.data.insert(block.data.end(), 4, 0);
        block.info.decodedSize = block.data.size();
        parsed.blocks.push_back(std::move(block));

        const auto index = LegacyRecordIndex::build(parsed);
        const auto* zeroMetadata = index.getClassDetails().get(0x041c, 1, 2, 0, 2);
        const auto zeroContinuation = zeroMetadata ? index.getClassDetails().continuationOf(*zeroMetadata) : std::span<const std::uint8_t>{};
        expectMapping(zeroContinuation.size() == 12 && zeroContinuation[4] == 21 && zeroContinuation.back() == 28 && zeroMetadata->trailerFirst == 0
                          && zeroMetadata->trailerSecond == 0,
            "A continued detail record did not retain its zero metadata words");
        const auto* nonzeroMetadata = index.getClassDetails().get(0x041c, 2, 3, 0, 2);
        const auto nonzeroContinuation = nonzeroMetadata ? index.getClassDetails().continuationOf(*nonzeroMetadata) : std::span<const std::uint8_t>{};
        expectMapping(nonzeroContinuation.size() == 12 && nonzeroContinuation[4] == 61 && nonzeroContinuation.back() == 68
                          && nonzeroMetadata->trailerFirst == 1 && nonzeroMetadata->trailerSecond == 0,
            "A continued detail record did not retain its nonzero metadata word");
        const auto* following = index.getClassDetails().get(0x0444, 3, 4, 0);
        const auto followingPayload = following ? index.getClassDetails().payloadOf(*following) : std::span<const std::uint8_t>{};
        expectMapping(followingPayload.size() == 10 && followingPayload.front() == 31 && followingPayload.back() == 40,
            "A detail continuation consumed the next record header");
    };

    verify(ByteOrder::BigEndian);
    verify(ByteOrder::LittleEndian);
}

void testDetailRowShape()
{
    finale_mus_reader::container::ParsedContainer parsed(FormatEpoch::UncompressedLegacy);
    parsed.byteOrder = ByteOrder::BigEndian;

    finale_mus_reader::container::DecodedBlock block;
    block.info.type = 0x0002;
    const std::vector<std::array<std::int16_t, 8>> detailRows{
        // cmper1, cmper2, packed tag halves, then five payload words
        {7, 9, 'C', 'L', 11, 22, 33, 44},
        {7, 9, 'C', 'L', 55, 66, 77, 88},
        {7, 8, 'C', 'L', 99, 0, 0, 0},
    };
    for (const auto& row : detailRows) {
        const auto push16 = [&](std::uint16_t v) {
            block.data.push_back(static_cast<std::uint8_t>(v >> 8U));
            block.data.push_back(static_cast<std::uint8_t>(v));
        };
        push16(static_cast<std::uint16_t>(row[0]));
        push16(static_cast<std::uint16_t>(row[1]));
        block.data.push_back(static_cast<std::uint8_t>(row[2]));
        block.data.push_back(static_cast<std::uint8_t>(row[3]));
        for (int i = 4; i < 8; ++i) {
            push16(static_cast<std::uint16_t>(row[i]));
        }
        push16(0);
    }
    parsed.blocks.push_back(std::move(block));

    const auto index = LegacyRecordIndex::build(parsed);
    const auto tag = finale_mus_reader::records::packTag("CL");
    const auto family = index.getDetails().getArray(tag, 7, 9);
    expectMapping(family.size() == 2, "Detail family did not group by both comparators");
    expectMapping(family[0].inci == 0 && family[1].inci == 1, "Detail incidences were not assigned in encounter order");
    expectMapping(family[0].wordCount == finale_mus_reader::records::detailWordCount, "Detail rows should carry five payload words");
    expectMapping(family[0].words[0] == 11 && family[1].words[0] == 55, "Detail payload was read from the wrong offset");

    const auto other = index.getDetails().getArray(tag, 7, 8);
    expectMapping(other.size() == 1 && other[0].words[0] == 99, "A different second comparator was not treated as a separate family");
    expectMapping(index.getDetails().get(tag, 7, 9, 1) != nullptr && index.getDetails().get(tag, 7, 9, 2) == nullptr,
        "Detail incidence lookup did not bound correctly");
    expectMapping(index.getDetails().getArray(tag, 1, 1).empty(), "An absent detail family returned rows");
    expectMapping(index.getOthers().empty(), "A details block produced others rows");
}

void testClassRecordSharingModes()
{
    using ShareMode = musx::dom::EnigmaBase::ShareMode;
    constexpr finale_mus_reader::CompactPartLayout measureLayouts[] = {{26, 8}};
    const auto parsed = makeClassContainer(
        {SyntheticClassRow{0x00b0, std::vector<std::int16_t>(13), 1}, SyntheticClassRow{0x00b0, std::vector<std::int16_t>(4), 1, 1}},
        ByteOrder::BigEndian);
    const auto index = LegacyRecordIndex::build(parsed);
    const auto* compactRow = index.getClassOthers().get(0x00b0, 1, 0, 0, 1);
    expectMapping(compactRow != nullptr, "The compact sharing test row was not decoded");
    auto compact = *compactRow;
    finale_mus_reader::RecordFamilySource compactSource{&index.getClassOthers(), compact.tag, true, false, measureLayouts};
    expectMapping(
        finale_mus_reader::recordShareMode(compactSource, compact) == ShareMode::Partial, "A known compact part class was not partially shared");

    compact.tag = 0x00bb;
    compactSource.identity = compact.tag;
    expectMapping(finale_mus_reader::recordShareMode(compactSource, compact) == ShareMode::None, "A standalone full part class was not unshared");

    compact.continuationSize = 24;
    expectMapping(
        finale_mus_reader::recordShareMode(compactSource, compact) == ShareMode::Partial, "A continued part class was not partially shared");

    compact.partId = musx::dom::SCORE_PARTID;
    expectMapping(
        finale_mus_reader::recordShareMode(compactSource, compact) == ShareMode::All, "A continued score class was not shared to all parts");
}

void testControlledExpressionUnlinkContinuations()
{
    const auto indexFor = [](std::string_view name) {
        const auto path = std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F2012" / name;
        std::ifstream input(path, std::ios::binary);
        const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        return LegacyRecordIndex::build(finale_mus_reader::container::parse(bytes.data(), bytes.size()));
    };

    const auto base = indexFor("F2012-noteartexp.mus");
    expectMapping(base.getClassOthers().get(0x00b1, 1, 0, 0, 1) == nullptr, "The linked expression fixture unexpectedly contains a part record");

    const auto unlinked = indexFor("F2012-noteartexp-unlnk.mus");
    const auto* unlinkedRow = unlinked.getClassOthers().get(0x00b1, 1, 0, 0, 1);
    const auto unlinkedMask = unlinkedRow ? unlinked.getClassOthers().continuationOf(*unlinkedRow) : std::span<const std::uint8_t>{};
    constexpr std::array<std::uint8_t, 24> expectedUnlinked{
        0x18, 0, 0, 0, 0, 0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0x40, 0, 0, 0, 0, 0, 0, 0, 0};
    expectMapping(unlinkedRow && std::equal(unlinkedMask.begin(), unlinkedMask.end(), expectedUnlinked.begin(), expectedUnlinked.end()),
        "Unlink did not create the expected clear editable mask group");

    const auto moved = indexFor("F2012-noteartexp-unlnk-move.mus");
    const auto* movedRow = moved.getClassOthers().get(0x00b1, 1, 0, 0, 1);
    const auto movedMask = movedRow ? moved.getClassOthers().continuationOf(*movedRow) : std::span<const std::uint8_t>{};
    constexpr std::array<std::uint8_t, 24> expectedMoved{
        0x18, 0, 0, 0, 0, 0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0x40, 0, 0, 0, 0, 0xff, 0xff, 0, 0};
    const finale_mus_reader::RecordFamilySource movedSource{&moved.getClassOthers(), 0x00b1, true, false, {}};
    expectMapping(movedRow && std::equal(movedMask.begin(), movedMask.end(), expectedMoved.begin(), expectedMoved.end())
                      && finale_mus_reader::recordShareMode(movedSource, *movedRow) == musx::dom::EnigmaBase::ShareMode::Partial,
        "Editing the unlinked expression did not change its retained mask");
}

// The others pool keeps working through the same normalized index, and the word stream is
// still addressed across incidences.
void testOtherRowsRemainSearchable()
{
    const auto parsed = makeContainer({
        {GLOBALS_CMPER, "94", {1, 2, 3, 4, 5, 6}},
        {GLOBALS_CMPER, "94", {7, 8, 9, 10, 11, 12}},
        {3, "LA", {-14, 0, 0, 0, 0, 0}},
    });
    const auto index = LegacyRecordIndex::build(parsed);
    const auto spacing = finale_mus_reader::records::packTag("94");
    expectMapping(index.getOthers().getArray(spacing, GLOBALS_CMPER).size() == 2, "Others family did not group by comparator");
    expectMapping(index.getOthers().cmpersForTag(finale_mus_reader::records::packTag("LA")) == std::vector<std::uint16_t>{3},
        "cmpersForTag did not report the layer comparator");
    const auto straddle = index.word(spacing, GLOBALS_CMPER, 6);
    expectMapping(straddle && straddle->value == 7, "Word addressing did not continue into the next incidence");
    expectMapping(!index.word(spacing, GLOBALS_CMPER, 12), "Word addressing ran past the last incidence");
}

// Tag queries see only their own tag's rows, so neighboring tags, other parts, and other first
// comparators must not leak into a result, and results keep comparator order with the score part first.
void testTagQueriesStayWithinTheirTag()
{
    using finale_mus_reader::records::LegacyRow;
    using finale_mus_reader::records::LegacyRowPool;
    using finale_mus_reader::records::LegacyTag;
    const auto row = [](LegacyTag tag, std::uint16_t partId, std::uint16_t cmper1, std::uint16_t cmper2) {
        LegacyRow result;
        result.tag = tag;
        result.partId = partId;
        result.cmper1 = cmper1;
        result.cmper2 = cmper2;
        return result;
    };
    const auto pool = LegacyRowPool::build(
        {
            row(0x9002, 0, 1, 0),
            row(0x8fff, 0, 4, 0),
            row(0x8001, 3, 5, 6),
            row(0x8001, 0, 5, 7),
            row(0x8001, 0, 2, 9),
            row(0x8001, 0, 5, 6),
            row(0x8001, 0, 5, 6),
            row(0x8000, 0, 1, 1),
            row(0x8003, 2, 8, 0),
        },
        {}, ByteOrder::BigEndian);
    expectMapping(pool.cmpersForTag(0x8001) == std::vector<std::uint16_t>{2, 5}, "cmpersForTag left its tag or part");
    expectMapping(pool.cmpersForTag(0x8001, 3) == std::vector<std::uint16_t>{5}, "cmpersForTag ignored the part");
    expectMapping(pool.cmpersForTag(0x8002).empty(), "cmpersForTag reported an absent tag");
    expectMapping(pool.secondCmpersForTag(0x8001, 5) == std::vector<std::uint16_t>{6, 7}, "secondCmpersForTag left its comparator");
    expectMapping(pool.secondCmpersForTag(0x8001, 5, 3) == std::vector<std::uint16_t>{6}, "secondCmpersForTag ignored the part");
    expectMapping(pool.partIdsForTag(0x8001) == std::vector<std::uint16_t>{0, 3}, "partIdsForTag did not put the score first");
    expectMapping(pool.partIdsForTag(0x8003) == std::vector<std::uint16_t>{2}, "partIdsForTag reported another tag's part");
    expectMapping(pool.tagsInRange(0x8001, 0x8fff) == std::vector<LegacyTag>{0x8001, 0x8003, 0x8fff}, "tagsInRange did not bound its range");
    expectMapping(pool.tagsInRange(0xa001, 0xafff).empty(), "tagsInRange reported an empty range");
}

TEST_CASE("Class-record continuation segment", "[class]")
{
    testClassRecordContinuationSegment();
}
TEST_CASE("Detail class-record continuation metadata", "[class]")
{
    testDetailClassRecordContinuationMetadata();
}
TEST_CASE("Class-record sharing modes", "[class]")
{
    testClassRecordSharingModes();
}
TEST_CASE("Controlled expression unlink continuations", "[class][reader]")
{
    testControlledExpressionUnlinkContinuations();
}
TEST_CASE("Detail row shape", "[class]")
{
    testDetailRowShape();
}
TEST_CASE("Other rows remain searchable", "[class]")
{
    testOtherRowsRemainSearchable();
}
TEST_CASE("Tag queries stay within their tag", "[class]")
{
    testTagQueriesStayWithinTheirTag();
}

} // namespace
} // namespace finale_mus_reader_tests
