// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "container/mus_container.h"
#include "musx/dom/Fundamentals.h"

namespace finale_mus_reader {
namespace records {

/// @brief What identifies a record's type.
/// @details Through Finale 2006 this is the two-character tag packed into 16 bits. From 2007
/// it is the numeric class id that replaced the tag, standing in for what EnigmaXML names as
/// an element. Both are 16-bit, so one field serves every era and a mapping table simply
/// states the identity it wants.
using LegacyTag = std::uint16_t;

/// @brief Packs a two-character tag, so lookups compare an integer rather than a string.
/// @details The packed form is always in logical order, first character in the high byte,
/// whatever byte order the file used to store it.
[[nodiscard]] constexpr LegacyTag packTag(std::string_view tag)
{
    return static_cast<LegacyTag>((static_cast<unsigned char>(tag[0]) << 8U) | static_cast<unsigned char>(tag[1]));
}

/// @brief Unpacks a tag back into its two characters, for diagnostics and reporting.
[[nodiscard]] constexpr std::array<char, 2> tagChars(LegacyTag tag)
{
    return {static_cast<char>(tag >> 8U), static_cast<char>(tag & 0xffU)};
}

/// @brief Unpacks a tag as a readable string, for diagnostics and reporting.
[[nodiscard]] inline std::string tagText(LegacyTag tag)
{
    const auto chars = tagChars(tag);
    return std::string(chars.data(), chars.size());
}

/// @brief One normalized record row, whatever epoch it came from.
/// @details Through Finale 2006 every pool row is 16 bytes in one of two shapes. An other is
/// a comparator, a tag, and six payload words. A detail carries a second comparator, which
/// displaces its tag by two bytes and leaves five payload words. Normalizing both into this
/// struct lets mapping tables address either without knowing the epoch or the pool.
struct LegacyRow
{
    LegacyTag tag{};
    /// @brief Source part that owns the record; zero is the score.
    /// @details Variable zlib records carry this in their header. Earlier normalized pools
    /// currently contain score records only and therefore leave it zero.
    std::uint16_t partId{};
    std::uint16_t cmper1{};
    /// @brief Always zero for an others row.
    std::uint16_t cmper2{};
    std::uint32_t inci{};
    /// @brief Payload words in source order, six for an other and five for a detail.
    /// @details Numeric fields are byte-order corrected, so these are logical values.
    std::array<std::int16_t, 6> words{};
    /// @brief The payload as raw bytes, held by the owning pool.
    /// @details Character payloads are not byte-order sensitive: a little-endian file stores
    /// a font name as plain text, so reading it through @ref words would transpose every
    /// character pair. Text fields must read these bytes instead. The 2007 encoding has a
    /// length-governed payload rather than six fixed words, so the bytes are the only
    /// representation common to every era.
    std::uint32_t payloadOffset{};
    std::uint32_t payloadSize{};
    /// @brief Same-sized continuation bytes following a zlib primary payload, if present.
    /// @details The repeated length prefix remains part of this span. Its remaining bytes are
    /// bit masks selecting the part payload bits that replace the corresponding score bits.
    std::uint32_t continuationOffset{};
    std::uint32_t continuationSize{};
    /// @brief Whether a continued part row has a structurally compatible score payload.
    bool continuationOverlayReady{};
    /// @brief Offset of this row's resolved bytes within the pool's effective-part storage.
    std::uint32_t effectivePayloadOffset{};
    /// @brief Size of the resolved payload, which may differ from the physical part payload.
    std::uint32_t effectivePayloadSize{};
    /// @brief The two words terminating a zlib class record.
    std::int16_t trailerFirst{};
    std::int16_t trailerSecond{};
    std::uint8_t wordCount{};
    std::size_t blockOffset{};
    std::size_t decodedOffset{};
};

/// @brief Payload words and bytes in one fixed-width other or detail incidence.
inline constexpr std::uint8_t otherWordCount = 6;
inline constexpr std::uint8_t detailWordCount = 5;
inline constexpr std::size_t otherInciByteCount = otherWordCount * sizeof(std::int16_t);
inline constexpr std::size_t detailInciByteCount = detailWordCount * sizeof(std::int16_t);

/// @brief One searchable pool of normalized rows.
/// @details Rows are held in a single sorted vector and found by binary search, so a lookup
/// allocates nothing and a record family occupies a contiguous range. Accessors mirror
/// musxdom's object pools: @ref get returns one incidence, @ref getArray returns the family.
class LegacyRowPool
{
public:
    /// @brief Sorts the rows and assigns incidences within each family.
    /// @param payload Backing bytes that the rows index into.
    static LegacyRowPool build(std::vector<LegacyRow> rows, std::vector<std::uint8_t> payload, ByteOrder byteOrder);

    /// @brief Returns a row's payload bytes in file order.
    [[nodiscard]] std::span<const std::uint8_t> payloadOf(const LegacyRow& row) const
    {
        return std::span<const std::uint8_t>(m_payload.data() + row.payloadOffset, row.payloadSize);
    }

    /// @brief Returns payload bytes with a continued part row resolved against its score row.
    /// @details Physical payloads remain available through @ref payloadOf. For a continued part,
    /// set continuation bits select the stored part bits and clear bits retain the score bits.
    [[nodiscard]] std::span<const std::uint8_t> effectivePayloadOf(const LegacyRow& row) const;

    /// @brief Returns a zlib row's uninterpreted continuation bytes, or an empty span.
    [[nodiscard]] std::span<const std::uint8_t> continuationOf(const LegacyRow& row) const
    {
        if (row.continuationSize == 0) {
            return {};
        }
        return std::span<const std::uint8_t>(m_payload.data() + row.continuationOffset, row.continuationSize);
    }

    /// @brief Returns every row of a family, in incidence order.
    [[nodiscard]] std::span<const LegacyRow> getArray(
        LegacyTag tag, std::uint16_t cmper1, std::uint16_t cmper2 = 0, std::uint16_t partId = musx::dom::SCORE_PARTID) const;

    /// @brief Returns one incidence of a family, or nullptr when it is absent.
    [[nodiscard]] const LegacyRow* get(
        LegacyTag tag, std::uint16_t cmper1, std::uint16_t cmper2, std::uint32_t inci, std::uint16_t partId = musx::dom::SCORE_PARTID) const;

    /// @brief Returns every distinct first comparator carried by a tag and part.
    [[nodiscard]] std::vector<std::uint16_t> cmpersForTag(LegacyTag tag, std::uint16_t partId = musx::dom::SCORE_PARTID) const;

    /// @brief Returns every distinct second comparator for a tag, part, and first comparator.
    [[nodiscard]] std::vector<std::uint16_t> secondCmpersForTag(
        LegacyTag tag, std::uint16_t cmper1, std::uint16_t partId = musx::dom::SCORE_PARTID) const;

    /// @brief Returns every distinct source part carried by a tag, score first.
    [[nodiscard]] std::vector<std::uint16_t> partIdsForTag(LegacyTag tag) const;

    /// @brief Returns every distinct tag from @p first through @p last that carries a row, in ascending order.
    [[nodiscard]] std::vector<LegacyTag> tagsInRange(LegacyTag first, LegacyTag last) const;

    [[nodiscard]] bool empty() const { return m_rows.empty(); }
    [[nodiscard]] std::size_t size() const { return m_rows.size(); }

private:
    /// @brief Returns the contiguous rows whose tags lie from @p first through @p last.
    [[nodiscard]] std::span<const LegacyRow> rowsForTags(LegacyTag first, LegacyTag last) const;

    std::vector<LegacyRow> m_rows;
    std::vector<std::uint8_t> m_payload;
    std::vector<std::uint8_t> m_effectivePartPayloads;
};

/// @brief One payload word together with the provenance of the row that held it.
struct RecordWord
{
    std::int16_t value{};
    std::size_t blockOffset{};
    std::size_t decodedOffset{};
};

/// @brief One physical entry row from a fixed-width entry pool.
struct EntryRow
{
    std::array<std::uint8_t, 38> bytes{};
    std::size_t blockOffset{};
    std::size_t decodedOffset{};
};

/// @brief One physical entry row from a Coda entry pool.
struct CodaEntryRow
{
    std::array<std::uint8_t, 32> bytes{};
    std::size_t blockOffset{};
    std::size_t decodedOffset{};
    std::uint32_t number{};
};

/// @brief The normalized, searchable record set for one source file.
/// @details Pools are reached the way musxdom's document reaches its own: @ref getOthers and
/// @ref getDetails. Mapping tables consume this rather than the container, so a new epoch is
/// added by teaching @ref build to produce rows, not by changing any table.
class LegacyRecordIndex
{
public:
    [[nodiscard]] static LegacyRecordIndex build(const container::ParsedContainer& parsed);

    /// @brief Tagged 16-byte others rows, for every epoch through Finale 2006.
    [[nodiscard]] const LegacyRowPool& getOthers() const { return m_others; }
    /// @brief Tagged 16-byte details rows, for every epoch through Finale 2006.
    [[nodiscard]] const LegacyRowPool& getDetails() const { return m_details; }
    /// @brief Class-identified variable-length others records, for Finale 2007 and later.
    /// @details A separate pathway on purpose. The 2007 encoding shares the logical model of
    /// class, comparator, part, and an incidence stream, but nothing of the physical one:
    /// records are length-governed byte payloads rather than six fixed words, so a mapping
    /// table addresses them by byte offset and must say which encoding it targets.
    [[nodiscard]] const LegacyRowPool& getClassOthers() const { return m_classOthers; }
    /// @brief Class-identified variable-length detail records, for Finale 2007 and later.
    /// @details Finale reuses numeric class ids between physical pools, so details must not
    /// share a searchable pool with others even though their record framing is related.
    [[nodiscard]] const LegacyRowPool& getClassDetails() const { return m_classDetails; }

    /// @brief The document's text pool as one uninterrupted byte stream.
    /// @details The text pool is the one pre-2007 pool that is not made of records. It holds
    /// `^keyword(n) ... ^end` chunks of Enigma text end to end, so a reader walks it as a
    /// string rather than indexing it by comparator. The four epochs put it in four different
    /// block types and change nothing else about it, which is why one accessor serves all of
    /// them.
    ///
    /// Empty for a Coda-banner document. That era keeps its text after the last record pool
    /// rather than in a block, and the container does not report the region, so the epoch is
    /// deliberately uncovered here rather than silently mis-sliced.
    [[nodiscard]] std::span<const std::uint8_t> getTexts() const { return m_texts; }

    /// @brief Fixed 38-byte entry rows from the uncompressed, DCL, and zlib epochs.
    [[nodiscard]] std::span<const EntryRow> getEntryRows() const { return m_entryRows; }
    [[nodiscard]] std::span<const CodaEntryRow> getCodaEntryRows() const { return m_codaEntryRows; }
    [[nodiscard]] std::size_t unsupportedEntryBlockCount() const { return m_unsupportedEntryBlocks; }

    /// @brief Reads one word of an others family as a continuous stream across incidences.
    /// @param wordIndex Absolute index, `incidence * 6 + slot`. Addressing the family as one
    /// stream is what lets a four-byte value straddle an incidence boundary.
    [[nodiscard]] std::optional<RecordWord> word(LegacyTag tag, std::uint16_t cmper, std::size_t wordIndex) const;

    [[nodiscard]] bool empty() const
    {
        return m_others.empty() && m_details.empty() && m_classOthers.empty() && m_classDetails.empty() && m_texts.empty();
    }

private:
    LegacyRowPool m_others;
    LegacyRowPool m_details;
    LegacyRowPool m_classOthers;
    LegacyRowPool m_classDetails;
    std::vector<EntryRow> m_entryRows;
    std::vector<CodaEntryRow> m_codaEntryRows;
    std::size_t m_unsupportedEntryBlocks{};
    std::vector<std::uint8_t> m_texts;
};

} // namespace records
} // namespace finale_mus_reader
