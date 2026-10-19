// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "import/support/legacy_mapping.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using PartVoicingTarget = musx::dom::others::PartVoicing;

constexpr records::LegacyTag partVoicingClass = 0x0121;
constexpr std::size_t partVoicingPayloadSize = 12;

void reportPartVoicing(const ImportContext& context, const PartVoicingTarget& instance, const records::LegacyRow& row)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<PartVoicingTarget>(instance.getSourcePartId(), instance.getCmper());
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        const auto reportField = [&](const char* member, std::size_t offset, std::int64_t value) {
            reporting.report().setField(key, member,
                typename Reporting::FieldInfo{Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset + offset, value, partVoicingClass});
        };
        reportField("enabled", 0, instance.enabled);
        reportField("voicingType", 0, static_cast<int>(instance.voicingType));
        reportField("singleLayerVoiceType", 0, static_cast<int>(instance.singleLayerVoiceType));
        reportField("select1st", 0, instance.select1st);
        reportField("select2nd", 0, instance.select2nd);
        reportField("select3rd", 0, instance.select3rd);
        reportField("select4th", 0, instance.select4th);
        reportField("select5th", 0, instance.select5th);
        reportField("selectFromBottom", 0, instance.selectFromBottom);
        reportField("selectSingleNote", 0, instance.selectSingleNote);
        reportField("singleLayer", 2, instance.singleLayer);
        reportField("multiLayer", 4, instance.multiLayer);
    });
}

} // namespace

void importPartVoicing(const ImportContext& context)
{
    // Linked parts and their voicing records begin in the zlib epoch.
    if (!sourceMatches(context.profile, EpochMask::Zlib)) {
        return;
    }

    const RecordFamilySource source{.pool = &context.index.getClassOthers(), .identity = partVoicingClass, .classRecords = true};
    for (const auto& [partId, cmper] : recordKeys(source)) {
        if (partId == musx::dom::SCORE_PARTID) {
            continue;
        }
        const auto rows = source.pool->getArray(source.identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto& row = rows.front();
        const auto payload = source.pool->effectivePayloadOf(row);
        if (payload.size() < partVoicingPayloadSize) {
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Info, "Part voicing record for part " + std::to_string(partId) + " is shorter than its layout."});
            continue;
        }

        const auto flags = payloadWord(payload, 0, context.profile.byteOrder);
        if ((flags & 0x0003) > 1 || ((flags & 0x0038) >> 3) > 3) {
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Info, "Part voicing record for part " + std::to_string(partId) + " has an unknown selection rule."});
            continue;
        }

        auto instance = createOthersRecordTarget<PartVoicingTarget>(context.document, source, row, cmper);
        instance->enabled = (flags & 0x0004) != 0;
        instance->voicingType =
            (flags & 0x0003) == 1 ? PartVoicingTarget::VoicingType::UseMultipleLayers : PartVoicingTarget::VoicingType::UseSingleLayer;
        instance->singleLayerVoiceType = static_cast<PartVoicingTarget::SingleLayerVoiceType>((flags & 0x0038) >> 3);
        instance->select1st = (flags & 0x0040) != 0;
        instance->select2nd = (flags & 0x0080) != 0;
        instance->select3rd = (flags & 0x0100) != 0;
        instance->select4th = (flags & 0x0200) != 0;
        instance->select5th = (flags & 0x0400) != 0;
        instance->selectSingleNote = (flags & 0x1000) != 0;
        instance->selectFromBottom = (flags & 0x2000) != 0;
        instance->singleLayer = payloadWord(payload, 2, context.profile.byteOrder);
        instance->multiLayer = payloadWord(payload, 4, context.profile.byteOrder);

        reportPartVoicing(context, *instance, row);
        context.document->getOthers()->add(PartVoicingTarget::XmlNodeName, std::move(instance));
    }
}

} // namespace others
} // namespace finale_mus_reader
