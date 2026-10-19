// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"
#include <algorithm>
#include <set>

namespace finale_mus_reader_tests {
namespace {
using namespace classes;
using ChordTestTarget = musx::dom::details::ChordAssign;

ImportReport importChordTest(
    const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile, const musx::dom::DocumentPtr& document)
{
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::details::importChordAssigns(context);
    finale_mus_reader::runDeferredChecks(pending);
    return report;
}

TEST_CASE("ChordAssign decodes packed fields and native long positions", "[class][chord-assign]")
{
    for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        std::vector<std::int16_t> words = {static_cast<std::int16_t>(0xfee6), static_cast<std::int16_t>(0xc963), 27, -13, 45, 0x53, -17, 39, 12,
            static_cast<std::int16_t>(0x8007), 0x1234, 0x5678, 83, 72, 0};
        if (order == ByteOrder::LittleEndian) {
            std::swap(words[10], words[11]);
        }
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
        profile.byteOrder = order;
        const auto report = importChordTest(makeDetailClassContainer(3, 5, 0, words, order, 0x0450), profile, document);
        const auto target = document->getDetails()->get<ChordTestTarget>(0, 3, 5, musx::dom::Inci(0));
        REQUIRE(target);
        CHECK(target->rootScaleNum == 6);
        CHECK(target->rootAlter == -2);
        CHECK(target->rootLowerCase);
        CHECK(target->playSuffix);
        CHECK(target->showRoot);
        CHECK(target->playRoot);
        CHECK(target->showFretboard);
        CHECK(target->showSuffix);
        CHECK(target->playFretboard);
        CHECK(target->bassScaleNum == 3);
        CHECK(target->bassAlter == -7);
        CHECK(target->bassLowerCase);
        CHECK(target->bassPosition == ChordTestTarget::BassPosition::Subtext);
        CHECK(target->showAltBass);
        CHECK(target->playAltBass);
        CHECK(target->suffixId == 27);
        CHECK(target->horzOff == -13);
        CHECK(target->vertOff == 45);
        CHECK(target->capoValue == 19);
        CHECK(target->useLocalCapo);
        CHECK(target->fbHorzOff == -17);
        CHECK(target->fbVertOff == 39);
        CHECK(target->fbStyleId == 12);
        CHECK(target->fretboardGroupInci == 7);
        CHECK(target->useFretboardFont);
        CHECK(target->horzEdu == 0x12345678);
        CHECK(target->chPercent == 83);
        CHECK(target->fbPercent == 72);
        REQUIRE(report.fields.size() == 1);
        std::set<std::string> actual;
        for (const auto& [name, field] : report.fields.begin()->second) {
            actual.insert(name);
            CHECK(field.origin == ValueOrigin::LegacyMus);
        }
        CHECK(actual
              == std::set<std::string>{"suffixId", "fbStyleId", "rootScaleNum", "rootAlter", "rootLowerCase", "playSuffix", "showRoot", "playRoot",
                  "showFretboard", "showSuffix", "playFretboard", "bassScaleNum", "bassAlter", "bassLowerCase", "bassPosition", "showAltBass",
                  "playAltBass", "capoValue", "useLocalCapo", "fretboardGroupInci", "useFretboardFont", "horzOff", "vertOff", "fbHorzOff",
                  "fbVertOff", "horzEdu", "chPercent", "fbPercent"});
        CHECK(ChordTestTarget::xmlMappingArray().size() == actual.size());
    }
}

TEST_CASE("ChordAssign converts compact source flags and entry timing", "[class][chord-assign]")
{
    for (const char* path : {"evidence/F100/F100-chord2.mus", "evidence/F263/F263-F100-chord2.mus", "evidence/F372/F372-F263-F100-chord2.mus"}) {
        const auto result = readFixture(path);
        const auto values = result.document->getDetails()->getAllSources<ChordTestTarget>();
        REQUIRE(values.size() == 1);
        const auto& value = values.front();
        CHECK(value->getCmper1() == 1);
        CHECK(value->getCmper2() == 1);
        CHECK(value->rootScaleNum == 2);
        CHECK(value->showRoot);
        CHECK_FALSE(value->playRoot);
        CHECK_FALSE(value->playSuffix);
        CHECK(value->showSuffix);
        CHECK(value->horzOff == 28);
        CHECK(value->vertOff == 76);
        CHECK(value->fbHorzOff == -4);
        CHECK(value->fbVertOff == 26);
        CHECK(value->horzEdu == 0);
        CHECK(value->chPercent == 100);
        CHECK(value->fbPercent == 100);
    }
}

TEST_CASE("ChordAssign handles entry layouts in each epoch and byte order", "[class][chord-assign]")
{
    using namespace musx::dom;
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            auto seed = readFixture("evidence/F2006/F2006-lyric-vcs.mus");
            auto document = seed.document;
            const auto frames = document->getDetails()->getArray<details::GFrameHold>(0);
            REQUIRE_FALSE(frames.empty());
            const auto frame = frames.front();
            EntryNumber number = 0;
            frame->iterateRawEntries([&](const MusxInstance<Entry>& entry, LayerIndex) {
                number = entry->getEntryNumber();
                return false;
            });
            REQUIRE(number != 0);
            const bool compact = epoch == FormatEpoch::CodaBanner;
            const std::vector<std::int16_t> words =
                compact ? std::vector<std::int16_t>{0x82, 0, 3, -2, 76} : std::vector<std::int16_t>{0x60e2, 0x4061, 3, -2, 76, -9, 11, 4, 5, 0x47};
            const auto high = static_cast<Cmper>(number >> 16U);
            const auto low = static_cast<Cmper>(number);
            const auto parsed = epoch == FormatEpoch::ZlibLegacy ? makeDetailClassContainer(high, low, 0, words, order, 0x0407)
                                                                 : makeDetailContainer(epoch, high, low, words, compact ? "CH" : "Ch", order);
            auto profile = SourceProfile(epoch);
            profile.byteOrder = order;
            profile.version = SourceVersion{.major = 5};
            const auto report = importChordTest(parsed, profile, document);
            const auto values = document->getDetails()->getAllSources<ChordTestTarget>();
            REQUIRE(values.size() == 1);
            CHECK(values.front()->getCmper1() == frame->getStaff());
            CHECK(values.front()->getCmper2() == frame->getMeasure());
            CHECK(values.front()->suffixId == 3);
            CHECK(values.front()->horzEdu == 0);
            CHECK(values.front()->chPercent == 100);
            CHECK(values.front()->fbPercent == 100);
            REQUIRE(report.fields.size() == 2);
            const auto chordFields =
                std::ranges::find_if(report.fields, [](const auto& item) { return item.first.classType == typeid(ChordTestTarget); });
            REQUIRE(chordFields != report.fields.end());
            CHECK(chordFields->second.size() == 28);
            // The entry-chord conversion sets the flag of the measure it moved a chord to.
            const auto measure = document->getOthers()->get<others::Measure>(0, frame->getMeasure());
            REQUIRE(measure);
            CHECK(measure->hasChord);
            CHECK(report.fields.at(finale_mus_reader::instanceKey<others::Measure>(0, frame->getMeasure())).at("hasChord").origin
                  == ValueOrigin::LegacyMusAdjusted);
        }
    }
}

TEST_CASE("ChordAssign converts whole rests and reads the attached entry size", "[class][chord-assign]")
{
    using namespace musx::dom;
    auto seed = readFixture("evidence/F2006/F2006-lyric-vcs.mus");
    auto document = seed.document;
    const auto frame = document->getDetails()->getArray<details::GFrameHold>(0).front();
    EntryNumber number = 0;
    frame->iterateRawEntries([&](const MusxInstance<Entry>& entry, LayerIndex) {
        number = entry->getEntryNumber();
        auto changed = std::const_pointer_cast<Entry>(entry);
        changed->isNote = false;
        changed->duration = Edu(NoteType::Whole);
        changed->noteDetail = true;
        return false;
    });
    auto size = std::make_shared<details::EntrySize>(document, Cmper(0), EnigmaBase::ShareMode::All, number);
    size->percent = 67;
    document->getDetails()->add(details::EntrySize::XmlNodeName, size);
    const auto measure = document->getOthers()->get<others::Measure>(0, frame->getMeasure());
    REQUIRE(measure);
    auto profile = SourceProfile(FormatEpoch::DclLegacy);
    const auto report = importChordTest(makeDetailContainer(profile.epoch, static_cast<Cmper>(number >> 16U), static_cast<Cmper>(number),
                                            {0x60e0, 0, 1, 12, 0, 12, 0, 1, 0, 0}, "Ch"),
        profile, document);
    const auto values = document->getDetails()->getAllSources<ChordTestTarget>();
    REQUIRE(values.size() == 1);
    CHECK(values.front()->horzEdu == measure->calcDuration().calcEduDuration() / 2 - 512);
    CHECK(values.front()->chPercent == 67);
    CHECK(values.front()->fbPercent == 100);
    CHECK(values.front()->fbHorzOff == 0);
    const auto chordFields = std::ranges::find_if(report.fields, [](const auto& item) { return item.first.classType == typeid(ChordTestTarget); });
    REQUIRE(chordFields != report.fields.end());
    CHECK(chordFields->second.at("horzEdu").origin == ValueOrigin::LegacyMusAdjusted);
}

TEST_CASE("ChordAssign fretboard coordinates change at the Finale 2010 beta boundary", "[class][chord-assign]")
{
    for (const std::uint8_t build : {std::uint8_t(6), std::uint8_t(7)}) {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
        profile.byteOrder = ByteOrder::LittleEndian;
        profile.version = SourceVersion{.major = 15, .devStatus = 2, .build = build};
        const auto report = importChordTest(
            makeDetailClassContainer(1, 1, 0, {0x60e0, 0, 1, 12, -30, 0, 20, -25, 1, 0, 0, 0, 100, 100, 0}, profile.byteOrder, 0x0450), profile,
            document);
        const auto value = document->getDetails()->getAllSources<ChordTestTarget>().front();
        CHECK(value->fbHorzOff == (build == 6 ? 8 : 20));
        CHECK(value->fbVertOff == (build == 6 ? 5 : -25));
        CHECK(report.fields.begin()->second.at("fbHorzOff").origin == (build == 6 ? ValueOrigin::LegacyMusAdjusted : ValueOrigin::LegacyMus));
    }
}

TEST_CASE("ChordAssign reports undefined bass placement without inventing an enum", "[class][chord-assign]")
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    const auto report = importChordTest(
        makeDetailClassContainer(1, 1, 0, {0x60e0, 0x4080, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 100, 100, 0}, profile.byteOrder, 0x0450), profile, document);
    REQUIRE(report.diagnostics.size() == 1);
    CHECK(report.fields.begin()->second.at("bassPosition").origin == ValueOrigin::Unmapped);
    CHECK(document->getDetails()->getAllSources<ChordTestTarget>().front()->bassPosition == ChordTestTarget::BassPosition::AfterRoot);
}

TEST_CASE("ChordAssign completes a part's measure list from the score", "[class][chord-assign]")
{
    using namespace musx::dom;
    auto seed = readFixture("evidence/F2006/F2006-lyric-vcs.mus");
    auto document = seed.document;
    const auto frame = document->getDetails()->getArray<details::GFrameHold>(0).front();
    std::vector<EntryNumber> numbers;
    frame->iterateRawEntries([&](const MusxInstance<Entry>& entry, LayerIndex) {
        numbers.push_back(entry->getEntryNumber());
        return numbers.size() < 2;
    });
    REQUIRE(numbers.size() == 2);
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    const auto record = [&](EntryNumber number, Cmper part, std::int16_t offset) {
        std::vector<std::uint16_t> mask(10);
        mask[3] = 0xffff;
        return makeDetailClassContainer(static_cast<Cmper>(number >> 16U), static_cast<Cmper>(number), part,
            {0x60e0, 0, 1, offset, 0, 12, 0, 1, 0, 0}, profile.byteOrder, 0x0407, part != 0, mask);
    };
    auto parsed = record(numbers[0], 0, 12);
    for (const auto& extra : {record(numbers[1], 0, 15), record(numbers[1], 1, 23)}) {
        auto& block = parsed.blocks.front();
        block.data.insert(block.data.end(), extra.blocks.front().data.begin(), extra.blocks.front().data.end());
        block.info.decodedSize = block.data.size();
    }
    const auto report = importChordTest(parsed, profile, document);
    const auto values = document->getDetails()->getAllSources<ChordTestTarget>();
    REQUIRE(values.size() == 4);
    const auto part = document->getDetails()->get<ChordTestTarget>(1, frame->getStaff(), frame->getMeasure(), Inci(1));
    REQUIRE(part);
    CHECK(part->getSourcePartId() == 1);
    CHECK(part->horzOff == 23);
    const auto inherited = document->getDetails()->get<ChordTestTarget>(1, frame->getStaff(), frame->getMeasure(), Inci(0));
    REQUIRE(inherited);
    CHECK(inherited->getSourcePartId() == 1);
    CHECK(inherited->horzOff == 12);
    CHECK(report.diagnostics.empty());
}

TEST_CASE("ChordAssign keeps a part's chord only when its show flags or offsets are unlinked", "[class][chord-assign]")
{
    using namespace musx::dom;
    struct Case
    {
        std::size_t word;
        std::uint16_t bits;
        bool kept;
    };
    for (const auto& [word, bits, kept] :
        {Case{1, 0xffff, false}, Case{0, 0x9fbf, false}, Case{0, 0x0040, true}, Case{0, 0x4000, true}, Case{4, 0x0001, true}}) {
        auto seed = readFixture("evidence/F2006/F2006-lyric-vcs.mus");
        auto document = seed.document;
        const auto frame = document->getDetails()->getArray<details::GFrameHold>(0).front();
        EntryNumber number = 0;
        frame->iterateRawEntries([&](const MusxInstance<Entry>& entry, LayerIndex) {
            number = entry->getEntryNumber();
            return false;
        });
        auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
        profile.byteOrder = ByteOrder::LittleEndian;
        const auto record = [&](Cmper part) {
            std::vector<std::uint16_t> mask(10);
            mask[word] = bits;
            return makeDetailClassContainer(static_cast<Cmper>(number >> 16U), static_cast<Cmper>(number), part,
                {0x60e0, 0, 1, 12, 0, 12, 0, 1, 0, 0}, profile.byteOrder, 0x0407, part != 0, mask);
        };
        auto parsed = record(0);
        const auto part = record(1);
        auto& block = parsed.blocks.front();
        block.data.insert(block.data.end(), part.blocks.front().data.begin(), part.blocks.front().data.end());
        block.info.decodedSize = block.data.size();
        importChordTest(parsed, profile, document);
        CHECK(document->getDetails()->getAllSources<ChordTestTarget>().size() == (kept ? 2 : 1));
    }
}

TEST_CASE("ChordAssign rejects incomplete measure assignments and empty input", "[class][chord-assign]")
{
    for (std::size_t count : {std::size_t(0), std::size_t(14)}) {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
        profile.byteOrder = ByteOrder::LittleEndian;
        const auto report =
            importChordTest(makeDetailClassContainer(1, 1, 0, std::vector<std::int16_t>(count), profile.byteOrder, 0x0450), profile, document);
        CHECK(document->getDetails()->getAllSources<ChordTestTarget>().empty());
        CHECK(report.diagnostics.size() == (count ? 1 : 0));
    }
}

TEST_CASE("Before Finale 2010 a measure with a chord takes the chord flag", "[class][chord-assign]")
{
    const auto result = readFixture("evidence/F372/F372-F263-F100-chord2.mus");
    const auto measure = result.document->getOthers()->get<musx::dom::others::Measure>(musx::dom::SCORE_PARTID, 1);
    REQUIRE(measure);
    CHECK(measure->hasChord);
    const auto* field = result.report.findField<musx::dom::others::Measure>("hasChord", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    REQUIRE(field);
    CHECK(field->origin == ValueOrigin::LegacyMusAdjusted);
}

} // namespace
} // namespace finale_mus_reader_tests
