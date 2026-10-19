// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <iterator>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using SystemLockTarget = musx::dom::others::SystemLock;

const FieldMapping systemLockFields[] = {
    MUS_WORD(SystemLockTarget, "FM", CMPER_FROM_TARGET, 0, 0, endMeas),
};

constexpr records::LegacyTag systemLockClass = 0x0093;

const FieldMapping systemLockClassFields[] = {
    MUS_CLASS_WORD(SystemLockTarget, systemLockClass, CMPER_FROM_TARGET, 0, endMeas),
};

const MappingTable& systemLockTable()
{
    static const MappingTable table{.reportPrefix = "others.lockMeas",
        .epochs = EpochMask::CodaBanner | EpochMask::FixedRow,
        .targetKind = TargetKind::OthersFromRecords,
        .recordIdentity = records::packTag("FM"),
        .createTarget = &createOthersTarget<SystemLockTarget>,
        .fields = systemLockFields,
        .fieldCount = std::size(systemLockFields)};
    return table;
}

const MappingTable& systemLockClassTable()
{
    static const MappingTable table{.reportPrefix = "others.lockMeas",
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OthersFromRecords,
        .recordIdentity = systemLockClass,
        .createTarget = &createOthersTarget<SystemLockTarget>,
        .fields = systemLockClassFields,
        .fieldCount = std::size(systemLockClassFields)};
    return table;
}

} // namespace

void importSystemLocks(const ImportContext& context)
{
    applyMappingTables({&systemLockTable(), &systemLockClassTable()}, context.index, context.profile, context.document, context.report);
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        for (const auto& lock : context.document->getOthers()->getAllSources<SystemLockTarget>()) {
            reporting.report().setInstanceOrigin(
                reporting.template instanceKey<SystemLockTarget>(lock->getSourcePartId(), lock->getCmper()), Reporting::Origin::LegacyMus);
        }
    });
}

} // namespace others
} // namespace finale_mus_reader
