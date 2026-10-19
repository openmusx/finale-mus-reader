// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/details.h"
#include "import/others.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "import/shared/page_text_records.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace details {
namespace {

using MeasureTextTarget = musx::dom::details::MeasureTextAssign;
constexpr records::LegacyTag measureTextClass = 0x0420;
constexpr std::size_t measureTextWordCount = 5;
constexpr std::uint16_t measureTextHiddenFlag = 0x0001U;

using EarlyBlockKeys = std::set<std::pair<musx::dom::Cmper, musx::dom::Cmper>>;

/// @brief A pre-DCL assignment whose stored offsets await the measure's layout and its staff's size.
struct EarlyPlacement
{
    std::shared_ptr<MeasureTextTarget> target;
    const records::LegacyRow* horizontalRow{};
    std::size_t horizontalOffset{};
    std::int16_t horizontal{};
    const records::LegacyRow* verticalRow{};
    std::size_t verticalOffset{};
    std::int16_t vertical{};
};

// Before the DCL epoch the horizontal word is an EVPU offset from the measure's left edge. Measured
// from the first beat instead, a position before it stays an EVPU displacement and any other is
// converted to Edu through the measure's coordinate conversion with the source chart justification.
// A measure the document lacks has no spacing, so its offset is kept as stored.
void placeHorizontal(const ImportContext& context, const RecordFamilySource& source, const EarlyPlacement& placement)
{
    auto& target = *placement.target;
    const auto measure = context.document->getOthers()->get<musx::dom::others::Measure>(musx::dom::SCORE_PARTID, target.getCmper2());
    bool converted = false;
    if (measure) {
        const auto justification = others::beatChartJustification(context, *measure);
        const auto fromFirstBeat = std::lround(placement.horizontal - measure->calcFirstBeatEvpu(justification));
        if (fromFirstBeat < 0) {
            target.xDispEvpu = static_cast<musx::dom::Evpu>(fromFirstBeat);
        } else {
            target.xDispEvpu = 0;
            target.xDispEdu = static_cast<musx::dom::Edu>(std::lround(measure->calcEduFromEvpu(placement.horizontal, justification)));
            converted = true;
        }
    }
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<MeasureTextTarget>(
            musx::dom::SCORE_PARTID, target.getCmper1(), target.getInci().value_or(0), target.getCmper2());
        const auto* stored = converted ? "xDispEdu" : "xDispEvpu";
        const auto* other = converted ? "xDispEvpu" : "xDispEdu";
        reportLegacyField(reporting, key, source, *placement.horizontalRow, stored, placement.horizontalOffset, placement.horizontal,
            Reporting::Origin::LegacyMusAdjusted);
        reportFallbackField(reporting, key, other, Reporting::Origin::LegacyBehavior, 0);
    });
}

// Believed: before the DCL epoch the vertical offset is scaled by the staff's size on the system
// that contains the measure, so the unscaled displacement divides by that size. A system's own
// size does not scale it, and later sources store it unscaled.
void unscaleVertical(const ImportContext& context, const RecordFamilySource& source, const EarlyPlacement& placement)
{
    using StaffSystem = musx::dom::others::StaffSystem;
    if (placement.vertical == 0) {
        return;
    }
    const auto systems = context.document->getOthers()->getArray<StaffSystem>(musx::dom::SCORE_PARTID);
    const auto meas = placement.target->getCmper2();
    // A source saved without page layout has no systems, and its offset is not reduced. musxdom's
    // measure-to-system lookup reads page ranges it resolves only once the document is
    // finished, so the saved systems are searched directly.
    const auto system =
        std::ranges::find_if(systems, [meas](const auto& candidate) { return meas >= candidate->startMeas && meas < candidate->endMeas; });
    if (system == systems.end()) {
        return;
    }
    const auto scaling = (*system)->calcStaffScaling(placement.target->getCmper1());
    if (scaling == 1 || scaling <= 0) {
        return;
    }
    placement.target->yDisp = static_cast<musx::dom::Evpu>(std::lround((musx::util::Fraction(placement.vertical) / scaling).toDouble()));
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<MeasureTextTarget>(
            musx::dom::SCORE_PARTID, placement.target->getCmper1(), placement.target->getInci().value_or(0), meas);
        reportLegacyField(reporting, key, source, *placement.verticalRow, "yDisp", placement.verticalOffset, placement.vertical,
            Reporting::Origin::LegacyMusAdjusted);
    });
}

void reportMeasureText(const ImportContext& context, const RecordFamilySource& source, std::span<const records::LegacyRow> rows,
    const MeasureTextTarget& target, std::size_t at, std::uint16_t partId, bool earlyCoordinates)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key =
            reporting.template instanceKey<MeasureTextTarget>(partId, target.getCmper1(), target.getInci().value_or(0), target.getCmper2());
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        using Origin = typename Reporting::Origin;
        const auto field = [&](const char* name, std::size_t slot, std::int64_t value, Origin origin) {
            const auto physicalSlot = at + slot;
            reportLegacyField(
                reporting, key, source, source.rowOfWord(rows, physicalSlot), name, source.byteOffsetInRow(physicalSlot * 2), value, origin);
        };
        field("block", 0, target.block, Origin::LegacyMus);
        // An early horizontal offset is reported once it is placed.
        if (!earlyCoordinates) {
            field("xDispEdu", 1, target.xDispEdu, Origin::LegacyMus);
            field("xDispEvpu", 1, target.xDispEvpu, Origin::LegacyMus);
        }
        field("yDisp", 2, target.yDisp, Origin::LegacyMus);
        field("hidden", 4, target.hidden, Origin::LegacyMus);
    });
}

void importMeasureTextFamily(
    const ImportContext& context, const RecordFamilySource& source, EarlyBlockKeys& earlyBlocks, std::vector<EarlyPlacement>& placements)
{
    // From the DCL epoch the horizontal word is a positive Edu or negative EVPU displacement from
    // the measure's first beat. Earlier offsets are placed once measures and staff systems exist.
    const bool earlyCoordinates = !sourceAtOrAfter(context.profile, FormatEpoch::DclLegacy);
    const bool earlyConnectors = !context.index.getOthers().cmpersForTag(others::earlyTextBlockTag).empty();
    for (const auto& [partId, staffId] : recordKeys(source)) {
        for (const auto meas : source.pool->secondCmpersForTag(source.identity, staffId, partId)) {
            const auto rows = source.pool->getArray(source.identity, staffId, meas, partId);
            const auto words = collectRecordWords(source, rows, context.profile.byteOrder);
            if (words.size() % measureTextWordCount != 0) {
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Info, "Measure text assignment for staff " + std::to_string(staffId) + ", measure "
                                                             + std::to_string(meas) + " has an incomplete trailing tuple."});
            }
            for (std::size_t at = 0; at + measureTextWordCount <= words.size(); at += measureTextWordCount) {
                const auto inci = static_cast<musx::dom::Inci>(at / measureTextWordCount);
                auto target = createDetailsRecordTarget<MeasureTextTarget>(context.document, source, rows.front(), staffId, meas, inci);
                if (!target) {
                    continue;
                }
                target->block = static_cast<musx::dom::Cmper>(words[at]);
                const auto xDisp = words[at + 1];
                if (earlyCoordinates || xDisp < 0) {
                    target->xDispEvpu = xDisp;
                } else {
                    target->xDispEdu = xDisp;
                }
                target->yDisp = words[at + 2];
                target->hidden = (static_cast<std::uint16_t>(words[at + 4]) & measureTextHiddenFlag) != 0;
                if (earlyConnectors) {
                    earlyBlocks.emplace(partId, target->block);
                }
                reportMeasureText(context, source, rows, *target, at, partId, earlyCoordinates);
                if (earlyCoordinates) {
                    placements.push_back({target, &source.rowOfWord(rows, at + 1), source.byteOffsetInRow((at + 1) * 2), words[at + 1],
                        &source.rowOfWord(rows, at + 2), source.byteOffsetInRow((at + 2) * 2), words[at + 2]});
                }
                context.document->getDetails()->add(MeasureTextTarget::XmlNodeName, std::move(target));
            }
        }
    }
}

} // namespace

void importMeasureTextAssigns(const ImportContext& context)
{
    // Coda-banner sources spell the fixed-row selector differently; neither spelling is
    // version-gated.
    const auto fixedTag =
        context.index.getDetails().cmpersForTag(others::measureTextTag).empty() ? others::codaMeasureTextTag : others::measureTextTag;
    const auto source =
        selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), fixedTag, measureTextClass, true);
    if (!source) {
        return;
    }
    EarlyBlockKeys earlyBlocks;
    std::vector<EarlyPlacement> placements;
    importMeasureTextFamily(context, *source, earlyBlocks, placements);
    if (!placements.empty()) {
        // Measures, beat charts, staff systems, and staff sizes finish before an early offset is
        // placed.
        context.pending.defer(DeferredStage::ResolveStoredRefs, [&context, source = *source, placements = std::move(placements)] {
            for (const auto& placement : placements) {
                placeHorizontal(context, source, placement);
                unscaleVertical(context, source, placement);
            }
        });
    }
    if (!earlyBlocks.empty()) {
        // Stored TextBlocks finish before a block without one resolves its PT connector.
        context.pending.defer(DeferredStage::ClaimStoredIds, [&context, earlyBlocks = std::move(earlyBlocks)] {
            for (const auto& [partId, blockId] : earlyBlocks) {
                others::resolveEarlyTextBlock(context, partId, blockId);
            }
        });
    }
}

} // namespace details
} // namespace finale_mus_reader
