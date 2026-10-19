// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

using PercussionNoteCodeTestTarget = musx::dom::details::PercussionNoteCode;

musx::dom::DocumentPtr makePercussionNoteCodeDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    return session.getDocument();
}

ImportReport importPercussionNoteCodes(
    const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile, const musx::dom::DocumentPtr& document)
{
    ImportReport report(profile.epoch);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{
        LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::details::importPercussionNoteCodes(context);
    return report;
}

TEST_CASE("Finale 2010 percussion-note codes use five-word elements", "[class]")
{
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
        profile.byteOrder = byteOrder;
        const auto document = makePercussionNoteCodeDocument();
        const auto report = importPercussionNoteCodes(
            makeDetailClassContainer(0x1234, 0x5678, 0, {1, 14, 0, 0, 0, 3, 16, 0, 0, 0}, byteOrder, 0x0451), profile, document);

        constexpr musx::dom::EntryNumber entryNumber = 0x12345678;
        const auto first = document->getDetails()->get<PercussionNoteCodeTestTarget>(musx::dom::SCORE_PARTID, entryNumber, musx::dom::Inci(0));
        const auto second = document->getDetails()->get<PercussionNoteCodeTestTarget>(musx::dom::SCORE_PARTID, entryNumber, musx::dom::Inci(1));
        REQUIRE(first);
        CHECK(first->noteId == 1);
        CHECK(first->noteCode == 14);
        REQUIRE(second);
        CHECK(second->noteId == 3);
        CHECK(second->noteCode == 16);
        CHECK(reportedFieldCount(report) == 4);
        const auto firstKey = finale_mus_reader::instanceKey<PercussionNoteCodeTestTarget>(
            musx::dom::SCORE_PARTID, musx::dom::Cmper(0x1234), musx::dom::Inci(0), musx::dom::Cmper(0x5678));
        const auto secondKey = finale_mus_reader::instanceKey<PercussionNoteCodeTestTarget>(
            musx::dom::SCORE_PARTID, musx::dom::Cmper(0x1234), musx::dom::Inci(1), musx::dom::Cmper(0x5678));
        REQUIRE(report.findField(firstKey, "noteId"));
        REQUIRE(report.findField(secondKey, "noteCode"));
        CHECK(report.findField(firstKey, "noteId")->rawValue == 1);
        CHECK(report.findField(secondKey, "noteCode")->rawValue == 16);
    }
}

TEST_CASE("Percussion-note code rejects incomplete and diagnoses nonzero tails", "[class]")
{
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;

    const auto incompleteDocument = makePercussionNoteCodeDocument();
    const auto incompleteReport =
        importPercussionNoteCodes(makeDetailClassContainer(0, 42, 0, {1, 14, 0}, ByteOrder::LittleEndian, 0x0451), profile, incompleteDocument);
    CHECK(incompleteDocument->getDetails()->getAllSources<PercussionNoteCodeTestTarget>().empty());
    CHECK(incompleteReport.diagnostics.size() == 1);

    const auto tailDocument = makePercussionNoteCodeDocument();
    const auto tailReport =
        importPercussionNoteCodes(makeDetailClassContainer(0, 43, 0, {1, 14, 0, 7, 0}, ByteOrder::LittleEndian, 0x0451), profile, tailDocument);
    CHECK(tailDocument->getDetails()->get<PercussionNoteCodeTestTarget>(musx::dom::SCORE_PARTID, musx::dom::EntryNumber(43), musx::dom::Inci(0)));
    CHECK(tailReport.diagnostics.size() == 1);
}

TEST_CASE("Legacy percussion-note codes follow input keys and preserve note attachment", "[class][percussion]")
{
    using namespace finale_mus_reader;
    using Staff = musx::dom::others::Staff;
    using Code = PercussionNoteCodeTestTarget;
    int scenario = 0;
    SECTION("percussion") {}
    SECTION("ordinary staff")
    {
        scenario = 1;
    }
    SECTION("rest")
    {
        scenario = 2;
    }
    SECTION("unselected input key")
    {
        scenario = 3;
    }
    SECTION("stored code takes precedence")
    {
        scenario = 4;
    }
    SECTION("a custom key does not change input keys")
    {
        scenario = 5;
    }
    SECTION("a tonal center does not change input keys")
    {
        scenario = 6;
    }
    SECTION("non-MIDI output falls back to staff position")
    {
        scenario = 8;
    }
    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            const auto result = readFixture("evidence/F263/F263-dots.mus");
            const auto document = result.document;
            const auto entry = document->getEntries()->get(1);
            REQUIRE(entry);
            REQUIRE(entry->notes.size() == 2);
            const auto staff = document->getOthers()->get<Staff>(0, 1);
            REQUIRE(staff);
            addScrollViewPercussionStaves(document, {1});
            entry->notes[0]->harmLev = 6;
            entry->notes[0]->harmAlt = 0;
            entry->notes[1]->harmLev = 11;
            entry->notes[1]->harmAlt = scenario == 3 ? 1 : 0;
            if (scenario == 1) {
                const_cast<Staff*>(staff.get())->notationStyle = Staff::NotationStyle::Standard;
            }
            if (scenario == 2) {
                const_cast<musx::dom::Entry*>(entry.get())->isNote = false;
            }
            if (scenario == 5) {
                using namespace musx::dom;
                auto format = std::make_shared<musx::dom::others::KeyFormat>(document, Cmper(0), EnigmaBase::ShareMode::All, Cmper(2));
                format->semitones = 24;
                format->scaleTones = 7;
                document->getOthers()->add(musx::dom::others::KeyFormat::XmlNodeName, format);
                auto map = std::make_shared<musx::dom::others::KeyMapArray>(document, Cmper(0), EnigmaBase::ShareMode::All, Cmper(2));
                const std::array<int, 7> naturalSteps{0, 4, 8, 10, 14, 18, 22};
                for (int index = 0; index < 24; ++index) {
                    auto step = std::make_shared<musx::dom::others::KeyMapArray::StepElement>();
                    const auto found = std::find(naturalSteps.begin(), naturalSteps.end(), index);
                    step->diatonic = found != naturalSteps.end();
                    step->hlevel = unsigned(std::distance(naturalSteps.begin(), found));
                    map->steps.push_back(step);
                }
                document->getOthers()->add(musx::dom::others::KeyMapArray::XmlNodeName, map);
                auto attributes = std::make_shared<musx::dom::others::KeyAttributes>(document, Cmper(0), EnigmaBase::ShareMode::All, Cmper(2));
                attributes->middleCKey = 60;
                attributes->gotoKey = 2;
                document->getOthers()->add(musx::dom::others::KeyAttributes::XmlNodeName, attributes);
                document->getOthers()->get<musx::dom::others::Measure>(0, 1)->globalKeySig->key = 0x0200;
            } else if (scenario == 6) {
                document->getOthers()->get<musx::dom::others::Measure>(0, 1)->globalKeySig->key = 1;
            }
            auto parsed = epoch == FormatEpoch::ZlibLegacy ? makeClassContainer(0x0084, {1, 0, 0, 0, 0, -32640}, byteOrder, 1)
                                                           : makeContainer({{1, "DS", {1, 0, 0, 0, 0, -32640}}}, epoch, byteOrder);
            for (const int input : {71, 79}) {
                const std::vector<std::int16_t> words{std::int16_t(scenario == 8 ? 255 : 54), std::int16_t(input == 71 ? 4 : 6), 207, 250, 0};
                auto detail = epoch == FormatEpoch::ZlibLegacy
                                  ? makeDetailClassContainer(1, static_cast<musx::dom::Cmper>(input), 0, words, byteOrder, 0x040e)
                                  : makeDetailContainer(epoch, 1, static_cast<musx::dom::Cmper>(input), words, "DF", byteOrder);
                parsed.blocks.push_back(std::move(detail.blocks.front()));
            }
            if (scenario == 4) {
                const std::vector<std::int16_t> words{std::int16_t(entry->notes[0]->getNoteId()), 99, 0, 0, 0};
                auto stored = epoch == FormatEpoch::ZlibLegacy ? makeDetailClassContainer(0, 1, 0, words, byteOrder, 0x0451)
                                                               : makeDetailContainer(epoch, 0, 1, words, "nC", byteOrder);
                parsed.blocks.push_back(std::move(stored.blocks.front()));
            }
            auto profile = SourceProfile(epoch);
            profile.byteOrder = byteOrder;
            if (epoch == FormatEpoch::ZlibLegacy) {
                profile.version = SourceVersion{.major = versions::finale2008.major};
            }
            ImportReport report(epoch);
            PendingReferences pending;
            musx::factory::ConstructionContext construction;
            const ImportContext context{LegacyRecordIndex::build(parsed), profile, noSource, document, document, report, pending, construction};
            details::importPercussionNoteCodes(context);
            others::importPercussionNoteInfo(context);
            others::importDrumStaff(context);
            runDeferredChecks(pending);
            const auto codes = document->getDetails()->getArray<Code>(0, musx::dom::EntryNumber(1));
            if (scenario == 1 || scenario == 2) {
                CHECK(codes.empty());
                CHECK(entry->notes[0]->harmLev == 6);
                CHECK(entry->notes[1]->harmLev == 11);
                continue;
            }
            if (scenario == 3) {
                REQUIRE(codes.size() == 1);
                CHECK(codes[0]->noteCode == 7);
                CHECK(entry->notes[1]->harmLev == 11);
                CHECK(entry->notes[1]->harmAlt == 1);
                continue;
            }
            CAPTURE(scenario, int(epoch), int(byteOrder));
            REQUIRE(codes.size() == 2);
            CHECK(codes[0]->noteId == entry->notes[0]->getNoteId());
            CHECK(codes[1]->noteId == entry->notes[1]->getNoteId());
            if (scenario == 8) {
                CHECK(codes[0]->noteCode == 4095);
                CHECK(codes[1]->noteCode == 8191);
                CHECK(entry->notes[0]->harmLev == 4);
                CHECK(entry->notes[1]->harmLev == 6);
                CHECK(entry->notes[0]->harmAlt == 0);
                CHECK(entry->notes[1]->harmAlt == 0);
                continue;
            }
            CHECK(codes[0]->noteCode == (scenario == 4 ? 99 : 7));
            CHECK(codes[1]->noteCode == 4103);
            for (const auto& note : entry->notes) {
                if (scenario == 4 && note == entry->notes[0]) {
                    CHECK(note->harmLev == 6);
                    CHECK(note->harmAlt == 0);
                    continue;
                }
                CHECK(note->harmLev == -4);
                CHECK(note->harmAlt == 1);
            }
            CHECK(entry->noteDetail);
            const auto key = instanceKey<Code>(musx::dom::Cmper(0), musx::dom::Cmper(0), musx::dom::Inci(0), musx::dom::Cmper(1));
            REQUIRE(report.findField(key, "noteCode"));
            CHECK(report.findField(key, "noteCode")->origin == (scenario == 4 ? ValueOrigin::LegacyMus : ValueOrigin::LegacyBehavior));
        }
    }
}

TEST_CASE("Percussion-note selectors are case sensitive", "[class]")
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::DclLegacy, FormatEpoch::UncompressedLegacy}) {
        for (const auto* selector : {"NC", "nC"}) {
            auto profile = SourceProfile(epoch);
            profile.byteOrder = ByteOrder::BigEndian;
            const auto document = makePercussionNoteCodeDocument();
            const auto report = importPercussionNoteCodes(makeDetailContainer(epoch, 0, 42, {1, 14, 0, 0, 0}, selector), profile, document);
            CHECK(document->getDetails()->getAllSources<PercussionNoteCodeTestTarget>().size() == (std::string_view(selector) == "nC" ? 1 : 0));
            CHECK(reportedFieldCount(report) == (std::string_view(selector) == "nC" ? 2 : 0));
        }
    }
}
} // namespace
} // namespace finale_mus_reader_tests
