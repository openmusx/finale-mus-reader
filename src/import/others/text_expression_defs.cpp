// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"
#include "import/shared/expression_common.h"
#include "import/support/enigma_text.h"
#include "import/support/legacy_font.h"
#include "musx/musx.h"

#include <string>
#include <string_view>

namespace finale_mus_reader {
namespace others {
namespace {

using TextExpressionTarget = musx::dom::others::TextExpressionDef;
constexpr std::size_t primeExpressionHeaderSize = 36;

/// @brief Applies an inline expression's hide controls the way Finale's upgrade does.
/// @details Each '<' deepens a hidden span and each '>' makes it shallower; the effects change only
/// when the depth leaves or returns to zero, so an unmatched '>' cancels the next '<'. A backslash
/// makes the following bracket literal only outside a hidden span, and otherwise is kept with the
/// character it precedes. The controls themselves are removed.
std::string applyExpressionHideControls(std::string_view text, int baseEffects, int hiddenEffects)
{
    std::string result;
    int depth = 0;
    bool escaped = false;
    for (const auto ch : text) {
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch != '<' && ch != '>') {
            if (escaped) {
                result += '\\';
            }
            result += ch;
            escaped = false;
            continue;
        }
        if (depth <= 0 && escaped) {
            result += ch;
            escaped = false;
            continue;
        }
        escaped = false;
        if (ch == '<') {
            if (depth == 0) {
                result += "^nfx(" + std::to_string(hiddenEffects) + ")";
            }
            ++depth;
        } else if (--depth == 0) {
            result += "^nfx(" + std::to_string(baseEffects) + ")";
        }
    }
    return result;
}

/// @brief Replaces an inline expression's playback placeholder the way Finale's upgrade does.
/// @details "##" stands for a literal '#'. The first remaining '#' becomes the insert the pound
/// mode selects, and any later one stays literal. Mode 3 inserts nothing but still collapses "##".
std::string replaceExpressionPound(std::string_view text, std::uint16_t poundMode)
{
    std::string result;
    bool replaced = false;
    for (std::size_t at = 0; at < text.size(); ++at) {
        if (text[at] != '#' || replaced) {
            result += text[at];
            continue;
        }
        if (at + 1 < text.size() && text[at + 1] == '#') {
            result += '#';
            ++at;
            continue;
        }
        replaced = true;
        switch (poundMode) {
        case 0: result += "^value()"; break;
        case 1: result += "^pass()"; break;
        case 2: result += "^control()"; break;
        default: result += '#'; break;
        }
    }
    return result;
}

void synthesizeExpressionText(const ImportContext& context, const std::shared_ptr<TextExpressionTarget>& target, const records::LegacyRow& row,
    std::span<const std::uint8_t> payload)
{
    using Block = musx::dom::others::TextBlock;
    using RawText = musx::dom::texts::ExpressionText;
    const auto textNumber = context.document->getTexts()->nextFreeCmper<RawText>();
    const auto blockNumber = context.document->getOthers()->nextFreeCmper<Block>(target->getSourcePartId());
    if (!textNumber || !blockNumber) {
        context.report.diagnostics.push_back(
            {musx::util::Logger::LogLevel::Warning, "Text expression synthesis exhausted the text identifier space."});
        return;
    }
    const auto packed = payloadWord(payload, 0, context.profile.byteOrder);
    musx::dom::FontInfo font(context.document);
    const bool coda = context.profile.epoch == FormatEpoch::CodaBanner;
    assignPackedFont(font, context.construction, packed, coda);
    font.setEnigmaStyles(payloadWord(payload, 2, context.profile.byteOrder));
    const auto flags = payloadWord(payload, 10, context.profile.byteOrder);
    const bool hideText = sourceAtOrAfter(context.profile, FormatEpoch::UncompressedLegacy, versions::finale97) && bool(flags & 0x0200U);
    // From F2002 '<' and '>' in the text are the hide controls. Before F2002 the flag hides the
    // whole text: the upgrade wraps it in one hidden span and escapes its own brackets.
    const bool wholeHidden =
        hideText
        && !(context.profile.epoch == FormatEpoch::DclLegacy && sourceAtOrAfter(context.profile, FormatEpoch::DclLegacy, versions::finale2002));
    const auto baseEffects = font.getEnigmaStyles();
    auto hiddenFont = font;
    hiddenFont.hidden = true;
    const auto hiddenEffects = hiddenFont.getEnigmaStyles();
    const auto plain = text::normalizeLineBreaks(
        text::toUtf8(payloadString(payload, 12, payload.size() - 12), context.document, font.fontId, text::UnresolvedFontFallback::Text));
    std::string enigma;
    for (const auto ch : plain) {
        enigma += ch == '^' ? std::string("^^") : std::string(1, ch);
    }
    if (wholeHidden) {
        std::string wrapped = "<";
        for (const auto ch : enigma) {
            if (ch == '<' || ch == '>') {
                wrapped += '\\';
            }
            wrapped += ch;
        }
        if (wrapped.back() == '\\') {
            wrapped.back() = ' ';
        }
        enigma = wrapped + ">";
    }
    if (hideText) {
        enigma = applyExpressionHideControls(enigma, baseEffects, hiddenEffects);
    }
    enigma = replaceExpressionPound(enigma, static_cast<std::uint16_t>(flags >> 14U));
    auto raw = std::make_shared<RawText>(context.document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, *textNumber);
    // The base font state comes first even when the text opens with a hide control, as Finale writes it.
    raw->text = text::initializeEnigmaTextFontState(std::string(), font) + enigma;
    auto block = std::make_shared<Block>(context.document, target->getSourcePartId(), musx::dom::EnigmaBase::ShareMode::All, *blockNumber);
    block->textId = *textNumber;
    block->textType = Block::TextType::Expression;
    block->lineSpacingPercentage = 100;
    block->newPos36 = true;
    block->showShape = true;
    block->wordWrap = true;
    target->textIdKey = *blockNumber;
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto rawKey = reporting.template instanceKey<RawText>(musx::dom::SCORE_PARTID, *textNumber);
        const auto blockKey = reporting.template instanceKey<Block>(target->getSourcePartId(), *blockNumber);
        reporting.report().setInstanceOrigin(rawKey, Reporting::Origin::LegacyMus);
        reporting.report().setInstanceOrigin(blockKey, Reporting::Origin::LegacyBehavior);
        reporting.report().setField(
            rawKey, "text", typename Reporting::FieldInfo{Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset, 0, textExpressionTag});
        reporting.report().setField(reporting.template instanceKey<TextExpressionTarget>(target->getSourcePartId(), target->getCmper()), "textIdKey",
            typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, row.blockOffset, row.decodedOffset, *blockNumber, textExpressionTag});
    });
    context.document->getTexts()->add(RawText::XmlNodeName, std::move(raw));
    context.document->getOthers()->add(Block::XmlNodeName, std::move(block));
}

constexpr const char* textExpressionFields[] = {"textIdKey", "categoryId", "rehearsalMarkStyle", "value", "execShape", "auxData1", "playPass",
    "hideMeasureNum", "matchPlayback", "useAuxData", "hasEnclosure", "breakMmRest", "createdByHp", "playbackType", "horzMeasExprAlign",
    "vertMeasExprAlign", "horzExprJustification", "measXAdjust", "yAdjustEntry", "yAdjustBaseline", "useCategoryFonts", "useCategoryPos",
    "description"};

void reportAbsentTextExpressionFields(
    ImportReport& report, const ReportInstance& reportInstance, bool prime, bool hasCategory, bool hasRehearsalStyle, bool hasBreakMmRest)
{
    withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
        if (!prime) {
            for (const auto* member : {"description"}) {
                reporting.report().setField(
                    reporting.instanceKey(reportInstance), member, typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, 0});
            }
        }
        if (!hasCategory) {
            for (const auto* member : {"useCategoryFonts", "useCategoryPos"}) {
                reporting.report().setField(
                    reporting.instanceKey(reportInstance), member, typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, 0});
            }
        }
        if (!hasRehearsalStyle) {
            for (const auto* member : {"rehearsalMarkStyle", "hideMeasureNum", "matchPlayback"}) {
                reporting.report().setField(
                    reporting.instanceKey(reportInstance), member, typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, 0});
            }
        }
        if (!hasBreakMmRest) {
            reporting.report().setField(
                reporting.instanceKey(reportInstance), "breakMmRest", typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, 0});
        }
    });
}

} // namespace

void importTextExpressionDefs(const ImportContext& context)
{
    const bool prime = sourceAtOrAfter(context.profile, FormatEpoch::DclLegacy, versions::finale2004);
    const auto source =
        selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), textExpressionTag, textExpressionClass);
    if (!source) {
        return;
    }
    std::vector<std::function<void()>> synthesizedTexts;
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        bool completeRows = !rows.empty();
        for (std::size_t index = 0; index < rows.size() && !source->classRecords; ++index) {
            completeRows = completeRows && rows[index].inci == index;
        }
        const auto payload = collectRecordPayload(*source, rows);
        const auto words = payloadWords(payload, context.profile.byteOrder);
        if (!completeRows || payload.size() < (prime ? primeExpressionHeaderSize : 12)) {
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Warning, "Text expression " + std::to_string(cmper) + " has an incomplete header."});
            continue;
        }
        auto target = createOthersRecordTarget<TextExpressionTarget>(context.document, *source, rows.front(), cmper);
        if (!target) {
            continue;
        }
        const auto reportInstance = ReportInstance::of<TextExpressionTarget>(partId, cmper);
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            reporting.report().setInstanceOrigin(reporting.instanceKey(reportInstance), Reporting::Origin::LegacyMus);
            for (const auto* field : textExpressionFields) {
                reporting.report().setField(
                    reporting.instanceKey(reportInstance), field, typename Reporting::FieldInfo{Reporting::Origin::Unmapped, 0, 0, 0});
            }
        });
        const auto assign = [&](auto member, const char* name, auto value, std::size_t slot, bool adjusted = false) {
            target.get()->*member = value;
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto& row = source->rowOfWord(rows, slot);
                const auto offset = source->byteOffsetInRow(slot * 2);
                reporting.report().setField(reporting.instanceKey(reportInstance), name,
                    typename Reporting::FieldInfo{adjusted ? Reporting::Origin::LegacyMusAdjusted : Reporting::Origin::LegacyMus, row.blockOffset,
                        row.decodedOffset + offset, words[slot], source->identity});
            });
        };
        const auto flags = static_cast<std::uint16_t>(words[5]);
        const bool hasCategory = sourceAtOrAfter(context.profile, FormatEpoch::ZlibLegacy, versions::finale2009);
        // Believed: automatic rehearsal styles and the hide-number/match-playback
        // controls share the Finale 2010 introduction boundary.
        const bool hasRehearsalStyle = sourceAtOrAfter(context.profile, FormatEpoch::ZlibLegacy, versions::finale2010);
        const bool hasBreakMmRest = sourceAtOrAfter(context.profile, FormatEpoch::DclLegacy, versions::finale2002);
        reportAbsentTextExpressionFields(context.report, reportInstance, prime, hasCategory, hasRehearsalStyle, hasBreakMmRest);
        if (!prime) {
            applyEarlyExpressionPositioning(context.report, reportInstance, *target);
        }
        if (hasRehearsalStyle) {
            assign(&TextExpressionTarget::hideMeasureNum, "hideMeasureNum", bool(flags & 0x8000U), 5);
            assign(&TextExpressionTarget::matchPlayback, "matchPlayback", bool(flags & 0x4000U), 5);
            if (auto style = expressionRehearsalStyle(words[1])) {
                assign(&TextExpressionTarget::rehearsalMarkStyle, "rehearsalMarkStyle", *style, 1);
            }
        }
        if (prime) {
            assign(&TextExpressionTarget::textIdKey, "textIdKey", static_cast<musx::dom::Cmper>(words[0]), 0);
        }
        assignExpressionPlayback<TextExpressionTarget>(assign, words);
        // A Coda definition names its enclosure in the 0x0f00 nibble, and an enclosure record holds
        // its geometry; those bits do not carry the later flags. Coda enclosures are out of scope,
        // so such a definition reports no enclosure.
        const bool codaEnclosure = context.profile.epoch == FormatEpoch::CodaBanner && (flags & 0x0f00U)
                                   && !context.index.getOthers().getArray(records::packTag("De"), cmper).empty();
        if (codaEnclosure) {
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                reportFallbackField(reporting, reporting.instanceKey(reportInstance), "hasEnclosure", Reporting::Origin::OutOfScope, flags & 0x0f00U);
            });
        } else {
            assign(&TextExpressionTarget::hasEnclosure, "hasEnclosure", bool(flags & 0x0800U), 5);
        }
        assign(&TextExpressionTarget::createdByHp, "createdByHp", !codaEnclosure && bool(flags & 0x0100U), 5, codaEnclosure);
        if (hasBreakMmRest) {
            assign(&TextExpressionTarget::breakMmRest, "breakMmRest", bool(flags & 0x0400U), 5);
        }
        if (prime) {
            assignExpressionPositioning<TextExpressionTarget>(*target, assign, words, hasCategory);
            recoverExpressionDescription(context, *source, rows, payload, target, reportInstance, primeExpressionHeaderSize);
        } else {
            synthesizedTexts.push_back([&context, target, row = rows.front(), payload] { synthesizeExpressionText(context, target, row, payload); });
        }
        if (!hasCategory) {
            scheduleExpressionMiscCategory(context, target);
        }
        context.document->getOthers()->add(TextExpressionTarget::XmlNodeName, std::move(target));
    }
    context.pending.defer(DeferredStage::Synthesize, std::move(synthesizedTexts));
}

} // namespace others
} // namespace finale_mus_reader
