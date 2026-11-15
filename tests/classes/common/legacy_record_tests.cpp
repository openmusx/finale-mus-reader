// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <fstream>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

// A class record is its key, payload length, payload, mask length, and mask. A part record
// becomes a part row only where Finale makes it a part object: a shared class has no part
// rows, an unshared class takes no mask, and a mask needs a score row to resolve against.
void testClassRecordMasksAndPartRouting()
{
    const auto verify = [](ByteOrder byteOrder) {
        auto parsed = makeClassContainer(
            {
                SyntheticClassRow{0x00b1, {1, 2, 3, 4, 5, 6}, 1},
                SyntheticClassRow{0x00b1, {11, 12, 13, 14, 15, 16}, 1, 1, true, {0xffff, 0, 0, 0, 0, 0x00ff}},
                SyntheticClassRow{0x00b1, {21, 22}, 2, 1, true, {0xffff}},
                SyntheticClassRow{0x00b1, {7, 8}, 5, 0, true, {0xffff}},
                SyntheticClassRow{0x00d6, {31, 32}, 3},
                SyntheticClassRow{0x00d6, {41, 42}, 3, 2},
                SyntheticClassRow{0x00bb, {1, 2}, 4},
                SyntheticClassRow{0x00bb, {3, 4}, 4, 2, true, {0xffff}},
                SyntheticClassRow{0x00bb, {5, 6}, 4, 3},
            },
            byteOrder);
        const auto index = LegacyRecordIndex::build(parsed);
        const auto& others = index.getClassOthers();

        const auto* part = others.get(0x00b1, 1, 0, 0, 1);
        expectMapping(part != nullptr, "A masked part record with a score record was not kept");
        const auto payload = part ? others.payloadOf(*part) : std::span<const std::uint8_t>{};
        const auto mask = part ? others.maskOf(*part) : std::span<const std::uint8_t>{};
        expectMapping(
            payload.size() == 12 && finale_mus_reader::payloadWord(payload, 0, byteOrder) == 11, "The part payload was not retained physically");
        expectMapping(mask.size() == 12 && finale_mus_reader::payloadWord(mask, 0, byteOrder) == 0xffff
                          && finale_mus_reader::payloadWord(mask, 10, byteOrder) == 0x00ff,
            "The mask was not retained with one word per payload word");
        const auto effective = part ? others.effectivePayloadOf(*part) : std::span<const std::uint8_t>{};
        expectMapping(effective.size() == 12 && finale_mus_reader::payloadWord(effective, 0, byteOrder) == 11
                          && finale_mus_reader::payloadWord(effective, 2, byteOrder) == 2
                          && finale_mus_reader::payloadWord(effective, 10, byteOrder) == 16,
            "The mask did not select part bits over score bits, including in the final word");

        expectMapping(others.get(0x00b1, 2, 0, 0, 1) == nullptr, "A masked part record with no score record was kept");
        const auto* maskedScore = others.get(0x00b1, 5, 0, 0);
        expectMapping(maskedScore && maskedScore->maskSize == 0, "A score record retained a mask");
        expectMapping(others.partIdsForTag(0x00d6) == std::vector<std::uint16_t>{0}, "A part record of a class shared with the score was kept");
        expectMapping(others.partIdsForTag(0x00bb) == std::vector<std::uint16_t>{0, 3},
            "An unshared class kept a masked part record or dropped an unmasked one");
    };

    verify(ByteOrder::BigEndian);
    verify(ByteOrder::LittleEndian);
}

// A mask length that is neither zero nor the payload length is not a record boundary, so the
// block ends there rather than resuming at an arbitrary offset.
void testMismatchedMaskLengthEndsBlock()
{
    auto parsed = makeClassContainer({SyntheticClassRow{0x00b1, {1, 2}, 1}, SyntheticClassRow{0x00d6, {3, 4}, 2}}, ByteOrder::BigEndian);
    // The first record's mask length follows its 10-byte header and 4-byte payload.
    parsed.blocks.front().data[17] = 2;
    const auto index = LegacyRecordIndex::build(parsed);
    expectMapping(index.getClassOthers().empty(), "A record with a mismatched mask length was decoded");
}

void testDetailClassRecordMasks()
{
    const auto verify = [](ByteOrder byteOrder) {
        auto parsed = makeDetailClassContainer(1, 2, 0, {1, 2, 3, 4, 5}, byteOrder, 0x041c);
        auto& data = parsed.blocks.front().data;
        const auto append = [&](std::uint16_t cmper1, std::uint16_t cmper2, std::uint16_t partId, std::uint16_t classId, bool hasMask) {
            const auto next = makeDetailClassContainer(cmper1, cmper2, partId, {11, 12, 13, 14, 15}, byteOrder, classId, hasMask, {0, 0xffff});
            data.insert(data.end(), next.blocks.front().data.begin(), next.blocks.front().data.end());
        };
        append(1, 2, 2, 0x041c, true);
        append(3, 4, 0, 0x0444, false);
        append(3, 4, 2, 0x0444, true);
        parsed.blocks.front().info.decodedSize = data.size();

        const auto index = LegacyRecordIndex::build(parsed);
        const auto& details = index.getClassDetails();
        const auto* part = details.get(0x041c, 1, 2, 0, 2);
        const auto effective = part ? details.effectivePayloadOf(*part) : std::span<const std::uint8_t>{};
        expectMapping(part && details.maskOf(*part).size() == 10 && finale_mus_reader::payloadWord(effective, 0, byteOrder) == 1
                          && finale_mus_reader::payloadWord(effective, 2, byteOrder) == 12,
            "A masked detail part record did not resolve against its score record");
        expectMapping(details.partIdsForTag(0x0444) == std::vector<std::uint16_t>{0}, "An unshared detail class kept a masked part record");
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
    constexpr std::size_t measureSizes[] = {8, 4};
    const auto parsed = makeClassContainer(
        {SyntheticClassRow{0x00b0, std::vector<std::int16_t>(12), 1}, SyntheticClassRow{0x00b0, std::vector<std::int16_t>(2), 1, 1},
            SyntheticClassRow{0x00b0, std::vector<std::int16_t>(4), 2, 1}},
        ByteOrder::BigEndian);
    const auto index = LegacyRecordIndex::build(parsed);
    const finale_mus_reader::RecordFamilySource source{&index.getClassOthers(), 0x00b0, true, false, measureSizes};
    const auto* shortRow = index.getClassOthers().get(0x00b0, 1, 0, 0, 1);
    expectMapping(shortRow && finale_mus_reader::recordShareMode(source, *shortRow) == ShareMode::Partial,
        "A recognized compact part record was not partially shared whatever its score record's size");
    const auto* orphan = index.getClassOthers().get(0x00b0, 2, 0, 0, 1);
    expectMapping(orphan && finale_mus_reader::recordShareMode(source, *orphan) == ShareMode::None,
        "A compact part record with no score record was partially shared");

    auto row = *shortRow;
    row.payloadSize = 6;
    expectMapping(finale_mus_reader::recordShareMode(source, row) == ShareMode::None, "An unrecognized part record size was partially shared");
    row.maskSize = 6;
    expectMapping(finale_mus_reader::recordShareMode(source, row) == ShareMode::Partial, "A masked part record was not partially shared");
    row.partId = musx::dom::SCORE_PARTID;
    expectMapping(finale_mus_reader::recordShareMode(source, row) == ShareMode::All, "A score record was not shared to all parts");
}

void testControlledExpressionUnlinkMasks()
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
    const auto unlinkedMask = unlinkedRow ? unlinked.getClassOthers().maskOf(*unlinkedRow) : std::span<const std::uint8_t>{};
    constexpr std::array<std::uint8_t, 24> expectedUnlinked{
        0, 0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0x40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    expectMapping(unlinkedRow && std::equal(unlinkedMask.begin(), unlinkedMask.end(), expectedUnlinked.begin(), expectedUnlinked.end()),
        "Unlink did not create the expected clear editable mask group");

    const auto moved = indexFor("F2012-noteartexp-unlnk-move.mus");
    const auto* movedRow = moved.getClassOthers().get(0x00b1, 1, 0, 0, 1);
    const auto movedMask = movedRow ? moved.getClassOthers().maskOf(*movedRow) : std::span<const std::uint8_t>{};
    constexpr std::array<std::uint8_t, 24> expectedMoved{
        0, 0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0x40, 0, 0, 0, 0, 0xff, 0xff, 0, 0, 0, 0, 0, 0};
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
        {});
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

TEST_CASE("Class-record masks and part routing", "[class]")
{
    testClassRecordMasksAndPartRouting();
}
TEST_CASE("Class-record mismatched mask length", "[class]")
{
    testMismatchedMaskLengthEndsBlock();
}
TEST_CASE("Detail class-record masks", "[class]")
{
    testDetailClassRecordMasks();
}
TEST_CASE("Class-record sharing modes", "[class]")
{
    testClassRecordSharingModes();
}
TEST_CASE("Controlled expression unlink masks", "[class][reader]")
{
    testControlledExpressionUnlinkMasks();
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
