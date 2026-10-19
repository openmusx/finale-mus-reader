// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <set>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using Accidental = musx::dom::details::AccidentalAlterations;
using Notehead = musx::dom::details::NoteAlterations;

musx::dom::DocumentPtr import(const finale_mus_reader::container::ParsedContainer& parsed, SourceProfile profile)
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    ImportReport report(profile.epoch);
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{
        LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::details::importAccidentalAlterations(context);
    finale_mus_reader::details::importNoteAlterations(context);
    finale_mus_reader::runDeferredChecks(pending);
    return document;
}

TEST_CASE("Combined notehead details split accidental displacement", "[class]")
{
    auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    const auto document = import(makeDetailContainer(profile.epoch, 0, 42, {1, 75, -3, 9, 32}, "CN"), profile);
    const auto note = document->getDetails()->get<Notehead>(0, musx::dom::EntryNumber(42), musx::dom::Inci(0));
    const auto accidental = document->getDetails()->get<Accidental>(0, musx::dom::EntryNumber(42), musx::dom::Inci(0));
    REQUIRE(note);
    REQUIRE(accidental);
    CHECK(note->noteId == 1);
    CHECK(note->percent == 75);
    CHECK(note->nxdisp == -3);
    CHECK(note->altNhead == 32);
    CHECK(accidental->noteId == 1);
    CHECK(accidental->percent == 100);
    CHECK(accidental->vOffset == 0);
    CHECK(accidental->hOffset == 9);
}

TEST_CASE("Default combined notehead alterations are omitted after normalization", "[class]")
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy}) {
        for (const auto percent : {0, 100}) {
            for (const auto accidentalOffset : {0, -52}) {
                auto profile = SourceProfile(epoch);
                profile.byteOrder = ByteOrder::BigEndian;
                const auto document =
                    import(makeDetailContainer(epoch, 0, 42, {1, std::int16_t(percent), 0, std::int16_t(accidentalOffset), 0}, "CN"), profile);
                CHECK(document->getDetails()->getAllSources<Notehead>().empty());
                const auto accidental = document->getDetails()->get<Accidental>(0, musx::dom::EntryNumber(42), musx::dom::Inci(0));
                if (accidentalOffset == 0) {
                    CHECK_FALSE(accidental);
                } else {
                    REQUIRE(accidental);
                    CHECK(accidental->percent == 100);
                    CHECK(accidental->hOffset == accidentalOffset);
                }
            }
        }
    }
}

TEST_CASE("Combined note alterations compact incidences after omitted records", "[class][notehead]")
{
    auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    const auto document =
        import(makeDetailContainer(profile.epoch, 0, 42, {1, 80, 0, 0, 0, 2, 100, 0, -52, 0, 3, 70, 0, -24, 0, 4, 0, 0, 0, 0, 5, 60, 0, 0, 0}, "CN"),
            profile);
    const auto details = document->getDetails();
    const auto entryNumber = musx::dom::EntryNumber(42);
    REQUIRE(details->getAllSources<Notehead>().size() == 3);
    REQUIRE(details->getAllSources<Accidental>().size() == 2);
    const std::array expectedNoteIds{1, 3, 5};
    for (std::size_t index = 0; index < expectedNoteIds.size(); ++index) {
        const auto alteration = details->get<Notehead>(0, entryNumber, static_cast<musx::dom::Inci>(index));
        REQUIRE(alteration);
        CHECK(alteration->noteId == expectedNoteIds[index]);
    }
    const std::array expectedAccidentalIds{2, 3};
    for (std::size_t index = 0; index < expectedAccidentalIds.size(); ++index) {
        const auto alteration = details->get<Accidental>(0, entryNumber, static_cast<musx::dom::Inci>(index));
        REQUIRE(alteration);
        CHECK(alteration->noteId == expectedAccidentalIds[index]);
    }
}

TEST_CASE("Finale 1 combined detail keeps accidental size independent of notehead size", "[class]")
{
    const auto baseline = readFixture("evidence/F100/F100-note.mus");
    CHECK(baseline.document->getDetails()->getAllSources<Notehead>().empty());
    CHECK(baseline.document->getDetails()->getAllSources<Accidental>().empty());

    const auto resized = readFixture("evidence/F100/F100-note-redux.mus");
    const auto resizedNote = resized.document->getDetails()->get<Notehead>(0, 1, musx::dom::Inci(0));
    REQUIRE(resizedNote);
    CHECK(resizedNote->percent == 79);
    CHECK(resized.document->getDetails()->getAllSources<Accidental>().empty());

    const auto displaced = readFixture("evidence/F100/F100-note-redux-accimv.mus");
    const auto displacedNote = displaced.document->getDetails()->get<Notehead>(0, 1, musx::dom::Inci(0));
    const auto displacedAccidental = displaced.document->getDetails()->get<Accidental>(0, 1, musx::dom::Inci(0));
    REQUIRE(displacedNote);
    REQUIRE(displacedAccidental);
    CHECK(displacedNote->percent == 79);
    CHECK(displacedAccidental->percent == 100);
    CHECK(displacedAccidental->hOffset == -52);
}

TEST_CASE("Finale 1 combined details resolve TCD references to Note IDs", "[class][fixture][notehead]")
{
    const auto imported = readFixture("evidence/F100/F100-chord-notealt.mus");
    const auto entry = imported.document->getEntries()->get(1);
    REQUIRE(entry);
    REQUIRE(entry->notes.size() == 3);
    const std::array expectedPercent{63, 51, 83};
    for (std::size_t index = 0; index < entry->notes.size(); ++index) {
        const auto noteId = static_cast<musx::dom::NoteNumber>(index + 1);
        CHECK(entry->notes[index]->getNoteId() == noteId);
        const auto alteration = imported.document->getDetails()->get<Notehead>(0, 1, static_cast<musx::dom::Inci>(index));
        REQUIRE(alteration);
        CHECK(alteration->noteId == noteId);
        CHECK(alteration->percent == expectedPercent[index]);
    }
}

// The Coda format flags state the reference form; the version plays no part. Finale 1.0.0 stores
// 0x00, the later Mac releases 0x03, and Windows documents 0x07.
TEST_CASE("Combined note detail reference mode follows the Coda format flags", "[class][notehead]")
{
    const auto modeFor = [](FormatEpoch epoch, std::uint8_t flags) {
        auto profile = SourceProfile(epoch);
        profile.codaFormatFlags = flags;
        return finale_mus_reader::combinedNoteReferenceMode(profile);
    };
    CHECK(modeFor(FormatEpoch::CodaBanner, 0x00) == finale_mus_reader::NoteReferenceMode::TcdOnly);
    CHECK(modeFor(FormatEpoch::CodaBanner, 0x01) == finale_mus_reader::NoteReferenceMode::TcdOnly);
    CHECK(modeFor(FormatEpoch::CodaBanner, 0x03) == finale_mus_reader::NoteReferenceMode::NoteIdOnly);
    CHECK(modeFor(FormatEpoch::CodaBanner, 0x07) == finale_mus_reader::NoteReferenceMode::NoteIdOnly);
    CHECK(modeFor(FormatEpoch::UncompressedLegacy, 0x00) == finale_mus_reader::NoteReferenceMode::NoteIdOnly);
}

TEST_CASE("Finale 2.6.3 combined details use stored Note IDs", "[class][fixture][notehead]")
{
    const auto imported = readFixture("evidence/F263/F263-chord-notemods.mus");
    const auto entry = imported.document->getEntries()->get(1);
    REQUIRE(entry);
    REQUIRE(entry->notes.size() == 3);
    for (std::size_t index = 0; index < entry->notes.size(); ++index) {
        const auto noteId = static_cast<musx::dom::NoteNumber>(index + 1);
        CHECK(entry->notes[index]->getNoteId() == noteId);
        const auto alteration = imported.document->getDetails()->get<Notehead>(0, 1, static_cast<musx::dom::Inci>(index));
        REQUIRE(alteration);
        CHECK(alteration->noteId == noteId);
    }
    CHECK(imported.document->getDetails()->get<Notehead>(0, 1, musx::dom::Inci(0))->percent == 67);
    CHECK(imported.document->getDetails()->get<Notehead>(0, 1, musx::dom::Inci(1))->percent == 71);
    CHECK(imported.document->getDetails()->get<Notehead>(0, 1, musx::dom::Inci(2))->percent == 83);
    const auto second = imported.document->getDetails()->get<Accidental>(0, 1, musx::dom::Inci(0));
    const auto third = imported.document->getDetails()->get<Accidental>(0, 1, musx::dom::Inci(1));
    REQUIRE(second);
    REQUIRE(third);
    CHECK(second->noteId == 2);
    CHECK(third->noteId == 3);
    CHECK(second->hOffset == -56);
    CHECK(third->hOffset == -24);
}

TEST_CASE("Split notehead details decode 2001 fields", "[class]")
{
    auto profile = SourceProfile(FormatEpoch::DclLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    const auto noteDocument = import(makeDetailContainer(profile.epoch, 0, 42, {2, 88, -4, 65, 6, 0, 18, 0, 7, 0}, "cn"), profile);
    const auto note = noteDocument->getDetails()->get<Notehead>(0, musx::dom::EntryNumber(42), musx::dom::Inci(0));
    REQUIRE(note);
    CHECK(note->noteId == 2);
    CHECK(note->percent == 88);
    CHECK(note->nxdisp == -4);
    CHECK(note->allowVertPos);
    CHECK(note->enharmonic);
    CHECK(note->customFont->fontSize == 18);
    CHECK(note->nydisp == 7);

    const auto accidentalDocument = import(makeDetailContainer(profile.epoch, 0, 43, {3, 92, 5, -6, 65, 0, 18, 0, 2, 0}, "Ah"), profile);
    const auto accidental = accidentalDocument->getDetails()->get<Accidental>(0, musx::dom::EntryNumber(43), musx::dom::Inci(0));
    REQUIRE(accidental);
    CHECK(accidental->noteId == 3);
    CHECK(accidental->percent == 92);
    CHECK(accidental->vOffset == 5);
    CHECK(accidental->hOffset == -6);
    CHECK(accidental->customFont->fontSize == 18);
    CHECK(accidental->allowVertPos);
}

TEST_CASE("Combined zero notehead percentage means normal size", "[class]")
{
    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        auto profile = SourceProfile(epoch);
        profile.byteOrder = ByteOrder::BigEndian;
        const auto parsed = epoch == FormatEpoch::UncompressedLegacy ? makeDetailContainer(epoch, 0, 42, {1, 0, -3, 0, 32}, "CN")
                                                                     : makeDetailContainer(epoch, 0, 42, {1, 0, -3, 65, 0, 0, 18, 0, 7, 0}, "cn");
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        auto referenceSession = musx::factory::DocumentFactory::begin();
        const auto reference = std::move(referenceSession).finish();
        ImportReport report(profile.epoch);
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{
            LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::details::importNoteAlterations(context);
        const auto note = document->getDetails()->get<Notehead>(0, musx::dom::EntryNumber(42), musx::dom::Inci(0));
        const auto percent = report.findField<Notehead>("percent", 0, musx::dom::Cmper(0), musx::dom::Inci(0), musx::dom::Cmper(42));
        REQUIRE(note);
        REQUIRE(percent);
        CHECK(note->percent == (epoch == FormatEpoch::UncompressedLegacy ? 100 : 0));
        CHECK(percent->rawValue == 0);
        CHECK(percent->origin == (epoch == FormatEpoch::UncompressedLegacy ? ValueOrigin::LegacyMusAdjusted : ValueOrigin::LegacyMus));
    }
}

TEST_CASE("Version 17 notehead uses the narrow character layout", "[class]")
{
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    profile.version = SourceVersion{.major = 17};
    const auto document = import(makeDetailClassContainer(0, 44, 0, {1, 0, 0, 119, 0, 12, 0, 0, 0, 0}, profile.byteOrder, 0x0422), profile);
    const auto note = document->getDetails()->get<Notehead>(0, musx::dom::EntryNumber(44), musx::dom::Inci(0));
    REQUIRE(note);
    CHECK(note->altNhead == 119);
    CHECK(note->customFont->fontId == 12);
    CHECK_FALSE(note->enharmonic);
}

TEST_CASE("Version 17 notehead preserves a Unicode character in the narrow word", "[class]")
{
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    profile.version = SourceVersion{.major = finale_mus_reader::versions::finale2012.major};
    const auto document = import(
        makeDetailClassContainer(0, 7, 0, {1, 100, 1, static_cast<std::int16_t>(0xfea4), 3, 15, 24, 0, -1, 0}, profile.byteOrder, 0x0422), profile);
    const auto note = document->getDetails()->get<Notehead>(0, musx::dom::EntryNumber(7), musx::dom::Inci(0));
    REQUIRE(note);
    CHECK(note->altNhead == 0xfea4);
    CHECK(note->useOwnFont);
    CHECK(note->allowVertPos);
    CHECK(note->customFont->fontId == 15);
    CHECK(note->customFont->fontSize == 24);
    CHECK(note->nydisp == -1);
}

TEST_CASE("Version 17 accidental uses the narrow character layout", "[class]")
{
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    profile.version = SourceVersion{.major = 17};
    const auto document = import(makeDetailClassContainer(0, 45, 0, {1, 90, 2, -3, 65, 12, 24, 0, 2, 0}, profile.byteOrder, 0x03ef), profile);
    const auto accidental = document->getDetails()->get<Accidental>(0, musx::dom::EntryNumber(45), musx::dom::Inci(0));
    REQUIRE(accidental);
    CHECK(accidental->altChar == 65);
    CHECK(accidental->customFont->fontId == 12);
    CHECK(accidental->customFont->fontSize == 24);
    CHECK(accidental->allowVertPos);
}

TEST_CASE("Version 17 accidental preserves a Unicode character in the narrow word", "[class]")
{
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    profile.version = SourceVersion{.major = finale_mus_reader::versions::finale2012.major};
    const auto document = import(
        makeDetailClassContainer(0, 5, 0, {1, 89, -5, 3, static_cast<std::int16_t>(0xfc0c), 15, 24, 0, 3, 0}, profile.byteOrder, 0x03ef), profile);
    const auto accidental = document->getDetails()->get<Accidental>(0, musx::dom::EntryNumber(5), musx::dom::Inci(0));
    REQUIRE(accidental);
    CHECK(accidental->altChar == 0xfc0c);
    CHECK(accidental->percent == 89);
    CHECK(accidental->vOffset == -5);
    CHECK(accidental->hOffset == 3);
    CHECK(accidental->customFont->fontId == 15);
    CHECK(accidental->customFont->fontSize == 24);
    CHECK(accidental->useOwnFont);
    CHECK(accidental->allowVertPos);
}

TEST_CASE("Combined notehead reports every target field and preserves raw values", "[class]")
{
    auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    const auto parsed = makeDetailContainer(profile.epoch, 0, 42, {1, 75, -3, 9, 32}, "CN");
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    ImportReport report(profile.epoch);
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{
        LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::details::importAccidentalAlterations(context);
    finale_mus_reader::details::importNoteAlterations(context);
    const auto noteId = report.findField<Notehead>("noteId", 0, musx::dom::Cmper(0), musx::dom::Inci(0), musx::dom::Cmper(42));
    const auto accidental = report.findField<Accidental>("hOffset", 0, musx::dom::Cmper(0), musx::dom::Inci(0), musx::dom::Cmper(42));
    const auto accidentalPercent = report.findField<Accidental>("percent", 0, musx::dom::Cmper(0), musx::dom::Inci(0), musx::dom::Cmper(42));
    const auto unresolved = report.findField<Notehead>("customFont.fontId", 0, musx::dom::Cmper(0), musx::dom::Inci(0), musx::dom::Cmper(42));
    REQUIRE(noteId);
    REQUIRE(accidental);
    REQUIRE(accidentalPercent);
    REQUIRE(unresolved);
    CHECK(noteId->rawValue == 1);
    CHECK(accidental->rawValue == 9);
    CHECK(accidental->origin == ValueOrigin::LegacyMus);
    CHECK(accidentalPercent->origin == ValueOrigin::LegacyBehavior);
    CHECK(accidentalPercent->rawValue == 100);
    CHECK(unresolved->origin == ValueOrigin::Unmapped);
}

template <typename Target>
void checkSingleIncidenceNoteDetail(const char* tag, std::uint16_t classId)
{
    using namespace finale_mus_reader;
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            for (const musx::dom::Cmper part : {musx::dom::Cmper(0), musx::dom::Cmper(2)}) {
                if (part && epoch != FormatEpoch::ZlibLegacy) {
                    continue;
                }
                auto profile = SourceProfile(epoch);
                profile.byteOrder = order;
                profile.version = SourceVersion{.major = 1};
                const std::vector<std::int16_t> words{5, 14, -12, 8, -1, 0, 9, 17, -3, 42};
                const auto parsed =
                    epoch == FormatEpoch::ZlibLegacy
                        ? makeDetailClassContainer(1, 42, part, words, order, classId)
                        : makeDetailContainer(epoch, 1, 42, words,
                              epoch == FormatEpoch::CodaBanner && std::is_same_v<Target, musx::dom::details::DotAlterations> ? "DO" : tag, order);
                const auto document = musx::factory::DocumentFactory::create<musx::xml::pugi::Document>(std::string_view(
                    R"xml(<finale><entries><entry entnum="65578" prev="0" next="0"><numNotes>2</numNotes><isNote/><note id="1"/><note id="2"/></entry></entries></finale>)xml"));
                ImportReport report(epoch);
                PendingReferences pending;
                musx::factory::ConstructionContext construction;
                const ImportContext context{LegacyRecordIndex::build(parsed), profile, noSource, document, document, report, pending, construction};
                if constexpr (std::is_same_v<Target, musx::dom::details::CrossStaff>) {
                    details::importCrossStaff(context);
                } else if constexpr (std::is_same_v<Target, musx::dom::details::PercussionNoteCode>) {
                    details::importPercussionNoteCodes(context);
                } else if constexpr (std::is_same_v<Target, musx::dom::details::TablatureNoteMods>) {
                    details::importTablatureNoteMods(context);
                } else {
                    details::importDotAlterations(context);
                }
                constexpr musx::dom::EntryNumber entry = 65578;
                const auto first = document->getDetails()->get<Target>(part, entry, musx::dom::Inci(0));
                const auto second = document->getDetails()->get<Target>(part, entry, musx::dom::Inci(1));
                REQUIRE(first);
                REQUIRE(second);
                CHECK(first->noteId == 5);
                CHECK(second->noteId == 0);
                std::set<std::string> expected{"noteId"};
                if constexpr (std::is_same_v<Target, musx::dom::details::CrossStaff>) {
                    CHECK(first->staff == 14);
                    CHECK(second->staff == 9);
                    expected.insert("staff");
                } else if constexpr (std::is_same_v<Target, musx::dom::details::PercussionNoteCode>) {
                    CHECK(first->noteCode == 14);
                    CHECK(second->noteCode == 9);
                    expected.insert("noteCode");
                } else if constexpr (std::is_same_v<Target, musx::dom::details::TablatureNoteMods>) {
                    CHECK(first->stringNumber == 14);
                    CHECK(second->stringNumber == 9);
                    expected.insert("stringNumber");
                } else {
                    CHECK(first->hOffset == 14);
                    CHECK(first->vOffset == -12);
                    CHECK(first->interdotSpacing == 8);
                    CHECK(second->vOffset == 17);
                    CHECK(second->interdotSpacing == -3);
                    expected.insert({"hOffset", "vOffset", "interdotSpacing"});
                }
                REQUIRE(report.fields.size() == 2);
                for (const auto& [key, fields] : report.fields) {
                    std::set<std::string> actual;
                    for (const auto& [name, info] : fields) {
                        actual.insert(name);
                        CHECK(info.origin == ValueOrigin::LegacyMus);
                    }
                    CHECK(actual == expected);
                }
                // Even a Coda reference matching a TCD must retain its Note ID.
                pending.noteTcdByEntryIndex[entry] = {5, 0};
                runDeferredChecks(pending);
                CHECK(first->noteId == 5);
                CHECK(second->noteId == 0);
            }
        }
    }
}

TEST_CASE("Single-incidence note details map every field in all four epochs", "[class][notehead]")
{
    checkSingleIncidenceNoteDetail<musx::dom::details::CrossStaff>("CD", 0x040c);
    checkSingleIncidenceNoteDetail<musx::dom::details::DotAlterations>("do", 0x040d);
    checkSingleIncidenceNoteDetail<musx::dom::details::PercussionNoteCode>("nC", 0x0451);
    checkSingleIncidenceNoteDetail<musx::dom::details::TablatureNoteMods>("GN", 0x042d);
}

template <typename Target>
void checkSingleNoteDetailRemap(const char* tag)
{
    using namespace finale_mus_reader;
    for (const auto& finalIds : {std::vector<musx::dom::NoteNumber>{5}, {6}, {musx::dom::Note::RESTID}, {5, 6}}) {
        auto profile = SourceProfile(FormatEpoch::DclLegacy);
        profile.byteOrder = ByteOrder::BigEndian;
        const auto parsed = makeDetailContainer(profile.epoch, 0, 42, {5, -30, 0, 0, 0}, tag);
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        ImportReport report(profile.epoch);
        PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const ImportContext context{LegacyRecordIndex::build(parsed), profile, noSource, document, document, report, pending, construction};
        if constexpr (std::is_same_v<Target, musx::dom::details::CrossStaff>) {
            details::importCrossStaff(context);
        } else if constexpr (std::is_same_v<Target, musx::dom::details::PercussionNoteCode>) {
            details::importPercussionNoteCodes(context);
        } else if constexpr (std::is_same_v<Target, musx::dom::details::TablatureNoteMods>) {
            details::importTablatureNoteMods(context);
        } else {
            details::importDotAlterations(context);
        }
        pending.noteIdsByEntryIndex[42] = {std::vector<musx::dom::NoteNumber>(finalIds.size(), 5), finalIds};
        runDeferredChecks(pending);
        const auto detail = document->getDetails()->get<Target>(0, musx::dom::EntryNumber(42), musx::dom::Inci(0));
        REQUIRE(detail);
        if constexpr (std::is_same_v<Target, musx::dom::details::PercussionNoteCode>) {
            CHECK(detail->noteCode == 65506);
        } else if constexpr (std::is_same_v<Target, musx::dom::details::TablatureNoteMods>) {
            CHECK(detail->stringNumber == -30);
        }
        const auto expected = finalIds.size() == 1 ? finalIds.front() : 5;
        CHECK(detail->noteId == expected);
        const auto info = report.findField<Target>("noteId", 0, musx::dom::Cmper(0), musx::dom::Inci(0), musx::dom::Cmper(42));
        REQUIRE(info);
        CHECK(info->rawValue == 5);
        CHECK(info->origin == (expected == 5 ? ValueOrigin::LegacyMus : ValueOrigin::LegacyMusAdjusted));
        CHECK(report.diagnostics.empty() == (finalIds.size() == 1));
    }
}

TEST_CASE("Single-incidence note details remap unique changed Note IDs and preserve ambiguity", "[class][notehead]")
{
    checkSingleNoteDetailRemap<musx::dom::details::CrossStaff>("CD");
    checkSingleNoteDetailRemap<musx::dom::details::DotAlterations>("do");
    checkSingleNoteDetailRemap<musx::dom::details::PercussionNoteCode>("nC");
    checkSingleNoteDetailRemap<musx::dom::details::TablatureNoteMods>("GN");
}

TEST_CASE("Pre-zlib cross-staff dots clear vertical offsets after Note ID remapping without dropping incidences", "[class][notehead]")
{
    using namespace finale_mus_reader;
    using Target = musx::dom::details::DotAlterations;
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto order : {DeferredOrder::AsRegistered, DeferredOrder::ReversedWithinStage}) {
            SourceProfile profile(epoch);
            profile.byteOrder = ByteOrder::BigEndian;
            const std::vector<std::int16_t> words{1, 0, -12, 0, 0, 2, 0, 8, 0, 0, 3, 0, 0, 0, 0, 5, 3, 17, 9, 0, 7, 0, 13, 0, 0};
            const auto parsed = epoch == FormatEpoch::ZlibLegacy
                                    ? makeDetailClassContainer(0, 42, 0, words, profile.byteOrder, 0x040d)
                                    : makeDetailContainer(epoch, 0, 42, words, epoch == FormatEpoch::CodaBanner ? "DO" : "do");
            const auto document = musx::factory::DocumentFactory::create<musx::xml::pugi::Document>(std::string_view(R"xml(
                <finale><entries><entry entnum="42" prev="0" next="0"><numNotes>5</numNotes><isNote/>
                <note id="1"><crossStaff/></note><note id="2"/><note id="3"><crossStaff/></note>
                <note id="4"><crossStaff/></note><note id="6"><crossStaff/></note>
                </entry></entries></finale>)xml"));
            ImportReport report(epoch);
            PendingReferences pending;
            musx::factory::ConstructionContext construction;
            const ImportContext context{LegacyRecordIndex::build(parsed), profile, noSource, document, document, report, pending, construction};
            details::importDotAlterations(context);
            pending.noteIdsByEntryIndex[42] = {{1, 2, 3, 4, 5}, {1, 2, 3, 4, 6}};
            const auto first = document->getDetails()->get<Target>(0, musx::dom::EntryNumber(42), musx::dom::Inci(0));
            REQUIRE(first);
            CHECK(first->vOffset == -12);
            runDeferredChecks(pending, order);
            REQUIRE(document->getDetails()->getAllSources<Target>().size() == 5);
            const std::array expectedIds{1, 2, 3, 6, 7};
            const std::array expectedVertical{epoch == FormatEpoch::ZlibLegacy ? -12 : 0, 8, 0, epoch == FormatEpoch::ZlibLegacy ? 17 : 0, 13};
            for (std::size_t i = 0; i < expectedIds.size(); ++i) {
                const auto detail = document->getDetails()->get<Target>(0, musx::dom::EntryNumber(42), musx::dom::Inci(i));
                REQUIRE(detail);
                CHECK(detail->noteId == expectedIds[i]);
                CHECK(detail->vOffset == expectedVertical[i]);
                CHECK(detail->hOffset == (i == 3 ? 3 : 0));
                CHECK(detail->interdotSpacing == (i == 3 ? 9 : 0));
                const auto info = report.findField<Target>("vOffset", 0, musx::dom::Cmper(0), musx::dom::Inci(i), musx::dom::Cmper(42));
                REQUIRE(info);
                CHECK(info->rawValue == words[i * 5 + 2]);
                const bool adjusted = epoch != FormatEpoch::ZlibLegacy && (i == 0 || i == 3);
                CHECK(info->origin == (adjusted ? ValueOrigin::LegacyMusAdjusted : ValueOrigin::LegacyMus));
            }
        }
    }
}

TEST_CASE("Dot detail fixture preserves the stored rest Note ID", "[class][fixture][notehead]")
{
    const auto result = readFixture("evidence/F2008/F2008-F372-F263-F100-msnm-enclevr.mus");
    const auto detail = result.document->getDetails()->get<musx::dom::details::DotAlterations>(0, musx::dom::EntryNumber(14), musx::dom::Inci(0));
    REQUIRE(detail);
    CHECK(detail->noteId == musx::dom::Note::RESTID);
    CHECK(detail->hOffset == 0);
    CHECK(detail->vOffset == 0);
    CHECK(detail->interdotSpacing == 0);
}

TEST_CASE("Single-incidence note details preserve complete tuples before a truncated tail", "[class][notehead]")
{
    using namespace finale_mus_reader;
    for (const auto classId : {std::uint16_t(0x040c), std::uint16_t(0x040d), std::uint16_t(0x042d), std::uint16_t(0x0451)}) {
        for (const auto& words : {std::vector<std::int16_t>{}, {1, 2}, {1, 2, 0, 0, 0, 2, 3}}) {
            auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
            profile.byteOrder = ByteOrder::LittleEndian;
            const auto parsed = makeDetailClassContainer(0, 42, 0, words, profile.byteOrder, classId);
            auto session = musx::factory::DocumentFactory::begin();
            const auto document = session.getDocument();
            ImportReport report(profile.epoch);
            PendingReferences pending;
            musx::factory::ConstructionContext construction;
            const ImportContext context{LegacyRecordIndex::build(parsed), profile, noSource, document, document, report, pending, construction};
            details::importCrossStaff(context);
            details::importDotAlterations(context);
            details::importTablatureNoteMods(context);
            details::importPercussionNoteCodes(context);
            CHECK(
                document->getDetails()->getAllSources<musx::dom::details::TablatureNoteMods>().size() == (classId == 0x042d ? words.size() / 5 : 0));
            CHECK(
                document->getDetails()->getAllSources<musx::dom::details::PercussionNoteCode>().size() == (classId == 0x0451 ? words.size() / 5 : 0));
            CHECK(document->getDetails()->getAllSources<musx::dom::details::CrossStaff>().size() == (classId == 0x040c ? words.size() / 5 : 0));
            CHECK(document->getDetails()->getAllSources<musx::dom::details::DotAlterations>().size() == (classId == 0x040d ? words.size() / 5 : 0));
            CHECK(report.diagnostics.size() == (words.empty() ? 0 : 1));
        }
    }
}

TEST_CASE("Dot details overlay only unlinked part fields", "[class][notehead]")
{
    using namespace finale_mus_reader;
    using Target = musx::dom::details::DotAlterations;
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    auto parsed = makeDetailClassContainer(0, 42, 0, {1, -30, 12, 8, 0}, profile.byteOrder, 0x040d);
    const auto part = makeDetailClassContainer(0, 42, 2, {9, 18, 99, 99, 0}, profile.byteOrder, 0x040d, true, {0, 0xffff, 0});
    auto& data = parsed.blocks.front().data;
    data.insert(data.end(), part.blocks.front().data.begin(), part.blocks.front().data.end());
    parsed.blocks.front().info.decodedSize = data.size();
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    ImportReport report(profile.epoch);
    PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const ImportContext context{LegacyRecordIndex::build(parsed), profile, noSource, document, document, report, pending, construction};
    details::importDotAlterations(context);
    runDeferredChecks(pending);
    const auto detail = document->getDetails()->get<Target>(2, musx::dom::EntryNumber(42), musx::dom::Inci(0));
    REQUIRE(detail);
    CHECK(detail->getShareMode() == musx::dom::EnigmaBase::ShareMode::Partial);
    CHECK(detail->noteId == 1);
    CHECK(detail->hOffset == 18);
    CHECK(detail->vOffset == 12);
    CHECK(detail->interdotSpacing == 8);
}

TEST_CASE("Finale 2.6 dot details preserve signed adjustments and Note ID attachment", "[class][fixture][notehead]")
{
    using Target = musx::dom::details::DotAlterations;
    const auto baseline = readFixture("evidence/F263/F263-dots.mus");
    CHECK(baseline.document->getDetails()->getAllSources<Target>().empty());
    const auto result = readFixture("evidence/F263/F263-dots-spaced.mus");
    const auto details = result.document->getDetails();
    const auto records = details->getArray<Target>(0, musx::dom::EntryNumber(1));
    REQUIRE(records.size() == 2);
    for (std::size_t index = 0; index < records.size(); ++index) {
        const auto& detail = records[index];
        const int sign = index == 0 ? 1 : -1;
        CHECK(detail->noteId == 2 - index);
        CHECK(detail->hOffset == -3 * sign);
        CHECK(detail->vOffset == 7 * sign);
        CHECK(detail->interdotSpacing == 11 * sign);
        for (const auto* name : {"noteId", "hOffset", "vOffset", "interdotSpacing"}) {
            const auto info = result.report.findField<Target>(name, 0, musx::dom::Cmper(0), static_cast<musx::dom::Inci>(index), musx::dom::Cmper(1));
            REQUIRE(info);
            CHECK(info->origin == ValueOrigin::LegacyMus);
        }
    }
    const musx::dom::details::GFrameHoldContext frameContext(result.document, 0, 1, 1);
    bool found = false;
    frameContext.iterateEntries([&](const musx::dom::EntryInfoPtr& entryInfo) {
        if (entryInfo->getEntry()->getEntryNumber() == 1) {
            for (std::size_t index = 0; index < 2; ++index) {
                const musx::dom::NoteInfoPtr noteInfo(entryInfo, index);
                REQUIRE(noteInfo);
                const auto detail = details->getForNote<Target>(noteInfo);
                REQUIRE(detail);
                CHECK(detail->getInci() == 1 - index);
                CHECK(detail->noteId == noteInfo->getNoteId());
            }
            found = true;
        }
        return true;
    });
    CHECK(found);
}

} // namespace
} // namespace finale_mus_reader_tests
