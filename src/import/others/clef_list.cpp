// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

#include "import/shared/gframe_records.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using ClefListTarget = musx::dom::others::ClefList;

constexpr records::LegacyTag clefListTag = records::packTag("CE");
constexpr records::LegacyTag clefListClass = 0x007f;

// One clef list item is six words: clef index, Edu position, vertical adjustment, percent,
// horizontal adjustment, and flags. Each item is one incidence, so a fixed-row list stores one
// item per row and a zlib payload concatenates them.
constexpr std::size_t clefItemWords = 6;
constexpr std::size_t clefFlagsWord = 5;

constexpr std::uint16_t clefHiddenBit = 0x0002;
constexpr std::uint16_t clefUnlockVertBit = 0x0004;
constexpr std::uint16_t clefForcedBit = 0x0008;
constexpr std::uint16_t clefAfterBarlineBit = 0x0010;

/// Translates the two clef-display bits. **Unverified:** no list sets both, and hidden is
/// preferred when both are set.
musx::dom::ShowClefMode clefMode(std::uint16_t flags)
{
    if ((flags & clefHiddenBit) != 0) {
        return musx::dom::ShowClefMode::Never;
    }
    if ((flags & clefForcedBit) != 0) {
        return musx::dom::ShowClefMode::Always;
    }
    return musx::dom::ShowClefMode::WhenNeeded;
}

/// @brief One item of a list: its incidence, the stored item, and where its words are.
struct ClefItem
{
    musx::dom::Inci inci{};
    std::span<const std::int16_t> words;
    std::size_t at{};
    /// @brief The converted Edu position of an item stored by EVPU.
    std::optional<musx::dom::Edu> convertedEdu;
};

void addClefItem(const ImportContext& context, const RecordFamilySource& source, std::span<const records::LegacyRow> rows, std::uint16_t partId,
    musx::dom::Cmper cmper, const ClefItem& item)
{
    auto target = createOthersRecordTarget<ClefListTarget>(context.document, source, rows.front(), cmper, item.inci);
    if (!target) {
        return;
    }
    const auto flags = static_cast<std::uint16_t>(item.words[clefFlagsWord]);
    target->clefIndex = static_cast<musx::dom::ClefIndex>(item.words[0]);
    target->xEduPos = item.convertedEdu.value_or(item.words[1]);
    target->yEvpuPos = item.words[2];
    target->percent = item.words[3];
    target->xEvpuOffset = item.words[4];
    target->clefMode = clefMode(flags);
    target->unlockVert = (flags & clefUnlockVertBit) != 0;
    target->afterBarline = (flags & clefAfterBarlineBit) != 0;

    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<ClefListTarget>(partId, cmper, item.inci);
        const auto reportField = [&](const char* member, std::size_t word, bool adjusted = false) {
            const auto& row = source.rowOfWord(rows, item.at + word);
            reportLegacyField(reporting, key, source, row, member, source.byteOffsetInRow((item.at + word) * sizeof(std::uint16_t)),
                word == clefFlagsWord ? flags : item.words[word], adjusted ? Reporting::Origin::LegacyMusAdjusted : Reporting::Origin::LegacyMus);
        };
        reportField("clefIndex", 0);
        reportField("xEduPos", 1, item.convertedEdu.has_value());
        reportField("yEvpuPos", 2);
        reportField("percent", 3);
        reportField("xEvpuOffset", 4);
        reportField("clefMode", clefFlagsWord);
        reportField("unlockVert", clefFlagsWord);
        reportField("afterBarline", clefFlagsWord);
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
    });
    context.document->getOthers()->add(ClefListTarget::XmlNodeName, std::move(target));
}

void importClefListFamily(const ImportContext& context, const RecordFamilySource& source, std::uint16_t partId, musx::dom::Cmper cmper)
{
    const auto rows = source.pool->getArray(source.identity, cmper, 0, partId);
    if (rows.empty()) {
        return;
    }
    const auto words = collectRecordWords(source, rows, context.profile.byteOrder);
    if (words.size() % clefItemWords != 0) {
        context.report.diagnostics.push_back(
            {musx::util::Logger::LogLevel::Info, "Clef list " + std::to_string(cmper) + " has an incomplete trailing item."});
    }
    for (std::size_t at = 0; at + clefItemWords <= words.size(); at += clefItemWords) {
        addClefItem(context, source, rows, partId, cmper,
            {static_cast<musx::dom::Inci>(at / clefItemWords), std::span<const std::int16_t>(words.data() + at, clefItemWords), at, std::nullopt});
    }
}

/// @brief Adds the barline clef that precedes a converted Coda-banner list. Only its clef is known;
/// every other member takes the value the upgrade gives a barline item.
void addCodaBarlineClef(const ImportContext& context, const RecordFamilySource& source, std::span<const records::LegacyRow> rows,
    musx::dom::Cmper cmper, musx::dom::ClefIndex clef, const RecordFamilySource& frames, const records::LegacyRow& frame)
{
    auto target = createOthersRecordTarget<ClefListTarget>(context.document, source, rows.front(), cmper, 0);
    if (!target) {
        return;
    }
    target->clefIndex = clef;
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<ClefListTarget>(musx::dom::SCORE_PARTID, cmper, musx::dom::Inci{0});
        reportLegacyField(reporting, key, frames, frame, "clefIndex", details::gframe::codaClefSlot * sizeof(std::uint16_t), clef,
            Reporting::Origin::LegacyMusAdjusted);
        for (const auto* member : {"xEduPos", "yEvpuPos", "percent", "xEvpuOffset", "clefMode", "unlockVert", "afterBarline"}) {
            reportFallbackField(reporting, key, member, Reporting::Origin::LegacyBehavior, 0);
        }
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMusAdjusted);
    });
    context.document->getOthers()->add(ClefListTarget::XmlNodeName, std::move(target));
}

/// @brief Gives a frame whose list converts to no mid-measure clef the list's barline clef as its
/// single clef, and clears its list reference.
void useBarlineClefOnly(const ImportContext& context, const RecordFamilySource& source, std::span<const records::LegacyRow> rows, std::size_t itemAt,
    musx::dom::ClefIndex clef, musx::dom::Cmper staffId, musx::dom::Cmper measure)
{
    using HoldTarget = musx::dom::details::GFrameHold;
    const auto hold = context.document->getDetails()->get<HoldTarget>(musx::dom::SCORE_PARTID, staffId, measure);
    if (!hold) {
        return;
    }
    auto* target = const_cast<HoldTarget*>(hold.get());
    target->clefId = clef;
    target->clefListId = 0;
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<HoldTarget>(musx::dom::SCORE_PARTID, staffId, std::nullopt, measure);
        reportLegacyField(reporting, key, source, source.rowOfWord(rows, itemAt), "clefId", source.byteOffsetInRow(itemAt * sizeof(std::uint16_t)),
            clef, Reporting::Origin::LegacyMusAdjusted);
        reportFallbackField(reporting, key, "clefListId", Reporting::Origin::LegacyBehavior, 0);
    });
}

// A Coda-banner list holds only the mid-measure clefs of the one frame that names it, each placed
// by its EVPU offset from the measure's left edge. It is converted the way Finale's own upgrade
// converts it: the clef in effect at the barline becomes incidence 0, and each stored clef follows
// at the Edu its offset converts to through the measure's spacing. A clef that converts to the
// barline replaces the barline clef, and a list left with no mid-measure clef is not created: the
// frame takes that barline clef as its single clef instead. A list no frame names is not created
// either. Believed: the conversion is exact in a measure without a beat chart and approximate in
// one with a beat chart.
void importCodaClefLists(const ImportContext& context, const RecordFamilySource& source)
{
    const RecordFamilySource frames{&context.index.getDetails(), details::gframe::tag, false, true};
    std::set<musx::dom::Cmper> converted;
    for (const auto staffId : frames.pool->cmpersForTag(frames.identity)) {
        // The clef in effect starts at the staff's default and follows each frame in measure order:
        // a single-clef frame's clef, or the last clef of a frame's list.
        const auto staff = context.document->getOthers()->get<musx::dom::others::Staff>(musx::dom::SCORE_PARTID, staffId);
        auto inEffect = staff ? staff->defaultClef : musx::dom::ClefIndex{};
        for (const auto meas : frames.pool->secondCmpersForTag(frames.identity, staffId)) {
            const auto frameRows = frames.pool->getArray(frames.identity, staffId, meas);
            if (frameRows.empty()) {
                continue;
            }
            const auto& frame = frameRows.front();
            const auto clefWord = static_cast<std::uint16_t>(frame.words[details::gframe::codaClefSlot]);
            if ((static_cast<std::uint16_t>(frame.words[details::gframe::earlyFlagsSlot]) & details::gframe::clefListBit) == 0) {
                inEffect = static_cast<musx::dom::ClefIndex>(clefWord);
                continue;
            }
            const auto listId = static_cast<musx::dom::Cmper>(clefWord);
            const auto rows = source.pool->getArray(source.identity, listId, 0);
            const auto words = collectRecordWords(source, rows, context.profile.byteOrder);
            if (words.size() < clefItemWords) {
                continue;
            }
            auto barlineClef = inEffect;
            inEffect = static_cast<musx::dom::ClefIndex>(words[words.size() / clefItemWords * clefItemWords - clefItemWords]);
            const auto measure = context.document->getOthers()->get<musx::dom::others::Measure>(musx::dom::SCORE_PARTID, meas);
            if (!measure || !converted.insert(listId).second) {
                context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
                    "Clef list " + std::to_string(listId) + " for staff " + std::to_string(staffId) + ", measure " + std::to_string(meas)
                        + (measure ? " is named by more than one frame." : " has no measure.")});
                continue;
            }
            if (words.size() % clefItemWords != 0) {
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Info, "Clef list " + std::to_string(listId) + " has an incomplete trailing item."});
            }
            std::vector<ClefItem> items;
            std::optional<std::size_t> barlineItemAt;
            for (std::size_t at = 0; at + clefItemWords <= words.size(); at += clefItemWords) {
                const std::span<const std::int16_t> item(words.data() + at, clefItemWords);
                const auto edu =
                    static_cast<musx::dom::Edu>(std::lround(measure->calcEduFromEvpu(item[1], beatChartJustification(context, *measure), true)));
                if (edu <= 0) {
                    barlineClef = static_cast<musx::dom::ClefIndex>(item[0]);
                    barlineItemAt = at;
                    continue;
                }
                items.push_back({static_cast<musx::dom::Inci>(items.size() + 1), item, at, edu});
            }
            if (items.empty()) {
                if (barlineItemAt) {
                    useBarlineClefOnly(context, source, rows, *barlineItemAt, barlineClef, staffId, meas);
                }
                continue;
            }
            addCodaBarlineClef(context, source, rows, listId, barlineClef, frames, frame);
            for (const auto& item : items) {
                addClefItem(context, source, rows, musx::dom::SCORE_PARTID, listId, item);
            }
        }
    }
}

} // namespace

void importClefLists(const ImportContext& context)
{
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), clefListTag, clefListClass);
    if (!source) {
        return;
    }
    if (context.profile.epoch == FormatEpoch::CodaBanner) {
        // Measures, beat charts, and staves finish before a Coda-banner list is converted.
        context.pending.defer(DeferredStage::CompletePools, [&context, source = *source] { importCodaClefLists(context, source); });
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        importClefListFamily(context, *source, partId, cmper);
    }
}

} // namespace others
} // namespace finale_mus_reader
