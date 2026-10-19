// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <tuple>
#include <utility>

#include "import/shared/coda_text_records.h"
#include "import/shared/page_text_records.h"
#include "import/support/legacy_mapping.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using PageTextTarget = musx::dom::others::PageTextAssign;
constexpr records::LegacyTag pageTextClass = 0x00c2;
constexpr auto codaPageTextTag = records::packTag("HS");
constexpr auto codaPageTextCharactersTag = records::packTag("HT");
constexpr std::uint16_t exceptPageOneFlag = 0x4000U;
constexpr std::size_t pageTextWordsPerAssignment = 12;

bool applyPageTextFlags(PageTextTarget& target, std::uint16_t flags)
{
    if ((flags & 0x0003U) == 3 || ((flags >> 2U) & 3U) == 3 || ((flags >> 4U) & 3U) == 3 || ((flags >> 8U) & 3U) == 3) {
        return false;
    }
    const auto parity = flags & 3U;
    target.oddEven = parity == 1   ? PageTextTarget::PageAssignType::Odd
                     : parity == 2 ? PageTextTarget::PageAssignType::Even
                                   : PageTextTarget::PageAssignType::AllPages;
    target.hPosLp = static_cast<PageTextTarget::HorizontalAlignment>((flags >> 2U) & 3U);
    target.hPosRp = static_cast<PageTextTarget::HorizontalAlignment>((flags >> 4U) & 3U);
    target.hPosPageEdge = (flags & 0x0040U) != 0;
    target.hidden = (flags & 0x0080U) != 0;
    const auto vertical = (flags >> 8U) & 3U;
    target.vPos = vertical == 1   ? PageTextTarget::VerticalAlignment::Bottom
                  : vertical == 2 ? PageTextTarget::VerticalAlignment::Center
                                  : PageTextTarget::VerticalAlignment::Top;
    target.vPosPageEdge = (flags & 0x0400U) != 0;
    target.indRpPos = (flags & 0x0800U) != 0;
    return true;
}

using PageIncidenceByPart = std::map<std::pair<musx::dom::Cmper, musx::dom::Cmper>, EmittedIncidence>;

PageIncidenceByPart importEarlyPageTextRecords(const ImportContext& context)
{
    PageIncidenceByPart nextIncidence;
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), pageTextTag, pageTextClass);
    if (!source) {
        return nextIncidence;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        for (const auto& row : source->pool->getArray(source->identity, cmper, 0, partId)) {
            if (row.wordCount < 6) {
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Info, "Early page text assignment " + std::to_string(cmper) + " has an incomplete row."});
                continue;
            }
            if (row.words[0] == 0) {
                continue;
            }
            const auto flags = static_cast<std::uint16_t>(row.words[5]);
            const auto key = std::pair{musx::dom::Cmper(partId), musx::dom::Cmper(cmper)};
            const auto inci = nextIncidence[key].current();
            auto target = createOthersRecordTarget<PageTextTarget>(context.document, *source, row, cmper, inci);
            if (!target || !applyPageTextFlags(*target, flags)) {
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Info, "Early page text assignment " + std::to_string(cmper) + " has an unknown alignment flag."});
                continue;
            }
            target->block = static_cast<musx::dom::Cmper>(row.words[0]);
            target->xDisp = row.words[1];
            target->yDisp = row.words[2];
            target->startPage = row.words[3] == 0 ? musx::dom::PageCmper(cmper == 0 ? 1 : cmper) : static_cast<musx::dom::PageCmper>(row.words[3]);
            target->endPage = row.words[4] == 0 ? musx::dom::PageCmper(cmper) : static_cast<musx::dom::PageCmper>(row.words[4]);
            resolveEarlyTextBlock(context, partId, target->block);
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<PageTextTarget>(partId, cmper, inci);
                reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
                for (const auto& [member, slot, value] : {std::tuple{"block", 0, std::int64_t(target->block)},
                         {"xDisp", 1, std::int64_t(target->xDisp)}, {"yDisp", 2, std::int64_t(target->yDisp)},
                         {"oddEven", 5, std::int64_t(target->oddEven)}, {"hPosLp", 5, std::int64_t(target->hPosLp)},
                         {"hPosRp", 5, std::int64_t(target->hPosRp)}, {"hidden", 5, std::int64_t(target->hidden)},
                         {"vPos", 5, std::int64_t(target->vPos)}, {"hPosPageEdge", 5, std::int64_t(target->hPosPageEdge)},
                         {"vPosPageEdge", 5, std::int64_t(target->vPosPageEdge)}, {"indRpPos", 5, std::int64_t(target->indRpPos)}}) {
                    reportLegacyField(reporting, key, *source, row, member, source->byteOffsetInRow(slot * 2), value);
                }
                if (row.words[3] == 0) {
                    reportFallbackField(reporting, key, "startPage", Reporting::Origin::LegacyBehavior, target->startPage);
                } else {
                    reportLegacyField(reporting, key, *source, row, "startPage", source->byteOffsetInRow(6), target->startPage);
                }
                if (row.words[4] == 0) {
                    reportFallbackField(reporting, key, "endPage", Reporting::Origin::LegacyBehavior, target->endPage);
                } else {
                    reportLegacyField(reporting, key, *source, row, "endPage", source->byteOffsetInRow(8), target->endPage);
                }
                reportFallbackField(reporting, key, "rightPgXDisp", Reporting::Origin::LegacyBehavior, target->rightPgXDisp);
                reportFallbackField(reporting, key, "rightPgYDisp", Reporting::Origin::LegacyBehavior, target->rightPgYDisp);
            });
            nextIncidence[key].add(*context.document->getOthers(), std::move(target));
        }
    }
    return nextIncidence;
}

void importLegacyPageTexts(const ImportContext& context, PageIncidenceByPart nextIncidence)
{
    const RecordFamilySource source{&context.index.getOthers(), codaPageTextTag, false, false};
    for (const auto cmper : source.pool->cmpersForTag(codaPageTextTag)) {
        const auto textRows = source.pool->getArray(codaPageTextCharactersTag, cmper);
        for (const auto& row : source.pool->getArray(codaPageTextTag, cmper)) {
            if (row.wordCount < 6) {
                continue;
            }
            // An HS style can outlive its deleted HT text. Later saves omit that empty
            // assignment and renumber the following page-text incidences.
            if (coda_text::readBlockCharacters(*source.pool, textRows, row.inci).empty()) {
                continue;
            }
            const auto sourceInci = static_cast<musx::dom::Inci>(row.inci);
            const auto found = context.pending.codaTextBlockByStyle.find({cmper, sourceInci});
            if (found == context.pending.codaTextBlockByStyle.end()) {
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Info, "Page text assignment " + std::to_string(cmper) + " has no paired TextBlock."});
                continue;
            }
            const auto blockId = found->second;
            const auto key = std::pair{musx::dom::Cmper(musx::dom::SCORE_PARTID), musx::dom::Cmper(cmper)};
            const auto inci = nextIncidence[key].current();
            auto target = createOthersRecordTarget<PageTextTarget>(context.document, source, row, cmper, inci);
            target->block = blockId;
            target->xDisp = row.words[0];
            const auto flags = static_cast<std::uint16_t>(row.words[5]);
            const auto textSize = coda_text::styleFontSize(row, context.profile.epoch).size;
            const auto lowPlacement = context.profile.epoch != FormatEpoch::CodaBanner && (flags & 0x00c0U) == 0;
            // Believed: the stored vertical word is a handle offset. Font metrics vary, so
            // these size-based conversions approximate the upgraded placement.
            if (lowPlacement) {
                const auto fontNumber = static_cast<std::uint16_t>(row.words[2]) & 0x00ffU;
                target->yDisp = row.words[1] - 3 * fontNumber;
            } else {
                target->yDisp = (flags & 0x0040U) != 0 ? row.words[1] - textSize : 3 * textSize - row.words[1];
            }
            target->startPage = cmper == 0 ? musx::dom::PageCmper((flags & exceptPageOneFlag) != 0 ? 2 : 1) : musx::dom::PageCmper(cmper);
            target->endPage = static_cast<musx::dom::PageCmper>(cmper);
            if ((flags & 0x3000U) == 0x1000U) {
                target->oddEven = PageTextTarget::PageAssignType::Even;
            } else if ((flags & 0x3000U) == 0x2000U) {
                target->oddEven = PageTextTarget::PageAssignType::Odd;
            }
            const auto alignment = flags & 3U;
            if (alignment > 2) {
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Info, "Page text assignment " + std::to_string(cmper) + " has an unknown horizontal alignment."});
                continue;
            }
            target->hPosLp = static_cast<PageTextTarget::HorizontalAlignment>(alignment);
            target->hPosRp = target->hPosLp;
            target->vPos =
                (flags & 0x0040U) != 0 || lowPlacement ? PageTextTarget::VerticalAlignment::Bottom : PageTextTarget::VerticalAlignment::Top;
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<PageTextTarget>(musx::dom::SCORE_PARTID, cmper, inci);
                reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
                const auto stored = [&](const char* member, std::size_t slot, std::int64_t value) {
                    reportLegacyField(reporting, key, source, row, member, source.byteOffsetInRow(slot * 2), value);
                };
                const auto behavior = [&](const char* member, std::int64_t value) {
                    reportFallbackField(reporting, key, member, Reporting::Origin::LegacyBehavior, value);
                };
                const auto unmapped = [&](const char* member, std::int64_t value) {
                    reportFallbackField(reporting, key, member, Reporting::Origin::Unmapped, value);
                };
                reporting.report().setField(key, "block", {Reporting::Origin::LegacyMusAdjusted, row.blockOffset, row.decodedOffset, sourceInci});
                stored("xDisp", 0, row.words[0]);
                reportLegacyField(
                    reporting, key, source, row, "yDisp", source.byteOffsetInRow(2), row.words[1], Reporting::Origin::LegacyMusAdjusted);
                behavior("startPage", target->startPage);
                behavior("endPage", target->endPage);
                stored("oddEven", 5, flags);
                stored("hPosLp", 5, flags);
                behavior("hPosRp", static_cast<int>(target->hPosRp));
                unmapped("hidden", target->hidden);
                stored("vPos", 5, flags);
                unmapped("hPosPageEdge", target->hPosPageEdge);
                unmapped("vPosPageEdge", target->vPosPageEdge);
                behavior("indRpPos", target->indRpPos);
                unmapped("rightPgXDisp", target->rightPgXDisp);
                unmapped("rightPgYDisp", target->rightPgYDisp);
            });
            nextIncidence[key].add(*context.document->getOthers(), std::move(target));
        }
    }
}

void reportPageText(const ImportContext& context, const RecordFamilySource& source, std::span<const records::LegacyRow> rows,
    const PageTextTarget& target, std::size_t wordOffset)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<PageTextTarget>(target.getSourcePartId(), target.getCmper(), target.getInci().value_or(0));
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        const auto field = [&](const char* name, std::size_t slot, std::int64_t value) {
            const auto physicalSlot = wordOffset + slot;
            reportLegacyField(reporting, key, source, source.rowOfWord(rows, physicalSlot), name, source.byteOffsetInRow(physicalSlot * 2), value);
        };
        field("block", 0, target.block);
        field("xDisp", 1, target.xDisp);
        field("yDisp", 2, target.yDisp);
        field("startPage", 3, target.startPage);
        field("endPage", 4, target.endPage);
        field("oddEven", 5, static_cast<int>(target.oddEven));
        field("hPosLp", 5, static_cast<int>(target.hPosLp));
        field("hPosRp", 5, static_cast<int>(target.hPosRp));
        field("hidden", 5, target.hidden);
        field("vPos", 5, static_cast<int>(target.vPos));
        field("hPosPageEdge", 5, target.hPosPageEdge);
        field("vPosPageEdge", 5, target.vPosPageEdge);
        field("indRpPos", 5, target.indRpPos);
        field("rightPgXDisp", 6, target.rightPgXDisp);
        field("rightPgYDisp", 7, target.rightPgYDisp);
    });
}

void importPageTextRecord(
    const ImportContext& context, const RecordFamilySource& source, std::span<const records::LegacyRow> rows, std::uint16_t cmper)
{
    const auto words = collectRecordWords(source, rows, context.profile.byteOrder);
    if (words.size() % pageTextWordsPerAssignment != 0) {
        context.report.diagnostics.push_back(
            {musx::util::Logger::LogLevel::Info, "Page text assignment " + std::to_string(cmper) + " has an incomplete tuple."});
    }
    EmittedIncidence incidence;
    for (std::size_t at = 0; at + pageTextWordsPerAssignment <= words.size(); at += pageTextWordsPerAssignment) {
        const auto flags = static_cast<std::uint16_t>(words[at + 5]);
        const auto inci = incidence.current();
        auto target = createOthersRecordTarget<PageTextTarget>(context.document, source, rows.front(), cmper, inci);
        if (!target || !applyPageTextFlags(*target, flags)) {
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Info, "Page text assignment " + std::to_string(cmper) + " has an unknown alignment flag."});
            continue;
        }
        target->block = static_cast<musx::dom::Cmper>(words[at]);
        target->xDisp = words[at + 1];
        target->yDisp = words[at + 2];
        target->startPage = static_cast<musx::dom::PageCmper>(words[at + 3]);
        target->endPage = static_cast<musx::dom::PageCmper>(words[at + 4]);
        target->rightPgXDisp = words[at + 6];
        target->rightPgYDisp = words[at + 7];
        reportPageText(context, source, rows, *target, at);
        incidence.add(*context.document->getOthers(), std::move(target));
    }
}

} // namespace

void importPageTextAssigns(const ImportContext& context)
{
    if (!sourceAtOrAfter(context.profile, FormatEpoch::UncompressedLegacy, versions::finale3_7)) {
        const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), pageTextTag, pageTextClass);
        if ((source && !recordKeys(*source).empty()) || hasLegacyPageTextStyle(context)) {
            // Both text stores finish before the assignments resolve their block references.
            context.pending.defer(DeferredStage::ClaimStoredIds, [&context] {
                auto nextIncidence = importEarlyPageTextRecords(context);
                if (hasLegacyPageTextStyle(context)) {
                    importLegacyPageTexts(context, std::move(nextIncidence));
                }
            });
        }
        return;
    }

    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), pageTextTag, pageTextClass);
    if (source) {
        const auto keys = recordKeys(*source);
        if (!keys.empty()) {
            for (const auto& [partId, cmper] : keys) {
                const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
                if (!rows.empty()) {
                    importPageTextRecord(context, *source, rows, cmper);
                }
            }
            return;
        }
    }
}

} // namespace others
} // namespace finale_mus_reader
