// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "import/shared/repeat_flags.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using Target = musx::dom::others::TextRepeatAssign;
constexpr auto assignmentTag = records::packTag("RU");
constexpr auto styleTag = records::packTag("RS");
constexpr records::LegacyTag assignmentClass = 0x00f3;

} // namespace

void importTextRepeatAssigns(const ImportContext& context)
{
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), assignmentTag, assignmentClass);
    if (!source) {
        return;
    }
    const bool selfContained = source->classRecords || sourceAtOrAfter(context.profile, FormatEpoch::DclLegacy, versions::finale2005);
    const bool earlyTriggers = context.profile.epoch == FormatEpoch::CodaBanner
                               || sourcePredatesVersion(context.profile, FormatEpoch::UncompressedLegacy, versions::finale2000);
    if (context.profile.epoch == FormatEpoch::DclLegacy && !context.profile.version) {
        context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info, "Text-repeat assignment layout requires a source version."});
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto allRows = source->pool->getArray(source->identity, cmper, 0, partId);
        if (allRows.empty()) {
            continue;
        }
        const std::size_t wordsPerAssignment = selfContained ? 12 : 6;
        const std::size_t rowsPerAssignment = source->classRecords || !selfContained ? 1 : 2;
        for (std::size_t rowIndex = 0; rowIndex < allRows.size(); rowIndex += rowsPerAssignment) {
            const auto rows = allRows.subspan(rowIndex, (std::min)(rowsPerAssignment, allRows.size() - rowIndex));
            const auto payload = collectRecordPayload(*source, rows);
            if (payload.size() % 2 != 0) {
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Info, "Text-repeat assignment for measure " + std::to_string(cmper) + " has an incomplete word."});
                continue;
            }
            for (std::size_t wordOffset = 0; wordOffset < payload.size() / 2; wordOffset += wordsPerAssignment) {
                const auto availableWords = payload.size() / 2 - wordOffset;
                // Believed: a final self-contained incidence can stop after the six common words.
                if (availableWords < 6 || (selfContained && availableWords < wordsPerAssignment && availableWords != 6)) {
                    context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
                        "Text-repeat assignment for measure " + std::to_string(cmper) + " has an incomplete incidence."});
                    break;
                }
                const auto word = [&](std::size_t slot) { return payloadWord(payload, (wordOffset + slot) * 2, context.profile.byteOrder); };
                const auto signedWord = [&](std::size_t slot) { return static_cast<std::int16_t>(word(slot)); };
                const auto flags = word(5);
                const auto actionBits = static_cast<std::uint16_t>((flags & 0x0070) >> 4);
                const auto triggerBits = static_cast<std::uint16_t>((flags & 0x0c00) >> 10);
                if ((selfContained && actionBits >= repeat::actions.size()) || (!earlyTriggers && triggerBits >= repeat::triggers.size())
                    || (earlyTriggers && (flags & 0x0900) == 0x0900)) {
                    context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
                        "Text-repeat assignment for measure " + std::to_string(cmper) + " has an unknown action or trigger."});
                    continue;
                }
                const auto inci = static_cast<musx::dom::Inci>(source->classRecords ? rows.front().inci + wordOffset / wordsPerAssignment
                                                               : selfContained      ? rows.front().inci / 2
                                                                                    : rows.front().inci);
                auto target = createOthersRecordTarget<Target>(context.document, *source, rows.front(), cmper, inci);
                target->passNumber = signedWord(1);
                target->targetValue = signedWord(2);
                target->textRepeatId = word(3);
                target->individualPlacement = (flags & 0x0001) != 0;
                target->topStaffOnly = selfContained && (flags & 0x0002) != 0;
                target->hidden = selfContained && (flags & 0x4000) != 0;
                target->resetOnAction = (flags & (selfContained ? 0x0004 : 0x0040)) != 0;
                target->jumpOnMultiplePasses = (flags & 0x0008) != 0;
                target->autoUpdate = selfContained && (flags & 0x0080) != 0;
                target->jumpAction = selfContained           ? repeat::actions[actionBits]
                                     : (flags & 0x0200) != 0 ? musx::dom::others::RepeatActionType::JumpToMark
                                     : (flags & 0x1000) != 0 ? musx::dom::others::RepeatActionType::JumpRelative
                                                             : musx::dom::others::RepeatActionType::JumpAbsolute;
                target->trigger = earlyTriggers ? (flags & 0x0100) != 0   ? musx::dom::others::RepeatTriggerType::UntilPass
                                                  : (flags & 0x0800) != 0 ? musx::dom::others::RepeatTriggerType::OnPass
                                                                          : musx::dom::others::RepeatTriggerType::Always
                                                : repeat::triggers[triggerBits];
                target->jumpIfIgnoring = (flags & 0x2000) != 0;
                if (selfContained) {
                    target->horzPos = signedWord(0);
                    target->vertPos = signedWord(4);
                    if (availableWords >= wordsPerAssignment) {
                        target->staffList = word(6);
                    }
                }

                std::optional<RecordFamilySource> style;
                std::span<const records::LegacyRow> styleRows;
                if (!selfContained) {
                    style = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), styleTag, 0x00f4);
                    if (style) {
                        styleRows = style->pool->getArray(style->identity, target->textRepeatId, 0, partId);
                        if (styleRows.empty() && partId != musx::dom::SCORE_PARTID) {
                            styleRows = style->pool->getArray(style->identity, target->textRepeatId, 0, musx::dom::SCORE_PARTID);
                        }
                        if (!styleRows.empty()) {
                            const auto stylePayload = collectRecordPayload(*style, styleRows);
                            if (stylePayload.size() >= 4) {
                                target->horzPos = static_cast<std::int16_t>(payloadWord(stylePayload, 0, context.profile.byteOrder));
                                target->vertPos = static_cast<std::int16_t>(payloadWord(stylePayload, 2, context.profile.byteOrder));
                            }
                        }
                    }
                }

                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                    const auto key = reporting.template instanceKey<Target>(partId, cmper, target->getInci().value_or(0));
                    reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
                    const auto field = [&](const char* name, std::size_t slot, std::int64_t value) {
                        const auto physicalSlot = source->classRecords ? wordOffset + slot : slot;
                        reportLegacyField(
                            reporting, key, *source, source->rowOfWord(rows, physicalSlot), name, source->byteOffsetInRow(physicalSlot * 2), value);
                    };
                    const auto fallback = [&](const char* name, std::int64_t value) {
                        reportFallbackField(reporting, key, name, Reporting::Origin::Unmapped, value);
                    };
                    field("passNumber", 1, target->passNumber);
                    field("targetValue", 2, target->targetValue);
                    field("textRepeatId", 3, target->textRepeatId);
                    if (selfContained) {
                        field("horzPos", 0, target->horzPos);
                        field("vertPos", 4, target->vertPos);
                        if (availableWords >= wordsPerAssignment) {
                            field("staffList", 6, target->staffList);
                        } else {
                            reportFallbackField(reporting, key, "staffList", Reporting::Origin::LegacyBehavior, target->staffList);
                        }
                    } else {
                        if (style && !styleRows.empty()) {
                            reportLegacyField(
                                reporting, key, *style, style->rowOfWord(styleRows, 0), "horzPos", style->byteOffsetInRow(0), target->horzPos);
                            reportLegacyField(
                                reporting, key, *style, style->rowOfWord(styleRows, 1), "vertPos", style->byteOffsetInRow(2), target->vertPos);
                        } else {
                            fallback("horzPos", target->horzPos);
                            fallback("vertPos", target->vertPos);
                        }
                        reportFallbackField(reporting, key, "staffList", Reporting::Origin::Finale27Default, target->staffList);
                    }
                    field("individualPlacement", 5, target->individualPlacement);
                    if (selfContained) {
                        field("topStaffOnly", 5, target->topStaffOnly);
                        field("hidden", 5, target->hidden);
                    } else {
                        reportFallbackField(reporting, key, "topStaffOnly", Reporting::Origin::LegacyBehavior, target->topStaffOnly);
                        reportFallbackField(reporting, key, "hidden", Reporting::Origin::LegacyBehavior, target->hidden);
                    }
                    field("resetOnAction", 5, target->resetOnAction);
                    field("jumpOnMultiplePasses", 5, target->jumpOnMultiplePasses);
                    field("jumpAction", 5, static_cast<int>(target->jumpAction));
                    if (selfContained) {
                        field("autoUpdate", 5, target->autoUpdate);
                    } else {
                        reportFallbackField(reporting, key, "autoUpdate", Reporting::Origin::LegacyBehavior, target->autoUpdate);
                    }
                    field("trigger", 5, static_cast<int>(target->trigger));
                    field("jumpIfIgnoring", 5, target->jumpIfIgnoring);
                });
                context.document->getOthers()->add(Target::XmlNodeName, std::move(target));
            }
        }
    }
}

} // namespace others
} // namespace finale_mus_reader
