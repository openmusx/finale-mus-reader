// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/details.h"
#include "import/shared/enclosure.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace finale_mus_reader {
namespace details {
namespace {
using NumberPosition = musx::dom::details::MeasureNumberIndividualPositioning;
constexpr std::size_t numberPositionWords = 15;

// Slot numbers refer to the normalized detail layout, including the contained enclosure.
template <typename Field>
void visitNumberPositionFields(const NumberPosition& target, Field field)
{
    field("measNumRegion", 0, target.measNumRegion);
    field("xOffset", 1, target.xOffset);
    field("yOffset", 2, target.yOffset);
    field("xOffset2", 3, target.xOffset2);
    field("forceVisibility", 4, static_cast<int>(target.forceVisibility));
    field("useEnclosure", 4, target.useEnclosure);
    field("isAlternateNumber", 4, target.isAlternateNumber);
    const auto& encl = *target.enclosure;
    field("enclosure.xAdd", 5, encl.xAdd);
    field("enclosure.yAdd", 6, encl.yAdd);
    field("enclosure.xMargin", 7, encl.xMargin);
    field("enclosure.yMargin", 8, encl.yMargin);
    field("enclosure.lineWidth", 9, encl.lineWidth);
    field("enclosure.fixedSize", 10, encl.fixedSize);
    field("enclosure.notTall", 10, encl.notTall);
    field("enclosure.equalAspect", 10, encl.equalAspect);
    field("enclosure.opaque", 10, encl.opaque);
    field("enclosure.shape", 10, static_cast<int>(encl.shape));
    field("enclosure.roundCorners", 11, encl.roundCorners);
    field("enclosure.cornerRadius", 12, encl.cornerRadius);
}

void importNumberPositions(const ImportContext& context, const RecordFamilySource& source)
{
    for (const auto& [partId, staff] : recordKeys(source)) {
        for (const auto measure : source.pool->secondCmpersForTag(source.identity, staff, partId)) {
            const auto rows = source.pool->getArray(source.identity, staff, measure, partId);
            const auto words = collectRecordWords(source, rows, context.profile.byteOrder);
            if (words.size() % numberPositionWords != 0) {
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Info, "Individual measure-number positioning has an incomplete trailing item."});
            }
            for (std::size_t at = 0; at + numberPositionWords <= words.size(); at += numberPositionWords) {
                const auto inci = static_cast<musx::dom::Inci>(at / numberPositionWords);
                const auto& row = source.rowOfWord(rows, at);
                auto target = createDetailsRecordTarget<NumberPosition>(context.document, source, row, staff, measure, inci);
                target->measNumRegion = static_cast<musx::dom::Cmper>(words[at]);
                target->xOffset = words[at + 1];
                target->yOffset = words[at + 2];
                target->xOffset2 = words[at + 3];
                const auto flags = static_cast<std::uint16_t>(words[at + 4]);
                const auto visibility = flags & 3U;
                target->forceVisibility = visibility == 1   ? NumberPosition::ForceVisibility::Hide
                                          : visibility == 2 ? NumberPosition::ForceVisibility::Show
                                                            : NumberPosition::ForceVisibility::None;
                target->useEnclosure = (flags & 4U) != 0;
                target->isAlternateNumber = (flags & 8U) != 0;
                target->enclosure = std::make_shared<musx::dom::others::Enclosure>(context.document);
                std::array<std::uint16_t, 6> enclosureWords;
                for (std::size_t slot = 0; slot < enclosureWords.size(); ++slot) {
                    enclosureWords[slot] = static_cast<std::uint16_t>(words[at + 5 + slot]);
                }
                const bool validShape = decodeLegacyEnclosure(*target->enclosure, enclosureWords);
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                    const auto key = reporting.template instanceKey<NumberPosition>(partId, staff, inci, measure);
                    reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
                    const auto field = [&](const char* name, std::size_t slot, std::int64_t raw) {
                        reportLegacyField(
                            reporting, key, source, source.rowOfWord(rows, at + slot), name, source.byteOffsetInRow((at + slot) * 2), raw);
                    };
                    visitNumberPositionFields(*target, [&](const char* name, std::size_t slot, std::int64_t value) {
                        if (slot >= 11) {
                            reportFallbackField(reporting, key, name, Reporting::Origin::MusxOnly, value);
                        } else if (!validShape && std::string_view(name) == "enclosure.shape") {
                            reportFallbackField(reporting, key, name, Reporting::Origin::Unmapped, value);
                        } else {
                            field(name, slot, slot == 4 || slot == 10 ? static_cast<std::uint16_t>(words[at + slot]) : words[at + slot]);
                        }
                    });
                });
                context.document->getDetails()->add(NumberPosition::XmlNodeName, std::move(target));
            }
        }
    }
}

using NumberRegion = musx::dom::others::MeasureNumberRegion;
using NumberMeasure = musx::dom::others::Measure;

std::shared_ptr<NumberPosition> createEarlyNumberPosition(const ImportContext& context, const RecordFamilySource& source,
    const records::LegacyRow& row, musx::dom::Cmper staff, musx::dom::Cmper measure, musx::dom::Cmper region)
{
    const auto existing = context.document->getDetails()->getArray<NumberPosition>(0, staff, measure);
    const auto inci = existing.empty() ? 0 : existing.back()->getInci().value_or(0) + 1;
    auto target = createDetailsRecordTarget<NumberPosition>(context.document, source, row, staff, measure, static_cast<musx::dom::Inci>(inci));
    target->measNumRegion = region;
    target->enclosure = std::make_shared<musx::dom::others::Enclosure>(context.document);
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<NumberPosition>(musx::dom::SCORE_PARTID, staff, target->getInci(), measure);
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMusAdjusted);
        visitNumberPositionFields(*target, [&](const char* name, std::size_t slot, std::int64_t value) {
            reportFallbackField(reporting, key, name, slot >= 11 ? Reporting::Origin::MusxOnly : Reporting::Origin::LegacyBehavior, value);
        });
        reportFallbackField(reporting, key, "measNumRegion", Reporting::Origin::LegacyMusAdjusted, region);
    });
    context.document->getDetails()->add(NumberPosition::XmlNodeName, target);
    return target;
}

void markNumberPositionMeasure(const ImportContext& context, musx::dom::Cmper measure)
{
    if (const auto target = context.document->getOthers()->get<NumberMeasure>(0, measure)) {
        std::const_pointer_cast<NumberMeasure>(target)->hasMeasNumbIndivPos = true;
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            reportFallbackField(reporting, reporting.template instanceKey<NumberMeasure>(musx::dom::SCORE_PARTID, measure), "hasMeasNumbIndivPos",
                Reporting::Origin::LegacyMusAdjusted, 1);
        });
    }
}

void convertEarlyNumberPositions(const ImportContext& context)
{
    const auto regions = context.document->getOthers()->getArray<NumberRegion>(0);
    const RecordFamilySource positions{&context.index.getOthers(), records::packTag("MI")};
    for (const auto& [part, measure] : recordKeys(positions)) {
        const auto region = std::find_if(regions.begin(), regions.end(), [&](const auto& value) { return value->calcIncludesMeasure(measure); });
        if (region == regions.end()) {
            continue;
        }
        const auto measureRows = context.index.getOthers().getArray(records::packTag("MS"), measure, 0);
        for (const auto& row : positions.pool->getArray(positions.identity, measure, 0, part)) {
            auto target = createEarlyNumberPosition(
                context, positions, row, static_cast<musx::dom::Cmper>(row.words[0]), measure, static_cast<musx::dom::Cmper>((*region)->region));
            target->xOffset = row.words[2];
            target->yOffset = row.words[3];
            target->xOffset2 = row.words[4];
            const auto flags = measureRows.empty() ? 0U : static_cast<std::uint16_t>(measureRows.front().words[4]);
            target->forceVisibility = (flags & 0x0200U)   ? NumberPosition::ForceVisibility::Hide
                                      : (flags & 0x0800U) ? NumberPosition::ForceVisibility::Show
                                                          : NumberPosition::ForceVisibility::None;
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key =
                    reporting.template instanceKey<NumberPosition>(musx::dom::SCORE_PARTID, target->getCmper1(), target->getInci(), measure);
                for (const auto& [name, slot] : {std::pair{"xOffset", 2U}, {"yOffset", 3U}, {"xOffset2", 4U}}) {
                    reportLegacyField(reporting, key, positions, row, name, slot * 2, row.words[slot]);
                }
                if (!measureRows.empty()) {
                    const RecordFamilySource measures{&context.index.getOthers(), records::packTag("MS")};
                    reportLegacyField(
                        reporting, key, measures, measureRows.front(), "forceVisibility", 8, flags, Reporting::Origin::LegacyMusAdjusted);
                }
            });
            markNumberPositionMeasure(context, measure);
        }
    }

    // Coda enclosure conversion is intentionally outside the supported enclosure scope.
    if (context.profile.epoch == FormatEpoch::CodaBanner) {
        return;
    }
    const RecordFamilySource enclosures{&context.index.getOthers(), records::packTag("Me")};
    for (const auto& [part, measure] : recordKeys(enclosures)) {
        if (!context.document->getOthers()->get<NumberMeasure>(0, measure)) {
            continue;
        }
        const auto rows = enclosures.pool->getArray(enclosures.identity, measure, 0, part);
        if (rows.empty()) {
            continue;
        }
        for (const auto& region : regions) {
            if (!region->calcIncludesMeasure(measure)) {
                continue;
            }
            for (const auto& staff : context.document->getScrollViewStaves(0)) {
                const auto existing = context.document->getDetails()->getArray<NumberPosition>(0, staff->staffId, measure);
                const auto found = std::find_if(existing.begin(), existing.end(),
                    [&](const auto& value) { return value->measNumRegion == region->region && !value->isAlternateNumber; });
                auto target = found != existing.end() ? std::const_pointer_cast<NumberPosition>(*found)
                                                      : createEarlyNumberPosition(context, enclosures, rows.front(), staff->staffId, measure,
                                                            static_cast<musx::dom::Cmper>(region->region));
                target->useEnclosure = true;
                std::array<std::uint16_t, 6> enclosureWords;
                for (std::size_t slot = 0; slot < enclosureWords.size(); ++slot) {
                    enclosureWords[slot] = static_cast<std::uint16_t>(rows.front().words[slot]);
                }
                const bool validShape = decodeLegacyEnclosure(*target->enclosure, enclosureWords);
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                    const auto key =
                        reporting.template instanceKey<NumberPosition>(musx::dom::SCORE_PARTID, staff->staffId, target->getInci(), measure);
                    reportFallbackField(reporting, key, "useEnclosure", Reporting::Origin::LegacyMusAdjusted, 1);
                    visitNumberPositionFields(*target, [&](const char* name, std::size_t slot, std::int64_t value) {
                        if (slot < 5 || slot >= 11) {
                            return;
                        }
                        if (!validShape && std::string_view(name) == "enclosure.shape") {
                            reportFallbackField(reporting, key, name, Reporting::Origin::Unmapped, value);
                        } else {
                            reportLegacyField(reporting, key, enclosures, rows.front(), name, (slot - 5) * 2, rows.front().words[slot - 5]);
                        }
                    });
                });
            }
        }
        markNumberPositionMeasure(context, measure);
    }
}

void normalizeNumberPositionScaling(const ImportContext& context)
{
    // The region payload states whether the separate score/part layout is already present.
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), records::packTag("MN"), 0x00a4);
    if (!source) {
        return;
    }
    const auto regions = context.document->getOthers()->getArray<NumberRegion>(0);
    for (const auto& position : context.document->getDetails()->getAllSources<NumberPosition>()) {
        // The region-layout scaling conversion applies only to score-owned positions.
        if (position->getSourcePartId() != musx::dom::SCORE_PARTID) {
            continue;
        }
        const auto region = std::find_if(regions.begin(), regions.end(), [&](const auto& value) { return value->region == position->measNumRegion; });
        if (region == regions.end() || !(*region)->scoreData || !(*region)->scoreData->showOnStart) {
            continue;
        }
        if (source->classRecords) {
            const auto rows = source->pool->getArray(source->identity, (*region)->getCmper(), 0);
            if (collectRecordPayload(*source, rows).size() != 96) {
                continue;
            }
        }
        const auto systems = context.document->getOthers()->getArray<musx::dom::others::StaffSystem>(position->getSourcePartId());
        const auto measure = position->getCmper2();
        const auto system =
            std::find_if(systems.begin(), systems.end(), [&](const auto& value) { return value->startMeas <= measure && measure < value->endMeas; });
        if (system == systems.end() || (*system)->startMeas == measure || (*system)->horzPercent <= 0) {
            continue;
        }
        auto target = std::const_pointer_cast<NumberPosition>(position);
        const auto offset = (*region)->scoreData->startXdisp;
        const float scale = (1000000.0F / static_cast<float>((*system)->horzPercent * 100.0)) * 0.01F;
        for (const auto& [name, member] : {std::pair{"xOffset", &NumberPosition::xOffset}, {"xOffset2", &NumberPosition::xOffset2}}) {
            const auto adjusted = static_cast<std::int16_t>(
                static_cast<std::int16_t>(std::lround(static_cast<float>(target.get()->*member + offset) * scale)) - offset);
            if (adjusted != target.get()->*member) {
                target.get()->*member = adjusted;
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                    const auto key =
                        reporting.template instanceKey<NumberPosition>(target->getSourcePartId(), target->getCmper1(), target->getInci(), measure);
                    reporting.report().fieldProvenance(key, name).origin = Reporting::Origin::LegacyMusAdjusted;
                });
            }
        }
    }
}

} // namespace

void importMeasureNumberIndividualPositioning(const ImportContext& context)
{
    const auto source =
        selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), records::packTag("MI"), 0x041f, true);
    if (source) {
        importNumberPositions(context, *source);
    }
    if (context.profile.epoch != FormatEpoch::ZlibLegacy) {
        context.pending.defer(DeferredStage::NormalizeStoredValues, [&context] { convertEarlyNumberPositions(context); });
    }
    context.pending.defer(DeferredStage::RefreshDerivedFlags, [&context] { normalizeNumberPositionScaling(context); });
}
} // namespace details
} // namespace finale_mus_reader
