// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <cstdint>
#include <iterator>
#include <memory>
#include <vector>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using MmRestTarget = musx::dom::others::MultimeasureRest;

// One multimeasure rest per comparator, which is the rest's first measure. Through Finale 2006
// the record is the other-pool tag `XI`; from Finale 2007 it is a class record of its own,
// stored per part.
constexpr const char* mmRestTag = "XI";
constexpr records::LegacyTag mmRestClass = 0x00af;

/// @brief An absolute word index into the record's second fixed row.
/// @details The class record coalesces both rows into one payload, so its byte offsets are the
/// fixed rows' word slots counted across the row boundary.
constexpr std::uint32_t secondRowWord(std::uint32_t slot)
{
    return records::otherWordCount + slot;
}

// Slots of the second fixed row, so that the fixed-row and class tables name the same field by
// the same number. Slot 4 is filler.
constexpr std::uint32_t symbolSpacingSlot = 0;
constexpr std::uint32_t numHorzAdjSlot = 1;
constexpr std::uint32_t shapeStartAdjustSlot = 2;
constexpr std::uint32_t shapeEndAdjustSlot = 3;
constexpr std::uint32_t mmRestFlagsSlot = 5;

// Bit 0 of the flags word is "use symbols". "No horizontal stretch" has no legacy bit: the
// option postdates every legacy format.
constexpr std::uint8_t useSymbolsBit = 0;

// Both fixed-row layouts share the first row. A record written before Finale 3.5 stores that
// row alone, and the engine reads nothing from the second row it does not have; see
// @ref reportFirstRowOnlyRecords. In that early row, slots 4 and 5 are **Believed** to be
// the start number and the symbol threshold of the later layout: they are zero wherever the
// early row occurs, which is also what both fields mean for a rest that predates symbols.
const FieldMapping mmRestFields[] = {
    MUS_WORD(MmRestTarget, mmRestTag, CMPER_FROM_TARGET, /*incidence*/ 0, /*slot*/ 0, measWidth),
    MUS_WORD(MmRestTarget, mmRestTag, CMPER_FROM_TARGET, /*incidence*/ 0, /*slot*/ 1, nextMeas),
    MUS_WORD(MmRestTarget, mmRestTag, CMPER_FROM_TARGET, /*incidence*/ 0, /*slot*/ 2, numVertAdj),
    MUS_WORD(MmRestTarget, mmRestTag, CMPER_FROM_TARGET, /*incidence*/ 0, /*slot*/ 3, shapeDef),
    MUS_WORD(MmRestTarget, mmRestTag, CMPER_FROM_TARGET, /*incidence*/ 0, /*slot*/ 4, numStart),
    MUS_WORD(MmRestTarget, mmRestTag, CMPER_FROM_TARGET, /*incidence*/ 0, /*slot*/ 5, symbolThreshold),
    MUS_WORD(MmRestTarget, mmRestTag, CMPER_FROM_TARGET, /*incidence*/ 1, symbolSpacingSlot, symbolSpacing),
    MUS_WORD(MmRestTarget, mmRestTag, CMPER_FROM_TARGET, /*incidence*/ 1, numHorzAdjSlot, numHorzAdj),
    MUS_WORD(MmRestTarget, mmRestTag, CMPER_FROM_TARGET, /*incidence*/ 1, shapeStartAdjustSlot, shapeStartAdjust),
    MUS_WORD(MmRestTarget, mmRestTag, CMPER_FROM_TARGET, /*incidence*/ 1, shapeEndAdjustSlot, shapeEndAdjust),
    MUS_BIT(MmRestTarget, mmRestTag, CMPER_FROM_TARGET, /*incidence*/ 1, mmRestFlagsSlot, useSymbolsBit, useSymbols),
};

const FieldMapping classMmRestFields[] = {
    MUS_CLASS_WORD(MmRestTarget, mmRestClass, CMPER_FROM_TARGET, classWordOffset(0), measWidth),
    MUS_CLASS_WORD(MmRestTarget, mmRestClass, CMPER_FROM_TARGET, classWordOffset(1), nextMeas),
    MUS_CLASS_WORD(MmRestTarget, mmRestClass, CMPER_FROM_TARGET, classWordOffset(2), numVertAdj),
    MUS_CLASS_WORD(MmRestTarget, mmRestClass, CMPER_FROM_TARGET, classWordOffset(3), shapeDef),
    MUS_CLASS_WORD(MmRestTarget, mmRestClass, CMPER_FROM_TARGET, classWordOffset(4), numStart),
    MUS_CLASS_WORD(MmRestTarget, mmRestClass, CMPER_FROM_TARGET, classWordOffset(5), symbolThreshold),
    MUS_CLASS_WORD(MmRestTarget, mmRestClass, CMPER_FROM_TARGET, classWordOffset(secondRowWord(symbolSpacingSlot)), symbolSpacing),
    MUS_CLASS_WORD(MmRestTarget, mmRestClass, CMPER_FROM_TARGET, classWordOffset(secondRowWord(numHorzAdjSlot)), numHorzAdj),
    MUS_CLASS_WORD(MmRestTarget, mmRestClass, CMPER_FROM_TARGET, classWordOffset(secondRowWord(shapeStartAdjustSlot)), shapeStartAdjust),
    MUS_CLASS_WORD(MmRestTarget, mmRestClass, CMPER_FROM_TARGET, classWordOffset(secondRowWord(shapeEndAdjustSlot)), shapeEndAdjust),
    MUS_CLASS_BIT(MmRestTarget, mmRestClass, CMPER_FROM_TARGET, classWordOffset(secondRowWord(mmRestFlagsSlot)), useSymbolsBit, useSymbols),
};

constexpr const char* mmRestReportPrefix = "others.mmRest";

const MappingTable& mmRestTable()
{
    static const MappingTable table{.reportPrefix = mmRestReportPrefix,
        .epochs = EpochMask::CodaBanner | EpochMask::FixedRow,
        .targetKind = TargetKind::OthersFromRecords,
        .recordIdentity = records::packTag(mmRestTag),
        .createTarget = &createOthersTarget<MmRestTarget>,
        .fields = mmRestFields,
        .fieldCount = std::size(mmRestFields)};
    return table;
}

const MappingTable& classMmRestTable()
{
    static const MappingTable table{.reportPrefix = mmRestReportPrefix,
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OthersFromRecords,
        .recordIdentity = mmRestClass,
        .createTarget = &createOthersTarget<MmRestTarget>,
        .fields = classMmRestFields,
        .fieldCount = std::size(classMmRestFields)};
    return table;
}

/// @brief Completes the rests whose record stores only the first fixed row.
/// @details Finale 3.5 added the second row, and a rest written before it keeps its one-row
/// record even in a document a later version saved, so the layout is decided per record rather
/// than per file. Such a rest predates symbols and H-bar and horizontal number adjustments, so
/// those fields are era behavior: no symbols and no adjustment. Its symbol spacing is
/// **Believed** to be the document's own default spacing; the value is inert while the rest
/// draws no symbols. The options are read after every importer has run, because they are
/// overlaid after the others pool is filled.
void reportFirstRowOnlyRecords(const ImportContext& context)
{
    if (context.profile.epoch == FormatEpoch::ZlibLegacy) {
        return;
    }
    const auto& pool = context.index.getOthers();
    std::vector<std::shared_ptr<MmRestTarget>> firstRowOnly;
    for (const auto& pooled : context.document->getOthers()->getAllSources<MmRestTarget>()) {
        if (pool.getArray(records::packTag(mmRestTag), pooled->getCmper(), 0, pooled->getSourcePartId()).size() == 1) {
            firstRowOnly.push_back(std::const_pointer_cast<MmRestTarget>(pooled));
        }
    }
    if (firstRowOnly.empty()) {
        return;
    }
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        for (const auto& rest : firstRowOnly) {
            const auto key = reporting.template instanceKey<MmRestTarget>(rest->getSourcePartId(), rest->getCmper());
            reportFallbackField(reporting, key, "numHorzAdj", Reporting::Origin::LegacyBehavior, rest->numHorzAdj);
            reportFallbackField(reporting, key, "shapeStartAdjust", Reporting::Origin::LegacyBehavior, rest->shapeStartAdjust);
            reportFallbackField(reporting, key, "shapeEndAdjust", Reporting::Origin::LegacyBehavior, rest->shapeEndAdjust);
            reportFallbackField(reporting, key, "useSymbols", Reporting::Origin::LegacyBehavior, rest->useSymbols);
        }
    });
    context.pending.defer(DeferredStage::ResolveStoredRefs, [&context, firstRowOnly = std::move(firstRowOnly)] {
        const auto options = context.document->getOptions()->get<musx::dom::options::MultimeasureRestOptions>();
        if (!options) {
            return;
        }
        for (const auto& rest : firstRowOnly) {
            rest->symbolSpacing = options->symSpacing;
        }
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            for (const auto& rest : firstRowOnly) {
                const auto key = reporting.template instanceKey<MmRestTarget>(rest->getSourcePartId(), rest->getCmper());
                reportFallbackField(reporting, key, "symbolSpacing", Reporting::Origin::LegacyBehavior, rest->symbolSpacing);
            }
        });
    });
}

} // namespace

void importMultimeasureRests(const ImportContext& context)
{
    applyMappingTables({&mmRestTable(), &classMmRestTable()}, context.index, context.profile, context.document, context.report);
    reportFirstRowOnlyRecords(context);
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        for (const auto& rest : context.document->getOthers()->getAllSources<MmRestTarget>()) {
            const auto key = reporting.template instanceKey<MmRestTarget>(rest->getSourcePartId(), rest->getCmper());
            reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
            // "Stretch horizontally" is a Finale 27 option, so no legacy record can state it.
            reportFallbackField(reporting, key, "noHorizontalStretch", Reporting::Origin::MusxOnly, rest->noHorizontalStretch);
        }
    });
}

} // namespace others
} // namespace finale_mus_reader
