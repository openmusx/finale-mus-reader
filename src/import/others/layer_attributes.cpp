// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <iterator>
#include <map>
#include <span>
#include <utility>
#include <vector>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using LayerAttributesTarget = musx::dom::others::LayerAttributes;

// Layer attributes are ordinary other records rather than synthetic preferences: the record
// comparator is the 0-based layer index.
constexpr records::LegacyTag layerAttributesTag = records::packTag("LA");
constexpr records::LegacyTag layerAttributesClass = 0x00a3;

// The record is two live words inside a six-word row: a signed rest offset, then four reserved
// words, then one packed flag word.
constexpr std::uint32_t layerRestOffsetSlot = 0;
constexpr std::uint32_t layerFlagSlot = 5;

// Bit positions of the flag word. The record stores them as masks, and the two halves of the
// word arrived at different times: the upper group belongs to the original layer dialog, and
// playback, spacing, and the two hidden-material tests were added later, which is why they do
// not sit adjacent to it.
constexpr std::uint8_t layerIgnoreHiddenLayersBit = 0;      // 0x0001
constexpr std::uint8_t layerHideWhenInactiveBit = 1;        // 0x0002
constexpr std::uint8_t layerFlipTiesBit = 7;                // 0x0080
constexpr std::uint8_t layerFloatLayerBit = 8;              // 0x0100
constexpr std::uint8_t layerUseRestOffsetBit = 9;           // 0x0200
constexpr std::uint8_t layerFreezeStemsUpBit = 10;          // 0x0400
constexpr std::uint8_t layerFreezeLayerBit = 11;            // 0x0800
constexpr std::uint8_t layerPlaybackBit = 12;               // 0x1000
constexpr std::uint8_t layerAffectSpacingBit = 13;          // 0x2000
constexpr std::uint8_t layerIgnoreHiddenNotesBit = 14;      // 0x4000

// The fixed-row layout, used by every epoch through Finale 2006.
const FieldMapping layerFields[] = {
    MUS_WORD(LayerAttributesTarget, "LA", CMPER_FROM_TARGET, /*incidence*/ 0, layerRestOffsetSlot, restOffset),
    MUS_BIT(LayerAttributesTarget, "LA", CMPER_FROM_TARGET, 0, layerFlagSlot, layerIgnoreHiddenLayersBit, ignoreHiddenLayers),
    MUS_BIT(LayerAttributesTarget, "LA", CMPER_FROM_TARGET, 0, layerFlagSlot, layerHideWhenInactiveBit, hideLayer),
    MUS_BIT(LayerAttributesTarget, "LA", CMPER_FROM_TARGET, 0, layerFlagSlot, layerFlipTiesBit, freezTiesToStems),
    MUS_BIT(LayerAttributesTarget, "LA", CMPER_FROM_TARGET, 0, layerFlagSlot, layerFloatLayerBit, onlyIfOtherLayersHaveNotes),
    MUS_BIT(LayerAttributesTarget, "LA", CMPER_FROM_TARGET, 0, layerFlagSlot, layerUseRestOffsetBit, useRestOffset),
    MUS_BIT(LayerAttributesTarget, "LA", CMPER_FROM_TARGET, 0, layerFlagSlot, layerFreezeStemsUpBit, freezeStemsUp),
    MUS_BIT(LayerAttributesTarget, "LA", CMPER_FROM_TARGET, 0, layerFlagSlot, layerFreezeLayerBit, freezeLayer),
    MUS_BIT(LayerAttributesTarget, "LA", CMPER_FROM_TARGET, 0, layerFlagSlot, layerPlaybackBit, playback),
    MUS_BIT(LayerAttributesTarget, "LA", CMPER_FROM_TARGET, 0, layerFlagSlot, layerAffectSpacingBit, affectSpacing),
    MUS_BIT(LayerAttributesTarget, "LA", CMPER_FROM_TARGET, 0, layerFlagSlot, layerIgnoreHiddenNotesBit, ignoreHiddenNotesOnly),
};

// The zlib layout. The class record keeps the same word stream the fixed row carried, so its
// offsets are the slots above doubled rather than a second statement of the layout.
const FieldMapping classLayerFields[] = {
    MUS_CLASS_BITS(LayerAttributesTarget, layerAttributesClass, classWordOffset(layerRestOffsetSlot), 0, 0, restOffset),
    MUS_CLASS_BITS(LayerAttributesTarget, layerAttributesClass, classWordOffset(layerFlagSlot), layerIgnoreHiddenLayersBit, 1, ignoreHiddenLayers),
    MUS_CLASS_BITS(LayerAttributesTarget, layerAttributesClass, classWordOffset(layerFlagSlot), layerHideWhenInactiveBit, 1, hideLayer),
    MUS_CLASS_BITS(LayerAttributesTarget, layerAttributesClass, classWordOffset(layerFlagSlot), layerFlipTiesBit, 1, freezTiesToStems),
    MUS_CLASS_BITS(LayerAttributesTarget, layerAttributesClass, classWordOffset(layerFlagSlot), layerFloatLayerBit, 1, onlyIfOtherLayersHaveNotes),
    MUS_CLASS_BITS(LayerAttributesTarget, layerAttributesClass, classWordOffset(layerFlagSlot), layerUseRestOffsetBit, 1, useRestOffset),
    MUS_CLASS_BITS(LayerAttributesTarget, layerAttributesClass, classWordOffset(layerFlagSlot), layerFreezeStemsUpBit, 1, freezeStemsUp),
    MUS_CLASS_BITS(LayerAttributesTarget, layerAttributesClass, classWordOffset(layerFlagSlot), layerFreezeLayerBit, 1, freezeLayer),
    MUS_CLASS_BITS(LayerAttributesTarget, layerAttributesClass, classWordOffset(layerFlagSlot), layerPlaybackBit, 1, playback),
    MUS_CLASS_BITS(LayerAttributesTarget, layerAttributesClass, classWordOffset(layerFlagSlot), layerAffectSpacingBit, 1, affectSpacing),
    MUS_CLASS_BITS(LayerAttributesTarget, layerAttributesClass, classWordOffset(layerFlagSlot), layerIgnoreHiddenNotesBit, 1, ignoreHiddenNotesOnly),
};

// The six-word row a release writes for a layer it stores no record for. Every member is clear
// and the rest offset is zero, except that the layer always plays back and always affects music
// spacing, because no release that omits the row has a setting for either.
//
// This is behavior, not a synthesized default: it is what the layer did. On layers 0 and 1 it
// disagrees with the pinned baseline, which carries the modern new-document rest offset and
// freeze settings, so applying the baseline there would assert settings the document never had.
//
// **Believed** to occur only in Coda-banner and uncompressed documents. The test is the row's
// absence rather than a version, so a DCL or zlib document that omitted it is treated the same
// way. The cost is that a document whose record pool failed to frame is indistinguishable from
// one that never stored the row, and both get these values.
constexpr std::int16_t layerBehaviorFlags = static_cast<std::int16_t>((1U << layerPlaybackBit) | (1U << layerAffectSpacingBit));
constexpr std::int16_t layerBehaviorRow[records::otherWordCount] = {0, 0, 0, 0, 0, layerBehaviorFlags};

// Reads one mapped field out of that row through the field's own slot and bit range, so the
// layout is stated once in the tables above rather than again here. A class-record field
// addresses bytes, so its offset halves back to the slot the row is indexed by.
[[nodiscard]] std::int64_t layerBehaviorValue(const FieldMapping& field, bool classRecords)
{
    const auto slot = classRecords ? field.source.wordSlot / 2U : field.source.wordSlot;
    return extractBits(layerBehaviorRow[slot], field.source.bits);
}

// Playback and music spacing are the two members no release before Finale 2002 offers a setting
// for. Such a release always plays the layer back and always lets it affect spacing, and the bits
// that later carry those choices are clear, so reading them yields the opposite of what the era
// did. The value is known exactly while nothing in the file records it.
//
// The field is selected by the bit position its own mapping row states, so the two members are
// named once in the tables above rather than again here.
[[nodiscard]] bool isPreFinale2002Setting(const FieldMapping& field)
{
    return field.source.bits.bitCount == 1 && (field.source.bits.firstBit == layerPlaybackBit || field.source.bits.firstBit == layerAffectSpacingBit);
}

// **A version gate, and deliberately so.** The boundary falls inside the DCL epoch, between
// Finale 2001 and Finale 2002, so no epoch gate can express it. The record states nothing about
// which layout it uses -- it is six words in both -- and the only candidate marker is the bit
// content itself, which a Finale 2002 user who muted every layer would reproduce exactly. That is
// a guess about contents rather than a fact about shape, so the version decides. The cost is the
// documented one: a DCL document whose version cannot be recovered fails closed and keeps the
// stored bits.
[[nodiscard]] bool sourcePredatesLayerPlaybackSettings(const SourceProfile& profile)
{
    return sourcePredatesVersion(profile, FormatEpoch::DclLegacy, versions::finale2002);
}

// The destination for one source record: the seeded object of that identity where the baseline
// has one, and a new source-owned object where it does not.
//
// The registry hands every record identity the source carries to this, so a comparator outside
// the range musxdom reads still reaches the document. Such a comparator is something the file
// said, and discarding it here would be this reader deciding what a document may contain.
// Identity and share mode of a created object come from the row through the shared helper, so it
// is built exactly as any other source-owned others class would be -- including the part-scoped
// row that Finale's own UI cannot produce, since layer attributes are not unlinkable.
MappingTarget poolOrOverlayLayerTarget(
    const musx::dom::DocumentPtr& document, const RecordFamilySource& source, const records::LegacyRow& row, std::uint16_t cmper)
{
    for (const auto& existing : document->getOthers()->getAllSources<LayerAttributesTarget>(cmper)) {
        if (existing->getSourcePartId() != row.partId) {
            continue;
        }
        return makeMappingTarget(row.partId, cmper, const_cast<LayerAttributesTarget*>(existing.get()));
    }
    auto instance = createOthersRecordTarget<LayerAttributesTarget>(document, source, row, cmper);
    if (!instance) {
        return {};
    }
    auto* raw = instance.get();
    document->getOthers()->add(LayerAttributesTarget::XmlNodeName, std::move(instance));
    return makeMappingTarget(row.partId, cmper, raw);
}

const MappingTable& layerAttributesTable()
{
    static const MappingTable table{.reportPrefix = "others.layerAtts",
        // The Coda-banner epoch is covered deliberately, not by omission. A release writes the
        // row only once a layer setting leaves its default, so a document of any of these eras
        // may carry it or not, and the absence is handled by the pass below rather than by
        // narrowing this gate. Believed: an early row that exists uses the Finale
        // 3.7.2-through-2006 layout.
        .epochs = EpochMask::CodaBanner | EpochMask::FixedRow,
        .targetKind = TargetKind::OthersFromRecords,
        .recordIdentity = layerAttributesTag,
        .createTarget = &poolOrOverlayLayerTarget,
        .fields = layerFields,
        .fieldCount = std::size(layerFields)};
    return table;
}

const MappingTable& classLayerAttributesTable()
{
    static const MappingTable table{.reportPrefix = "others.layerAtts",
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OthersFromRecords,
        .recordIdentity = layerAttributesClass,
        .createTarget = &poolOrOverlayLayerTarget,
        .fields = classLayerFields,
        .fieldCount = std::size(classLayerFields)};
    return table;
}

/// @brief One instance's values as the pinned baseline seeded them, in the field order below.
using SeededLayerValues = std::map<std::pair<std::uint16_t, musx::dom::Cmper>, std::vector<std::int64_t>>;

/// @brief Records what the baseline seeded, before any table overwrites it.
/// @details The tables reach only the identities the source has a record for, so the objects
/// they never touch are exactly the ones the pass below has to finish. Taking the values here
/// rather than reading the reference document again is what keeps that comparison a fact about
/// this document: an identity absent from the result is one the baseline did not seed, and
/// nothing about it can be reported as a retained baseline default.
[[nodiscard]] SeededLayerValues captureSeededLayers(const musx::dom::DocumentPtr& document, std::span<const FieldMapping> fields)
{
    SeededLayerValues result;
    for (const auto& instance : document->getOthers()->getAllSources<LayerAttributesTarget>()) {
        std::vector<std::int64_t> values;
        values.reserve(fields.size());
        for (const auto& field : fields) {
            values.push_back(field.read(instance.get()));
        }
        result.emplace(std::pair{instance->getSourcePartId(), instance->getCmper()}, std::move(values));
    }
    return result;
}

/// @brief Supplies every member the era decides and the record did not.
/// @details Two cases, and one rule. A layer the file stores no row for takes the whole behavior
/// row, which is the case the tables cannot reach at all because no record named it. Playback and
/// music spacing take the era's value below Finale 2002 whether or not a row exists, because the
/// bit that would otherwise be read means nothing in those releases.
///
/// A value the baseline already supplies is not asserted again, per the options fallback rule, so
/// it stays @ref ValueOrigin::Finale27Default even where this overrode a stored bit to reach it:
/// the baseline was already right and the file was not.
void applyLayerEraBehavior(
    const ImportContext& context, const RecordFamilySource& source, std::span<const FieldMapping> fields, const SeededLayerValues& seeded)
{
    const bool predatesPlayback = sourcePredatesLayerPlaybackSettings(context.profile);
    for (const auto& instance : context.document->getOthers()->getAllSources<LayerAttributesTarget>()) {
        const auto partId = instance->getSourcePartId();
        const auto cmper = instance->getCmper();
        const bool stored = !source.pool->getArray(source.identity, cmper, 0, partId).empty();
        const auto wasSeeded = seeded.find({partId, cmper});
        for (std::size_t index = 0; index < fields.size(); ++index) {
            const auto& field = fields[index];
            if (stored && !(predatesPlayback && isPreFinale2002Setting(field))) {
                continue;
            }
            const auto value = layerBehaviorValue(field, source.classRecords);
            field.apply(const_cast<LayerAttributesTarget*>(instance.get()), value);
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const bool baselineSupplies = wasSeeded != seeded.end() && wasSeeded->second[index] == value;
                reporting.report().setField(reporting.template instanceKey<LayerAttributesTarget>(partId, cmper), field.fieldName,
                    typename Reporting::FieldInfo{
                        baselineSupplies ? Reporting::Origin::Finale27Default : Reporting::Origin::LegacyBehavior, 0, 0, value});
            });
        }
    }
}

} // namespace

void importLayerAttributes(const ImportContext& context)
{
    const auto source =
        selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), layerAttributesTag, layerAttributesClass);
    if (!source) {
        return;
    }
    const std::span<const FieldMapping> fields = source->classRecords ? std::span<const FieldMapping>(classLayerFields, std::size(classLayerFields))
                                                                      : std::span<const FieldMapping>(layerFields, std::size(layerFields));

    const auto seeded = captureSeededLayers(context.document, fields);
    applyMappingTables({&layerAttributesTable(), &classLayerAttributesTable()}, context.index, context.profile, context.document, context.report);
    // The tables reach one destination per source record. Everything left is a layer this file
    // says nothing about, plus the two members its release had no setting for.
    applyLayerEraBehavior(context, *source, fields, seeded);
}

} // namespace others
} // namespace finale_mus_reader
