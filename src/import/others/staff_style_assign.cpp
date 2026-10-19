// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using StaffStyleAssignTarget = musx::dom::others::StaffStyleAssign;
using StaffStyleAssignStaffTarget = musx::dom::others::Staff;

constexpr auto staffStyleAssignTag = records::packTag("Sy");
constexpr records::LegacyTag staffStyleAssignClass = 0x00e9;
constexpr std::size_t staffStyleAssignBytes = 24;
constexpr std::size_t staffStyleAssignStyleOffset = 0;
constexpr std::size_t staffStyleAssignStartMeasOffset = 12;
constexpr std::size_t staffStyleAssignStartEduOffset = 14;
constexpr std::size_t staffStyleAssignEndMeasOffset = 18;
constexpr std::size_t staffStyleAssignEndEduOffset = 20;

template <typename Reporting>
void reportStaffStyleAssignField(Reporting& reporting, const StaffStyleAssignTarget& target, const RecordFamilySource& source,
    std::span<const records::LegacyRow> rows, std::string member, std::size_t byteOffset, std::int64_t value)
{
    const auto& row = source.rowOfByte(rows, byteOffset);
    reporting.report().setField(reporting.template instanceKey<StaffStyleAssignTarget>(target.getSourcePartId(), target.getCmper(), target.getInci()),
        std::move(member),
        {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset + source.byteOffsetInRow(byteOffset), value, source.identity});
}

void reportStaffStyleAssign(const ImportContext& context, const RecordFamilySource& source, std::span<const records::LegacyRow> rows,
    const StaffStyleAssignTarget& target, std::size_t at)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<StaffStyleAssignTarget>(target.getSourcePartId(), target.getCmper(), target.getInci());
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        const auto report = [&](const char* member, std::size_t offset, std::int64_t value) {
            reportStaffStyleAssignField(reporting, target, source, rows, member, at + offset, value);
        };
        report("styleId", staffStyleAssignStyleOffset, target.styleId);
        report("startMeas", staffStyleAssignStartMeasOffset, target.startMeas);
        report("startEdu", staffStyleAssignStartEduOffset, target.startEdu);
        report("endMeas", staffStyleAssignEndMeasOffset, target.endMeas);
        report("endEdu", staffStyleAssignEndEduOffset, target.endEdu);
    });
}

void refreshStaffHasStyles(const ImportContext& context)
{
    for (const auto& staff : context.document->getOthers()->getAllSources<StaffStyleAssignStaffTarget>()) {
        const bool hasStyles = !context.document->getOthers()->getArray<StaffStyleAssignTarget>(staff->getSourcePartId(), staff->getCmper()).empty();
        const_cast<StaffStyleAssignStaffTarget*>(staff.get())->hasStyles = hasStyles;
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            const auto key = reporting.template instanceKey<StaffStyleAssignStaffTarget>(staff->getSourcePartId(), staff->getCmper());
            reporting.report().setField(key, "hasStyles", {Reporting::Origin::LegacyMusAdjusted, 0, 0, hasStyles});
        });
    }

    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        struct ConstructedCount
        {
            musx::dom::Cmper partId{};
            musx::dom::Cmper staffId{};
            std::size_t count{};
        };
        std::vector<ConstructedCount> constructed;
        for (const auto& assignment : context.document->getOthers()->getAllSources<StaffStyleAssignTarget>()) {
            const auto found = std::ranges::find_if(constructed,
                [&](const auto& value) { return value.partId == assignment->getSourcePartId() && value.staffId == assignment->getCmper(); });
            if (found == constructed.end()) {
                constructed.push_back({assignment->getSourcePartId(), assignment->getCmper(), 1});
            } else {
                ++found->count;
            }
        }

        auto& report = reporting.report();
        for (auto& audit : report.staffStyleAssignmentAudits) {
            const auto found =
                std::ranges::find_if(constructed, [&](const auto& value) { return value.partId == audit.partId && value.staffId == audit.staffId; });
            audit.constructedAssignments = found == constructed.end() ? 0 : found->count;
            if (audit.constructedAssignments != audit.expectedAssignments) {
                throw std::logic_error("Staff-style assignment import count does not match its "
                                       "source structure");
            }
        }
        for (const auto& value : constructed) {
            if (!report.findStaffStyleAssignmentAudit(value.partId, value.staffId)) {
                throw std::logic_error("A staff-style assignment importer did not report its source structure");
            }
        }
        report.staffStyleAssignmentAuditComplete = true;
    });
}

void importStaffStyleAssignFamily(const ImportContext& context, const RecordFamilySource& source)
{
    for (const auto& [partId, staffId] : recordKeys(source)) {
        const auto rows = source.pool->getArray(source.identity, staffId, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto payload = collectRecordPayload(source, rows);
        const bool malformedSource = payload.size() % staffStyleAssignBytes != 0;
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            reporting.report().expectStaffStyleAssignments(partId, staffId, payload.size() / staffStyleAssignBytes, malformedSource);
        });
        if (malformedSource) {
            context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
                "Staff style assignments for staff " + std::to_string(staffId) + " have an incomplete trailing tuple."});
        }
        for (std::size_t at = 0; at + staffStyleAssignBytes <= payload.size(); at += staffStyleAssignBytes) {
            const auto inci = static_cast<musx::dom::Inci>(at / staffStyleAssignBytes);
            auto target = createOthersRecordTarget<StaffStyleAssignTarget>(context.document, source, rows.front(), staffId, inci);
            if (!target) {
                continue;
            }
            const auto longOrder = nativeLongWordOrder(context.profile.byteOrder);
            target->styleId = payloadWord(payload, at + staffStyleAssignStyleOffset, context.profile.byteOrder);
            target->startMeas = payloadWord(payload, at + staffStyleAssignStartMeasOffset, context.profile.byteOrder);
            target->startEdu = payloadLong(payload, at + staffStyleAssignStartEduOffset, context.profile.byteOrder, longOrder);
            target->endMeas = payloadWord(payload, at + staffStyleAssignEndMeasOffset, context.profile.byteOrder);
            target->endEdu = payloadLong(payload, at + staffStyleAssignEndEduOffset, context.profile.byteOrder, longOrder);
            reportStaffStyleAssign(context, source, rows, *target, at);
            context.document->getOthers()->add(StaffStyleAssignTarget::XmlNodeName, std::move(target));
        }
    }
}

} // namespace

void importStaffStyleAssignments(const ImportContext& context)
{
    const auto source =
        selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), staffStyleAssignTag, staffStyleAssignClass);
    if (source) {
        importStaffStyleAssignFamily(context, *source);
    }
    context.pending.defer(DeferredStage::RefreshDerivedFlags, [&context] { refreshStaffHasStyles(context); });
}

} // namespace others
} // namespace finale_mus_reader
