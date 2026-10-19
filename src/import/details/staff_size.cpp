// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/details.h"

#include <cstdint>
#include <optional>
#include <string>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace details {
namespace {

using StaffSizeTarget = musx::dom::details::StaffSize;

constexpr auto staffSizeTag = records::packTag("LP");
constexpr records::LegacyTag staffSizeClass = 0x0410;

void reportStaffSize(const ImportContext& context, const StaffSizeTarget& target, const records::LegacyRow& row)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key =
            reporting.template instanceKey<StaffSizeTarget>(target.getSourcePartId(), target.getCmper1(), std::nullopt, target.getCmper2());
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        reporting.report().setField(
            key, "staffPercent", {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset, target.staffPercent, row.tag});
    });
}

void importStaffSizeFamily(const ImportContext& context, const RecordFamilySource& source)
{
    for (const auto& [partId, systemId] : recordKeys(source)) {
        for (const auto staffId : source.pool->secondCmpersForTag(source.identity, systemId, partId)) {
            const auto* row = source.pool->get(source.identity, systemId, staffId, 0, partId);
            if (!row) {
                continue;
            }
            const auto payload = source.pool->effectivePayloadOf(*row);
            if (payload.size() < sizeof(std::uint16_t)) {
                context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
                    "Staff-size detail for system " + std::to_string(systemId) + ", staff " + std::to_string(staffId) + " is truncated."});
                continue;
            }
            auto target = createDetailsRecordTarget<StaffSizeTarget>(context.document, source, *row, systemId, staffId);
            target->staffPercent = static_cast<std::int16_t>(payloadWord(payload, 0, context.profile.byteOrder));
            reportStaffSize(context, *target, *row);
            context.document->getDetails()->add(StaffSizeTarget::XmlNodeName, std::move(target));
        }
    }
}

} // namespace

void importStaffSizes(const ImportContext& context)
{
    // Believed: the single-incidence LP layout is unchanged across the fixed-row epochs.
    const auto source =
        selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), staffSizeTag, staffSizeClass, true);
    if (source) {
        importStaffSizeFamily(context, *source);
    }
}

} // namespace details
} // namespace finale_mus_reader
