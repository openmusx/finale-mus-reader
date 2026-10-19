// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using MultiStaffGroupIdTarget = musx::dom::others::MultiStaffGroupId;
using MultiStaffInstrumentGroupTarget = musx::dom::others::MultiStaffInstrumentGroup;

constexpr records::LegacyTag multiStaffInstrumentGroupClass = 0x0142;
constexpr records::LegacyTag multiStaffGroupIdClass = 0x0143;
constexpr std::size_t multiStaffRecordSize = records::otherWordCount * 2U;

RecordFamilySource multiStaffSource(const ImportContext& context, records::LegacyTag classId)
{
    return {.pool = &context.index.getClassOthers(), .identity = classId, .classRecords = true};
}

bool storesMultiStaffInstrumentGroups(const SourceProfile& profile)
{
    // Believed: these objects were introduced in Finale 2012. A zlib source whose version
    // cannot be recovered fails closed rather than interpreting an earlier use of either ID.
    return sourceAtOrAfter(profile, FormatEpoch::ZlibLegacy, versions::finale2012);
}

} // namespace

void importMultiStaffGroupIds(const ImportContext& context)
{
    if (!storesMultiStaffInstrumentGroups(context.profile)) {
        return;
    }
    const auto source = multiStaffSource(context, multiStaffGroupIdClass);
    for (const auto& [partId, cmper] : recordKeys(source)) {
        const auto rows = source.pool->getArray(source.identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto payload = source.pool->effectivePayloadOf(rows.front());
        if (payload.size() < multiStaffRecordSize) {
            continue;
        }
        auto target = createOthersRecordTarget<MultiStaffGroupIdTarget>(context.document, source, rows.front(), cmper);
        target->staffGroupId = payloadWord(payload, 0, context.profile.byteOrder);
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            const auto key = reporting.template instanceKey<MultiStaffGroupIdTarget>(partId, cmper);
            reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
            reporting.report().setField(key, "staffGroupId",
                {Reporting::Origin::LegacyMus, rows.front().blockOffset, rows.front().decodedOffset, target->staffGroupId, source.identity});
        });
        context.document->getOthers()->add(MultiStaffGroupIdTarget::XmlNodeName, std::move(target));
    }
}

void importMultiStaffInstrumentGroups(const ImportContext& context)
{
    if (!storesMultiStaffInstrumentGroups(context.profile)) {
        return;
    }
    const auto source = multiStaffSource(context, multiStaffInstrumentGroupClass);
    for (const auto& [partId, cmper] : recordKeys(source)) {
        const auto rows = source.pool->getArray(source.identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto payload = source.pool->effectivePayloadOf(rows.front());
        if (payload.size() < multiStaffRecordSize) {
            continue;
        }
        auto target = createOthersRecordTarget<MultiStaffInstrumentGroupTarget>(context.document, source, rows.front(), cmper);
        // Believed: the three leading words are the format's three available staff slots.
        for (std::size_t index = 0; index < 3; ++index) {
            const auto offset = index * 2U;
            const auto staff = static_cast<musx::dom::StaffCmper>(payloadWord(payload, offset, context.profile.byteOrder));
            if (staff == 0) {
                continue;
            }
            target->staffNums.push_back(staff);
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<MultiStaffInstrumentGroupTarget>(partId, cmper);
                reporting.report().setField(key, "staffNums[" + std::to_string(target->staffNums.size() - 1) + "]",
                    {Reporting::Origin::LegacyMus, rows.front().blockOffset, rows.front().decodedOffset + offset, staff, source.identity});
            });
        }
        if (target->staffNums.empty()) {
            continue;
        }
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            reporting.report().setInstanceOrigin(
                reporting.template instanceKey<MultiStaffInstrumentGroupTarget>(partId, cmper), Reporting::Origin::LegacyMus);
        });
        context.document->getOthers()->add(MultiStaffInstrumentGroupTarget::XmlNodeName, std::move(target));
    }
}

} // namespace others
} // namespace finale_mus_reader
