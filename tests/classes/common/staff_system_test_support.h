// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace classes {

constexpr std::size_t staffSystemFieldCount = 17;

inline ImportReport importStaffSystems(
    const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile, const musx::dom::DocumentPtr& document)
{
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importStaffSystems(context);
    finale_mus_reader::runDeferredChecks(pending);
    return report;
}

inline musx::dom::DocumentPtr emptyStaffSystemDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    return session.getDocument();
}

inline std::vector<std::int16_t> staffSystemWords(std::int16_t flags = 0x2017, std::int16_t staffHeight = 1536, bool extended = true)
{
    std::vector<std::int16_t> result{-12, 576, -13, -110, 4, flags, 9};
    result.insert(result.end(), {0, 12050});
    result.insert(result.end(), {70, -490, staffHeight});
    if (extended) {
        result.insert(result.end(), {24, 36});
    }
    return result;
}

} // namespace classes
} // namespace finale_mus_reader_tests
