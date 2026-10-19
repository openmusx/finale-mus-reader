// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"
#include "import/entries.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

TEST_CASE("Finale 2002 entry rows recover notes and TCD displacement", "[class][entry][fixture]")
{
    const auto baseline = readFixture("evidence/F2002/F2002-baseline.mus");
    const auto changed = readFixture("evidence/F2002/F2002-changed-C-to-D.mus");
    const auto first = baseline.document->getEntries()->get(1);
    const auto changedFirst = changed.document->getEntries()->get(1);
    REQUIRE(first);
    REQUIRE(changedFirst);
    CHECK(first->duration == 1024);
    CHECK(first->isNote);
    CHECK(first->numNotes == 1);
    REQUIRE(first->notes.size() == 1);
    REQUIRE(changedFirst->notes.size() == 1);
    CHECK(first->notes[0]->getNoteId() == 1);
    CHECK(first->notes[0]->harmLev == 0);
    CHECK(changedFirst->notes[0]->harmLev == 1);
    CHECK(changedFirst->notes[0]->harmAlt == 0);
    CHECK(changed.document->getEntries()->get(2));
    CHECK(changed.document->getEntries()->get(3));
}

TEST_CASE("Finale 2008 entry block retains the fixed row layout", "[class][entry][fixture]")
{
    const auto imported = readFixture("evidence/F2008/F2008-notehead-colors.mus");
    const auto first = imported.document->getEntries()->get(1);
    REQUIRE(first);
    CHECK(first->duration == 1024);
    CHECK(first->numNotes == 1);
    REQUIRE(first->notes.size() == 1);
    CHECK(first->notes[0]->getNoteId() == 1);
    CHECK(first->notes[0]->harmLev == 6);
}

TEST_CASE("Finale 2008 continuation rows retain all twelve notes", "[class][entry][fixture]")
{
    const auto imported = readFixture("evidence/F2008/F2008-maxnotes.mus");
    std::shared_ptr<const musx::dom::Entry> chord;
    for (std::uint32_t number = 1; number < 100; ++number) {
        const auto candidate = imported.document->getEntries()->get(number);
        if (candidate && candidate->numNotes == 12) {
            chord = candidate;
            break;
        }
    }
    REQUIRE(chord);
    REQUIRE(chord->notes.size() == 12);
    const std::array expectedIds{12, 11, 10, 9, 8, 7, 6, 1, 5, 4, 3, 2};
    for (std::size_t i = 0; i < chord->notes.size(); ++i) {
        CHECK(chord->notes[i]->getNoteId() == expectedIds[i]);
    }
}

TEST_CASE("Finale 1997 uncompressed entry rows recover notes", "[class][entry][fixture]")
{
    const auto imported = readFixture("evidence/F97/F97-altnotation.mus");
    const auto first = imported.document->getEntries()->get(1);
    REQUIRE(first);
    CHECK(first->duration == 1024);
    REQUIRE(first->notes.size() == 1);
    CHECK(first->notes[0]->harmLev == 7);
}

TEST_CASE("Coda entry rows recover note IDs and packed TCD", "[class][entry][fixture]")
{
    const auto imported = readFixture("evidence/F263/F263-maxnotes.mus");
    const auto first = imported.document->getEntries()->get(1);
    REQUIRE(first);
    CHECK(first->duration == 1024);
    CHECK(first->numNotes == 12);
    REQUIRE(first->notes.size() == 12);
    CHECK(first->sorted);
    CHECK(first->notes[0]->getNoteId() == 1);
    CHECK(first->notes[1]->getNoteId() == 2);
    CHECK(first->notes[1]->harmLev == 1);
    CHECK(first->getNext()->getEntryNumber() == 2);
    CHECK(first->notes[10]->getNoteId() == 11);
    CHECK(first->notes[11]->getNoteId() == 12);
    CHECK(first->notes[11]->harmLev == 11);
    CHECK_FALSE(imported.document->getEntries()->get(8));
}

TEST_CASE("Finale 2012 little-endian rows preserve entry links and note ID", "[class][entry][fixture]")
{
    const auto imported = readFixture("evidence/F2012/F2012-noteartexp.mus");
    const auto first = imported.document->getEntries()->get(1);
    REQUIRE(first);
    CHECK(first->duration == 1024);
    REQUIRE(first->notes.size() == 1);
    CHECK(first->notes[0]->getNoteId() == 1);
    CHECK(first->notes[0]->harmLev == 6);
    CHECK(first->getNext()->getEntryNumber() == 2);
}

TEST_CASE("A two-note fixed row retains both note tuples", "[class][entry][fixture]")
{
    const auto imported = readFixture("evidence/F372/F372-F263-F100-chord2.mus");
    const auto entry = imported.document->getEntries()->get(14);
    REQUIRE(entry);
    CHECK(entry->numNotes == 2);
    REQUIRE(entry->notes.size() == 2);
    CHECK(entry->notes[0]->harmLev == -1);
    CHECK(entry->notes[1]->harmLev == -1);
    CHECK(entry->notes[0]->getNoteId() == 31);
    CHECK(entry->notes[1]->getNoteId() == 31);
}

TEST_CASE("Entry continuation rows carry notes after the first two", "[class][entry]")
{
    finale_mus_reader::container::ParsedContainer parsed(FormatEpoch::UncompressedLegacy);
    parsed.byteOrder = ByteOrder::BigEndian;
    finale_mus_reader::container::DecodedBlock block;
    block.info.type = 0x0003;
    const std::array<std::uint8_t, 38> first = {
        0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0xc0, 0, 0, 0, 0, 0, 0, 3, 0, 0x29, 0x81, 1, 0, 0, 0, 0x69, 0x81, 2, 0, 0};
    const std::array<std::uint8_t, 38> continuation = {
        0, 0, 0, 1, 0, 1, 0, 0x89, 0x81, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    block.data.insert(block.data.end(), first.begin(), first.end());
    block.data.insert(block.data.end(), continuation.begin(), continuation.end());
    block.info.decodedSize = block.data.size();
    parsed.blocks.push_back(std::move(block));

    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::entries::importEntries(context);

    const auto entry = document->getEntries()->get(1);
    REQUIRE(entry);
    REQUIRE(entry->notes.size() == 3);
    CHECK(entry->notes[0]->harmLev == 2);
    CHECK(entry->notes[1]->harmLev == 6);
    CHECK(entry->notes[2]->harmLev == 8);
    CHECK(entry->notes[2]->getNoteId() == 3);
    const auto* thirdOrigin = report.findField<musx::dom::Note>("harmLev", 0, musx::dom::Cmper(0), musx::dom::Inci(3), musx::dom::Cmper(1));
    REQUIRE(thirdOrigin);
    CHECK(thirdOrigin->decodedOffset == 44);

    parsed.blocks[0].data[43] = 2;
    const auto invalidIndex = LegacyRecordIndex::build(parsed);
    auto invalidSession = musx::factory::DocumentFactory::begin();
    const auto invalidDocument = invalidSession.getDocument();
    ImportReport invalidReport(profile.epoch);
    const finale_mus_reader::ImportContext invalidContext{
        invalidIndex, profile, noSource, invalidDocument, reference, invalidReport, pending, construction};
    finale_mus_reader::entries::importEntries(invalidContext);
    CHECK_FALSE(invalidDocument->getEntries()->get(1));
    CHECK_FALSE(invalidReport.diagnostics.empty());
}

TEST_CASE("Entry continuation rows retain all twelve notes", "[class][entry]")
{
    finale_mus_reader::container::ParsedContainer parsed(FormatEpoch::DclLegacy);
    parsed.byteOrder = ByteOrder::BigEndian;
    finale_mus_reader::container::DecodedBlock block;
    block.info.type = 0x0011;
    const auto putWord = [](std::array<std::uint8_t, 38>& row, std::size_t offset, std::uint16_t value) {
        row[offset] = static_cast<std::uint8_t>(value >> 8U);
        row[offset + 1] = static_cast<std::uint8_t>(value);
    };
    for (std::size_t incidence = 0; incidence < 3; ++incidence) {
        std::array<std::uint8_t, 38> row{};
        putWord(row, 2, 1);
        putWord(row, 4, static_cast<std::uint16_t>(incidence));
        if (incidence == 0) {
            putWord(row, 14, 1024);
            putWord(row, 24, 12);
            row[18] = 0xea;
            row[19] = 0xc0;
            row[20] = 0xa9;
            row[21] = 0x43;
            putWord(row, 22, 0x03f4);
        }
        const auto firstNote = incidence == 0 ? 0 : 2 + (incidence - 1) * 5;
        const std::size_t capacity = incidence == 0 ? 2 : 5;
        for (std::size_t slot = 0; slot < capacity && firstNote + slot < 12; ++slot) {
            const auto offset = (incidence == 0 ? 26 : 6) + slot * 6;
            putWord(row, offset, static_cast<std::uint16_t>((firstNote + slot) << 4U));
            row[offset + 2] = 0x80;
            row[offset + 3] = static_cast<std::uint8_t>(firstNote + slot + 1);
            if (firstNote + slot == 0) {
                row[offset + 2] = 0xff;
                row[offset + 5] = 0x02;
            }
        }
        block.data.insert(block.data.end(), row.begin(), row.end());
    }
    block.info.decodedSize = block.data.size();
    parsed.blocks.push_back(std::move(block));

    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    profile.version = SourceVersion{.major = 9};
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::entries::importEntries(context);

    const auto entry = document->getEntries()->get(1);
    REQUIRE(entry);
    REQUIRE(entry->notes.size() == 12);
    CHECK(entry->v2Launch);
    CHECK(entry->createdByHP);
    CHECK(entry->playDisabledByHP);
    CHECK(entry->graceNote);
    CHECK(entry->noteDetail);
    CHECK(entry->isHidden);
    CHECK(entry->beam);
    CHECK(entry->crossStaff);
    CHECK(entry->checkAccis);
    CHECK(entry->sorted);
    CHECK(entry->slashGrace);
    CHECK(entry->flatBeam);
    CHECK(entry->notes[0]->tieStart);
    CHECK(entry->notes[0]->tieEnd);
    CHECK(entry->notes[0]->crossStaff);
    CHECK(entry->notes[0]->showAcci);
    CHECK(entry->notes[0]->freezeAcci);
    for (std::size_t i = 0; i < entry->notes.size(); ++i) {
        CHECK(entry->notes[i]->getNoteId() == i + 1);
        CHECK(entry->notes[i]->harmLev == static_cast<int>(i));
    }

    auto olderSession = musx::factory::DocumentFactory::begin();
    const auto olderDocument = olderSession.getDocument();
    SourceProfile olderProfile = profile;
    olderProfile.version = SourceVersion{.major = 8};
    ImportReport olderReport(olderProfile.epoch);
    const finale_mus_reader::ImportContext olderContext{index, olderProfile, noSource, olderDocument, reference, olderReport, pending, construction};
    finale_mus_reader::entries::importEntries(olderContext);
    const auto olderEntry = olderDocument->getEntries()->get(1);
    REQUIRE(olderEntry);
    CHECK_FALSE(olderEntry->createdByHP);
    CHECK_FALSE(olderEntry->playDisabledByHP);
}

TEST_CASE("Entry Note IDs fill zero slots with the lowest unused IDs and retain changed index mappings", "[class][entry]")
{
    using NoteNumber = musx::dom::NoteNumber;
    struct EntryCase
    {
        std::uint32_t number;
        std::uint32_t flags;
        std::vector<NoteNumber> ids;
    };
    const std::vector<EntryCase> cases{
        {1, 0xc0000000U, {0, 5, 2, 0}},
        {2, 0xc0000000U, {0, 0, 0}},
        {3, 0xc0000000U, {1, 2}},
        {4, 0x80000000U, {5}},
        {5, 0x80000000U, {0}},
        {6, 0x80000000U, {0, 1}},
        {7, 0x81000000U, {0, 1}},
        {8, 0x80000000U, {musx::dom::Note::RESTID}},
        {9, 0xc0000000U, {0}},
    };
    finale_mus_reader::container::ParsedContainer parsed(FormatEpoch::UncompressedLegacy);
    parsed.byteOrder = ByteOrder::BigEndian;
    finale_mus_reader::container::DecodedBlock block;
    block.info.type = 0x0003;
    const auto putWord = [](std::array<std::uint8_t, 38>& row, std::size_t offset, std::uint16_t value) {
        row[offset] = static_cast<std::uint8_t>(value >> 8U);
        row[offset + 1] = static_cast<std::uint8_t>(value);
    };
    const auto putLong = [&](std::array<std::uint8_t, 38>& row, std::size_t offset, std::uint32_t value) {
        putWord(row, offset, static_cast<std::uint16_t>(value >> 16U));
        putWord(row, offset + 2, static_cast<std::uint16_t>(value));
    };
    constexpr std::size_t maxIdsInTwoRows = 2 + (38 - 8) / 6;
    for (const auto& item : cases) {
        REQUIRE(item.ids.size() <= maxIdsInTwoRows);
        std::array<std::uint8_t, 38> first{};
        putLong(first, 0, item.number);
        putLong(first, 18, item.flags);
        putWord(first, 24, static_cast<std::uint16_t>(item.ids.size()));
        for (std::size_t index = 0; index < (std::min)(item.ids.size(), std::size_t(2)); ++index) {
            putLong(first, 28 + index * 6, 0x80000000U | (std::uint32_t(item.ids[index]) << 16U));
        }
        block.data.insert(block.data.end(), first.begin(), first.end());
        if (item.ids.size() > 2) {
            std::array<std::uint8_t, 38> continuation{};
            putLong(continuation, 0, item.number);
            putWord(continuation, 4, 1);
            for (std::size_t index = 2; index < (std::min)(item.ids.size(), maxIdsInTwoRows); ++index) {
                putLong(continuation, 8 + (index - 2) * 6, 0x80000000U | (std::uint32_t(item.ids[index]) << 16U));
            }
            block.data.insert(block.data.end(), continuation.begin(), continuation.end());
        }
    }
    block.info.decodedSize = block.data.size();
    parsed.blocks.push_back(std::move(block));
    const auto index = LegacyRecordIndex::build(parsed);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    ImportReport report(profile.epoch);
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::entries::importEntries(context);

    const auto noteIds = [&](std::uint32_t number) {
        std::vector<NoteNumber> ids;
        const auto entry = document->getEntries()->get(number);
        REQUIRE(entry);
        for (const auto& note : entry->notes) {
            ids.push_back(note->getNoteId());
        }
        return ids;
    };
    CHECK((noteIds(1) == std::vector<NoteNumber>{1, 5, 2, 3}));
    CHECK((noteIds(2) == std::vector<NoteNumber>{1, 2, 3}));
    CHECK((noteIds(3) == std::vector<NoteNumber>{1, 2}));
    CHECK((noteIds(4) == std::vector<NoteNumber>{musx::dom::Note::RESTID}));
    CHECK((noteIds(5) == std::vector<NoteNumber>{musx::dom::Note::RESTID}));
    CHECK((noteIds(6) == std::vector<NoteNumber>{0, 1}));
    CHECK((noteIds(7) == std::vector<NoteNumber>{0, 1}));
    CHECK((noteIds(8) == std::vector<NoteNumber>{musx::dom::Note::RESTID}));
    CHECK((noteIds(9) == std::vector<NoteNumber>{1}));
    CHECK(pending.noteIdsByEntryIndex.size() == 5);
    CHECK((pending.noteIdsByEntryIndex.at(1).original == std::vector<NoteNumber>{0, 5, 2, 0}));
    CHECK((pending.noteIdsByEntryIndex.at(1).final == std::vector<NoteNumber>{1, 5, 2, 3}));
    CHECK((pending.noteIdsByEntryIndex.at(2).final == std::vector<NoteNumber>{1, 2, 3}));
    CHECK((pending.noteIdsByEntryIndex.at(4).final == std::vector<NoteNumber>{musx::dom::Note::RESTID}));
    CHECK((pending.noteIdsByEntryIndex.at(5).final == std::vector<NoteNumber>{musx::dom::Note::RESTID}));
    using NoteAlterations = musx::dom::details::NoteAlterations;
    const auto makeDetail = [&](std::uint32_t entryNumber, NoteNumber sourceId) {
        auto detail = std::make_shared<NoteAlterations>(
            document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, entryNumber, musx::dom::Inci(0));
        detail->noteId = sourceId;
        finale_mus_reader::registerNoteIdRemap(context, detail, &NoteAlterations::noteId);
        return detail;
    };
    const auto changedZero = makeDetail(9, 0);
    const auto changedRest = makeDetail(4, 5);
    const auto ambiguous = makeDetail(1, 0);
    const auto unchanged = makeDetail(1, 5);
    pending.noteIdsByEntryIndex.insert_or_assign(10, finale_mus_reader::PendingReferences::NoteIdsByIndex{{5, 5}, {5, 6}});
    const auto changedAndUnchanged = makeDetail(10, 5);
    finale_mus_reader::runDeferredChecks(pending);
    CHECK(changedZero->noteId == 1);
    CHECK(changedRest->noteId == musx::dom::Note::RESTID);
    CHECK(ambiguous->noteId == 0);
    CHECK(unchanged->noteId == 5);
    CHECK(changedAndUnchanged->noteId == 5);
    const auto restWarnings = std::count_if(report.diagnostics.begin(), report.diagnostics.end(), [](const auto& diagnostic) {
        return diagnostic.level == musx::util::Logger::LogLevel::Warning
               && diagnostic.message.find("non-floating rest with multiple Notes") != std::string::npos;
    });
    CHECK(restWarnings == 1);
}

TEST_CASE("Coda Note ID synthesis records every index of a changed Entry", "[class][entry]")
{
    finale_mus_reader::container::ParsedContainer parsed(FormatEpoch::CodaBanner);
    parsed.byteOrder = ByteOrder::BigEndian;
    finale_mus_reader::container::DecodedBlock block;
    block.info.type = 0x0003;
    const auto putLong = [](std::array<std::uint8_t, 32>& row, std::size_t offset, std::uint32_t value) {
        row[offset] = static_cast<std::uint8_t>(value >> 24U);
        row[offset + 1] = static_cast<std::uint8_t>(value >> 16U);
        row[offset + 2] = static_cast<std::uint8_t>(value >> 8U);
        row[offset + 3] = static_cast<std::uint8_t>(value);
    };
    std::array<std::uint8_t, 32> blank{};
    std::array<std::uint8_t, 32> first{};
    putLong(first, 12, 2);
    putLong(first, 20, 0xc0000000U);
    putLong(first, 24, 0x80000000U);
    putLong(first, 28, 0x80050000U);
    std::array<std::uint8_t, 32> continuation{};
    putLong(continuation, 8, 1);
    putLong(continuation, 20, 0xc0000000U);
    putLong(continuation, 24, 0x80020000U);
    putLong(continuation, 28, 0x80000000U);
    std::array<std::uint8_t, 32> rest{};
    putLong(rest, 20, 0x80000000U);
    putLong(rest, 24, 0x80000000U);
    for (const auto& row : {blank, first, continuation, rest}) {
        block.data.insert(block.data.end(), row.begin(), row.end());
    }
    block.info.decodedSize = block.data.size();
    parsed.blocks.push_back(std::move(block));
    const auto index = LegacyRecordIndex::build(parsed);
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    ImportReport report(profile.epoch);
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::entries::importEntries(context);

    const auto chord = document->getEntries()->get(1);
    REQUIRE(chord);
    REQUIRE(chord->notes.size() == 4);
    CHECK(chord->notes[0]->getNoteId() == 1);
    CHECK(chord->notes[1]->getNoteId() == 5);
    CHECK(chord->notes[2]->getNoteId() == 2);
    CHECK(chord->notes[3]->getNoteId() == 3);
    const auto singleRest = document->getEntries()->get(3);
    REQUIRE(singleRest);
    REQUIRE(singleRest->notes.size() == 1);
    CHECK(singleRest->notes[0]->getNoteId() == musx::dom::Note::RESTID);
    CHECK(pending.noteIdsByEntryIndex.size() == 2);
    CHECK((pending.noteIdsByEntryIndex.at(1).final == std::vector<musx::dom::NoteNumber>{1, 5, 2, 3}));
    CHECK((pending.noteIdsByEntryIndex.at(3).final == std::vector<musx::dom::NoteNumber>{musx::dom::Note::RESTID}));
}

} // namespace
} // namespace finale_mus_reader_tests
