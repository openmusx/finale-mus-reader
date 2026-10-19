// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include "class_test_support.h"

#include <functional>

namespace finale_mus_reader_tests {
namespace classes {

constexpr int evpusPerSpace = static_cast<int>(musx::dom::EVPU_PER_SPACE);
inline musx::dom::DocumentPtr emptyStaffDocument(musx::dom::Cmper musicFontId = 0)
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto fontOptions = std::make_shared<musx::dom::options::FontOptions>(document);
    auto musicFont = std::make_shared<musx::dom::FontInfo>(document);
    musicFont->fontId = musicFontId;
    fontOptions->fontOptions.emplace(musx::dom::options::FontOptions::FontType::Music, std::move(musicFont));
    auto noteheadFont = std::make_shared<musx::dom::FontInfo>(document);
    noteheadFont->fontSize = 24;
    fontOptions->fontOptions.emplace(musx::dom::options::FontOptions::FontType::Noteheads, std::move(noteheadFont));
    document->getOptions()->add(musx::dom::options::FontOptions::XmlNodeName, std::move(fontOptions));
    return document;
}

inline ImportReport staffImport(const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile,
    const musx::dom::DocumentPtr& document, bool styles = false, const std::function<void(const finale_mus_reader::ImportContext&)>& alsoImport = {})
{
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto referenceDocument = referenceSession.getDocument();
    auto referenceStaff = std::make_shared<musx::dom::others::Staff>(
        referenceDocument, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper{1});
    referenceStaff->staffLines = 5;
    referenceStaff->lineSpace = evpusPerSpace;
    referenceStaff->dwRestOffset = -4;
    referenceStaff->wRestOffset = -4;
    referenceStaff->hRestOffset = -4;
    referenceStaff->otherRestOffset = -4;
    referenceStaff->stemReversal = -4;
    referenceStaff->botRepeatDotOff = -5;
    referenceStaff->topRepeatDotOff = -3;
    referenceDocument->getOthers()->add(musx::dom::others::Staff::XmlNodeName, referenceStaff);
    auto referenceFontOptions = std::make_shared<musx::dom::options::FontOptions>(referenceDocument);
    auto referenceNoteheadFont = std::make_shared<musx::dom::FontInfo>(referenceDocument);
    referenceNoteheadFont->fontSize = 24;
    referenceFontOptions->fontOptions.emplace(musx::dom::options::FontOptions::FontType::Noteheads, std::move(referenceNoteheadFont));
    referenceDocument->getOptions()->add(musx::dom::options::FontOptions::XmlNodeName, std::move(referenceFontOptions));
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importStaff(context);
    if (styles) {
        finale_mus_reader::others::importStaffStyles(context);
    }
    if (alsoImport) {
        alsoImport(context);
    }
    finale_mus_reader::runDeferredChecks(pending);
    return report;
}

} // namespace classes
} // namespace finale_mus_reader_tests
