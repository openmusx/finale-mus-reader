// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/details.h"

#include <cstddef>
#include <cstdint>
#include <optional>

#include "musx/musx.h"

namespace finale_mus_reader::details {
namespace {

using Target = musx::dom::details::ArticulationAssign;
constexpr auto assignmentTag = records::packTag("IM");
constexpr records::LegacyTag assignmentClass = 0x03f1;
constexpr std::size_t fixedWordsPerAssignment = 5;
constexpr std::size_t classWordsPerAssignment = 10;
constexpr std::size_t assignmentVerticalOffsetSlot = 3;

void importSource(const ImportContext& context, const RecordFamilySource& source)
{
    for (const auto& [partId, entryHigh] : recordKeys(source)) {
        for (const auto entryLow : source.pool->secondCmpersForTag(source.identity, entryHigh, partId)) {
            const auto rows = source.pool->getArray(source.identity, entryHigh, entryLow, partId);
            const auto words = collectRecordWords(source, rows, context.profile.byteOrder);
            const auto wordsPerAssignment = source.classRecords ? classWordsPerAssignment : fixedWordsPerAssignment;
            if (words.size() % wordsPerAssignment != 0) {
                context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info, "Articulation assignment has an incomplete tuple."});
            }
            for (std::size_t at = 0; at + wordsPerAssignment <= words.size(); at += wordsPerAssignment) {
                // The older assignment upgrade discards a row without a definition.
                if (!source.classRecords && words[at] == 0) {
                    continue;
                }
                const auto inci = static_cast<musx::dom::Inci>(at / wordsPerAssignment);
                auto target = createDetailsRecordTarget<Target>(context.document, source, source.rowOfWord(rows, at), entryHigh, entryLow, inci);
                target->articDef = static_cast<musx::dom::Cmper>(static_cast<std::uint16_t>(words[at]));
                target->horzOffset = words[at + 1];
                target->horzAdd = words[at + 2];
                target->vertOffset = words[at + assignmentVerticalOffsetSlot];
                target->vertAdd = words[at + 4];
                target->neverStack = true;
                target->numSlursAvoided = -1;
                if (source.classRecords) {
                    target->hide = (static_cast<std::uint16_t>(words[at + 5]) & 1U) != 0;
                }
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                    const auto key = reporting.template instanceKey<Target>(
                        partId, entryHigh, inci, static_cast<musx::dom::Cmper>((static_cast<std::uint32_t>(entryHigh) << 16U) | entryLow));
                    reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
                    const auto stored = [&](const char* name, std::size_t slot) {
                        reportLegacyField(reporting, key, source, source.rowOfWord(rows, at + slot), name, source.byteOffsetInRow((at + slot) * 2),
                            words[at + slot]);
                    };
                    stored("articDef", 0);
                    stored("horzOffset", 1);
                    stored("horzAdd", 2);
                    stored("vertOffset", assignmentVerticalOffsetSlot);
                    stored("vertAdd", 4);
                    if (source.classRecords) {
                        stored("hide", 5);
                    } else {
                        reportFallbackField(reporting, key, "hide", Reporting::Origin::LegacyBehavior, 0);
                    }
                    for (const auto* field : {"overridePlacement", "aboveEntry", "avoidSlur"}) {
                        reportFallbackField(reporting, key, field, Reporting::Origin::LegacyBehavior, 0);
                    }
                    reportFallbackField(reporting, key, "neverStack", Reporting::Origin::LegacyBehavior, 1);
                    reportFallbackField(reporting, key, "numSlursAvoided", Reporting::Origin::LegacyBehavior, -1);
                });
                context.document->getDetails()->add(Target::XmlNodeName, std::move(target));
            }
        }
    }
}

/// @brief Converts the stored vertical offset to the maintained-placement convention.
/// @details Below-entry placement reverses the old sign before automatic placement removes
/// the definition's default distance. Both operations store a signed 16-bit result.
/// Score changes propagate through part link masks; unlinked part offsets are preserved.
/// Rest and reversed-stem geometry corrections require layout and are not applied here.
void normalizeArticulationVerticalOffsets(const ImportContext& context, const RecordFamilySource& source)
{
    using Definition = musx::dom::others::ArticulationDef;
    const auto assignments = context.document->getDetails()->getAllSources<Target>();
    // Complete score normalization before merging any linked part bits.
    for (const bool parts : {false, true}) {
        for (const auto& assignment : assignments) {
            const auto partId = assignment->getSourcePartId();
            if ((partId != musx::dom::SCORE_PARTID) != parts) {
                continue;
            }
            const auto number = assignment->getEntryNumber();
            auto offset = assignment->vertOffset;
            if (!parts) {
                const auto definition = assignment->articDef == 0
                                            ? nullptr
                                            : context.document->getOthers()->get<Definition>(musx::dom::SCORE_PARTID, assignment->articDef);
                if (!definition) {
                    continue;
                }
                if (definition->autoVertMode == Definition::AutoVerticalMode::BelowEntry) {
                    offset = static_cast<std::int16_t>(-offset);
                }
                if (definition->autoVert) {
                    offset = static_cast<std::int16_t>(offset - definition->defVertPos);
                }
            } else {
                const auto rows =
                    source.pool->getArray(source.identity, static_cast<std::uint16_t>(number >> 16U), static_cast<std::uint16_t>(number), partId);
                const auto at = static_cast<std::size_t>(assignment->getInci().value_or(0)) * classWordsPerAssignment;
                const auto mask = partUnlinkedBits(source, rows, at + assignmentVerticalOffsetSlot, context.profile.byteOrder);
                const auto score = context.document->getDetails()->get<Target>(musx::dom::SCORE_PARTID, number, assignment->getInci());
                if (!mask || *mask == 0xffffU || !score) {
                    continue;
                }
                offset = static_cast<std::int16_t>(
                    (static_cast<std::uint16_t>(score->vertOffset) & ~*mask) | (static_cast<std::uint16_t>(offset) & *mask));
            }
            if (offset == assignment->vertOffset) {
                continue;
            }
            std::const_pointer_cast<Target>(assignment)->vertOffset = offset;
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<Target>(
                    partId, static_cast<musx::dom::Cmper>(number >> 16U), assignment->getInci(), static_cast<musx::dom::Cmper>(number));
                if (auto* field = reporting.report().findField(key, "vertOffset")) {
                    field->origin = Reporting::Origin::LegacyMusAdjusted;
                }
            });
        }
    }
}

/// @brief Clears the articulation flag of each entry that has no articulation assignment.
/// @details Before Finale 3.0 the stored flag can be set with nothing assigned, and Finale's
/// upgrade clears it on every such entry.
void clearUnassignedArticulationFlags(const ImportContext& context)
{
    context.document->getEntries()->iterateRawEntries([&](const musx::dom::MusxInstance<musx::dom::Entry>& entry) {
        const auto number = entry->getEntryNumber();
        if (!entry->articDetail || !context.document->getDetails()->getArray<Target>(musx::dom::SCORE_PARTID, number).empty()) {
            return true;
        }
        const_cast<musx::dom::Entry*>(entry.get())->articDetail = false;
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            const auto key = reporting.template instanceKey<musx::dom::Entry>(
                musx::dom::SCORE_PARTID, static_cast<musx::dom::Cmper>(number >> 16U), std::nullopt, static_cast<musx::dom::Cmper>(number));
            if (auto* field = reporting.report().findField(key, "articDetail")) {
                field->origin = Reporting::Origin::LegacyMusAdjusted;
            }
        });
        return true;
    });
}

} // namespace

void importArticulationAssigns(const ImportContext& context)
{
    const auto source =
        selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), assignmentTag, assignmentClass, true);
    if (source) {
        importSource(context, *source);
        context.pending.defer(
            DeferredStage::NormalizeStoredValues, [&context, source = *source] { normalizeArticulationVerticalOffsets(context, source); });
    }
    if (sourcePredatesVersion(context.profile, FormatEpoch::UncompressedLegacy, versions::finale3_0)) {
        context.pending.defer(DeferredStage::RefreshDerivedFlags, [&context] { clearUnassignedArticulationFlags(context); });
    }
}

} // namespace finale_mus_reader::details
