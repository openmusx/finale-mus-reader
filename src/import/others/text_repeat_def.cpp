// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "import/support/text_encoding.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using Definition = musx::dom::others::TextRepeatDef;
using Text = musx::dom::others::TextRepeatText;
constexpr auto definitionTag = records::packTag("RS");
constexpr auto textTag = records::packTag("RT");
constexpr records::LegacyTag definitionClass = 0x00f4;
constexpr records::LegacyTag textClass = 0x00f6;

} // namespace

void importTextRepeatDefs(const ImportContext& context)
{
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), definitionTag, definitionClass);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto payload = collectRecordPayload(*source, rows);
        if (payload.size() < 12) {
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Info, "Text-repeat definition " + std::to_string(cmper) + " is shorter than its layout."});
            continue;
        }
        const auto word = [&](std::size_t slot) { return payloadWord(payload, slot * 2, context.profile.byteOrder); };
        const auto flags = word(5);
        const auto alignment = flags & 0x0003U;
        if (alignment == 3) {
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Info, "Text-repeat definition " + std::to_string(cmper) + " uses unsupported justification code 3."});
        }
        if (payload.size() >= 14) {
            const auto passCount = word(6);
            if (passCount > (payload.size() - 14) / 2) {
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Info, "Text-repeat definition " + std::to_string(cmper) + " has an incomplete pass list."});
                continue;
            }
        }
        auto target = createOthersRecordTarget<Definition>(context.document, *source, rows.front(), cmper);
        target->font->fontId = context.construction.assignFontId(word(2));
        target->font->fontSize = word(3);
        target->font->setEnigmaStyles(word(4));
        target->justification = alignment == 1   ? musx::dom::AlignJustify::Right
                                : alignment == 2 ? musx::dom::AlignJustify::Center
                                                 : musx::dom::AlignJustify::Left;
        target->poundReplace = (flags & 0x0080U) != 0   ? Definition::PoundReplaceOption::MeasureNumber
                               : (flags & 0x0040U) != 0 ? Definition::PoundReplaceOption::RepeatID
                                                        : Definition::PoundReplaceOption::Passes;
        target->hasEnclosure = (flags & (context.profile.version && context.profile.version->major <= 2 ? 0x0100U : 0x0800U)) != 0;
        target->useThisFont = (flags & 0x0020U) != 0;
        if (payload.size() >= 14) {
            const auto passCount = word(6);
            for (std::size_t index = 0; index < passCount; ++index) {
                target->passList.push_back(static_cast<std::int16_t>(word(7 + index)));
            }
        }
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            const auto key = reporting.template instanceKey<Definition>(partId, cmper);
            reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
            const auto field = [&](const char* name, std::size_t slot, std::int64_t value) {
                reportLegacyField(reporting, key, *source, source->rowOfWord(rows, slot), name, source->byteOffsetInRow(slot * 2), value);
            };
            field("font.fontId", 2, word(2));
            field("font.fontSize", 3, target->font->fontSize);
            field("font.bold", 4, target->font->bold);
            field("font.italic", 4, target->font->italic);
            field("font.underline", 4, target->font->underline);
            field("font.strikeout", 4, target->font->strikeout);
            field("font.absolute", 4, target->font->absolute);
            field("font.hidden", 4, target->font->hidden);
            if (alignment == 3) {
                reportLegacyField(reporting, key, *source, source->rowOfWord(rows, 5), "justification", source->byteOffsetInRow(10), alignment,
                    Reporting::Origin::Unmapped);
            } else {
                field("justification", 5, static_cast<int>(target->justification));
            }
            field("poundReplace", 5, static_cast<int>(target->poundReplace));
            field("hasEnclosure", 5, target->hasEnclosure);
            field("useThisFont", 5, target->useThisFont);
            if (payload.size() >= 14) {
                field("passList", 6, static_cast<std::int64_t>(target->passList.size()));
                for (std::size_t index = 0; index < target->passList.size(); ++index) {
                    const auto slot = 7 + index;
                    const auto name = "passList[" + std::to_string(index) + "]";
                    field(name.c_str(), slot, target->passList[index]);
                }
            } else {
                reportFallbackField(reporting, key, "passList", Reporting::Origin::Unmapped, 0);
            }
        });
        context.document->getOthers()->add(Definition::XmlNodeName, std::move(target));
    }
}

void importTextRepeatTexts(const ImportContext& context)
{
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), textTag, textClass);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto payload = collectRecordPayload(*source, rows);
        auto target = createOthersRecordTarget<Text>(context.document, *source, rows.front(), cmper);
        if (versions::storesUnicodeCodepoints(context.profile.version)) {
            target->text = text::utf16ToUtf8(payloadWords(payload, context.profile.byteOrder));
        } else {
            const auto narrow = payloadString(payload, 0, payload.size());
            const auto definition = context.document->getOthers()->get<Definition>(partId, cmper);
            target->text = definition ? text::toUtf8(narrow, context.document, definition->font->fontId, text::UnresolvedFontFallback::Text)
                                      : text::toUtf8(narrow, context.profile.platform);
        }
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            const auto key = reporting.template instanceKey<Text>(partId, cmper);
            reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
            reportLegacyField(reporting, key, *source, rows.front(), "text", 0, static_cast<std::int64_t>(payload.size()));
        });
        context.document->getOthers()->add(Text::XmlNodeName, std::move(target));
    }
}

} // namespace others
} // namespace finale_mus_reader
