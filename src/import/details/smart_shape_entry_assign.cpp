// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/details.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "import/shared/skipped_smart_shapes.h"
#include "musx/musx.h"

namespace finale_mus_reader::details {
namespace {

using Target = musx::dom::details::SmartShapeEntryAssign;
constexpr auto assignmentTag = records::packTag("Ex");
constexpr records::LegacyTag assignmentClass = 0x0428;
// Before Finale 2011, a SmartShape attached to a lyric records its entry in a separate lyric-mark
// family with the same five-word tuple. Each of its tuples is an entry assignment that follows
// the entry's ordinary ones. A document stores lyric marks only until they are folded into the
// ordinary family, so the two never describe the same mark twice.
constexpr auto lyricMarkTag = records::packTag("Lx");
constexpr records::LegacyTag lyricMarkClass = 0x0429;
constexpr std::size_t wordsPerAssignment = 5;

void importSource(
    const ImportContext& context, const RecordFamilySource& source, const SmartShapeKeys& skipped, std::set<musx::dom::EntryNumber>* markedEntries)
{
    for (const auto& [partId, entryHigh] : recordKeys(source)) {
        for (const auto entryLow : source.pool->secondCmpersForTag(source.identity, entryHigh, partId)) {
            const auto rows = source.pool->getArray(source.identity, entryHigh, entryLow, partId);
            const auto words = collectRecordWords(source, rows, context.profile.byteOrder);
            if (words.size() % wordsPerAssignment != 0) {
                context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info, "SmartShape entry assignment has an incomplete tuple."});
            }
            const auto entryNumber = static_cast<musx::dom::EntryNumber>((static_cast<std::uint32_t>(entryHigh) << 16U) | entryLow);
            auto inci = static_cast<musx::dom::Inci>(context.document->getDetails()->getArray<Target>(partId, entryNumber).size());
            for (std::size_t at = 0; at + wordsPerAssignment <= words.size(); at += wordsPerAssignment) {
                // An assignment of a shape the reader skips would name a shape that does not exist.
                if (namesSkippedSmartShape(skipped, partId, static_cast<std::uint16_t>(words[at]))) {
                    continue;
                }
                if (markedEntries) {
                    markedEntries->insert(entryNumber);
                }
                auto target = createDetailsRecordTarget<Target>(context.document, source, source.rowOfWord(rows, at), entryHigh, entryLow, inci);
                target->shapeNum = static_cast<std::uint16_t>(words[at]);
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                    const auto key = reporting.template instanceKey<Target>(
                        partId, entryHigh, inci, static_cast<musx::dom::Cmper>((static_cast<std::uint32_t>(entryHigh) << 16U) | entryLow));
                    reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
                    reportLegacyField(
                        reporting, key, source, source.rowOfWord(rows, at), "shapeNum", source.byteOffsetInRow(at * 2), target->shapeNum);
                });
                context.document->getDetails()->add(Target::XmlNodeName, std::move(target));
                ++inci;
            }
        }
    }
}

// Before this release, Finale's conversion sets the SmartShape flag of every entry with an
// assignment, whatever the stored bit says.
constexpr VersionBound enablerBitsCheckedBefore{13, 0, 1, 2, 66};

/// @brief Sets the SmartShape flag of each entry Finale's conversion marks.
/// @details The conversion sets the flag as it folds each lyric mark into the ordinary family, and
/// on every entry with an assignment when the source predates @ref enablerBitsCheckedBefore.
void markAssignedEntries(const ImportContext& context, const std::set<musx::dom::EntryNumber>& entries)
{
    for (const auto number : entries) {
        const auto entry = context.document->getEntries()->get(number);
        if (!entry || entry->smartShapeDetail) {
            continue;
        }
        const_cast<musx::dom::Entry*>(entry.get())->smartShapeDetail = true;
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            const auto key = reporting.template instanceKey<musx::dom::Entry>(
                musx::dom::SCORE_PARTID, static_cast<musx::dom::Cmper>(number >> 16U), std::nullopt, static_cast<musx::dom::Cmper>(number));
            if (auto* field = reporting.report().findField(key, "smartShapeDetail")) {
                field->origin = Reporting::Origin::LegacyMusAdjusted;
            }
        });
    }
}

} // namespace

void importSmartShapeEntryAssigns(const ImportContext& context)
{
    // Assignments wait for the decision about which stored shapes are recovered.
    auto markedEntries = std::make_shared<std::set<musx::dom::EntryNumber>>();
    context.pending.defer(DeferredStage::ClaimStoredIds, [&context, markedEntries] {
        // Ordinary assignments first, so that lyric marks take the incidences after them.
        const auto& skipped = findSkippedSmartShapes(context);
        const bool checksEnablerBits = sourcePredatesVersion(context.profile, FormatEpoch::ZlibLegacy, enablerBitsCheckedBefore);
        for (const auto& [tag, classId] : {std::pair{assignmentTag, assignmentClass}, std::pair{lyricMarkTag, lyricMarkClass}}) {
            if (const auto source =
                    selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), tag, classId, true)) {
                importSource(context, *source, skipped, tag == lyricMarkTag || checksEnablerBits ? markedEntries.get() : nullptr);
            }
        }
    });
    context.pending.defer(DeferredStage::RefreshDerivedFlags, [&context, markedEntries] { markAssignedEntries(context, *markedEntries); });
}

} // namespace finale_mus_reader::details
