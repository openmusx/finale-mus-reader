// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include <iterator>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using LineCurveOptionsTarget = musx::dom::options::LineCurveOptions;

constexpr const char* lineCurveOptionsReportPrefix = "options.lineCurveOptions";
constexpr std::uint16_t curveResolutionSelector = 15;
constexpr std::uint16_t codaPostScriptUnderlineSelector = 52;
constexpr std::uint16_t enclosureWidthSelector = 27;
constexpr std::uint16_t restLegerLengthSelector = 1;
constexpr std::uint16_t staffLineWidthSelector = 58;
constexpr std::uint16_t legerLineSelector = 59;
constexpr std::uint16_t postScriptUnderlineSelector = 62;
constexpr std::uint16_t shapeSlurTipSelector = 97;
constexpr std::int64_t legacyLineCurveWidth = 118;

double lineCurveTenThousandths(std::int64_t value)
{
    return static_cast<double>(value) / 10000.0;
}

const FieldMapping codaLineCurveFields[] = {
    MUS_NUMERIC_WORD(LineCurveOptionsTarget, curveResolutionSelector, 0, 4, bezierStep),
};

const FieldMapping earlyCodaUnderlineFields[] = {
    // These source-owned values survive even when a direct modern conversion discards them.
    // Selector 62, when present, selects the later representation instead.
    MUS_NUMERIC_FIELD_AS_IF(LineCurveOptionsTarget, codaPostScriptUnderlineSelector, 0, 0, ValueWidth::Long, LongWordOrder::HighFirst, BitRange{},
        nullptr, nullptr, psUlDepth, legacySinglePrecision(value)),
    MUS_NUMERIC_FIELD_AS_IF(LineCurveOptionsTarget, codaPostScriptUnderlineSelector, 0, 2, ValueWidth::Long, LongWordOrder::HighFirst, BitRange{},
        nullptr, nullptr, psUlWidth, legacySinglePrecision(value)),
};

const FieldMapping lateCodaUnderlineFields[] = {
    MUS_NUMERIC_FIELD_AS_IF(LineCurveOptionsTarget, postScriptUnderlineSelector, 0, 0, ValueWidth::Long, LongWordOrder::HighFirst, BitRange{},
        nullptr, nullptr, psUlDepth, lineCurveTenThousandths(value)),
    MUS_NUMERIC_FIELD_AS_IF(LineCurveOptionsTarget, postScriptUnderlineSelector, 0, 2, ValueWidth::Long, LongWordOrder::HighFirst, BitRange{},
        nullptr, nullptr, psUlWidth, lineCurveTenThousandths(value)),
};

bool hasLateCodaUnderlineLayout(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    // Selector 62's presence directly identifies the later Coda underline layout. When it
    // is absent, the earlier table reads selector 52 if present and otherwise changes nothing.
    return readGlobalWords(index, profile, postScriptUnderlineSelector).present;
}

bool hasEarlyCodaUnderlineLayout(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return !hasLateCodaUnderlineLayout(index, profile);
}

const FieldMapping fixedLineCurveFields[] = {
    MUS_NUMERIC_WORD(LineCurveOptionsTarget, curveResolutionSelector, 0, 4, bezierStep),
    MUS_NUMERIC_WORD(LineCurveOptionsTarget, enclosureWidthSelector, 0, 3, enclosureWidth),
    MUS_NUMERIC_WORD(LineCurveOptionsTarget, staffLineWidthSelector, 0, 5, staffLineWidth),
    MUS_NUMERIC_WORD(LineCurveOptionsTarget, legerLineSelector, 0, 0, legerLineWidth),
    MUS_NUMERIC_WORD(LineCurveOptionsTarget, legerLineSelector, 0, 1, legerFrontLength),
    MUS_NUMERIC_WORD(LineCurveOptionsTarget, legerLineSelector, 0, 2, legerBackLength),
    MUS_NUMERIC_FIELD_AS_IF(LineCurveOptionsTarget, postScriptUnderlineSelector, 0, 0, ValueWidth::Long, LongWordOrder::HighFirst, BitRange{},
        nullptr, nullptr, psUlDepth, lineCurveTenThousandths(value)),
    MUS_NUMERIC_FIELD_AS_IF(LineCurveOptionsTarget, postScriptUnderlineSelector, 0, 2, ValueWidth::Long, LongWordOrder::HighFirst, BitRange{},
        nullptr, nullptr, psUlWidth, lineCurveTenThousandths(value)),
};

const FieldMapping dclLineCurveFields[] = {
    MUS_NUMERIC_WORD(LineCurveOptionsTarget, restLegerLengthSelector, 0, 2, restLegerFrontLength),
    MUS_NUMERIC_WORD(LineCurveOptionsTarget, restLegerLengthSelector, 0, 3, restLegerBackLength),
    MUS_NUMERIC_FIELD_AS_IF(LineCurveOptionsTarget, shapeSlurTipSelector, 0, 0, ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr,
        nullptr, pathSlurTipWidth, lineCurveTenThousandths(value)),
};

const FieldMapping classLineCurveFields[] = {
    MUS_CLASS_WORD(LineCurveOptionsTarget, numericGlobalClass(curveResolutionSelector), GLOBALS_CMPER, classWordOffset(4), bezierStep),
    MUS_CLASS_WORD(LineCurveOptionsTarget, numericGlobalClass(enclosureWidthSelector), GLOBALS_CMPER, classWordOffset(3), enclosureWidth),
    MUS_CLASS_WORD(LineCurveOptionsTarget, numericGlobalClass(staffLineWidthSelector), GLOBALS_CMPER, classWordOffset(5), staffLineWidth),
    MUS_CLASS_WORD(LineCurveOptionsTarget, numericGlobalClass(legerLineSelector), GLOBALS_CMPER, classWordOffset(0), legerLineWidth),
    MUS_CLASS_WORD(LineCurveOptionsTarget, numericGlobalClass(legerLineSelector), GLOBALS_CMPER, classWordOffset(1), legerFrontLength),
    MUS_CLASS_WORD(LineCurveOptionsTarget, numericGlobalClass(legerLineSelector), GLOBALS_CMPER, classWordOffset(2), legerBackLength),
    MUS_CLASS_WORD(LineCurveOptionsTarget, numericGlobalClass(restLegerLengthSelector), GLOBALS_CMPER, classWordOffset(2), restLegerFrontLength),
    MUS_CLASS_WORD(LineCurveOptionsTarget, numericGlobalClass(restLegerLengthSelector), GLOBALS_CMPER, classWordOffset(3), restLegerBackLength),
    MUS_CLASS_FIELD_AS_IF(LineCurveOptionsTarget, numericGlobalClass(postScriptUnderlineSelector), GLOBALS_CMPER, classWordOffset(0),
        ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, psUlDepth, lineCurveTenThousandths(value)),
    MUS_CLASS_FIELD_AS_IF(LineCurveOptionsTarget, numericGlobalClass(postScriptUnderlineSelector), GLOBALS_CMPER, classWordOffset(2),
        ValueWidth::Long, LongWordOrder::HighFirst, BitRange{}, nullptr, psUlWidth, lineCurveTenThousandths(value)),
    MUS_CLASS_FIELD_AS_IF(LineCurveOptionsTarget, numericGlobalClass(shapeSlurTipSelector), GLOBALS_CMPER, classWordOffset(0), ValueWidth::Long,
        LongWordOrder::HighFirst, BitRange{}, nullptr, pathSlurTipWidth, lineCurveTenThousandths(value)),
};

const MappingTable& codaLineCurveTable()
{
    static const MappingTable table{.reportPrefix = lineCurveOptionsReportPrefix,
        .epochs = EpochMask::CodaBanner,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<LineCurveOptionsTarget>,
        .fields = codaLineCurveFields,
        .fieldCount = std::size(codaLineCurveFields)};
    return table;
}

const MappingTable& fixedLineCurveTable()
{
    static const MappingTable table{.reportPrefix = lineCurveOptionsReportPrefix,
        .epochs = EpochMask::Uncompressed | EpochMask::Dcl,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<LineCurveOptionsTarget>,
        .fields = fixedLineCurveFields,
        .fieldCount = std::size(fixedLineCurveFields)};
    return table;
}

const MappingTable& earlyCodaUnderlineTable()
{
    static const MappingTable table{.reportPrefix = lineCurveOptionsReportPrefix,
        .epochs = EpochMask::CodaBanner,
        .applies = hasEarlyCodaUnderlineLayout,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<LineCurveOptionsTarget>,
        .fields = earlyCodaUnderlineFields,
        .fieldCount = std::size(earlyCodaUnderlineFields)};
    return table;
}

const MappingTable& lateCodaUnderlineTable()
{
    static const MappingTable table{.reportPrefix = lineCurveOptionsReportPrefix,
        .epochs = EpochMask::CodaBanner,
        .applies = hasLateCodaUnderlineLayout,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<LineCurveOptionsTarget>,
        .fields = lateCodaUnderlineFields,
        .fieldCount = std::size(lateCodaUnderlineFields)};
    return table;
}

const MappingTable& classLineCurveTable()
{
    static const MappingTable table{.reportPrefix = lineCurveOptionsReportPrefix,
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<LineCurveOptionsTarget>,
        .fields = classLineCurveFields,
        .fieldCount = std::size(classLineCurveFields)};
    return table;
}

const MappingTable& dclLineCurveTable()
{
    static const MappingTable table{.reportPrefix = lineCurveOptionsReportPrefix,
        .epochs = EpochMask::Dcl,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<LineCurveOptionsTarget>,
        .fields = dclLineCurveFields,
        .fieldCount = std::size(dclLineCurveFields)};
    return table;
}

void applyLegacyLineCurveBehavior(const ImportContext& context)
{
    const auto pooled = context.document->getOptions()->get<LineCurveOptionsTarget>();
    if (!pooled) {
        return;
    }
    const auto target = std::const_pointer_cast<LineCurveOptionsTarget>(pooled);

    target->enclosureRoundCorners = false;
    target->enclosureCornerRadius = 0;
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        reporting.template behaviorField<LineCurveOptionsTarget>("enclosureRoundCorners", 0);
        reporting.template behaviorField<LineCurveOptionsTarget>("enclosureCornerRadius", 0);
    });

    const auto applyLegacyWidth = [&](auto member, const char* name) {
        target.get()->*member = legacyLineCurveWidth;
        withReporting(
            context.report, [&]<typename Reporting>(Reporting& reporting) { reporting.template behaviorField<LineCurveOptionsTarget>(name, 0); });
    };
    if (context.profile.epoch == FormatEpoch::CodaBanner) {
        // The Coda layout predates the three stored width controls and uses one common
        // historical width for all three.
        applyLegacyWidth(&LineCurveOptionsTarget::enclosureWidth, "enclosureWidth");
        applyLegacyWidth(&LineCurveOptionsTarget::staffLineWidth, "staffLineWidth");
        applyLegacyWidth(&LineCurveOptionsTarget::legerLineWidth, "legerLineWidth");
    } else if (context.profile.epoch == FormatEpoch::UncompressedLegacy
               && !readGlobalWords(context.index, context.profile, enclosureWidthSelector).present) {
        // In the uncompressed layout, selector 27's presence directly states whether the
        // enclosure width is stored. Its absence selects the earlier fixed behavior; other
        // epochs are excluded because the same absence has not been established there.
        applyLegacyWidth(&LineCurveOptionsTarget::enclosureWidth, "enclosureWidth");
    }

    if (context.profile.epoch != FormatEpoch::CodaBanner || target->bezierStep != 0) {
        return;
    }
    // Coda stores zero for the original sixteen-step curve resolution; a nonzero word is
    // an explicit resolution and is retained as stored.
    target->bezierStep = 16;
    withReporting(
        context.report, [&]<typename Reporting>(Reporting& reporting) { reporting.template behaviorField<LineCurveOptionsTarget>("bezierStep", 0); });
}

} // namespace

void importLineCurveOptions(const ImportContext& context)
{
    applyMappingTables({&codaLineCurveTable(), &earlyCodaUnderlineTable(), &lateCodaUnderlineTable(), &fixedLineCurveTable(), &dclLineCurveTable(),
                           &classLineCurveTable()},
        context.index, context.profile, context.document, context.report);
    applyLegacyLineCurveBehavior(context);
}

} // namespace options
} // namespace finale_mus_reader
