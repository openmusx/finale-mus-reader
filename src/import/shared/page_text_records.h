// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <memory>
#include <string>
#include <utility>

#include "import/support/legacy_mapping.h"
#include "musx/musx.h"

namespace finale_mus_reader::others {

/// @brief The fixed-row page text assignment selector.
inline constexpr auto pageTextTag = records::packTag("pT");
/// @brief The pre-3.7 TextBlock connector, keyed by block comparator.
inline constexpr auto earlyTextBlockTag = records::packTag("PT");
/// @brief The fixed-row measure text assignment selector from Finale 3.7 onward.
inline constexpr auto measureTextTag = records::packTag("mt");
/// @brief The Coda-banner measure text assignment selector.
inline constexpr auto codaMeasureTextTag = records::packTag("MT");

/// @brief Whether a pre-3.7 page-text style selector is present in a source eligible to use it.
inline bool hasLegacyPageTextStyle(const ImportContext& context)
{
    const auto& profile = context.profile;
    if (profile.version && VersionBound{profile.version->major, profile.version->minor, profile.version->maint} >= versions::finale3_7) {
        return false;
    }
    if (!sourcePredatesVersion(profile, FormatEpoch::UncompressedLegacy, versions::finale3_7)) {
        return false;
    }
    return !context.index.getOthers().cmpersForTag(records::packTag("HS")).empty();
}

/// @brief Whether early page or measure text assignments name TextBlocks through `PT` connectors.
/// @details Those block comparators are the source's own, so TextBlocks synthesized for `HS`
/// text must not take them.
inline bool hasEarlyConnectedTextAssignments(const ImportContext& context)
{
    const auto& others = context.index.getOthers();
    const auto& details = context.index.getDetails();
    return !others.cmpersForTag(earlyTextBlockTag).empty()
           && (!others.cmpersForTag(pageTextTag).empty() || !details.cmpersForTag(measureTextTag).empty()
               || !details.cmpersForTag(codaMeasureTextTag).empty());
}

/// @brief Supplies the TextBlock an early assignment names when no stored `TX` record defines it.
/// @details A `PT` row keyed by the block comparator supplies the separate BlockText number in its
/// first word. The rest of the TextBlock takes its early behavior.
inline void resolveEarlyTextBlock(const ImportContext& context, musx::dom::Cmper partId, musx::dom::Cmper blockId)
{
    using TextBlockTarget = musx::dom::others::TextBlock;
    if (context.document->getOthers()->get<TextBlockTarget>(partId, blockId)) {
        return;
    }
    const auto* connector = context.index.getOthers().get(earlyTextBlockTag, blockId, 0, 0, partId);
    if (!connector || connector->wordCount == 0 || connector->words[0] <= 0) {
        context.report.diagnostics.push_back(
            {musx::util::Logger::LogLevel::Info, "Early text block " + std::to_string(blockId) + " has no text connector."});
        return;
    }
    const auto textId = static_cast<musx::dom::Cmper>(connector->words[0]);
    auto block = std::make_shared<TextBlockTarget>(context.document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, blockId);
    block->textId = textId;
    block->lineSpacingPercentage = 100;
    block->showShape = context.profile.epoch != FormatEpoch::CodaBanner;
    block->wordWrap = true;
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<TextBlockTarget>(musx::dom::SCORE_PARTID, blockId);
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyBehavior);
        const RecordFamilySource connectorSource{&context.index.getOthers(), earlyTextBlockTag, false, false};
        reportLegacyField(reporting, key, connectorSource, *connector, "textId", connectorSource.byteOffsetInRow(0), textId);
        reporting.report().setField(key, "lineSpacingPercentage", {Reporting::Origin::LegacyBehavior, 0, 0, 100});
        reporting.report().setField(key, "justify", {Reporting::Origin::LegacyBehavior, 0, 0, 0});
        reporting.report().setField(key, "showShape", {Reporting::Origin::LegacyBehavior, 0, 0, block->showShape});
        reporting.report().setField(key, "wordWrap", {Reporting::Origin::LegacyBehavior, 0, 0, 1});
    });
    context.document->getOthers()->add(TextBlockTarget::XmlNodeName, std::move(block));
}

} // namespace finale_mus_reader::others
