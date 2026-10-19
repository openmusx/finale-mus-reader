// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"
#include <set>

namespace finale_mus_reader_tests {
namespace {
using namespace classes;
using namespace finale_mus_reader;
using TieTestBase = musx::dom::details::TieAlterBase;

const std::vector<std::int16_t> fullTieTestWords{5, -12, 23, 0x03ff, 0x181f, 0, -31, 42, 0x600f, 0x5010, 1536, -57, 1024, 63, 0x6000};

template <typename Target>
void importTestTie(const ImportContext& context)
{
    if constexpr (std::is_same_v<Target, musx::dom::details::TieAlterEnd>) {
        details::importTieAlterEnd(context);
    } else {
        details::importTieAlterStart(context);
    }
}

template <typename Target>
void checkTieLayout(const container::ParsedContainer& parsed, SourceProfile profile, bool early)
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    ImportReport report(profile.epoch);
    PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const auto index = LegacyRecordIndex::build(parsed);
    const ImportContext context{index, profile, noSource, document, document, report, pending, construction};
    importTestTie<Target>(context);
    const auto target = document->getDetails()->template get<Target>(0, musx::dom::EntryNumber(42), musx::dom::Inci(0));
    REQUIRE(target);
    CHECK(target->noteId == 5);
    CHECK(target->xStart == -12);
    CHECK(target->xEnd == 23);
    CHECK(target->down);
    CHECK(target->freezeDirection);
    const auto key = instanceKey<Target>(0, musx::dom::Cmper(0), musx::dom::Inci(0), musx::dom::Cmper(42));
    std::set<std::string> expected{
        "noteId",
        "xStart",
        "xEnd",
        "yStart",
        "yEnd",
        "outerLocal",
        "outerOn",
        "afterSingleDotLocal",
        "afterSingleDotOn",
        "afterMultiDotsLocal",
        "afterMultiDotsOn",
        "shiftForSecondsLocal",
        "shiftForSecondsOn",
        "beforeSingleAcciLocal",
        "beforeSingleAcciOn",
        "down",
        "breakTimeLocal",
        "breakTimeOn",
        "breakKeyLocal",
        "breakKeyOn",
        "freezeDirection",
        "noSpecialArc",
        "stPtAdjOn",
        "stPtEntCnct",
        "stPtContext",
        "enPtAdjOn",
        "enPtEntCnct",
        "enPtContext",
        "insetRatio1",
        "height1",
        "insetRatio2",
        "height2",
        "ctlPtAdjOn",
        "ctlPtFixed",
    };
    std::set<std::string> actual;
    for (const auto& [name, info] : report.fields.at(key)) {
        actual.insert(name);
        CHECK(info.origin != ValueOrigin::Unmapped);
        CHECK(info.origin != ValueOrigin::MusxOnly);
    }
    CHECK(actual == expected);
    CHECK(actual.size() == TieTestBase::xmlMappingArray().size());
    CHECK(report.fields.at(key).at("noteId").rawValue == 5);
    CHECK(report.fields.at(key).at("xStart").rawValue == 65524);
    if (early) {
        CHECK(target->height1 == -57);
        CHECK(target->height2 == -57);
        CHECK(target->yStart == -9);
        CHECK(target->yEnd == -9);
        CHECK_FALSE(target->outerLocal);
        CHECK_FALSE(target->ctlPtAdjOn);
        CHECK(report.fields.at(key).at("outerLocal").origin == ValueOrigin::Finale27Default);
        CHECK(report.fields.at(key).at("height1").origin == ValueOrigin::LegacyMus);
        CHECK(report.fields.at(key).at("yStart").origin == ValueOrigin::LegacyMus);
    } else {
        CHECK(target->noteId == 5);
        CHECK(target->xStart == -12);
        CHECK(target->xEnd == 23);
        CHECK(target->yStart == -31);
        CHECK(target->yEnd == 42);
        CHECK(target->outerLocal == true);
        CHECK(target->outerOn == true);
        CHECK(target->afterSingleDotLocal == true);
        CHECK(target->afterSingleDotOn == true);
        CHECK(target->afterMultiDotsLocal == true);
        CHECK(target->afterMultiDotsOn == true);
        CHECK(target->shiftForSecondsLocal == true);
        CHECK(target->shiftForSecondsOn == true);
        CHECK(target->beforeSingleAcciLocal == true);
        CHECK(target->beforeSingleAcciOn == true);
        CHECK(target->down == true);
        CHECK(target->breakTimeLocal == true);
        CHECK(target->breakTimeOn == true);
        CHECK(target->breakKeyLocal == true);
        CHECK(target->breakKeyOn == true);
        CHECK(target->freezeDirection == true);
        CHECK(target->noSpecialArc == true);
        CHECK(target->stPtAdjOn == true);
        CHECK(target->stPtEntCnct == TieTestBase::ConnectionType::SystemEnd);
        CHECK(target->stPtContext == TieTestBase::DirectionContext::Over);
        CHECK(target->enPtAdjOn == true);
        CHECK(target->enPtEntCnct == TieTestBase::ConnectionType::SystemStart);
        CHECK(target->enPtContext == TieTestBase::DirectionContext::Under);
        CHECK(target->insetRatio1 == 1536);
        CHECK(target->height1 == -57);
        CHECK(target->insetRatio2 == 1024);
        CHECK(target->height2 == 63);
        CHECK(target->ctlPtAdjOn == true);
        CHECK(target->ctlPtFixed == true);
        for (const auto& [name, info] : report.fields.at(key)) {
            CHECK(info.origin == ValueOrigin::LegacyMus);
        }
    }
}

template <typename Target>
void exerciseTieLayouts(const char* tag, const char* verticalTag, std::uint16_t classId)
{
    for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
            SourceProfile profile(epoch);
            profile.byteOrder = order;
            profile.version = SourceVersion{.major = 3, .minor = 8, .devStatus = 1, .build = 4};
            const bool early = epoch == FormatEpoch::CodaBanner;
            auto parsed =
                epoch == FormatEpoch::ZlibLegacy
                    ? makeDetailClassContainer(0, 42, 0, fullTieTestWords, order, classId)
                    : makeDetailContainer(epoch, 0, 42, early ? std::vector<std::int16_t>{5, -12, 23, -57, 0x0801} : fullTieTestWords, tag, order);
            if (early) {
                auto vertical = makeDetailContainer(epoch, 0, 42, {5, -9, 0, 0, 0}, verticalTag, order);
                parsed.blocks.push_back(std::move(vertical.blocks.front()));
            }
            checkTieLayout<Target>(parsed, profile, early);
        }
        SourceProfile old(FormatEpoch::UncompressedLegacy);
        old.byteOrder = order;
        old.version = SourceVersion{.major = 3, .minor = 8, .devStatus = 1, .build = 3};
        auto parsed = makeDetailContainer(old.epoch, 0, 42, {5, -12, 23, -57, 0x0801}, tag, order);
        auto vertical = makeDetailContainer(old.epoch, 0, 42, {7, 99, 0, 0, 0, 5, -9, 0, 0, 0}, verticalTag, order);
        parsed.blocks.push_back(std::move(vertical.blocks.front()));
        checkTieLayout<Target>(parsed, old, true);
    }
}

TEST_CASE("Tie alterations decode both families, all epochs and the expansion boundary", "[class][tie-alter]")
{
    exerciseTieLayouts<musx::dom::details::TieAlterEnd>("Te", "te", 0x042e);
    exerciseTieLayouts<musx::dom::details::TieAlterStart>("Ts", "ts", 0x042f);
}

template <typename Target>
void exerciseTieRemap(const char* tag)
{
    for (const auto& finalIds : {std::vector<musx::dom::NoteNumber>{5}, {6}, {musx::dom::Note::RESTID}, {5, 6}}) {
        SourceProfile profile(FormatEpoch::DclLegacy);
        profile.byteOrder = ByteOrder::BigEndian;
        const auto parsed = makeDetailContainer(profile.epoch, 0, 42, fullTieTestWords, tag);
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        ImportReport report(profile.epoch);
        PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const auto index = LegacyRecordIndex::build(parsed);
        const ImportContext context{index, profile, noSource, document, document, report, pending, construction};
        importTestTie<Target>(context);
        pending.noteIdsByEntryIndex[42] = {std::vector<musx::dom::NoteNumber>(finalIds.size(), 5), finalIds};
        runDeferredChecks(pending);
        const auto target = document->getDetails()->template get<Target>(0, musx::dom::EntryNumber(42), musx::dom::Inci(0));
        REQUIRE(target);
        const auto expected = finalIds.size() == 1 ? finalIds.front() : 5;
        CHECK(target->noteId == expected);
        const auto* info = report.findField<Target>("noteId", 0, musx::dom::Cmper(0), musx::dom::Inci(0), musx::dom::Cmper(42));
        REQUIRE(info);
        CHECK(info->rawValue == 5);
        CHECK(info->origin == (expected == 5 ? ValueOrigin::LegacyMus : ValueOrigin::LegacyMusAdjusted));
        CHECK(report.diagnostics.empty() == (finalIds.size() == 1));
    }
}
TEST_CASE("Tie alteration note references remap uniquely and preserve ambiguous ids", "[class][tie-alter]")
{
    exerciseTieRemap<musx::dom::details::TieAlterEnd>("Te");
    exerciseTieRemap<musx::dom::details::TieAlterStart>("Ts");
}

template <typename Target>
void exerciseTieSharing(std::uint16_t classId)
{
    for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        SourceProfile profile(FormatEpoch::ZlibLegacy);
        profile.byteOrder = order;
        auto parsed = makeDetailClassContainer(0, 42, 0, fullTieTestWords, order, classId);
        auto partWords = fullTieTestWords;
        partWords[1] = -77;
        partWords[2] = 999;
        std::vector<std::uint16_t> masks(15);
        masks[1] = 0xffff;
        auto part = makeDetailClassContainer(0, 42, 2, partWords, order, classId, true, masks);
        parsed.blocks.front().data.insert(parsed.blocks.front().data.end(), part.blocks.front().data.begin(), part.blocks.front().data.end());
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        ImportReport report(profile.epoch);
        PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const auto index = LegacyRecordIndex::build(parsed);
        const ImportContext context{index, profile, noSource, document, document, report, pending, construction};
        importTestTie<Target>(context);
        const auto target = document->getDetails()->template get<Target>(2, musx::dom::EntryNumber(42), musx::dom::Inci(0));
        REQUIRE(target);
        CHECK(target->xStart == -77);
        CHECK(target->xEnd == 23);
        CHECK(target->getShareMode() == musx::dom::EnigmaBase::ShareMode::Partial);
    }
}
TEST_CASE("Tie alterations use linked-part masks with score inheritance", "[class][tie-alter]")
{
    exerciseTieSharing<musx::dom::details::TieAlterEnd>(0x042e);
    exerciseTieSharing<musx::dom::details::TieAlterStart>(0x042f);
}

TEST_CASE("Tie alteration incomplete records do not create phantom instances", "[class][tie-alter]")
{
    for (const auto size : {0U, 5U, 14U, 15U, 16U, 30U}) {
        SourceProfile profile(FormatEpoch::ZlibLegacy);
        profile.byteOrder = ByteOrder::BigEndian;
        const auto parsed = makeDetailClassContainer(0, 42, 0, std::vector<std::int16_t>(size, 0), profile.byteOrder, 0x042f);
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        ImportReport report(profile.epoch);
        PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const auto index = LegacyRecordIndex::build(parsed);
        const ImportContext context{index, profile, noSource, document, document, report, pending, construction};
        details::importTieAlterStart(context);
        const auto records = document->getDetails()->getArray<musx::dom::details::TieAlterStart>(0, musx::dom::EntryNumber(42));
        CHECK(records.size() == size / 15);
    }
}
TEST_CASE("Early tie references convert TCD values and remain discoverable on Notes", "[class][tie-alter]")
{
    for (const bool ambiguous : {false, true}) {
        auto imported = readFixture("evidence/F100/F100-chord-notealt.mus");
        const auto document = imported.document;
        const auto entry = document->getEntries()->get(1);
        REQUIRE(entry);
        REQUIRE(entry->notes.size() == 3);
        SourceProfile profile(FormatEpoch::CodaBanner);
        profile.byteOrder = ByteOrder::BigEndian;
        profile.version = SourceVersion{.major = 1};
        auto parsed = makeDetailContainer(profile.epoch, 0, 1, {77, -12, 23, -57, 0x0801}, "Ts");
        auto end = makeDetailContainer(profile.epoch, 0, 1, {77, -12, 23, -57, 0x0801}, "Te");
        parsed.blocks.push_back(std::move(end.blocks.front()));
        ImportReport report(profile.epoch);
        PendingReferences pending;
        pending.noteTcdByEntryIndex[1] = {77, static_cast<std::uint16_t>(ambiguous ? 77 : 78), 79};
        musx::factory::ConstructionContext construction;
        const auto index = LegacyRecordIndex::build(parsed);
        const ImportContext context{index, profile, noSource, document, document, report, pending, construction};
        details::importTieAlterStart(context);
        details::importTieAlterEnd(context);
        runDeferredChecks(pending);
        const auto check = [&]<typename Target>() {
            const auto target = document->getDetails()->get<Target>(0, musx::dom::EntryNumber(1), musx::dom::Inci(0));
            REQUIRE(target);
            CHECK(target->noteId == (ambiguous ? 77 : entry->notes.front()->getNoteId()));
            const auto* info = report.findField<Target>("noteId", 0, musx::dom::Cmper(0), musx::dom::Inci(0), musx::dom::Cmper(1));
            REQUIRE(info);
            CHECK(info->rawValue == 77);
            CHECK(info->origin == (ambiguous ? ValueOrigin::LegacyMus : ValueOrigin::LegacyMusAdjusted));
            if (!ambiguous) {
                bool found = false;
                const musx::dom::details::GFrameHoldContext frame(document, 0, 1, 1);
                frame.iterateEntries([&](const musx::dom::EntryInfoPtr& entryInfo) {
                    if (entryInfo->getEntry()->getEntryNumber() == 1) {
                        const musx::dom::NoteInfoPtr noteInfo(entryInfo, 0);
                        const auto attached = document->getDetails()->getForNote<Target>(noteInfo);
                        REQUIRE(attached);
                        CHECK(attached->getNoteId() == noteInfo->getNoteId());
                        found = true;
                    }
                    return true;
                });
                CHECK(found);
            }
        };
        check.template operator()<musx::dom::details::TieAlterStart>();
        check.template operator()<musx::dom::details::TieAlterEnd>();
        CHECK(report.diagnostics.empty() == !ambiguous);
    }
}
TEST_CASE("Controlled early tie edits preserve horizontal, vertical, and contour values", "[class][fixture][tie-alter]")
{
    using Target = musx::dom::details::TieAlterStart;
    for (const bool coda : {true, false}) {
        const std::string stem = coda ? "evidence/F263/F263-tie" : "evidence/F372/F372-tie";
        const auto baseline = readFixture(stem + ".mus");
        CHECK(baseline.document->getDetails()->getAllSources<Target>().empty());
        const auto imported = readFixture(stem + "-alt.mus");
        const auto target = imported.document->getDetails()->get<Target>(0, musx::dom::EntryNumber(1), musx::dom::Inci(0));
        REQUIRE(target);
        CHECK(target->noteId == 1);
        CHECK(target->xStart == (coda ? 7 : 13));
        CHECK(target->xEnd == (coda ? -7 : -13));
        CHECK(target->height1 == (coda ? 11 : 12));
        CHECK(target->height2 == target->height1);
        CHECK(target->down == coda);
        CHECK(target->freezeDirection);
        CHECK_FALSE(target->ctlPtAdjOn);
        CHECK(target->yStart == (coda ? 0 : -11));
        CHECK(target->yEnd == target->yStart);
        const auto* height = imported.report.findField<Target>("height2", 0, musx::dom::Cmper(0), musx::dom::Inci(0), musx::dom::Cmper(1));
        REQUIRE(height);
        CHECK(height->rawValue == (coda ? 11 : 12));
        CHECK(height->origin == ValueOrigin::LegacyMus);
        const auto* vertical = imported.report.findField<Target>("yEnd", 0, musx::dom::Cmper(0), musx::dom::Inci(0), musx::dom::Cmper(1));
        REQUIRE(vertical);
        CHECK(vertical->origin == (coda ? ValueOrigin::Finale27Default : ValueOrigin::LegacyMus));
    }
}
} // namespace
} // namespace finale_mus_reader_tests
