// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>

#include "import/shared/repeat_flags.h"
#include "import/support/text_encoding.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using EndingStartTarget = musx::dom::others::RepeatEndingStart;
using EndingPassTarget = musx::dom::others::RepeatPassList;
using EndingTextTarget = musx::dom::others::RepeatEndingText;
constexpr auto endingStartTag = records::packTag("ES");
constexpr auto endingPassTag = records::packTag("EE");
constexpr auto endingTextTag = records::packTag("ET");
constexpr records::LegacyTag endingStartClass = 0x00cc;
constexpr records::LegacyTag endingTextClass = 0x00cd;
constexpr records::LegacyTag endingPassClass = 0x00ce;

void reportEndingStart(const ImportContext& context, const EndingStartTarget& target, const RecordFamilySource& source,
    std::span<const records::LegacyRow> rows, bool modernFlags, bool hasTail, bool earlyEndLine, std::int16_t storedTargetValue)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto highByte = context.profile.byteOrder == ByteOrder::BigEndian ? 0U : 1U;
        const auto lowByte = 1U - highByte;
        const auto key = reporting.template instanceKey<EndingStartTarget>(target.getSourcePartId(), target.getCmper());
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        const auto field = [&](const char* name, std::size_t slot, std::int64_t value, std::size_t byteInWord = 0) {
            const auto& row = source.rowOfWord(rows, slot);
            reportLegacyField(reporting, key, source, row, name, source.byteOffsetInRow(slot * 2) + byteInWord, value);
        };
        const auto fallback = [&](const char* name, typename Reporting::Origin origin, std::int64_t value) {
            reportFallbackField(reporting, key, name, origin, value);
        };
        if (modernFlags) {
            field("staffList", 0, target.staffList);
        } else {
            fallback("staffList", Reporting::Origin::Finale27Default, target.staffList);
        }
        field("targetValue", 1, storedTargetValue);
        if (target.jumpAction == musx::dom::others::RepeatActionType::Stop) {
            reporting.report().findField(key, "targetValue")->origin = Reporting::Origin::LegacyMusAdjusted;
        }
        field("textHPos", 2, target.textHPos, hasTail ? 0U : highByte);
        field("leftHPos", 3, target.leftHPos, hasTail ? 0U : highByte);
        field("leftVPos", 4, target.leftVPos, hasTail ? 0U : highByte);
        field("individualPlacement", 5, target.individualPlacement);
        field("jumpAction", 5, static_cast<int>(target.jumpAction));
        field("jumpIfIgnoring", 5, target.jumpIfIgnoring);
        if (modernFlags) {
            field("topStaffOnly", 5, target.topStaffOnly);
            field("hidden", 5, target.hidden);
        } else {
            fallback("topStaffOnly", Reporting::Origin::LegacyBehavior, target.topStaffOnly);
            fallback("hidden", Reporting::Origin::LegacyBehavior, target.hidden);
        }
        fallback("trigger", Reporting::Origin::LegacyBehavior, static_cast<int>(target.trigger));
        if (hasTail) {
            field("endLineVPos", earlyEndLine ? 10 : 7, target.endLineVPos);
            field("textVPos", 8, target.textVPos);
            field("rightHPos", 9, target.rightHPos);
            field("rightVPos", 10, target.rightVPos);
        } else {
            field("endLineVPos", 4, target.endLineVPos, lowByte);
            field("textVPos", 2, target.textVPos, lowByte);
            field("rightHPos", 3, target.rightHPos, lowByte);
            field("rightVPos", 4, target.rightVPos, lowByte);
        }
    });
}

} // namespace

void importRepeatEndingStarts(const ImportContext& context)
{
    const auto source =
        selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), endingStartTag, endingStartClass);
    if (!source) {
        return;
    }
    // The stored F2005-and-later record uses the former counter word for its staff-list reference.
    const bool modernFlags = source->classRecords || sourceAtOrAfter(context.profile, FormatEpoch::DclLegacy, versions::finale2005);
    const bool earlyEndLine = context.profile.epoch == FormatEpoch::UncompressedLegacy
                              && sourcePredatesVersion(context.profile, FormatEpoch::UncompressedLegacy, versions::finale2000);
    if (context.profile.epoch == FormatEpoch::DclLegacy && !context.profile.version) {
        context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info, "Repeat-ending layout requires a source version."});
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
                {musx::util::Logger::LogLevel::Info, "Repeat-ending record for measure " + std::to_string(cmper) + " is shorter than its layout."});
            continue;
        }
        const auto word = [&](std::size_t slot) { return payloadWord(payload, slot * 2, context.profile.byteOrder); };
        const auto signedWord = [&](std::size_t slot) { return static_cast<std::int16_t>(word(slot)); };
        const auto signedHighByte = [&](std::size_t slot) { return static_cast<std::int8_t>(word(slot) >> 8); };
        const auto signedLowByte = [&](std::size_t slot) { return static_cast<std::int8_t>(word(slot) & 0xff); };
        const auto flags = word(5);
        const auto actionBits = static_cast<std::uint16_t>((flags & 0x0070) >> 4);
        if (modernFlags && actionBits >= repeat::actions.size()) {
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Info, "Repeat-ending record for measure " + std::to_string(cmper) + " has an unknown action."});
            continue;
        }
        auto target = createOthersRecordTarget<EndingStartTarget>(context.document, *source, rows.front(), cmper);
        if (modernFlags) {
            target->staffList = word(0);
        }
        const auto storedTargetValue = signedWord(1);
        target->targetValue = storedTargetValue;
        target->textHPos = hasTail ? signedWord(2) : signedHighByte(2);
        if (!hasTail) {
            target->textVPos = static_cast<std::uint8_t>(word(2) & 0xff);
        }
        target->leftHPos = hasTail ? signedWord(3) : signedHighByte(3);
        target->leftVPos = hasTail ? signedWord(4) : signedHighByte(4);
        target->individualPlacement = (flags & 0x0001) != 0;
        target->topStaffOnly = modernFlags && (flags & 0x0002) != 0;
        target->hidden = modernFlags && (flags & 0x0004) != 0;
        target->jumpIfIgnoring = (flags & 0x2000) != 0;
        const bool earlyRelative = context.profile.epoch == FormatEpoch::CodaBanner || earlyEndLine;
        target->jumpAction = modernFlags                                    ? repeat::actions[actionBits]
                             : !earlyRelative && (flags & 0x0c00) == 0x0c00 ? musx::dom::others::RepeatActionType::Stop
                             : earlyRelative || (flags & 0x1000) != 0       ? musx::dom::others::RepeatActionType::JumpRelative
                                                                            : musx::dom::others::RepeatActionType::JumpAbsolute;
        if (target->jumpAction == musx::dom::others::RepeatActionType::Stop) {
            // Stop ignores the stored destination and exposes its canonical unused target.
            target->targetValue = -1;
        }
        target->trigger = musx::dom::others::RepeatTriggerType::OnPass;
        if (hasTail) {
            target->endLineVPos = signedWord(earlyEndLine ? 10 : 7);
            target->textVPos = signedWord(8);
            target->rightHPos = signedWord(9);
            target->rightVPos = signedWord(10);
        } else {
            target->endLineVPos = signedLowByte(4);
            target->rightHPos = signedLowByte(3);
            target->rightVPos = signedLowByte(4);
        }
        reportEndingStart(context, *target, *source, rows, modernFlags, hasTail, earlyEndLine, storedTargetValue);
        context.document->getOthers()->add(EndingStartTarget::XmlNodeName, std::move(target));
    }
}

void importRepeatEndingTexts(const ImportContext& context)
{
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), endingTextTag, endingTextClass);
    if (!source) {
        return;
    }
    // The text is null-terminated across as many incidences as it needs: UTF-16 code units in
    // the record's byte order from Finale 2012, otherwise narrow bytes. Believed: narrow bytes
    // are in the encoding of the ending font from FontOptions, or of the source platform when
    // there is no ending font. Font id 0 is not a stand-in: it names the music font. Finale 27's
    // upgrade decodes them as Mac Roman even when that font names a Windows character set; the
    // font is used here.
    const auto font = musx::dom::options::FontOptions::getFontInfoOrNull(context.document, musx::dom::options::FontOptions::FontType::Ending);
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto payload = collectRecordPayload(*source, rows);
        auto target = createOthersRecordTarget<EndingTextTarget>(context.document, *source, rows.front(), cmper);
        if (versions::storesUnicodeCodepoints(context.profile.version)) {
            target->text = text::utf16ToUtf8(payloadWords(payload, context.profile.byteOrder));
        } else {
            const auto narrow = payloadString(payload, 0, payload.size());
            target->text = font ? text::toUtf8(narrow, context.document, font->fontId, text::UnresolvedFontFallback::Text)
                                : text::toUtf8(narrow, context.profile.platform);
        }
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            const auto key = reporting.template instanceKey<EndingTextTarget>(partId, cmper);
            reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
            reporting.report().setField(key, "text",
                typename Reporting::FieldInfo{Reporting::Origin::LegacyMus, rows.front().blockOffset, rows.front().decodedOffset,
                    static_cast<std::int64_t>(payload.size()), source->identity});
        });
        context.document->getOthers()->add(EndingTextTarget::XmlNodeName, std::move(target));
    }
}

void importRepeatPassLists(const ImportContext& context)
{
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), endingPassTag, endingPassClass);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        auto target = createOthersRecordTarget<EndingPassTarget>(context.document, *source, rows.front(), cmper);
        bool terminated = false;
        for (const auto& row : rows) {
            const auto payload = source->pool->effectivePayloadOf(row);
            for (std::size_t offset = 0; offset + 2 <= payload.size() && !terminated; offset += 2) {
                const auto value = payloadWord(payload, offset, context.profile.byteOrder);
                if (value == 0) {
                    terminated = true;
                    break;
                }
                target->values.push_back(value);
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                    const auto key = reporting.template instanceKey<EndingPassTarget>(partId, cmper);
                    reporting.report().setField(key, "values[" + std::to_string(target->values.size() - 1) + "]",
                        {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset + offset, value, source->identity});
                });
            }
        }
        if (target->values.empty()) {
            continue;
        }
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            reporting.report().setInstanceOrigin(reporting.template instanceKey<EndingPassTarget>(partId, cmper), Reporting::Origin::LegacyMus);
        });
        context.document->getOthers()->add(EndingPassTarget::XmlNodeName, std::move(target));
    }
}

} // namespace others
} // namespace finale_mus_reader
