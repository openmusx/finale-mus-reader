// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include <bit>
#include <cmath>
#include <iterator>
#include <span>
#include <vector>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using MusicSpacingTarget = musx::dom::options::MusicSpacingOptions;
using ColUnisonsChoice = MusicSpacingTarget::ColUnisonsChoice;
using GraceNoteSpacing = MusicSpacingTarget::GraceNoteSpacing;
using ManualPositioning = MusicSpacingTarget::ManualPositioning;

constexpr std::uint16_t musicSpacingSelector = 94;
constexpr std::uint16_t olderMusicSpacingSelector = 89;
constexpr std::size_t olderMusicSpacingWords = 13;
constexpr std::uint16_t avoidLedgerLinesMask = 0x0100;
constexpr std::uint16_t avoidAllUnisonsMask = 0x0080;
constexpr std::uint16_t avoidDifferentUnisonsMask = 0x0200;
constexpr std::uint16_t clearManualPositionsMask = 0x8000;
constexpr std::uint16_t useManualPositionsMask = 0x2000;
constexpr musx::dom::Evpu preFinale2000MinimumWidth = 360;
constexpr double scalingFactorDivisor = 10000.0;

bool hasAllotmentTail(const SourceProfile& profile)
{
    return sourceAtOrAfter(profile, FormatEpoch::DclLegacy, VersionBound{10, 0, 0, 0, 4});
}

constexpr ColUnisonsChoice decodeUnisons(std::int64_t flags)
{
    if ((flags & avoidAllUnisonsMask) != 0) {
        return ColUnisonsChoice::All;
    }
    if ((flags & avoidDifferentUnisonsMask) != 0) {
        return ColUnisonsChoice::DiffNoteheads;
    }
    return ColUnisonsChoice::None;
}

constexpr ManualPositioning decodeManualPositioning(std::int64_t flags)
{
    if ((flags & clearManualPositionsMask) != 0) {
        return ManualPositioning::Clear;
    }
    if ((flags & useManualPositionsMask) != 0) {
        return ManualPositioning::Incorporate;
    }
    return ManualPositioning::Ignore;
}

constexpr GraceNoteSpacing decodeGraceNoteSpacing(std::int64_t value)
{
    switch (value) {
    case 1: return GraceNoteSpacing::KeepCurrent;
    case 2: return GraceNoteSpacing::Automatic;
    default: return GraceNoteSpacing::ResetToEntry;
    }
}

// Selector 94 is one continuous word stream. The fixed-row encodings split it into
// six-word incidences; the zlib encoding coalesces the same words into class 108.
const FieldMapping fixedRowSpacingFields[] = {
    MUS_WORD(MusicSpacingTarget, "39", GLOBALS_CMPER, 0, 0, musFront),
    MUS_WORD(MusicSpacingTarget, "39", GLOBALS_CMPER, 0, 1, musBack),
    MUS_WORD(MusicSpacingTarget, "94", GLOBALS_CMPER, 0, 1, minWidth),
    MUS_WORD(MusicSpacingTarget, "94", GLOBALS_CMPER, 0, 2, maxWidth),
    MUS_WORD(MusicSpacingTarget, "94", GLOBALS_CMPER, 0, 3, minDistance),
    MUS_WORD(MusicSpacingTarget, "94", GLOBALS_CMPER, 0, 4, minDistTiedNotes),
    MUS_BIT(MusicSpacingTarget, "94", GLOBALS_CMPER, 0, 5, 0, avoidColNotes),
    MUS_BIT(MusicSpacingTarget, "94", GLOBALS_CMPER, 0, 5, 1, avoidColLyrics),
    MUS_BIT(MusicSpacingTarget, "94", GLOBALS_CMPER, 0, 5, 2, avoidColChords),
    MUS_BIT(MusicSpacingTarget, "94", GLOBALS_CMPER, 0, 5, 3, avoidColArtics),
    MUS_BIT(MusicSpacingTarget, "94", GLOBALS_CMPER, 0, 5, 5, avoidColClefs),
    MUS_BIT(MusicSpacingTarget, "94", GLOBALS_CMPER, 0, 5, 6, avoidColSeconds),
    MUS_FIELD_AS_IF(MusicSpacingTarget, "94", GLOBALS_CMPER, 0, 5, ValueWidth::Word, LongWordOrder::HighFirst, BitRange{}, nullptr, nullptr,
        avoidColUnisons, decodeUnisons(value)),
    MUS_FIELD_AS_IF(MusicSpacingTarget, "94", GLOBALS_CMPER, 0, 5, ValueWidth::Word, LongWordOrder::HighFirst, BitRange{}, nullptr, nullptr,
        avoidColLedgers, (value & avoidLedgerLinesMask) == 0),
    MUS_FIELD_AS_IF(MusicSpacingTarget, "94", GLOBALS_CMPER, 0, 5, ValueWidth::Word, LongWordOrder::HighFirst, BitRange{}, nullptr, nullptr,
        manualPositioning, decodeManualPositioning(value)),
    MUS_BIT(MusicSpacingTarget, "94", GLOBALS_CMPER, 0, 5, 11, ignoreHidden),
    MUS_BIT(MusicSpacingTarget, "94", GLOBALS_CMPER, 0, 5, 14, interpolateAllotments),
    MUS_BIT(MusicSpacingTarget, "94", GLOBALS_CMPER, 0, 5, 12, usePrinter),
    MUS_WORD_AS_IF(MusicSpacingTarget, "94", GLOBALS_CMPER, 1, 0, nullptr, useAllottmentTables, value != 0),
    MUS_LONG(MusicSpacingTarget, "94", GLOBALS_CMPER, 1, 1, LongWordOrder::LowFirst, referenceDuration),
    MUS_LONG(MusicSpacingTarget, "94", GLOBALS_CMPER, 1, 3, LongWordOrder::LowFirst, referenceWidth),
    MUS_FIELD_AS_IF(MusicSpacingTarget, "94", GLOBALS_CMPER, 1, 5, ValueWidth::Long, LongWordOrder::LowFirst, BitRange{}, nullptr, nullptr,
        scalingFactor, static_cast<double>(value) / scalingFactorDivisor),
    MUS_WORD_IF_SOURCE(MusicSpacingTarget, "94", GLOBALS_CMPER, 2, 1, &hasAllotmentTail, defaultAllotment),
    MUS_FIELD_AS_IF(MusicSpacingTarget, "94", GLOBALS_CMPER, 2, 2, ValueWidth::Long, LongWordOrder::LowFirst, BitRange{}, &hasAllotmentTail, nullptr,
        minDistGrace, value),
    MUS_FIELD_AS_IF(MusicSpacingTarget, "94", GLOBALS_CMPER, 2, 4, ValueWidth::Word, LongWordOrder::HighFirst, BitRange{}, &hasAllotmentTail, nullptr,
        graceNoteSpacing, decodeGraceNoteSpacing(value)),
};

const FieldMapping classRecordSpacingFields[] = {
    MUS_CLASS_WORD(MusicSpacingTarget, numericGlobalClass(39), GLOBALS_CMPER, classWordOffset(0), musFront),
    MUS_CLASS_WORD(MusicSpacingTarget, numericGlobalClass(39), GLOBALS_CMPER, classWordOffset(1), musBack),
    MUS_CLASS_WORD(MusicSpacingTarget, numericGlobalClass(musicSpacingSelector), GLOBALS_CMPER, classWordOffset(1), minWidth),
    MUS_CLASS_WORD(MusicSpacingTarget, numericGlobalClass(musicSpacingSelector), GLOBALS_CMPER, classWordOffset(2), maxWidth),
    MUS_CLASS_WORD(MusicSpacingTarget, numericGlobalClass(musicSpacingSelector), GLOBALS_CMPER, classWordOffset(3), minDistance),
    MUS_CLASS_WORD(MusicSpacingTarget, numericGlobalClass(musicSpacingSelector), GLOBALS_CMPER, classWordOffset(4), minDistTiedNotes),
    MUS_CLASS_BIT(MusicSpacingTarget, numericGlobalClass(musicSpacingSelector), GLOBALS_CMPER, classWordOffset(5), 0, avoidColNotes),
    MUS_CLASS_BIT(MusicSpacingTarget, numericGlobalClass(musicSpacingSelector), GLOBALS_CMPER, classWordOffset(5), 1, avoidColLyrics),
    MUS_CLASS_BIT(MusicSpacingTarget, numericGlobalClass(musicSpacingSelector), GLOBALS_CMPER, classWordOffset(5), 2, avoidColChords),
    MUS_CLASS_BIT(MusicSpacingTarget, numericGlobalClass(musicSpacingSelector), GLOBALS_CMPER, classWordOffset(5), 3, avoidColArtics),
    MUS_CLASS_BIT(MusicSpacingTarget, numericGlobalClass(musicSpacingSelector), GLOBALS_CMPER, classWordOffset(5), 5, avoidColClefs),
    MUS_CLASS_BIT(MusicSpacingTarget, numericGlobalClass(musicSpacingSelector), GLOBALS_CMPER, classWordOffset(5), 6, avoidColSeconds),
    MUS_CLASS_FIELD_AS_IF(MusicSpacingTarget, numericGlobalClass(musicSpacingSelector), GLOBALS_CMPER, classWordOffset(5), ValueWidth::Word,
        LongWordOrder::HighFirst, BitRange{}, nullptr, avoidColUnisons, decodeUnisons(value)),
    MUS_CLASS_FIELD_AS_IF(MusicSpacingTarget, numericGlobalClass(musicSpacingSelector), GLOBALS_CMPER, classWordOffset(5), ValueWidth::Word,
        LongWordOrder::HighFirst, BitRange{}, nullptr, avoidColLedgers, (value & avoidLedgerLinesMask) == 0),
    MUS_CLASS_FIELD_AS_IF(MusicSpacingTarget, numericGlobalClass(musicSpacingSelector), GLOBALS_CMPER, classWordOffset(5), ValueWidth::Word,
        LongWordOrder::HighFirst, BitRange{}, nullptr, manualPositioning, decodeManualPositioning(value)),
    MUS_CLASS_BIT(MusicSpacingTarget, numericGlobalClass(musicSpacingSelector), GLOBALS_CMPER, classWordOffset(5), 11, ignoreHidden),
    MUS_CLASS_BIT(MusicSpacingTarget, numericGlobalClass(musicSpacingSelector), GLOBALS_CMPER, classWordOffset(5), 14, interpolateAllotments),
    MUS_CLASS_BIT(MusicSpacingTarget, numericGlobalClass(musicSpacingSelector), GLOBALS_CMPER, classWordOffset(5), 12, usePrinter),
    MUS_CLASS_WORD_AS_IF(
        MusicSpacingTarget, numericGlobalClass(musicSpacingSelector), GLOBALS_CMPER, classWordOffset(6), nullptr, useAllottmentTables, value != 0),
    MUS_CLASS_LONG(
        MusicSpacingTarget, numericGlobalClass(musicSpacingSelector), GLOBALS_CMPER, classWordOffset(7), LongWordOrder::LowFirst, referenceDuration),
    MUS_CLASS_LONG(
        MusicSpacingTarget, numericGlobalClass(musicSpacingSelector), GLOBALS_CMPER, classWordOffset(9), LongWordOrder::LowFirst, referenceWidth),
    MUS_CLASS_FIELD_AS_IF(MusicSpacingTarget, numericGlobalClass(musicSpacingSelector), GLOBALS_CMPER, classWordOffset(11), ValueWidth::Long,
        LongWordOrder::LowFirst, BitRange{}, nullptr, scalingFactor, static_cast<double>(value) / scalingFactorDivisor),
    MUS_CLASS_WORD(MusicSpacingTarget, numericGlobalClass(musicSpacingSelector), GLOBALS_CMPER, classWordOffset(13), defaultAllotment),
    MUS_CLASS_LONG(
        MusicSpacingTarget, numericGlobalClass(musicSpacingSelector), GLOBALS_CMPER, classWordOffset(14), LongWordOrder::LowFirst, minDistGrace),
    MUS_CLASS_WORD_AS_IF(MusicSpacingTarget, numericGlobalClass(musicSpacingSelector), GLOBALS_CMPER, classWordOffset(16), nullptr, graceNoteSpacing,
        decodeGraceNoteSpacing(value)),
};

bool hasOlderMusicSpacing(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return readGlobalWords(index, profile, olderMusicSpacingSelector).words.size() >= olderMusicSpacingWords;
}

bool hasNewerMusicSpacing(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return !hasOlderMusicSpacing(index, profile);
}

const MappingTable& fixedRowMusicSpacingTable()
{
    static const MappingTable table{.reportPrefix = "options.musicSpacing",
        .epochs = EpochMask::CodaBanner | EpochMask::FixedRow,
        .applies = &hasNewerMusicSpacing,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<MusicSpacingTarget>,
        .fields = fixedRowSpacingFields,
        .fieldCount = std::size(fixedRowSpacingFields)};
    return table;
}

const MappingTable& classRecordMusicSpacingTable()
{
    static const MappingTable table{.reportPrefix = "options.musicSpacing",
        .epochs = EpochMask::Zlib,
        .applies = &hasNewerMusicSpacing,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<MusicSpacingTarget>,
        .fields = classRecordSpacingFields,
        .fieldCount = std::size(classRecordSpacingFields)};
    return table;
}

std::uint32_t swapSpacingWords(std::uint32_t value)
{
    return (value >> 16) | (value << 16);
}

template <std::uint32_t Maximum, std::uint32_t Fallback>
std::optional<std::int64_t> repairOlderSpacingInteger(std::int64_t value, const records::LegacyRecordIndex&, const SourceProfile&)
{
    if (value >= 1 && value <= Maximum) {
        return std::nullopt;
    }
    const auto reversed = swapSpacingWords(static_cast<std::uint32_t>(value));
    return reversed >= 1 && reversed <= Maximum ? reversed : Fallback;
}

std::optional<std::int64_t> repairOlderSpacingScaling(std::int64_t value, const records::LegacyRecordIndex&, const SourceProfile&)
{
    if (value >= 10000 && value <= 20000) {
        return std::nullopt;
    }
    const auto reversed = std::bit_cast<float>(swapSpacingWords(std::bit_cast<std::uint32_t>(static_cast<float>(value) * 0.0001F)));
    return std::isfinite(reversed) && reversed >= 1.0F && reversed <= 2.0F ? static_cast<std::int64_t>(std::lround(reversed * 10000.0F)) : 16180;
}

// The older family has the same first thirteen words and takes precedence when both exist.
std::vector<FieldMapping> olderSpacingFields(std::span<const FieldMapping> fields, bool classRecords)
{
    std::vector<FieldMapping> result;
    const auto identity = classRecords ? numericGlobalClass(musicSpacingSelector) : numericGlobalTag(musicSpacingSelector);
    for (auto field : fields) {
        const auto word = classRecords ? field.source.wordSlot / 2 : field.source.incidence * 6 + field.source.wordSlot;
        if (field.source.identity == identity && word < olderMusicSpacingWords) {
            field.source.identity = classRecords ? numericGlobalClass(olderMusicSpacingSelector) : numericGlobalTag(olderMusicSpacingSelector);
            if (word == 7) {
                field.sourceAdjustment = &repairOlderSpacingInteger<16384, 1024>;
            } else if (word == 9) {
                field.sourceAdjustment = &repairOlderSpacingInteger<1152, 84>;
            } else if (word == 11) {
                field.sourceAdjustment = &repairOlderSpacingScaling;
            }
            result.push_back(field);
        } else if (field.source.identity != identity) {
            result.push_back(field);
        }
    }
    return result;
}

const MappingTable& olderMusicSpacingTable(bool classRecords)
{
    static const auto fixedFields = olderSpacingFields(fixedRowSpacingFields, false);
    static const auto classFields = olderSpacingFields(classRecordSpacingFields, true);
    static const MappingTable fixedTable{.reportPrefix = "options.musicSpacing",
        .epochs = EpochMask::CodaBanner | EpochMask::FixedRow,
        .applies = &hasOlderMusicSpacing,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<MusicSpacingTarget>,
        .fields = fixedFields.data(),
        .fieldCount = fixedFields.size()};
    static const MappingTable classTable{.reportPrefix = "options.musicSpacing",
        .epochs = EpochMask::Zlib,
        .applies = &hasOlderMusicSpacing,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<MusicSpacingTarget>,
        .fields = classFields.data(),
        .fieldCount = classFields.size()};
    return classRecords ? classTable : fixedTable;
}

void applyAvoidColStemsBehavior(MusicSpacingTarget& target, ImportReport& report)
{
    withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
        const auto origin = target.avoidColStems ? Reporting::Origin::LegacyBehavior : Reporting::Origin::MusxOnly;
        // Legacy formats predate stem collision avoidance. A true seed must be overridden;
        // a false seed already represents the MUSX-only setting without an override.
        reporting.report().setField(reporting.template instanceKey<MusicSpacingTarget>(), "avoidColStems", {origin, 0, 0, 0});
    });
    target.avoidColStems = false;
}

void applyPreSelector94Behavior(
    const records::LegacyRecordIndex& index, const SourceProfile& profile, MusicSpacingTarget& target, ImportReport& report)
{
    if (readGlobalWords(index, profile, musicSpacingSelector).present || hasOlderMusicSpacing(index, profile)) {
        return;
    }
    // Selector 94 stores the document-level choices that supersede these behaviors.
    // Its presence is authoritative even where the source version is unavailable.
    target.useAllottmentTables = true;
    target.avoidColUnisons = ColUnisonsChoice::None;
    target.ignoreHidden = true;
    target.minWidth = preFinale2000MinimumWidth;
    withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
        reporting.template behaviorField<MusicSpacingTarget>("useAllottmentTables", 1);
        reporting.template behaviorField<MusicSpacingTarget>("avoidColUnisons", 0);
        reporting.template behaviorField<MusicSpacingTarget>("ignoreHidden", 1);
        reporting.template behaviorField<MusicSpacingTarget>("minWidth", preFinale2000MinimumWidth);
    });
}

void applyEarlierSpacingTail(MusicSpacingTarget& target, ImportReport& report)
{
    // The older family has no independent allotment or grace-note tail.
    const auto apply = [&](auto& member, const char* name, auto value) {
        if (member != value) {
            member = value;
            withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
                reporting.template behaviorField<MusicSpacingTarget>(name, static_cast<std::int64_t>(value));
            });
        }
    };
    apply(target.defaultAllotment, "defaultAllotment", 84);
    apply(target.minDistGrace, "minDistGrace", 12);
    apply(target.graceNoteSpacing, "graceNoteSpacing", GraceNoteSpacing::Automatic);
}

} // namespace

void importMusicSpacingOptions(const ImportContext& context)
{
    applyMappingTables({&fixedRowMusicSpacingTable(), &classRecordMusicSpacingTable(), &olderMusicSpacingTable(false), &olderMusicSpacingTable(true)},
        context.index, context.profile, context.document, context.report);
    if (const auto target = context.document->getOptions()->get<MusicSpacingTarget>()) {
        auto& mutableTarget = *const_cast<MusicSpacingTarget*>(target.get());
        applyPreSelector94Behavior(context.index, context.profile, mutableTarget, context.report);
        applyAvoidColStemsBehavior(mutableTarget, context.report);
        if (!hasAllotmentTail(context.profile) || hasOlderMusicSpacing(context.index, context.profile)) {
            applyEarlierSpacingTail(mutableTarget, context.report);
        }
    }
}

} // namespace options
} // namespace finale_mus_reader
