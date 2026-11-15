// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "records/legacy_record_index.h"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <span>
#include <sstream>
#include <tuple>
#include <utility>

#include "musx/util/Logger.h"

namespace finale_mus_reader {
namespace records {
namespace {

constexpr std::size_t rowSize = 16;

std::string legacyClassIdText(LegacyTag tag)
{
    constexpr int hexDigits = sizeof(LegacyTag) * 2;
    std::ostringstream result;
    result << "0x" << std::hex << std::setfill('0') << std::setw(hexDigits) << tag;
    return result.str();
}

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

// How a zlib class keeps its part data. Every class follows one of three sharing models, and the
// class's model, not the shape of a part record, decides whether that record becomes a part
// object.
enum class PartSharing {
    /// @brief The class has no part objects; every object belongs to the score.
    Score,
    /// @brief A part object is a complete record of its own, with no mask.
    Unshared,
    /// @brief A part object is the score object with the bits its mask selects replaced.
    Partial,
};

// Sorted, for binary search. A class in neither list of its pool is shared with the score.
// Others class 0x00b0 (measure specification) keeps its part data in compact records without a
// mask, so it is listed with the unshared classes.
constexpr std::uint16_t partialOtherClasses[] = {
    0x00b1, 0x00b3, 0x00b4, 0x00b5, 0x00b6, 0x00bc, 0x00bd, 0x00c2, 0x00cb, 0x00cc, 0x00cf, 0x00d1, 0x00d2, 0x00d3, 0x00d4, 0x00d9, 0x00f3};
constexpr std::uint16_t unsharedOtherClasses[] = {0x007a, 0x0093, 0x0098, 0x009f, 0x00af, 0x00b0, 0x00bb, 0x00df, 0x00e9, 0x0120, 0x0121, 0x0143};
constexpr std::uint16_t partialDetailClasses[] = {0x03f1, 0x03f2, 0x03f3, 0x03f4, 0x03f5, 0x03f6, 0x03f7, 0x03f8, 0x03f9, 0x03fa, 0x0407, 0x0418,
    0x0419, 0x041a, 0x041c, 0x041d, 0x041f, 0x0420, 0x0422, 0x0423, 0x0430, 0x0450, 0x0452, 0x0453, 0x0454};
constexpr std::uint16_t unsharedDetailClasses[] = {0x0406, 0x0410, 0x0421, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448, 0x0449, 0x044a, 0x044b};

PartSharing partSharingOf(bool details, std::uint16_t classId)
{
    const auto listed = [classId](std::span<const std::uint16_t> classes) { return std::binary_search(classes.begin(), classes.end(), classId); };
    if (listed(details ? std::span<const std::uint16_t>(partialDetailClasses) : partialOtherClasses)) {
        return PartSharing::Partial;
    }
    if (listed(details ? std::span<const std::uint16_t>(unsharedDetailClasses) : unsharedOtherClasses)) {
        return PartSharing::Unshared;
    }
    return PartSharing::Score;
}

// Whether Finale makes a part record into a part object. A part record of a class shared with
// the score describes no part object, and neither does a masked part record of a class whose
// part objects are not partially shared.
bool keepsPartRecord(bool details, const LegacyRow& row)
{
    if (row.partId == musx::dom::SCORE_PARTID) {
        return true;
    }
    const auto sharing = partSharingOf(details, row.tag);
    const char* reason = sharing == PartSharing::Score                           ? "is shared with the score"
                         : sharing == PartSharing::Unshared && row.maskSize != 0 ? "does not take a part mask"
                                                                                 : nullptr;
    if (!reason) {
        return true;
    }
    musx::util::Logger::log(musx::util::Logger::LogLevel::Info, std::string(details ? "Detail" : "Other") + " class " + legacyClassIdText(row.tag)
                                                                    + " " + reason + "; its record for part " + std::to_string(row.partId)
                                                                    + " was not imported.");
    return false;
}

// The 2007 encoding replaces the fixed row with streams of self-describing records. Other
// records in block 0x001a carry class, cmper1, part id, and a 32-bit payload length. Detail
// records in block 0x001b retain the pre-zlib second comparator between cmper1 and part id.
// Incidence arrays remain in each class-specific payload rather than becoming a header
// dimension.
// Class 0x041d establishes that the second comparator is the measure. Its payload retains the
// fixed-detail representation: an 18-word graphic assignment rounded up to four five-word rows.
//
// The payload is followed by a 32-bit mask length and the mask. A zero length means no mask;
// otherwise the mask is exactly as long as the payload, and any other length ends the block.
// Only a part record's mask has meaning, so a score record's mask is not retained.
std::vector<LegacyRow> decodeClassRecords(const container::ParsedContainer& parsed, bool details, std::vector<std::uint8_t>& payload)
{
    constexpr std::uint16_t otherRecordBlockType = 0x001a;
    constexpr std::uint16_t detailRecordBlockType = 0x001b;
    constexpr std::size_t otherHeaderSize = 10;
    constexpr std::size_t detailHeaderSize = 12;
    constexpr std::size_t maskLengthSize = 4;

    std::vector<LegacyRow> result;
    for (const auto& block : parsed.blocks) {
        if (block.info.type != (details ? detailRecordBlockType : otherRecordBlockType)) {
            continue;
        }
        // Detail headers retain both comparators and the part id before their 32-bit length,
        // making them one word wider than other headers in either byte order.
        const auto headerSize = details ? detailHeaderSize : otherHeaderSize;
        std::size_t offset = 0;
        while (offset + headerSize <= block.data.size()) {
            const auto* header = block.data.data() + offset;
            const auto classId = static_cast<std::uint16_t>(readWord(header, parsed.byteOrder));
            const auto length = readLong(header + headerSize - 4, parsed.byteOrder);
            if (classId == 0 || length > block.data.size() - offset - headerSize) {
                break;
            }
            const auto maskLengthOffset = offset + headerSize + length;
            if (maskLengthSize > block.data.size() - maskLengthOffset) {
                break;
            }
            const auto maskLength = readLong(block.data.data() + maskLengthOffset, parsed.byteOrder);
            const auto maskOffset = maskLengthOffset + maskLengthSize;
            if ((maskLength != 0 && maskLength != length) || maskLength > block.data.size() - maskOffset) {
                break;
            }
            LegacyRow decoded;
            decoded.tag = classId;
            decoded.cmper1 = readCmper(header + 2, parsed.byteOrder);
            if (details) {
                decoded.cmper2 = readCmper(header + 4, parsed.byteOrder);
                decoded.partId = readCmper(header + 6, parsed.byteOrder);
            } else {
                decoded.partId = readCmper(header + 4, parsed.byteOrder);
            }
            decoded.payloadOffset = static_cast<std::uint32_t>(payload.size());
            decoded.payloadSize = length;
            const auto* body = header + headerSize;
            payload.insert(payload.end(), body, body + length);
            if (maskLength != 0 && decoded.partId != musx::dom::SCORE_PARTID) {
                decoded.maskOffset = static_cast<std::uint32_t>(payload.size());
                decoded.maskSize = maskLength;
                payload.insert(payload.end(), block.data.begin() + maskOffset, block.data.begin() + maskOffset + maskLength);
            }
            decoded.blockOffset = block.info.sourceOffset;
            decoded.decodedOffset = offset;
            if (keepsPartRecord(details, decoded)) {
                result.push_back(decoded);
            }
            offset = maskOffset + maskLength;
        }
    }
    return result;
}

} // namespace

LegacyRowPool LegacyRowPool::build(std::vector<LegacyRow> rows, std::vector<std::uint8_t> payload)
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
    const auto unresolvable = [&result](const LegacyRow& row) {
        if (row.maskSize == 0 || result.get(row.tag, row.cmper1, row.cmper2, row.inci, musx::dom::SCORE_PARTID)) {
            return false;
        }
        musx::util::Logger::log(musx::util::Logger::LogLevel::Info, "Class " + legacyClassIdText(row.tag) + " has a masked record for part "
                                                                        + std::to_string(row.partId)
                                                                        + " but no score record of the same identity; it was not imported.");
        return true;
    };
    // Only part rows are removed, so the score rows the predicate looks up stay in place.
    std::erase_if(result.m_rows, unresolvable);

    // A part payload longer than its score is resolved against zero bytes past the score's end;
    // bytes past the part's end keep the score's values.
    for (auto& row : result.m_rows) {
        if (row.maskSize == 0) {
            continue;
        }
        const auto* score = result.get(row.tag, row.cmper1, row.cmper2, row.inci, musx::dom::SCORE_PARTID);
        const auto partPayload = result.payloadOf(row);
        const auto scorePayload = result.payloadOf(*score);
        const auto mask = result.maskOf(row);
        row.effectivePayloadOffset = static_cast<std::uint32_t>(result.m_effectivePartPayloads.size());
        result.m_effectivePartPayloads.insert(result.m_effectivePartPayloads.end(), scorePayload.begin(), scorePayload.end());
        row.effectivePayloadSize = (std::max)(row.payloadSize, score->payloadSize);
        result.m_effectivePartPayloads.resize(row.effectivePayloadOffset + row.effectivePayloadSize, 0);
        auto* effective = result.m_effectivePartPayloads.data() + row.effectivePayloadOffset;
        for (std::size_t offset = 0; offset < mask.size(); ++offset) {
            effective[offset] = static_cast<std::uint8_t>((effective[offset] & ~mask[offset]) | (partPayload[offset] & mask[offset]));
        }
    }
    return result;
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
        result.m_others = LegacyRowPool::build(std::move(othersRows), std::move(othersPayload));

        std::vector<std::uint8_t> detailsPayload;
        auto detailRows = decodeRows(parsed, types->details, true, detailsPayload);
        result.m_details = LegacyRowPool::build(std::move(detailRows), std::move(detailsPayload));
    } else if (parsed.formatEpoch == FormatEpoch::ZlibLegacy) {
        std::vector<std::uint8_t> othersPayload;
        auto othersRows = decodeClassRecords(parsed, false, othersPayload);
        result.m_classOthers = LegacyRowPool::build(std::move(othersRows), std::move(othersPayload));

        std::vector<std::uint8_t> detailsPayload;
        auto detailRows = decodeClassRecords(parsed, true, detailsPayload);
        result.m_classDetails = LegacyRowPool::build(std::move(detailRows), std::move(detailsPayload));
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
