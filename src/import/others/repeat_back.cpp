// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>

#include "import/shared/repeat_flags.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using RepeatBackTarget = musx::dom::others::RepeatBack;
constexpr auto repeatBackTag = records::packTag("BR");
constexpr records::LegacyTag repeatBackClass = 0x00cb;

void reportRepeatBack(const ImportContext& context, const RepeatBackTarget& target, const RecordFamilySource& source,
    std::span<const records::LegacyRow> rows, bool modernFlags, bool hasTail)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<RepeatBackTarget>(target.getSourcePartId(), target.getCmper());
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        const auto field = [&](const char* name, std::size_t slot, std::int64_t value) {
            const auto& row = source.rowOfWord(rows, slot);
            reportLegacyField(reporting, key, source, row, name, source.byteOffsetInRow(slot * 2), value);
        };
        const auto defaultField = [&](const char* name, std::int64_t value) {
            reportFallbackField(reporting, key, name, Reporting::Origin::Finale27Default, value);
        };
        field("passNumber", 1, target.passNumber);
        field("targetValue", 2, target.targetValue);
        field("leftHPos", 3, target.leftHPos);
        field("leftVPos", 4, target.leftVPos);
        field("individualPlacement", 5, target.individualPlacement);
        if (modernFlags) {
            field("topStaffOnly", 5, target.topStaffOnly);
        } else {
            reportFallbackField(reporting, key, "topStaffOnly", Reporting::Origin::LegacyBehavior, target.topStaffOnly);
        }
        if (modernFlags) {
            field("hidden", 5, target.hidden);
        } else {
            reportFallbackField(reporting, key, "hidden", Reporting::Origin::LegacyBehavior, target.hidden);
        }
        field("resetOnAction", 5, target.resetOnAction);
        field("jumpAction", 5, static_cast<int>(target.jumpAction));
        field("trigger", 5, static_cast<int>(target.trigger));
        if (modernFlags) {
            field("staffList", 6, target.staffList);
        } else {
            defaultField("staffList", target.staffList);
        }
        if (hasTail) {
            field("rightHPos", 9, target.rightHPos);
            field("rightVPos", 10, target.rightVPos);
        } else {
            defaultField("rightHPos", target.rightHPos);
            defaultField("rightVPos", target.rightVPos);
        }
    });
}

} // namespace

void importRepeatBacks(const ImportContext& context)
{
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), repeatBackTag, repeatBackClass);
    if (!source) {
        return;
    }
    // The trigger bits change at Finale 2000; the action and reset flags change at Finale 2005.
    const bool modernFlags = source->classRecords || sourceAtOrAfter(context.profile, FormatEpoch::DclLegacy, versions::finale2005);
    const bool earlyTriggers = context.profile.epoch == FormatEpoch::CodaBanner
                               || sourcePredatesVersion(context.profile, FormatEpoch::UncompressedLegacy, versions::finale2000);
    if (context.profile.epoch == FormatEpoch::DclLegacy && !context.profile.version) {
        context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info, "Repeat-back layout requires a source version."});
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto payload = collectRecordPayload(*source, rows);
        const bool hasTail = payload.size() >= 24;
        if (payload.size() < 12 || (!hasTail && context.profile.epoch != FormatEpoch::CodaBanner)) {
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Info, "Repeat-back record for measure " + std::to_string(cmper) + " is shorter than its layout."});
            continue;
        }
        const auto word = [&](std::size_t slot) { return payloadWord(payload, slot * 2, context.profile.byteOrder); };
        const auto signedWord = [&](std::size_t slot) { return static_cast<std::int16_t>(word(slot)); };
        const auto flags = word(5);
        const auto triggerBits = static_cast<std::uint16_t>((flags & 0x0c00) >> 10);
        const auto actionBits = static_cast<std::uint16_t>((flags & 0x0070) >> 4);
        if ((!earlyTriggers && triggerBits >= repeat::triggers.size()) || (earlyTriggers && (flags & 0x0900) == 0x0900)
            || (modernFlags && actionBits >= repeat::actions.size())) {
            context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
                "Repeat-back record for measure " + std::to_string(cmper) + " has an unknown action or trigger."});
            continue;
        }
        auto target = createOthersRecordTarget<RepeatBackTarget>(context.document, *source, rows.front(), cmper);
        target->passNumber = signedWord(1);
        target->targetValue = signedWord(2);
        target->leftHPos = signedWord(3);
        target->leftVPos = signedWord(4);
        target->individualPlacement = (flags & 0x0001) != 0;
        target->topStaffOnly = modernFlags && (flags & 0x0002) != 0;
        target->hidden = modernFlags && (flags & 0x4000) != 0;
        target->resetOnAction = (flags & (modernFlags ? 0x0004 : 0x0040)) != 0;
        if (modernFlags) {
            target->staffList = word(6);
        }
        target->jumpAction = modernFlags             ? repeat::actions[actionBits]
                             : (flags & 0x1000) != 0 ? musx::dom::others::RepeatActionType::JumpRelative
                                                     : musx::dom::others::RepeatActionType::JumpAbsolute;
        if (earlyTriggers) {
            target->trigger = (flags & 0x0100) != 0   ? musx::dom::others::RepeatTriggerType::UntilPass
                              : (flags & 0x0800) != 0 ? musx::dom::others::RepeatTriggerType::OnPass
                                                      : musx::dom::others::RepeatTriggerType::Always;
        } else {
            target->trigger = repeat::triggers[triggerBits];
        }
        if (hasTail) {
            target->rightHPos = signedWord(9);
            target->rightVPos = signedWord(10);
        }
        reportRepeatBack(context, *target, *source, rows, modernFlags, hasTail);
        context.document->getOthers()->add(RepeatBackTarget::XmlNodeName, std::move(target));
    }
}

} // namespace others
} // namespace finale_mus_reader
