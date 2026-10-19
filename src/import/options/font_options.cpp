// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include "import/options/test_access.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using FontDefinition = musx::dom::others::FontDefinition;
using FontInfo = musx::dom::FontInfo;
using FontOptionsTarget = musx::dom::options::FontOptions;
using FontType = FontOptionsTarget::FontType;

constexpr std::uint16_t fontOptionsSelector = 24;
constexpr std::size_t tupleFieldCount = 3;
constexpr std::size_t wordSize = 2;
constexpr std::size_t tupleByteSize = tupleFieldCount * wordSize;
constexpr std::size_t fontTypeCount = static_cast<std::size_t>(FontType::TimePlusParts) + 1;
// Before Finale 2012 this ordinal is a drawing-time tablature scratch slot, not a setting.
constexpr std::size_t legacyTablatureHoldingSlot = 13;

enum class TupleField : std::size_t {
    FontId = 0,
    Size = 1,
    Effects = 2
};

struct FontOptionsLayout
{
    EpochMask epochs{};
    RecordEncoding encoding{};
    records::LegacyTag identity{};
};

struct EarlyField
{
    records::LegacyTag identity{};
    std::uint32_t word{};
};

struct EarlyTuple
{
    FontType type{};
    std::array<EarlyField, tupleFieldCount> fields{};
};

// One row per variable-length physical representation. Tuple counts come from the file.
//
// The fixed-row row is gated on the epoch rather than on a version. Selector 24 is the
// default-font array in every epoch that uses fixed rows except the Coda-banner era, where it
// holds one row of unrelated values, and `EpochMask::FixedRow` excludes exactly that era. A
// version gate would add nothing here and could only cost files, since the era boundary and
// the epoch boundary are the same one.
const std::array<FontOptionsLayout, 2> layouts{{
    {EpochMask::FixedRow, RecordEncoding::FixedRow, records::packTag("24")},
    {EpochMask::Zlib, RecordEncoding::ClassRecord, numericGlobalClass(fontOptionsSelector)},
}};

constexpr EarlyField earlyField(std::string_view tag, std::uint32_t word)
{
    return {records::packTag(tag), word};
}

constexpr EarlyTuple contiguousEarlyTuple(FontType type, std::string_view tag, std::uint32_t firstWord)
{
    return {type, {earlyField(tag, firstWord), earlyField(tag, firstWord + 1), earlyField(tag, firstWord + 2)}};
}

// Established except for StaffNames: Finale 1.0 calls the source preference "Name", and this
// era has exactly one of it where Finale 3.0 and later store four. **Believed:** that one
// preference continues as StaffNames. See @ref codaNameCompanionTypes.
const std::array<EarlyTuple, 14> earlyCodaTuples{{
    contiguousEarlyTuple(FontType::Music, "02", 0),
    contiguousEarlyTuple(FontType::Key, "03", 3),
    {FontType::Clef, {earlyField("04", 0), earlyField("39", 4), earlyField("39", 5)}},
    contiguousEarlyTuple(FontType::Time, "03", 0),
    contiguousEarlyTuple(FontType::Chord, "02", 3),
    contiguousEarlyTuple(FontType::ChordAcci, "37", 0),
    contiguousEarlyTuple(FontType::Ending, "05", 0),
    contiguousEarlyTuple(FontType::Tuplet, "36", 0),
    contiguousEarlyTuple(FontType::TextBlock, "26", 0),
    contiguousEarlyTuple(FontType::LyricVerse, "26", 3),
    contiguousEarlyTuple(FontType::LyricChorus, "27", 0),
    contiguousEarlyTuple(FontType::LyricSection, "27", 3),
    contiguousEarlyTuple(FontType::StaffNames, "04", 3),
    // Words 1-3 of the multimeasure-rest defaults, which hold the rest number's font in this
    // era. Finale 3.0 stopped storing it there.
    contiguousEarlyTuple(FontType::MultiMeasRest, "25", 1),
}};

// The Coda-banner era exposes a single "Name" font preference. Finale 3.0 split it into the
// four modern name types, which that era stores as separate tuples at physical ordinals 31,
// 32, 33 and 39 -- so this fan-out belongs to the Coda epoch alone and must not be applied
// to any later one.
//
// Recovering only StaffNames would leave the other three at Finale 27 defaults and so emit a
// document whose staff names use one face and size while its group and abbreviated names use
// another. The source never had that split, and neither does the Finale 27 baseline, which
// sets all four identically. Propagating the one preference to all four is the reading that
// preserves the source's own behavior.
//
// This deliberately diverges from what Finale's own upgrade produces, and the divergence is
// open to revision: that upgrade's output for these four types tracks the default file it ran
// under rather than the source document, so it does not settle the question. The three propagated types report as ValueOrigin::LegacyBehavior,
// not LegacyMus: the bytes are read from the source, but the assignment restores an era
// behavior rather than an option the source stored.
constexpr std::array<FontType, 3> codaNameCompanionTypes{
    FontType::AbbrvStaffNames,
    FontType::GroupNames,
    FontType::AbbrvGroupNames,
};

const FontOptionsLayout* layoutFor(const SourceProfile& profile)
{
    for (const auto& layout : layouts) {
        if (sourceMatches(profile, layout.epochs)) {
            return &layout;
        }
    }
    return nullptr;
}

std::size_t tupleCount(const FontOptionsLayout& layout, const records::LegacyRecordIndex& index, ImportReport& report)
{
    if (layout.encoding == RecordEncoding::FixedRow) {
        const auto rows = index.getOthers().getArray(layout.identity, GLOBALS_CMPER);
        return rows.size() * records::otherWordCount / tupleFieldCount;
    }

    const auto* row = index.getClassOthers().get(layout.identity, GLOBALS_CMPER, 0, 0);
    if (!row) {
        return 0;
    }
    const auto bytes = index.getClassOthers().effectivePayloadOf(*row);
    if (const auto trailing = bytes.size() % tupleByteSize; trailing != 0) {
        report.diagnostics.push_back({musx::util::Logger::LogLevel::Verbose,
            "Ignored " + std::to_string(trailing) + " trailing byte(s) after the last complete legacy font-options tuple."});
    }
    return bytes.size() / tupleByteSize;
}

SourceLocation sourceFor(const FontOptionsLayout& layout, std::size_t ordinal, TupleField field)
{
    const auto fieldIndex = static_cast<std::size_t>(field);
    SourceLocation result;
    result.identity = layout.identity;
    result.selector = GLOBALS_CMPER;
    result.width = ValueWidth::Word;
    if (layout.encoding == RecordEncoding::FixedRow) {
        const auto absoluteWord = ordinal * tupleFieldCount + fieldIndex;
        result.incidence = static_cast<std::uint32_t>(absoluteWord / records::otherWordCount);
        result.wordSlot = static_cast<std::uint32_t>(absoluteWord % records::otherWordCount);
    } else {
        result.incidence = 0;
        result.wordSlot = static_cast<std::uint32_t>(ordinal * tupleByteSize + fieldIndex * wordSize);
    }
    return result;
}

std::optional<ResolvedValue> readTupleField(
    const FontOptionsLayout& layout, const records::LegacyRecordIndex& index, const SourceProfile& profile, std::size_t ordinal, TupleField field)
{
    return readSourceValue(index, layout.encoding, GLOBALS_CMPER, sourceFor(layout, ordinal, field), profile.byteOrder);
}

std::optional<ResolvedValue> readEarlyTupleField(
    const records::LegacyRecordIndex& index, const EarlyTuple& tuple, TupleField field, ByteOrder byteOrder)
{
    const auto& fieldSource = tuple.fields[static_cast<std::size_t>(field)];
    SourceLocation source;
    source.identity = fieldSource.identity;
    source.selector = GLOBALS_CMPER;
    source.incidence = 0;
    source.wordSlot = fieldSource.word;
    source.width = ValueWidth::Word;
    return readSourceValue(index, RecordEncoding::FixedRow, GLOBALS_CMPER, source, byteOrder);
}

template <typename Reporting>
void reportField(Reporting& reporting, FontType type, const char* member, typename Reporting::Origin origin, std::int64_t rawValue,
    std::size_t blockOffset = 0, std::size_t decodedOffset = 0)
{
    reporting.report().setField(reporting.template instanceKey<FontOptionsTarget>(),
        "fonts[" + std::to_string(static_cast<std::size_t>(type)) + "]." + member, {origin, blockOffset, decodedOffset, rawValue});
}

template <typename Reporting>
void reportPhysicalField(Reporting& reporting, std::size_t ordinal, const char* member, const ResolvedValue& source, std::int64_t rawValue)
{
    reporting.report().setField(reporting.template instanceKey<FontOptionsTarget>(), "physical[" + std::to_string(ordinal) + "]." + member,
        {Reporting::Origin::LegacyMus, source.blockOffset, source.decodedOffset, rawValue});
}

template <typename Reporting>
void reportPhysicalTuple(
    Reporting& reporting, std::size_t ordinal, const ResolvedValue& fontId, const ResolvedValue& size, const ResolvedValue& effects)
{
    reportPhysicalField(reporting, ordinal, "fontId", fontId, static_cast<std::uint16_t>(fontId.value));
    reportPhysicalField(reporting, ordinal, "fontSize", size, static_cast<std::int16_t>(size.value));
    reportPhysicalField(reporting, ordinal, "effects", effects, static_cast<std::uint16_t>(effects.value));
}

/// @brief Maps a stored tuple position to the modern FontType it means.
/// @details Two layouts exist, and which applies is decided by epoch first. The uncompressed
/// and DCL epochs are entirely pre-2012, so they need no version test at all and keep working
/// on a file whose header version cannot be recovered. Only the zlib epoch spans the boundary,
/// so only it carries a version gate, framed inside that epoch.
///
/// Epoch has to lead here rather than version: major 12 occurs in both the DCL epoch
/// (Finale 2006) and the zlib epoch (Finale 2007), so no version range separates them.
std::optional<FontType> semanticType(const SourceProfile& profile, std::size_t physicalOrdinal)
{
    // Believed: the modern physical ordinals begin in Finale 2012. Earlier sources use the
    // legacy layout, in which tablature occupies ordinal 28 and percussion has no tuple.
    if (sourceAtOrAfter(profile, FormatEpoch::ZlibLegacy, versions::finale2012)) {
        if (physicalOrdinal < fontTypeCount) {
            return static_cast<FontType>(physicalOrdinal);
        }
        return std::nullopt;
    }

    // A zlib file whose version will not parse falls here, which is the safe direction: the
    // pre-2012 layout is what every epoch before it uses, and the epochs that cannot reach
    // 2012 at all never consult the version.
    if (physicalOrdinal == legacyTablatureHoldingSlot) {
        return std::nullopt;
    }
    if (physicalOrdinal == 28) {
        return FontType::Tablature;
    }
    if (physicalOrdinal < fontTypeCount) {
        return static_cast<FontType>(physicalOrdinal);
    }
    return std::nullopt;
}

bool isZeroTuple(const ResolvedValue& fontId, const ResolvedValue& size, const ResolvedValue& effects)
{
    return fontId.value == 0 && size.value == 0 && effects.value == 0;
}

bool isStructuralTupleFill(
    std::size_t tupleCount, std::size_t physicalOrdinal, const ResolvedValue& fontId, const ResolvedValue& size, const ResolvedValue& effects)
{
    // A fixed 16-byte other row has a four-byte header and room for exactly two
    // six-byte tuples. The zlib representation preserves the same 12-byte tuple-pair
    // grouping, consistent with Finale's plug-in-facing Enigma record model. When the
    // logical collection has an odd size, Finale zero-fills the second tuple of the final
    // pair. This is structural completion, not a terminator or a semantic tuple limit.
    return physicalOrdinal + 1 == tupleCount && physicalOrdinal % 2 == 1 && isZeroTuple(fontId, size, effects);
}

musx::dom::Cmper resolveReferenceFont(
    const musx::dom::DocumentPtr& document, const musx::dom::DocumentPtr& referenceDocument, musx::dom::Cmper referenceId, ImportReport& report);

struct ResolvedFontTuple
{
    musx::dom::Cmper fontId{};
    int fontSize{};
    std::uint16_t effects{};
    bool fromReference{};
};

ResolvedFontTuple resolveRecoveredTuple(const musx::dom::DocumentPtr& document, const musx::dom::DocumentPtr& referenceDocument, FontType type,
    musx::dom::Cmper fontId, int fontSize, std::uint16_t effects, ImportReport& report)
{
    if (document->getOthers()->get<FontDefinition>(musx::dom::SCORE_PARTID, fontId)) {
        return {fontId, fontSize, effects, false};
    }
    const auto reference = referenceDocument->getOptions()->get<FontOptionsTarget>();
    if (!reference) {
        throw std::logic_error("FontOptions reference document is incomplete");
    }
    const auto referenceFont = reference->getFontInfo(type);

    // A missing MUS definition leaves no name to reconcile, so select the reference
    // definition by semantic FontOptions type and take its entire tuple. Point size and
    // effects are face-dependent; retaining either source value would combine values that
    // existed in neither document. The imported reference face may reuse an existing target
    // definition by normalized name or occupy the next available comparator.
    const auto resolvedId = resolveReferenceFont(document, referenceDocument, referenceFont->fontId, report);
    report.diagnostics.push_back({musx::util::Logger::LogLevel::Verbose,
        "Legacy FontOptions type " + std::to_string(static_cast<std::size_t>(type)) + " referenced missing font definition " + std::to_string(fontId)
            + "; used the same-type Finale 27 reference font, size and effects as target"
              " font id "
            + std::to_string(resolvedId) + '.'});
    return {resolvedId, referenceFont->fontSize, referenceFont->getEnigmaStyles(), true};
}

void insertRecoveredTuple(const musx::dom::DocumentPtr& document, const musx::dom::DocumentPtr& referenceDocument,
    const std::shared_ptr<FontOptionsTarget>& target, FontType type, const ResolvedValue& fontId, const ResolvedValue& size,
    const ResolvedValue& effects, ImportReport& report, musx::factory::ConstructionContext& construction, bool inheritedTuple = false)
{
    const auto resolved = resolveRecoveredTuple(document, referenceDocument, type, musx::dom::Cmper(static_cast<std::uint16_t>(fontId.value)),
        static_cast<std::int16_t>(size.value), static_cast<std::uint16_t>(effects.value), report);
    auto font = std::make_shared<FontInfo>(document);
    font->fontId = construction.assignFontId(resolved.fontId);
    font->fontSize = resolved.fontSize;
    font->setEnigmaStyles(resolved.effects);
    target->fontOptions.insert_or_assign(type, std::move(font));

    withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
        const auto resolvedOrigin = resolved.fromReference ? Reporting::Origin::Finale27Default
                                    : inheritedTuple       ? Reporting::Origin::LegacyBehavior
                                                           : Reporting::Origin::LegacyMus;
        reportField(reporting, type, "fontId", resolvedOrigin, resolved.fontId, resolved.fromReference ? 0 : fontId.blockOffset,
            resolved.fromReference ? 0 : fontId.decodedOffset);
        reportField(reporting, type, "fontSize", resolvedOrigin, resolved.fontSize, resolved.fromReference ? 0 : size.blockOffset,
            resolved.fromReference ? 0 : size.decodedOffset);
        reportField(reporting, type, "effects", resolvedOrigin, resolved.effects, resolved.fromReference ? 0 : effects.blockOffset,
            resolved.fromReference ? 0 : effects.decodedOffset);
    });
}

/// @brief Resolves a reference font into this document, reporting when it cannot be done.
/// @details musxdom owns the rule: match by normalized name, never by comparator, and treat 0 as
/// the default-music-font sentinel. This only adapts the result to the importer's reporting, which
/// is why the legacy side keeps no font-matching logic of its own.
musx::dom::Cmper resolveReferenceFont(
    const musx::dom::DocumentPtr& document, const musx::dom::DocumentPtr& referenceDocument, musx::dom::Cmper referenceId, ImportReport& report)
{
    const auto referenceFont = referenceDocument->getOthers()->get<FontDefinition>(musx::dom::SCORE_PARTID, referenceId);
    if (!referenceFont) {
        report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning,
            "Finale 27 FontOptions referenced missing font definition " + std::to_string(referenceId) + "; substituted font id 0."});
        return 0;
    }
    if (const auto resolved = musx::dom::importFontDefinitionInto(document, referenceFont, baselineObjectReporter(report))) {
        return *resolved;
    }
    report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning,
        "No free font comparator remained for Finale 27 font \"" + referenceFont->name + "\"; substituted font id 0."});
    return 0;
}

/// @brief Restates an already-reported FontOptions field as coming from the reference.
/// @details The capture pass reports each recovered tuple as @ref ValueOrigin::LegacyMus.
/// When the substitution below replaces one, that report becomes untrue, so the existing
/// entry is rewritten in place rather than appended to: a second entry for the same target
/// would leave the diagnostics claiming both origins at once.
template <typename Reporting>
void retargetReportedOrigin(Reporting& reporting, FontType type, const char* member, std::int64_t rawValue)
{
    const auto target = "fonts[" + std::to_string(static_cast<std::size_t>(type)) + "]." + member;
    if (auto* info = reporting.report().findField(reporting.template instanceKey<FontOptionsTarget>(), target)) {
        info->origin = Reporting::Origin::Finale27Default;
        info->rawValue = rawValue;
        // The value no longer comes from anywhere in the source file.
        info->blockOffset = 0;
        info->decodedOffset = 0;
    }
}

void repairMissingRecoveredFontDefinitionsImpl(const musx::dom::DocumentPtr& document, const musx::dom::DocumentPtr& referenceDocument,
    const std::shared_ptr<FontOptionsTarget>& target, ImportReport& report, musx::factory::ConstructionContext& construction)
{
    for (const auto& [type, font] : target->fontOptions) {
        const auto missingId = font->fontId;
        if (document->getOthers()->get<FontDefinition>(musx::dom::SCORE_PARTID, missingId)) {
            continue;
        }

        const auto resolved = resolveRecoveredTuple(document, referenceDocument, type, missingId, font->fontSize, font->getEnigmaStyles(), report);
        auto replacement = std::make_shared<FontInfo>(document);
        replacement->fontId = construction.assignFontId(resolved.fontId);
        replacement->fontSize = resolved.fontSize;
        replacement->setEnigmaStyles(resolved.effects);
        target->fontOptions.insert_or_assign(type, std::move(replacement));

        // Nothing in this tuple came from the source any more, so the report must stop saying
        // it did: leaving these as LegacyMus would make a substituted value look recovered.
        withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
            retargetReportedOrigin(reporting, type, "fontId", resolved.fontId);
            retargetReportedOrigin(reporting, type, "fontSize", resolved.fontSize);
            retargetReportedOrigin(reporting, type, "effects", resolved.effects);
        });
    }
}

void completeFromReference(const musx::dom::DocumentPtr& document, const musx::dom::DocumentPtr& referenceDocument,
    const std::shared_ptr<FontOptionsTarget>& target, ImportReport& report, musx::factory::ConstructionContext& construction)
{
    const auto reference = referenceDocument->getOptions()->get<FontOptionsTarget>();
    if (!reference) {
        throw std::logic_error("FontOptions reference document is incomplete");
    }

    for (std::size_t ordinal = 0; ordinal < fontTypeCount; ++ordinal) {
        const auto type = static_cast<FontType>(ordinal);
        if (target->fontOptions.contains(type)) {
            continue;
        }
        const auto source = reference->getFontInfo(type);
        auto font = std::make_shared<FontInfo>(document);
        font->fontId = construction.assignFontId(resolveReferenceFont(document, referenceDocument, source->fontId, report));
        font->fontSize = source->fontSize;
        font->setEnigmaStyles(source->getEnigmaStyles());
        target->fontOptions.emplace(type, font);

        withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
            reportField(reporting, type, "fontId", Reporting::Origin::Finale27Default, font->fontId);
            reportField(reporting, type, "fontSize", Reporting::Origin::Finale27Default, font->fontSize);
            reportField(reporting, type, "effects", Reporting::Origin::Finale27Default, font->getEnigmaStyles());
        });
    }
}

} // namespace

void repairMissingRecoveredFontDefinitions(const musx::dom::DocumentPtr& document, const musx::dom::DocumentPtr& referenceDocument,
    const std::shared_ptr<FontOptionsTarget>& target, ImportReport& report, musx::factory::ConstructionContext& construction)
{
    repairMissingRecoveredFontDefinitionsImpl(document, referenceDocument, target, report, construction);
}

void captureFontOptions(const records::LegacyRecordIndex& index, const SourceProfile& profile, const musx::dom::DocumentPtr& document,
    const musx::dom::DocumentPtr& referenceDocument, ImportReport& report, musx::factory::ConstructionContext& construction)
{
    auto target = std::make_shared<FontOptionsTarget>(document);

    // The epoch alone, with no version test. The range this used to carry, 1.0 through 2.ff,
    // is simply a restatement of the Coda-banner era, so it could never exclude a Mac document
    // -- but it excluded every Windows one, because that era states a platform where it states
    // a version and `PC 1.0+` yields none. The tuple locations below hold for both platforms.
    if (profile.epoch == FormatEpoch::CodaBanner) {
        for (const auto& tuple : earlyCodaTuples) {
            const auto fontId = readEarlyTupleField(index, tuple, TupleField::FontId, profile.byteOrder);
            const auto size = readEarlyTupleField(index, tuple, TupleField::Size, profile.byteOrder);
            const auto effects = readEarlyTupleField(index, tuple, TupleField::Effects, profile.byteOrder);
            if (fontId && size && effects) {
                insertRecoveredTuple(document, referenceDocument, target, tuple.type, *fontId, *size, *effects, report, construction);
                if (tuple.type == FontType::StaffNames) {
                    for (const auto companion : codaNameCompanionTypes) {
                        insertRecoveredTuple(document, referenceDocument, target, companion, *fontId, *size, *effects, report, construction, true);
                    }
                }
            }
        }
    } else if (const auto* layout = layoutFor(profile)) {
        const auto count = tupleCount(*layout, index, report);
        for (std::size_t physicalOrdinal = 0; physicalOrdinal < count; ++physicalOrdinal) {
            const auto fontId = readTupleField(*layout, index, profile, physicalOrdinal, TupleField::FontId);
            const auto size = readTupleField(*layout, index, profile, physicalOrdinal, TupleField::Size);
            const auto effects = readTupleField(*layout, index, profile, physicalOrdinal, TupleField::Effects);
            if (!fontId || !size || !effects) {
                report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning,
                    "Ignored an incomplete legacy font-options tuple at ordinal " + std::to_string(physicalOrdinal) + '.'});
                continue;
            }

            if (isStructuralTupleFill(count, physicalOrdinal, *fontId, *size, *effects)) {
                withReporting(report,
                    [&]<typename Reporting>(Reporting& reporting) { reportPhysicalTuple(reporting, physicalOrdinal, *fontId, *size, *effects); });
            } else if (const auto type = semanticType(profile, physicalOrdinal)) {
                insertRecoveredTuple(document, referenceDocument, target, *type, *fontId, *size, *effects, report, construction);
            } else {
                withReporting(report,
                    [&]<typename Reporting>(Reporting& reporting) { reportPhysicalTuple(reporting, physicalOrdinal, *fontId, *size, *effects); });
                // Finale's upgrade discards the holding slot, so its leftover contents are not news.
                if (!isZeroTuple(*fontId, *size, *effects) && profile.epoch == FormatEpoch::ZlibLegacy
                    && physicalOrdinal != legacyTablatureHoldingSlot) {
                    report.diagnostics.push_back(
                        {musx::util::Logger::LogLevel::Info, "Captured nonzero legacy font-options tuple " + std::to_string(physicalOrdinal)
                                                                 + " beyond musxdom's current FontType range."});
                }
            }
        }
    }

    completeFromReference(document, referenceDocument, target, report, construction);
    document->getOptions()->add(FontOptionsTarget::XmlNodeName, std::move(target));
}

void importFontOptions(const ImportContext& context)
{
    // No mapping table: every type is either a recovered physical tuple translated through a
    // versioned semantic map or a baseline entry completed with a remapped font id, and
    // neither is a fixed source location a table row could name.
    captureFontOptions(context.index, context.profile, context.document, context.referenceDocument, context.report, context.construction);
}

} // namespace options
} // namespace finale_mus_reader
