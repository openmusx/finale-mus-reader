// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace classes {

inline musx::dom::DocumentPtr staffSizeDocument(musx::dom::Cmper systemId)
{
    auto session = musx::factory::DocumentFactory::begin();
    auto document = session.getDocument();
    auto system =
        std::make_shared<musx::dom::others::StaffSystem>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, systemId);
    document->getOthers()->add(musx::dom::others::StaffSystem::XmlNodeName, std::move(system));
    return document;
}

inline ImportReport importStaffSizes(const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile,
    const musx::dom::DocumentPtr& document, bool expectInline = true)
{
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::details::importStaffSizes(context);
    CHECK(document->getDetails()->getAllSources<musx::dom::details::StaffSize>().empty() != expectInline);
    finale_mus_reader::runDeferredChecks(pending);
    return report;
}

} // namespace classes
} // namespace finale_mus_reader_tests
