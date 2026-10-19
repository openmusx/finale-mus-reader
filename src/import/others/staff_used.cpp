// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using StaffUsedTarget = musx::dom::others::StaffUsed;
using StaffSystemTarget = musx::dom::others::StaffSystem;
using ListKey = std::pair<musx::dom::Cmper, musx::dom::Cmper>;

constexpr records::LegacyTag compactStaffUsedTag = records::packTag("IU");
constexpr records::LegacyTag rangedStaffUsedTag = records::packTag("Iu");
constexpr records::LegacyTag staffUsedClass = 0x009f;
constexpr std::size_t rowBytes = records::otherWordCount * sizeof(std::uint16_t);
constexpr std::size_t earlyElementBytes = rowBytes / 2;
constexpr std::size_t rangedElementBytes = 2 * rowBytes;
constexpr musx::dom::Cmper codaStaffSet1SystemId = 65530;
constexpr musx::dom::Cmper codaStaffSetCount = 4;

constexpr std::size_t staffIdOffset = 0;
constexpr std::size_t earlyDistanceOffset = 4;
constexpr std::size_t distanceOffset = 8;
constexpr std::size_t rangeStartMeasOffset = 12;
constexpr std::size_t rangeStartEduOffset = 14;
constexpr std::size_t rangeEndMeasOffset = 18;
constexpr std::size_t rangeEndEduOffset = 20;

struct StaffUsedSourcePosition
{
    std::size_t blockOffset{};
    std::size_t decodedOffset{};
    records::LegacyTag identity{};
    std::int64_t rawValue{};
};

struct StaffUsedEntry
{
    std::shared_ptr<StaffUsedTarget> target;
    StaffUsedSourcePosition staffIdSource;
    StaffUsedSourcePosition distanceSource;
    std::optional<StaffUsedSourcePosition> startMeasSource{};
    std::optional<StaffUsedSourcePosition> startEduSource{};
    std::optional<StaffUsedSourcePosition> endMeasSource{};
    std::optional<StaffUsedSourcePosition> endEduSource{};
};

struct StaffUsedListSource
{
    std::optional<StaffUsedSourcePosition> normalization;
};

using StaffUsedListSources = std::map<ListKey, StaffUsedListSource>;

musx::dom::Cmper modernStaffUsedCmper(const ImportContext& context, musx::dom::Cmper sourceCmper)
{
    // Believed: the four Coda-era Staff Sets immediately follow the Special Part Extraction and temporary-system cmpers. Finale 3.x moved
    // the expanded eight-set namespace lower so that it would remain within 16 bits.
    if (context.profile.epoch == FormatEpoch::CodaBanner && sourceCmper >= codaStaffSet1SystemId
        && sourceCmper < codaStaffSet1SystemId + codaStaffSetCount) {
        return static_cast<musx::dom::Cmper>(musx::dom::STAFF_SET_1_SYSTEM_ID + sourceCmper - codaStaffSet1SystemId);
    }
    return sourceCmper;
}

StaffUsedSourcePosition sourcePosition(
    const RecordFamilySource& source, std::span<const records::LegacyRow> rows, std::size_t byteOffset, std::int64_t rawValue)
{
    const auto& row = source.rowOfByte(rows, byteOffset);
    return {row.blockOffset, row.decodedOffset + source.byteOffsetInRow(byteOffset), source.identity, rawValue};
}

template <typename Reporting>
void reportStaffUsedField(Reporting& reporting, const typename Reporting::InstanceKey& key, const char* member, const StaffUsedSourcePosition& source,
    typename Reporting::Origin origin)
{
    reporting.report().setField(
        key, member, typename Reporting::FieldInfo{origin, source.blockOffset, source.decodedOffset, source.rawValue, source.identity});
}

void reportSourceStaffUsed(const ImportContext& context, const StaffUsedEntry& entry, bool normalized)
{
    const auto& target = *entry.target;
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<StaffUsedTarget>(target.getSourcePartId(), target.getCmper(), target.getInci());
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        reportStaffUsedField(reporting, key, "staffId", entry.staffIdSource, Reporting::Origin::LegacyMus);
        reportStaffUsedField(
            reporting, key, "distFromTop", entry.distanceSource, normalized ? Reporting::Origin::LegacyMusAdjusted : Reporting::Origin::LegacyMus);
        const auto reportRange = [&](const char* member, const std::optional<StaffUsedSourcePosition>& source, std::int64_t value) {
            const std::string reportMember = std::string("range.") + member;
            if (source) {
                reportStaffUsedField(reporting, key, reportMember.c_str(), *source, Reporting::Origin::LegacyMus);
            } else {
                reporting.report().setField(key, reportMember, typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, value});
            }
        };
        reportRange("startMeas", entry.startMeasSource, target.range->startMeas);
        reportRange("startEdu", entry.startEduSource, target.range->startEdu);
        reportRange("endMeas", entry.endMeasSource, target.range->endMeas);
        reportRange("endEdu", entry.endEduSource, target.range->endEdu);
    });
}

std::shared_ptr<musx::dom::others::EnigmaMusicRange> wholeDocumentRange(const musx::dom::DocumentPtr& document)
{
    auto result = std::make_shared<musx::dom::others::EnigmaMusicRange>(document);
    result->startMeas = 1;
    result->endMeas = static_cast<musx::dom::MeasCmper>((std::numeric_limits<std::int16_t>::max)());
    result->endEdu = (std::numeric_limits<musx::dom::Edu>::max)();
    return result;
}

std::shared_ptr<musx::dom::others::EnigmaMusicRange> copyRange(
    const musx::dom::DocumentPtr& document, const std::shared_ptr<musx::dom::others::EnigmaMusicRange>& source)
{
    auto result = std::make_shared<musx::dom::others::EnigmaMusicRange>(document);
    if (source) {
        result->startMeas = source->startMeas;
        result->startEdu = source->startEdu;
        result->endMeas = source->endMeas;
        result->endEdu = source->endEdu;
    }
    return result;
}

StaffUsedEntry readStaffUsedEntry(const ImportContext& context, const RecordFamilySource& source, std::span<const records::LegacyRow> rows,
    std::span<const std::uint8_t> payload, std::size_t sourceByteBase, musx::dom::Cmper cmper, musx::dom::Inci inci, bool earlyLayout,
    bool storesRange)
{
    const auto word = [&](std::size_t offset) { return payloadWord(payload, offset, context.profile.byteOrder); };
    const auto signedLong = [&](std::size_t offset) {
        return payloadLong(payload, offset, context.profile.byteOrder, nativeLongWordOrder(context.profile.byteOrder));
    };
    const auto staffId = static_cast<musx::dom::StaffCmper>(word(staffIdOffset));
    const auto rawDistance = earlyLayout ? static_cast<std::int16_t>(word(earlyDistanceOffset)) : signedLong(distanceOffset);
    auto target = createOthersRecordTarget<StaffUsedTarget>(context.document, source, rows.front(), cmper, inci);
    target->staffId = staffId;
    target->distFromTop = rawDistance;
    target->range = wholeDocumentRange(context.document);

    StaffUsedEntry result{target, sourcePosition(source, rows, sourceByteBase + staffIdOffset, staffId),
        sourcePosition(source, rows, sourceByteBase + (earlyLayout ? earlyDistanceOffset : distanceOffset), rawDistance)};
    if (storesRange) {
        target->range->startMeas = static_cast<musx::dom::MeasCmper>(word(rangeStartMeasOffset));
        target->range->startEdu = signedLong(rangeStartEduOffset);
        target->range->endMeas = static_cast<musx::dom::MeasCmper>(word(rangeEndMeasOffset));
        target->range->endEdu = signedLong(rangeEndEduOffset);
        result.startMeasSource = sourcePosition(source, rows, sourceByteBase + rangeStartMeasOffset, target->range->startMeas);
        result.startEduSource = sourcePosition(source, rows, sourceByteBase + rangeStartEduOffset, target->range->startEdu);
        result.endMeasSource = sourcePosition(source, rows, sourceByteBase + rangeEndMeasOffset, target->range->endMeas);
        result.endEduSource = sourcePosition(source, rows, sourceByteBase + rangeEndEduOffset, target->range->endEdu);
    }
    return result;
}

StaffUsedListSource staffUsedListSource(const ImportContext& context, const std::vector<StaffUsedEntry>& entries)
{
    const bool sourcePreservesListOrigin = sourceAtOrAfter(context.profile, FormatEpoch::DclLegacy);
    std::optional<StaffUsedSourcePosition> normalizationSource;
    if (!sourcePreservesListOrigin) {
        const auto normalizationEntry = std::ranges::max_element(entries, {}, [](const auto& entry) { return entry.target->distFromTop; });
        normalizationSource = (*normalizationEntry).distanceSource;
        normalizationSource->rawValue = (*normalizationEntry).target->distFromTop;
    }
    return {normalizationSource};
}

void publishStaffUsedList(const ImportContext& context, std::vector<StaffUsedEntry> entries, const StaffUsedListSources& listSources)
{
    if (entries.empty()) {
        return;
    }
    const bool normalize = !sourceAtOrAfter(context.profile, FormatEpoch::DclLegacy);
    const ListKey key{entries.front().target->getSourcePartId(), entries.front().target->getCmper()};
    const auto foundSource = listSources.find(key);
    const auto normalizationSource = foundSource == listSources.end() ? std::nullopt : foundSource->second.normalization;
    const auto normalization = normalizationSource ? static_cast<musx::dom::Evpu>(normalizationSource->rawValue) : 0;
    for (auto& entry : entries) {
        if (normalize) {
            entry.target->distFromTop -= normalization;
        }
        reportSourceStaffUsed(context, entry, normalize);
        context.document->getOthers()->add(StaffUsedTarget::XmlNodeName, std::move(entry.target));
    }
}

void importStaffUsedFamily(const ImportContext& context, const RecordFamilySource& source, std::vector<std::vector<StaffUsedEntry>>& lists)
{
    const bool storesRange = source.classRecords || source.identity == rangedStaffUsedTag;
    const bool earlyLayout = !storesRange && sourcePredatesVersion(context.profile, FormatEpoch::UncompressedLegacy, versions::finale3_5);
    const std::size_t elementBytes = earlyLayout ? earlyElementBytes : storesRange ? rangedElementBytes : rowBytes;
    for (const auto& [partId, cmper] : recordKeys(source)) {
        const auto rows = source.pool->getArray(source.identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto payload = collectRecordPayload(source, rows);
        const auto targetCmper = modernStaffUsedCmper(context, cmper);
        std::vector<StaffUsedEntry> entries;
        const auto completeSize = payload.size() - payload.size() % elementBytes;
        for (std::size_t offset = 0; offset < completeSize; offset += elementBytes) {
            const auto rowCount = source.classRecords ? 1 : (std::max)(std::size_t(1), elementBytes / rowBytes);
            const auto firstRow = std::span<const records::LegacyRow>(&source.rowOfByte(rows, offset), rowCount);
            auto entry = readStaffUsedEntry(context, source, firstRow, std::span(payload).subspan(offset, elementBytes),
                source.classRecords ? offset
                : earlyLayout       ? offset % rowBytes
                                    : 0,
                targetCmper, static_cast<musx::dom::Inci>(entries.size()), earlyLayout, storesRange);
            if (entry.target->staffId != 0) {
                entries.push_back(std::move(entry));
            }
        }
        if (completeSize != payload.size()) {
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Info, "StaffUsed list " + std::to_string(cmper) + " ends with an incomplete logical element."});
        }
        if (!entries.empty()) {
            lists.push_back(std::move(entries));
        }
    }
}

std::optional<StaffUsedSourcePosition> normalizationForList(
    const StaffUsedListSources& sources, const musx::dom::MusxInstanceList<StaffUsedTarget>& list)
{
    if (list.empty()) {
        return std::nullopt;
    }
    const auto found = sources.find({list.front()->getSourcePartId(), list.front()->getCmper()});
    return found == sources.end() ? std::nullopt : found->second.normalization;
}

void reportSynthesizedStaffUsed(const ImportContext& context, const StaffUsedTarget& target)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<StaffUsedTarget>(target.getSourcePartId(), target.getCmper(), target.getInci());
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyBehavior);
        const auto behavior = [&](const char* member, std::int64_t value) {
            reporting.report().setField(key, member, typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, value});
        };
        behavior("staffId", target.staffId);
        behavior("distFromTop", target.distFromTop);
        behavior("range.startMeas", target.range->startMeas);
        behavior("range.startEdu", target.range->startEdu);
        behavior("range.endMeas", target.range->endMeas);
        behavior("range.endEdu", target.range->endEdu);
    });
}

bool hasAuthoredStaffUsedList(const ImportContext& context, musx::dom::Cmper partId, musx::dom::Cmper cmper)
{
    const auto sources = context.document->getOthers()->getAllSources<StaffUsedTarget>(cmper);
    return std::ranges::any_of(sources, [&](const auto& source) { return source->getSourcePartId() == partId; });
}

bool hasCorrespondingStaffSystem(const ImportContext& context, const std::vector<StaffUsedEntry>& entries)
{
    if (entries.empty()) {
        return false;
    }
    const auto& first = *entries.front().target;
    const auto cmper = first.getCmper();
    if (cmper == musx::dom::BASE_SYSTEM_ID || cmper > static_cast<musx::dom::Cmper>((std::numeric_limits<musx::dom::StaffCmper>::max)())) {
        return true;
    }
    if (context.document->calcScrollViewCmper(first.getSourcePartId()) == cmper) {
        return true;
    }
    return context.document->getOthers()->get<StaffSystemTarget>(first.getSourcePartId(), cmper) != nullptr
           && context.pending.staffSystemsWithOwnStaffLists.contains({first.getSourcePartId(), cmper});
}

musx::dom::Evpu scaledNormalization(const StaffSystemTarget& system, const StaffUsedSourcePosition& normalizationSource)
{
    const auto normalization = system.scaleVert ? static_cast<double>(normalizationSource.rawValue) * system.ssysPercent / 100.0
                                                : static_cast<double>(normalizationSource.rawValue);
    return static_cast<musx::dom::Evpu>(std::lround(normalization));
}

void applyStaffUsedNormalizationToSystem(const ImportContext& context, StaffSystemTarget& system, const StaffUsedSourcePosition& normalizationSource,
    const std::optional<StaffUsedSourcePosition>& finaleUpgradeLossSource)
{
    const auto originalTop = system.top;
    system.top += scaledNormalization(system, normalizationSource);
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<StaffSystemTarget>(system.getSourcePartId(), system.getCmper());
        auto info = typename Reporting::FieldInfo{Reporting::Origin::LegacyMusAdjusted, normalizationSource.blockOffset,
            normalizationSource.decodedOffset, normalizationSource.rawValue, normalizationSource.identity};
        if (finaleUpgradeLossSource) {
            info.finaleUpgradeLossValue = originalTop + scaledNormalization(system, *finaleUpgradeLossSource);
        }
        reporting.report().setField(key, "top", std::move(info));
    });
}

void completeStaffUsedLists(const ImportContext& context, const StaffUsedListSources& listSources)
{
    std::set<musx::dom::Cmper> partsWithoutStaves;
    for (const auto& sourceSystem : context.document->getOthers()->getAllSources<StaffSystemTarget>()) {
        auto& system = *const_cast<StaffSystemTarget*>(sourceSystem.get());
        const auto partId = system.getSourcePartId();
        std::optional<StaffUsedSourcePosition> normalization;
        std::optional<StaffUsedSourcePosition> finaleUpgradeLossSource;
        if (hasAuthoredStaffUsedList(context, partId, system.getCmper())) {
            const auto systemList = context.document->getOthers()->getArray<StaffUsedTarget>(partId, system.getCmper());
            normalization = normalizationForList(listSources, systemList);
        } else {
            const auto templateCmper = context.document->calcScrollViewCmper(partId);
            const auto templateList = context.document->getOthers()->getArray<StaffUsedTarget>(partId, templateCmper);
            normalization = normalizationForList(listSources, templateList);
            if (templateCmper != musx::dom::BASE_SYSTEM_ID) {
                const auto staleSystem = listSources.find({partId, system.getCmper()});
                const auto baseSystem = listSources.find({partId, musx::dom::BASE_SYSTEM_ID});
                const auto alternate = staleSystem != listSources.end() ? staleSystem : baseSystem;
                if (alternate != listSources.end()) {
                    finaleUpgradeLossSource = alternate->second.normalization;
                }
            }
            const auto shareMode = partId == musx::dom::SCORE_PARTID ? musx::dom::EnigmaBase::ShareMode::All : musx::dom::EnigmaBase::ShareMode::None;
            for (const auto& source : templateList) {
                auto target =
                    std::make_shared<StaffUsedTarget>(context.document, partId, shareMode, system.getCmper(), source->getInci().value_or(0));
                target->staffId = source->staffId;
                target->distFromTop = source->distFromTop;
                target->range = copyRange(context.document, source->range);
                reportSynthesizedStaffUsed(context, *target);
                context.document->getOthers()->add(StaffUsedTarget::XmlNodeName, std::move(target));
            }
            if (templateList.empty()) {
                // A part with no staves has no list for any system; it is reported once below.
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Verbose, "Staff system " + std::to_string(system.getCmper()) + " has no StaffUsed list to copy."});
                partsWithoutStaves.insert(partId);
            }
        }
        if (normalization) {
            applyStaffUsedNormalizationToSystem(context, system, *normalization, finaleUpgradeLossSource);
        }
    }
    for (const auto partId : partsWithoutStaves) {
        context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
            partId == musx::dom::SCORE_PARTID ? "The document has no staves." : "Part " + std::to_string(partId) + " has no staves."});
    }
}

std::optional<RecordFamilySource> staffUsedSource(const ImportContext& context)
{
    if (context.profile.epoch == FormatEpoch::ZlibLegacy) {
        return RecordFamilySource{.pool = &context.index.getClassOthers(), .identity = staffUsedClass, .classRecords = true};
    }
    const auto& pool = context.index.getOthers();
    if (!pool.cmpersForTag(rangedStaffUsedTag).empty()) {
        return RecordFamilySource{.pool = &pool, .identity = rangedStaffUsedTag};
    }
    return RecordFamilySource{.pool = &pool, .identity = compactStaffUsedTag};
}

} // namespace

void importStaffUsed(const ImportContext& context)
{
    const auto source = staffUsedSource(context);
    if (!source) {
        return;
    }
    std::vector<std::vector<StaffUsedEntry>> lists;
    importStaffUsedFamily(context, *source, lists);
    context.pending.defer(DeferredStage::ResolveStoredRefs, [&context, lists = std::move(lists)]() mutable {
        StaffUsedListSources listSources;
        for (const auto& entries : lists) {
            if (!entries.empty()) {
                const ListKey key{entries.front().target->getSourcePartId(), entries.front().target->getCmper()};
                listSources.emplace(key, staffUsedListSource(context, entries));
            }
        }
        for (auto& entries : lists) {
            if (hasCorrespondingStaffSystem(context, entries)) {
                publishStaffUsedList(context, std::move(entries), listSources);
            }
        }
        completeStaffUsedLists(context, listSources);
    });
}

} // namespace others
} // namespace finale_mus_reader
