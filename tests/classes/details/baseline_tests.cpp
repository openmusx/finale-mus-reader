// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include <algorithm>
#include <set>

#include "baseline_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using namespace musx::dom::details;

constexpr musx::dom::Cmper cmperZero = 0;
constexpr musx::dom::Cmper cmperOne = 1;
constexpr musx::dom::Cmper cmperTwo = 2;
constexpr musx::dom::Cmper cmperEight = 8;
constexpr musx::dom::Inci inciZero = 0;
constexpr musx::dom::Inci inciNine = 9;

musx::dom::DocumentPtr baselineDocument()
{
    return musx::factory::DocumentFactory::begin().getDocument();
}

std::vector<std::int16_t> baselineWords(std::int32_t displacement, std::uint16_t lyricNumber, ByteOrder byteOrder)
{
    const auto value = static_cast<std::uint32_t>(displacement);
    const auto high = static_cast<std::int16_t>(value >> 16U);
    const auto low = static_cast<std::int16_t>(value);
    return byteOrder == ByteOrder::BigEndian ? std::vector<std::int16_t>{high, low, static_cast<std::int16_t>(lyricNumber), 11, 12}
                                             : std::vector<std::int16_t>{low, high, static_cast<std::int16_t>(lyricNumber), 11, 12};
}

template <typename T>
void checkNonLyricFamily(const char* tag, std::uint16_t classId, musx::dom::Cmper cmper1 = 7)
{
    constexpr musx::dom::Cmper cmper2 = 12;
    constexpr std::int32_t displacement = -70000;
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            const auto words = baselineWords(displacement, 99, byteOrder);
            const auto parsed = epoch == FormatEpoch::ZlibLegacy
                                    ? makeDetailClassContainer(cmper1, cmper2, musx::dom::SCORE_PARTID, words, byteOrder, classId)
                                    : makeDetailContainer(epoch, cmper1, cmper2, words, tag, byteOrder);
            auto profile = SourceProfile(epoch);
            profile.byteOrder = byteOrder;
            const auto document = baselineDocument();
            const auto report = importBaselines(parsed, profile, document);
            const auto target = document->getDetails()->get<T>(musx::dom::SCORE_PARTID, cmper1, cmper2);
            REQUIRE(target);
            CHECK(target->baselineDisplacement == displacement);
            CHECK_FALSE(target->lyricNumber);
            const auto* displacementField =
                report.template findField<T>("baselineDisplacement", musx::dom::SCORE_PARTID, cmper1, std::nullopt, cmper2);
            const auto* lyricField = report.template findField<T>("lyricNumber", musx::dom::SCORE_PARTID, cmper1, std::nullopt, cmper2);
            REQUIRE(displacementField);
            REQUIRE(lyricField);
            CHECK(displacementField->origin == ValueOrigin::LegacyMus);
            CHECK(displacementField->rawValue == displacement);
            CHECK(lyricField->origin == ValueOrigin::LegacyBehavior);
        }
    }
}

template <typename T>
void checkLyricFamily(const char* tag, std::uint16_t classId, musx::dom::Cmper cmper1 = 5)
{
    constexpr musx::dom::Cmper cmper2 = 9;
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            auto words = baselineWords(-144, 1, byteOrder);
            const auto second = baselineWords(-184, 2, byteOrder);
            words.insert(words.end(), second.begin(), second.end());
            const auto parsed = epoch == FormatEpoch::ZlibLegacy
                                    ? makeDetailClassContainer(cmper1, cmper2, musx::dom::SCORE_PARTID, words, byteOrder, classId)
                                    : makeDetailContainer(epoch, cmper1, cmper2, words, tag, byteOrder);
            auto profile = SourceProfile(epoch);
            profile.byteOrder = byteOrder;
            const auto document = baselineDocument();
            const auto report = importBaselines(parsed, profile, document);
            const auto targets = document->getDetails()->getArray<T>(musx::dom::SCORE_PARTID, cmper1, cmper2);
            REQUIRE(targets.size() == 2);
            CHECK(targets[0]->baselineDisplacement == -144);
            CHECK(targets[0]->lyricNumber == 1);
            CHECK(targets[1]->baselineDisplacement == -184);
            CHECK(targets[1]->lyricNumber == 2);
            for (musx::dom::Inci inci = 0; inci < 2; ++inci) {
                const auto* displacement = report.template findField<T>("baselineDisplacement", musx::dom::SCORE_PARTID, cmper1, inci, cmper2);
                const auto* lyric = report.template findField<T>("lyricNumber", musx::dom::SCORE_PARTID, cmper1, inci, cmper2);
                REQUIRE(displacement);
                REQUIRE(lyric);
                CHECK(displacement->origin == ValueOrigin::LegacyMus);
                CHECK(lyric->origin == ValueOrigin::LegacyMus);
            }
        }
    }
}

template <typename T>
void checkShortPartialGlobalLyricArray(std::uint16_t classId)
{
    constexpr auto byteOrder = ByteOrder::LittleEndian;
    std::vector<std::int16_t> scoreWords;
    for (std::uint16_t lyricNumber = 1; lyricNumber <= 10; ++lyricNumber) {
        const auto words = baselineWords(-104 - 40 * lyricNumber, lyricNumber, byteOrder);
        scoreWords.insert(scoreWords.end(), words.begin(), words.end());
    }
    auto parsed = makeDetailClassContainer(0, 0, musx::dom::SCORE_PARTID, scoreWords, byteOrder, classId);

    std::vector<std::int16_t> partWords(6 * finale_mus_reader::records::detailWordCount);
    const auto third = baselineWords(-165, 3, byteOrder);
    const auto sixth = baselineWords(-144, 6, byteOrder);
    std::copy(third.begin(), third.end(), partWords.begin() + 2 * finale_mus_reader::records::detailWordCount);
    std::copy(sixth.begin(), sixth.end(), partWords.begin() + 5 * finale_mus_reader::records::detailWordCount);
    std::vector<std::uint16_t> masks(partWords.size());
    masks[10] = masks[11] = 0xffff;
    masks[25] = masks[26] = 0xffff;
    auto part = makeDetailClassContainer(0, 0, 2, partWords, byteOrder, classId, true, masks);
    auto& data = parsed.blocks.front().data;
    data.insert(data.end(), part.blocks.front().data.begin(), part.blocks.front().data.end());
    parsed.blocks.front().info.decodedSize = data.size();

    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = byteOrder;
    const auto document = baselineDocument();
    const auto report = importBaselines(parsed, profile, document);
    const auto values = document->getDetails()->getArray<T>(cmperTwo, cmperZero, cmperZero);
    REQUIRE(values.size() == 10);
    for (std::size_t index = 0; index < values.size(); ++index) {
        const auto expected = index == 2 ? -165 : index == 5 ? -144 : -144 - 40 * static_cast<std::int32_t>(index);
        CHECK(values[index]->baselineDisplacement == expected);
        CHECK(values[index]->lyricNumber == index + 1);
        CHECK(values[index]->getShareMode() == musx::dom::EnigmaBase::ShareMode::Partial);
    }
    CHECK(report.findField<T>("baselineDisplacement", cmperTwo, cmperZero, inciNine, cmperZero)->origin == ValueOrigin::LegacyMus);
}

template <typename GlobalT, typename SystemT>
void checkSharedFixedSelector(const char* tag, std::optional<musx::dom::Cmper> lyricNumber = std::nullopt)
{
    constexpr musx::dom::Cmper system = 8;
    constexpr musx::dom::Cmper staff = 1;
    const auto parsed = makeDetailContainer(FormatEpoch::UncompressedLegacy, system, staff,
        baselineWords(-222, lyricNumber.value_or(cmperZero), ByteOrder::BigEndian), tag, ByteOrder::BigEndian);
    auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    const auto document = baselineDocument();
    const auto report = importBaselines(parsed, profile, document);

    CHECK(document->getDetails()->getArray<GlobalT>(cmperZero, system, staff).empty());
    const auto values = document->getDetails()->getArray<SystemT>(cmperZero, system, staff);
    REQUIRE(values.size() == 1);
    CHECK(values.front()->baselineDisplacement == -222);
    CHECK(values.front()->lyricNumber == lyricNumber);
    CHECK(report.findField<SystemT>("baselineDisplacement", cmperZero, system, values.front()->getInci(), staff)->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Baseline families recover the common struct in every located epoch", "[class][baseline]")
{
    checkNonLyricFamily<BaselineChords>("CL", 0x03f2, 0);
    checkNonLyricFamily<BaselineExpressionsAbove>("XA", 0x03f3, 0);
    checkNonLyricFamily<BaselineExpressionsBelow>("XB", 0x03f4, 0);
    checkNonLyricFamily<BaselineFretboards>("GL", 0x03f5, 0);
    checkLyricFamily<BaselineLyricsChorus>("Bc", 0x03f6, 0);
    checkLyricFamily<BaselineLyricsSection>("Bs", 0x03f7, 0);
    checkLyricFamily<BaselineLyricsVerse>("Bv", 0x03f8, 0);
    checkNonLyricFamily<BaselineSystemChords>("Cl", 0x0443);
    checkNonLyricFamily<BaselineSystemExpressionsAbove>("Xa", 0x0444);
    checkNonLyricFamily<BaselineSystemExpressionsBelow>("Xb", 0x0445);
    checkNonLyricFamily<BaselineSystemFretboards>("Gl", 0x0446);
    checkLyricFamily<BaselineSystemLyricsChorus>("bc", 0x0447);
    checkLyricFamily<BaselineSystemLyricsSection>("BS", 0x0448);
    checkLyricFamily<BaselineSystemLyricsVerse>("bv", 0x0449);
}

TEST_CASE("Fixed-row baseline selectors distinguish global and system identities", "[class][baseline]")
{
    checkSharedFixedSelector<BaselineChords, BaselineSystemChords>("CL");
    checkSharedFixedSelector<BaselineExpressionsAbove, BaselineSystemExpressionsAbove>("XA");
    checkSharedFixedSelector<BaselineExpressionsBelow, BaselineSystemExpressionsBelow>("XB");
    checkSharedFixedSelector<BaselineFretboards, BaselineSystemFretboards>("GL");
    checkSharedFixedSelector<BaselineLyricsChorus, BaselineSystemLyricsChorus>("Bc", musx::dom::Cmper{3});
    checkSharedFixedSelector<BaselineLyricsSection, BaselineSystemLyricsSection>("Bs", musx::dom::Cmper{3});
    checkSharedFixedSelector<BaselineLyricsVerse, BaselineSystemLyricsVerse>("Bv", musx::dom::Cmper{3});
}

TEST_CASE("Baseline rejects an incomplete coalesced struct", "[class][baseline]")
{
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    const auto document = baselineDocument();
    const auto report = importBaselines(
        makeDetailClassContainer(0, 0, musx::dom::SCORE_PARTID, {144, 0, 1, 0, 0, 7}, ByteOrder::LittleEndian, 0x03f6), profile, document);
    CHECK(document->getDetails()->getArray<BaselineLyricsChorus>(cmperZero, cmperZero, cmperZero).empty());
    CHECK(report.diagnostics.size() == 1);
}

TEST_CASE("Short partial global lyric arrays inherit the complete score array", "[class][baseline]")
{
    checkShortPartialGlobalLyricArray<BaselineLyricsChorus>(0x03f6);
    checkShortPartialGlobalLyricArray<BaselineLyricsSection>(0x03f7);
    checkShortPartialGlobalLyricArray<BaselineLyricsVerse>(0x03f8);
}

TEST_CASE("Baseline leaves absent global lyric families absent", "[class][baseline]")
{
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    const auto document = baselineDocument();
    const auto report = importBaselines(
        makeDetailClassContainer(0, 0, musx::dom::SCORE_PARTID, baselineWords(-777, 7, ByteOrder::LittleEndian), ByteOrder::LittleEndian, 0x03f8),
        profile, document);
    const auto verses = document->getDetails()->getArray<BaselineLyricsVerse>(cmperZero, cmperZero, cmperZero);
    REQUIRE(verses.size() == 1);
    CHECK(verses.front()->baselineDisplacement == -777);
    CHECK(verses.front()->lyricNumber == 7);
    CHECK(report.findField<BaselineLyricsVerse>("baselineDisplacement", cmperZero, cmperZero, inciZero, cmperZero)->origin == ValueOrigin::LegacyMus);

    const auto choruses = document->getDetails()->getArray<BaselineLyricsChorus>(cmperZero, cmperZero, cmperZero);
    CHECK(choruses.empty());
    CHECK(document->getDetails()->getArray<BaselineLyricsSection>(cmperZero, cmperZero, cmperZero).empty());
}

TEST_CASE("Extended lyric baseline number comes from the integer tag component", "[class][baseline]")
{
    constexpr char extendedVerseTwo[] = {static_cast<char>(0x80), static_cast<char>(0x02), '\0'};
    constexpr char extendedVerseThree[] = {static_cast<char>(0x80), static_cast<char>(0x03), '\0'};
    auto parsed = makeDetailContainer(
        FormatEpoch::UncompressedLegacy, 0, 0, baselineWords(-222, 57, ByteOrder::BigEndian), extendedVerseThree, ByteOrder::BigEndian);
    auto systemTwo = makeDetailContainer(
        FormatEpoch::UncompressedLegacy, 8, 1, baselineWords(0, 88, ByteOrder::BigEndian), extendedVerseTwo, ByteOrder::BigEndian);
    auto system = makeDetailContainer(
        FormatEpoch::UncompressedLegacy, 8, 1, baselineWords(-333, 99, ByteOrder::BigEndian), extendedVerseThree, ByteOrder::BigEndian);
    parsed.blocks.push_back(std::move(systemTwo.blocks.front()));
    parsed.blocks.push_back(std::move(system.blocks.front()));
    const auto document = baselineDocument();
    auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    const auto report = importBaselines(parsed, profile, document);
    const auto verses = document->getDetails()->getArray<BaselineLyricsVerse>(cmperZero, cmperZero, cmperZero);
    REQUIRE(verses.size() == 1);
    CHECK(verses.front()->getInci() == 0);
    CHECK(verses.front()->baselineDisplacement == -222);
    CHECK(verses.front()->lyricNumber == 3);
    CHECK(report.findField<BaselineLyricsVerse>("lyricNumber", cmperZero, cmperZero, inciZero, cmperZero)->origin == ValueOrigin::LegacyMus);
    const auto systemVerses = document->getDetails()->getArray<BaselineSystemLyricsVerse>(cmperZero, cmperEight, cmperOne);
    REQUIRE(systemVerses.size() == 2);
    CHECK(systemVerses[0]->getInci() == 0);
    CHECK(systemVerses[0]->baselineDisplacement == 0);
    CHECK(systemVerses[0]->lyricNumber == 2);
    CHECK(systemVerses[1]->getInci() == 1);
    CHECK(systemVerses[1]->baselineDisplacement == -333);
    CHECK(systemVerses[1]->lyricNumber == 3);
}

TEST_CASE("Baseline supplies each missing global expression family independently", "[class][baseline]")
{
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::LittleEndian;
    const auto document = baselineDocument();
    const auto report = importBaselines(
        makeDetailClassContainer(0, 0, musx::dom::SCORE_PARTID, baselineWords(321, 0, ByteOrder::LittleEndian), ByteOrder::LittleEndian, 0x03f3),
        profile, document);

    const auto above = document->getDetails()->getArray<BaselineExpressionsAbove>(cmperZero, cmperZero, cmperZero);
    REQUIRE(above.size() == 1);
    CHECK(above.front()->baselineDisplacement == 321);
    CHECK(report.findField<BaselineExpressionsAbove>("baselineDisplacement", cmperZero, cmperZero, std::nullopt, cmperZero)->origin
          == ValueOrigin::LegacyMus);

    const auto below = document->getDetails()->getArray<BaselineExpressionsBelow>(cmperZero, cmperZero, cmperZero);
    REQUIRE(below.size() == 1);
    CHECK(below.front()->baselineDisplacement == -144);
    CHECK(report.findField<BaselineExpressionsBelow>("baselineDisplacement", cmperZero, cmperZero, std::nullopt, cmperZero)->origin
          == ValueOrigin::Finale27Default);
    CHECK(report.findField<BaselineExpressionsBelow>("lyricNumber", cmperZero, cmperZero, std::nullopt, cmperZero)->origin
          == ValueOrigin::LegacyBehavior);
}

TEST_CASE("Controlled fixtures recover global baseline defaults", "[class][baseline]")
{
    const auto earliest = readFixture("evidence/F100/F100-global-lyric-baseline.mus");
    const auto earliestVerses = earliest.document->getDetails()->getArray<BaselineLyricsVerse>(cmperZero, cmperZero, cmperZero);
    REQUIRE(earliestVerses.size() == 1);
    CHECK(earliestVerses.front()->baselineDisplacement == -120);
    CHECK(earliestVerses.front()->lyricNumber == 1);
    CHECK(earliest.report.findField<BaselineLyricsVerse>("baselineDisplacement", cmperZero, cmperZero, inciZero, cmperZero)->origin
          == ValueOrigin::LegacyMus);

    const auto coda = readFixture("evidence/F263/F263-fretboards.mus");
    const auto codaChords = coda.document->getDetails()->get<BaselineChords>(cmperZero, cmperZero, cmperZero);
    const auto codaFretboards = coda.document->getDetails()->get<BaselineFretboards>(cmperZero, cmperZero, cmperZero);
    REQUIRE(codaChords);
    REQUIRE(codaFretboards);
    CHECK(codaChords->baselineDisplacement == 288);
    CHECK(codaFretboards->baselineDisplacement == 144);

    const auto fixedFretboards = readFixture("evidence/F372/F372-fretboard-baselines.mus");
    const auto globalFretboard = fixedFretboards.document->getDetails()->get<BaselineFretboards>(cmperZero, cmperZero, cmperZero);
    const auto staffFretboard = fixedFretboards.document->getDetails()->get<BaselineFretboards>(cmperZero, cmperZero, cmperOne);
    const auto systemFretboard = fixedFretboards.document->getDetails()->get<BaselineSystemFretboards>(cmperZero, cmperOne, cmperOne);
    REQUIRE(globalFretboard);
    REQUIRE(staffFretboard);
    REQUIRE(systemFretboard);
    CHECK(globalFretboard->baselineDisplacement == 164);
    CHECK(staffFretboard->baselineDisplacement == -48);
    CHECK(systemFretboard->baselineDisplacement == 24);
    CHECK(fixedFretboards.report.findField<BaselineSystemFretboards>("baselineDisplacement", cmperZero, cmperOne, std::nullopt, cmperOne)->origin
          == ValueOrigin::LegacyMus);

    const auto fixed = readFixture("evidence/F2005/F2005-baseline.mus");
    const auto chords = fixed.document->getDetails()->get<BaselineChords>(cmperZero, cmperZero, cmperZero);
    const auto above = fixed.document->getDetails()->get<BaselineExpressionsAbove>(cmperZero, cmperZero, cmperZero);
    const auto below = fixed.document->getDetails()->get<BaselineExpressionsBelow>(cmperZero, cmperZero, cmperZero);
    REQUIRE(chords);
    REQUIRE(above);
    REQUIRE(below);
    CHECK(chords->baselineDisplacement == 144);
    CHECK(above->baselineDisplacement == 144);
    CHECK(below->baselineDisplacement == -144);

    const auto legacySystem = readFixture("evidence/F98/F98-baseline.mus");
    const auto legacySystemChord = legacySystem.document->getDetails()->get<BaselineSystemChords>(cmperZero, cmperEight, cmperOne);
    REQUIRE(legacySystemChord);
    CHECK(legacySystemChord->baselineDisplacement == 0);
    CHECK_FALSE(legacySystem.document->getDetails()->get<BaselineChords>(cmperZero, cmperEight, cmperOne));
    CHECK(legacySystem.report.findField<BaselineSystemChords>("baselineDisplacement", cmperZero, cmperEight, std::nullopt, cmperOne)->origin
          == ValueOrigin::LegacyMus);

    const auto classRecords = readFixture("evidence/F2012/F2012-baseline.mus");
    const auto verses = classRecords.document->getDetails()->getArray<BaselineLyricsVerse>(cmperZero, cmperZero, cmperZero);
    REQUIRE(verses.size() == 10);
    CHECK(verses.front()->baselineDisplacement == -144);
    CHECK(verses.front()->lyricNumber == 1);
    CHECK(verses.back()->baselineDisplacement == -504);
    CHECK(verses.back()->lyricNumber == 10);

    const auto extended = readFixture("evidence/F97/F97-altnotation.mus");
    const auto extendedVerses = extended.document->getDetails()->getArray<BaselineLyricsVerse>(cmperZero, cmperZero, cmperZero);
    REQUIRE(extendedVerses.size() == 10);
    CHECK(extendedVerses.front()->baselineDisplacement == -144);
    CHECK(extendedVerses.front()->lyricNumber == 1);
    CHECK(extendedVerses.back()->baselineDisplacement == -576);
    CHECK(extendedVerses.back()->lyricNumber == 10);

    const auto oldest = readFixture("evidence/F100/F100-baseline.mus");
    const auto defaultVerses = oldest.document->getDetails()->getArray<BaselineLyricsVerse>(cmperZero, cmperZero, cmperZero);
    CHECK(defaultVerses.empty());
    const auto defaultAbove = oldest.document->getDetails()->get<BaselineExpressionsAbove>(cmperZero, cmperZero, cmperZero);
    const auto defaultBelow = oldest.document->getDetails()->get<BaselineExpressionsBelow>(cmperZero, cmperZero, cmperZero);
    REQUIRE(defaultAbove);
    REQUIRE(defaultBelow);
    CHECK(defaultAbove->baselineDisplacement == 144);
    CHECK(defaultBelow->baselineDisplacement == -144);
    CHECK(oldest.report.findField<BaselineExpressionsAbove>("baselineDisplacement", cmperZero, cmperZero, std::nullopt, cmperZero)->origin
          == ValueOrigin::Finale27Default);
    CHECK(oldest.report.findField<BaselineExpressionsBelow>("baselineDisplacement", cmperZero, cmperZero, std::nullopt, cmperZero)->origin
          == ValueOrigin::Finale27Default);
}

TEST_CASE("Baseline reporting covers the inherited persisted fields", "[class][baseline]")
{
    CHECK(BaselineChords::xmlMappingArray().size() == 2);
    CHECK(BaselineLyricsVerse::xmlMappingArray().size() == 2);
}

} // namespace
} // namespace finale_mus_reader_tests
