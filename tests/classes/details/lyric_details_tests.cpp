// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <optional>
#include <set>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

template <typename Target>
void checkAssignment(
    const ImportResult& result, musx::dom::EntryNumber entry, unsigned lyricNumber, unsigned syllable, finale_mus_reader::records::LegacyTag identity)
{
    const auto item = result.document->getDetails()->get<Target>(musx::dom::SCORE_PARTID, entry, musx::dom::Inci(0));
    REQUIRE(item);
    CHECK(item->lyricNumber == lyricNumber);
    CHECK(item->syllable == syllable);
    const auto key = finale_mus_reader::instanceKey<Target>(
        musx::dom::SCORE_PARTID, musx::dom::Cmper(0), musx::dom::Inci(0), static_cast<musx::dom::Cmper>(entry));
    const auto& fields = result.report.fields.at(key);
    std::set<std::string> actual;
    for (const auto& [name, field] : fields) {
        actual.insert(name);
    }
    CHECK(actual == std::set<std::string>{"lyricNumber", "syllable", "horzOffset", "vertOffset", "floatingHorzOff", "wext", "displayVerseNum"});
    CHECK(fields.at("lyricNumber").sourceIdentity == identity);
    CHECK(fields.at("lyricNumber").rawValue == lyricNumber);
}

TEST_CASE("Lyric assignments retain all three text types through the DCL and zlib layouts", "[class][lyric-details]")
{
    const auto dcl = readFixture("evidence/F2006/F2006-lyric-vcs.mus");
    checkAssignment<musx::dom::details::LyricAssignChorus>(dcl, 17, 2, 1, finale_mus_reader::records::packTag("ch"));
    checkAssignment<musx::dom::details::LyricAssignSection>(dcl, 17, 3, 1, finale_mus_reader::records::packTag("se"));
    checkAssignment<musx::dom::details::LyricAssignVerse>(dcl, 17, 1, 1, finale_mus_reader::records::packTag("ve"));
    const auto zlibEarly = readFixture("evidence/F2008/F2008-F2006-lyric-vcs.mus");
    checkAssignment<musx::dom::details::LyricAssignChorus>(zlibEarly, 17, 2, 1, 0x0418);
    checkAssignment<musx::dom::details::LyricAssignSection>(zlibEarly, 17, 3, 1, 0x0419);
    checkAssignment<musx::dom::details::LyricAssignVerse>(zlibEarly, 17, 1, 1, 0x041a);
    const auto zlibLater = readFixture("evidence/F2012/F2012-F2006-lyric-vcs.mus");
    checkAssignment<musx::dom::details::LyricAssignChorus>(zlibLater, 17, 2, 1, 0x0452);
    checkAssignment<musx::dom::details::LyricAssignSection>(zlibLater, 17, 3, 1, 0x0453);
    checkAssignment<musx::dom::details::LyricAssignVerse>(zlibLater, 17, 1, 1, 0x0454);
}

TEST_CASE("Lyric assignment signed offsets and later flags decode in either byte order", "[class][lyric-details]")
{
    for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        const auto parsed = makeDetailClassContainer(0, 27, 0, {4, 2, -12, 23, 45, -67, static_cast<std::int16_t>(0x8000), 0, 0, 0}, order, 0x0454);
        auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
        profile.byteOrder = order;
        profile.version = SourceVersion{.major = 17};
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        const auto index = LegacyRecordIndex::build(parsed);
        auto referenceSession = musx::factory::DocumentFactory::begin();
        const auto reference = std::move(referenceSession).finish();
        ImportReport report(profile.epoch);
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::details::importLyricAssignVerse(context);
        using Target = musx::dom::details::LyricAssignVerse;
        const auto item = document->getDetails()->get<Target>(0, 27, musx::dom::Inci(0));
        REQUIRE(item);
        CHECK(item->horzOffset == -12);
        CHECK(item->vertOffset == 23);
        CHECK(item->wext == 45);
        CHECK(item->floatingHorzOff == -67);
        CHECK(item->displayVerseNum);
        const auto key = finale_mus_reader::instanceKey<Target>(0, musx::dom::Cmper(0), musx::dom::Inci(0), musx::dom::Cmper(27));
        CHECK(report.fields.at(key).at("displayVerseNum").rawValue == 0x8000);
    }
}

TEST_CASE("Five-word verse assignment decodes across the four container epochs", "[class][lyric-details]")
{
    using Target = musx::dom::details::LyricAssignVerse;
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            const auto parsed = epoch == FormatEpoch::ZlibLegacy ? makeDetailClassContainer(0, 27, 0, {9, 4, -8, 12, 33}, order, 0x041a)
                                                                 : makeDetailContainer(epoch, 0, 27, {9, 4, -8, 12, 33}, "ve", order);
            auto profile = SourceProfile(epoch);
            profile.byteOrder = order;
            // A Finale 2008 file version, before the ten-word classes.
            profile.version = SourceVersion{.major = 13};
            auto session = musx::factory::DocumentFactory::begin();
            const auto document = session.getDocument();
            const auto index = LegacyRecordIndex::build(parsed);
            auto referenceSession = musx::factory::DocumentFactory::begin();
            const auto reference = std::move(referenceSession).finish();
            ImportReport report(epoch);
            finale_mus_reader::PendingReferences pending;
            musx::factory::ConstructionContext construction;
            const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
            finale_mus_reader::details::importLyricAssignVerse(context);
            const auto item = document->getDetails()->get<Target>(0, 27, musx::dom::Inci(0));
            REQUIRE(item);
            CHECK(item->lyricNumber == 9);
            CHECK(item->syllable == 4);
            CHECK(item->horzOffset == -8);
            CHECK(item->vertOffset == 12);
            CHECK(item->wext == 33);
            CHECK(item->floatingHorzOff == 0);
            CHECK_FALSE(item->displayVerseNum);
        }
    }
}

TEST_CASE("Lyric entry alignment reads three overrides and leaves default unset", "[class][lyric-details]")
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            for (const auto flags : {0x0102, 0x0303, 0}) {
                const auto words = std::vector<std::int16_t>{0, 0, 0, 0, static_cast<std::int16_t>(flags)};
                const auto parsed = epoch == FormatEpoch::ZlibLegacy ? makeDetailClassContainer(0, 27, 0, words, order, 0x041b)
                                                                     : makeDetailContainer(epoch, 0, 27, words, "LE", order);
                auto profile = SourceProfile(epoch);
                profile.byteOrder = order;
                auto session = musx::factory::DocumentFactory::begin();
                const auto document = session.getDocument();
                const auto index = LegacyRecordIndex::build(parsed);
                auto referenceSession = musx::factory::DocumentFactory::begin();
                const auto reference = std::move(referenceSession).finish();
                ImportReport report(epoch);
                finale_mus_reader::PendingReferences pending;
                musx::factory::ConstructionContext construction;
                const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
                finale_mus_reader::details::importLyricEntryInfo(context);
                using Target = musx::dom::details::LyricEntryInfo;
                const auto item = document->getDetails()->get<Target>(0, 27);
                REQUIRE(item);
                if (flags == 0x0102) {
                    CHECK(item->justify == musx::dom::AlignJustify::Left);
                    CHECK(item->align == musx::dom::AlignJustify::Center);
                } else if (flags == 0x0303) {
                    CHECK(item->justify == musx::dom::AlignJustify::Right);
                    CHECK(item->align == musx::dom::AlignJustify::Right);
                } else {
                    CHECK_FALSE(item->justify);
                    CHECK_FALSE(item->align);
                }
                const auto key = finale_mus_reader::instanceKey<Target>(0, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(27));
                const auto& fields = report.fields.at(key);
                CHECK(fields.size() == 2);
                CHECK(fields.at("justify").origin == ValueOrigin::LegacyMus);
                CHECK(fields.at("justify").rawValue == (flags & 3));
                CHECK(fields.at("align").origin == ValueOrigin::LegacyMus);
                CHECK(fields.at("align").rawValue == ((flags >> 8) & 3));
            }
        }
    }
}

// The source version decides which assignment classes a zlib document stores, so a record of the
// other set is not read even when present.
TEST_CASE("Zlib lyric assignment classes are selected by source version", "[class][lyric-details]")
{
    using Target = musx::dom::details::LyricAssignVerse;
    const auto importWith = [](finale_mus_reader::records::LegacyTag classId, std::uint8_t major) {
        const auto words =
            classId == 0x0454 ? std::vector<std::int16_t>{9, 4, -8, 12, 33, 0, 0, 0, 0, 0} : std::vector<std::int16_t>{9, 4, -8, 12, 33};
        const auto parsed = makeDetailClassContainer(0, 27, 0, words, ByteOrder::LittleEndian, classId);
        auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
        profile.byteOrder = ByteOrder::LittleEndian;
        profile.version = SourceVersion{.major = major};
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        const auto index = LegacyRecordIndex::build(parsed);
        auto referenceSession = musx::factory::DocumentFactory::begin();
        const auto reference = std::move(referenceSession).finish();
        ImportReport report(profile.epoch);
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::details::importLyricAssignVerse(context);
        return document->getDetails()->get<Target>(0, 27, musx::dom::Inci(0)) != nullptr;
    };
    CHECK(importWith(0x041a, 15));
    CHECK_FALSE(importWith(0x041a, 16));
    CHECK(importWith(0x0454, 16));
    CHECK_FALSE(importWith(0x0454, 15));
}

TEST_CASE("A part keeps a pre-2011 lyric list only when an offset is unlinked", "[class][lyric-details]")
{
    using Target = musx::dom::details::LyricAssignVerse;
    struct Case
    {
        finale_mus_reader::records::LegacyTag classId;
        std::uint8_t major;
        std::size_t word;
        std::size_t partCount;
    };
    // Words 2 and 3 are the offsets. The ten-word class is stored by Finale 2011 and later, which
    // needs no conversion, so its part records are kept as stored.
    for (const auto& [classId, major, word, partCount] :
        {Case{0x041a, 15, 1, 0}, Case{0x041a, 15, 7, 2}, Case{0x041a, 15, 3, 2}, Case{0x0454, 16, 1, 1}}) {
        const auto record = [&](musx::dom::Cmper part, std::int16_t offset) {
            std::vector<std::uint16_t> mask(10);
            mask[word] = 0xffff;
            return makeDetailClassContainer(0, 27, part, {9, 4, -8, 12, 33, 9, 5, offset, 14, 40}, ByteOrder::LittleEndian, classId, part != 0, mask);
        };
        auto parsed = record(0, -6);
        const auto part = record(1, 99);
        auto& block = parsed.blocks.front();
        block.data.insert(block.data.end(), part.blocks.front().data.begin(), part.blocks.front().data.end());
        block.info.decodedSize = block.data.size();
        auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
        profile.byteOrder = ByteOrder::LittleEndian;
        profile.version = SourceVersion{.major = major};
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        const auto index = LegacyRecordIndex::build(parsed);
        auto referenceSession = musx::factory::DocumentFactory::begin();
        const auto reference = std::move(referenceSession).finish();
        ImportReport report(profile.epoch);
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::details::importLyricAssignVerse(context);
        std::size_t count = 0;
        for (const auto& item : document->getDetails()->getAllSources<Target>()) {
            count += item->getSourcePartId() == 1 ? 1 : 0;
        }
        CHECK(count == partCount);
        if (classId == 0x041a && partCount != 0) {
            const auto linked = document->getDetails()->get<Target>(1, 27, musx::dom::Inci(word < 5 ? 1 : 0));
            REQUIRE(linked);
            CHECK(linked->getSourcePartId() == 1);
            CHECK(linked->horzOffset == (word < 5 ? -6 : -8));
            CHECK(document->getDetails()->get<Target>(1, 27, musx::dom::Inci(1))->horzOffset == (word == 7 ? 99 : -6));
        }
    }
}

} // namespace
} // namespace finale_mus_reader_tests
