// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "records/legacy_record_index.h"

#include <algorithm>
#include <limits>
#include <tuple>
#include <utility>

#include "musx/util/Logger.h"

namespace finale_mus_reader {
namespace records {
namespace {

constexpr std::size_t rowSize = 16;

auto familyKey(const LegacyRow& row)
{
    return std::tie(row.tag, row.cmper1, row.cmper2, row.partId);
}

std::int16_t readWord(const std::uint8_t* data, ByteOrder byteOrder)
{
    if (byteOrder == ByteOrder::BigEndian) {
        return static_cast<std::int16_t>((static_cast<std::uint16_t>(data[0]) << 8U) | data[1]);
    }
    return static_cast<std::int16_t>(data[0] | (static_cast<std::uint16_t>(data[1]) << 8U));
}

std::uint16_t readCmper(const std::uint8_t* data, ByteOrder byteOrder)
{
    return static_cast<std::uint16_t>(readWord(data, byteOrder));
}

std::uint32_t readLong(const std::uint8_t* data, ByteOrder byteOrder)
{
    if (byteOrder == ByteOrder::BigEndian) {
        return (static_cast<std::uint32_t>(data[0]) << 24U) | (static_cast<std::uint32_t>(data[1]) << 16U)
               | (static_cast<std::uint32_t>(data[2]) << 8U) | data[3];
    }
    return data[0] | (static_cast<std::uint32_t>(data[1]) << 8U) | (static_cast<std::uint32_t>(data[2]) << 16U)
           | (static_cast<std::uint32_t>(data[3]) << 24U);
}

// Through Finale 2006 the pools appear in a fixed order and only the framing differs, so a
// block type selects a pool the same way in every epoch. Coda-banner pools are reported
// under the uncompressed numbering by the container.
struct PoolTypes
{
    std::uint16_t others{};
    std::uint16_t details{};
};

std::optional<PoolTypes> poolTypesFor(FormatEpoch epoch)
{
    switch (epoch) {
    case FormatEpoch::CodaBanner:
    case FormatEpoch::UncompressedLegacy: return PoolTypes{0x0001, 0x0002};
    case FormatEpoch::DclLegacy: return PoolTypes{0x000f, 0x0010};
    case FormatEpoch::ZlibLegacy: break;
    }
    return std::nullopt;
}

// The text pool follows the record pools in every epoch and keeps the same contents, so only
// its block number changes. It is the fourth typed block where the pre-2007 eras number their
// pools from one, and 0x0017 in the zlib era, which numbers its blocks differently and puts
// entries and texts outside the class-record range entirely.
//
// A Coda-banner document reports nothing here on purpose. Its text region sits after the last
// record pool as length-prefixed chunks rather than inside a block, and the container's pool
// walk stops before reaching it, so there is no block to name. Returning nothing is what keeps
// that era honestly uncovered instead of reading someone else's bytes as text.
std::optional<std::uint16_t> textBlockTypeFor(FormatEpoch epoch)
{
    switch (epoch) {
    case FormatEpoch::UncompressedLegacy: return std::uint16_t(0x0004);
    case FormatEpoch::DclLegacy: return std::uint16_t(0x0012);
    case FormatEpoch::ZlibLegacy: return std::uint16_t(0x0017);
    case FormatEpoch::CodaBanner: break;
    }
    return std::nullopt;
}

std::vector<std::uint8_t> collectTexts(const container::ParsedContainer& parsed)
{
    std::vector<std::uint8_t> result;
    // The Coda-banner epoch keeps its text outside the pool chain rather than in a block, so
    // the container hands it over separately. Everything downstream sees one text stream
    // whichever way the era stores it.
    if (parsed.formatEpoch == FormatEpoch::CodaBanner) {
        return parsed.textRegion;
    }
    const auto type = textBlockTypeFor(parsed.formatEpoch);
    if (!type) {
        return result;
    }
    for (const auto& block : parsed.blocks) {
        if (block.info.type == *type) {
            result.insert(result.end(), block.data.begin(), block.data.end());
        }
    }
    return result;
}

// An other is cmper, tag, six words. A detail carries a second cmper, which pushes the tag
// two bytes along and leaves five words.
std::vector<LegacyRow> decodeRows(
    const container::ParsedContainer& parsed, std::uint16_t blockType, bool isDetail, std::vector<std::uint8_t>& payload)
{
    std::vector<LegacyRow> result;
    for (const auto& block : parsed.blocks) {
        if (block.info.type != blockType) {
            continue;
        }
        // Entry pools are not this shape and are frequently not a multiple of the row size,
        // so a partial trailing row is ignored rather than treated as an error.
        for (std::size_t offset = 0; offset + rowSize <= block.data.size(); offset += rowSize) {
            const auto* row = block.data.data() + offset;
            LegacyRow decoded;
            decoded.cmper1 = readCmper(row, parsed.byteOrder);
            const std::size_t tagOffset = isDetail ? 4 : 2;
            if (isDetail) {
                decoded.cmper2 = readCmper(row + 2, parsed.byteOrder);
            }
            // The tag is stored as a 16-bit value, so a little-endian file holds "FN" as
            // the bytes "NF". Reading the bytes literally mismatches every tag in such a
            // file and does so silently, because the rows still look structurally valid.
            // The two characters are taken in the file's order and repacked logically, so
            // everything above this point sees a tag that reads as text.
            const bool bigEndian = parsed.byteOrder == ByteOrder::BigEndian;
            const std::array<char, 2> tagBytes{
                static_cast<char>(row[tagOffset + (bigEndian ? 0 : 1)]), static_cast<char>(row[tagOffset + (bigEndian ? 1 : 0)])};
            decoded.tag = packTag(std::string_view(tagBytes.data(), tagBytes.size()));
            decoded.wordCount = isDetail ? detailWordCount : otherWordCount;
            const auto* source = row + tagOffset + 2;
            for (std::uint8_t i = 0; i < decoded.wordCount; ++i) {
                decoded.words[i] = readWord(source + i * 2, parsed.byteOrder);
            }
            decoded.payloadOffset = static_cast<std::uint32_t>(payload.size());
            decoded.payloadSize = static_cast<std::uint32_t>(decoded.wordCount * 2);
            payload.insert(payload.end(), source, source + decoded.payloadSize);
            decoded.blockOffset = block.info.sourceOffset;
            decoded.decodedOffset = offset;
            result.push_back(decoded);
        }
    }
    return result;
}

// The 2007 encoding replaces the fixed row with streams of self-describing records. Other
// records in block 0x001a carry class, cmper1, part id, and a 32-bit payload length. Detail
// records in block 0x001b retain the pre-zlib second comparator between cmper1 and part id;
// the endian-specific length geometry is handled below. Incidence arrays remain in each
// class-specific payload rather than becoming a header dimension.
// Class 0x041d establishes that the second comparator is the measure. Its payload retains the
// fixed-detail representation: an 18-word graphic assignment rounded up to four five-word rows.
std::vector<LegacyRow> decodeClassRecords(const container::ParsedContainer& parsed, bool details, std::vector<std::uint8_t>& payload)
{
    constexpr std::uint16_t otherRecordBlockType = 0x001a;
    constexpr std::uint16_t detailRecordBlockType = 0x001b;
    constexpr std::size_t otherHeaderSize = 10;
    constexpr std::size_t detailHeaderSize = 12;
    constexpr std::size_t trailerSize = 4;

    std::vector<LegacyRow> result;
    for (const auto& block : parsed.blocks) {
        const bool isDetail = block.info.type == detailRecordBlockType;
        if (block.info.type != (details ? detailRecordBlockType : otherRecordBlockType)) {
            continue;
        }
        // Detail headers retain both comparators and the part id before their 32-bit length,
        // making them one word wider than other headers in either byte order.
        const auto headerSize = isDetail ? detailHeaderSize : otherHeaderSize;
        std::size_t offset = 0;
        while (offset + headerSize <= block.data.size()) {
            const auto* header = block.data.data() + offset;
            const auto classId = static_cast<std::uint16_t>(readWord(header, parsed.byteOrder));
            const auto length = readLong(header + (isDetail ? 8U : 6U), parsed.byteOrder);
            if (classId == 0 || length > block.data.size() - offset - headerSize) {
                break;
            }

            const auto primaryEnd = offset + headerSize + length;
            auto trailerOffset = primaryEnd;
            // Some records carry a same-sized continuation segment. Its first four bytes
            // repeat the payload length in the file's byte order. It has no independent
            // class/key header, so the normalized row retains it with its primary payload
            // and advances across the segment.
            bool hasContinuation = false;
            if (length >= 4 && length <= block.data.size() - primaryEnd && readLong(block.data.data() + primaryEnd, parsed.byteOrder) == length) {
                trailerOffset += length;
                hasContinuation = true;
            }
            if (trailerOffset + trailerSize > block.data.size()) {
                break;
            }
            const auto trailerFirst = readWord(block.data.data() + trailerOffset, parsed.byteOrder);
            const auto trailerSecond = readWord(block.data.data() + trailerOffset + 2, parsed.byteOrder);
            // A continued record repurposes its trailer words as metadata, whereas
            // ordinary records retain the two-word zero trailer. The repeated length
            // above states which form applies, so the metadata need not be interpreted
            // to find the next record.
            if (!hasContinuation && (trailerFirst != 0 || trailerSecond != 0)) {
                break;
            }
            LegacyRow decoded;
            decoded.tag = classId;
            decoded.cmper1 = readCmper(header + 2, parsed.byteOrder);
            if (isDetail) {
                decoded.cmper2 = readCmper(header + 4, parsed.byteOrder);
                decoded.partId = readCmper(header + 6, parsed.byteOrder);
            } else {
                decoded.partId = readCmper(header + 4, parsed.byteOrder);
            }
            decoded.payloadOffset = static_cast<std::uint32_t>(payload.size());
            decoded.payloadSize = length;
            const auto* body = header + headerSize;
            payload.insert(payload.end(), body, body + length);
            if (hasContinuation) {
                decoded.continuationOffset = static_cast<std::uint32_t>(payload.size());
                decoded.continuationSize = length;
                payload.insert(payload.end(), block.data.begin() + primaryEnd, block.data.begin() + trailerOffset);
            }
            decoded.trailerFirst = trailerFirst;
            decoded.trailerSecond = trailerSecond;
            decoded.blockOffset = block.info.sourceOffset;
            decoded.decodedOffset = offset;
            result.push_back(decoded);

            offset = trailerOffset + trailerSize;
        }
    }
    return result;
}

} // namespace

LegacyRowPool LegacyRowPool::build(std::vector<LegacyRow> rows, std::vector<std::uint8_t> payload, ByteOrder byteOrder)
{
    // Sorting by family keeps each family contiguous, so a lookup is a binary search rather
    // than a hashed allocation. Fixed rows derive incidence from encounter order. A zlib class
    // record normally holds the complete incidence array in its payload, so its row-level
    // incidence is likewise zero unless several physical records share one complete identity.
    //
    // Incidence is defined by encounter order, so decode order is carried into the sort key
    // rather than left to the algorithm's stability. Relying on std::stable_sort would work
    // but would silently break if the comparator were ever reused with std::sort.
    for (std::size_t i = 0; i < rows.size(); ++i) {
        rows[i].inci = static_cast<std::uint32_t>(i);
    }
    std::sort(rows.begin(), rows.end(), [](const LegacyRow& left, const LegacyRow& right) {
        return std::tuple_cat(familyKey(left), std::tie(left.inci)) < std::tuple_cat(familyKey(right), std::tie(right.inci));
    });
    std::uint32_t inci = 0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (i != 0 && familyKey(rows[i]) == familyKey(rows[i - 1])) {
            ++inci;
        } else {
            inci = 0;
        }
        rows[i].inci = inci;
    }

    LegacyRowPool result;
    result.m_rows = std::move(rows);
    result.m_payload = std::move(payload);
    constexpr std::size_t continuationPrefixSize = 4;
    for (auto& row : result.m_rows) {
        if (row.partId == musx::dom::SCORE_PARTID || row.continuationSize == 0) {
            continue;
        }
        const auto* score = result.get(row.tag, row.cmper1, row.cmper2, row.inci, musx::dom::SCORE_PARTID);
        if (!score || row.continuationSize != row.payloadSize || row.continuationSize < continuationPrefixSize) {
            continue;
        }
        const auto partPayload = result.payloadOf(row);
        const auto scorePayload = result.payloadOf(*score);
        const auto continuation = result.continuationOf(row);
        row.effectivePayloadOffset = static_cast<std::uint32_t>(result.m_effectivePartPayloads.size());
        result.m_effectivePartPayloads.insert(result.m_effectivePartPayloads.end(), scorePayload.begin(), scorePayload.end());
        row.effectivePayloadSize = (std::max)(row.payloadSize, score->payloadSize);
        result.m_effectivePartPayloads.resize(row.effectivePayloadOffset + row.effectivePayloadSize, 0);
        auto* effective = result.m_effectivePartPayloads.data() + row.effectivePayloadOffset;
        for (std::size_t offset = 0; offset < continuation.size() - continuationPrefixSize; ++offset) {
            const auto mask = continuation[continuationPrefixSize + offset];
            effective[offset] = static_cast<std::uint8_t>((effective[offset] & ~mask) | (partPayload[offset] & mask));
        }
        // The two terminal words extend the editable mask across the part payload's final
        // four bytes, which the continuation uses for its length prefix.
        for (std::size_t i = 0; i < continuationPrefixSize; ++i) {
            const auto maskWord = static_cast<std::uint16_t>(i < 2 ? row.trailerFirst : row.trailerSecond);
            const auto shift = byteOrder == ByteOrder::BigEndian ? (i % 2 == 0 ? 8U : 0U) : (i % 2 == 0 ? 0U : 8U);
            const auto mask = static_cast<std::uint8_t>(maskWord >> shift);
            const auto offset = row.payloadSize - continuationPrefixSize + i;
            effective[offset] = static_cast<std::uint8_t>((effective[offset] & ~mask) | (partPayload[offset] & mask));
        }
        row.continuationOverlayReady = true;
    }
    return result;
}

std::span<const std::uint8_t> LegacyRowPool::effectivePayloadOf(const LegacyRow& row) const
{
    const bool unresolvedContinuation = row.partId != musx::dom::SCORE_PARTID && row.continuationSize != 0 && !row.continuationOverlayReady;
    MUSX_ASSERT_IF(unresolvedContinuation)
    {
        musx::util::Logger::log(musx::util::Logger::LogLevel::Warning, "A continued part record has no structurally compatible score record.");
        return payloadOf(row);
    }
    if (!row.continuationOverlayReady) {
        return payloadOf(row);
    }
    return std::span<const std::uint8_t>(m_effectivePartPayloads.data() + row.effectivePayloadOffset, row.effectivePayloadSize);
}

std::span<const LegacyRow> LegacyRowPool::getArray(LegacyTag tag, std::uint16_t cmper1, std::uint16_t cmper2, std::uint16_t partId) const
{
    const auto key = std::tie(tag, cmper1, cmper2, partId);
    const auto range = std::equal_range(m_rows.begin(), m_rows.end(), key, [](const auto& left, const auto& right) {
        if constexpr (std::is_same_v<std::decay_t<decltype(left)>, LegacyRow>) {
            return familyKey(left) < right;
        } else {
            return left < familyKey(right);
        }
    });
    if (range.first == range.second) {
        return {};
    }
    return std::span<const LegacyRow>(&*range.first, static_cast<std::size_t>(std::distance(range.first, range.second)));
}

const LegacyRow* LegacyRowPool::get(LegacyTag tag, std::uint16_t cmper1, std::uint16_t cmper2, std::uint32_t inci, std::uint16_t partId) const
{
    const auto family = getArray(tag, cmper1, cmper2, partId);
    const auto found =
        std::lower_bound(family.begin(), family.end(), inci, [](const LegacyRow& row, std::uint32_t value) { return row.inci < value; });
    return found != family.end() && found->inci == inci ? &*found : nullptr;
}

std::span<const LegacyRow> LegacyRowPool::rowsForTags(LegacyTag first, LegacyTag last) const
{
    // Rows sort by tag before any other key, so the rows of a tag range are contiguous.
    const auto begin = std::lower_bound(m_rows.begin(), m_rows.end(), first, [](const LegacyRow& row, LegacyTag tag) { return row.tag < tag; });
    const auto end = std::upper_bound(begin, m_rows.end(), last, [](LegacyTag tag, const LegacyRow& row) { return tag < row.tag; });
    return std::span<const LegacyRow>(begin, end);
}

std::vector<std::uint16_t> LegacyRowPool::cmpersForTag(LegacyTag tag, std::uint16_t partId) const
{
    std::vector<std::uint16_t> result;
    for (const auto& row : rowsForTags(tag, tag)) {
        if (row.partId == partId && (result.empty() || result.back() != row.cmper1)) {
            result.push_back(row.cmper1);
        }
    }
    return result;
}

std::vector<std::uint16_t> LegacyRowPool::secondCmpersForTag(LegacyTag tag, std::uint16_t cmper1, std::uint16_t partId) const
{
    std::vector<std::uint16_t> result;
    for (const auto& row : rowsForTags(tag, tag)) {
        if (row.partId == partId && row.cmper1 == cmper1 && (result.empty() || result.back() != row.cmper2)) {
            result.push_back(row.cmper2);
        }
    }
    return result;
}

std::vector<LegacyTag> LegacyRowPool::tagsInRange(LegacyTag first, LegacyTag last) const
{
    std::vector<LegacyTag> result;
    for (const auto& row : rowsForTags(first, last)) {
        if (result.empty() || result.back() != row.tag) {
            result.push_back(row.tag);
        }
    }
    return result;
}

std::vector<std::uint16_t> LegacyRowPool::partIdsForTag(LegacyTag tag) const
{
    std::vector<std::uint16_t> result;
    for (const auto& row : rowsForTags(tag, tag)) {
        if (std::find(result.begin(), result.end(), row.partId) != result.end()) {
            continue;
        }
        result.push_back(row.partId);
    }
    std::sort(result.begin(), result.end(), [](auto left, auto right) {
        if (left == musx::dom::SCORE_PARTID) {
            return true;
        }
        if (right == musx::dom::SCORE_PARTID) {
            return false;
        }
        return left < right;
    });
    return result;
}

LegacyRecordIndex LegacyRecordIndex::build(const container::ParsedContainer& parsed)
{
    LegacyRecordIndex result;
    if (parsed.formatEpoch == FormatEpoch::CodaBanner) {
        for (const auto& block : parsed.blocks) {
            if (block.info.type != 0x0003) {
                continue;
            }
            if (block.data.size() % CodaEntryRow{}.bytes.size() != 0) {
                ++result.m_unsupportedEntryBlocks;
                continue;
            }
            for (std::size_t offset = 0; offset < block.data.size(); offset += CodaEntryRow{}.bytes.size()) {
                CodaEntryRow row;
                std::copy_n(block.data.begin() + offset, row.bytes.size(), row.bytes.begin());
                row.blockOffset = block.info.sourceOffset;
                row.decodedOffset = offset;
                row.number = static_cast<std::uint32_t>(offset / row.bytes.size());
                result.m_codaEntryRows.push_back(row);
            }
        }
    } else {
        const auto entryType = parsed.formatEpoch == FormatEpoch::UncompressedLegacy ? 0x0003
                               : parsed.formatEpoch == FormatEpoch::DclLegacy        ? 0x0011
                                                                                     : 0x0016;
        for (const auto& block : parsed.blocks) {
            if (block.info.type != entryType) {
                continue;
            }
            if (block.data.size() % EntryRow{}.bytes.size() != 0) {
                ++result.m_unsupportedEntryBlocks;
                continue;
            }
            for (std::size_t offset = 0; offset < block.data.size(); offset += EntryRow{}.bytes.size()) {
                EntryRow row;
                std::copy_n(block.data.begin() + offset, row.bytes.size(), row.bytes.begin());
                row.blockOffset = block.info.sourceOffset;
                row.decodedOffset = offset;
                result.m_entryRows.push_back(row);
            }
        }
    }
    if (const auto types = poolTypesFor(parsed.formatEpoch)) {
        std::vector<std::uint8_t> othersPayload;
        auto othersRows = decodeRows(parsed, types->others, false, othersPayload);
        result.m_others = LegacyRowPool::build(std::move(othersRows), std::move(othersPayload), parsed.byteOrder);

        std::vector<std::uint8_t> detailsPayload;
        auto detailRows = decodeRows(parsed, types->details, true, detailsPayload);
        result.m_details = LegacyRowPool::build(std::move(detailRows), std::move(detailsPayload), parsed.byteOrder);
    } else if (parsed.formatEpoch == FormatEpoch::ZlibLegacy) {
        std::vector<std::uint8_t> othersPayload;
        auto othersRows = decodeClassRecords(parsed, false, othersPayload);
        result.m_classOthers = LegacyRowPool::build(std::move(othersRows), std::move(othersPayload), parsed.byteOrder);

        std::vector<std::uint8_t> detailsPayload;
        auto detailRows = decodeClassRecords(parsed, true, detailsPayload);
        result.m_classDetails = LegacyRowPool::build(std::move(detailRows), std::move(detailsPayload), parsed.byteOrder);
    }
    result.m_texts = collectTexts(parsed);
    return result;
}

std::optional<RecordWord> LegacyRecordIndex::word(LegacyTag tag, std::uint16_t cmper, std::size_t wordIndex) const
{
    const auto family = m_others.getArray(tag, cmper);
    if (family.empty()) {
        return std::nullopt;
    }
    const auto incidence = wordIndex / family.front().wordCount;
    const auto slot = wordIndex % family.front().wordCount;
    if (incidence >= family.size()) {
        return std::nullopt;
    }
    const auto& row = family[incidence];
    return RecordWord{row.words[slot], row.blockOffset, row.decodedOffset};
}

} // namespace records
} // namespace finale_mus_reader
