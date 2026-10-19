// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using BeamOptionsTarget = musx::dom::options::BeamOptions;

constexpr const char* beamOptionsReportPrefix = "options.beamOptions";
constexpr std::uint16_t beamGeometrySelector = 20;
constexpr std::uint16_t beamFlagsSelector = 41;
constexpr std::uint16_t beamStubSelector = 3;
constexpr std::uint16_t beamWidthSelector = 62;

/// @brief Converts the stored end/all/standard/extreme order to musxdom's
/// end/standard/extreme/all order.
BeamOptionsTarget::FlattenStyle beamFlattenStyle(std::int64_t value)
{
    switch (value) {
    case 0: return BeamOptionsTarget::FlattenStyle::OnEndNotes;
    case 1: return BeamOptionsTarget::FlattenStyle::AlwaysFlat;
    case 2: return BeamOptionsTarget::FlattenStyle::OnStandardNote;
    case 3: return BeamOptionsTarget::FlattenStyle::OnExtremeNote;
    default: return BeamOptionsTarget::FlattenStyle::OnEndNotes;
    }
}

std::optional<std::int64_t> adjustEarlyBeamDistance(std::int64_t value, const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    if (!storesPreFinale35StemAndBeamUnits(index, profile)) {
        return std::nullopt;
    }
    return musx::dom::Evpu(value * musx::dom::EVPU_PER_STAFF_POSITION);
}

std::optional<std::int64_t> adjustCodaBeamWidth(std::int64_t value, const records::LegacyRecordIndex&, const SourceProfile&)
{
    return legacyTenThousandthsPointToEfix(value);
}

bool sourceStoresSeparateFourEighthsBeamOption(const SourceProfile& profile)
{
    // Finale 3.7 through 98 store this switch separately; Finale 2000 moves it into
    // selector 41's packed flag word.
    return sourceMatches(profile, EpochMask::Uncompressed) && sourceAtOrAfter(profile, FormatEpoch::UncompressedLegacy, versions::finale3_7)
           && sourcePredatesVersion(profile, FormatEpoch::UncompressedLegacy, versions::finale2000);
}

bool sourceUsesFinale98FourGroupsRestBehavior(const SourceProfile& profile)
{
    // Finale 98 writes this UI state but does not restore it when reopening a document.
    return sourceMatchesVersion(profile, FormatEpoch::UncompressedLegacy, versions::finale98);
}

const FieldMapping beamGeometryFields[] = {
    MUS_WORD_ADJUSTED(BeamOptionsTarget, "20", GLOBALS_CMPER, 0, 0, adjustEarlyBeamDistance, maxSlope),
    MUS_WORD(BeamOptionsTarget, "20", GLOBALS_CMPER, 0, 1, beamSepar),
    MUS_WORD_ADJUSTED(BeamOptionsTarget, "20", GLOBALS_CMPER, 0, 2, adjustEarlyBeamDistance, maxFromMiddle),
    MUS_BITS_AS(BeamOptionsTarget, "41", GLOBALS_CMPER, 0, 0, 0, 0, beamingStyle, beamFlattenStyle(value)),
};

const FieldMapping fixedBeamSizeFields[] = {
    MUS_WORD(BeamOptionsTarget, "03", GLOBALS_CMPER, 0, 3, beamStubLength),
    MUS_LONG(BeamOptionsTarget, "62", GLOBALS_CMPER, 0, 4, LongWordOrder::HighFirst, beamWidth),
};

const FieldMapping codaBeamWidthFields[] = {
    withSourceAdjustment(MUS_LONG(BeamOptionsTarget, "62", GLOBALS_CMPER, 0, 4, LongWordOrder::HighFirst, beamWidth), adjustCodaBeamWidth),
};

const FieldMapping fixedBeamFlagFields[] = {
    MUS_BIT(BeamOptionsTarget, "41", GLOBALS_CMPER, 0, 1, 0, extendBeamsOverRests),
    MUS_BIT(BeamOptionsTarget, "41", GLOBALS_CMPER, 0, 1, 1, incRestsInFourGroups),
    MUS_BIT(BeamOptionsTarget, "41", GLOBALS_CMPER, 0, 1, 3, beamFourEighthsInCommonTime),
    MUS_BIT(BeamOptionsTarget, "41", GLOBALS_CMPER, 0, 1, 4, beamThreeEighthsInCommonTime),
    MUS_BIT(BeamOptionsTarget, "41", GLOBALS_CMPER, 0, 1, 5, oldFinaleRestBeams),
    MUS_BIT(BeamOptionsTarget, "41", GLOBALS_CMPER, 0, 1, 6, dispHalfStemsOnRests),
    MUS_BIT(BeamOptionsTarget, "41", GLOBALS_CMPER, 0, 1, 7, spanSpace),
    MUS_BIT(BeamOptionsTarget, "41", GLOBALS_CMPER, 0, 1, 8, extendSecBeamsOverRests),
};

const FieldMapping separateBeamFourEighthsFields[] = {
    MUS_BIT(BeamOptionsTarget, "09", GLOBALS_CMPER, 0, 5, 0, beamFourEighthsInCommonTime),
};

const FieldMapping separateBeamRestFields[] = {
    MUS_BIT(BeamOptionsTarget, "16", GLOBALS_CMPER, 0, 4, 0, extendBeamsOverRests),
    MUS_BIT(BeamOptionsTarget, "16", GLOBALS_CMPER, 0, 4, 0, extendSecBeamsOverRests),
};

const FieldMapping separateBeamHalfStemFields[] = {
    MUS_BIT(BeamOptionsTarget, "22", GLOBALS_CMPER, 0, 0, 0, dispHalfStemsOnRests),
};

const FieldMapping classBeamFields[] = {
    MUS_CLASS_WORD(BeamOptionsTarget, numericGlobalClass(beamStubSelector), GLOBALS_CMPER, classWordOffset(3), beamStubLength),
    MUS_CLASS_WORD(BeamOptionsTarget, numericGlobalClass(beamGeometrySelector), GLOBALS_CMPER, classWordOffset(0), maxSlope),
    MUS_CLASS_WORD(BeamOptionsTarget, numericGlobalClass(beamGeometrySelector), GLOBALS_CMPER, classWordOffset(1), beamSepar),
    MUS_CLASS_WORD(BeamOptionsTarget, numericGlobalClass(beamGeometrySelector), GLOBALS_CMPER, classWordOffset(2), maxFromMiddle),
    MUS_CLASS_SELECTED_BITS_AS(
        BeamOptionsTarget, numericGlobalClass(beamFlagsSelector), GLOBALS_CMPER, classWordOffset(0), 0, 0, beamingStyle, beamFlattenStyle(value)),
    MUS_CLASS_BIT(BeamOptionsTarget, numericGlobalClass(beamFlagsSelector), GLOBALS_CMPER, classWordOffset(1), 0, extendBeamsOverRests),
    MUS_CLASS_BIT(BeamOptionsTarget, numericGlobalClass(beamFlagsSelector), GLOBALS_CMPER, classWordOffset(1), 1, incRestsInFourGroups),
    MUS_CLASS_BIT(BeamOptionsTarget, numericGlobalClass(beamFlagsSelector), GLOBALS_CMPER, classWordOffset(1), 3, beamFourEighthsInCommonTime),
    MUS_CLASS_BIT(BeamOptionsTarget, numericGlobalClass(beamFlagsSelector), GLOBALS_CMPER, classWordOffset(1), 4, beamThreeEighthsInCommonTime),
    MUS_CLASS_BIT(BeamOptionsTarget, numericGlobalClass(beamFlagsSelector), GLOBALS_CMPER, classWordOffset(1), 5, oldFinaleRestBeams),
    MUS_CLASS_BIT(BeamOptionsTarget, numericGlobalClass(beamFlagsSelector), GLOBALS_CMPER, classWordOffset(1), 6, dispHalfStemsOnRests),
    MUS_CLASS_BIT(BeamOptionsTarget, numericGlobalClass(beamFlagsSelector), GLOBALS_CMPER, classWordOffset(1), 7, spanSpace),
    MUS_CLASS_BIT(BeamOptionsTarget, numericGlobalClass(beamFlagsSelector), GLOBALS_CMPER, classWordOffset(1), 8, extendSecBeamsOverRests),
    MUS_CLASS_LONG(BeamOptionsTarget, numericGlobalClass(beamWidthSelector), GLOBALS_CMPER, classWordOffset(4), LongWordOrder::HighFirst, beamWidth),
};

const MappingTable& beamGeometryTable()
{
    static const MappingTable table{.reportPrefix = beamOptionsReportPrefix,
        .epochs = EpochMask::CodaBanner | EpochMask::Uncompressed | EpochMask::Dcl,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<BeamOptionsTarget>,
        .fields = beamGeometryFields,
        .fieldCount = std::size(beamGeometryFields)};
    return table;
}

const MappingTable& fixedBeamSizeTable()
{
    static const MappingTable table{.reportPrefix = beamOptionsReportPrefix,
        .epochs = EpochMask::Uncompressed | EpochMask::Dcl,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<BeamOptionsTarget>,
        .fields = fixedBeamSizeFields,
        .fieldCount = std::size(fixedBeamSizeFields)};
    return table;
}

const MappingTable& codaBeamWidthTable()
{
    static const MappingTable table{.reportPrefix = beamOptionsReportPrefix,
        .epochs = EpochMask::CodaBanner,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<BeamOptionsTarget>,
        .fields = codaBeamWidthFields,
        .fieldCount = std::size(codaBeamWidthFields)};
    return table;
}

const MappingTable& fixedBeamFlagTable()
{
    static const MappingTable table{.reportPrefix = beamOptionsReportPrefix,
        .epochs = EpochMask::Uncompressed | EpochMask::Dcl,
        .applies = &storesPackedBeamFlagLayout,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<BeamOptionsTarget>,
        .fields = fixedBeamFlagFields,
        .fieldCount = std::size(fixedBeamFlagFields)};
    return table;
}

const MappingTable& separateBeamFourEighthsTable()
{
    static const MappingTable table{.reportPrefix = beamOptionsReportPrefix,
        .epochs = EpochMask::Uncompressed,
        .sourceApplies = &sourceStoresSeparateFourEighthsBeamOption,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<BeamOptionsTarget>,
        .fields = separateBeamFourEighthsFields,
        .fieldCount = std::size(separateBeamFourEighthsFields)};
    return table;
}

const MappingTable& separateBeamHalfStemTable()
{
    static const MappingTable table{.reportPrefix = beamOptionsReportPrefix,
        .epochs = EpochMask::Uncompressed,
        .applies = &storesLoneStemFlagLayout,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<BeamOptionsTarget>,
        .fields = separateBeamHalfStemFields,
        .fieldCount = std::size(separateBeamHalfStemFields)};
    return table;
}

const MappingTable& separateBeamRestTable()
{
    static const MappingTable table{.reportPrefix = beamOptionsReportPrefix,
        .epochs = EpochMask::CodaBanner | EpochMask::Uncompressed,
        .applies = &storesLoneStemFlagLayout,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<BeamOptionsTarget>,
        .fields = separateBeamRestFields,
        .fieldCount = std::size(separateBeamRestFields)};
    return table;
}

const MappingTable& classBeamTable()
{
    static const MappingTable table{.reportPrefix = beamOptionsReportPrefix,
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<BeamOptionsTarget>,
        .fields = classBeamFields,
        .fieldCount = std::size(classBeamFields)};
    return table;
}

} // namespace

void importBeamOptions(const ImportContext& context)
{
    applyMappingTables({&beamGeometryTable(), &fixedBeamSizeTable(), &codaBeamWidthTable(), &separateBeamRestTable(), &separateBeamHalfStemTable(),
                           &fixedBeamFlagTable(), &separateBeamFourEighthsTable(), &classBeamTable()},
        context.index, context.profile, context.document, context.report);

    const auto pooled = context.document->getOptions()->get<BeamOptionsTarget>();
    if (!pooled) {
        return;
    }
    const auto target = std::const_pointer_cast<BeamOptionsTarget>(pooled);
    if (!storesLoneStemFlagLayout(context.index, context.profile)) {
        return;
    }

    const auto applyBehavior = [&](bool& property, const char* member, bool value) {
        property = value;
        withReporting(context.report,
            [&]<typename Reporting>(Reporting& reporting) { reporting.template behaviorField<BeamOptionsTarget>(member, value ? 1 : 0); });
    };
    // The early layout stores some switches separately; others are fixed source behavior.
    applyBehavior(target->oldFinaleRestBeams, "oldFinaleRestBeams", true);
    applyBehavior(target->spanSpace, "spanSpace", true);
    if (sourceUsesFinale98FourGroupsRestBehavior(context.profile)) {
        applyBehavior(target->incRestsInFourGroups, "incRestsInFourGroups", false);
    }
}

} // namespace options
} // namespace finale_mus_reader
