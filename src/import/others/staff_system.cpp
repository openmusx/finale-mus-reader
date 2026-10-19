// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "import/others.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using StaffSystemTarget = musx::dom::others::StaffSystem;

constexpr records::LegacyTag staffSystemTag = records::packTag("SS");
constexpr records::LegacyTag systemScalingTag = records::packTag("SP");
constexpr records::LegacyTag staffSystemClass = 0x00df;
constexpr std::size_t staffSystemRowBytes = records::otherWordCount * sizeof(std::uint16_t);
constexpr std::size_t compactStaffSystemBytes = staffSystemRowBytes;
constexpr std::size_t staffSystemBaseBytes = 24;
constexpr std::size_t staffSystemExtendedBytes = 28;
constexpr musx::dom::Efix staffHeightEfixPerLegacyUnit = 4;
/// @brief The height of a standard five-line staff, which a record without a stored height takes.
constexpr auto standardStaffHeightEfix = static_cast<musx::dom::Efix>(4 * musx::dom::EFIX_PER_SPACE);
constexpr int compactStaffSystemPercent = 100;

constexpr std::size_t topOffset = 0;
constexpr std::size_t leftOffset = 2;
constexpr std::size_t rightOffset = 4;
constexpr std::size_t bottomOffset = 6;
constexpr std::size_t startMeasOffset = 8;
constexpr std::size_t flagsOffset = 10;
constexpr std::size_t endMeasOffset = 12;
constexpr std::size_t horzPercentOffset = 14;
constexpr std::size_t ssysPercentOffset = 18;
constexpr std::size_t distanceToPrevOffset = 20;
constexpr std::size_t staffHeightOffset = 22;
constexpr std::size_t extraStartSystemSpaceOffset = 24;
constexpr std::size_t extraEndSystemSpaceOffset = 26;

constexpr std::uint16_t holdMarginsMask = 0x0001;
constexpr std::uint16_t scaleVertMask = 0x0002;
constexpr std::uint16_t noNamesMask = 0x2000;
constexpr std::uint16_t placeEndSpaceBeforeBarlineMask = 0x0010;
constexpr std::uint16_t hasStaffScalingMask = 0x0400;
constexpr std::uint16_t ownStaffListMask = 0x4000;
constexpr std::uint16_t compactLayoutMask = 0x0080;
constexpr std::uint16_t compactScalingMask = 0x0800;
constexpr records::LegacyTag compactLayoutTag = records::packTag("fS");
constexpr records::LegacyTag earlyCompactLayoutTag = records::packTag("FS");
constexpr std::uint16_t compactScaleVertMask = 0x2000;
constexpr std::uint16_t compactScalingHoldMarginsMask = 0x4000;

enum class StaffSystemLayout {
    Compact,
    Expanded,
};

struct CompactSystemScaling
{
    records::LegacyRow row;
    std::int16_t percent{};
    std::uint16_t flags{};
};

struct CompactStaffSystemEntry
{
    std::shared_ptr<StaffSystemTarget> target;
    records::LegacyRow row;
    std::int16_t storedTopOrDistance{};
    std::optional<CompactSystemScaling> scaling;
    std::optional<records::LegacyRow> layout;
    records::LegacyTag layoutTag{};
    std::int32_t storedHorzPercent{};
};

std::optional<musx::dom::MeasCmper> lastStaffSystemMeasure(const ImportContext& context)
{
    std::optional<musx::dom::MeasCmper> result;
    for (const auto& measure : context.document->getOthers()->getArray<musx::dom::others::Measure>(musx::dom::SCORE_PARTID)) {
        if (measure->getCmper() <= (std::numeric_limits<musx::dom::MeasCmper>::max)()) {
            result = (std::max)(result.value_or(0), static_cast<musx::dom::MeasCmper>(measure->getCmper()));
        }
    }
    return result;
}

bool staffSystemCoversFinalMeasure(const StaffSystemTarget& system, std::optional<musx::dom::MeasCmper> lastMeasure)
{
    return lastMeasure && system.startMeas > 0 && system.startMeas <= *lastMeasure && system.endMeas > *lastMeasure;
}

void finishStaffSystemEnds(const ImportContext& context, const RecordFamilySource& source, std::optional<musx::dom::MeasCmper> lastMeasure)
{
    if (!lastMeasure || *lastMeasure == (std::numeric_limits<musx::dom::MeasCmper>::max)()) {
        return;
    }
    for (const auto partId : recordPartIds(source)) {
        const auto systems = context.document->getOthers()->getArray<StaffSystemTarget>(partId);
        if (systems.empty() || systems.back()->startMeas == 0) {
            continue;
        }
        auto& target = *const_cast<StaffSystemTarget*>(systems.back().get());
        const auto end = static_cast<musx::dom::MeasCmper>(*lastMeasure + 1);
        if (target.endMeas != end) {
            target.endMeas = end;
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<StaffSystemTarget>(target.getSourcePartId(), target.getCmper());
                reporting.report().fieldProvenance(key, "endMeas").origin = Reporting::Origin::LegacyMusAdjusted;
            });
        }
    }
}

void warnAboutStaffSystemSequence(const ImportContext& context, const RecordFamilySource& source)
{
    for (const auto partId : recordPartIds(source)) {
        unsigned expected = 1;
        for (const auto& system : context.document->getOthers()->getArray<StaffSystemTarget>(partId)) {
            if (system->getCmper() != expected) {
                context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning,
                    "Nonsequential staff system IDs in part " + std::to_string(partId) + ": expected " + std::to_string(expected) + ", found "
                        + std::to_string(system->getCmper()) + ". Stored systems are preserved."});
            }
            expected = unsigned(system->getCmper()) + 1;
        }
    }
}

template <typename Reporting>
void reportStaffSystemRowField(Reporting& reporting, const typename Reporting::InstanceKey& key, const records::LegacyRow& row,
    records::LegacyTag identity, const char* member, std::size_t byteOffset, std::int64_t value, typename Reporting::Origin origin)
{
    reporting.report().setField(key, member, typename Reporting::FieldInfo{origin, row.blockOffset, row.decodedOffset + byteOffset, value, identity});
}

template <typename Reporting>
void reportStaffSystemSourceField(Reporting& reporting, const typename Reporting::InstanceKey& key, const RecordFamilySource& source,
    std::span<const records::LegacyRow> rows, const char* member, std::size_t byteOffset, std::int64_t value, typename Reporting::Origin origin)
{
    const auto& row = source.rowOfByte(rows, byteOffset);
    reportStaffSystemRowField(reporting, key, row, source.identity, member, source.byteOffsetInRow(byteOffset), value, origin);
}

void reportStaffSystem(const ImportContext& context, const RecordFamilySource& source, std::span<const records::LegacyRow> rows,
    const StaffSystemTarget& target, bool uncompressed, bool hasExtendedFields, bool hasStoredStaffHeight, std::int16_t storedTop,
    musx::dom::MeasCmper storedEndMeas, std::int32_t storedHorzPercent, std::int16_t storedStaffHeight)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<StaffSystemTarget>(target.getSourcePartId(), target.getCmper());
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        const auto sourceField = [&](const char* member, std::size_t offset, std::int64_t value) {
            reportStaffSystemSourceField(reporting, key, source, rows, member, offset, value, Reporting::Origin::LegacyMus);
        };

        if (uncompressed && target.getCmper() != 1) {
            reporting.unmappedField(key, "top", target.top);
        } else {
            reportStaffSystemSourceField(reporting, key, source, rows, "top", topOffset, storedTop, Reporting::Origin::LegacyMus);
        }
        sourceField("left", leftOffset, target.left);
        sourceField("right", rightOffset, target.right);
        sourceField("bottom", bottomOffset, target.bottom);
        sourceField("startMeas", startMeasOffset, target.startMeas);
        reportStaffSystemSourceField(reporting, key, source, rows, "endMeas", endMeasOffset, storedEndMeas, Reporting::Origin::LegacyMus);
        sourceField("horzPercent", horzPercentOffset, storedHorzPercent);
        sourceField("ssysPercent", ssysPercentOffset, target.ssysPercent);
        if (uncompressed && target.getCmper() == 1) {
            reporting.report().setField(key, "distanceToPrev", typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, 0});
        } else {
            sourceField("distanceToPrev", uncompressed ? topOffset : distanceToPrevOffset, target.distanceToPrev);
        }
        if (hasStoredStaffHeight) {
            sourceField("staffHeight", staffHeightOffset, storedStaffHeight);
        } else {
            reporting.report().setField(
                key, "staffHeight", typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, target.staffHeight});
        }

        sourceField("noNames", flagsOffset, target.noNames);
        sourceField("hasStaffScaling", flagsOffset, target.hasStaffScaling);
        sourceField("placeEndSpaceBeforeBarline", flagsOffset, target.placeEndSpaceBeforeBarline);
        sourceField("scaleVert", flagsOffset, target.scaleVert);
        sourceField("holdMargins", flagsOffset, target.holdMargins);

        if (hasExtendedFields) {
            sourceField("extraStartSystemSpace", extraStartSystemSpaceOffset, target.extraStartSystemSpace);
            sourceField("extraEndSystemSpace", extraEndSystemSpaceOffset, target.extraEndSystemSpace);
        } else {
            reporting.report().setField(
                key, "extraStartSystemSpace", typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, target.extraStartSystemSpace});
            reporting.report().setField(
                key, "extraEndSystemSpace", typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, target.extraEndSystemSpace});
        }
    });
}

void reportCompactStaffSystem(const ImportContext& context, const RecordFamilySource& source, const records::LegacyRow& row,
    const StaffSystemTarget& target, std::int16_t storedTopOrDistance, const std::optional<CompactSystemScaling>& scaling)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<StaffSystemTarget>(target.getSourcePartId(), target.getCmper());
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        const auto rows = std::span<const records::LegacyRow>(&row, 1);
        const auto sourceField = [&](const char* member, std::size_t offset, std::int64_t value) {
            reportStaffSystemSourceField(reporting, key, source, rows, member, offset, value, Reporting::Origin::LegacyMus);
        };
        const auto behaviorField = [&](const char* member, std::int64_t value) {
            reporting.report().setField(key, member, typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, value});
        };

        if (target.getCmper() == 1) {
            sourceField("top", topOffset, storedTopOrDistance);
        } else {
            reporting.unmappedField(key, "top", target.top);
        }
        sourceField("left", leftOffset, target.left);
        sourceField("right", rightOffset, target.right);
        sourceField("bottom", bottomOffset, target.bottom);
        sourceField("startMeas", startMeasOffset, target.startMeas);
        reporting.unmappedField(key, "horzPercent", 0);
        if (scaling) {
            reportStaffSystemRowField(
                reporting, key, scaling->row, systemScalingTag, "ssysPercent", 0, scaling->percent, Reporting::Origin::LegacyMus);
        } else {
            behaviorField("ssysPercent", target.ssysPercent);
        }
        behaviorField("staffHeight", target.staffHeight);
        sourceField("noNames", flagsOffset, target.noNames);
        sourceField("hasStaffScaling", flagsOffset, target.hasStaffScaling);
        sourceField("placeEndSpaceBeforeBarline", flagsOffset, target.placeEndSpaceBeforeBarline);
        if (scaling) {
            reportStaffSystemRowField(
                reporting, key, scaling->row, systemScalingTag, "scaleVert", flagsOffset, target.scaleVert, Reporting::Origin::LegacyMus);
            reportStaffSystemRowField(
                reporting, key, scaling->row, systemScalingTag, "holdMargins", flagsOffset, target.holdMargins, Reporting::Origin::LegacyMus);
        } else {
            behaviorField("scaleVert", target.scaleVert);
            behaviorField("holdMargins", target.holdMargins);
        }
        if (target.getCmper() == 1) {
            behaviorField("distanceToPrev", target.distanceToPrev);
        } else {
            sourceField("distanceToPrev", topOffset, storedTopOrDistance);
        }
        behaviorField("extraStartSystemSpace", target.extraStartSystemSpace);
        behaviorField("extraEndSystemSpace", target.extraEndSystemSpace);
    });
}

std::optional<CompactSystemScaling> readCompactSystemScaling(const ImportContext& context, musx::dom::Cmper partId, musx::dom::Cmper systemId)
{
    const auto rows = context.index.getOthers().getArray(systemScalingTag, systemId, 0, std::uint16_t(partId));
    if (rows.empty()) {
        return std::nullopt;
    }
    if (rows.size() != 1 || rows.front().wordCount != records::otherWordCount) {
        context.report.diagnostics.push_back(
            {musx::util::Logger::LogLevel::Info, "Compact system scaling " + std::to_string(systemId) + " does not have its six-word layout."});
        return std::nullopt;
    }

    return CompactSystemScaling{rows.front(), rows.front().words[0], static_cast<std::uint16_t>(rows.front().words[5])};
}

void finishCompactStaffSystems(const ImportContext& context, const RecordFamilySource& source, std::vector<CompactStaffSystemEntry> systems)
{
    for (auto& entry : systems) {
        auto& target = *entry.target;
        reportCompactStaffSystem(context, source, entry.row, target, entry.storedTopOrDistance, entry.scaling);
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            const auto key = reporting.template instanceKey<StaffSystemTarget>(target.getSourcePartId(), target.getCmper());
            if (entry.layout) {
                reportStaffSystemRowField(reporting, key, *entry.layout, entry.layoutTag, "endMeas", 2, target.endMeas, Reporting::Origin::LegacyMus);
                reportStaffSystemRowField(reporting, key, *entry.layout, entry.layoutTag, "horzPercent",
                    entry.layoutTag == earlyCompactLayoutTag ? 4 : 6,
                    entry.layoutTag == earlyCompactLayoutTag ? entry.layout->words[2] : entry.storedHorzPercent, Reporting::Origin::LegacyMus);
            } else {
                for (const auto* member : {"endMeas", "horzPercent"}) {
                    reporting.report().setField(key, member, typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, 0});
                }
            }
        });
        context.document->getOthers()->add(StaffSystemTarget::XmlNodeName, entry.target);
    }
    warnAboutStaffSystemSequence(context, source);
}

void importCompactStaffSystemFamily(const ImportContext& context, const RecordFamilySource& source, std::optional<musx::dom::MeasCmper> lastMeasure)
{
    std::vector<CompactStaffSystemEntry> systems;
    for (const auto partId : recordPartIds(source)) {
        for (const auto systemId : source.pool->cmpersForTag(source.identity, partId)) {
            const auto rows = source.pool->getArray(source.identity, systemId, 0, partId);
            if (rows.empty()) {
                continue;
            }
            const auto payload = collectRecordPayload(source, rows);
            if (payload.size() != compactStaffSystemBytes) {
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Info, "Compact staff system " + std::to_string(systemId) + " does not have its six-word layout."});
                continue;
            }

            const auto startMeasure = static_cast<musx::dom::MeasCmper>(payloadWord(payload, startMeasOffset, context.profile.byteOrder));
            if (lastMeasure && startMeasure > *lastMeasure) {
                break;
            }

            auto target = createOthersRecordTarget<StaffSystemTarget>(context.document, source, rows.front(), systemId);
            const auto signedWord = [&](std::size_t offset) {
                return static_cast<std::int16_t>(payloadWord(payload, offset, context.profile.byteOrder));
            };
            const auto storedTopOrDistance = signedWord(topOffset);
            const auto flags = payloadWord(payload, flagsOffset, context.profile.byteOrder);
            auto scaling = (flags & compactScalingMask) != 0 ? readCompactSystemScaling(context, musx::dom::Cmper(partId), systemId) : std::nullopt;
            if (scaling && scaling->percent == 0) {
                scaling.reset();
            }
            if ((flags & ownStaffListMask) != 0) {
                context.pending.staffSystemsWithOwnStaffLists.emplace(partId, systemId);
            }
            target->left = signedWord(leftOffset);
            target->right = signedWord(rightOffset);
            target->bottom = signedWord(bottomOffset);
            target->startMeas = startMeasure;
            target->noNames = (flags & noNamesMask) != 0;
            target->placeEndSpaceBeforeBarline = (flags & placeEndSpaceBeforeBarlineMask) != 0;
            target->hasStaffScaling = (flags & hasStaffScalingMask) != 0;
            target->ssysPercent = compactStaffSystemPercent;
            target->staffHeight = standardStaffHeightEfix;
            target->holdMargins = true;
            target->scaleVert = (flags & scaleVertMask) != 0;
            if (scaling) {
                target->ssysPercent = scaling->percent;
                target->scaleVert = target->scaleVert || (scaling->flags & compactScaleVertMask) != 0;
                target->holdMargins = (flags & holdMarginsMask) != 0 || (scaling->flags & compactScalingHoldMarginsMask) != 0;
            }
            if (systemId != 1) {
                target->distanceToPrev = storedTopOrDistance;
            } else {
                target->top = storedTopOrDistance;
            }

            CompactStaffSystemEntry entry{target, rows.front(), storedTopOrDistance, scaling, std::nullopt, 0, 0};
            if ((flags & compactLayoutMask) != 0) {
            // The renamed record widens the horizontal percentage from a word to a long.
            // Its tag distinguishes the layouts even when the source has no version.
                for (const auto tag : {compactLayoutTag, earlyCompactLayoutTag}) {
                    const auto layoutRows = context.index.getOthers().getArray(tag, systemId, 0, partId);
                    if (layoutRows.empty()) {
                        continue;
                    }
                    if (layoutRows.size() != 1 || layoutRows.front().wordCount != records::otherWordCount) {
                        context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
                            "Compact system layout " + std::to_string(systemId) + " does not have its six-word layout."});
                        break;
                    }
                    entry.layout = layoutRows.front();
                    entry.layoutTag = tag;
                    const auto& words = entry.layout->words;
                    target->endMeas = words[1];
                    entry.storedHorzPercent =
                        tag == earlyCompactLayoutTag
                            ? std::int32_t(words[2]) * 100
                            : static_cast<std::int32_t>((std::uint32_t(std::uint16_t(words[3])) << 16) | std::uint16_t(words[4]));
                    target->horzPercent = static_cast<double>(entry.storedHorzPercent) / 100.0;
                    break;
                }
            }
            systems.push_back(std::move(entry));
            if (staffSystemCoversFinalMeasure(*target, lastMeasure)) {
                break;
            }
        }
    }
    finishCompactStaffSystems(context, source, std::move(systems));
}

std::optional<StaffSystemLayout> selectStaffSystemLayout(const RecordFamilySource& source)
{
    for (const auto& [partId, systemId] : recordKeys(source)) {
        const auto rows = source.pool->getArray(source.identity, systemId, 0, partId);
        if (rows.empty()) {
            continue;
        }

        // A StaffSystem family has one layout. The exact six-word compact row
        // is structurally disjoint from the expanded layout's twelve-word
        // prefix, so payload width selects the decoder without dating the file.
        return collectRecordPayload(source, rows).size() == compactStaffSystemBytes ? StaffSystemLayout::Compact : StaffSystemLayout::Expanded;
    }
    return std::nullopt;
}

void importStaffSystemFamily(const ImportContext& context, const RecordFamilySource& source, std::optional<musx::dom::MeasCmper> lastMeasure)
{
    const bool uncompressed = context.profile.epoch == FormatEpoch::UncompressedLegacy;
    for (const auto partId : recordPartIds(source)) {
        for (const auto systemId : source.pool->cmpersForTag(source.identity, partId)) {
            const auto rows = source.pool->getArray(source.identity, systemId, 0, partId);
            if (rows.empty()) {
                continue;
            }
            const auto payload = collectRecordPayload(source, rows);
            if (payload.size() < staffSystemBaseBytes || (payload.size() > staffSystemBaseBytes && payload.size() < staffSystemExtendedBytes)) {
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Info, "Staff system " + std::to_string(systemId) + " is shorter than a recognized layout."});
                continue;
            }

            const auto startMeasure = static_cast<musx::dom::MeasCmper>(payloadWord(payload, startMeasOffset, context.profile.byteOrder));
            if (lastMeasure && startMeasure > *lastMeasure) {
                break;
            }

            auto target = createOthersRecordTarget<StaffSystemTarget>(context.document, source, rows.front(), systemId);
            const auto word = [&](std::size_t offset) { return payloadWord(payload, offset, context.profile.byteOrder); };
            const auto signedWord = [&](std::size_t offset) { return static_cast<std::int16_t>(word(offset)); };

            const auto storedTop = signedWord(topOffset);
            if (!uncompressed || systemId == 1) {
                target->top = storedTop;
            }
            target->left = signedWord(leftOffset);
            target->right = signedWord(rightOffset);
            target->bottom = signedWord(bottomOffset);
            target->startMeas = startMeasure;
            const auto storedEndMeas = static_cast<musx::dom::MeasCmper>(word(endMeasOffset));
            target->endMeas = storedEndMeas;
            const auto storedHorzPercent = payloadLong(payload, horzPercentOffset, context.profile.byteOrder, LongWordOrder::HighFirst);
            target->horzPercent = static_cast<double>(storedHorzPercent) / 100.0;
            target->ssysPercent = signedWord(ssysPercentOffset);
            target->distanceToPrev = signedWord(distanceToPrevOffset);
            if (uncompressed) {
                target->distanceToPrev = systemId == 1 ? 0 : storedTop;
            }

            const auto storedStaffHeight = signedWord(staffHeightOffset);
            const bool hasStoredStaffHeight =
                storedStaffHeight != 0 && !sourcePredatesVersion(context.profile, FormatEpoch::DclLegacy, VersionBound{7, 0, 0, 2, 3});
            target->staffHeight = hasStoredStaffHeight ? storedStaffHeight * staffHeightEfixPerLegacyUnit : standardStaffHeightEfix;

            const auto flags = word(flagsOffset);
            if ((flags & ownStaffListMask) != 0 || sourceAtOrAfter(context.profile, FormatEpoch::ZlibLegacy, versions::finale2011)) {
                context.pending.staffSystemsWithOwnStaffLists.emplace(partId, systemId);
            }
            target->noNames = (flags & noNamesMask) != 0;
            target->hasStaffScaling = (flags & hasStaffScalingMask) != 0;
            target->placeEndSpaceBeforeBarline = (flags & placeEndSpaceBeforeBarlineMask) != 0;
            target->scaleVert = (flags & scaleVertMask) != 0;
            target->holdMargins = (flags & holdMarginsMask) != 0;

            const bool hasExtendedFields = payload.size() >= staffSystemExtendedBytes;
            if (hasExtendedFields) {
                target->extraStartSystemSpace = signedWord(extraStartSystemSpaceOffset);
                target->extraEndSystemSpace = signedWord(extraEndSystemSpaceOffset);
            }

            reportStaffSystem(context, source, rows, *target, uncompressed, hasExtendedFields, hasStoredStaffHeight, storedTop, storedEndMeas,
                storedHorzPercent, storedStaffHeight);
            const bool coversFinalMeasure = staffSystemCoversFinalMeasure(*target, lastMeasure);
            context.document->getOthers()->add(StaffSystemTarget::XmlNodeName, std::move(target));
            if (coversFinalMeasure) {
                break;
            }
        }
    }
    warnAboutStaffSystemSequence(context, source);
}

}  // namespace

void importStaffSystems(const ImportContext& context)
{
    const auto source =
        selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), staffSystemTag, staffSystemClass);
    if (!source) {
        return;
    }
    const auto layout = selectStaffSystemLayout(*source);
    if (!layout) {
        return;
    }
    context.pending.defer(DeferredStage::CompletePools, [&context, source = *source, layout = *layout] {
        const auto lastMeasure = lastStaffSystemMeasure(context);
        if (layout == StaffSystemLayout::Compact) {
            importCompactStaffSystemFamily(context, source, lastMeasure);
        } else {
            importStaffSystemFamily(context, source, lastMeasure);
        }
        finishStaffSystemEnds(context, source, lastMeasure);
    });
}

}  // namespace others
}  // namespace finale_mus_reader
