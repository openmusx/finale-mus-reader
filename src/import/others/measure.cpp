// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "import/shared/barline_type.h"
#include "import/shared/measure_records.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace measure_internal {

// Named rather than anonymous: ReportState<DecodedMeasureFields<Reporting>> holds these by
// value and has external linkage, and under the unity build GCC rejects an anonymous-namespace
// member there (-Wsubobject-linkage). The names stay distinctive for the same reason every
// file-local name does.

/// @brief One measure's words together with the physical row each of them came from.
/// @details A fixed-row era spreads the stream over incidences and the zlib era coalesces it into
/// one payload, so every offset the report cites is taken from the row that actually holds the
/// word rather than computed from the start of the family.
struct MeasureRecord
{
    std::vector<std::uint16_t> words;
    std::vector<std::size_t> blockOffsets;
    std::vector<std::size_t> decodedOffsets;

    [[nodiscard]] std::uint16_t word(std::size_t slot) const { return slot < words.size() ? words[slot] : 0; }

    [[nodiscard]] std::int16_t signedWord(std::size_t slot) const { return static_cast<std::int16_t>(word(slot)); }
};

/// @brief One decoded member, with the word it came from and what that word held.
template <typename Reporting>
struct DecodedMeasureField
{
    const char* member{};
    typename Reporting::Origin origin = Reporting::Origin::LegacyMus;
    std::size_t slot{};
    std::int64_t stored{};
    /// @brief The record the value came from, when it is not the measure's own.
    const MeasureRecord* record{};
};

template <typename Reporting>
using DecodedMeasureFields = std::vector<DecodedMeasureField<Reporting>>;

} // namespace measure_internal

namespace {

using measure_internal::DecodedMeasureField;
using measure_internal::DecodedMeasureFields;
using measure_internal::MeasureRecord;

using MeasureTarget = musx::dom::others::Measure;

// The display time signature of the six-word era, which has no word of its own for it. `ms` is a
// second record under the same comparator whose layout mirrors `MS`: the beats and divisions sit
// in the slots `MS` gives the actual time signature, and the first flag word carries the composite
// bits at the same positions. A measure has one only when it uses a display time signature, so the
// record's presence is what says so.
constexpr records::LegacyTag displayTimeSigTag = records::packTag("ms");

// The measure record's word stream, which every era shares a prefix of. The first six words are
// unchanged from Finale 1.0.0 through Finale 2012; the rest arrive in two later additions.
constexpr std::size_t measureWidthSlot = 0;
constexpr std::size_t measureKeySlot = 1;
constexpr std::size_t measureBeatsSlot = 2;
constexpr std::size_t measureDivBeatSlot = 3;
constexpr std::size_t measurePrimaryFlagsSlot = 4;
// Slot 5, the second flag word, and slot 8, the third, are in the shared measure record header.
constexpr std::size_t measureDispBeatsSlot = 6;
constexpr std::size_t measureDispDivBeatSlot = 7;
constexpr std::size_t measureCustomBarShapeSlot = 9;
constexpr std::size_t measureCustomLeftBarShapeSlot = 10;
constexpr std::size_t measureFrontSpaceExtraSlot = 11;
constexpr std::size_t measureBackSpaceExtraSlot = 12;

// The three word counts a measure record has held, and how they are told apart.
// **The record states its own shape**, so the layout is read rather than dated: six words
// is one 16-byte row, twelve is two, and the thirteenth word needs a third row in the fixed-row
// epochs while it merely lengthens the class record in the zlib epoch.
constexpr std::size_t measureWordsThroughFinale98 = 6;
constexpr std::size_t measureWordsThroughFinale2004 = 12;
constexpr std::size_t measureWordsFromFinale2005 = 13;

// The compact record a zlib-era part carries for a measure it has unlinked. Four words, and only
// the four values a part may differ in; everything else comes from the score object this overlays.
// The flag word is its own, not a copy of either flag word of the score layout: the positioning
// mode occupies the same low bits, but the page break sits at 0x4000, where the score record's
// first flag word spells "hide cautionary".
constexpr std::size_t measureCompactPartPayloadSize = 8;
constexpr std::size_t measureCompactWidthSlot = 0;
constexpr std::size_t measureCompactFlagSlot = 1;
constexpr std::size_t measureCompactFrontSpaceExtraSlot = 2;
constexpr std::size_t measureCompactBackSpaceExtraSlot = 3;
constexpr std::uint16_t measureCompactPageBreakMask = 0x4000;

constexpr std::size_t measureScorePayloadSize = 26;
constexpr CompactPartLayout measureCompactLayouts[] = {{measureScorePayloadSize, measureCompactPartPayloadSize}};

// The first flag word (slot 4). Every bit below holds its position from Finale 1.0.0 through
// Finale 2012 except @ref primaryNoBarlineMask, which the Coda-banner epoch uses for something else
// entirely; see @ref MeasureFlagLayout.
constexpr std::uint16_t primaryBreakWordExtMask = 0x8000;
constexpr std::uint16_t primaryHideCautionMask = 0x4000;
constexpr std::uint16_t primaryHasSmartShapeMask = 0x2000;
constexpr std::uint16_t primaryGroupBarlineOverrideMask = 0x1000;
constexpr std::uint16_t primaryShowFullNamesMask = 0x0800;
constexpr std::uint16_t primaryMeasNumbIndivPosMask = 0x0400;
constexpr std::uint16_t primaryAllowSplitPointsMask = 0x0100;
constexpr std::uint16_t primaryCompositeNumeratorMask = 0x0080;
constexpr std::uint16_t primaryCompositeDenominatorMask = 0x0040;
constexpr std::uint16_t primaryNeverShowKeyMask = 0x0020;
constexpr std::uint16_t primaryNeverShowTimeMask = 0x0010;
constexpr std::uint16_t primaryEvenlyAcrossMeasureMask = 0x0008;
constexpr std::uint16_t primaryPositioningModeMask = 0x0007;
// The Coda-banner meaning of the bit that later carries the group-barline override: the measure
// draws no barline at all.
constexpr std::uint16_t primaryNoBarlineMask = 0x1000;

// The second flag word (slot 5), in the layout Finale 3.0 and later use.
constexpr std::uint16_t secondaryBeginNewSystemMask = 0x8000;
// The has-expression bit is secondaryHasExpressionMask in the shared measure record header.
constexpr std::uint16_t secondaryBreakMmRestMask = 0x2000;
constexpr std::uint16_t secondaryNoMeasNumMask = 0x1000;
constexpr std::uint16_t secondaryAlwaysShowKeyMask = 0x0800;
constexpr std::uint16_t secondaryAlwaysShowTimeMask = 0x0400;
constexpr std::uint16_t secondaryHasOssiaMask = 0x0200;
constexpr std::uint16_t secondaryHasTextBlockMask = 0x0100;
constexpr std::uint16_t secondaryBarlineMask = 0x00f0;
constexpr std::uint8_t secondaryBarlineShift = 4;
constexpr std::uint16_t secondaryForwardRepeatMask = 0x0008;
constexpr std::uint16_t secondaryBackwardRepeatMask = 0x0004;
constexpr std::uint16_t secondaryHasEndingMask = 0x0002;
constexpr std::uint16_t secondaryHasTextRepeatMask = 0x0001;

// The Coda-banner reading of the second flag word, which differs from the later one in bits
// 4 through 7 only.
// The later word spells a barline type there; this one has the measure-has-expression flag at the
// bottom of the group and then three independent barline and multimeasure-rest bits above it.
constexpr std::uint16_t codaHasExpressionMask = 0x0010;
constexpr std::uint16_t codaDoubleBarlineMask = 0x0020;
constexpr std::uint16_t codaBreakMmRestMask = 0x0040;
constexpr std::uint16_t codaFinalBarlineMask = 0x0080;

// The third flag word (slot 8), present only once the record has twelve words.
constexpr std::uint16_t extendedAbbrvTimeMask = 0x0002;
constexpr std::uint16_t extendedUseDisplayTimesigMask = 0x0004;
constexpr std::uint16_t extendedLeftBarlineMask = 0x00f0;
constexpr std::uint8_t extendedLeftBarlineShift = 4;
constexpr std::uint16_t extendedCompositeDispNumeratorMask = 0x0100;
constexpr std::uint16_t extendedCompositeDispDenominatorMask = 0x0200;
constexpr std::uint16_t extendedPageBreakMask = 0x0800;

// Finale's positioning codes. Three of them translate directly; the two that carry individual
// positioning are 4 and 6 rather than the 3 and 4 musxdom numbers them. Code 3 is reserved and
// code 5 selects the measure's own mode inside an ossia, which musxdom has no value for; both
// fall back to the manual mode while the report keeps the stored code.
[[nodiscard]] MeasureTarget::PositioningType positioningTypeOf(std::uint16_t stored)
{
    using PositioningType = MeasureTarget::PositioningType;
    switch (stored) {
    case 1: return PositioningType::TimeSignature;
    case 2: return PositioningType::BeatChart;
    case 4: return PositioningType::TimeSigPlusPositioning;
    case 6: return PositioningType::BeatChartPlusPositioning;
    default: return PositioningType::Manual;
    }
}

/// @brief Which reading the flag word at @ref measureSecondaryFlagsSlot takes.
/// @details An epoch gate: the Coda-banner epoch takes the early reading, Finale 3.0 and later
/// the other. Nothing in the record states which, and the two are indistinguishable by shape --
/// both are one six-word row.
enum class MeasureFlagLayout : std::uint8_t {
    /// @brief Finale 1.0.0 through 2.6.
    Coda,
    /// @brief Finale 3.0 and later.
    Later
};

/// @brief The layout one document's measure records use.
struct MeasureLayout
{
    /// @brief How many of the thirteen words this document's records supply.
    std::size_t slots{};
    MeasureFlagLayout flags{};
    /// @brief Whether the first flag word's `0x0800` carries "Show Full Staff & Group Names".
    /// @details **A version gate.** The bit predates the setting and carries a different meaning
    /// in earlier releases. Nothing in the record distinguishes the two readings -- the word is the
    /// same width throughout -- so the version decides, and a document whose version cannot be
    /// recovered fails closed and takes the era's behavior.
    bool storesShowFullNames{};

    [[nodiscard]] bool hasDisplayTimeSig() const { return slots >= measureWordsThroughFinale2004; }
    [[nodiscard]] bool hasBackSpaceExtra() const { return slots >= measureWordsFromFinale2005; }
};

/// @brief Reads one measure's whole word stream, in whichever encoding the source uses.
/// @details A fixed row arrives with its words already normalized to the container's byte order,
/// so they are taken as they stand; a class record is raw bytes and is normalized here. That is
/// the same split every other collection decoder makes, and it is why this takes the record
/// family rather than a pool.
[[nodiscard]] MeasureRecord readMeasureRecord(const RecordFamilySource& source, std::span<const records::LegacyRow> rows, ByteOrder byteOrder)
{
    MeasureRecord result;
    for (const auto& row : rows) {
        const auto append = [&](std::uint16_t value, std::size_t offset) {
            result.words.push_back(value);
            result.blockOffsets.push_back(row.blockOffset);
            result.decodedOffsets.push_back(row.decodedOffset + offset);
        };
        if (source.classRecords) {
            const auto payload = source.pool->effectivePayloadOf(row);
            for (std::size_t offset = 0; offset + 1 < payload.size(); offset += 2) {
                append(payloadWord(payload, offset, byteOrder), offset);
            }
        } else {
            for (std::uint8_t slot = 0; slot < row.wordCount; ++slot) {
                append(static_cast<std::uint16_t>(row.words[slot]), std::size_t{slot} * 2);
            }
        }
    }
    return result;
}

/// @brief Not a valid measure comparator. Measures are numbered from 1.
inline constexpr std::uint16_t invalidMeasureCmper = 0;

/// @brief The largest word stream any of a document's score measure records supplies.
/// @details The layout is a property of the document rather than of one measure, and a fixed-row
/// era pads its last row, so the widest stream is what says which of the three layouts is in use.
[[nodiscard]] std::size_t measureStoredWordCount(const RecordFamilySource& source)
{
    std::size_t result = 0;
    for (const auto cmper : source.pool->cmpersForTag(source.identity, musx::dom::SCORE_PARTID)) {
        if (cmper == invalidMeasureCmper) {
            continue;
        }
        std::size_t words = 0;
        for (const auto& row : source.pool->getArray(source.identity, cmper, 0, musx::dom::SCORE_PARTID)) {
            words += source.classRecords ? source.pool->effectivePayloadOf(row).size() / 2 : row.wordCount;
        }
        result = (std::max)(result, words);
    }
    return result;
}

/// @brief Reads the display time-signature record of one measure, if it has one.
/// @details Only the six-word layout uses it; from Finale 2000 the measure record carries the
/// display time signature itself, and no document of a later layout writes this record.
[[nodiscard]] std::optional<MeasureRecord> readDisplayTimeSigRecord(
    const ImportContext& context, const RecordFamilySource& source, const MeasureLayout& layout, std::uint16_t partId, std::uint16_t cmper)
{
    if (layout.hasDisplayTimeSig() || source.classRecords) {
        return std::nullopt;
    }
    const auto rows = source.pool->getArray(displayTimeSigTag, cmper, 0, partId);
    if (rows.empty()) {
        return std::nullopt;
    }
    return readMeasureRecord(source, rows, context.profile.byteOrder);
}

[[nodiscard]] MeasureLayout measureLayoutOf(const ImportContext& context, const RecordFamilySource& source)
{
    const auto stored = measureStoredWordCount(source);
    const auto slots = stored >= measureWordsFromFinale2005      ? measureWordsFromFinale2005
                       : stored >= measureWordsThroughFinale2004 ? measureWordsThroughFinale2004
                                                                 : measureWordsThroughFinale98;
    const auto flags = context.profile.epoch == FormatEpoch::CodaBanner ? MeasureFlagLayout::Coda : MeasureFlagLayout::Later;
    return {slots, flags, sourceAtOrAfter(context.profile, FormatEpoch::ZlibLegacy, versions::finale2011)};
}

/// @brief Collects the decoded members of one measure so they can be reported in one pass.
class MeasureDecoder
{
public:
    MeasureDecoder(ImportReport& report, const MeasureRecord& record, const MeasureLayout& layout, MeasureTarget& target,
        const MeasureRecord* displayRecord = nullptr)
        : m_report(report), m_record(record), m_layout(layout), m_target(target), m_display(displayRecord)
    {}

    /// @brief The measure's display time-signature record, or null when it has none.
    [[nodiscard]] const MeasureRecord* display() const { return m_display; }

    /// @brief Assigns a member the record supplies, and records where it came from.
    template <typename Member, typename Value>
    void stored(const char* name, Member& member, std::size_t slot, Value value, std::int64_t rawValue)
    {
        member = value;
        noteStored(name, slot, rawValue, nullptr);
    }

    /// @brief Assigns a member another record supplies, so the report cites that record's offsets.
    template <typename Member, typename Value>
    void storedFrom(const MeasureRecord& record, const char* name, Member& member, std::size_t slot, Value value, std::int64_t rawValue)
    {
        member = value;
        noteStored(name, slot, rawValue, &record);
    }

    /// @brief Assigns a member no layout of this era stores, at the value the era behaved as.
    /// @details The value takes the member's own type, so a literal converts at the call rather
    /// than narrowing inside the assignment.
    template <typename Member>
    void behavior(const char* name, Member& member, std::type_identity_t<Member> value)
    {
        member = value;
        noteBehavior(name, static_cast<std::int64_t>(value));
    }

    /// @brief Records a member whose legacy source has not been located in any layout.
    template <typename Member>
    void unmapped(const char* name, const Member& member)
    {
        noteUnmapped(name, static_cast<std::int64_t>(member));
    }

    /// @brief Assigns a flag the record supplies, from a mask of the word at @p slot.
    void flag(const char* name, bool& member, std::size_t slot, std::uint16_t mask)
    {
        stored(name, member, slot, (m_record.word(slot) & mask) != 0, m_record.word(slot));
    }

    [[nodiscard]] const MeasureRecord& record() const { return m_record; }
    [[nodiscard]] const MeasureLayout& layout() const { return m_layout; }
    [[nodiscard]] MeasureTarget& target() const { return m_target; }
    [[nodiscard]] const auto& fields() const { return m_fields; }

private:
    /// @brief Record where a member's value came from. Provenance is carried only in a build that
    /// can report it; the assignments that call these happen in every build.
    void noteStored(const char* name, std::size_t slot, std::int64_t stored, const MeasureRecord* from)
    {
        withReporting(m_report, [&]<typename Reporting>(Reporting& reporting) {
            reporting.state(m_fields).push_back({name, Reporting::Origin::LegacyMus, slot, stored, from});
        });
    }

    void noteBehavior(const char* name, std::int64_t value)
    {
        withReporting(m_report, [&]<typename Reporting>(Reporting& reporting) {
            reporting.state(m_fields).push_back({name, Reporting::Origin::LegacyBehavior, 0, value, nullptr});
        });
    }

    void noteUnmapped(const char* name, std::int64_t value)
    {
        withReporting(m_report, [&]<typename Reporting>(Reporting& reporting) {
            reporting.state(m_fields).push_back({name, Reporting::Origin::Unmapped, 0, value, nullptr});
        });
    }

    ImportReport& m_report;
    const MeasureRecord& m_record;
    const MeasureLayout& m_layout;
    MeasureTarget& m_target;
    const MeasureRecord* m_display{};
    [[no_unique_address]] ReportState<DecodedMeasureFields> m_fields;
};

/// @brief Decodes the six words every era stores, other than the two flag words.
void decodeCommonWords(MeasureDecoder& decoder)
{
    const auto& record = decoder.record();
    auto& target = decoder.target();
    decoder.stored("width", target.width, measureWidthSlot, record.signedWord(measureWidthSlot), record.signedWord(measureWidthSlot));
    decoder.stored("globalKeySig->key", target.globalKeySig->key, measureKeySlot, record.word(measureKeySlot), record.word(measureKeySlot));
    decoder.stored("beats", target.beats, measureBeatsSlot, record.word(measureBeatsSlot), record.word(measureBeatsSlot));
    decoder.stored("divBeat", target.divBeat, measureDivBeatSlot, record.word(measureDivBeatSlot), record.word(measureDivBeatSlot));
}

/// @brief Decodes the first flag word, whose bits hold their positions across every era.
void decodePrimaryFlags(MeasureDecoder& decoder)
{
    auto& target = decoder.target();
    const auto primary = decoder.record().word(measurePrimaryFlagsSlot);
    decoder.flag("breakWordExt", target.breakWordExt, measurePrimaryFlagsSlot, primaryBreakWordExtMask);
    decoder.flag("hideCaution", target.hideCaution, measurePrimaryFlagsSlot, primaryHideCautionMask);
    decoder.flag("hasSmartShape", target.hasSmartShape, measurePrimaryFlagsSlot, primaryHasSmartShapeMask);
    decoder.flag("hasMeasNumbIndivPos", target.hasMeasNumbIndivPos, measurePrimaryFlagsSlot, primaryMeasNumbIndivPosMask);
    decoder.flag("allowSplitPoints", target.allowSplitPoints, measurePrimaryFlagsSlot, primaryAllowSplitPointsMask);
    decoder.flag("compositeNumerator", target.compositeNumerator, measurePrimaryFlagsSlot, primaryCompositeNumeratorMask);
    decoder.flag("compositeDenominator", target.compositeDenominator, measurePrimaryFlagsSlot, primaryCompositeDenominatorMask);
    decoder.flag("evenlyAcrossMeasure", target.evenlyAcrossMeasure, measurePrimaryFlagsSlot, primaryEvenlyAcrossMeasureMask);
    decoder.stored(
        "positioningMode", target.positioningMode, measurePrimaryFlagsSlot, positioningTypeOf(primary & primaryPositioningModeMask), primary);

    // The bit that later overrides a staff group's barline is how a Coda-banner measure says it
    // draws no barline at all, so it is not read as the override there.
    if (decoder.layout().flags == MeasureFlagLayout::Coda) {
        decoder.behavior("groupBarlineOverride", target.groupBarlineOverride, false);
    } else {
        decoder.flag("groupBarlineOverride", target.groupBarlineOverride, measurePrimaryFlagsSlot, primaryGroupBarlineOverrideMask);
    }

    // "Show Full Staff & Group Names" arrives with Finale 2011. The bit predates the setting and
    // carries a different meaning in earlier releases, so their value is not read.
    if (decoder.layout().storesShowFullNames) {
        decoder.flag("showFullNames", target.showFullNames, measurePrimaryFlagsSlot, primaryShowFullNamesMask);
    } else {
        decoder.behavior("showFullNames", target.showFullNames, false);
    }
}

/// @brief Applies the key and time signature show modes, each of which two bits decide.
/// @details The two live in different words -- "never" in the first flag word and "always" in the
/// measure flags -- because they arrived as separate checkboxes before the popup that replaced
/// them. Only one can be set from the interface; "never" wins if a document has both.
void decodeShowModes(MeasureDecoder& decoder)
{
    auto& target = decoder.target();
    const auto primary = decoder.record().word(measurePrimaryFlagsSlot);
    const auto measure = decoder.record().word(measureSecondaryFlagsSlot);
    const bool neverKey = (primary & primaryNeverShowKeyMask) != 0;
    const bool neverTime = (primary & primaryNeverShowTimeMask) != 0;
    const bool alwaysKey = (measure & secondaryAlwaysShowKeyMask) != 0;
    const bool alwaysTime = (measure & secondaryAlwaysShowTimeMask) != 0;
    decoder.stored("showKey", target.showKey, measurePrimaryFlagsSlot,
        neverKey    ? MeasureTarget::ShowKeySigMode::Never
        : alwaysKey ? MeasureTarget::ShowKeySigMode::Always
                    : MeasureTarget::ShowKeySigMode::IfNeeded,
        primary);
    decoder.stored("showTime", target.showTime, measurePrimaryFlagsSlot,
        neverTime    ? MeasureTarget::ShowTimeSigMode::Never
        : alwaysTime ? MeasureTarget::ShowTimeSigMode::Always
                     : MeasureTarget::ShowTimeSigMode::IfNeeded,
        primary);
}

/// @brief Decodes the measure flag word in the layout Finale 3.0 and later use.
void decodeLaterSecondaryFlags(MeasureDecoder& decoder)
{
    auto& target = decoder.target();
    const auto measure = decoder.record().word(measureSecondaryFlagsSlot);
    decoder.flag("beginNewSystem", target.beginNewSystem, measureSecondaryFlagsSlot, secondaryBeginNewSystemMask);
    decoder.flag("hasExpression", target.hasExpression, measureSecondaryFlagsSlot, secondaryHasExpressionMask);
    decoder.flag("breakMmRest", target.breakMmRest, measureSecondaryFlagsSlot, secondaryBreakMmRestMask);
    decoder.flag("noMeasNum", target.noMeasNum, measureSecondaryFlagsSlot, secondaryNoMeasNumMask);
    decoder.flag("hasOssia", target.hasOssia, measureSecondaryFlagsSlot, secondaryHasOssiaMask);
    decoder.flag("hasTextBlock", target.hasTextBlock, measureSecondaryFlagsSlot, secondaryHasTextBlockMask);
    decoder.stored("barlineType", target.barlineType, measureSecondaryFlagsSlot,
        barlineTypeOf(static_cast<std::uint16_t>((measure & secondaryBarlineMask) >> secondaryBarlineShift)), measure);
    decoder.flag("forwardRepeatBar", target.forwardRepeatBar, measureSecondaryFlagsSlot, secondaryForwardRepeatMask);
    decoder.flag("backwardsRepeatBar", target.backwardsRepeatBar, measureSecondaryFlagsSlot, secondaryBackwardRepeatMask);
    decoder.flag("hasEnding", target.hasEnding, measureSecondaryFlagsSlot, secondaryHasEndingMask);
    decoder.flag("hasTextRepeat", target.hasTextRepeat, measureSecondaryFlagsSlot, secondaryHasTextRepeatMask);
}

/// @brief Decodes the Coda-banner measure flag word.
/// @details Bits 4 through 7 are the whole of the difference. Where the later word spells a
/// barline type in one nibble, this one has the measure-has-expression flag and then three bits
/// that are read together: a double barline, a multimeasure-rest break, and a final barline.
///
/// Finale 27 additionally sets the word-extension break on every measure whose Coda barline is
/// double or final. That is the upgrade's reading of what those barlines meant, not a bit the
/// record carries, and it is not reproduced: the first flag word has a word-extension bit of its
/// own and this era leaves it clear.
///
/// The repeat, ossia, system-break, and show-mode bits hold the positions the later word gives
/// them, **believed** for the four repeat bits and the never-show-key bit. The two bits the later
/// word uses for the expression flag and the multimeasure-rest break are deliberately not read,
/// because this era spells both somewhere else.
void decodeCodaSecondaryFlags(MeasureDecoder& decoder)
{
    using BarlineType = MeasureTarget::BarlineType;
    auto& target = decoder.target();
    const auto primary = decoder.record().word(measurePrimaryFlagsSlot);
    const auto measure = decoder.record().word(measureSecondaryFlagsSlot);
    decoder.flag("beginNewSystem", target.beginNewSystem, measureSecondaryFlagsSlot, secondaryBeginNewSystemMask);
    decoder.flag("hasExpression", target.hasExpression, measureSecondaryFlagsSlot, codaHasExpressionMask);
    decoder.flag("breakMmRest", target.breakMmRest, measureSecondaryFlagsSlot,
        static_cast<std::uint16_t>(codaBreakMmRestMask | codaDoubleBarlineMask | codaFinalBarlineMask));
    decoder.flag("noMeasNum", target.noMeasNum, measureSecondaryFlagsSlot, secondaryNoMeasNumMask);
    decoder.flag("hasOssia", target.hasOssia, measureSecondaryFlagsSlot, secondaryHasOssiaMask);
    decoder.flag("hasTextBlock", target.hasTextBlock, measureSecondaryFlagsSlot, secondaryHasTextBlockMask);
    // The barline is spread across the two words: the first flag word says whether one is drawn at
    // all, and the measure word chooses between the two types that are not the normal one.
    const auto barline = (primary & primaryNoBarlineMask) != 0    ? BarlineType::None
                         : (measure & codaFinalBarlineMask) != 0  ? BarlineType::Final
                         : (measure & codaDoubleBarlineMask) != 0 ? BarlineType::Double
                                                                  : BarlineType::Normal;
    decoder.stored("barlineType", target.barlineType, measureSecondaryFlagsSlot, barline, measure);
    decoder.flag("forwardRepeatBar", target.forwardRepeatBar, measureSecondaryFlagsSlot, secondaryForwardRepeatMask);
    decoder.flag("backwardsRepeatBar", target.backwardsRepeatBar, measureSecondaryFlagsSlot, secondaryBackwardRepeatMask);
    decoder.flag("hasEnding", target.hasEnding, measureSecondaryFlagsSlot, secondaryHasEndingMask);
    decoder.flag("hasTextRepeat", target.hasTextRepeat, measureSecondaryFlagsSlot, secondaryHasTextRepeatMask);
}

/// @brief Decodes the seven words Finale 2000 added, or supplies what their absence means.
/// @details A six-word document has no custom barline shapes, no left barline of its own, and no
/// extra space at either end of the bar, and each is reported as the era's behavior: a left
/// barline comes from the document's barline options, which is what the fifteen a later record
/// stores means. Its display time signature comes from a record of its own instead.
void decodeDisplayAndSpacingWords(MeasureDecoder& decoder)
{
    auto& target = decoder.target();
    const auto& record = decoder.record();
    if (!decoder.layout().hasDisplayTimeSig()) {
        // The display time signature lives in its own record here. Its presence is what says the
        // measure uses one, so a measure without the record has none rather than an absent value.
        if (const auto* display = decoder.display()) {
            const auto primary = display->word(measurePrimaryFlagsSlot);
            decoder.storedFrom(
                *display, "dispBeats", target.dispBeats, measureBeatsSlot, display->word(measureBeatsSlot), display->word(measureBeatsSlot));
            decoder.storedFrom(*display, "dispDivbeat", target.dispDivbeat, measureDivBeatSlot, display->word(measureDivBeatSlot),
                display->word(measureDivBeatSlot));
            decoder.storedFrom(*display, "useDisplayTimesig", target.useDisplayTimesig, measureBeatsSlot, true, 1);
            decoder.storedFrom(*display, "compositeDispNumerator", target.compositeDispNumerator, measurePrimaryFlagsSlot,
                (primary & primaryCompositeNumeratorMask) != 0, primary);
            decoder.storedFrom(*display, "compositeDispDenominator", target.compositeDispDenominator, measurePrimaryFlagsSlot,
                (primary & primaryCompositeDenominatorMask) != 0, primary);
        } else {
            decoder.behavior("dispBeats", target.dispBeats, 0);
            decoder.behavior("dispDivbeat", target.dispDivbeat, 0);
            decoder.behavior("useDisplayTimesig", target.useDisplayTimesig, false);
            decoder.behavior("compositeDispNumerator", target.compositeDispNumerator, false);
            decoder.behavior("compositeDispDenominator", target.compositeDispDenominator, false);
        }
        // No layout without the new flag word has a per-measure abbreviation at all: the era
        // abbreviates a time signature by the document-wide setting in TimeSignatureOptions, so
        // there is nothing per measure to recover.
        decoder.behavior("abbrvTime", target.abbrvTime, false);
        // The chord flag has no word in this layout: the era keeps chords as entry details rather
        // than measure ones, so false is what the measure record implies.
        decoder.behavior("hasChord", target.hasChord, false);
        decoder.behavior("customBarShape", target.customBarShape, 0);
        decoder.behavior("customLeftBarShape", target.customLeftBarShape, 0);
        decoder.behavior("leftBarlineType", target.leftBarlineType, MeasureTarget::BarlineType::OptionsDefault);
        decoder.behavior("pageBreak", target.pageBreak, false);
        decoder.behavior("frontSpaceExtra", target.frontSpaceExtra, 0);
        decoder.behavior("backSpaceExtra", target.backSpaceExtra, 0);
        return;
    }

    const auto extendedFlags = record.word(measureExtendedFlagsSlot);
    decoder.stored("dispBeats", target.dispBeats, measureDispBeatsSlot, record.word(measureDispBeatsSlot), record.word(measureDispBeatsSlot));
    decoder.stored(
        "dispDivbeat", target.dispDivbeat, measureDispDivBeatSlot, record.word(measureDispDivBeatSlot), record.word(measureDispDivBeatSlot));
    decoder.stored("customBarShape", target.customBarShape, measureCustomBarShapeSlot, record.word(measureCustomBarShapeSlot),
        record.word(measureCustomBarShapeSlot));
    decoder.stored("customLeftBarShape", target.customLeftBarShape, measureCustomLeftBarShapeSlot, record.word(measureCustomLeftBarShapeSlot),
        record.word(measureCustomLeftBarShapeSlot));
    decoder.stored("frontSpaceExtra", target.frontSpaceExtra, measureFrontSpaceExtraSlot, record.signedWord(measureFrontSpaceExtraSlot),
        record.signedWord(measureFrontSpaceExtraSlot));
    decoder.flag("abbrvTime", target.abbrvTime, measureExtendedFlagsSlot, extendedAbbrvTimeMask);
    decoder.flag("useDisplayTimesig", target.useDisplayTimesig, measureExtendedFlagsSlot, extendedUseDisplayTimesigMask);
    decoder.flag("compositeDispNumerator", target.compositeDispNumerator, measureExtendedFlagsSlot, extendedCompositeDispNumeratorMask);
    decoder.flag("compositeDispDenominator", target.compositeDispDenominator, measureExtendedFlagsSlot, extendedCompositeDispDenominatorMask);
    decoder.flag("pageBreak", target.pageBreak, measureExtendedFlagsSlot, extendedPageBreakMask);
    decoder.flag("hasChord", target.hasChord, measureExtendedFlagsSlot, extendedHasChordMask);
    decoder.stored("leftBarlineType", target.leftBarlineType, measureExtendedFlagsSlot,
        barlineTypeOf(static_cast<std::uint16_t>((extendedFlags & extendedLeftBarlineMask) >> extendedLeftBarlineShift)), extendedFlags);

    if (decoder.layout().hasBackSpaceExtra()) {
        decoder.stored("backSpaceExtra", target.backSpaceExtra, measureBackSpaceExtraSlot, record.signedWord(measureBackSpaceExtraSlot),
            record.signedWord(measureBackSpaceExtraSlot));
    } else {
        decoder.behavior("backSpaceExtra", target.backSpaceExtra, 0);
    }
}

/// @brief Supplies the members no measure record of any supported era carries.
/// @details The two key-signature switches are the era's behavior -- Finale 2014 built them out of
/// words that are filler through Finale 2012, so no supported layout can carry either.
void decodeAbsentMembers(MeasureDecoder& decoder)
{
    auto& target = decoder.target();
    decoder.behavior("globalKeySig->keyless", target.globalKeySig->keyless, false);
    decoder.behavior("globalKeySig->hideKeySigShowAccis", target.globalKeySig->hideKeySigShowAccis, false);
}

/// @brief Reports one measure's decoded members, each at the offset its own word came from.
template <typename Reporting>
void reportMeasure(Reporting& reporting, const typename Reporting::InstanceKey& key, const MeasureRecord& record,
    const ReportState<DecodedMeasureFields>& fields, records::LegacyTag identity)
{
    reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
    for (const auto& field : reporting.state(fields)) {
        // A field may come from a record other than the measure's own, and then it cites that
        // record's offsets and identity rather than the measure record's.
        const auto& from = field.record ? *field.record : record;
        const bool located = field.origin == Reporting::Origin::LegacyMus && field.slot < from.blockOffsets.size();
        reporting.report().setField(key, reporting.memberName(field.member),
            typename Reporting::FieldInfo{field.origin, located ? from.blockOffsets[field.slot] : 0, located ? from.decodedOffsets[field.slot] : 0,
                field.stored, located ? std::optional<std::uint16_t>{field.record ? displayTimeSigTag : identity} : std::nullopt});
    }
}

/// @brief Decodes one score measure into a new pooled object.
void importOneMeasure(const ImportContext& context, const RecordFamilySource& source, const MeasureLayout& layout,
    std::span<const records::LegacyRow> rows, std::uint16_t partId, std::uint16_t cmper)
{
    auto instance = createOthersRecordTarget<MeasureTarget>(context.document, source, rows.front(), cmper);
    if (!instance) {
        return;
    }
    // musxdom guarantees the contained key signature only from its own integrity check, which
    // runs later than this. The decoder writes into it, so it has to exist first.
    if (!instance->globalKeySig) {
        instance->globalKeySig = std::make_shared<musx::dom::KeySignature>(context.document);
    }

    const auto record = readMeasureRecord(source, rows, context.profile.byteOrder);
    const auto display = readDisplayTimeSigRecord(context, source, layout, partId, cmper);
    MeasureDecoder decoder(context.report, record, layout, *instance, display ? &*display : nullptr);
    decodeCommonWords(decoder);
    decodePrimaryFlags(decoder);
    decodeShowModes(decoder);
    if (layout.flags == MeasureFlagLayout::Coda) {
        decodeCodaSecondaryFlags(decoder);
    } else {
        decodeLaterSecondaryFlags(decoder);
    }
    decodeDisplayAndSpacingWords(decoder);
    decodeAbsentMembers(decoder);

    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        reportMeasure(reporting, reporting.template instanceKey<MeasureTarget>(partId, cmper), record, decoder.fields(), source.identity);
    });
    context.document->getOthers()->add(MeasureTarget::XmlNodeName, std::move(instance));
}

/// @brief Overlays one part's compact record onto the part object copied from the score.
/// @details The part object already holds every score value, because @ref
/// createOthersRecordTarget initialized it from the score instance the way a partially linked
/// object is defined to be built. Only the four members the compact record carries are read here,
/// and every other member keeps both the value and the provenance the score measure established.
void importOneCompactPartMeasure(const ImportContext& context, const RecordFamilySource& source, const MeasureLayout& layout,
    const records::LegacyRow& row, std::uint16_t partId, std::uint16_t cmper, MeasureTarget& target)
{
    const auto record = readMeasureRecord(source, {&row, 1}, context.profile.byteOrder);
    MeasureDecoder decoder(context.report, record, layout, target);
    const auto flags = record.word(measureCompactFlagSlot);
    decoder.stored(
        "width", target.width, measureCompactWidthSlot, record.signedWord(measureCompactWidthSlot), record.signedWord(measureCompactWidthSlot));
    decoder.stored("positioningMode", target.positioningMode, measureCompactFlagSlot, positioningTypeOf(flags & primaryPositioningModeMask), flags);
    decoder.flag("pageBreak", target.pageBreak, measureCompactFlagSlot, measureCompactPageBreakMask);
    decoder.stored("frontSpaceExtra", target.frontSpaceExtra, measureCompactFrontSpaceExtraSlot, record.signedWord(measureCompactFrontSpaceExtraSlot),
        record.signedWord(measureCompactFrontSpaceExtraSlot));
    decoder.stored("backSpaceExtra", target.backSpaceExtra, measureCompactBackSpaceExtraSlot, record.signedWord(measureCompactBackSpaceExtraSlot),
        record.signedWord(measureCompactBackSpaceExtraSlot));

    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<MeasureTarget>(partId, cmper);
        // The inherited members are the score measure's values, and they carry the score measure's
        // provenance with them, offsets included: the part object holds them because the score
        // record supplied them. Copying the entries rather than re-deriving them is what keeps a
        // part object from reporting a value as unsourced that the score record plainly stated.
        const auto scoreKey = reporting.template instanceKey<MeasureTarget>(musx::dom::SCORE_PARTID, cmper);
        if (const auto found = reporting.report().fields.find(scoreKey); found != reporting.report().fields.end()) {
            reporting.report().fields[key] = found->second;
        }
        reportMeasure(reporting, key, record, decoder.fields(), source.identity);
    });
}

/// @brief Decodes one part's record, whichever of the two forms it takes.
void importOnePartMeasure(const ImportContext& context, const RecordFamilySource& source, const MeasureLayout& layout,
    std::span<const records::LegacyRow> rows, std::uint16_t partId, std::uint16_t cmper)
{
    auto instance = createOthersRecordTarget<MeasureTarget>(context.document, source, rows.front(), cmper);
    if (!instance) {
        return;
    }
    if (!instance->globalKeySig) {
        instance->globalKeySig = std::make_shared<musx::dom::KeySignature>(context.document);
    }
    auto* target = instance.get();
    const bool compact =
        rows.front().payloadSize == measureCompactPartPayloadSize && instance->getShareMode() == musx::dom::EnigmaBase::ShareMode::Partial;
    context.document->getOthers()->add(MeasureTarget::XmlNodeName, std::move(instance));

    if (compact) {
        importOneCompactPartMeasure(context, source, layout, rows.front(), partId, cmper, *target);
        return;
    }
    // A part record that is not compact states the whole measure in the score layout, so it is
    // decoded exactly as a score measure is.
    const auto record = readMeasureRecord(source, rows, context.profile.byteOrder);
    const auto display = readDisplayTimeSigRecord(context, source, layout, partId, cmper);
    MeasureDecoder decoder(context.report, record, layout, *target, display ? &*display : nullptr);
    decodeCommonWords(decoder);
    decodePrimaryFlags(decoder);
    decodeShowModes(decoder);
    if (layout.flags == MeasureFlagLayout::Coda) {
        decodeCodaSecondaryFlags(decoder);
    } else {
        decodeLaterSecondaryFlags(decoder);
    }
    decodeDisplayAndSpacingWords(decoder);
    decodeAbsentMembers(decoder);
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        reportMeasure(reporting, reporting.template instanceKey<MeasureTarget>(partId, cmper), record, decoder.fields(), source.identity);
    });
}

} // namespace

void importMeasures(const ImportContext& context)
{
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), measureTag, measureClass,
        /*details*/ false, measureCompactLayouts);
    if (!source) {
        return;
    }
    const auto layout = measureLayoutOf(context, *source);

    // Score first, which @ref recordKeys guarantees, because a part record overlays the score
    // object of the same comparator and cannot be built before it exists.
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        if (cmper == invalidMeasureCmper) {
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Verbose, "Measure record at comparator 0 is not a measure and was not imported."});
            continue;
        }
        if (partId == musx::dom::SCORE_PARTID) {
            importOneMeasure(context, *source, layout, rows, partId, cmper);
        } else {
            // A part has exactly the score's measures, so a part record past the score's end is corrupt.
            if (!context.document->getOthers()->get<musx::dom::others::Measure>(musx::dom::SCORE_PARTID, cmper)) {
                context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning,
                    "Part " + std::to_string(partId) + " stores measure " + std::to_string(cmper) + " beyond the score's last measure."});
            }
            importOnePartMeasure(context, *source, layout, rows, partId, cmper);
        }
    }
}

} // namespace others
} // namespace finale_mus_reader
