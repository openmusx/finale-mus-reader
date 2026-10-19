// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using MmRestTarget = musx::dom::options::MultimeasureRestOptions;

// The multimeasure-rest defaults are a numeric global like any other option. They are not a
// record type of their own: through Finale 2006 the identity is the two decimal characters of
// the selector, and from Finale 2007 it is the class id the shared numericGlobalClass rule
// derives from that same selector.
constexpr std::uint16_t mmRestSelector = 25;

// "Automatically update multimeasure rests" is the one field of this class that lives
// somewhere else, on a selector of its own. See @ref storesMmRestAutoUpdate for why its
// presence rather than a version decides whether it can be read.
constexpr std::uint16_t mmRestUpdateSelector = 83;
constexpr std::uint32_t mmRestUpdateSlot = 4;

/// @brief An absolute word index into the family's second fixed row.
/// @details The zlib eras coalesce the whole incidence family into one payload and address it
/// by byte, where the fixed rows address the same option by incidence and slot. Deriving the
/// class offsets from the row size keeps the two encodings describing one word stream instead
/// of two independently maintained offset lists.
constexpr std::uint32_t secondRowWord(std::uint32_t slot)
{
    return records::otherWordCount + slot;
}

// Slots of the second fixed row, so that the fixed-row and class tables name the same field
// by the same number.
constexpr std::uint32_t symSpacingSlot = 0;
constexpr std::uint32_t numAdjXSlot = 1;
constexpr std::uint32_t startAdjustSlot = 2;
constexpr std::uint32_t endAdjustSlot = 3;
constexpr std::uint32_t mmRestFlagsSlot = 5;

// Bit 0 of the flags word is "use character rest style".
//
// It is the only bit of this word any legacy document uses; the word is exactly 0 or 1
// throughout the later layout. The class's other boolean,
// "noHorizontalStretch", has no bit here to find: the option arrived with Finale 27, so no
// legacy format has a place to put it. It is asserted rather than mapped -- see
// @ref captureMultimeasureRestOptions.
constexpr std::uint8_t useSymbolsBit = 0;

// The word count of the whole selector-25 family in its early layout: one incidence, where
// Finale 3.5 and later write two.
constexpr std::size_t earlyMmRestFamilyWords = records::otherWordCount;

/// @brief Whether this source stores the six-word multimeasure-rest record.
/// @details Finale 3.5 rewrote this option's record, and the boundary is what makes a marker
/// necessary rather than merely preferable here: it falls **inside** the uncompressed epoch,
/// so an epoch gate cannot express it, and no document of the two intervening versions 3.3 and
/// 3.4 is available to place the cut. A version range would therefore have to guess its own cut
/// point, and would in addition fail closed on the Coda era's Windows documents, which state a
/// platform where its Mac documents state a version and so carry no version at all. The
/// family's own size states the layout outright, and recovers those Windows documents that a
/// version range would skip without a word.
///
/// The early layout is one incidence of six words; the later is twelve, counting the zlib era's
/// coalesced payload. Nothing carries any other count.
///
/// The two layouts share only the measure width, in slot 0. The number adjustment and the
/// shape moved from slots 4 and 5 down to 2 and 3 when the record grew, so reading an early
/// document through the later table would report its number adjustment as "start number at"
/// and its shape comparator as the symbol threshold, while taking the number adjustment and
/// shape from two words that mean something else entirely. That is the cost of an ambiguous
/// file, and it is why a source with no selector 25 at all reads as the later layout:
/// recovering nothing is better than recovering three fields from the wrong slots.
bool statesEarlyMmRestLayout(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    if (profile.epoch == FormatEpoch::ZlibLegacy) {
        return false;
    }
    const auto family = readNumericGlobalWords(index, mmRestSelector);
    return family.present && family.words.size() <= earlyMmRestFamilyWords;
}

bool statesLaterMmRestLayout(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return !statesEarlyMmRestLayout(index, profile);
}

/// @brief Whether this source stores the automatic-update selector at all.
/// @details Selector 83 arrives with Finale 97, which is the one boundary of this class that
/// does not coincide with the layout marker above. Reading the record's presence rather than
/// dating the file keeps it out of a version range whose lower end would sit somewhere between
/// 3.7 and 97, and makes the absent case explicit instead of silently defaulting. A document
/// without the selector does not update automatically, which is asserted in
/// @ref captureMultimeasureRestOptions rather than left to the pinned baseline, which says the
/// opposite.
bool storesMmRestAutoUpdate(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    if (profile.epoch == FormatEpoch::ZlibLegacy) {
        return index.getClassOthers().get(numericGlobalClass(mmRestUpdateSelector), GLOBALS_CMPER, 0, 0) != nullptr;
    }
    return readNumericGlobalWords(index, mmRestUpdateSelector).present;
}

// Finale 3.5 through Finale 2006, in both fixed-row epochs. The record is two rows: the first
// carries the measure width, the second the symbol spacing and the flag.
//
// Slot 1 of the first row and slot 4 of the second are **open** and deliberately unmapped.
const FieldMapping mmRestFields[] = {
    MUS_WORD(MmRestTarget, "25", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 0, measWidth),
    MUS_WORD(MmRestTarget, "25", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 2, numAdjY),
    MUS_WORD(MmRestTarget, "25", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 3, shapeDef),
    MUS_WORD(MmRestTarget, "25", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 4, numStart),
    MUS_WORD(MmRestTarget, "25", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 5, useSymsThreshold),
    MUS_WORD(MmRestTarget, "25", GLOBALS_CMPER, /*incidence*/ 1, symSpacingSlot, symSpacing),
    MUS_WORD(MmRestTarget, "25", GLOBALS_CMPER, /*incidence*/ 1, numAdjXSlot, numAdjX),
    MUS_WORD(MmRestTarget, "25", GLOBALS_CMPER, /*incidence*/ 1, startAdjustSlot, startAdjust),
    MUS_WORD(MmRestTarget, "25", GLOBALS_CMPER, /*incidence*/ 1, endAdjustSlot, endAdjust),
    MUS_BIT(MmRestTarget, "25", GLOBALS_CMPER, /*incidence*/ 1, mmRestFlagsSlot, useSymbolsBit, useSymbols),
};

// Finale 1.0.0 through Finale 3.2, spanning the whole Coda-banner epoch and the first
// uncompressed releases. Three fields. The number adjustment and the shape sit at slots 4 and
// 5 here where the later layout puts them at 2 and 3, which is what the layout marker exists to
// distinguish. Slot 5 is a shape comparator in this era as in every later one, and the source's
// own comparator is kept because shape definitions are recovered from the source.
//
// Slots 1 to 3 are the multimeasure-rest number font (id, size, effects) in the Coda-banner era.
// They belong to FontOptions, which reads them there; Finale 3.0 and 3.2 leave them zero.
//
// The rest of the class keeps the pinned baseline's values, which match this era's behavior.
// The two H-bar adjustments are the exception, because the baseline does not agree there; they
// are asserted in @ref captureMultimeasureRestOptions.
const FieldMapping earlyMmRestFields[] = {
    MUS_WORD(MmRestTarget, "25", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 0, measWidth),
    MUS_WORD(MmRestTarget, "25", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 4, numAdjY),
    MUS_WORD(MmRestTarget, "25", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 5, shapeDef),
};

// Selector 83 word 4, for the documents that carry the selector. Word 2 of the same record is
// set in most documents and is **not** this flag; reading it instead would switch automatic
// updating on for roughly half of them.
const FieldMapping mmRestAutoUpdateFields[] = {
    MUS_WORD(MmRestTarget, "83", GLOBALS_CMPER, /*incidence*/ 0, mmRestUpdateSlot, autoUpdateMmRests),
};

// Finale 2007 and later: the same ten logical values, reached through the shared
// numericGlobalClass rule and addressed by byte offset in the coalesced payload. Both byte
// orders occur in this era.
const FieldMapping classMmRestFields[] = {
    MUS_CLASS_WORD(MmRestTarget, numericGlobalClass(mmRestSelector), GLOBALS_CMPER, classWordOffset(0), measWidth),
    MUS_CLASS_WORD(MmRestTarget, numericGlobalClass(mmRestSelector), GLOBALS_CMPER, classWordOffset(2), numAdjY),
    MUS_CLASS_WORD(MmRestTarget, numericGlobalClass(mmRestSelector), GLOBALS_CMPER, classWordOffset(3), shapeDef),
    MUS_CLASS_WORD(MmRestTarget, numericGlobalClass(mmRestSelector), GLOBALS_CMPER, classWordOffset(4), numStart),
    MUS_CLASS_WORD(MmRestTarget, numericGlobalClass(mmRestSelector), GLOBALS_CMPER, classWordOffset(5), useSymsThreshold),
    MUS_CLASS_WORD(MmRestTarget, numericGlobalClass(mmRestSelector), GLOBALS_CMPER, classWordOffset(secondRowWord(symSpacingSlot)), symSpacing),
    MUS_CLASS_WORD(MmRestTarget, numericGlobalClass(mmRestSelector), GLOBALS_CMPER, classWordOffset(secondRowWord(numAdjXSlot)), numAdjX),
    MUS_CLASS_WORD(MmRestTarget, numericGlobalClass(mmRestSelector), GLOBALS_CMPER, classWordOffset(secondRowWord(startAdjustSlot)), startAdjust),
    MUS_CLASS_WORD(MmRestTarget, numericGlobalClass(mmRestSelector), GLOBALS_CMPER, classWordOffset(secondRowWord(endAdjustSlot)), endAdjust),
    MUS_CLASS_BIT(
        MmRestTarget, numericGlobalClass(mmRestSelector), GLOBALS_CMPER, classWordOffset(secondRowWord(mmRestFlagsSlot)), useSymbolsBit, useSymbols),
};

const FieldMapping classMmRestAutoUpdateFields[] = {
    MUS_CLASS_WORD(MmRestTarget, numericGlobalClass(mmRestUpdateSelector), GLOBALS_CMPER, classWordOffset(mmRestUpdateSlot), autoUpdateMmRests),
};

constexpr const char* mmRestReportPrefix = "options.multimeasureRestOptions";

// Every epoch whose records are 16-byte rows, which is all three that are not zlib. The three
// tables below share this mask deliberately: the epoch says only how a record is addressed,
// and which of the two layouts a file uses is left entirely to the markers, so there is no
// epoch-shaped hole for a marker and a mask to disagree in. The Coda-banner epoch is covered
// because @ref statesEarlyMmRestLayout answers true for every document in it.
constexpr EpochMask fixedRowMmRestEpochs = EpochMask::CodaBanner | EpochMask::FixedRow;

const MappingTable& earlyMmRestTable()
{
    static const MappingTable table{.reportPrefix = mmRestReportPrefix,
        .epochs = fixedRowMmRestEpochs,
        .applies = &statesEarlyMmRestLayout,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<MmRestTarget>,
        .fields = earlyMmRestFields,
        .fieldCount = std::size(earlyMmRestFields)};
    return table;
}

const MappingTable& mmRestTable()
{
    static const MappingTable table{.reportPrefix = mmRestReportPrefix,
        .epochs = fixedRowMmRestEpochs,
        .applies = &statesLaterMmRestLayout,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<MmRestTarget>,
        .fields = mmRestFields,
        .fieldCount = std::size(mmRestFields)};
    return table;
}

const MappingTable& mmRestAutoUpdateTable()
{
    // The presence test alone decides this one, and it must: the capture pass asserts the
    // opposite when the same test says the record is absent, so a mask that could disagree
    // with it would leave a document with neither a read value nor an assertion, sitting at a
    // baseline default that says the opposite of both.
    static const MappingTable table{.reportPrefix = mmRestReportPrefix,
        .epochs = fixedRowMmRestEpochs,
        .applies = &storesMmRestAutoUpdate,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<MmRestTarget>,
        .fields = mmRestAutoUpdateFields,
        .fieldCount = std::size(mmRestAutoUpdateFields)};
    return table;
}

const MappingTable& classMmRestTable()
{
    static const MappingTable table{.reportPrefix = mmRestReportPrefix,
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<MmRestTarget>,
        .fields = classMmRestFields,
        .fieldCount = std::size(classMmRestFields)};
    return table;
}

const MappingTable& classMmRestAutoUpdateTable()
{
    static const MappingTable table{.reportPrefix = mmRestReportPrefix,
        .epochs = EpochMask::Zlib,
        .applies = &storesMmRestAutoUpdate,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<MmRestTarget>,
        .fields = classMmRestAutoUpdateFields,
        .fieldCount = std::size(classMmRestAutoUpdateFields)};
    return table;
}

/// @brief Records one value the era fixed rather than stored.
void reportEraBehavior(ImportReport& report, const char* member, std::int64_t value)
{
    withReporting(report, [&]<typename Reporting>(Reporting& reporting) { reporting.template behaviorField<MmRestTarget>(member, value); });
}

/// @brief Asserts the values a legacy source fixed rather than stored.
/// @details The two H-bar adjustments and automatic updating arrive with Finale 3.5 and
/// Finale 97 respectively, and the pinned baseline supplies all three with values a document
/// from before them cannot have meant. "Stretch Horizontally" arrived with Finale 27 and is
/// false for every legacy document, whatever its era. Runs before the scalar tables, whose
/// gates then leave the asserted fields alone.
void captureMultimeasureRestOptions(
    const records::LegacyRecordIndex& index, const SourceProfile& profile, const musx::dom::DocumentPtr& document, ImportReport& report)
{
    const auto pooled = document->getOptions()->get<MmRestTarget>();
    if (!pooled) {
        return;
    }
    // Pool instances are handed out const. Overlaying legacy values is the one reason this
    // library writes to them.
    auto target = std::const_pointer_cast<MmRestTarget>(pooled);

    // Two of this class's fields are not merely absent from the early record but *wrong* in
    // the pinned baseline, so leaving them to it would assert the opposite of what the source
    // means. Finale 27's "New Document Without Libraries" starts the H-bar 30 Evpu in and ends
    // it 30 Evpu out; a document written before Finale 3.5 has no such adjustment, because the
    // whole second row that carries it does not exist, so both adjustments are zero.
    //
    // The rest of the class is left to the baseline deliberately, because the baseline already
    // carries what that era does -- 2, 9 and 48 for the start number, threshold and
    // symbol spacing, and zero for the horizontal number adjustment -- and repeating a value
    // the pinned resource already states would be a second copy of one fact.
    if (statesEarlyMmRestLayout(index, profile)) {
        target->startAdjust = 0;
        target->endAdjust = 0;
        reportEraBehavior(report, "startAdjust", 0);
        reportEraBehavior(report, "endAdjust", 0);
    }

    // Likewise for automatic updating, which is a Finale 97 arrival. The baseline switches it
    // on, and a document from a version that had no such setting never updated its
    // multimeasure rests, so inheriting the baseline would claim a behavior the source cannot
    // have had. **An absent selector 83 means off, with no further qualification** --
    // including for a document whose
    // epoch could not be classified, which is exactly the document that must not be left
    // claiming a Finale 27 setting.
    if (!storesMmRestAutoUpdate(index, profile)) {
        target->autoUpdateMmRests = false;
        reportEraBehavior(report, "autoUpdateMmRests", 0);
    }

    // "Stretch Horizontally" is a Finale 27 feature. No legacy document of any era can state
    // it, so its value is known exactly for every file that can arrive here, and it is false.
    // This is the one field asserted unconditionally.
    //
    // The pinned baseline also leaves it false, so the usual rule -- leave a field to the
    // baseline where the baseline already agrees, rather than keeping a second copy of one fact
    // -- would say to omit this. It is asserted anyway, because the two statements are not the
    // same statement. The baseline saying false is one Finale 27 document's setting, which a
    // later pinned baseline could legitimately change; the feature postdating every legacy
    // format is a fact about the formats, and that makes the value known rather than
    // synthesized. Reporting it as a Finale 27 default would understate that, and would leave
    // the field looking like an unanswered question in a coverage report.
    target->noHorizontalStretch = false;
    reportEraBehavior(report, "noHorizontalStretch", 0);
}

/// @brief Checks the recovered H-bar shape reference against the shape pool.
/// @details The shape definitions belong to another importer, and the options pool runs before
/// the others pool, so this must run in the deferred phase that follows every importer.
void validateMultimeasureRestOptions(const musx::dom::DocumentPtr& document, ImportReport& report)
{
    const auto target = document->getOptions()->get<MmRestTarget>();
    if (!target || target->shapeDef == 0) {
        return;
    }
    // Comparator zero means the document draws no H-bar shape rather than naming a pooled
    // object. A nonzero one that no recovered definition answers is kept exactly as stored: it
    // is what the source says, and substituting the baseline's own shape would name a
    // different document's drawing.
    //
    // Verbose, because the stored value is kept and nothing is lost: this notes the dangling
    // reference for investigation rather than reporting a problem a host must act on.
    if (!document->getOthers()->get<musx::dom::others::ShapeDef>(musx::dom::SCORE_PARTID, target->shapeDef)) {
        report.diagnostics.push_back(
            {musx::util::Logger::LogLevel::Verbose, "The multimeasure rest H-bar names shape definition " + std::to_string(target->shapeDef)
                                                        + ", which this document does not define; it is kept as stored."});
    }
}

} // namespace

void importMultimeasureRestOptions(const ImportContext& context)
{
    // The capture pass runs first because it establishes the fields no era-appropriate table
    // will read, and the table machinery leaves an already-reported field alone rather than
    // downgrading it to a synthesized default.
    captureMultimeasureRestOptions(context.index, context.profile, context.document, context.report);
    applyMappingTables({&earlyMmRestTable(), &mmRestTable(), &mmRestAutoUpdateTable(), &classMmRestTable(), &classMmRestAutoUpdateTable()},
        context.index, context.profile, context.document, context.report);
    context.pending.defer(DeferredStage::ResolveStoredRefs, [&context] { validateMultimeasureRestOptions(context.document, context.report); });
}

} // namespace options
} // namespace finale_mus_reader
