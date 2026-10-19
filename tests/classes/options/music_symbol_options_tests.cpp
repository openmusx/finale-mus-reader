// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"
#include "import/support/field_manifest.h"

#include <algorithm>
#include <array>
#include <map>
#include <optional>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using MusicSymbolOptionsTestTarget = finale_mus_reader::options::MusicSymbolOptionsTarget;

musx::dom::DocumentPtr makeMusicSymbolOptionsDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<MusicSymbolOptionsTestTarget>(document);
    for (std::size_t index = 0; index < finale_mus_reader::options::musicSymbolOptionsFields().size(); ++index) {
        options.get()->*finale_mus_reader::options::musicSymbolOptionsFields()[index].member = static_cast<char32_t>(1000 + index);
    }
    document->getOptions()->add(MusicSymbolOptionsTestTarget::XmlNodeName, options);
    return std::move(session).finish();
}

std::shared_ptr<const MusicSymbolOptionsTestTarget> runMusicSymbolImport(
    const finale_mus_reader::container::ParsedContainer& parsed, ImportReport& report, std::optional<SourceVersion> sourceVersion = std::nullopt)
{
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.epoch = parsed.formatEpoch;
    switch (profile.epoch) {
    case FormatEpoch::CodaBanner: profile.version = SourceVersion{.major = 2, .minor = 6}; break;
    case FormatEpoch::UncompressedLegacy: profile.version = SourceVersion{.major = 9, .minor = 0}; break;
    case FormatEpoch::DclLegacy: profile.version = SourceVersion{.major = 7}; break;
    case FormatEpoch::ZlibLegacy: profile.version = SourceVersion{.major = 16}; break;
    }
    if (sourceVersion) {
        profile.version = *sourceVersion;
    }
    profile.byteOrder = parsed.byteOrder;
    const auto document = makeMusicSymbolOptionsDocument();
    const auto reference = makeMusicSymbolOptionsDocument();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{
        LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::options::importMusicSymbolOptions(context);
    return document->getOptions()->get<MusicSymbolOptionsTestTarget>();
}

TEST_CASE("Finale 2012 music-symbol long array is selected by payload shape", "[class]")
{
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        std::vector<std::int16_t> words(6);
        for (std::size_t index = 0; index < finale_mus_reader::options::musicSymbolOptionsFields().size(); ++index) {
            const auto value = static_cast<std::uint32_t>(0x10000 + index * 17);
            words.push_back(static_cast<std::int16_t>(value));
            words.push_back(static_cast<std::int16_t>(value >> 16U));
        }
        words.resize(words.size() + 2);
        ImportReport report(FormatEpoch::ZlibLegacy);
        const auto result = runMusicSymbolImport(makeClassContainer(0x0059, words, byteOrder), report);

        for (std::size_t index = 0; index < finale_mus_reader::options::musicSymbolOptionsFields().size(); ++index) {
            const auto& descriptor = finale_mus_reader::options::musicSymbolOptionsFields()[index];
            const auto expected = static_cast<char32_t>(0x10000 + index * 17);
            REQUIRE(result.get()->*descriptor.member == expected);
            const auto* info = report.findField<MusicSymbolOptionsTestTarget>(descriptor.memberName);
            REQUIRE(info);
            REQUIRE(info->origin == ValueOrigin::LegacyMus);
            REQUIRE(info->rawValue == static_cast<std::int64_t>(expected));
            REQUIRE(info->sourceIdentity == 0x0059);
        }
    }
}

TEST_CASE("Pre-2012 class 0x0059 recovers its two straight-flag symbols", "[class]")
{
    ImportReport report(FormatEpoch::ZlibLegacy);
    std::vector<std::int16_t> words{115, 83, 0, 0, 0, 0};
    const auto result = runMusicSymbolImport(makeClassContainer(0x0059, words, ByteOrder::LittleEndian), report);

    for (std::size_t index = 0; index < finale_mus_reader::options::musicSymbolOptionsFields().size(); ++index) {
        const auto& descriptor = finale_mus_reader::options::musicSymbolOptionsFields()[index];
        const auto isStraightUp = descriptor.member == &MusicSymbolOptionsTestTarget::flagStraightUp;
        const auto isStraightDown = descriptor.member == &MusicSymbolOptionsTestTarget::flagStraightDown;
        const auto expected = isStraightUp ? 115 : isStraightDown ? 83 : static_cast<int>(1000 + index);
        REQUIRE(result.get()->*descriptor.member == static_cast<char32_t>(expected));
        const auto* info = report.findField<MusicSymbolOptionsTestTarget>(descriptor.memberName);
        REQUIRE(info);
        REQUIRE(info->origin == (isStraightUp || isStraightDown ? ValueOrigin::LegacyMus : ValueOrigin::Finale27Default));
    }
}

const char* musicSymbolSelectorTag(std::uint16_t selector)
{
    switch (selector) {
    case 5: return "05";
    case 6: return "06";
    case 7: return "07";
    case 8: return "08";
    case 9: return "09";
    case 10: return "10";
    case 11: return "11";
    case 12: return "12";
    case 18: return "18";
    case 19: return "19";
    case 23: return "23";
    case 38: return "38";
    case 42: return "42";
    case 43: return "43";
    case 46: return "46";
    case 69: return "69";
    case 75: return "75";
    }
    throw std::logic_error("unexpected music-symbol selector");
}

std::map<std::uint16_t, std::array<std::int16_t, 6>> narrowMusicSymbolWords()
{
    std::map<std::uint16_t, std::array<std::int16_t, 6>> result;
    for (std::size_t index = 0; index < finale_mus_reader::options::musicSymbolOptionsFields().size(); ++index) {
        const auto& field = finale_mus_reader::options::musicSymbolOptionsFields()[index];
        if (field.narrowEra != finale_mus_reader::options::NarrowMusicSymbolEra::ZlibOnly) {
            result[field.narrowSource.selector][field.narrowSource.word] = static_cast<std::int16_t>(20 + index);
        }
    }
    return result;
}

finale_mus_reader::container::ParsedContainer narrowMusicSymbolContainer(FormatEpoch epoch)
{
    const auto values = narrowMusicSymbolWords();
    if (epoch == FormatEpoch::ZlibLegacy) {
        std::vector<SyntheticClassRow> rows;
        for (const auto& [selector, words] : values) {
            rows.push_back({finale_mus_reader::numericGlobalClass(selector), {words.begin(), words.end()}});
        }
        std::vector<std::int16_t> partsWords(14);
        const auto fields = finale_mus_reader::options::musicSymbolOptionsFields();
        for (std::size_t index = 0; index < fields.size(); ++index) {
            const auto& field = fields[index];
            if (field.narrowEra != finale_mus_reader::options::NarrowMusicSymbolEra::ZlibOnly) {
                continue;
            }
            partsWords[field.narrowSource.word] = static_cast<std::int16_t>(20 + index);
        }
        rows.push_back({finale_mus_reader::numericGlobalClass(18), partsWords});
        return makeClassContainer(rows, ByteOrder::BigEndian);
    }

    std::vector<SyntheticRow> rows;
    for (const auto& [selector, words] : values) {
        rows.push_back({GLOBALS_CMPER, musicSymbolSelectorTag(selector), words});
    }
    return makeContainer(rows, epoch);
}

TEST_CASE("Narrow music-symbol selectors recover in every pre-Unicode epoch", "[class]")
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        ImportReport report(epoch);
        const auto options = runMusicSymbolImport(narrowMusicSymbolContainer(epoch), report);
        for (std::size_t index = 0; index < finale_mus_reader::options::musicSymbolOptionsFields().size(); ++index) {
            const auto& descriptor = finale_mus_reader::options::musicSymbolOptionsFields()[index];
            INFO(descriptor.memberName);
            const auto* info = report.findField<MusicSymbolOptionsTestTarget>(descriptor.memberName);
            REQUIRE(info);
            const auto applies =
                descriptor.narrowEra == finale_mus_reader::options::NarrowMusicSymbolEra::Any
                || (descriptor.narrowEra != finale_mus_reader::options::NarrowMusicSymbolEra::ZlibOnly && epoch != FormatEpoch::CodaBanner)
                || (descriptor.narrowEra == finale_mus_reader::options::NarrowMusicSymbolEra::ZlibOnly && epoch == FormatEpoch::ZlibLegacy);
            const auto sharedApplies =
                descriptor.sharedSource
                && (descriptor.sharedEra == finale_mus_reader::options::SharedMusicSymbolEra::CodaOnly
                        ? epoch == FormatEpoch::CodaBanner
                        : descriptor.sharedEra == finale_mus_reader::options::SharedMusicSymbolEra::PreZlib && epoch != FormatEpoch::ZlibLegacy);
            if (applies) {
                REQUIRE(options.get()->*descriptor.member == static_cast<char32_t>(20 + index));
                REQUIRE(info->origin == ValueOrigin::LegacyMus);
            } else if (sharedApplies) {
                REQUIRE(options.get()->*descriptor.member == options.get()->*(*descriptor.sharedSource));
                REQUIRE(info->origin == ValueOrigin::LegacyBehavior);
            } else {
                REQUIRE(options.get()->*descriptor.member == static_cast<char32_t>(1000 + index));
                REQUIRE(info->origin == ValueOrigin::Finale27Default);
            }
        }
    }
}

TEST_CASE("Expanded music-symbol characters begin with Finale 3.5", "[class]")
{
    using finale_mus_reader::options::NarrowMusicSymbolEra;
    constexpr std::array gatedMembers{
        std::string_view("backRepeatDot"),
        std::string_view("eightVaUp"),
        std::string_view("forwardRepeatDot"),
        std::string_view("dblWholeSlash"),
        std::string_view("eightVbDown"),
        std::string_view("oneBarRepeat"),
        std::string_view("quarterSlash"),
        std::string_view("slashBar"),
        std::string_view("twoBarRepeat"),
        std::string_view("flagStraightUp"),
        std::string_view("flagStraightDown"),
    };
    const auto fields = finale_mus_reader::options::musicSymbolOptionsFields();
    for (const auto& field : fields) {
        const bool expected = std::ranges::find(gatedMembers, field.memberName) != gatedMembers.end();
        INFO(field.memberName);
        REQUIRE((field.narrowEra == NarrowMusicSymbolEra::Finale35AndLater) == expected);
    }

    for (const auto& [version, recovers] : {
             std::pair{SourceVersion{.major = 3, .minor = 2}, false},
             std::pair{SourceVersion{.major = 3, .minor = 5}, true},
         }) {
        ImportReport report(FormatEpoch::UncompressedLegacy);
        const auto options = runMusicSymbolImport(narrowMusicSymbolContainer(FormatEpoch::UncompressedLegacy), report, version);
        for (const auto memberName : gatedMembers) {
            const auto field = std::ranges::find(fields, memberName, &finale_mus_reader::options::MusicSymbolOptionsField::memberName);
            REQUIRE(field != fields.end());
            const auto index = static_cast<std::size_t>(field - fields.begin());
            INFO(memberName);
            REQUIRE(options.get()->*field->member == static_cast<char32_t>(recovers ? 20 + index : 1000 + index));
            REQUIRE(report.findField<MusicSymbolOptionsTestTarget>(memberName)->origin
                    == (recovers ? ValueOrigin::LegacyMus : ValueOrigin::Finale27Default));
        }
    }
}

TEST_CASE("Later expanded music-symbol characters begin with Finale 3.5.1", "[class]")
{
    using finale_mus_reader::options::NarrowMusicSymbolEra;
    constexpr std::array gatedMembers{
        std::string_view("flag16Up"),
        std::string_view("flag16Down"),
        std::string_view("fifteenMaUp"),
        std::string_view("fifteenMbDown"),
        std::string_view("trillChar"),
        std::string_view("wiggleChar"),
    };
    const auto fields = finale_mus_reader::options::musicSymbolOptionsFields();
    for (const auto& field : fields) {
        const bool expected = std::ranges::find(gatedMembers, field.memberName) != gatedMembers.end();
        INFO(field.memberName);
        REQUIRE((field.narrowEra == NarrowMusicSymbolEra::Finale351AndLater) == expected);
    }

    for (const auto& [version, recovers] : {
             std::pair{SourceVersion{.major = 3, .minor = 5}, false},
             std::pair{SourceVersion{.major = 3, .minor = 5, .maint = 1}, true},
         }) {
        ImportReport report(FormatEpoch::UncompressedLegacy);
        const auto options = runMusicSymbolImport(narrowMusicSymbolContainer(FormatEpoch::UncompressedLegacy), report, version);
        for (const auto memberName : gatedMembers) {
            const auto field = std::ranges::find(fields, memberName, &finale_mus_reader::options::MusicSymbolOptionsField::memberName);
            REQUIRE(field != fields.end());
            const auto index = static_cast<std::size_t>(field - fields.begin());
            INFO(memberName);
            REQUIRE(options.get()->*field->member == static_cast<char32_t>(recovers ? 20 + index : 1000 + index));
            REQUIRE(report.findField<MusicSymbolOptionsTestTarget>(memberName)->origin
                    == (recovers ? ValueOrigin::LegacyMus : ValueOrigin::Finale27Default));
        }
    }
}

TEST_CASE("Coda key-signature characters decode ordinary accidental bytes through the key font", "[class][reader]")
{
    const auto result = readFixture("evidence/F263/F263-musechars.mus", fixtureLegacySymbolFonts);
    const auto options = result.document->getOptions()->get<MusicSymbolOptionsTestTarget>();
    REQUIRE(options);
    for (const auto member : {std::string_view("keySigNatural"), std::string_view("keySigFlat"), std::string_view("keySigSharp"),
             std::string_view("keySigDblFlat"), std::string_view("keySigDblSharp")}) {
        const auto* info = result.report.findField<MusicSymbolOptionsTestTarget>(member);
        INFO(member);
        REQUIRE(info);
        REQUIRE(info->origin == ValueOrigin::LegacyBehavior);
    }
    REQUIRE(options->keySigNatural == 110);
    REQUIRE(options->keySigFlat == 98);
    REQUIRE(options->keySigSharp == 35);
    REQUIRE(options->keySigDblFlat == 186);
    REQUIRE(options->keySigDblSharp == 220);
}

TEST_CASE("Coda leaves 8vb-down at the pinned default", "[class][reader]")
{
    for (const auto fixture : {"evidence/F100/F100-baseline.mus", "evidence/F263/F263-musechars.mus"}) {
        const auto result = readFixture(fixture);
        const auto options = result.document->getOptions()->get<MusicSymbolOptionsTestTarget>();
        REQUIRE(options);
        INFO(fixture);
        REQUIRE(options->eightVbDown == 195);
        const auto* info = result.report.findField<MusicSymbolOptionsTestTarget>("eightVbDown");
        REQUIRE(info);
        REQUIRE(info->origin == ValueOrigin::Finale27Default);
        REQUIRE(info->rawValue == 195);
    }
}

TEST_CASE("Finale 1 flag character controls map to primary and second flags", "[class][reader]")
{
    const auto result = readFixture("evidence/F100/F100-flagchars.mus", fixtureLegacySymbolFonts);
    const auto options = result.document->getOptions()->get<MusicSymbolOptionsTestTarget>();
    REQUIRE(options);

    CHECK(options->flagUp == 115);
    CHECK(options->flagDown == 87);
    CHECK(options->flag2Up == 184);
    CHECK(options->flag2Down == 186);
    for (const auto member : {std::string_view("flagUp"), std::string_view("flagDown"), std::string_view("flag2Up"), std::string_view("flag2Down")}) {
        INFO(member);
        REQUIRE(result.report.findField<MusicSymbolOptionsTestTarget>(member)->origin == ValueOrigin::LegacyMus);
    }

    CHECK(options->flag16Up == 114);
    CHECK(options->flag16Down == 82);
    CHECK(result.report.findField<MusicSymbolOptionsTestTarget>("flag16Up")->origin == ValueOrigin::Finale27Default);
    CHECK(result.report.findField<MusicSymbolOptionsTestTarget>("flag16Down")->origin == ValueOrigin::Finale27Default);
}

TEST_CASE("Pre-Unicode music symbols use their category font encodings", "[class][reader]")
{
    const auto accidentalResult = readFixture("evidence/F100/F100-accis.mus", fixtureLegacySymbolFonts);
    const auto accidentalOptions = accidentalResult.document->getOptions()->get<MusicSymbolOptionsTestTarget>();
    REQUIRE(accidentalOptions);
    CHECK(accidentalOptions->dblFlat == 0xba);
    CHECK(accidentalOptions->dblSharp == 0xdc);
    CHECK(accidentalOptions->chordDblFlat == 0x222b);
    CHECK(accidentalOptions->chordDblSharp == 0x2039);

    const auto keyResult = readFixture("evidence/F100/F100-key-font.mus", fixtureLegacySymbolFonts);
    const auto keyOptions = keyResult.document->getOptions()->get<MusicSymbolOptionsTestTarget>();
    REQUIRE(keyOptions);
    CHECK(keyOptions->dblFlat == 0xba);
    CHECK(keyOptions->dblSharp == 0xdc);
    CHECK(keyOptions->keySigDblFlat == 0x222b);
    CHECK(keyOptions->keySigDblSharp == 0x2039);
    CHECK(keyResult.report.findField<MusicSymbolOptionsTestTarget>("keySigDblFlat")->rawValue == 0xba);
    CHECK(keyResult.report.findField<MusicSymbolOptionsTestTarget>("keySigDblSharp")->rawValue == 0xdc);
}

TEST_CASE("Time-signature symbols are shared with parts before the zlib epoch", "[class][reader]")
{
    const auto result = readFixture("evidence/F2006/F2006-timesig-plus.mus");
    const auto options = result.document->getOptions()->get<MusicSymbolOptionsTestTarget>();
    REQUIRE(options);
    REQUIRE(options->timeSigPlus == 44);
    REQUIRE(options->timeSigPlusParts == 44);
    REQUIRE(options->timeSigAbrvCommonParts == options->timeSigAbrvCommon);
    REQUIRE(options->timeSigAbrvCutParts == options->timeSigAbrvCut);
    REQUIRE(result.report.findField<MusicSymbolOptionsTestTarget>("timeSigPlus")->origin == ValueOrigin::LegacyMus);
    REQUIRE(result.report.findField<MusicSymbolOptionsTestTarget>("timeSigPlusParts")->origin == ValueOrigin::LegacyBehavior);
    REQUIRE(result.report.findField<MusicSymbolOptionsTestTarget>("timeSigAbrvCommonParts")->origin == ValueOrigin::LegacyBehavior);
    REQUIRE(result.report.findField<MusicSymbolOptionsTestTarget>("timeSigAbrvCutParts")->origin == ValueOrigin::LegacyBehavior);
}

TEST_CASE("Zlib music-symbol options store separate parts time-signature symbols", "[class][reader]")
{
    const auto result = readFixture("evidence/F2008/F2008-timesig-plusparts.mus");
    const auto options = result.document->getOptions()->get<MusicSymbolOptionsTestTarget>();
    REQUIRE(options);
    REQUIRE(options->timeSigPlus == 43);
    REQUIRE(options->timeSigPlusParts == 42);
    REQUIRE(options->timeSigAbrvCommon == 99);
    REQUIRE(static_cast<std::uint32_t>(options->timeSigAbrvCommonParts) == 98);
    REQUIRE(options->timeSigAbrvCut == 67);
    REQUIRE(static_cast<std::uint32_t>(options->timeSigAbrvCutParts) == 69);
    for (const auto member :
        {std::string_view("timeSigPlusParts"), std::string_view("timeSigAbrvCommonParts"), std::string_view("timeSigAbrvCutParts")}) {
        const auto* info = result.report.findField<MusicSymbolOptionsTestTarget>(member);
        REQUIRE(info);
        REQUIRE(info->origin == ValueOrigin::LegacyMus);
        REQUIRE(info->sourceIdentity == 0x0020);
    }
    REQUIRE(result.report.findField<MusicSymbolOptionsTestTarget>("timeSigPlusParts")->rawValue == 42);
    REQUIRE(result.report.findField<MusicSymbolOptionsTestTarget>("timeSigAbrvCommonParts")->rawValue == 98);
    REQUIRE(result.report.findField<MusicSymbolOptionsTestTarget>("timeSigAbrvCutParts")->rawValue == 69);
}

TEST_CASE("Controlled Finale 2012 music symbols recover all persisted fields", "[class][reader]")
{
    const auto result = readFixture("evidence/F2012/F2012-baseline.mus");
    const auto options = result.document->getOptions()->get<MusicSymbolOptionsTestTarget>();
    REQUIRE(options);
    REQUIRE(options->noteheadQuarter == 207);
    REQUIRE(options->flag2Down == 0xF0EF);
    REQUIRE(options->flagStraightDown == 0);
    for (const auto& descriptor : finale_mus_reader::options::musicSymbolOptionsFields()) {
        INFO(descriptor.memberName);
        const auto* info = result.report.findField<MusicSymbolOptionsTestTarget>(descriptor.memberName);
        REQUIRE(info);
        REQUIRE(info->origin == ValueOrigin::LegacyMus);
    }
}

TEST_CASE("Controlled straight flags use selector 75 before and after Unicode expansion", "[class][reader]")
{
    const auto narrow = readFixture("evidence/F2000/F2000-lyropts-align-just.mus");
    const auto narrowOptions = narrow.document->getOptions()->get<MusicSymbolOptionsTestTarget>();
    REQUIRE(narrowOptions);
    REQUIRE(narrowOptions->flagStraightUp == 115);
    REQUIRE(narrowOptions->flagStraightDown == 83);
    REQUIRE(narrow.report.findField<MusicSymbolOptionsTestTarget>("flagStraightUp")->origin == ValueOrigin::LegacyMus);
    REQUIRE(narrow.report.findField<MusicSymbolOptionsTestTarget>("flagStraightDown")->origin == ValueOrigin::LegacyMus);

    const auto unicode = readFixture("evidence/F2012/F2012-upstem-flags.mus");
    const auto unicodeOptions = unicode.document->getOptions()->get<MusicSymbolOptionsTestTarget>();
    REQUIRE(unicodeOptions);
    REQUIRE(unicodeOptions->flagStraightUp == 183);
    REQUIRE(unicodeOptions->flagStraightDown == 183);
    REQUIRE(unicode.report.findField<MusicSymbolOptionsTestTarget>("flagStraightUp")->sourceIdentity == 0x0059);
    REQUIRE(unicode.report.findField<MusicSymbolOptionsTestTarget>("flagStraightDown")->sourceIdentity == 0x0059);
}

TEST_CASE("Finale 97 stores the default measure rest in selector 9 word 4", "[class][reader]")
{
    for (const auto& [fixture, expected] : {
             std::pair{"evidence/F97/Fin97-baseline.mus", char32_t{183}},
             std::pair{"evidence/F97/F97-def-measrest.mus", char32_t{206}},
         }) {
        const auto result = readFixture(fixture, fixtureLegacySymbolFonts);
        const auto options = result.document->getOptions()->get<MusicSymbolOptionsTestTarget>();
        REQUIRE(options);
        INFO(fixture);
        REQUIRE(options->restDefMeas == expected);
        const auto* info = result.report.findField<MusicSymbolOptionsTestTarget>("restDefMeas");
        REQUIRE(info);
        REQUIRE(info->origin == ValueOrigin::LegacyMus);
        REQUIRE(info->rawValue == static_cast<std::int64_t>(expected));
        REQUIRE(info->sourceIdentity == finale_mus_reader::numericGlobalTag(9));
    }
}

TEST_CASE("A zero default measure rest uses the stored whole-rest glyph", "[class]")
{
    ImportReport report(FormatEpoch::UncompressedLegacy);
    std::array<std::int16_t, 6> wholeRestWords{};
    wholeRestWords[3] = 183;
    std::array<std::int16_t, 6> defaultMeasureRestWords{};
    const auto options = runMusicSymbolImport(
        makeContainer({{GLOBALS_CMPER, "08", wholeRestWords}, {GLOBALS_CMPER, "09", defaultMeasureRestWords}}, FormatEpoch::UncompressedLegacy),
        report);

    REQUIRE(options->restWhole == 183);
    REQUIRE(options->restDefMeas == options->restWhole);
    const auto* info = report.findField<MusicSymbolOptionsTestTarget>("restDefMeas");
    REQUIRE(info);
    REQUIRE(info->origin == ValueOrigin::LegacyMusAdjusted);
    REQUIRE(info->rawValue == 0);
    REQUIRE(info->sourceIdentity == finale_mus_reader::numericGlobalTag(9));
}
} // namespace
} // namespace finale_mus_reader_tests
