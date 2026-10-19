// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "finale_mus_reader/reader.h"

#include "import/support/deferred_order.h"
#include "import/support/instrument_catalog.h"
#include "import/support/reporting.h"
#include "import/support/soundmaps.h"
#include "import/support/text_encoding.h"
#include "musx/dom/Document.h"
#include "musx/dom/Fundamentals.h"
#include "musx/factory/ConstructionContext.h"
#include "musx/factory/PoolFactory.h"
#include "records/legacy_record_index.h"
#include "support/finale_version.h"

namespace finale_mus_reader {

namespace percussion {
class MappingTables;
struct LegacyConversion;
} // namespace percussion

struct ImportContext;

/// @brief Width of a mapped value in the source record.
enum class ValueWidth : std::uint8_t {
    Byte = 1,
    Word = 2,
    Long = 4
};

/// @brief Interprets a recovered 32-bit value as an IEEE-754 single-precision value.
[[nodiscard]] double legacySinglePrecision(std::int64_t value);

/// @brief Converts a legacy point measurement into musxdom's Efix units.
[[nodiscard]] musx::dom::Efix legacyPointsToEfix(double value);

/// @brief Converts a legacy point measurement stored in ten-thousandths into Efix units.
[[nodiscard]] musx::dom::Efix legacyTenThousandthsPointToEfix(std::int64_t value);

/// @brief Reads a legacy string that occupies a run of consecutive record rows.
/// @details Rows are fixed width, so a string longer than one row continues into the next and
/// the last one is padded: the string ends at the first NUL. Character payloads are not
/// byte-order sensitive, so the bytes are read directly rather than through the decoded words.
/// A little-endian file stores a name as plain text, and reading it through the words would
/// transpose every character pair.
/// @param family The rows of one record family, as @ref records::LegacyRowPool::getArray returns them.
/// @param firstIncidence The incidence the string starts at.
/// @param incidenceCount How many incidences the string may occupy, for a record that keeps
/// something other than the string in the rows that follow it.
[[nodiscard]] std::string readRowText(const records::LegacyRowPool& pool, std::span<const records::LegacyRow> family,
    std::uint32_t firstIncidence = 0, std::uint32_t incidenceCount = (std::numeric_limits<std::uint32_t>::max)());

/// @brief Which of a four-byte value's two payload words comes first.
/// @details Independent of container byte order, which the record index has already
/// applied when reading each individual word.
enum class LongWordOrder : std::uint8_t {
    HighFirst,
    LowFirst
};

/// @brief Returns the long-word order native to the container's byte order.
[[nodiscard]] constexpr LongWordOrder nativeLongWordOrder(ByteOrder byteOrder)
{
    return byteOrder == ByteOrder::BigEndian ? LongWordOrder::HighFirst : LongWordOrder::LowFirst;
}

/// @brief Selects a bit range within the source value. A zero count means the whole value.
struct BitRange
{
    std::uint8_t firstBit{};
    std::uint8_t bitCount{};
};

/// @brief Narrows a decoded value to the bit range a mapping selected.
/// @details A zero bit count returns the value unchanged, which is what makes a whole field and
/// a bit range one code path. The shift is unsigned, so a negative word contributes its stored
/// bits rather than its sign.
[[nodiscard]] constexpr std::int64_t extractBits(std::int64_t value, BitRange bits)
{
    if (bits.bitCount == 0) {
        return value;
    }
    const auto mask = (std::uint64_t{1} << bits.bitCount) - 1U;
    return static_cast<std::int64_t>((static_cast<std::uint64_t>(value) >> bits.firstBit) & mask);
}

/// @brief Which record encoding a table reads, and therefore how its fields are addressed.
enum class RecordEncoding : std::uint8_t {
    /// @brief Tagged 16-byte rows, every epoch through Finale 2006. Fields address a word
    /// slot in the family's incidence stream, so a value may straddle an incidence.
    FixedRow,
    /// @brief Class-identified variable-length records, Finale 2007 and later. Fields
    /// address a byte offset inside a single record's payload.
    ClassRecord
};

/// @brief One compact part representation used by a class-record importer.
/// @details Layout tables live with the importer that decodes the class. Multiple entries permit
/// the same class to use different structural representations within the zlib epoch.
struct CompactPartLayout
{
    std::size_t scorePayloadSize{};
    std::size_t partPayloadSize{};
};

/// @brief One logical record family selected from either fixed rows or class records.
struct RecordFamilySource
{
    const records::LegacyRowPool* pool{};
    records::LegacyTag identity{};
    bool classRecords{};
    bool details{};
    std::span<const CompactPartLayout> compactPartLayouts{};

    /// @brief Returns how many payload words one fixed row of this family holds.
    [[nodiscard]] std::size_t fixedRowWords() const { return details ? records::detailWordCount : records::otherWordCount; }

    /// @brief Returns the row that stores the given word of the family's collected word stream:
    /// the one class record, or the fixed row whose incidence the index falls in.
    [[nodiscard]] const records::LegacyRow& rowOfWord(std::span<const records::LegacyRow> rows, std::size_t wordIndex) const
    {
        return rows[classRecords ? 0 : wordIndex / fixedRowWords()];
    }

    /// @brief Returns the row that stores the given byte of the family's collected payload.
    [[nodiscard]] const records::LegacyRow& rowOfByte(std::span<const records::LegacyRow> rows, std::size_t byteOffset) const
    {
        return rowOfWord(rows, byteOffset / sizeof(std::uint16_t));
    }

    /// @brief Returns the given payload byte's offset within the row that stores it.
    [[nodiscard]] std::size_t byteOffsetInRow(std::size_t byteOffset) const
    {
        return classRecords ? byteOffset : byteOffset % (fixedRowWords() * sizeof(std::uint16_t));
    }
};

/// @brief Assigns consecutive incidences to the objects added under one pool identity.
/// @details Create each target with @ref current and add it with @ref add, the only operation
/// that advances the counter, so a skipped source record leaves no gap. Keep one counter per
/// part and comparator group. A stored incidence that links to another record must be resolved
/// before using this numbering.
class EmittedIncidence
{
public:
    [[nodiscard]] musx::dom::Inci current() const { return m_next; }

    /// @brief Adds a target created with @ref current to the pool and advances the counter.
    /// @throws std::logic_error if the target carries any other incidence.
    template <typename Pool, typename T>
    void add(Pool& pool, std::shared_ptr<T> target)
    {
        if (target->getInci() != m_next) {
            throw std::logic_error(std::string(T::XmlNodeName) + " incidence " + std::to_string(target->getInci().value_or(-1)) + " was added where "
                                   + std::to_string(m_next) + " is next.");
        }
        pool.add(T::XmlNodeName, std::move(target));
        ++m_next;
    }

private:
    musx::dom::Inci m_next{};
};

/// @brief Returns the source parts that a selected family contains, score
/// first.
/// @details Fixed-row epochs currently normalize score data only. Class records
/// state their part in each header, so their actual part set is returned.
[[nodiscard]] std::vector<std::uint16_t> recordPartIds(const RecordFamilySource& source);

/// @brief Returns every part/comparator key stored by a selected record family.
[[nodiscard]] std::vector<std::pair<std::uint16_t, std::uint16_t>> recordKeys(const RecordFamilySource& source);

/// @brief Derives a DOM sharing mode from one normalized record's physical
/// form.
/// @details Score records are always shared to all parts. A part continuation
/// marks a partial instance. Classes with a compact part record are also
/// partial when their importer-provided layout matches; other standalone part records are
/// unshared.
[[nodiscard]] musx::dom::EnigmaBase::ShareMode recordShareMode(const RecordFamilySource& source, const records::LegacyRow& row);

/// @brief Initializes a partially linked part target from the score instance of the same identity.
/// @details That is what partial sharing means: the part record states only what the part changed,
/// and everything else is the score's. musxdom owns that operation, so it is called rather than
/// reproduced. A part record whose score instance has not been created is left uninitialized
/// instead; the record enumeration hands out score records first, so that case is a source whose
/// score record is missing rather than an ordering mistake. Every record target is created through
/// this one path.
template <typename T>
void initializePartialFromScore(const std::shared_ptr<T>& target, const std::shared_ptr<const T>& score)
{
    if (target && score && target->getShareMode() == musx::dom::EnigmaBase::ShareMode::Partial) {
        musx::factory::PartSharingFactory::initializePartial(target, score);
    }
}

/// @brief Creates an others instance with identity and sharing taken from its source row.
/// @details A partially linked part instance is initialized by @ref initializePartialFromScore.
template <typename T>
[[nodiscard]] std::shared_ptr<T> createOthersRecordTarget(const musx::dom::DocumentPtr& document, const RecordFamilySource& source,
    const records::LegacyRow& row, musx::dom::Cmper cmper, musx::dom::Inci inci = 0)
{
    const auto shareMode = recordShareMode(source, row);
    std::shared_ptr<T> target;
    // A class without an incidence is looked up with none, not with incidence zero: the pool
    // distinguishes the two, and asking for zero finds nothing where the objects have no
    // incidence at all.
    std::optional<musx::dom::Inci> lookupInci;
    if constexpr (std::is_constructible_v<T, const musx::dom::DocumentPtr&, std::uint16_t, musx::dom::EnigmaBase::ShareMode, musx::dom::Cmper,
                      musx::dom::Inci>) {
        target = std::make_shared<T>(document, row.partId, shareMode, cmper, inci);
        lookupInci = inci;
    } else {
        target = std::make_shared<T>(document, row.partId, shareMode, cmper);
    }
    if (shareMode == musx::dom::EnigmaBase::ShareMode::Partial) {
        initializePartialFromScore(target, document->getOthers()->template get<T>(musx::dom::SCORE_PARTID, cmper, lookupInci));
    }
    return target;
}

/// @brief Creates a details instance with identity and sharing taken from its
/// source row.
/// @details A partially linked part instance is initialized by @ref initializePartialFromScore.
template <typename T>
[[nodiscard]] std::shared_ptr<T> createDetailsRecordTarget(const musx::dom::DocumentPtr& document, const RecordFamilySource& source,
    const records::LegacyRow& row, musx::dom::Cmper cmper1, musx::dom::Cmper cmper2, musx::dom::Inci inci = 0)
{
    const auto shareMode = recordShareMode(source, row);
    const bool partial = shareMode == musx::dom::EnigmaBase::ShareMode::Partial;
    std::shared_ptr<T> target;
    if constexpr (std::is_base_of_v<musx::dom::EntryDetailsBase, T>) {
        const auto entryNumber = (static_cast<musx::dom::EntryNumber>(cmper1) << 16U) | cmper2;
        target = std::make_shared<T>(document, row.partId, shareMode, entryNumber, inci);
        if (partial) {
            initializePartialFromScore(target, document->getDetails()->template get<T>(musx::dom::SCORE_PARTID, entryNumber, inci));
        }
    } else if constexpr (std::is_constructible_v<T, const musx::dom::DocumentPtr&, std::uint16_t, musx::dom::EnigmaBase::ShareMode, musx::dom::Cmper,
                             musx::dom::Cmper, musx::dom::Inci>) {
        target = std::make_shared<T>(document, row.partId, shareMode, cmper1, cmper2, inci);
        if (partial) {
            initializePartialFromScore(target, document->getDetails()->template get<T>(musx::dom::SCORE_PARTID, cmper1, cmper2, inci));
        }
    } else {
        target = std::make_shared<T>(document, row.partId, shareMode, cmper1, cmper2);
        if (partial) {
            initializePartialFromScore(target, document->getDetails()->template get<T>(musx::dom::SCORE_PARTID, cmper1, cmper2));
        }
    }
    return target;
}

/// @brief Selects a fixed-row family through the DCL epoch and its class-record replacement
/// in the zlib epoch.
[[nodiscard]] std::optional<RecordFamilySource> selectRecordFamilySource(const ImportContext& context, const records::LegacyRowPool& fixedPool,
    const records::LegacyRowPool& classPool, records::LegacyTag fixedTag, records::LegacyTag classId, bool details = false,
    std::span<const CompactPartLayout> compactPartLayouts = {});

/// @brief Collects a record family's payload bytes in incidence order.
[[nodiscard]] std::vector<std::uint8_t> collectRecordPayload(const RecordFamilySource& source, std::span<const records::LegacyRow> rows);

/// @brief Collects a record family into one word stream in incidence order.
[[nodiscard]] std::vector<std::int16_t> collectRecordWords(
    const RecordFamilySource& source, std::span<const records::LegacyRow> rows, ByteOrder byteOrder);

/// @brief Reads one unsigned word from a payload in the container's byte order.
[[nodiscard]] std::uint16_t payloadWord(std::span<const std::uint8_t> payload, std::size_t offset, ByteOrder byteOrder);

/// @brief Reads two payload words as one signed long in their stored word order.
[[nodiscard]] std::int32_t payloadLong(std::span<const std::uint8_t> payload, std::size_t offset, ByteOrder byteOrder, LongWordOrder wordOrder);

/// @brief Reads a fixed-capacity, null-terminated byte string from a payload.
[[nodiscard]] std::string payloadString(std::span<const std::uint8_t> payload, std::size_t offset, std::size_t capacity);

/// @brief Returns a continued part row's link-mask word for one payload word.
/// @details A set bit is unlinked from the score. Returns nothing when the row at @p wordSlot is
/// not a continued part row or its continuation stores no mask for that word.
[[nodiscard]] std::optional<std::uint16_t> partUnlinkedBits(
    const RecordFamilySource& source, std::span<const records::LegacyRow> rows, std::size_t wordSlot, ByteOrder byteOrder);

/// @brief Reports whether Finale's conversion keeps a part's own copy of one record.
/// @details The pre-2010 chord and pre-2011 lyric conversions save a part's record only when
/// its link mask unlinks a bit that the class's link check reads. @p comparedBits lists those
/// bits for each word from @p at. A part row without a mask is a standalone record and is kept.
[[nodiscard]] bool partRecordUnlinked(const RecordFamilySource& source, std::span<const records::LegacyRow> rows, std::size_t at,
    std::span<const std::uint16_t> comparedBits, ByteOrder byteOrder);

/// @brief Where one field lives in the legacy record stream.
struct SourceLocation
{
    /// @brief The record identity: a packed two-character tag, or a numeric class id.
    records::LegacyTag identity{};
    /// @brief The record comparator. Ignored for @ref TargetKind::OthersByCmper tables,
    /// where the comparator of the target object is used instead.
    std::uint16_t selector{};
    std::uint32_t incidence{};
    /// @brief Word slot for @ref RecordEncoding::FixedRow, byte offset for
    /// @ref RecordEncoding::ClassRecord.
    std::uint32_t wordSlot{};
    ValueWidth width = ValueWidth::Word;
    LongWordOrder longOrder = LongWordOrder::HighFirst;
    BitRange bits{};
};

/// @brief The comparator that legacy synthetic preference records select.
/// @details Aliased from musxdom so mapping tables read in one column width.
inline constexpr musx::dom::Cmper GLOBALS_CMPER = musx::dom::MUSX_GLOBALS_CMPER;

/// @brief Placeholder selector for rows whose comparator comes from the target object.
inline constexpr musx::dom::Cmper CMPER_FROM_TARGET = 0;

/// @brief The named FI family used by several fixed-row option records.
inline constexpr std::string_view figureTag = "FI";

/// @brief The class-record replacement for @ref figureTag in the zlib epoch.
inline constexpr records::LegacyTag zlibFigureClass = 0x008d;

/// @brief Converts a fixed-row numeric global selector to its zlib-era class id.
/// @details The zlib serialization retained the logical option identities and added
/// `0x000e` while coalescing each incidence family into one class-record payload.
[[nodiscard]] constexpr records::LegacyTag numericGlobalClass(std::uint16_t selector)
{
    return static_cast<records::LegacyTag>(selector + 0x000eU);
}

/// @brief The fixed-row tag of a numeric global: the two decimal characters of its selector.
/// @details A numeric global is not a record type of its own. Through Finale 2006 it is
/// spelled as decimal digits, so selector 40 is the tag `40`, and only from Finale
/// 2007 does it become the class id @ref numericGlobalClass derives. Deriving the spelling
/// keeps a selector stated once as a number rather than again as a two-character literal.
[[nodiscard]] constexpr records::LegacyTag numericGlobalTag(std::uint16_t selector)
{
    return static_cast<records::LegacyTag>(
        (static_cast<std::uint16_t>('0' + (selector / 10U) % 10U) << 8U) | static_cast<std::uint16_t>('0' + selector % 10U));
}

/// @brief One numeric global's whole incidence family, read as a single word stream.
struct GlobalSelectorWords
{
    /// @brief Every payload word of every incidence, in incidence order.
    /// @details A logical element may therefore straddle an incidence boundary, and a
    /// collection is simply as many elements as the stream holds.
    std::vector<std::int16_t> words;
    /// @brief Whether the family is present at all, which zero words cannot express.
    bool present{};
    std::size_t blockOffset{};
    std::size_t decodedOffset{};
};

/// @brief Collects a numeric global's fixed-row incidence family into one word stream.
/// @details Offsets come from the first incidence, which is where the family begins.
[[nodiscard]] GlobalSelectorWords readNumericGlobalWords(const records::LegacyRecordIndex& index, std::uint16_t selector);

/// @brief The byte offset a class record keeps the given word slot at.
/// @details The zlib eras address the same option by byte where the fixed rows address it by
/// word slot, so a class table's offsets are its fixed-row slots doubled. Stating that once
/// keeps a table from spelling out byte numbers whose relationship to the slots is silent.
[[nodiscard]] constexpr std::uint32_t classWordOffset(std::uint32_t slot)
{
    return slot * 2U;
}

/// @brief One word of a stream, or zero past its end.
/// @details Every collection decoder reads a fixed shape out of a stream whose length is the
/// file's business, so running off the end is an ordinary event and yields the same zero a
/// short final element would carry.
[[nodiscard]] std::int16_t wordAt(const std::vector<std::int16_t>& words, std::size_t index);

/// @brief Reads a class-record payload as signed words in the file's own byte order.
/// @details The zlib eras keep the word stream the fixed rows carried and merely coalesce
/// it into one payload, so a collection decoder can work in words for every era rather than
/// once per encoding. A trailing odd byte cannot belong to a word and is dropped.
[[nodiscard]] std::vector<std::int16_t> payloadWords(std::span<const std::uint8_t> payload, ByteOrder byteOrder);

/// @brief The 8-bit character a pre-Unicode record stores in a 16-bit word.
/// @details A source may store one either zero-extended or sign-extended: the same
/// character 139 appears as `0x008b` in some files and `0xff8b` in others. Narrowing to the
/// low byte is what makes the two spellings agree. It must not be applied to a codepoint
/// that a Unicode-era record stores as a long.
[[nodiscard]] std::uint32_t narrowCodepoint(std::int16_t stored);

/// @brief The codepoint a Unicode-era record stores across two consecutive words.
/// @details The words are in the order the container has already normalized each one into,
/// and the low half comes first. **Unverified for a big-endian container:** such a file needs
/// the 2012 release on a PowerPC Mac, which its stated requirements allow only by implication
/// and the operating systems mostly forbid, so one may never exist. A caller that can reach the
/// big-endian case should say so rather than rely on that absence.
[[nodiscard]] std::uint32_t wideCodepoint(std::int16_t low, std::int16_t high);

/// @brief Converts Finale's first/opposite/center ordering to musxdom's
/// first/center/opposite ordering.
[[nodiscard]] constexpr std::int64_t legacyCenterOppositeOrder(std::int64_t value)
{
    if (value == 1) {
        return 2;
    }
    if (value == 2) {
        return 1;
    }
    return value;
}

namespace versions {
/// @brief Whether a source stores symbol codepoints as longs rather than as words.
/// @details A version that could not be recovered reads as pre-Unicode. That is the safe
/// direction and costs nothing: only the zlib epoch reaches Finale 2012 at all, and every
/// earlier epoch stores the narrow form regardless of what its header says.
[[nodiscard]] inline bool storesUnicodeCodepoints(const std::optional<SourceVersion>& version)
{
    return version && VersionBound{version->major, version->minor, version->maint} >= finale2012;
}
} // namespace versions

/// @brief Which format epochs a table applies to.
enum class EpochMask : std::uint8_t {
    None = 0,
    CodaBanner = 1U << 0,
    Uncompressed = 1U << 1,
    Dcl = 1U << 2,
    Zlib = 1U << 3,
    /// @brief The eras whose pools resolve to fixed 16-byte Enigma rows.
    FixedRow = Uncompressed | Dcl,
    Any = CodaBanner | Uncompressed | Dcl | Zlib
};

[[nodiscard]] constexpr EpochMask operator|(EpochMask left, EpochMask right)
{
    return static_cast<EpochMask>(static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
}

/// @brief The chronological position of a classified format epoch.
[[nodiscard]] constexpr std::uint8_t formatEpochOrdinal(FormatEpoch epoch)
{
    switch (epoch) {
    case FormatEpoch::CodaBanner: return 0;
    case FormatEpoch::UncompressedLegacy: return 1;
    case FormatEpoch::DclLegacy: return 2;
    case FormatEpoch::ZlibLegacy: return 3;
    }
    MUSX_ASSERT_IF(true)
    {
        throw std::logic_error("Format epoch is not classified");
    }
    return 0;
}

[[nodiscard]] constexpr bool epochMatches(EpochMask mask, FormatEpoch epoch)
{
    const auto bit = static_cast<EpochMask>(1U << formatEpochOrdinal(epoch));
    return (static_cast<std::uint8_t>(mask) & static_cast<std::uint8_t>(bit)) != 0;
}

/// @brief The classified properties of the source file that gate mapping rows.
struct SourceProfile
{
    constexpr explicit SourceProfile(FormatEpoch sourceEpoch)
        : epoch(sourceEpoch)
    {
        static_cast<void>(formatEpochOrdinal(sourceEpoch));
    }

    FormatEpoch epoch;
    std::optional<SourceVersion> version;
    ByteOrder byteOrder = ByteOrder::Unknown;
    SourcePlatform platform = SourcePlatform::Unknown;
    /// @brief The Coda-banner format flags; zero for every other epoch.
    std::uint8_t codaFormatFlags{};
    const text::SymbolFontNames* symbolFontNames{};
    const percussion::MappingTables* percussionMappings{};
    const soundmap::Documents* soundmaps{};
    const playback::InstrumentCatalog* instrumentCatalog{};
};

/// @brief Whether a source belongs to any of the requested format epochs.
[[nodiscard]] constexpr bool sourceMatches(const SourceProfile& profile, EpochMask epochs)
{
    return epochMatches(epochs, profile.epoch);
}

/// @brief Whether a source belongs to one exact format epoch and recovered major/minor version.
/// @details A source without a recovered version does not match.
[[nodiscard]] constexpr bool sourceMatchesVersion(
    FormatEpoch sourceEpoch, const SourceVersion* sourceVersion, FormatEpoch expectedEpoch, VersionBound expectedVersion)
{
    return sourceEpoch == expectedEpoch && sourceVersion && VersionBound{sourceVersion->major, sourceVersion->minor} == expectedVersion;
}

[[nodiscard]] constexpr bool sourceMatchesVersion(const SourceProfile& profile, FormatEpoch expectedEpoch, VersionBound expectedVersion)
{
    return sourceMatchesVersion(profile.epoch, profile.version ? &*profile.version : nullptr, expectedEpoch, expectedVersion);
}

/// @brief Whether a source is at or beyond a chronological format epoch.
[[nodiscard]] constexpr bool sourceAtOrAfter(const SourceProfile& profile, FormatEpoch boundaryEpoch)
{
    return formatEpochOrdinal(profile.epoch) >= formatEpochOrdinal(boundaryEpoch);
}

/// @brief Whether a source is at or beyond a chronological epoch and version boundary.
/// @details A later epoch passes without needing a version. The boundary epoch passes only
/// when its recovered version is at least the requested version.
[[nodiscard]] constexpr bool sourceAtOrAfter(
    FormatEpoch sourceEpoch, const SourceVersion* sourceVersion, FormatEpoch boundaryEpoch, VersionBound boundaryVersion)
{
    if (sourceEpoch != boundaryEpoch) {
        return formatEpochOrdinal(sourceEpoch) >= formatEpochOrdinal(boundaryEpoch);
    }
    return sourceVersion
           && VersionBound{sourceVersion->major, sourceVersion->minor, sourceVersion->maint, sourceVersion->devStatus, sourceVersion->build}
                  >= boundaryVersion;
}

[[nodiscard]] constexpr bool sourceAtOrAfter(const SourceProfile& profile, FormatEpoch boundaryEpoch, VersionBound boundaryVersion)
{
    return sourceAtOrAfter(profile.epoch, profile.version ? &*profile.version : nullptr, boundaryEpoch, boundaryVersion);
}

/// @brief Whether a source is earlier than a version boundary within the boundary epoch.
/// @details Earlier epochs pass without needing a version. The boundary epoch passes only
/// when its recovered version is earlier; an absent version fails closed.
[[nodiscard]] constexpr bool sourcePredatesVersion(
    FormatEpoch sourceEpoch, const SourceVersion* sourceVersion, FormatEpoch boundaryEpoch, VersionBound boundaryVersion)
{
    if (sourceEpoch != boundaryEpoch) {
        return formatEpochOrdinal(sourceEpoch) < formatEpochOrdinal(boundaryEpoch);
    }
    return sourceVersion
           && VersionBound{sourceVersion->major, sourceVersion->minor, sourceVersion->maint, sourceVersion->devStatus, sourceVersion->build}
                  < boundaryVersion;
}

[[nodiscard]] constexpr bool sourcePredatesVersion(const SourceProfile& profile, FormatEpoch boundaryEpoch, VersionBound boundaryVersion)
{
    return sourcePredatesVersion(profile.epoch, profile.version ? &*profile.version : nullptr, boundaryEpoch, boundaryVersion);
}

using SourceGate = bool (*)(const SourceProfile& profile);

/// @brief Optionally adjusts a recovered number for source-era behavior.
/// @details A disengaged result applies the stored value directly. An engaged result is
/// applied instead and reports @ref ValueOrigin::LegacyMusAdjusted while preserving the
/// stored value and source offsets in @ref FieldInfo.
using SourceAdjustment = std::optional<std::int64_t> (*)(std::int64_t value, const records::LegacyRecordIndex& index, const SourceProfile& profile);

/// @brief One numeric global's whole payload, in whichever encoding the source uses.
/// @details A capture pass reads a collection the field tables cannot express, and the two
/// encodings state the same word stream: the fixed-row epochs spread it over incidences and
/// the zlib epoch coalesces it into one class record. Dispatching on the epoch here keeps a
/// capture pass describing its collection rather than restating that relationship.
[[nodiscard]] GlobalSelectorWords readGlobalWords(const records::LegacyRecordIndex& index, const SourceProfile& profile, std::uint16_t selector);

inline constexpr std::uint16_t codaMigratedPointSizeSelector = 64;

/// @brief Whether a Coda document carries the later point-valued size layout.
/// @details The selector named by @ref codaMigratedPointSizeSelector is the structural marker.
/// When absent, the corresponding values occupy single-precision fields on selectors 54 and 55.
[[nodiscard]] bool storesCodaMigratedPointSizes(const records::LegacyRecordIndex& index, const SourceProfile& profile);

/// @brief Whether a Coda document uses the original single-precision point-size layout.
[[nodiscard]] bool storesCodaFloatPointSizes(const records::LegacyRecordIndex& index, const SourceProfile& profile);

/// @brief Whether selector 41 word 1 uses its early lone-stem-flag layout.
/// @details The compressed epochs always use the later packed layout. In the earlier epochs,
/// any bit above the lone flag identifies the packed layout; an absent, zero, or bit-zero-only
/// word remains ambiguous and is treated as the early layout.
[[nodiscard]] bool storesLoneStemFlagLayout(const records::LegacyRecordIndex& index, const SourceProfile& profile);

/// @brief Whether selector 41 word 1 uses its packed beam-and-stem flag layout.
[[nodiscard]] bool storesPackedBeamFlagLayout(const records::LegacyRecordIndex& index, const SourceProfile& profile);

/// @brief Whether the fixed-row options use the layout introduced in Finale 3.5.
/// @details Selector 75 is a required member of that layout. An absent selector selects the
/// earlier layout, including in a damaged later document.
[[nodiscard]] bool storesFinale35OptionLayout(const records::LegacyRecordIndex& index, const SourceProfile& profile);

/// @brief Whether the stem-and-beam option family uses its pre-Finale-3.5 units.
/// @details The early layout identifies itself with a 32-element stem-connection collection;
/// the later layout has 128 elements. A missing collection is treated as the later layout.
[[nodiscard]] bool storesPreFinale35StemAndBeamUnits(const records::LegacyRecordIndex& index, const SourceProfile& profile);

/// @brief Whether the stem-and-beam option family uses Finale-3.5-or-later units.
[[nodiscard]] bool storesFinale35StemAndBeamUnits(const records::LegacyRecordIndex& index, const SourceProfile& profile);

/// @brief A decoded mapping value together with the physical row that supplied it.
struct ResolvedValue
{
    std::int64_t value{};
    std::size_t blockOffset{};
    std::size_t decodedOffset{};
};

/// @brief Reads one numeric value through the common fixed-row/class-record machinery.
/// @details Dynamic collection mappings use this rather than duplicating bounds checks,
/// byte-order handling, long-word assembly, or provenance rules.
[[nodiscard]] std::optional<ResolvedValue> readSourceValue(const records::LegacyRecordIndex& index, RecordEncoding encoding, std::uint16_t cmper,
    const SourceLocation& source, ByteOrder byteOrder, std::uint16_t partId = musx::dom::SCORE_PARTID);

/// @brief Assigns a decoded or converted value to a member, converting through the member's own
/// type, so that the assignment itself never narrows.
template <typename T, typename Value>
void assignFrom(T& target, Value value)
{
    target = static_cast<T>(value);
}

/// @brief Assigns to an optional member, engaging it.
/// @details musxdom uses an optional where an absent element means something other than a
/// zero value -- for a mutually exclusive pair of spellings, which one the document used.
/// A table row that reaches such a member has by definition found the value in the source,
/// so the member is engaged; leaving a member disengaged is the business of whatever decides
/// the row does not apply.
template <typename T, typename Value>
void assignFrom(std::optional<T>& target, Value value)
{
    target = static_cast<T>(value);
}

/// @brief Reads a member as a report value.
template <typename T>
[[nodiscard]] std::int64_t readAs(const T& source)
{
    return static_cast<std::int64_t>(source);
}

/// @brief Reads an optional member as a report value, treating a disengaged one as zero.
template <typename T>
[[nodiscard]] std::int64_t readAs(const std::optional<T>& source)
{
    return source ? static_cast<std::int64_t>(*source) : 0;
}

/// @brief What a mapped field reads out of the record stream.
enum class FieldKind : std::uint8_t {
    /// @brief One numeric value from a word slot, optionally a bit range of it.
    Number,
    /// @brief Text assembled from every incidence at or after @ref SourceLocation::incidence.
    /// @details Legacy strings run across incidences rather than living in one row: a font
    /// name occupies the rows after its header, and header text blocks span four rows each.
    /// The payload is read as bytes and truncated at the first NUL.
    Text
};

/// @brief One mapped field: where it comes from and where it goes.
struct FieldMapping
{
    const char* fieldName{};
    FieldKind kind = FieldKind::Number;
    SourceLocation source{};
    SourceGate sourceApplies{};
    void (*apply)(void* instance, std::int64_t value){};
    /// @brief Reads the seeded default, so the report can record a synthesized value
    /// without a separately maintained list of supported fields.
    std::int64_t (*read)(const void* instance){};
    void (*applyText)(void* instance, std::string_view value){};
    /// @brief Optional test against the destination object, for a field only some records carry.
    /// @details Some records state their own layout in a field of their own: a custom line style
    /// stores a character where a solid line stores a width, and a line cap stores an arrowhead
    /// comparator where a hook stores a length. Reading the deciding field and then testing it is
    /// the same preference for self-description that @ref MappingTable::applies expresses for a
    /// whole file, one record at a time.
    ///
    /// The test runs against the target after every field declared before it has been applied, so
    /// the deciding field must appear earlier in the table than the rows that test it. A row whose
    /// test fails is neither read nor reported: the field does not exist for that record, and
    /// reporting it as a synthesized default would claim a destination the object does not have.
    ///
    /// The test belongs to the destination rather than to any one era, so every table that layers
    /// onto the same field must state the same one.
    bool (*targetApplies)(const void* instance){};
    SourceAdjustment sourceAdjustment{};
};

[[nodiscard]] constexpr FieldMapping withSourceAdjustment(FieldMapping field, SourceAdjustment adjustment)
{
    field.sourceAdjustment = adjustment;
    return field;
}

/// @brief How a table finds the objects it writes to.
enum class TargetKind : std::uint8_t {
    /// @brief A single options object; rows use their own selector.
    OptionsSingleton,
    /// @brief Pooled others objects that the pinned baseline already seeded. Each target's
    /// comparator is used as the selector, and a record with no seeded object is skipped.
    OthersByCmper,
    /// @brief Pooled others objects created from the records themselves, one per comparator
    /// carried by the table's tag. Used where the legacy file is the only source of the
    /// objects, so there is no seeded default to overlay and nothing to report as
    /// synthesized.
    OthersFromRecords
};

/// @brief One resolved destination object.
struct MappingTarget
{
    std::uint16_t partId{};
    std::uint16_t cmper{};
    void* instance{};
    [[no_unique_address]] ReportClass reportClass;
};

template <typename T>
[[nodiscard]] MappingTarget makeMappingTarget(std::uint16_t partId, std::uint16_t cmper, T* instance)
{
    return {partId, cmper, instance, ReportClass::of<T>()};
}

/// @brief A mapping table: one musxdom class, one applicability gate, a set of fields.
/// @details Several tables may target the same class. Every table whose gate matches is
/// applied in registry order, and a later row supersedes an earlier row for the same
/// field name, so a field that moves in a later version costs a one-row override table
/// rather than a restatement of the whole table.
struct MappingTable
{
    /// @brief Report target prefix, also the identity used when layering tables.
    const char* reportPrefix{};
    EpochMask epochs = EpochMask::FixedRow;
    SourceGate sourceApplies{};
    /// @brief Optional extra test, for a boundary neither the epoch nor the version states.
    /// @details Some layouts change at a release that sits inside one epoch, at a version no
    /// available file occupies, or in a way the record stream states more directly than the
    /// header does. Where the stream says which layout a file uses, a predicate reads that
    /// instead of dating the file, which is the same preference for self-description that
    /// decides the clef tuple width from its payload size. Prefer an epoch gate to this, and
    /// this to a version gate.
    bool (*applies)(const records::LegacyRecordIndex& index, const SourceProfile& profile){};
    RecordEncoding encoding = RecordEncoding::FixedRow;
    TargetKind targetKind = TargetKind::OptionsSingleton;
    std::vector<MappingTarget> (*enumerateTargets)(const musx::dom::DocumentPtr& document){};
    /// @brief Record identity whose comparators enumerate the objects, for @ref
    /// TargetKind::OthersFromRecords.
    records::LegacyTag recordIdentity{};
    /// @brief Supplies the destination for one source record, for @ref
    /// TargetKind::OthersFromRecords.
    /// @details Called once per record identity the family carries, so the class decides what
    /// each one means: pooling a new source-owned object is the usual answer, but a class the
    /// baseline also seeds may return the seeded object of that identity instead and be overlaid
    /// onto it. Returning an empty target skips the record.
    MappingTarget (*createTarget)(
        const musx::dom::DocumentPtr& document, const RecordFamilySource& source, const records::LegacyRow& row, std::uint16_t cmper){};
    const FieldMapping* fields{};
    std::size_t fieldCount{};
    /// @brief Optional pass over one target after every field of this table has been applied.
    /// @details For work that needs more than one recovered field at once and therefore
    /// cannot be expressed as a field mapping. Converting a legacy font name to UTF-8 is the
    /// motivating case: the encoding is named by the charset fields of the same record, so
    /// the name cannot be converted until both are in hand. Doing it here rather than inside
    /// the name's own apply keeps the reader from depending on the order fields happen to be
    /// declared in.
    ///
    /// The document is passed because a record may name something the document holds rather
    /// than something the record carries: a stored character is a byte in the encoding of the
    /// font definition its own field names, so decoding it needs both the recovered font id
    /// and the pool that id points into. A finalizer that reads the document depends on the
    /// classes it reads being imported first, which the registry order states.
    void (*finalizeTarget)(void* instance, const SourceProfile& profile, const musx::dom::DocumentPtr& document){};
    /// @brief Compact score/part payload pairs recognized by this class importer.
    std::span<const CompactPartLayout> compactPartLayouts{};
};

/// @brief Creates one others object of type T and adds it to the document pool.
template <typename T>
[[nodiscard]] MappingTarget createOthersTarget(
    const musx::dom::DocumentPtr& document, const RecordFamilySource& source, const records::LegacyRow& row, std::uint16_t cmper)
{
    auto instance = createOthersRecordTarget<T>(document, source, row, cmper);
    if (!instance) {
        return {};
    }
    auto* raw = instance.get();
    document->getOthers()->add(T::XmlNodeName, std::move(instance));
    return makeMappingTarget(row.partId, cmper, raw);
}

/// @brief Enumerates the single instance of an options class, if it was seeded.
template <typename T>
[[nodiscard]] std::vector<MappingTarget> enumerateOptionsTarget(const musx::dom::DocumentPtr& document)
{
    std::vector<MappingTarget> result;
    if (const auto instance = document->getOptions()->template get<T>()) {
        // Pool instances are handed out const. Overlaying legacy values is the one
        // reason this library writes to them, so the cast is confined to here.
        result.push_back(makeMappingTarget(musx::dom::SCORE_PARTID, 0, const_cast<T*>(instance.get())));
    }
    return result;
}

/// @brief Enumerates every seeded instance of an others class, in comparator order.
/// @details Targets come from the document rather than from the record stream, so a
/// file that lacks a record still reports the seeded default, and a record with no
/// seeded target is skipped rather than fabricated.
template <typename T>
[[nodiscard]] std::vector<MappingTarget> enumerateOthersTargets(const musx::dom::DocumentPtr& document)
{
    std::vector<MappingTarget> result;
    for (const auto& instance : document->getOthers()->template getArray<T>(musx::dom::SCORE_PARTID)) {
        result.push_back(makeMappingTarget(musx::dom::SCORE_PARTID, instance->getCmper(), const_cast<T*>(instance.get())));
    }
    return result;
}

/// @brief Applies an explicit list of tables to a seeded document.
/// @details Tables are layered in list order. Every field of every listed table is
/// reported, whether or not this file can supply it; the gates decide only what is read.
/// Recovered values carry @ref ValueOrigin::LegacyMus with the offsets they came from,
/// and everything else carries the seeded default as @ref ValueOrigin::Finale27Default.
/// @param skipRecord For a @ref TargetKind::OthersFromRecords table, returns true for a record,
/// named by its part and comparator, that creates no object.
void applyMappingTables(const std::vector<const MappingTable*>& tables, const records::LegacyRecordIndex& index, const SourceProfile& profile,
    const musx::dom::DocumentPtr& document, ImportReport& report,
    const std::function<bool(std::uint16_t partId, std::uint16_t cmper)>& skipRecord = {});

/// @brief A reference-document object a target field needs, resolved after the pools are complete.
/// @details Copying an object out of the reference allocates comparators in the target, so it
/// cannot run while the target's own pools are still being filled: a comparator handed out early
/// could collide with one the source is about to claim. Capture therefore records what it needs
/// and leaves the field at zero, and one phase at the end resolves every request at once.
///
/// @ref assign writes the resolved comparator wherever it belongs, so the queue stays ignorant of
/// what is waiting on it. Clef shapes are the only user today; nothing about it is clef-specific.
struct PendingShapeReference
{
    /// @brief The shape's comparator in the reference document, meaningless in the target.
    musx::dom::Cmper referenceShapeId{};
    /// @brief Writes the resolved target comparator into the field that needs it.
    std::function<void(musx::dom::Cmper)> assign;
    [[no_unique_address]] DeferredFieldReport reportField{};
};

/// @brief A reference-document custom line a target field needs, resolved after all source pools.
struct PendingCustomLineReference
{
    /// @brief The custom line's comparator in the reference document.
    musx::dom::Cmper referenceLineId{};
    /// @brief Writes the resolved target comparator into the field that needs it.
    std::function<void(musx::dom::Cmper)> assign;
    [[no_unique_address]] DeferredFieldReport reportField{};
};

/// @brief The ordered stages of deferred work, drained once every importer has run.
/// @details A step may rely on everything an earlier stage establishes, and on nothing a step of
/// its own stage does: within a stage, steps must be independent of each other. A step that
/// needs another step's result belongs in a later stage than that step. The stage a step names is
/// therefore the statement of what it depends on.
enum class DeferredStage : std::uint8_t {
    /// Completes source-owned objects whose construction needed other pools to be filled.
    CompletePools,
    /// Creates objects at comparators the source stores, before any step allocates a comparator in
    /// the same pool and could take one of them.
    ClaimStoredIds,
    /// Judges recovered references and values against the objects the source stores, before any
    /// step synthesizes an object such a reference could otherwise appear to name.
    ResolveStoredRefs,
    /// Normalizes stored values that depend on resolved source references.
    NormalizeStoredValues,
    /// Synthesizes the objects a Staff owns, such as its names and a single-string fret instrument,
    /// at newly allocated comparators. Ahead of Synthesize, which allocates from some of the same
    /// pools, so the order of allocation does not depend on importer order.
    SynthesizeStaffParts,
    /// Synthesizes other objects at newly allocated comparators.
    Synthesize,
    /// Refreshes flags that summarize other pools, such as Staff::hasStyles.
    RefreshDerivedFlags,
    /// Reads composite views, such as StaffComposite, that depend on the refreshed flags.
    ReadComposites,
};

/// @brief The number of @ref DeferredStage values.
inline constexpr std::size_t deferredStageCount = static_cast<std::size_t>(DeferredStage::ReadComposites) + 1;

/// @brief Work deferred until every source pool is filled, and drained in one phase afterwards.
/// @details Two kinds. A reference-object request copies an object out of the pinned baseline,
/// which allocates comparators and therefore cannot run while a pool is still being filled. A
/// deferred step is anything an importer can only decide once another class's objects exist --
/// resolving a recovered comparator against the pool that owns its referent, most of all.
///
/// Deferring such a step is what keeps the importer registry an unordered list. An importer that
/// consulted another pool directly would silently depend on running after whoever fills it, and
/// that dependency would live in the registry's line order where nothing checks it. A dependency
/// between deferred steps is stated instead by the @ref DeferredStage each one names.
struct PendingReferences
{
    std::vector<PendingShapeReference> shapes; ///< Shape definitions requested by recovered classes.
    std::vector<PendingCustomLineReference> customLines; ///< Custom lines requested by recovered classes.
    /// @brief Legacy staff systems whose flags select a system-specific StaffUsed list.
    std::set<std::pair<musx::dom::Cmper, musx::dom::Cmper>> staffSystemsWithOwnStaffLists;
    /// @brief An HS row's HT text id, replaced by its synthesized TextBlock id during materialization.
    std::map<std::pair<musx::dom::Cmper, musx::dom::Inci>, musx::dom::Cmper> codaTextBlockByStyle;
    /// @brief Original and final Note IDs by zero-based Note index, for Entries whose IDs changed.
    struct NoteIdsByIndex
    {
        std::vector<musx::dom::NoteNumber> original;
        std::vector<musx::dom::NoteNumber> final;
    };
    std::map<musx::dom::EntryNumber, NoteIdsByIndex> noteIdsByEntryIndex;
    /// @brief Stored TCD words by Note index for Coda Entries, used when a combined detail does not name a Note ID.
    std::map<musx::dom::EntryNumber, std::vector<std::uint16_t>> noteTcdByEntryIndex;
    /// @brief The legacy percussion-layout upgrade, computed once by the first importer that needs it.
    std::shared_ptr<const percussion::LegacyConversion> legacyPercussion;
    /// @brief The stored SmartShapes the reader does not recover, keyed `{partId, shapeNum}`,
    /// computed once by the first deferred step that needs them.
    std::optional<std::set<std::pair<musx::dom::Cmper, musx::dom::Cmper>>> skippedSmartShapes;
    /// @brief Deferred steps by stage, each stage in registration order.
    std::array<std::vector<std::function<void()>>, deferredStageCount> steps;

    /// @brief Registers @p step to run in @p stage once every importer has finished.
    void defer(DeferredStage stage, std::function<void()> step) { steps[static_cast<std::size_t>(stage)].push_back(std::move(step)); }

    /// @brief Registers @p sequence as one step of @p stage that runs its members in order.
    /// @details For an importer whose per-object steps allocate comparators from a shared pool: the
    /// order of its own objects, not the drain, then decides the comparators they receive.
    void defer(DeferredStage stage, std::vector<std::function<void()>> sequence)
    {
        if (!sequence.empty()) {
            defer(stage, [sequence = std::move(sequence)] {
                for (const auto& step : sequence) {
                    step();
                }
            });
        }
    }
};

/// @brief Runs every deferred step, stage by stage.
/// @details The last step of the phase that follows every importer, and the reason registry order
/// carries no meaning: an importer that needs another class's objects registers the work here and
/// this runs it once every pool is filled. Exposed because a test that drives one importer alone
/// still has to close the phase, and doing that by repeating the loop would let a test pass
/// against a drain the reader no longer performs the same way.
void runDeferredChecks(PendingReferences& pending, DeferredOrder order = DeferredOrder::AsRegistered);

/// @brief Everything the importer for one musxdom class is handed.
/// @details Passed by reference and outlived by nothing: it is built once per import and
/// exists only for the duration of the pass over the registered importers.
struct ImportContext
{
    const records::LegacyRecordIndex& index;
    const SourceProfile& profile;
    /// @brief The source file's own bytes, for content that lives outside the record pools.
    /// @details The 0x200 header is document content as well as classification data: it
    /// carries the File Info strings, which are a `texts` pool class and belong to no record.
    /// An importer that reads them needs the file rather than the index, which is the only
    /// reason this is here.
    std::span<const std::uint8_t> source;
    const musx::dom::DocumentPtr& document;
    /// @brief The separately owned pinned baseline, read-only. An object owned by it must
    /// never be inserted into @ref document; copy what is needed instead.
    const musx::dom::DocumentPtr& referenceDocument;
    ImportReport& report;
    /// @brief Reference objects a class needs copied, drained after every pool is filled.
    PendingReferences& pending;
    /// @brief The construction session's font registry.
    /// @details Every font comparator this import leaves in the document must be registered
    /// here, because musxdom resolves the registered set once at @ref
    /// musx::factory::DocumentFactory::ConstructionSession::finish and supplies a placeholder
    /// definition for any comparator the document does not define. A comparator that is never
    /// registered gets no placeholder, and `FontInfo::getName` and everything routed through
    /// it -- `calcIsSameTypeface`, `calcIsSMuFL` -- throw on it instead.
    ///
    /// Register the value a field finally holds, not every value it passes through. Several
    /// importers overwrite a recovered comparator during a later repair pass, and registering
    /// the discarded one would mint a placeholder definition that nothing references.
    musx::factory::ConstructionContext& construction;
};

enum class NoteReferenceMode {
    NoteIdOnly,
    TcdOnly
};

/// @brief Chooses the reference encoded by a combined Coda note detail.
[[nodiscard]] NoteReferenceMode combinedNoteReferenceMode(const SourceProfile& profile);

/// @brief Defers a note detail's source ID translation until Entries have been imported.
void deferNoteIdRemap(const ImportContext& context, const std::shared_ptr<musx::dom::NoteDetailsBase>& detail,
    std::function<void(musx::dom::NoteNumber)> assign, NoteReferenceMode mode = NoteReferenceMode::NoteIdOnly);

template <typename T>
void registerNoteIdRemap(const ImportContext& context, const std::shared_ptr<T>& detail, musx::dom::NoteNumber T::* member,
    NoteReferenceMode mode = NoteReferenceMode::NoteIdOnly)
{
    static_assert(std::is_base_of_v<musx::dom::NoteDetailsBase, T>);
    deferNoteIdRemap(
        context, detail,
        [detail, member, &context](musx::dom::NoteNumber id) {
            detail.get()->*member = id;
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto entryNumber = detail->getEntryNumber();
                const auto key = reporting.template instanceKey<T>(detail->getSourcePartId(), static_cast<musx::dom::Cmper>(entryNumber >> 16U),
                    detail->getInci(), static_cast<musx::dom::Cmper>(entryNumber));
                if (auto* field = reporting.report().findField(key, "noteId")) {
                    field->origin = Reporting::Origin::LegacyMusAdjusted;
                }
            });
        },
        mode);
}

/// @brief Reports every object created by a reference-document import as a pinned default.
musx::dom::ImportObjectCallback baselineObjectReporter(ImportReport& report);

/// @brief Recovers one musxdom class, from record identity to finished object.
/// @details One per class. An importer owns every decision its class needs -- which tables
/// apply to which epoch, which capture pass builds a collection, and what must be checked
/// afterwards -- so that the registry states only which classes are imported and in what
/// order. Nothing outside the class's own translation unit knows how many layouts it has.
using ClassImporter = void (*)(const ImportContext& context);

/// @brief Applies every registered mapping table to a seeded document.
/// @details Runs the registered importers in order, then drains @ref PendingReferences in one
/// final phase. That phase allocates `others` comparators, so nothing may allocate one after
/// this returns. @p order is a testing aid; the reader always uses the default.
void applyLegacyMappings(const records::LegacyRecordIndex& index, const SourceProfile& profile, std::span<const std::uint8_t> source,
    const musx::dom::DocumentPtr& document, const musx::dom::DocumentPtr& referenceDocument, ImportReport& report,
    musx::factory::ConstructionContext& construction, DeferredOrder order = DeferredOrder::AsRegistered);

} // namespace finale_mus_reader

/// @brief Declares a numeric field mapping with every column stated explicitly, including the
/// @ref FieldMapping::targetApplies test.
/// @details `member` names the destination as the C++ path that reaches it from `Class`, so a
/// field inside a contained object is written `charParams->lineChar` and needs no second
/// spelling: the report turns that path into the dotted one it prints.
#define MUS_IDENTIFIED_FIELD_IF(                                                                                                                    \
    Class, identityValue, selectorValue, incidenceValue, slotValue, widthValue, orderValue, bitsValue, sourceGateValue, appliesValue, member)       \
    ::finale_mus_reader::FieldMapping                                                                                                               \
    {                                                                                                                                               \
        #member, ::finale_mus_reader::FieldKind::Number,                                                                                            \
            ::finale_mus_reader::SourceLocation{(identityValue), static_cast<std::uint16_t>(selectorValue),                                         \
                static_cast<std::uint32_t>(incidenceValue), static_cast<std::uint32_t>(slotValue), (widthValue), (orderValue), (bitsValue)},        \
            (sourceGateValue),                                                                                                                      \
            [](void* instance, std::int64_t value) { ::finale_mus_reader::assignFrom(static_cast<Class*>(instance)->member, value); },              \
            [](const void* instance) -> std::int64_t { return ::finale_mus_reader::readAs(static_cast<const Class*>(instance)->member); }, nullptr, \
            (appliesValue)                                                                                                                          \
    }

#define MUS_FIELD_IF(                                                                                                                                \
    Class, tagText, selectorValue, incidenceValue, slotValue, widthValue, orderValue, bitsValue, sourceGateValue, appliesValue, member)              \
    MUS_IDENTIFIED_FIELD_IF(Class, ::finale_mus_reader::records::packTag(tagText), selectorValue, incidenceValue, slotValue, widthValue, orderValue, \
        bitsValue, sourceGateValue, appliesValue, member)

#define MUS_NUMERIC_FIELD(Class, selectorValue, incidenceValue, slotValue, widthValue, orderValue, bitsValue, sourceGateValue, member)       \
    MUS_IDENTIFIED_FIELD_IF(Class, ::finale_mus_reader::numericGlobalTag(selectorValue), ::finale_mus_reader::GLOBALS_CMPER, incidenceValue, \
        slotValue, widthValue, orderValue, bitsValue, sourceGateValue, nullptr, member)

/// @brief Declares a numeric field mapping with every column stated explicitly.
#define MUS_FIELD(Class, tagText, selectorValue, incidenceValue, slotValue, widthValue, orderValue, bitsValue, sourceGateValue, member) \
    MUS_FIELD_IF(Class, tagText, selectorValue, incidenceValue, slotValue, widthValue, orderValue, bitsValue, sourceGateValue, nullptr, member)

/// @brief Declares a class-record field mapping with every column stated explicitly, including
/// the @ref FieldMapping::targetApplies test.
/// @details Every class-record spelling below is this form with one or more columns fixed.
#define MUS_CLASS_FIELD_IF(Class, identityValue, selectorValue, byteOffset, widthValue, orderValue, bitsValue, appliesValue, member)                \
    ::finale_mus_reader::FieldMapping                                                                                                               \
    {                                                                                                                                               \
        #member, ::finale_mus_reader::FieldKind::Number,                                                                                            \
            ::finale_mus_reader::SourceLocation{(identityValue), static_cast<std::uint16_t>(selectorValue), 0,                                      \
                static_cast<std::uint32_t>(byteOffset), (widthValue), (orderValue), (bitsValue)},                                                   \
            nullptr, [](void* instance, std::int64_t value) { ::finale_mus_reader::assignFrom(static_cast<Class*>(instance)->member, value); },     \
            [](const void* instance) -> std::int64_t { return ::finale_mus_reader::readAs(static_cast<const Class*>(instance)->member); }, nullptr, \
            (appliesValue)                                                                                                                          \
    }

/// @brief A bit range of a class-identified record, addressed by byte offset in its payload,
/// in a record found under an explicit comparator.
#define MUS_CLASS_SELECTED_BITS(Class, classId, selectorValue, byteOffset, firstBit, bitCount, member)   \
    MUS_CLASS_FIELD_IF(Class, classId, selectorValue, byteOffset, ::finale_mus_reader::ValueWidth::Word, \
        ::finale_mus_reader::LongWordOrder::HighFirst,                                                   \
        (::finale_mus_reader::BitRange{static_cast<std::uint8_t>(firstBit), static_cast<std::uint8_t>(bitCount)}), nullptr, member)

/// @brief A bit range of a class-identified record, addressed by byte offset in its payload.
/// @details The comparator is left at zero because the tables that use this are others
/// tables, where the target object's own comparator selects the record and the field's
/// comparator is ignored. An options singleton has no such comparator and must name the
/// record's own, so it uses @ref MUS_CLASS_WORD or @ref MUS_CLASS_BIT instead.
#define MUS_CLASS_BITS(Class, classId, byteOffset, firstBit, bitCount, member) \
    MUS_CLASS_SELECTED_BITS(Class, classId, 0, byteOffset, firstBit, bitCount, member)

/// @brief A whole two-byte field of a class-identified record found under a comparator.
/// @details The zero bit count is what selects the whole value rather than a range of it,
/// and a whole value is read signed.
#define MUS_CLASS_WORD(Class, classId, selector, byteOffset, member) MUS_CLASS_SELECTED_BITS(Class, classId, selector, byteOffset, 0, 0, member)

/// @brief A single bit of a class-identified record's payload word, under a comparator.
#define MUS_CLASS_BIT(Class, classId, selector, byteOffset, bitIndex, member) \
    MUS_CLASS_SELECTED_BITS(Class, classId, selector, byteOffset, bitIndex, 1, member)

/// @brief A four-byte field of a class-identified record, spanning two payload words.
/// @details The order is the mapping's own, not the container's: the zlib serialization kept
/// the word pair the fixed rows carried, so the same word-order rule applies to both.
#define MUS_CLASS_LONG(Class, classId, selector, byteOffset, order, member) \
    MUS_CLASS_FIELD_IF(                                                     \
        Class, classId, selector, byteOffset, ::finale_mus_reader::ValueWidth::Long, (order), ::finale_mus_reader::BitRange{}, nullptr, member)

/// @brief A whole two-byte field a record carries only when it selects that layout.
#define MUS_CLASS_WORD_IF(Class, classId, selector, byteOffset, applies, member)                                                                   \
    MUS_CLASS_FIELD_IF(Class, classId, selector, byteOffset, ::finale_mus_reader::ValueWidth::Word, ::finale_mus_reader::LongWordOrder::HighFirst, \
        ::finale_mus_reader::BitRange{}, (applies), member)

/// @brief A four-byte field a record carries only when it selects that layout.
#define MUS_CLASS_LONG_IF(Class, classId, selector, byteOffset, order, applies, member) \
    MUS_CLASS_FIELD_IF(                                                                 \
        Class, classId, selector, byteOffset, ::finale_mus_reader::ValueWidth::Long, (order), ::finale_mus_reader::BitRange{}, (applies), member)

/// @brief The counterpart of @ref MUS_CLASS_FIELD_IF for a value that needs converting on the
/// way in. `value` names the extracted source value.
#define MUS_CLASS_FIELD_AS_IF(Class, identityValue, selectorValue, byteOffset, widthValue, orderValue, bitsValue, appliesValue, member, ...)        \
    ::finale_mus_reader::FieldMapping                                                                                                               \
    {                                                                                                                                               \
        #member, ::finale_mus_reader::FieldKind::Number,                                                                                            \
            ::finale_mus_reader::SourceLocation{(identityValue), static_cast<std::uint16_t>(selectorValue), 0,                                      \
                static_cast<std::uint32_t>(byteOffset), (widthValue), (orderValue), (bitsValue)},                                                   \
            nullptr,                                                                                                                                \
            [](void* instance, std::int64_t value) { ::finale_mus_reader::assignFrom(static_cast<Class*>(instance)->member, (__VA_ARGS__)); },      \
            [](const void* instance) -> std::int64_t { return ::finale_mus_reader::readAs(static_cast<const Class*>(instance)->member); }, nullptr, \
            (appliesValue)                                                                                                                          \
    }

/// @brief A transformed bit range of a class-identified record, in a record found under an
/// explicit comparator.
/// @details The counterpart of @ref MUS_CLASS_SELECTED_BITS for a value that needs converting
/// on the way in. An @ref TargetKind::OptionsSingleton table takes its comparator from the
/// field rather than from the target, so a singleton needs this form wherever
/// @ref MUS_CLASS_BITS_AS would leave the comparator at zero and find no record.
#define MUS_CLASS_SELECTED_BITS_AS(Class, classId, selectorValue, byteOffset, firstBit, bitCount, member, ...) \
    MUS_CLASS_FIELD_AS_IF(Class, classId, selectorValue, byteOffset, ::finale_mus_reader::ValueWidth::Word,    \
        ::finale_mus_reader::LongWordOrder::HighFirst,                                                         \
        (::finale_mus_reader::BitRange{static_cast<std::uint8_t>(firstBit), static_cast<std::uint8_t>(bitCount)}), nullptr, member, __VA_ARGS__)

/// @brief A bit range of a class-identified record, assigned through a conversion expression.
#define MUS_CLASS_BITS_AS(Class, classId, byteOffset, firstBit, bitCount, member, ...) \
    MUS_CLASS_SELECTED_BITS_AS(Class, classId, 0, byteOffset, firstBit, bitCount, member, __VA_ARGS__)

/// @brief A whole two-byte field a record carries only when it selects that layout, assigned
/// through a conversion expression.
#define MUS_CLASS_WORD_AS_IF(Class, classId, selector, byteOffset, applies, member, ...)               \
    MUS_CLASS_FIELD_AS_IF(Class, classId, selector, byteOffset, ::finale_mus_reader::ValueWidth::Word, \
        ::finale_mus_reader::LongWordOrder::HighFirst, ::finale_mus_reader::BitRange{}, (applies), member, __VA_ARGS__)

/// @brief A field musxdom exposes only through a setter, applied by calling it.
/// @details Use where musxdom owns the meaning of the stored value -- a packed style mask is
/// the case in hand -- so that meaning is decoded in musxdom rather than restated here.
/// `owner` is the path to the object holding the property, `field` names it for the report,
/// and `setter` is the method that applies the value once cast to `Stored`.
///
/// The seeded default is not read back, because a table that needs this form builds its
/// objects from the records themselves and so has no seeded default to report.
#define MUS_CLASS_SET_IF(Class, classId, selector, byteOffset, applies, owner, field, setter, Stored)                                               \
    ::finale_mus_reader::FieldMapping                                                                                                               \
    {                                                                                                                                               \
        #owner "." #field, ::finale_mus_reader::FieldKind::Number,                                                                                  \
            ::finale_mus_reader::SourceLocation{(classId), static_cast<std::uint16_t>(selector), 0, static_cast<std::uint32_t>(byteOffset),         \
                ::finale_mus_reader::ValueWidth::Word, ::finale_mus_reader::LongWordOrder::HighFirst, ::finale_mus_reader::BitRange{}},             \
            nullptr, [](void* instance, std::int64_t value) { static_cast<Class*>(instance)->owner->setter(static_cast<Stored>(value)); }, nullptr, \
            nullptr, (applies)                                                                                                                      \
    }

/// @brief Text running from a byte offset to the end of a class-identified record's payload.
#define MUS_CLASS_TEXT(Class, classId, byteOffset, member)                                                                                      \
    ::finale_mus_reader::FieldMapping                                                                                                           \
    {                                                                                                                                           \
        #member, ::finale_mus_reader::FieldKind::Text,                                                                                          \
            ::finale_mus_reader::SourceLocation{(classId), 0, 0, static_cast<std::uint32_t>(byteOffset), ::finale_mus_reader::ValueWidth::Word, \
                ::finale_mus_reader::LongWordOrder::HighFirst, ::finale_mus_reader::BitRange{}},                                                \
            nullptr, nullptr, nullptr, [](void* instance, std::string_view value) { static_cast<Class*>(instance)->member.assign(value); }      \
    }

/// @brief Declares an explicitly identified numeric field that converts its extracted value.
/// @details `value` names the extracted source value in the assignment expression.
#define MUS_IDENTIFIED_FIELD_AS_IF(                                                                                                                 \
    Class, identityValue, selectorValue, incidenceValue, slotValue, widthValue, orderValue, bitsValue, sourceGateValue, appliesValue, member, ...)  \
    ::finale_mus_reader::FieldMapping                                                                                                               \
    {                                                                                                                                               \
        #member, ::finale_mus_reader::FieldKind::Number,                                                                                            \
            ::finale_mus_reader::SourceLocation{(identityValue), static_cast<std::uint16_t>(selectorValue),                                         \
                static_cast<std::uint32_t>(incidenceValue), static_cast<std::uint32_t>(slotValue), (widthValue), (orderValue), (bitsValue)},        \
            (sourceGateValue),                                                                                                                      \
            [](void* instance, std::int64_t value) { ::finale_mus_reader::assignFrom(static_cast<Class*>(instance)->member, (__VA_ARGS__)); },      \
            [](const void* instance) -> std::int64_t { return ::finale_mus_reader::readAs(static_cast<const Class*>(instance)->member); }, nullptr, \
            (appliesValue)                                                                                                                          \
    }

/// @brief The counterpart of @ref MUS_FIELD_IF for a value that needs converting on the way
/// in. `value` names the extracted source value.
#define MUS_FIELD_AS_IF(                                                                                                                     \
    Class, tagText, selectorValue, incidenceValue, slotValue, widthValue, orderValue, bitsValue, sourceGateValue, appliesValue, member, ...) \
    MUS_IDENTIFIED_FIELD_AS_IF(Class, ::finale_mus_reader::records::packTag(tagText), selectorValue, incidenceValue, slotValue, widthValue,  \
        orderValue, bitsValue, sourceGateValue, appliesValue, member, __VA_ARGS__)

/// @brief Converts a numeric-global field while deriving its tag from its selector.
#define MUS_NUMERIC_FIELD_AS_IF(                                                                                                                \
    Class, selectorValue, incidenceValue, slotValue, widthValue, orderValue, bitsValue, sourceGateValue, appliesValue, member, ...)             \
    MUS_IDENTIFIED_FIELD_AS_IF(Class, ::finale_mus_reader::numericGlobalTag(selectorValue), ::finale_mus_reader::GLOBALS_CMPER, incidenceValue, \
        slotValue, widthValue, orderValue, bitsValue, sourceGateValue, appliesValue, member, __VA_ARGS__)

/// @brief A bit range assigned through an explicit conversion expression.
/// @details Use where the stored encoding does not match the destination type, such as an
/// enum whose values differ from the legacy encoding. `value` names the extracted bits.
#define MUS_BITS_AS(Class, tagText, selectorValue, incidenceValue, slotValue, firstBit, bitCount, member, ...)                               \
    MUS_FIELD_AS_IF(Class, tagText, selectorValue, incidenceValue, slotValue, ::finale_mus_reader::ValueWidth::Word,                         \
        ::finale_mus_reader::LongWordOrder::HighFirst,                                                                                       \
        (::finale_mus_reader::BitRange{static_cast<std::uint8_t>(firstBit), static_cast<std::uint8_t>(bitCount)}), nullptr, nullptr, member, \
        __VA_ARGS__)

/// @brief A two-byte field a record carries only when it selects that layout, assigned
/// through a conversion expression.
#define MUS_WORD_AS_IF(Class, tagText, selector, incidence, slot, applies, member, ...)                                                              \
    MUS_FIELD_AS_IF(Class, tagText, selector, incidence, slot, ::finale_mus_reader::ValueWidth::Word, ::finale_mus_reader::LongWordOrder::HighFirst, \
        ::finale_mus_reader::BitRange{}, nullptr, (applies), member, __VA_ARGS__)

/// @brief The fixed-row spelling of @ref MUS_CLASS_SET_IF.
#define MUS_SET_IF(Class, tagText, selector, incidence, slot, applies, owner, field, setter, Stored)                                                \
    ::finale_mus_reader::FieldMapping                                                                                                               \
    {                                                                                                                                               \
        #owner "." #field, ::finale_mus_reader::FieldKind::Number,                                                                                  \
            ::finale_mus_reader::SourceLocation{::finale_mus_reader::records::packTag(tagText), static_cast<std::uint16_t>(selector),               \
                static_cast<std::uint32_t>(incidence), static_cast<std::uint32_t>(slot), ::finale_mus_reader::ValueWidth::Word,                     \
                ::finale_mus_reader::LongWordOrder::HighFirst, ::finale_mus_reader::BitRange{}},                                                    \
            nullptr, [](void* instance, std::int64_t value) { static_cast<Class*>(instance)->owner->setter(static_cast<Stored>(value)); }, nullptr, \
            nullptr, (applies)                                                                                                                      \
    }

/// @brief Text assembled from every incidence at or after `firstIncidence`.
#define MUS_TEXT(Class, tagText, selectorValue, firstIncidence, member)                                                                              \
    ::finale_mus_reader::FieldMapping                                                                                                                \
    {                                                                                                                                                \
        #member, ::finale_mus_reader::FieldKind::Text,                                                                                               \
            ::finale_mus_reader::SourceLocation{::finale_mus_reader::records::packTag(tagText), static_cast<std::uint16_t>(selectorValue),           \
                static_cast<std::uint16_t>(firstIncidence), 0, ::finale_mus_reader::ValueWidth::Word, ::finale_mus_reader::LongWordOrder::HighFirst, \
                ::finale_mus_reader::BitRange{}},                                                                                                    \
            nullptr, nullptr, nullptr, [](void* instance, std::string_view value) { static_cast<Class*>(instance)->member.assign(value); }           \
    }

/// @brief A two-byte field.
#define MUS_WORD(Class, tagText, selector, incidence, slot, member)                                                                            \
    MUS_FIELD(Class, tagText, selector, incidence, slot, ::finale_mus_reader::ValueWidth::Word, ::finale_mus_reader::LongWordOrder::HighFirst, \
        ::finale_mus_reader::BitRange{}, nullptr, member)

/// @brief A two-byte field addressed by its numeric global selector.
#define MUS_NUMERIC_WORD(Class, selector, incidence, slot, member)                                                                            \
    MUS_NUMERIC_FIELD(Class, selector, incidence, slot, ::finale_mus_reader::ValueWidth::Word, ::finale_mus_reader::LongWordOrder::HighFirst, \
        ::finale_mus_reader::BitRange{}, nullptr, member)

/// @brief A two-byte field optionally adjusted for source-era behavior.
#define MUS_WORD_ADJUSTED(Class, tagText, selector, incidence, slot, adjustment, member) \
    ::finale_mus_reader::withSourceAdjustment(MUS_WORD(Class, tagText, selector, incidence, slot, member), (adjustment))

/// @brief A two-byte field restricted by a source gate.
#define MUS_WORD_IF_SOURCE(Class, tagText, selector, incidence, slot, sourceGate, member)                                                      \
    MUS_FIELD(Class, tagText, selector, incidence, slot, ::finale_mus_reader::ValueWidth::Word, ::finale_mus_reader::LongWordOrder::HighFirst, \
        ::finale_mus_reader::BitRange{}, (sourceGate), member)

/// @brief A one-byte field, narrowed from its payload word.
#define MUS_BYTE(Class, tagText, selector, incidence, slot, member)                                                                            \
    MUS_FIELD(Class, tagText, selector, incidence, slot, ::finale_mus_reader::ValueWidth::Byte, ::finale_mus_reader::LongWordOrder::HighFirst, \
        ::finale_mus_reader::BitRange{}, nullptr, member)

/// @brief A four-byte field spanning two consecutive payload words.
#define MUS_LONG(Class, tagText, selector, incidence, slot, order, member) \
    MUS_FIELD(                                                             \
        Class, tagText, selector, incidence, slot, ::finale_mus_reader::ValueWidth::Long, (order), ::finale_mus_reader::BitRange{}, nullptr, member)

/// @brief A single bit of a payload word.
#define MUS_BIT(Class, tagText, selector, incidence, slot, bitIndex, member)                                                                   \
    MUS_FIELD(Class, tagText, selector, incidence, slot, ::finale_mus_reader::ValueWidth::Word, ::finale_mus_reader::LongWordOrder::HighFirst, \
        (::finale_mus_reader::BitRange{static_cast<std::uint8_t>(bitIndex), 1}), nullptr, member)

/// @brief A single bit addressed by its numeric global selector.
#define MUS_NUMERIC_BIT(Class, selector, incidence, slot, bitIndex, member)                                                                   \
    MUS_NUMERIC_FIELD(Class, selector, incidence, slot, ::finale_mus_reader::ValueWidth::Word, ::finale_mus_reader::LongWordOrder::HighFirst, \
        (::finale_mus_reader::BitRange{static_cast<std::uint8_t>(bitIndex), 1}), nullptr, member)

/// @brief A contiguous bit range of a payload word.
#define MUS_BITS(Class, tagText, selector, incidence, slot, firstBit, bitCount, member)                                                        \
    MUS_FIELD(Class, tagText, selector, incidence, slot, ::finale_mus_reader::ValueWidth::Word, ::finale_mus_reader::LongWordOrder::HighFirst, \
        (::finale_mus_reader::BitRange{static_cast<std::uint8_t>(firstBit), static_cast<std::uint8_t>(bitCount)}), nullptr, member)

/// @brief A two-byte field a record carries only when it selects that layout.
#define MUS_WORD_IF(Class, tagText, selector, incidence, slot, applies, member)                                                                   \
    MUS_FIELD_IF(Class, tagText, selector, incidence, slot, ::finale_mus_reader::ValueWidth::Word, ::finale_mus_reader::LongWordOrder::HighFirst, \
        ::finale_mus_reader::BitRange{}, nullptr, (applies), member)
