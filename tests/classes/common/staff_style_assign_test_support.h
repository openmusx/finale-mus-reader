// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <limits>
#include <optional>

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace classes {

struct StaffStyleAssignImportResult
{
    musx::dom::DocumentPtr document;
    ImportReport report;
};

inline StaffStyleAssignImportResult importStaffStyleAssigns(const finale_mus_reader::container::ParsedContainer& parsed,
    bool addUnreportedAssignment = false, std::optional<SourceVersion> sourceVersion = std::nullopt)
{
    const auto index = LegacyRecordIndex::build(parsed);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto staff =
        std::make_shared<musx::dom::others::Staff>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper{4});
    document->getOthers()->add(musx::dom::others::Staff::XmlNodeName, std::move(staff));
    auto style = std::make_shared<musx::dom::others::StaffStyle>(
        document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper{7});
    document->getOthers()->add(musx::dom::others::StaffStyle::XmlNodeName, std::move(style));
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto referenceDocument = referenceSession.getDocument();
    auto referenceStaff = std::make_shared<musx::dom::others::Staff>(
        referenceDocument, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper{1});
    referenceStaff->lineSpace = 24;
    referenceStaff->dwRestOffset = -4;
    referenceStaff->wRestOffset = -4;
    referenceStaff->hRestOffset = -4;
    referenceStaff->otherRestOffset = -4;
    referenceStaff->stemReversal = -4;
    referenceStaff->botRepeatDotOff = -5;
    referenceStaff->topRepeatDotOff = -3;
    referenceDocument->getOthers()->add(musx::dom::others::Staff::XmlNodeName, std::move(referenceStaff));
    auto referenceFontOptions = std::make_shared<musx::dom::options::FontOptions>(referenceDocument);
    auto referenceNoteheadFont = std::make_shared<musx::dom::FontInfo>(referenceDocument);
    referenceNoteheadFont->fontSize = 24;
    referenceFontOptions->fontOptions.emplace(musx::dom::options::FontOptions::FontType::Noteheads, std::move(referenceNoteheadFont));
    referenceDocument->getOptions()->add(musx::dom::options::FontOptions::XmlNodeName, std::move(referenceFontOptions));
    const auto reference = std::move(referenceSession).finish();
    ImportReport report(parsed.formatEpoch);
    finale_mus_reader::PendingReferences pending;
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    profile.version = sourceVersion;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importStaffStyleAssignments(context);
    finale_mus_reader::details::importGFrameHolds(context);
    if (addUnreportedAssignment) {
        auto assignment = std::make_shared<musx::dom::others::StaffStyleAssign>(
            document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper{4}, musx::dom::Inci{0});
        document->getOthers()->add(musx::dom::others::StaffStyleAssign::XmlNodeName, std::move(assignment));
    }
    finale_mus_reader::runDeferredChecks(pending);
    return {document, std::move(report)};
}

} // namespace classes
} // namespace finale_mus_reader_tests
