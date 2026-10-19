// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <cstddef>
#include <string>
#include <utility>

#include "import/support/legacy_percussion.h"
#include "import/support/percussion_records.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using DrumStaffTarget = musx::dom::others::DrumStaff;
using DrumStaffStyleTarget = musx::dom::others::DrumStaffStyle;

constexpr std::size_t whichDrumLibOffset = 0;
constexpr std::size_t whichDrumLibSize = 2;

template <typename Reporting, typename Target>
void reportDrumStaffFamily(
    Reporting& reporting, const Target& target, const RecordFamilySource& source, const records::LegacyRow& row, musx::dom::Cmper storedValue)
{
    const auto key = reporting.template instanceKey<Target>(target.getSourcePartId(), target.getCmper());
    reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
    const auto origin = target.whichDrumLib == storedValue ? Reporting::Origin::LegacyMus : Reporting::Origin::LegacyMusAdjusted;
    reportLegacyField(reporting, key, source, row, "whichDrumLib", whichDrumLibOffset, storedValue, origin);
}

template <typename Target>
void addDrumStaffFamilyTarget(const ImportContext& context, const RecordFamilySource& source, const records::LegacyRow& row, musx::dom::Cmper cmper,
    musx::dom::Cmper storedValue, musx::dom::Cmper value)
{
    auto target = createOthersRecordTarget<Target>(context.document, source, row, cmper);
    if (!target) {
        return;
    }
    target->whichDrumLib = value;
    withReporting(
        context.report, [&]<typename Reporting>(Reporting& reporting) { reportDrumStaffFamily(reporting, *target, source, row, storedValue); });
    context.document->getOthers()->add(Target::XmlNodeName, std::move(target));
}

template <typename Target>
void importDrumStaffFamily(const ImportContext& context, const RecordFamilySource& source, std::string_view diagnosticName, bool legacyLayouts)
{
    for (const auto& [partId, cmper] : recordKeys(source)) {
        const auto rows = source.pool->getArray(source.identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto payload = collectRecordPayload(source, rows);
        if (payload.size() < whichDrumLibSize) {
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Info, std::string(diagnosticName) + " " + std::to_string(cmper) + " is shorter than its layout."});
            continue;
        }
        if (!legacyLayouts) {
            const auto value = payloadWord(payload, whichDrumLibOffset, context.profile.byteOrder);
            addDrumStaffFamilyTarget<Target>(context, source, rows.front(), cmper, value, value);
        }
    }
}

/// The legacy layout upgrade renumbers the layouts each assignment names and deletes an
/// assignment that selects nothing.
void importLegacyDrumStaffAssignments(const ImportContext& context)
{
    context.pending.defer(DeferredStage::NormalizeStoredValues, [&context] {
        for (const auto& [key, assignment] : percussion::legacyConversion(context).assignments) {
            if (!assignment.layoutId) {
                continue;
            }
            if (key.style) {
                addDrumStaffFamilyTarget<DrumStaffStyleTarget>(
                    context, assignment.source, assignment.row, key.cmper, assignment.storedLayoutId, *assignment.layoutId);
            } else {
                addDrumStaffFamilyTarget<DrumStaffTarget>(
                    context, assignment.source, assignment.row, key.cmper, assignment.storedLayoutId, *assignment.layoutId);
            }
        }
    });
}

} // namespace

void importDrumStaff(const ImportContext& context)
{
    // The Coda-banner epoch predates percussion maps.
    if (context.profile.epoch == FormatEpoch::CodaBanner) {
        return;
    }
    const bool legacyLayouts = percussion::storesLegacyLayouts(context);
    if (legacyLayouts) {
        importLegacyDrumStaffAssignments(context);
    }
    const auto drumStaffSource = selectRecordFamilySource(
        context, context.index.getOthers(), context.index.getClassOthers(), percussion_records::drumStaffTag, percussion_records::drumStaffClass);
    if (drumStaffSource) {
        importDrumStaffFamily<DrumStaffTarget>(context, *drumStaffSource, "Drum staff", legacyLayouts);
    }

    // Staff styles begin in Finale 2000.
    if (!sourceAtOrAfter(context.profile, FormatEpoch::UncompressedLegacy, versions::finale2000)) {
        return;
    }
    const auto drumStaffStyleSource = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(),
        percussion_records::drumStaffStyleTag, percussion_records::drumStaffStyleClass);
    if (drumStaffStyleSource) {
        importDrumStaffFamily<DrumStaffStyleTarget>(context, *drumStaffStyleSource, "Drum staff style", legacyLayouts);
    }
}

} // namespace others
} // namespace finale_mus_reader
