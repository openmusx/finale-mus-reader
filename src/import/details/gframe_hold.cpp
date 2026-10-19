// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/details.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "import/shared/gframe_records.h"
#include "import/shared/staff_defaults.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace details {
namespace {

using StaffStyleAssignTarget = musx::dom::others::StaffStyleAssign;
using StaffStyleAssignStaffTarget = musx::dom::others::Staff;
using StaffStyleAssignStyleTarget = musx::dom::others::StaffStyle;

constexpr std::size_t gframeHoldFinale98FlagsSlot = 1;
constexpr std::uint16_t gframeHoldAlternateNotationMask = 0x000f;
constexpr records::LegacyTag gframeHoldClass = 0x0414;
constexpr std::uint16_t clefDisplayMask = 0x0030;
constexpr std::uint16_t clefAfterBarlineBit = 0x0001;
constexpr std::uint16_t mirrorFrameBit = 0x0040;

struct GFrameLayout
{
    std::size_t clefSlot;
    std::size_t flagsSlot;
    std::optional<std::size_t> percentSlot;
    std::size_t firstFrameSlot;
    std::size_t knownFrames;
};

[[nodiscard]] GFrameLayout gframeLayout(const SourceProfile& profile)
{
    if (!sourceAtOrAfter(profile, FormatEpoch::UncompressedLegacy, versions::finale98)) {
        return {gframe::codaClefSlot, gframe::earlyFlagsSlot, std::nullopt, 0, 1};
    }
    if (sourceAtOrAfter(profile, FormatEpoch::DclLegacy, versions::finale2004)) {
        return {0, 1, 2, 3, 4};
    }
    if (sourceAtOrAfter(profile, FormatEpoch::UncompressedLegacy, versions::finale2000)) {
        return {0, 1, 6, 2, 4};
    }
    return {0, 1, std::nullopt, 2, 4};
}

void importGFrameHoldRecords(const ImportContext& context)
{
    const auto source =
        selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), gframe::tag, gframeHoldClass, true);
    if (!source) {
        return;
    }
    const auto layout = gframeLayout(context.profile);
    const auto clefOptions = context.document->getOptions()->get<musx::dom::options::ClefOptions>();
    const RecordFamilySource earlyLayers{&context.index.getDetails(), records::packTag("LL"), false, true};
    for (const auto& [partId, staffId] : recordKeys(*source)) {
        for (const auto measure : source->pool->secondCmpersForTag(source->identity, staffId, partId)) {
            if (staffId == 0 || measure == 0) {
                continue;
            }
            // A frame past the score's last measure belongs to no measure; Finale discards it.
            if (!context.document->getOthers()->get<musx::dom::others::Measure>(musx::dom::SCORE_PARTID, measure)) {
                continue;
            }
            const auto rows = source->pool->getArray(source->identity, staffId, measure, partId);
            if (rows.empty()) {
                continue;
            }
            const auto words = collectRecordWords(*source, rows, context.profile.byteOrder);
            if (words.size() <= (std::max)(layout.clefSlot, layout.flagsSlot)) {
                context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
                    "GFrameHold for staff " + std::to_string(staffId) + ", measure " + std::to_string(measure) + " is truncated."});
                continue;
            }
            auto target = createDetailsRecordTarget<musx::dom::details::GFrameHold>(context.document, *source, rows.front(), staffId, measure);
            const auto flags = static_cast<std::uint16_t>(words[layout.flagsSlot]);
            const auto clef = static_cast<std::uint16_t>(words[layout.clefSlot]);
            const bool listSelected = (flags & gframe::clefListBit) != 0;
            if (listSelected) {
                target->clefListId = clef;
            } else {
                target->clefId = static_cast<musx::dom::ClefIndex>(clef);
            }
            switch (flags & clefDisplayMask) {
            case 0x0000: target->showClefMode = musx::dom::ShowClefMode::WhenNeeded; break;
            case 0x0010: target->showClefMode = musx::dom::ShowClefMode::Never; break;
            case 0x0020: target->showClefMode = musx::dom::ShowClefMode::Always; break;
            default:
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Info, "GFrameHold has ambiguous clef display flags for staff " + std::to_string(staffId)
                                                             + ", measure " + std::to_string(measure) + "."});
                break;
            }
            const bool hasClefAfterBarlineFlag = sourceAtOrAfter(context.profile, FormatEpoch::UncompressedLegacy, versions::finale2000);
            if (hasClefAfterBarlineFlag) {
                target->clefAfterBarline = (flags & clefAfterBarlineBit) != 0;
            }
            // Believed: bit 0x0040 carries the mirror state in every GF flags layout.
            target->mirrorFrame = (flags & mirrorFrameBit) != 0;
            const bool hasPercentWord = layout.percentSlot && words.size() > *layout.percentSlot;
            const bool hasStoredPercent = hasPercentWord && words[*layout.percentSlot] != 0;
            const bool usesDefaultPercent =
                !layout.percentSlot || hasPercentWord || !sourceAtOrAfter(context.profile, FormatEpoch::DclLegacy, versions::finale2004);
            if (hasStoredPercent) {
                target->clefPercent = words[*layout.percentSlot];
            } else if (usesDefaultPercent && clefOptions) {
                target->clefPercent = clefOptions->clefChangePercent;
            }
            for (std::size_t layer = 0; layer < layout.knownFrames && layout.firstFrameSlot + layer < words.size(); ++layer) {
                target->frames[layer] = static_cast<musx::dom::Cmper>(static_cast<std::uint16_t>(words[layout.firstFrameSlot + layer]));
            }
            std::array<const records::LegacyRow*, 3> earlyLayerRows{};
            if (layout.knownFrames == 1 && words.size() > 3) {
                const auto listId = static_cast<musx::dom::Cmper>(static_cast<std::uint16_t>(words[3]));
                if (listId != 0) {
                    for (std::size_t layer = 1; layer < target->frames.size(); ++layer) {
                        const auto linkRows = earlyLayers.pool->getArray(earlyLayers.identity, listId, static_cast<musx::dom::Cmper>(layer), partId);
                        if (!linkRows.empty()) {
                            earlyLayerRows[layer - 1] = &linkRows.front();
                            target->frames[layer] = static_cast<musx::dom::Cmper>(static_cast<std::uint16_t>(linkRows.front().words[0]));
                        }
                    }
                }
            }
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<musx::dom::details::GFrameHold>(partId, staffId, std::nullopt, measure);
                reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
                const auto stored = [&](const char* member, std::size_t slot, std::int64_t raw) {
                    if (slot >= words.size()) {
                        reportFallbackField(reporting, key, member, Reporting::Origin::Unmapped, 0);
                        return;
                    }
                    const auto& row = source->rowOfWord(rows, slot);
                    reportLegacyField(reporting, key, *source, row, member, source->byteOffsetInRow(slot * sizeof(std::uint16_t)), raw);
                };
                if (target->clefId) {
                    stored("clefId", layout.clefSlot, clef);
                } else {
                    reportFallbackField(reporting, key, "clefId", Reporting::Origin::Unmapped, 0);
                }
                if (listSelected) {
                    stored("clefListId", layout.clefSlot, clef);
                } else {
                    reportFallbackField(reporting, key, "clefListId", Reporting::Origin::LegacyBehavior, 0);
                }
                if ((flags & clefDisplayMask) != clefDisplayMask) {
                    stored("showClefMode", layout.flagsSlot, flags);
                } else {
                    reportFallbackField(reporting, key, "showClefMode", Reporting::Origin::Unmapped, 0);
                }
                if (hasClefAfterBarlineFlag) {
                    stored("clefAfterBarline", layout.flagsSlot, flags);
                } else {
                    reportFallbackField(reporting, key, "clefAfterBarline", Reporting::Origin::Unmapped, 0);
                }
                if (hasStoredPercent) {
                    stored("clefPercent", *layout.percentSlot, words[*layout.percentSlot]);
                } else if (usesDefaultPercent && clefOptions) {
                    reportFallbackField(reporting, key, "clefPercent", Reporting::Origin::LegacyBehavior, target->clefPercent);
                } else {
                    reportFallbackField(reporting, key, "clefPercent", Reporting::Origin::Unmapped, 0);
                }
                stored("mirrorFrame", layout.flagsSlot, flags);
                constexpr std::array frameFields{"frame1", "frame2", "frame3", "frame4"};
                for (std::size_t layer = 0; layer < frameFields.size(); ++layer) {
                    if (layer < layout.knownFrames) {
                        const auto slot = layout.firstFrameSlot + layer;
                        stored(frameFields[layer], slot, slot < words.size() ? static_cast<std::uint16_t>(words[slot]) : 0);
                    } else if (earlyLayerRows[layer - 1]) {
                        const auto& row = *earlyLayerRows[layer - 1];
                        reportLegacyField(reporting, key, earlyLayers, row, frameFields[layer], earlyLayers.byteOffsetInRow(0),
                            static_cast<std::uint16_t>(row.words[0]));
                    } else if (words.size() > 3 && words[3] == 0) {
                        reportFallbackField(reporting, key, frameFields[layer], Reporting::Origin::LegacyBehavior, 0);
                    } else {
                        reportFallbackField(reporting, key, frameFields[layer], Reporting::Origin::Unmapped, 0);
                    }
                }
            });
            context.document->getDetails()->add(musx::dom::details::GFrameHold::XmlNodeName, std::move(target));
        }
    }
}

[[nodiscard]] std::size_t gframeHoldFlagsSlot(const SourceProfile& profile)
{
    // Preliminary: presumed flags occupy word 4 before Finale 98 and word 1 beginning
    // with Finale 98. No structural discriminator is known; an absent version uses word 4.
    return sourceAtOrAfter(profile, FormatEpoch::UncompressedLegacy, versions::finale98) ? gframeHoldFinale98FlagsSlot : gframe::earlyFlagsSlot;
}

struct AlternateNotationStyle
{
    StaffStyleAssignStaffTarget::AlternateNotation notation;
    musx::dom::Cmper preferredStyleId;
    std::string_view name;
};

struct AlternateNotationRun
{
    std::uint16_t partId{};
    musx::dom::Cmper staffId{};
    musx::dom::MeasCmper startMeas{};
    musx::dom::MeasCmper endMeas{};
    AlternateNotationStyle style;
    const records::LegacyRow* firstRow{};
    std::size_t flagsSlot{};
};

constexpr std::array canonicalAlternateNotationStyles{
    AlternateNotationStyle{StaffStyleAssignStaffTarget::AlternateNotation::Normal, musx::dom::Cmper{1}, "Normal Notation"},
    AlternateNotationStyle{StaffStyleAssignStaffTarget::AlternateNotation::SlashBeats, musx::dom::Cmper{2}, "Slash Notation"},
    AlternateNotationStyle{StaffStyleAssignStaffTarget::AlternateNotation::Rhythmic, musx::dom::Cmper{3}, "Rhythmic Notation"},
    AlternateNotationStyle{StaffStyleAssignStaffTarget::AlternateNotation::OneBarRepeat, musx::dom::Cmper{4}, "One Bar Repeats"},
    AlternateNotationStyle{StaffStyleAssignStaffTarget::AlternateNotation::TwoBarRepeat, musx::dom::Cmper{5}, "Two Bar Repeats"},
    AlternateNotationStyle{StaffStyleAssignStaffTarget::AlternateNotation::Blank, musx::dom::Cmper{6}, "Blank Notation"},
};

[[nodiscard]] std::optional<AlternateNotationStyle> alternateNotationStyle(std::uint16_t stored)
{
    switch (stored) {
    case 1: return canonicalAlternateNotationStyles[1];
    case 2: return canonicalAlternateNotationStyles[2];
    case 3: return canonicalAlternateNotationStyles[3];
    case 4:
    case 5: return canonicalAlternateNotationStyles[4];
    case 6: return canonicalAlternateNotationStyles[5];
    default: return std::nullopt;
    }
}

void applyAlternateNotationStyleBehavior(StaffStyleAssignStyleTarget& target)
{
    using Notation = StaffStyleAssignStaffTarget::AlternateNotation;
    target.copyable = true;
    target.addToMenu = true;
    target.altSlashDots = true;
    switch (target.altNotation) {
    case Notation::SlashBeats:
        target.altHideSmartShapes = true;
        target.altHideOtherNotes = true;
        target.altHideOtherArtics = true;
        target.altHideOtherLyrics = true;
        target.altHideOtherSmartShapes = true;
        target.altHideOtherExpressions = true;
        break;
    case Notation::Rhythmic:
        target.altHideOtherNotes = true;
        target.altHideOtherArtics = true;
        target.altHideOtherLyrics = true;
        target.altHideOtherSmartShapes = true;
        target.altHideOtherExpressions = true;
        break;
    case Notation::OneBarRepeat:
    case Notation::TwoBarRepeat:
        target.altHideArtics = true;
        target.altHideLyrics = true;
        target.altHideSmartShapes = true;
        target.altHideOtherNotes = true;
        target.altHideOtherArtics = true;
        target.altHideOtherLyrics = true;
        target.altHideOtherSmartShapes = true;
        target.altHideOtherExpressions = true;
        target.hideChords = true;
        target.hideFretboards = true;
        break;
    case Notation::Blank: target.altHideSmartShapes = true; break;
    case Notation::Normal:
    case Notation::BlankWithRests: break;
    }
}

void reportSynthesizedAlternateNotationStyle(const ImportContext& context, const StaffStyleAssignStyleTarget& target)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<StaffStyleAssignStyleTarget>(target.getSourcePartId(), target.getCmper());
        reporting.report().setInstanceOrigin(key, Reporting::Origin::Unmapped);
        const auto behavior = [&](const char* member, std::int64_t value) {
            reporting.report().setField(key, member, {Reporting::Origin::LegacyBehavior, 0, 0, value});
        };
        const auto defaulted = [&](const char* member, std::int64_t value) {
            reporting.report().setField(key, member, {Reporting::Origin::Finale27Default, 0, 0, value});
        };
        behavior("styleName", static_cast<std::int64_t>(target.styleName.size()));
        behavior("copyable", target.copyable);
        behavior("addToMenu", target.addToMenu);
        behavior("instUuid", 0);
        behavior("staffLines", target.staffLines.value_or(0));
        behavior("altNotation", static_cast<std::int64_t>(target.altNotation));
        behavior("altLayer", target.altLayer);
        behavior("altHideArtics", target.altHideArtics);
        behavior("altHideLyrics", target.altHideLyrics);
        behavior("altHideSmartShapes", target.altHideSmartShapes);
        behavior("altRhythmStemsUp", target.altRhythmStemsUp);
        behavior("altSlashDots", target.altSlashDots);
        behavior("altHideOtherNotes", target.altHideOtherNotes);
        behavior("altHideOtherArtics", target.altHideOtherArtics);
        behavior("altHideExpressions", target.altHideExpressions);
        behavior("altHideOtherLyrics", target.altHideOtherLyrics);
        behavior("altHideOtherSmartShapes", target.altHideOtherSmartShapes);
        behavior("altHideOtherExpressions", target.altHideOtherExpressions);
        behavior("hideChords", target.hideChords);
        behavior("hideFretboards", target.hideFretboards);
        behavior("masks.altNotation", target.masks->altNotation);
        behavior("masks.hideChords", target.masks->hideChords);
        behavior("masks.hideFretboards", target.masks->hideFretboards);
        defaulted("botRepeatDotOff", target.botRepeatDotOff);
        defaulted("topRepeatDotOff", target.topRepeatDotOff);
        defaulted("dwRestOffset", target.dwRestOffset);
        defaulted("wRestOffset", target.wRestOffset);
        defaulted("hRestOffset", target.hRestOffset);
        defaulted("otherRestOffset", target.otherRestOffset);
        defaulted("lineSpace", target.lineSpace);
        defaulted("noteFont.fontSize", target.noteFont->fontSize);
        defaulted("stemReversal", target.stemReversal);
    });
}

void reportAlternateNotationStyleSource(const ImportContext& context, const StaffStyleAssignStyleTarget& target, const records::LegacyRow& row,
    std::size_t flagsSlot, std::uint16_t storedType)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<StaffStyleAssignStyleTarget>(target.getSourcePartId(), target.getCmper());
        reporting.report().setField(key, "altNotation",
            {Reporting::Origin::LegacyMusAdjusted, row.blockOffset, row.decodedOffset + flagsSlot * sizeof(std::uint16_t), storedType, gframe::tag});
    });
}

[[nodiscard]] std::shared_ptr<const StaffStyleAssignStyleTarget> findAlternateNotationStyle(
    const ImportContext& context, StaffStyleAssignStaffTarget::AlternateNotation notation)
{
    const auto styles = context.document->getOthers()->getAllSources<StaffStyleAssignStyleTarget>();
    const auto found = std::ranges::find_if(styles, [&](const auto& style) {
        return style->getSourcePartId() == musx::dom::SCORE_PARTID && style->masks && style->masks->altNotation && style->altNotation == notation;
    });
    return found == styles.end() ? nullptr : *found;
}

[[nodiscard]] std::shared_ptr<const StaffStyleAssignStyleTarget> ensureAlternateNotationStyle(const ImportContext& context,
    const AlternateNotationStyle& style, const records::LegacyRow* sourceRow = nullptr, std::size_t flagsSlot = 0, std::uint16_t storedType = 0)
{
    if (auto existing = findAlternateNotationStyle(context, style.notation)) {
        if (sourceRow) {
            reportAlternateNotationStyleSource(context, *existing, *sourceRow, flagsSlot, storedType);
        }
        return existing;
    }

    auto styleId = style.preferredStyleId;
    if (context.document->getOthers()->get<StaffStyleAssignStyleTarget>(musx::dom::SCORE_PARTID, styleId)) {
        const auto nextStyleId = context.document->getOthers()->nextFreeCmper<StaffStyleAssignStyleTarget>(musx::dom::SCORE_PARTID);
        if (!nextStyleId) {
            context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info, "Alternate notation range has no available Staff Style "
                                                                                      "identifier."});
            return nullptr;
        }
        styleId = *nextStyleId;
    }
    auto target =
        std::make_shared<StaffStyleAssignStyleTarget>(context.document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, styleId);
    target->styleName = style.name;
    target->altNotation = style.notation;
    target->staffLines = 5;
    target->instUuid = std::string(musx::dom::uuid::BlankStaff);
    const auto& defaults = finale27StaffDefaults(context);
    if (!target->noteFont) {
        target->noteFont = std::make_shared<musx::dom::FontInfo>(target->getDocument());
    }
    target->botRepeatDotOff = defaults.botRepeatDotOff;
    target->topRepeatDotOff = defaults.topRepeatDotOff;
    target->dwRestOffset = defaults.dwRestOffset;
    target->wRestOffset = defaults.wRestOffset;
    target->hRestOffset = defaults.hRestOffset;
    target->otherRestOffset = defaults.otherRestOffset;
    target->lineSpace = defaults.lineSpace;
    target->noteFont->fontSize = finale27NoteheadFontSize(context);
    target->stemReversal = defaults.stemReversal;
    target->masks = std::make_shared<StaffStyleAssignStyleTarget::Masks>(target);
    target->masks->altNotation = true;
    target->masks->hideChords = true;
    target->masks->hideFretboards = true;
    applyAlternateNotationStyleBehavior(*target);
    reportSynthesizedAlternateNotationStyle(context, *target);
    if (sourceRow) {
        reportAlternateNotationStyleSource(context, *target, *sourceRow, flagsSlot, storedType);
    }
    context.document->getOthers()->add(StaffStyleAssignStyleTarget::XmlNodeName, target);
    return target;
}

void reportSynthesizedAlternateNotationAssignment(
    const ImportContext& context, const StaffStyleAssignTarget& target, const AlternateNotationRun& run, std::uint16_t storedType)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<StaffStyleAssignTarget>(target.getSourcePartId(), target.getCmper(), target.getInci());
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMusAdjusted);
        const auto adjusted = [&](const char* member, std::int64_t value) {
            reporting.report().setField(
                key, member, {Reporting::Origin::LegacyMusAdjusted, run.firstRow->blockOffset, run.firstRow->decodedOffset, value, gframe::tag});
        };
        adjusted("styleId", storedType);
        adjusted("startMeas", run.startMeas);
        adjusted("endMeas", run.endMeas);
        reporting.report().setField(key, "startEdu", {Reporting::Origin::LegacyBehavior, 0, 0, 0});
        reporting.report().setField(key, "endEdu", {Reporting::Origin::LegacyBehavior, 0, 0, target.endEdu});
    });
}

void addAlternateNotationRun(const ImportContext& context, const AlternateNotationRun& run, std::uint16_t storedType)
{
    const auto style = ensureAlternateNotationStyle(context, run.style, run.firstRow, run.flagsSlot, storedType);
    if (!style) {
        return;
    }
    const auto existing = context.document->getOthers()->getArray<StaffStyleAssignTarget>(run.partId, run.staffId);
    const auto inci = static_cast<musx::dom::Inci>(existing.size());
    auto target = std::make_shared<StaffStyleAssignTarget>(context.document, run.partId, musx::dom::EnigmaBase::ShareMode::All, run.staffId, inci);
    target->styleId = style->getCmper();
    target->startMeas = run.startMeas;
    target->startEdu = 0;
    target->endMeas = run.endMeas;
    target->endEdu = (std::numeric_limits<musx::dom::Edu>::max)();
    reportSynthesizedAlternateNotationAssignment(context, *target, run, storedType);
    context.document->getOthers()->add(StaffStyleAssignTarget::XmlNodeName, std::move(target));
    withReporting(context.report,
        [&]<typename Reporting>(Reporting& reporting) { reporting.report().expectStaffStyleAssignments(run.partId, run.staffId, 1, false); });
}

void synthesizeAlternateNotationRanges(const ImportContext& context)
{
    // GF is the pre-Finale-2000 representation. The version gate is confined to
    // the uncompressed epoch; every Coda-banner source predates it, and later
    // epochs do not use it.
    if (!sourcePredatesVersion(context.profile, FormatEpoch::UncompressedLegacy, versions::finale2000)) {
        return;
    }

    for (const auto& style : canonicalAlternateNotationStyles) {
        static_cast<void>(ensureAlternateNotationStyle(context, style));
    }

    const RecordFamilySource source{&context.index.getDetails(), gframe::tag, false, true};
    std::set<std::uint16_t> unknownTypes;
    for (const auto& [partId, staffId] : recordKeys(source)) {
        std::optional<AlternateNotationRun> run;
        std::uint16_t runStoredType{};
        const auto finishRun = [&] {
            if (run) {
                addAlternateNotationRun(context, *run, runStoredType);
            }
            run.reset();
        };
        for (const auto measure : source.pool->secondCmpersForTag(source.identity, staffId, partId)) {
            const auto rows = source.pool->getArray(source.identity, staffId, measure, partId);
            if (rows.empty()) {
                continue;
            }
            const auto flagsSlot = gframeHoldFlagsSlot(context.profile);
            const auto storedType =
                static_cast<std::uint16_t>(static_cast<std::uint16_t>(rows.front().words[flagsSlot]) & gframeHoldAlternateNotationMask);
            const auto style = alternateNotationStyle(storedType);
            if (!style) {
                finishRun();
                if (storedType != 0) {
                    unknownTypes.insert(storedType);
                }
                continue;
            }
            const auto measureId = static_cast<musx::dom::MeasCmper>(measure);
            if (run && run->style.notation == style->notation && measureId == static_cast<musx::dom::MeasCmper>(run->endMeas + 1)) {
                run->endMeas = measureId;
                continue;
            }
            finishRun();
            run = AlternateNotationRun{partId, staffId, measureId, measureId, *style, &rows.front(), flagsSlot};
            runStoredType = storedType;
        }
        finishRun();
    }
    for (const auto storedType : unknownTypes) {
        context.report.diagnostics.push_back(
            {musx::util::Logger::LogLevel::Info, "Alternate notation range has unsupported type " + std::to_string(storedType) + "."});
    }
}

} // namespace

void importGFrameHolds(const ImportContext& context)
{
    importGFrameHoldRecords(context);
    context.pending.defer(DeferredStage::CompletePools, [&context] { synthesizeAlternateNotationRanges(context); });
}

} // namespace details
} // namespace finale_mus_reader
