// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "import/shared/coda_text_records.h"
#include "import/shared/page_text_records.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using Target = musx::dom::others::TextBlock;

constexpr auto textBlockTag = records::packTag("TX");
constexpr auto codaTextStyleTag = records::packTag("HS");
constexpr records::LegacyTag textBlockClass = 0x00b7;
constexpr std::size_t textBlockWordCount = 12;
constexpr std::size_t textTypeWord = 12;

// The optional trailing word identifies the texts-pool family. Both encodings carry the same
// structural meaning, so the stored value is a more reliable selector than a
// document-version boundary. A zero or absent word predates the discriminator and retains the
// musxdom Block default.
std::optional<Target::TextType> textTypeFromLegacy(std::int16_t value)
{
    switch (static_cast<std::uint16_t>(value)) {
    case records::packTag("bl"):
    case 2004: return Target::TextType::Block;
    case records::packTag("xp"):
    case 2006: return Target::TextType::Expression;
    default: return std::nullopt;
    }
}

std::int32_t highFirstLong(std::int16_t high, std::int16_t low)
{
    return static_cast<std::int32_t>((static_cast<std::uint32_t>(static_cast<std::uint16_t>(high)) << 16U) | static_cast<std::uint16_t>(low));
}

void reportValue(const ImportContext& context, std::uint16_t partId, musx::dom::Cmper cmper, const char* field, std::int64_t rawValue,
    const records::LegacyRow& row)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        reporting.report().setField(reporting.template instanceKey<Target>(partId, cmper), field,
            {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset, rawValue});
    });
}

void reportBehavior(const ImportContext& context, std::uint16_t partId, musx::dom::Cmper cmper, const char* field, std::int64_t value)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        reporting.report().setField(reporting.template instanceKey<Target>(partId, cmper), field, {Reporting::Origin::LegacyBehavior, 0, 0, value});
    });
}

void applyLegacyTextBlockCorners(const ImportContext& context, std::uint16_t partId, musx::dom::Cmper cmper, Target& target)
{
    // Legacy MUS TextBlocks predate rounded frames and therefore use square corners with a
    // zero radius.
    target.roundCorners = false;
    target.cornerRadius = 0;
    reportBehavior(context, partId, cmper, "roundCorners", 0);
    reportBehavior(context, partId, cmper, "cornerRadius", 0);
}

void reportTextBlockFields(const ImportContext& context, const RecordFamilySource& source, std::span<const records::LegacyRow> rows,
    const std::vector<std::int16_t>& words, std::uint16_t partId, musx::dom::Cmper cmper, std::uint16_t flags, bool hasPercentageLineSpacing,
    bool upgradesZeroPercentToEvpu)
{
    withReporting(context.report, [&](auto&) {
        constexpr const char* directNames[] = {"textId", "width", "height", "shapeId"};
        for (std::size_t slot = 0; slot < std::size(directNames); ++slot) {
            reportValue(context, partId, cmper, directNames[slot], words[slot], source.rowOfWord(rows, slot));
        }
        reportValue(
            context, partId, cmper, hasPercentageLineSpacing ? "lineSpacingPercentage" : "lineSpacingEvpu", words[4], source.rowOfWord(rows, 4));
        if (upgradesZeroPercentToEvpu) {
            reportBehavior(context, partId, cmper, "lineSpacingEvpu", 0);
        }
        reportValue(context, partId, cmper, "xAdd", words[5], source.rowOfWord(rows, 5));
        reportValue(context, partId, cmper, "yAdd", words[6], source.rowOfWord(rows, 6));
        constexpr const char* flagNames[] = {"justify", "newPos36", "showShape", "noExpandSingleWord", "wordWrap"};
        const std::int64_t flagValues[] = {flags & 0x0007U, (flags >> 3U) & 1U, (flags >> 9U) & 1U, (flags >> 10U) & 1U, (flags >> 11U) & 1U};
        for (std::size_t index = 0; index < std::size(flagNames); ++index) {
            reportValue(context, partId, cmper, flagNames[index], flagValues[index], source.rowOfWord(rows, 7));
        }
        reportValue(context, partId, cmper, "inset", highFirstLong(words[8], words[9]), source.rowOfWord(rows, 8));
        reportValue(context, partId, cmper, "stdLineThickness", highFirstLong(words[10], words[11]), source.rowOfWord(rows, 10));
    });
}

void populateStoredTextBlock(
    const ImportContext& context, musx::dom::Cmper cmper, std::span<const records::LegacyRow> rows, const RecordFamilySource& source)
{
    const auto words = collectRecordWords(source, rows, context.profile.byteOrder);
    if (words.size() < textBlockWordCount) {
        context.report.diagnostics.push_back(
            {musx::util::Logger::LogLevel::Info, "Text block " + std::to_string(cmper) + " has an incomplete record."});
        return;
    }

    const auto partId = rows.front().partId;
    auto target = createOthersRecordTarget<Target>(context.document, source, rows.front(), cmper);
    if (!target) {
        return;
    }
    target->textId = static_cast<musx::dom::Cmper>(words[0]);
    target->width = words[1];
    target->height = words[2];
    target->shapeId = static_cast<musx::dom::Cmper>(words[3]);
    const auto flags = static_cast<std::uint16_t>(words[7]);
    const bool hasPercentageLineSpacing = (flags & 0x1000U) != 0;
    const bool upgradesZeroPercentToEvpu = hasPercentageLineSpacing && words[4] == 0;
    if (upgradesZeroPercentToEvpu) {
        target->lineSpacingEvpu = 0;
    } else if (hasPercentageLineSpacing) {
        target->lineSpacingPercentage = words[4];
    } else {
        target->lineSpacingEvpu = words[4];
    }
    target->xAdd = words[5];
    target->yAdd = words[6];
    target->justify = static_cast<Target::TextJustify>(legacyCenterOppositeOrder(flags & 0x0007U));
    target->newPos36 = (flags & 0x0008U) != 0;
    target->showShape = (flags & 0x0200U) != 0;
    target->noExpandSingleWord = (flags & 0x0400U) != 0;
    target->wordWrap = (flags & 0x0800U) != 0;
    target->inset = highFirstLong(words[8], words[9]);
    target->stdLineThickness = highFirstLong(words[10], words[11]);
    applyLegacyTextBlockCorners(context, partId, cmper, *target);
    if (words.size() > textTypeWord) {
        if (const auto textType = textTypeFromLegacy(words[textTypeWord])) {
            target->textType = *textType;
            withReporting(context.report,
                [&](auto&) { reportValue(context, partId, cmper, "textType", words[textTypeWord], source.rowOfWord(rows, textTypeWord)); });
        } else if (words[textTypeWord] != 0) {
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Info, "Text block " + std::to_string(cmper) + " has an unrecognized text-family discriminator "
                                                         + std::to_string(words[textTypeWord]) + "."});
        }
    }

    reportTextBlockFields(context, source, rows, words, partId, cmper, flags, hasPercentageLineSpacing, upgradesZeroPercentToEvpu);
    context.document->getOthers()->add(Target::XmlNodeName, std::move(target));
}

void importStoredTextBlocks(const ImportContext& context)
{
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), textBlockTag, textBlockClass);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        populateStoredTextBlock(context, cmper, source->pool->getArray(source->identity, cmper, 0, partId), *source);
    }
}

void importLegacyPageTextBlocks(const ImportContext& context)
{
    const auto& pool = context.index.getOthers();
    const bool separateBlockIds = hasEarlyConnectedTextAssignments(context);
    if (separateBlockIds) {
        // Connector blocks keep the source's comparators, so they exist before any block is numbered.
        for (const auto cmper : pool.cmpersForTag(earlyTextBlockTag)) {
            const auto* connector = pool.get(earlyTextBlockTag, cmper, 0, 0);
            // The globals record shares the connector tag but names no block.
            if (cmper != musx::dom::MUSX_GLOBALS_CMPER && connector && connector->wordCount > 0 && connector->words[0] > 0) {
                resolveEarlyTextBlock(context, musx::dom::SCORE_PARTID, cmper);
            }
        }
    }
    for (const auto cmper : context.index.getOthers().cmpersForTag(codaTextStyleTag)) {
        const auto textRows = pool.getArray(records::packTag("HT"), cmper);
        for (const auto& row : pool.getArray(codaTextStyleTag, cmper)) {
            const auto key = std::pair{musx::dom::Cmper(cmper), static_cast<musx::dom::Inci>(row.inci)};
            const auto found = context.pending.codaTextBlockByStyle.find(key);
            if (found == context.pending.codaTextBlockByStyle.end()) {
                continue;
            }
            if (context.profile.epoch != FormatEpoch::CodaBanner && coda_text::readBlockCharacters(pool, textRows, row.inci).empty()) {
                continue;
            }
            const auto textId = found->second;
            auto blockId = textId;
            if (separateBlockIds) {
                const auto freeId = context.document->getOthers()->nextFreeCmper<Target>(musx::dom::SCORE_PARTID);
                if (!freeId) {
                    context.report.diagnostics.push_back(
                        {musx::util::Logger::LogLevel::Info, "A legacy page text block exceeds the available comparators."});
                    continue;
                }
                blockId = *freeId;
            }
            if (context.document->getOthers()->get<Target>(musx::dom::SCORE_PARTID, blockId)) {
                continue;
            }
            auto target = std::make_shared<Target>(context.document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, blockId);
            target->textId = textId;
            target->lineSpacingPercentage = 100;
            const auto flags = static_cast<std::uint16_t>(row.words[5]);
            target->justify = static_cast<Target::TextJustify>(legacyCenterOppositeOrder(flags & 0x0003U));
            target->showShape = context.profile.epoch != FormatEpoch::CodaBanner;
            target->wordWrap = true;
            applyLegacyTextBlockCorners(context, musx::dom::SCORE_PARTID, blockId, *target);

            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto instance = reporting.template instanceKey<Target>(musx::dom::SCORE_PARTID, blockId);
                reporting.report().setInstanceOrigin(instance, Reporting::Origin::LegacyBehavior);
                if (context.profile.epoch != FormatEpoch::CodaBanner) {
                    reporting.report().setField(
                        instance, "textId", {Reporting::Origin::LegacyMusAdjusted, row.blockOffset, row.decodedOffset, textId});
                }
            });
            if (context.profile.epoch == FormatEpoch::CodaBanner) {
                reportValue(context, musx::dom::SCORE_PARTID, blockId, "textId", textId, row);
            }
            reportValue(context, musx::dom::SCORE_PARTID, blockId, "justify", flags & 0x0003U, row);
            reportBehavior(context, musx::dom::SCORE_PARTID, blockId, "lineSpacingPercentage", 100);
            reportBehavior(context, musx::dom::SCORE_PARTID, blockId, "showShape", target->showShape);
            reportBehavior(context, musx::dom::SCORE_PARTID, blockId, "wordWrap", 1);
            if (context.profile.epoch == FormatEpoch::CodaBanner) {
                reportBehavior(context, musx::dom::SCORE_PARTID, blockId, "shapeId", 0);
                reportBehavior(context, musx::dom::SCORE_PARTID, blockId, "newPos36", 0);
                reportBehavior(context, musx::dom::SCORE_PARTID, blockId, "noExpandSingleWord", 0);
            }
            context.document->getOthers()->add(Target::XmlNodeName, std::move(target));
            found->second = blockId;
        }
    }
}

} // namespace

void importTextBlocks(const ImportContext& context)
{
    if (context.profile.epoch != FormatEpoch::CodaBanner) {
        importStoredTextBlocks(context);
    }
    if (hasLegacyPageTextStyle(context)) {
        // HS/HT text is complete after the text importer, regardless of which early container
        // stores it. Materialization keeps TextBlock allocation ahead of assignment resolution.
        context.pending.defer(DeferredStage::CompletePools, [&context] { importLegacyPageTextBlocks(context); });
    }
}

} // namespace others
} // namespace finale_mus_reader
