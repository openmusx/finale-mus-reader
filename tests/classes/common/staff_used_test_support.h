// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <limits>
#include <vector>

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace classes {

inline std::vector<std::int16_t> staffUsedWords(std::uint16_t staffId, std::int32_t distance, std::int16_t startMeas = 1, std::int32_t startEdu = 0,
    std::int16_t endMeas = 32767, std::int32_t endEdu = (std::numeric_limits<std::int32_t>::max)(), ByteOrder byteOrder = ByteOrder::BigEndian)
{
    const auto appendLong = [byteOrder](std::vector<std::int16_t>& result, std::int32_t value) {
        const auto high = static_cast<std::int16_t>(static_cast<std::uint32_t>(value) >> 16U);
        const auto low = static_cast<std::int16_t>(value);
        result.push_back(byteOrder == ByteOrder::BigEndian ? high : low);
        result.push_back(byteOrder == ByteOrder::BigEndian ? low : high);
    };
    std::vector<std::int16_t> result{static_cast<std::int16_t>(staffId), 0, 0, 0};
    appendLong(result, distance);
    result.push_back(startMeas);
    appendLong(result, startEdu);
    result.push_back(endMeas);
    appendLong(result, endEdu);
    return result;
}

struct StaffUsedImport
{
    musx::dom::DocumentPtr document;
    ImportReport report;
};

inline StaffUsedImport importStaffUsed(
    const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile, bool includeLayoutClasses = false)
{
    auto session = musx::factory::DocumentFactory::begin();
    auto document = session.getDocument();
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importStaffUsed(context);
    if (includeLayoutClasses) {
        finale_mus_reader::others::importStaffSystems(context);
        finale_mus_reader::others::importPartGlobals(context);
    }
    finale_mus_reader::runDeferredChecks(pending);
    return {std::move(document), std::move(report)};
}

} // namespace classes
} // namespace finale_mus_reader_tests
