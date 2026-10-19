// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using PartVoicing = musx::dom::others::PartVoicing;

ImportReport partVoicingImport(const finale_mus_reader::container::ParsedContainer& parsed, const musx::dom::DocumentPtr& document)
{
    SourceProfile profile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = parsed.byteOrder;
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importPartVoicing(context);
    return report;
}

TEST_CASE("Part voicing recovers the controlled Finale 2008 edits")
{
    const auto base = readFixture("evidence/F2008/F2008-parts.mus");
    expect(base.document->getOthers()->getAllSources<PartVoicing>().empty(), "Parts without voicing acquired voicing objects");

    const auto voiced = readFixture("evidence/F2008/F2008-parts-voiced.mus");
    const auto first = voiced.document->getOthers()->get<PartVoicing>(1, 1);
    const auto second = voiced.document->getOthers()->get<PartVoicing>(2, 1);
    expect(first && second && voiced.document->getOthers()->getAllSources<PartVoicing>().size() == 2,
        "Both part scoped staff voicing records were not recovered");
    expect(first->enabled && first->voicingType == PartVoicing::VoicingType::UseMultipleLayers
               && first->singleLayerVoiceType == PartVoicing::SingleLayerVoiceType::TopNote,
        "The first part's top note rule was not recovered");
    expect(second->enabled && second->voicingType == PartVoicing::VoicingType::UseMultipleLayers
               && second->singleLayerVoiceType == PartVoicing::SingleLayerVoiceType::SelectedNotes && second->select2nd && second->select5th
               && second->selectFromBottom && second->singleLayer == 1 && second->multiLayer == 1,
        "The second part's selection and layer rules were not recovered");
    expect(!second->select1st && !second->select3rd && !second->select4th && !second->selectSingleNote,
        "The source record's clear flags were not preserved");
    expect(second->getShareMode() == musx::dom::EnigmaBase::ShareMode::None, "Voicing was incorrectly shared with the score");
    for (const auto* member : {"enabled", "voicingType", "singleLayerVoiceType", "select1st", "select2nd", "select3rd", "select4th", "select5th",
             "selectFromBottom", "selectSingleNote", "singleLayer", "multiLayer"}) {
        const auto* source = voiced.report.findField<PartVoicing>(member, 2, musx::dom::Cmper(1));
        expect(source && source->origin == ValueOrigin::LegacyMus, std::string("Unreported PartVoicing member: ") + member);
    }
    expect(reportedFieldCount(voiced.report) >= 24, "The part voicing field manifests are incomplete");
}

TEST_CASE("Part voicing is absent before linked parts")
{
    for (const auto* relative : {"evidence/F263/F263-baseline.mus", "evidence/F372/F372-baseline.mus", "evidence/F2002/F2002-empty.mus"}) {
        const auto result = readFixture(relative);
        expect(result.document->getOthers()->getAllSources<PartVoicing>().empty(), std::string("Pre-zlib voicing appeared in ") + relative);
    }
}

TEST_CASE("Part voicing rejects reserved selection rules and short records")
{
    for (const auto& words : {std::vector<std::int16_t>{6, 0, 0, 0, 0, 0}, std::vector<std::int16_t>{4, 0}}) {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        const auto parsed = makeClassContainer({SyntheticClassRow{0x0121, words, 1, 1}}, ByteOrder::LittleEndian);
        const auto report = partVoicingImport(parsed, document);
        expect(document->getOthers()->getAllSources<PartVoicing>().empty(), "An invalid voicing record created an object");
        expect(report.diagnostics.size() == 1, "An invalid voicing record was not diagnosed once");
    }
}

} // namespace
} // namespace finale_mus_reader_tests
