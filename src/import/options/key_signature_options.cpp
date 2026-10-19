// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include <cstdint>
#include <iterator>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using KeySignatureOptionsTarget = musx::dom::options::KeySignatureOptions;

constexpr std::uint16_t keyBehaviorSelector = 12;
constexpr std::uint16_t keySpacingSelector = 18;
constexpr std::uint16_t accidentalSpacingSelector = 21;
constexpr std::uint16_t firstSystemSelector = 27;
constexpr std::uint16_t keyTimeSpacingSelector = 39;
constexpr std::uint16_t simplifyKeySelector = 41;
constexpr std::uint16_t courtesySelector = 44;

// The Coda-banner layout shares the selector-12 key behavior words with later fixed-row
// layouts. The locations of showKeyFirstSystemOnly and keyTimeSepar remain unlocated in
// this layout, so they retain the baseline.
const FieldMapping codaKeySignatureFields[] = {
    MUS_NUMERIC_WORD(KeySignatureOptionsTarget, keyBehaviorSelector, 0, 1, doKeyCancel),
    MUS_NUMERIC_WORD(KeySignatureOptionsTarget, keyBehaviorSelector, 0, 2, doCStart),
    MUS_NUMERIC_WORD(KeySignatureOptionsTarget, keyBehaviorSelector, 0, 3, redisplayOnModeChange),
    MUS_NUMERIC_WORD(KeySignatureOptionsTarget, keySpacingSelector, 0, 0, keyFront),
    MUS_NUMERIC_WORD(KeySignatureOptionsTarget, keySpacingSelector, 0, 1, keyMid),
    MUS_NUMERIC_WORD(KeySignatureOptionsTarget, keySpacingSelector, 0, 2, keyBack),
    MUS_NUMERIC_WORD(KeySignatureOptionsTarget, accidentalSpacingSelector, 0, 4, acciAdd),
};

const FieldMapping fixedRowKeySignatureFields[] = {
    MUS_NUMERIC_WORD(KeySignatureOptionsTarget, keyBehaviorSelector, 0, 1, doKeyCancel),
    MUS_NUMERIC_WORD(KeySignatureOptionsTarget, keyBehaviorSelector, 0, 2, doCStart),
    MUS_NUMERIC_WORD(KeySignatureOptionsTarget, keyBehaviorSelector, 0, 3, redisplayOnModeChange),
    MUS_NUMERIC_WORD(KeySignatureOptionsTarget, keySpacingSelector, 0, 0, keyFront),
    MUS_NUMERIC_WORD(KeySignatureOptionsTarget, keySpacingSelector, 0, 1, keyMid),
    MUS_NUMERIC_WORD(KeySignatureOptionsTarget, keySpacingSelector, 0, 2, keyBack),
    MUS_NUMERIC_WORD(KeySignatureOptionsTarget, accidentalSpacingSelector, 0, 4, acciAdd),
    MUS_NUMERIC_WORD(KeySignatureOptionsTarget, firstSystemSelector, 0, 2, showKeyFirstSystemOnly),
    MUS_NUMERIC_WORD(KeySignatureOptionsTarget, keyTimeSpacingSelector, 0, 5, keyTimeSepar),
    MUS_NUMERIC_BIT(KeySignatureOptionsTarget, courtesySelector, 0, 3, 0, cautionaryKeyChanges),
};

const FieldMapping dclKeySignatureFields[] = {
    MUS_NUMERIC_WORD(KeySignatureOptionsTarget, simplifyKeySelector, 2, 1, simplifyKeyHoldOctave),
};

const FieldMapping classKeySignatureFields[] = {
    MUS_CLASS_WORD(KeySignatureOptionsTarget, numericGlobalClass(keyBehaviorSelector), GLOBALS_CMPER, classWordOffset(1), doKeyCancel),
    MUS_CLASS_WORD(KeySignatureOptionsTarget, numericGlobalClass(keyBehaviorSelector), GLOBALS_CMPER, classWordOffset(2), doCStart),
    MUS_CLASS_WORD(KeySignatureOptionsTarget, numericGlobalClass(keyBehaviorSelector), GLOBALS_CMPER, classWordOffset(3), redisplayOnModeChange),
    MUS_CLASS_WORD(KeySignatureOptionsTarget, numericGlobalClass(keySpacingSelector), GLOBALS_CMPER, classWordOffset(0), keyFront),
    MUS_CLASS_WORD(KeySignatureOptionsTarget, numericGlobalClass(keySpacingSelector), GLOBALS_CMPER, classWordOffset(1), keyMid),
    MUS_CLASS_WORD(KeySignatureOptionsTarget, numericGlobalClass(keySpacingSelector), GLOBALS_CMPER, classWordOffset(2), keyBack),
    MUS_CLASS_WORD(KeySignatureOptionsTarget, numericGlobalClass(accidentalSpacingSelector), GLOBALS_CMPER, classWordOffset(4), acciAdd),
    MUS_CLASS_WORD(KeySignatureOptionsTarget, numericGlobalClass(firstSystemSelector), GLOBALS_CMPER, classWordOffset(2), showKeyFirstSystemOnly),
    MUS_CLASS_WORD(KeySignatureOptionsTarget, numericGlobalClass(keyTimeSpacingSelector), GLOBALS_CMPER, classWordOffset(5), keyTimeSepar),
    MUS_CLASS_WORD(KeySignatureOptionsTarget, numericGlobalClass(simplifyKeySelector), GLOBALS_CMPER, classWordOffset(13), simplifyKeyHoldOctave),
    MUS_CLASS_BIT(KeySignatureOptionsTarget, numericGlobalClass(courtesySelector), GLOBALS_CMPER, classWordOffset(3), 0, cautionaryKeyChanges),
};

const MappingTable& codaKeySignatureTable()
{
    static const MappingTable table{.reportPrefix = "options.keySignatureOptions",
        .epochs = EpochMask::CodaBanner,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<KeySignatureOptionsTarget>,
        .fields = codaKeySignatureFields,
        .fieldCount = std::size(codaKeySignatureFields)};
    return table;
}

const MappingTable& fixedRowKeySignatureTable()
{
    static const MappingTable table{.reportPrefix = "options.keySignatureOptions",
        .epochs = EpochMask::FixedRow,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<KeySignatureOptionsTarget>,
        .fields = fixedRowKeySignatureFields,
        .fieldCount = std::size(fixedRowKeySignatureFields)};
    return table;
}

const MappingTable& classKeySignatureTable()
{
    static const MappingTable table{.reportPrefix = "options.keySignatureOptions",
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<KeySignatureOptionsTarget>,
        .fields = classKeySignatureFields,
        .fieldCount = std::size(classKeySignatureFields)};
    return table;
}

const MappingTable& dclKeySignatureTable()
{
    static const MappingTable table{.reportPrefix = "options.keySignatureOptions",
        .epochs = EpochMask::Dcl,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<KeySignatureOptionsTarget>,
        .fields = dclKeySignatureFields,
        .fieldCount = std::size(dclKeySignatureFields)};
    return table;
}

void applyKeySignatureLegacyBehavior(const ImportContext& context)
{
    const auto pooled = context.document->getOptions()->get<KeySignatureOptionsTarget>();
    if (!pooled) {
        return;
    }
    const auto target = std::const_pointer_cast<KeySignatureOptionsTarget>(pooled);

    target->doKeyCancelBetweenSharpsFlats = true;
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        reporting.template behaviorField<KeySignatureOptionsTarget>("doKeyCancelBetweenSharpsFlats", 1);
    });

    if (context.profile.epoch == FormatEpoch::CodaBanner || context.profile.epoch == FormatEpoch::UncompressedLegacy) {
        target->simplifyKeyHoldOctave = false;
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            reporting.template behaviorField<KeySignatureOptionsTarget>("simplifyKeyHoldOctave", 0);
        });
    }

    if (context.profile.epoch == FormatEpoch::CodaBanner) {
        target->cautionaryKeyChanges = true;
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            reporting.template behaviorField<KeySignatureOptionsTarget>("cautionaryKeyChanges", 1);
        });
    }
}

} // namespace

void importKeySignatureOptions(const ImportContext& context)
{
    applyMappingTables({&codaKeySignatureTable(), &fixedRowKeySignatureTable(), &dclKeySignatureTable(), &classKeySignatureTable()}, context.index,
        context.profile, context.document, context.report);
    applyKeySignatureLegacyBehavior(context);
}

} // namespace options
} // namespace finale_mus_reader
