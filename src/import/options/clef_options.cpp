// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include "import/options/test_access.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "import/support/text_encoding.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using ClefOptionsTarget = musx::dom::options::ClefOptions;
using ClefDef = ClefOptionsTarget::ClefDef;
using FontInfo = musx::dom::FontInfo;

// The clef table is a numeric global like any other option. It is not a record type of its
// own: through Finale 2006 it is the two decimal characters of its selector, and from 2007
// the class id that the shared numericGlobalClass rule derives from that same selector.
constexpr std::uint16_t clefTableSelector = 95;

// Before Finale 2001 there is no clef table. Each of the eight clefs of that era is a
// separate single-incidence global, selectors 28 through 35, and selector 36 is the tuplet
// font, so eight is a ceiling the record vocabulary itself imposes rather than a guess.
constexpr std::uint16_t earlyFirstSelector = 28;
constexpr std::size_t earlyClefCount = 8;

// Word slots within one early record. Word 1 carries an unidentified per-clef value that
// Finale 3.0 stops writing, so it is neither assigned nor converted.
constexpr std::uint32_t earlyMiddleCPosSlot = 0;
constexpr std::uint32_t earlyClefCharSlot = 2;
constexpr std::uint32_t earlyStaffPositionSlot = 3;
constexpr std::uint32_t earlyBaselineDifferenceSlot = 4;

// Before Finale 97 the baseline adjustment is a feature the document switches on, in bit 0 of
// word 5 of the *first* clef's selector. The switch is per document, not per clef: clearing it
// suppresses the adjustment for every clef, including ones that plainly store one.
//
// Finale 97, internally 3.8, dropped the checkbox and applies the adjustment unconditionally.
// Its files still carry other bits in word 5 -- 24, 30 and 36 all occur -- so testing the whole
// word for non-zero would read those as a switch that is not there.
//
// The window is bounded at both ends rather than left open at "3.8 or later". The early path
// only ever sees pre-2001 versions, so the upper bound changes nothing today; it is there so
// that a file whose version is recovered as something wild cannot land in the always-on case
// by accident. A wrong version should fall back to the bit test, which reads the document,
// rather than to an unconditional yes.
constexpr std::uint32_t earlyBaselineEnableSlot = 5;
constexpr std::uint16_t earlyBaselineEnableBit = 0x0001;
bool sourceAlwaysAdjustsClefBaseline(const SourceProfile& profile)
{
    return sourceAtOrAfter(profile, FormatEpoch::UncompressedLegacy, versions::finale97);
}

// Word counts of the two clef-table tuple shapes. They differ only in the clef character,
// which Finale 2012 widened from one word to a long when it gained Unicode text support.
constexpr std::size_t narrowTupleWords = 9;
constexpr std::size_t wideTupleWords = 10;
constexpr std::size_t clefWordSize = 2;

// The clef character is a long from Finale 2012, which is the shared Unicode boundary in
// legacy_mapping.h rather than a clef-specific version test.

// A harmonic level is one staff position, so the conversion of a pre-2001 baseline
// difference into Efix is musxdom's own two constants multiplied. Composing them rather
// than writing half of EFIX_PER_SPACE keeps the definition of a staff position in the one
// place musxdom already states it.
constexpr double efixPerHarmonicLevel = musx::dom::EFIX_PER_EVPU * musx::dom::EVPU_PER_STAFF_POSITION;

// Believed: clef tables from Finale 2003 onward hold this many definitions, as does the
// pinned Finale 27 baseline. It is what lets an ambiguous payload size be
// resolved: 360 bytes divides evenly by both tuple width, but only one of the two
// readings yields the collection Finale actually stores.
constexpr std::size_t modernClefCount = 18;

// Packed clef flags. Bit 1 is set exactly when the tuple carries its own font triple.
constexpr std::uint16_t isShapeBit = 0x0001;
constexpr std::uint16_t useOwnFontBit = 0x0002;
constexpr std::uint16_t scaleToStaffHeightBit = 0x0004;

/// @brief One clef definition as the source stores it, before any musxdom semantics.
struct PhysicalClef
{
    std::int16_t middleCPos{};
    std::uint32_t clefChar{};
    std::int16_t staffPosition{};
    /// @brief The difference between the clef's musical baseline, such as the G line of a
    /// treble clef, and its typographic baseline. A music font whose clefs already sit on
    /// the musical baseline leaves it zero.
    /// @details The unit changes with the era.
    ///
    /// From Finale 2001 it is Efix and passes through unchanged. The stored field is a signed
    /// word, so its range is about +/-512 Evpu; Finale saturates at -32768 rather than storing
    /// a larger negative the UI asks for.
    ///
    /// Through Finale 2000 it is harmonic levels, converted through
    /// @ref efixPerHarmonicLevel.
    ///
    /// A stored value only applies when @ref baselineEnabled says so. The raw stored word is
    /// always reported, enabled or not.
    std::int16_t baselineDifference{};
    /// @brief Whether @ref baselineDifference is already in the unit `baselineAdjust` uses.
    bool baselineIsEfix{};
    /// @brief Whether the document applies its baseline adjustments at all.
    /// @details Always true from Finale 2001, which has no such switch. Before that it is
    /// @ref earlyBaselineEnableSlot of the first clef's selector, and a stored value with the
    /// switch off is inert: assigning it would introduce an offset the source never applied.
    bool baselineEnabled{};
    std::uint16_t shapeId{};
    std::uint16_t fontComparator{};
    std::int16_t fontSize{};
    /// @brief Whether the source stored the character as a code point rather than as a byte in
    /// the clef's own font encoding.
    bool charIsCodepoint{};
    std::uint16_t fontEffects{};
    std::uint16_t flags{};
    std::size_t blockOffset{};
    std::size_t decodedOffset{};
};

/// @brief Reads one tuple out of a flat word stream, in either of the two tuple widths.
PhysicalClef decodeTuple(const std::vector<std::int16_t>& words, std::size_t first, std::size_t tupleWords)
{
    // The wide tuple is the narrow one with the clef character promoted to a long, so every
    // slot after the character shifts by exactly one word. Expressing it as an offset keeps
    // the two layouts from becoming two independently maintained field lists.
    const std::size_t shift = tupleWords == wideTupleWords ? 1 : 0;
    PhysicalClef result;
    result.middleCPos = wordAt(words, first);
    result.clefChar = shift == 0 ? narrowCodepoint(wordAt(words, first + 1)) : wideCodepoint(wordAt(words, first + 1), wordAt(words, first + 2));
    result.charIsCodepoint = shift != 0;
    result.staffPosition = wordAt(words, first + 2 + shift);
    result.baselineDifference = wordAt(words, first + 3 + shift);
    // Only the clef table has this word in Efix; the pre-2001 per-clef globals hold
    // harmonic levels in their own slot and set this false. The table era has no enable
    // switch either: the value applies whenever it is present.
    result.baselineIsEfix = true;
    result.baselineEnabled = true;
    result.shapeId = static_cast<std::uint16_t>(wordAt(words, first + 4 + shift));
    result.fontComparator = static_cast<std::uint16_t>(wordAt(words, first + 5 + shift));
    result.fontSize = wordAt(words, first + 6 + shift);
    result.fontEffects = static_cast<std::uint16_t>(wordAt(words, first + 7 + shift));
    result.flags = static_cast<std::uint16_t>(wordAt(words, first + 8 + shift));
    return result;
}

/// @brief Chooses the clef-table tuple width for a class-record payload.
/// @details Preferring the payload size over the version is deliberate. A size that divides
/// evenly by only one tuple width states its own layout, and some Finale 2007 files report
/// a major version far outside their release, so a version gate alone would misread them.
/// The version decides only the genuinely ambiguous case, a payload that divides by both.
std::optional<std::size_t> classTupleWords(std::size_t payloadSize, const std::optional<SourceVersion>& version)
{
    const auto narrowBytes = narrowTupleWords * clefWordSize;
    const auto wideBytes = wideTupleWords * clefWordSize;
    const bool fitsNarrow = payloadSize != 0 && payloadSize % narrowBytes == 0;
    const bool fitsWide = payloadSize != 0 && payloadSize % wideBytes == 0;
    if (fitsNarrow && !fitsWide) {
        return narrowTupleWords;
    }
    if (fitsWide && !fitsNarrow) {
        return wideTupleWords;
    }
    if (!fitsNarrow) {
        return std::nullopt;
    }
    // Both widths divide the payload, which happens at 360 bytes. Finale 2012 introduced
    // Unicode text and with it the long clef character, so the version separates the two.
    return versions::storesUnicodeCodepoints(version) ? wideTupleWords : narrowTupleWords;
}

template <typename Reporting>
void reportClefField(Reporting& reporting, std::size_t index, const char* member, typename Reporting::Origin origin, std::int64_t rawValue,
    std::size_t blockOffset = 0, std::size_t decodedOffset = 0)
{
    reporting.report().setField(reporting.template instanceKey<ClefOptionsTarget>(), "clefDefs[" + std::to_string(index) + "]." + member,
        {origin, blockOffset, decodedOffset, rawValue});
}

void reportRecoveredClef(ImportReport& report, std::size_t index, const PhysicalClef& stored)
{
    withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
        const auto block = stored.blockOffset;
        const auto decoded = stored.decodedOffset;
        reportClefField(reporting, index, "middleCPos", Reporting::Origin::LegacyMus, stored.middleCPos, block, decoded);
        reportClefField(reporting, index, "clefChar", Reporting::Origin::LegacyMus, static_cast<std::int64_t>(stored.clefChar), block, decoded);
        reportClefField(reporting, index, "staffPosition", Reporting::Origin::LegacyMus, stored.staffPosition, block, decoded);
        reportClefField(reporting, index, "shapeId", Reporting::Origin::LegacyMus, stored.shapeId, block, decoded);
        reportClefField(reporting, index, "flags", Reporting::Origin::LegacyMus, stored.flags, block, decoded);
        // The raw stored word, not the assigned Efix. A pre-2001 value is scaled on the way in,
        // and the report is the only place the original harmonic-level number survives.
        reportClefField(reporting, index, "baselineAdjust", Reporting::Origin::LegacyMus, stored.baselineDifference, block, decoded);
        if ((stored.flags & useOwnFontBit) != 0) {
            reportClefField(reporting, index, "font.fontId", Reporting::Origin::LegacyMus, stored.fontComparator, block, decoded);
            reportClefField(reporting, index, "font.fontSize", Reporting::Origin::LegacyMus, stored.fontSize, block, decoded);
            reportClefField(reporting, index, "font.effects", Reporting::Origin::LegacyMus, stored.fontEffects, block, decoded);
        }
    });
}

void reportClefDefaults(ImportReport& report, std::size_t index, const ClefDef& def)
{
    withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
        reportClefField(reporting, index, "middleCPos", Reporting::Origin::Finale27Default, def.middleCPos);
        reportClefField(reporting, index, "clefChar", Reporting::Origin::Finale27Default, static_cast<std::int64_t>(def.clefChar));
        reportClefField(reporting, index, "staffPosition", Reporting::Origin::Finale27Default, def.staffPosition);
        reportClefField(reporting, index, "shapeId", Reporting::Origin::Finale27Default, def.shapeId);
    });
}

/// @brief Turns one stored clef into a musxdom ClefDef and records where each value came from.
/// @details Before Finale 2012 the character is a byte in the encoding of the font that draws
/// the clef, not a code point, so it is decoded through that font exactly as legacy text is. A
/// clef without its own font uses the clef font from FontOptions, normally a symbol font whose
/// byte is a glyph number and survives unchanged; a clef given a text font of its own is the
/// case that makes the difference visible.
void insertRecoveredClef(const musx::dom::DocumentPtr& document, const std::shared_ptr<ClefOptionsTarget>& target, const PhysicalClef& stored,
    ImportReport& report, musx::factory::ConstructionContext& construction)
{
    const auto index = target->clefDefs.size();
    auto def = std::make_shared<ClefDef>(target);
    def->middleCPos = stored.middleCPos;
    def->staffPosition = stored.staffPosition;
    def->shapeId = musx::dom::Cmper(stored.shapeId);
    if (stored.baselineEnabled) {
        def->baselineAdjust =
            stored.baselineIsEfix ? musx::dom::Efix(stored.baselineDifference) : musx::dom::Efix(stored.baselineDifference * efixPerHarmonicLevel);
    }
    def->isShape = (stored.flags & isShapeBit) != 0;
    def->useOwnFont = (stored.flags & useOwnFontBit) != 0;
    def->scaleToStaffHeight = (stored.flags & scaleToStaffHeightBit) != 0;
    if (def->useOwnFont) {
        // musxdom's own resolver rejects useOwnFont without a font, so the instance is
        // always created when the bit is set, even if the stored triple were empty.
        auto font = std::make_shared<FontInfo>(target->getDocument());
        font->fontId = construction.assignFontId(musx::dom::Cmper(stored.fontComparator));
        font->fontSize = stored.fontSize;
        font->setEnigmaStyles(stored.fontEffects);
        def->font = std::move(font);
    }
    // After the font, because which font decodes the character is what the font block decides.
    def->clefChar = stored.charIsCodepoint ? static_cast<char32_t>(stored.clefChar)
                                           : text::codepointFromByte(static_cast<std::uint8_t>(stored.clefChar), document, def->calcFont()->fontId,
                                                 text::UnresolvedFontFallback::Symbol);
    target->clefDefs.push_back(std::move(def));

    reportRecoveredClef(report, index, stored);
}

bool captureFromWordStream(const musx::dom::DocumentPtr& document, const std::vector<std::int16_t>& words, std::size_t tupleWords,
    const PhysicalClef& provenance, const std::shared_ptr<ClefOptionsTarget>& target, ImportReport& report,
    musx::factory::ConstructionContext& construction)
{
    if (tupleWords == 0 || words.size() % tupleWords != 0) {
        report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning,
            "Legacy clef table holds " + std::to_string(words.size()) + " word(s), which is not a whole number of " + std::to_string(tupleWords)
                + "-word clef definitions; no clef definitions were recovered from the source."});
        return false;
    }
    for (std::size_t first = 0; first + tupleWords <= words.size(); first += tupleWords) {
        auto stored = decodeTuple(words, first, tupleWords);
        stored.blockOffset = provenance.blockOffset;
        stored.decodedOffset = provenance.decodedOffset;
        insertRecoveredClef(document, target, stored, report, construction);
    }
    return true;
}

/// @brief Recovers the eight separately stored clefs of the pre-2001 eras.
bool captureEarlyClefs(const musx::dom::DocumentPtr& document, const records::LegacyRecordIndex& index, const SourceProfile& profile,
    const std::shared_ptr<ClefOptionsTarget>& target, ImportReport& report, musx::factory::ConstructionContext& construction)
{
    // Read the document-wide switch from the first clef's record before any clef is built.
    const auto* firstRow = index.getOthers().get(numericGlobalTag(earlyFirstSelector), GLOBALS_CMPER, 0, 0);
    // The Coda era is excluded outright. Its word 4 is a mid-measure-clef baseline rather than
    // the general clef baseline musxdom means, and its switch is never set. Transferring it
    // would invent an offset from a field that does not mean the same thing.
    const bool baselineEnabled =
        sourceMatches(profile, EpochMask::FixedRow)
        && (sourceAlwaysAdjustsClefBaseline(profile)
            || (firstRow && (static_cast<std::uint16_t>(firstRow->words[earlyBaselineEnableSlot]) & earlyBaselineEnableBit) != 0));

    std::vector<PhysicalClef> stored;
    for (std::size_t i = 0; i < earlyClefCount; ++i) {
        const auto selector = static_cast<std::uint16_t>(earlyFirstSelector + i);
        const auto* row = index.getOthers().get(numericGlobalTag(selector), GLOBALS_CMPER, 0, 0);
        if (!row) {
            // The eight are written as a set. A partial set is a source this layout does
            // not describe, so nothing is taken from it rather than half a table.
            return false;
        }
        PhysicalClef clef;
        clef.middleCPos = row->words[earlyMiddleCPosSlot];
        clef.clefChar = narrowCodepoint(row->words[earlyClefCharSlot]);
        clef.staffPosition = row->words[earlyStaffPositionSlot];
        clef.baselineDifference = row->words[earlyBaselineDifferenceSlot];
        clef.baselineEnabled = baselineEnabled;
        clef.blockOffset = row->blockOffset;
        clef.decodedOffset = row->decodedOffset;
        stored.push_back(clef);
    }
    for (const auto& clef : stored) {
        insertRecoveredClef(document, target, clef, report, construction);
    }
    return true;
}

/// @brief Copies the reference document's scalar clef options onto a fresh target.
/// @details The clef definitions are rebuilt from the source, but the scalars around them
/// are ordinary numbers with no cross-document identity, so seeding them from the baseline
/// is safe and gives the scalar mapping table a default to overlay and to report.
void copyScalarsFromReference(const musx::dom::DocumentPtr& referenceDocument, const std::shared_ptr<ClefOptionsTarget>& target)
{
    const auto reference = referenceDocument->getOptions()->get<ClefOptionsTarget>();
    if (!reference) {
        throw std::logic_error("ClefOptions reference document is incomplete");
    }
    target->defaultClef = reference->defaultClef;
    target->clefChangePercent = reference->clefChangePercent;
    target->clefChangeOffset = reference->clefChangeOffset;
    target->clefFrontSepar = reference->clefFrontSepar;
    target->clefBackSepar = reference->clefBackSepar;
    target->showClefFirstSystemOnly = reference->showClefFirstSystemOnly;
    target->clefKeySepar = reference->clefKeySepar;
    target->clefTimeSepar = reference->clefTimeSepar;
    target->cautionaryClefChanges = reference->cautionaryClefChanges;
}

/// @brief Extends a short recovered collection with the platform baseline's own definitions.
/// @details Finale's own upgrade does exactly this: a Finale 2000 document keeps its eight
/// stored clefs and receives the rest from the version it is opened in. Reproducing that
/// keeps every clef index a document might reference addressable, because musxdom's
/// bounds-checked accessor throws rather than returning an empty definition.
void completeFromReference(const musx::dom::DocumentPtr& referenceDocument, const std::shared_ptr<ClefOptionsTarget>& target, ImportReport& report,
    PendingReferences& pending)
{
    const auto reference = referenceDocument->getOptions()->get<ClefOptionsTarget>();
    if (!reference) {
        throw std::logic_error("ClefOptions reference document is incomplete");
    }
    for (std::size_t index = target->clefDefs.size(); index < reference->clefDefs.size(); ++index) {
        const auto& source = reference->clefDefs[index];
        auto def = std::make_shared<ClefDef>(target);
        def->middleCPos = source->middleCPos;
        def->clefChar = source->clefChar;
        def->staffPosition = source->staffPosition;
        def->baselineAdjust = source->baselineAdjust;
        def->isShape = source->isShape;
        def->scaleToStaffHeight = source->scaleToStaffHeight;
        // A comparator is only meaningful in the document that issued it, so the baseline's
        // number is never written here. The shape is copied after every pool is complete, and
        // the field stays zero until then: a blank clef is honest, a foreign comparator is not.
        def->shapeId = 0;
        if (source->isShape && source->shapeId != 0) {
            pending.shapes.push_back({source->shapeId, [def](musx::dom::Cmper resolved) { def->shapeId = resolved; }});
            withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
                reporting.state(pending.shapes.back().reportField) = {
                    reporting.template instanceKey<ClefOptionsTarget>(), "clefDefs[" + std::to_string(index) + "].shapeId"};
            });
        }
        // Neither pinned baseline gives a clef its own font, so there is no baseline font
        // comparator to remap here. Should one ever appear, it would need the same
        // definition matching that FontOptions performs rather than a numeric copy.
        if (source->useOwnFont && source->font) {
            throw std::logic_error("Finale 27 ClefOptions baseline unexpectedly carries a clef-specific font");
        }
        reportClefDefaults(report, index, *def);
        target->clefDefs.push_back(std::move(def));
    }
}

// The scalars around the collection. The eras do not agree about what these selectors hold, so
// each has its own table.
//
// cautionaryClefChanges is bit 2 of the courtesy flags in selector 44 word 3, bit 0 of the same
// word being the key signature's.
//
// The Coda era is excluded: it has no packed courtesy word. Selector 12 holds these as separate
// boolean words, of which word 1 is the key signature's. Which word is the clef's is **open**.

// Finale 3.0 through 2006.
const FieldMapping clefScalarFields[] = {
    MUS_WORD(ClefOptionsTarget, "01", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 0, defaultClef),
    MUS_WORD(ClefOptionsTarget, "13", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 2, clefChangePercent),
    MUS_WORD(ClefOptionsTarget, "13", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 3, clefChangeOffset),
    MUS_WORD(ClefOptionsTarget, "19", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 0, clefFrontSepar),
    MUS_WORD(ClefOptionsTarget, "19", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 1, clefBackSepar),
    MUS_WORD(ClefOptionsTarget, "38", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 5, clefKeySepar),
    MUS_WORD(ClefOptionsTarget, "39", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 4, clefTimeSepar),
    MUS_BIT(ClefOptionsTarget, "27", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 1,
        /*bit*/ 0, showClefFirstSystemOnly),
    MUS_BIT(ClefOptionsTarget, "44", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 3,
        /*bit*/ 2, cautionaryClefChanges),
};

// Finale 1.8.7 through 2.6. Only five of the eight selectors mean the same thing here.
// Selector 27 word 1 and selector 39 word 4 are font sizes in this era, not clef spacing --
// they are the lyric-chorus and clef font tuples the FontOptions mapping reads -- and
// selector 38 word 5 holds something else again. Reading any of the three would produce a
// confident wrong number, so the era leaves them at the Finale 27 default.
const FieldMapping earlyClefScalarFields[] = {
    MUS_WORD(ClefOptionsTarget, "01", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 0, defaultClef),
    MUS_WORD(ClefOptionsTarget, "13", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 2, clefChangePercent),
    MUS_WORD(ClefOptionsTarget, "13", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 3, clefChangeOffset),
    MUS_WORD(ClefOptionsTarget, "19", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 0, clefFrontSepar),
    MUS_WORD(ClefOptionsTarget, "19", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 1, clefBackSepar),
};

// Finale 2007 and later. The same eight logical options, reached through the shared
// numericGlobalClass rule and addressed by byte offset rather than word slot.
const FieldMapping classClefScalarFields[] = {
    MUS_CLASS_WORD(ClefOptionsTarget, numericGlobalClass(1), GLOBALS_CMPER, classWordOffset(0), defaultClef),
    MUS_CLASS_WORD(ClefOptionsTarget, numericGlobalClass(13), GLOBALS_CMPER, classWordOffset(2), clefChangePercent),
    MUS_CLASS_WORD(ClefOptionsTarget, numericGlobalClass(13), GLOBALS_CMPER, classWordOffset(3), clefChangeOffset),
    MUS_CLASS_WORD(ClefOptionsTarget, numericGlobalClass(19), GLOBALS_CMPER, classWordOffset(0), clefFrontSepar),
    MUS_CLASS_WORD(ClefOptionsTarget, numericGlobalClass(19), GLOBALS_CMPER, classWordOffset(1), clefBackSepar),
    MUS_CLASS_WORD(ClefOptionsTarget, numericGlobalClass(38), GLOBALS_CMPER, classWordOffset(5), clefKeySepar),
    MUS_CLASS_WORD(ClefOptionsTarget, numericGlobalClass(39), GLOBALS_CMPER, classWordOffset(4), clefTimeSepar),
    MUS_CLASS_BIT(ClefOptionsTarget, numericGlobalClass(27), GLOBALS_CMPER, classWordOffset(1),
        /*bit*/ 0, showClefFirstSystemOnly),
    MUS_CLASS_BIT(ClefOptionsTarget, numericGlobalClass(44), GLOBALS_CMPER, classWordOffset(3),
        /*bit*/ 2, cautionaryClefChanges),
};

const MappingTable& clefOptionsTable()
{
    static const MappingTable table{.reportPrefix = "options.clefOptions",
        .epochs = EpochMask::FixedRow,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<ClefOptionsTarget>,
        .fields = clefScalarFields,
        .fieldCount = std::size(clefScalarFields)};
    return table;
}

const MappingTable& earlyClefOptionsTable()
{
    static const MappingTable table{.reportPrefix = "options.clefOptions",
        .epochs = EpochMask::CodaBanner,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<ClefOptionsTarget>,
        .fields = earlyClefScalarFields,
        .fieldCount = std::size(earlyClefScalarFields)};
    return table;
}

} // namespace

const MappingTable& classClefOptionsTable()
{
    static const MappingTable table{.reportPrefix = "options.clefOptions",
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<ClefOptionsTarget>,
        .fields = classClefScalarFields,
        .fieldCount = std::size(classClefScalarFields)};
    return table;
}

void validateClefOptions(const musx::dom::DocumentPtr& document, ImportReport& report)
{
    const auto target = document->getOptions()->get<ClefOptionsTarget>();
    if (!target) {
        return;
    }
    // The default clef is an index into the collection, and musxdom's accessor throws on an
    // out-of-range one. Warn and leave the recovered value rather than clamping it: clamping
    // would turn a decoding error, or a damaged file, into a plausible document and hide the
    // very thing worth knowing. An ordinary document is always in range, so this fires only on
    // something genuinely unexpected.
    if (target->defaultClef >= target->clefDefs.size()) {
        report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning,
            "The recovered default clef index " + std::to_string(target->defaultClef) + " is outside the " + std::to_string(target->clefDefs.size())
                + " clef definitions this document has; it is reported as read, not corrected."});
    }
}

void captureClefOptions(const records::LegacyRecordIndex& index, const SourceProfile& profile, const musx::dom::DocumentPtr& document,
    const musx::dom::DocumentPtr& referenceDocument, ImportReport& report, PendingReferences& pending,
    musx::factory::ConstructionContext& construction)
{
    auto target = std::make_shared<ClefOptionsTarget>(document);
    copyScalarsFromReference(referenceDocument, target);

    // The Coda era has no courtesy-clef option; the first version found to offer one is 3.6.2.
    // Its documents always show a courtesy clef, so that is asserted here rather than read:
    // selector 44 is zero throughout the era, and its bit 2 would say the opposite.
    //
    // Three neighboring options -- the clef-to-key and clef-to-time spacings and
    // "display clef only on first system" -- do not exist in that era either, but they need
    // no assertion. Both pinned baselines omit all three, so the seeded values are already
    // zero and false, which is what the era means. Where the baseline already gives the right
    // value, the baseline is the answer, and reporting it as a Finale 27 default is accurate
    // rather than an understatement.
    //
    // Asserting them instead would buy nothing. The baseline is a pinned artifact, generated
    // from committed resources whose hashes AGENTS.md records, so depending on it is not the
    // fragile choice it might look like.
    //
    // Nothing may read them: selectors 27 and 39 hold font tuples in this era, so reading the
    // later locations there would report a font size as a spacing value.
    if (profile.epoch == FormatEpoch::CodaBanner) {
        target->cautionaryClefChanges = true;
        withReporting(report,
            [&]<typename Reporting>(Reporting& reporting) { reporting.template behaviorField<ClefOptionsTarget>("cautionaryClefChanges", 1); });
    }

    if (profile.epoch == FormatEpoch::ZlibLegacy) {
        const auto* row = index.getClassOthers().get(numericGlobalClass(clefTableSelector), GLOBALS_CMPER, 0, 0);
        if (row) {
            const auto bytes = index.getClassOthers().effectivePayloadOf(*row);
            if (const auto tupleWords = classTupleWords(bytes.size(), profile.version)) {
                const auto words = payloadWords(bytes, profile.byteOrder);
                PhysicalClef provenance;
                provenance.blockOffset = row->blockOffset;
                provenance.decodedOffset = row->decodedOffset;
                if (*tupleWords == wideTupleWords && profile.byteOrder == ByteOrder::BigEndian) {
                    // **Unverified: the long clef character's word order in a big-endian
                    // container.** No such document is known.
                    // Rather than rely on that absence, the case announces itself when it
                    // arrives.
                    //
                    // Warning rather than something quieter, despite firing for essentially no
                    // real file. Nearly every other diagnostic here says a value is missing;
                    // this one says a value may be wrong. If the word order is the other way
                    // the clef characters are not slightly off but nonsense, and a clef table
                    // of nonsense is worth interrupting for.
                    report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning, "This document uses the Finale 2012 clef layout "
                                                                                         "in big-endian order, an untested combination; the clef "
                                                                                         "character's word order is unverified for it."});
                }
                captureFromWordStream(document, words, *tupleWords, provenance, target, report, construction);
            } else {
                report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning,
                    "Legacy clef table payload of " + std::to_string(bytes.size()) + " byte(s) matches neither clef-definition tuple width."});
            }
        }
    } else {
        const auto family = readNumericGlobalWords(index, clefTableSelector);
        PhysicalClef provenance;
        provenance.blockOffset = family.blockOffset;
        provenance.decodedOffset = family.decodedOffset;
        if (family.present) {
            captureFromWordStream(document, family.words, narrowTupleWords, provenance, target, report, construction);
        } else if (profile.epoch == FormatEpoch::CodaBanner || profile.epoch == FormatEpoch::UncompressedLegacy) {
            // Only these two eras keep clefs in selectors 28 through 35. Falling back on the
            // mere absence of selector 95 would be wrong for a DCL file that happens to lack
            // it: those selectors still exist there and hold unrelated option words, so the
            // reader would fabricate eight clef definitions and report them as recovered.
            captureEarlyClefs(document, index, profile, target, report, construction);
        } else {
            report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Warning, "The clef table is absent from a source era that "
                                                        "stores one; every clef definition came from the Finale 27 baseline."});
        }
    }

    if (target->clefDefs.size() > modernClefCount) {
        report.diagnostics.push_back(
            {musx::util::Logger::LogLevel::Info, "Legacy clef table holds " + std::to_string(target->clefDefs.size())
                                                     + " clef definitions, more than the " + std::to_string(modernClefCount) + " Finale 27 stores."});
    }
    completeFromReference(referenceDocument, target, report, pending);
    document->getOptions()->add(ClefOptionsTarget::XmlNodeName, std::move(target));
}

void importClefOptions(const ImportContext& context)
{
    // The definition collection is rebuilt from the source rather than seeded, so it has to
    // exist before the scalar tables overlay the object it belongs to. The three tables are
    // one logical group: they share a report prefix and disagree only about where this era
    // keeps the scalars.
    captureClefOptions(
        context.index, context.profile, context.document, context.referenceDocument, context.report, context.pending, context.construction);
    applyMappingTables(
        {&clefOptionsTable(), &earlyClefOptionsTable(), &classClefOptionsTable()}, context.index, context.profile, context.document, context.report);
    validateClefOptions(context.document, context.report);
}

} // namespace options
} // namespace finale_mus_reader
