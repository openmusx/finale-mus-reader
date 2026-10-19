// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

/// @brief A LyricOptions seeded with the opposite of everything the reader asserts.
/// @details Every value here contradicts what a legacy document means, so an implementation
/// that inherited the seed rather than asserting over it would fail. `hyphenChar` is the
/// deliberate exception: the reader must leave that one exactly as seeded.
musx::dom::DocumentPtr makeLyricDocument()
{
    using Lyrics = musx::dom::options::LyricOptions;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<Lyrics>(document);
    options->hyphenChar = U'~';
    options->useAltHyphenFont = true;
    options->wordExtLineWidth = 115;
    options->lyricUseEdgePunctuation = false;
    for (const auto type : {Lyrics::SyllablePosStyleType::Default, Lyrics::SyllablePosStyleType::WordExt, Lyrics::SyllablePosStyleType::First,
             Lyrics::SyllablePosStyleType::SystemStart}) {
        auto style = std::make_shared<Lyrics::SyllablePosStyle>();
        style->on = true;
        options->syllablePosStyles[type] = std::move(style);
    }
    auto zeroOffset = std::make_shared<Lyrics::WordExtConnectStyle>();
    zeroOffset->connectIndex = Lyrics::WordExtConnectIndex::SystemRight;
    zeroOffset->xOffset = 91;
    zeroOffset->yOffset = 92;
    options->wordExtConnectStyles[Lyrics::WordExtConnectStyleType::ZeroOffset] = std::move(zeroOffset);
    document->getOptions()->add(Lyrics::XmlNodeName, options);
    return std::move(session).finish();
}

void testLyricWordExtConnectionLayouts()
{
    using Lyrics = musx::dom::options::LyricOptions;
    using ConnectType = Lyrics::WordExtConnectStyleType;
    const auto runImport = [](std::vector<SyntheticRow> rows) {
        const auto document = makeLyricDocument();
        const auto reference = makeLyricDocument();
        auto profile = profileFor(9);
        profile.epoch = FormatEpoch::DclLegacy;
        ImportReport report(FormatEpoch::UncompressedLegacy);
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{LegacyRecordIndex::build(makeContainer(rows, FormatEpoch::DclLegacy)), profile, noSource,
            document, reference, report, pending, construction};
        finale_mus_reader::options::importLyricOptions(context);
        return document->getOptions()->get<Lyrics>();
    };

    const std::vector<SyntheticRow> earlyRows{
        {GLOBALS_CMPER, "55", {16, 4, 1, 17, 0, 0}},
        {GLOBALS_CMPER, "55", {19, 0, 0, 20, 0, 0}},
        {GLOBALS_CMPER, "55", {17, 8, 0, 18, -8, 0}},
        {GLOBALS_CMPER, "55", {16, 42, 0, 0, 0, 0}},
    };
    const auto early = runImport(earlyRows);
    expectMapping(early->wordExtConnectStyles.at(ConnectType::SystemStart)->connectIndex == Lyrics::WordExtConnectIndex::DurationLyrBaseline,
        "The eight-style word-extension table did not retain its third element");
    expectMapping(early->wordExtConnectStyles.at(ConnectType::DottedEnd)->xOffset == 8
                      && early->wordExtConnectStyles.at(ConnectType::DurationEnd)->xOffset == -8,
        "The eight-style word-extension table was shifted into the later layout");
    expectMapping(early->wordExtConnectStyles.at(ConnectType::ZeroOffset)->xOffset == 91
                      && early->wordExtConnectStyles.at(ConnectType::ZeroOffset)->yOffset == 92,
        "The absent ninth word-extension style overwrote the seeded value");

    auto laterRows = earlyRows;
    laterRows.push_back({GLOBALS_CMPER, "55", {17, 7, 8, 0, 0, 0}});
    const auto later = runImport(std::move(laterRows));
    expectMapping(later->wordExtConnectStyles.at(ConnectType::ZeroOffset)->connectIndex == Lyrics::WordExtConnectIndex::HeadRightLyrBaseline
                      && later->wordExtConnectStyles.at(ConnectType::ZeroOffset)->xOffset == 7
                      && later->wordExtConnectStyles.at(ConnectType::ZeroOffset)->yOffset == 8,
        "The ninth word-extension style was not read from the longer layout");
}

// "Ignore Syllable Edge Punctuation" arrives with Finale 2012 and is the one lyric field whose
// word exists before its meaning does: selector 57 word 4 is present but clear in Finale
// 2004-2010 documents, and those documents do not ignore the punctuation. Reading the word on
// such a document would invert the setting, so the gate is what this test pins -- one record,
// read under two versions, must give opposite answers.
void testLyricEdgePunctuationVersionGate()
{
    using Lyrics = musx::dom::options::LyricOptions;
    // Selector 57 with word 4 set: "use edge punctuation", the Finale 2012 spelling.
    const auto parsed = makeClassContainer(0x0047, {1, 0, 38, 1, 1, 0}, ByteOrder::LittleEndian);
    const auto runAt = [&](std::uint8_t major) {
        const auto document = makeLyricDocument();
        const auto reference = makeLyricDocument();
        auto profile = profileFor(major);
        profile.epoch = FormatEpoch::ZlibLegacy;
        profile.byteOrder = ByteOrder::LittleEndian;
        ImportReport report(FormatEpoch::UncompressedLegacy);
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{
            LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::options::importLyricOptions(context);
        return std::make_pair(document->getOptions()->get<Lyrics>(), field(report, "options.lyricOptions.lyricUseEdgePunctuation").origin);
    };

    // Finale 2011 is the first release that stores it, and Finale 2012 keeps doing so.
    for (const std::uint8_t major : {std::uint8_t{16}, std::uint8_t{17}}) {
        const auto [live, origin] = runAt(major);
        expectMapping(live->lyricUseEdgePunctuation && origin == ValueOrigin::LegacyMus,
            "A Finale 2011-or-later document did not read edge punctuation from selector 57"
            " word 4");
    }
    // Finale 2010 by version, on a record carrying the very same word. The era has no such
    // setting, so the word means nothing and the answer must come from era behavior instead:
    // Finale 2010 documents carry 0 here and Finale 2011 documents carry 1.
    const auto [ten, tenOrigin] = runAt(15);
    expectMapping(
        ten->lyricUseEdgePunctuation && tenOrigin == ValueOrigin::LegacyBehavior, "A pre-Finale-2011 document read a word its era does not use");
    // And a zlib document whose version could not be recovered falls to the same era behavior,
    // which is the right answer for every release but one.
    const auto document = makeLyricDocument();
    const auto reference = makeLyricDocument();
    SourceProfile unknown(FormatEpoch::ZlibLegacy);
    unknown.byteOrder = ByteOrder::LittleEndian;
    ImportReport report(FormatEpoch::UncompressedLegacy);
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{
        LegacyRecordIndex::build(parsed), unknown, noSource, document, reference, report, pending, construction};
    finale_mus_reader::options::importLyricOptions(context);
    expectMapping(document->getOptions()->get<Lyrics>()->lyricUseEdgePunctuation
                      && field(report, "options.lyricOptions.lyricUseEdgePunctuation").origin == ValueOrigin::LegacyBehavior,
        "A zlib document with no recoverable version did not fall back to era behavior");
}

// The punctuation tail is readable only where its encoding is verified. Finale 2011 stores the
// switch but predates Unicode, and no document of that release carries a tail for anyone to
// check against, so a tail found there is reported and left rather than guessed at.
void testLyricPunctuationTailEncodingGate()
{
    using Lyrics = musx::dom::options::LyricOptions;
    // Selector 57 with a four-character tail, the shape the Finale 2012 fixture carries.
    // Word 6 onward differs by era, so each version is given the layout its release writes:
    // 16-bit code units for Finale 2012, packed bytes for Finale 2011. Little-endian in both,
    // which is what makes the transposition hazard real rather than theoretical.
    const std::vector<std::int16_t> unicodeTail{1, 0, 38, 1, 1, 0, 0x23, 0x40, 0x25, 0x26, 0, 0};
    const std::vector<std::int16_t> byteTail{1, 0, 38, 1, 1, 0, static_cast<std::int16_t>(0x4023), static_cast<std::int16_t>(0x2625), 0, 0, 0, 0};
    const auto runAt = [&](std::uint8_t major) {
        const auto parsed = makeClassContainer(0x0047,
            finale_mus_reader::VersionBound{major} >= finale_mus_reader::versions::finale2012 ? unicodeTail : byteTail, ByteOrder::LittleEndian);
        const auto document = makeLyricDocument();
        const auto reference = makeLyricDocument();
        auto profile = profileFor(major);
        profile.epoch = FormatEpoch::ZlibLegacy;
        profile.byteOrder = ByteOrder::LittleEndian;
        ImportReport report(FormatEpoch::UncompressedLegacy);
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{
            LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::options::importLyricOptions(context);
        return std::make_pair(document->getOptions()->get<Lyrics>(), report);
    };

    const auto [twelve, twelveReport] = runAt(17);
    expectMapping(twelve->lyricPunctuationToIgnore == "#@%&", "A Finale 2012 tail was not decoded as UTF-16");
    expectMapping(field(twelveReport, "options.lyricOptions.lyricPunctuationToIgnore").origin == ValueOrigin::LegacyMus,
        "A decoded Finale 2012 tail was not reported as read");

    // Finale 2011 reads the switch from the same record but stores the tail as packed 8-bit
    // bytes. Read through the word path these same bytes would come back transposed, which is
    // what this half of the test pins: the words above hold 0x2340, 0x2526 and so on, so a
    // reader that treated them as code units would produce the pairs in the wrong order.
    const auto [eleven, elevenReport] = runAt(16);
    expectMapping(eleven->lyricUseEdgePunctuation, "A Finale 2011 document did not read the edge punctuation switch");
    expectMapping(eleven->lyricPunctuationToIgnore == "#@%&", "A Finale 2011 tail was not decoded as packed bytes");
    expectMapping(field(elevenReport, "options.lyricOptions.lyricPunctuationToIgnore").origin == ValueOrigin::LegacyMus,
        "A decoded Finale 2011 tail was not reported as read");
}

// Two lyric settings postdate Finale 2012, the last release this reader opens, so no legacy
// document can state either one. They are treated differently on purpose, and the difference is
// the whole point of this test: the switch is known false and is asserted over whatever the
// baseline says, while the hyphen character is left exactly as seeded because restating U+002D
// in code would be a second copy of a fact the pinned resource already carries.
void testLyricPostFormatAssertions()
{
    using Lyrics = musx::dom::options::LyricOptions;
    // A source with none of the six lyric selectors, which is what makes every value below
    // either an assertion or the seed.
    const auto index = LegacyRecordIndex::build(makeContainer({
        {GLOBALS_CMPER, "94", {0, 0, 0, 0, 0, 0}},
    }));
    const auto document = makeLyricDocument();
    const auto reference = makeLyricDocument();
    ImportReport report(FormatEpoch::UncompressedLegacy);
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profileFor(9), noSource, document, reference, report, pending, construction};
    finale_mus_reader::options::importLyricOptions(context);

    const auto lyrics = document->getOptions()->get<Lyrics>();
    expectMapping(!lyrics->useAltHyphenFont, "The alternate hyphen font switch was inherited rather than asserted");
    expectMapping(field(report, "options.lyricOptions.useAltHyphenFont").origin == ValueOrigin::LegacyBehavior,
        "The alternate hyphen font switch was not reported as era behavior");
    // The one field the reader must not touch. A hard-coded U+002D would show up here as the
    // seeded tilde being overwritten.
    expectMapping(lyrics->hyphenChar == U'~', "The hyphen character was asserted in code instead of taken from the seed");
    expectMapping(field(report, "options.lyricOptions.hyphenChar").origin == ValueOrigin::MusxOnly,
        "The post-legacy hyphen character was not reported as MUSX-only");
    // musxdom populates altHyphenFont only from an <altHyphenFont> element and synthesizes one
    // in integrityCheck, so the field surface is reported without claiming a source.
    for (const auto* member : {"altHyphenFont.fontId", "altHyphenFont.fontSize", "altHyphenFont.effects"}) {
        expectMapping(field(report, std::string("options.lyricOptions.") + member).origin == ValueOrigin::MusxOnly,
            std::string("The alternate hyphen font field was not MUSX-only: ") + member);
    }

    // The three assertions that do contradict the seed, for contrast: without any of the six
    // selectors the reader must not leave a document claiming these.
    expectMapping(lyrics->wordExtLineWidth == 224, "The word extension line width was inherited rather than asserted");
    expectMapping(lyrics->lyricUseEdgePunctuation, "The syllable edge punctuation setting was inherited rather than asserted");
    for (const auto type : {Lyrics::SyllablePosStyleType::WordExt, Lyrics::SyllablePosStyleType::First, Lyrics::SyllablePosStyleType::SystemStart}) {
        expectMapping(!lyrics->syllablePosStyles.at(type)->on, "An optional syllable position was left switched on by an era that has no record"
                                                               " for it");
    }
}

void testLyricOptionsRecovery()
{
    using Lyrics = musx::dom::options::LyricOptions;
    using SyllableType = Lyrics::SyllablePosStyleType;
    using ConnectType = Lyrics::WordExtConnectStyleType;
    using ConnectIndex = Lyrics::WordExtConnectIndex;
    using AlignJustify = musx::dom::AlignJustify;
    const auto read = [](const char* relative) {
        return Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / relative);
    };

    struct Expected
    {
        const char* path;
        const char* era;
        FormatEpoch epoch;
        int maxHyphenSeparation;
        int wordExtLineWidth;
        bool optionalPositionsOn;
        int wordExtHorzOffset;
        int wordExtVertOffset;
    };
    // The cases cover every epoch that stores the line width. Coda-banner documents state
    // neither it nor the syllable positions.
    const Expected fixtures[] = {
        {"evidence/F100/F100-baseline.mus", "Finale 1.0.0", FormatEpoch::CodaBanner, 144, 224, false, 4, 4},
        {"evidence/F263/F263-baseline.mus", "Finale 2.6.3", FormatEpoch::CodaBanner, 144, 224, false, 4, 4},
        {"evidence/F372/F372-baseline.mus", "Finale 3.7.2", FormatEpoch::UncompressedLegacy, 144, 118, false, 4, 4},
        {"evidence/F97/Fin97-baseline.mus", "Finale 97", FormatEpoch::UncompressedLegacy, 144, 118, false, 4, 4},
        {"evidence/F2000/F2000-baseline.mus", "Finale 2000", FormatEpoch::UncompressedLegacy, 144, 118, true, 4, 1},
        // A document that carries the selector with the "use this positioning" bit clear.
        {"evidence/F2000/F2000-multilayer.mus", "Finale 2000 multilayer", FormatEpoch::UncompressedLegacy, 144, 118, false, 4, 4},
        {"evidence/F2002/F2002-baseline.mus", "Finale 2002", FormatEpoch::DclLegacy, 144, 118, true, 4, 1},
        {"evidence/F2005/F2005-baseline.mus", "Finale 2005", FormatEpoch::DclLegacy, 144, 224, true, 4, 1},
        // Finale 2006 stores a distinct starting-connection horizontal offset.
        {"evidence/F2006/F2006-embedded-tif.mus", "Finale 2006", FormatEpoch::DclLegacy, 144, 224, true, 8, 1},
        // The zlib era in both byte orders.
        {"evidence/F2007/F2007-lyric-hyphens.mus", "Finale 2007", FormatEpoch::ZlibLegacy, 144, 224, true, 4, 1},
        {"evidence/F2012/F2012-upstem-flags.mus", "Finale 2012", FormatEpoch::ZlibLegacy, 144, 115, true, 4, 1},
    };
    for (const auto& fixture : fixtures) {
        const auto result = read(fixture.path);
        expect(result.report.formatEpoch == fixture.epoch, std::string("The fixture for ") + fixture.era + " is not the expected epoch");
        const auto lyrics = result.document->getOptions()->get<Lyrics>();
        expect(static_cast<bool>(lyrics), std::string("No lyric options for ") + fixture.era);
        const auto wrong = [&](const char* name) { return std::string("The lyric ") + name + " was wrong for " + fixture.era; };
        expect(lyrics->maxHyphenSeparation == fixture.maxHyphenSeparation, wrong("maximum hyphen separation"));
        expect(lyrics->wordExtLineWidth == fixture.wordExtLineWidth, wrong("word extension line width"));
        expect(lyrics->wordExtHorzOffset == fixture.wordExtHorzOffset, wrong("word extension horizontal offset"));
        expect(lyrics->wordExtVertOffset == fixture.wordExtVertOffset, wrong("word extension vertical offset"));
        // "Lift" and "Push" have homes of their own on selectors 29 and 30 in every epoch, so
        // these two are the only fields of this class a Coda-banner document supplies.
        expect(field(result, "options.lyricOptions.wordExtVertOffset").origin == ValueOrigin::LegacyMus
                   && field(result, "options.lyricOptions.wordExtHorzOffset").origin == ValueOrigin::LegacyMus,
            wrong("word extension offset provenance"));
        // The legacy alignment list is in neither musxdom's order nor its reverse.
        const auto style = [&](SyllableType type) {
            const auto found = lyrics->syllablePosStyles.find(type);
            expect(found != lyrics->syllablePosStyles.end() && found->second, wrong("syllable position style"));
            return found->second;
        };
        expect(style(SyllableType::Default)->align == AlignJustify::Center && style(SyllableType::Default)->justify == AlignJustify::Center,
            wrong("default syllable position"));
        expect(style(SyllableType::WordExt)->align == AlignJustify::Left && style(SyllableType::WordExt)->justify == AlignJustify::Left,
            wrong("word extension syllable position"));
        expect(style(SyllableType::First)->align == AlignJustify::Center && style(SyllableType::First)->justify == AlignJustify::Left,
            wrong("first syllable position"));
        expect(style(SyllableType::SystemStart)->align == AlignJustify::Center && style(SyllableType::SystemStart)->justify == AlignJustify::Left,
            wrong("system start syllable position"));
        for (const auto type : {SyllableType::WordExt, SyllableType::First, SyllableType::SystemStart}) {
            expect(style(type)->on == fixture.optionalPositionsOn, wrong("optional syllable positioning switch"));
        }
        // Before Finale 2012 punctuation is not ignored. Finale 2012 reads selector 57 word 4
        // instead.
        const bool isFinale2012 = fixture.epoch == FormatEpoch::ZlibLegacy && result.report.sourceVersion
                                  && finale_mus_reader::VersionBound{result.report.sourceVersion->major, result.report.sourceVersion->minor,
                                         result.report.sourceVersion->maint}
                                         >= finale_mus_reader::versions::finale2012;
        expect(lyrics->lyricUseEdgePunctuation == !isFinale2012, wrong("syllable edge punctuation setting"));
        expect(field(result, "options.lyricOptions.lyricUseEdgePunctuation").origin
                   == (isFinale2012 ? ValueOrigin::LegacyMus : ValueOrigin::LegacyBehavior),
            wrong("syllable edge punctuation provenance"));

        // Both settings postdate Finale 2012, so no source of any era states either, but they
        // are handled differently on purpose. The switch is known false and is asserted even
        // though the baseline agrees; the character can only come from the baseline, because
        // restating U+002D in code would duplicate the pinned resource. See
        // testLyricPostFormatAssertions, which seeds the opposite of both.
        expect(lyrics->hyphenChar == U'-', wrong("hyphen character"));
        expect(!lyrics->useAltHyphenFont, wrong("alternate hyphen font switch"));
        expect(field(result, "options.lyricOptions.hyphenChar").origin == ValueOrigin::MusxOnly, wrong("hyphen character provenance"));
        expect(field(result, "options.lyricOptions.useAltHyphenFont").origin == ValueOrigin::LegacyBehavior,
            wrong("alternate hyphen font switch provenance"));
        // The pinned <lyricOptions> carries no <altHyphenFont>. musxdom synthesizes it in
        // integrityCheck, while the reader reports its persisted leaves as MUSX-only.
        expect(static_cast<bool>(lyrics->altHyphenFont), wrong("alternate hyphen font, which musxdom should have synthesized"));
        for (const auto* member : {"fontId", "fontSize", "effects"}) {
            const auto description = std::string("alternate hyphen font ") + member + " provenance";
            expect(field(result, std::string("options.lyricOptions.altHyphenFont.") + member).origin == ValueOrigin::MusxOnly,
                wrong(description.c_str()));
        }
    }

    // Two Finale 2004 switches live in the words neighboring selector 57. Both words exist in
    // every era but have these meanings only from Finale 2004 onward.
    const auto noSmartWext = read("evidence/F2004/F2004-lyropts-nosmart-wext.mus");
    const auto needUnderscore = read("evidence/F2004/F2004-lyropts-needuscore.mus");
    const auto f2004Base = read("evidence/F2004/F2004-baseline.mus");
    expect(f2004Base.document->getOptions()->get<Lyrics>()->useSmartWordExtensions
               && !noSmartWext.document->getOptions()->get<Lyrics>()->useSmartWordExtensions,
        "Smart word extensions were not read from selector 34 word 5");
    expect(!f2004Base.document->getOptions()->get<Lyrics>()->wordExtNeedUnderscore
               && needUnderscore.document->getOptions()->get<Lyrics>()->wordExtNeedUnderscore,
        "The underscore requirement was not read from selector 57 word 1");
    for (const auto& [result, target] : {std::pair{std::cref(noSmartWext), "options.lyricOptions.useSmartWordExtensions"},
             std::pair{std::cref(needUnderscore), "options.lyricOptions.wordExtNeedUnderscore"}}) {
        expect(field(result.get(), target).origin == ValueOrigin::LegacyMus,
            std::string("A Finale 2004 lyric switch was not reported as read: ") + target);
    }
    // Neither option exists before Finale 2004, so both are known false for an earlier
    // document rather than merely unstated, and neither word may be read: Finale 2.6.3 stores
    // 12 in selector 34 word 5, which is no boolean at all.
    //
    // An upgrade may enable smart word extensions for an old document, but that does not mean
    // the source format contained the feature.
    const auto f263 = read("evidence/F263/F263-baseline.mus");
    expect(!f263.document->getOptions()->get<Lyrics>()->useSmartWordExtensions && !f263.document->getOptions()->get<Lyrics>()->wordExtNeedUnderscore
               && !f263.document->getOptions()->get<Lyrics>()->useSmartHyphens,
        "A Coda-banner document claimed a Finale 2004 lyric feature");
    for (const char* target :
        {"options.lyricOptions.useSmartWordExtensions", "options.lyricOptions.wordExtNeedUnderscore", "options.lyricOptions.useSmartHyphens"}) {
        expect(field(f263, target).origin == ValueOrigin::LegacyBehavior,
            std::string("A pre-Finale-2004 lyric switch was not reported as era behavior: ") + target);
    }

    // Finale 2000 is the first release with a dedicated Lyric Options dialog. `right` is the
    // alignment a stock document never uses.
    // This case sets the first syllable's alignment and the system-start syllable's
    // justification to Right, exercising both fields independently.
    const auto align = read("evidence/F2000/F2000-lyropts-align-just.mus");
    const auto alignLyrics = align.document->getOptions()->get<Lyrics>();
    const auto alignStyle = [&](SyllableType type) {
        const auto found = alignLyrics->syllablePosStyles.find(type);
        expect(found != alignLyrics->syllablePosStyles.end() && found->second,
            "A syllable position style is missing from the Finale 2000 alignment fixture");
        return found->second;
    };
    expect(alignStyle(SyllableType::First)->align == AlignJustify::Right && alignStyle(SyllableType::First)->justify == AlignJustify::Left,
        "The legacy alignment value 3 was not translated to right");
    expect(
        alignStyle(SyllableType::SystemStart)->justify == AlignJustify::Right && alignStyle(SyllableType::SystemStart)->align == AlignJustify::Center,
        "The legacy justification value 3 was not translated to right");
    // Setting a position's alignment enables it, and the word extension position is left off,
    // so the same record carries both states of the flag bit.
    expect(alignStyle(SyllableType::First)->on && alignStyle(SyllableType::SystemStart)->on && !alignStyle(SyllableType::WordExt)->on,
        "The Finale 2000 alignment fixture did not read its positioning switches");

    // Finale 1.0.0 stores "Lift" in selector 29 word 5 and "Push" in selector 30 word 5.
    const auto lift = read("evidence/F100/F100-wext-push-6-lift-5.mus");
    const auto liftLyrics = lift.document->getOptions()->get<Lyrics>();
    expect(liftLyrics->wordExtVertOffset == 5 && liftLyrics->wordExtHorzOffset == 6,
        "The Coda-banner lift and push were not read from selectors 29 and 30");
    // With no selector 55 in this era, the starting connection takes the dialog's own values.
    expect(liftLyrics->wordExtConnectStyles.at(ConnectType::DefaultStart)->xOffset == 6
               && liftLyrics->wordExtConnectStyles.at(ConnectType::DefaultStart)->yOffset == 5,
        "The Coda-banner starting connection did not take the recovered lift and push");
    // The baseline case verifies that stored values matching defaults are still read.
    const auto liftBase = read("evidence/F100/F100-baseline.mus");
    expect(liftBase.document->getOptions()->get<Lyrics>()->wordExtVertOffset == 4
               && liftBase.document->getOptions()->get<Lyrics>()->wordExtHorzOffset == 4,
        "The Coda-banner baseline did not read its own lift and push");

    // Finale 3.7.2 adds hyphen spacing and word-extension line thickness to the two settings
    // already stored by the Coda-banner epoch.
    const auto f372opts = read("evidence/F372/F372-lyricopts-changed.mus");
    const auto f372Lyrics = f372opts.document->getOptions()->get<Lyrics>();
    expect(f372Lyrics->maxHyphenSeparation == 157 && f372Lyrics->wordExtLineWidth == 134 && f372Lyrics->wordExtVertOffset == 5
               && f372Lyrics->wordExtHorzOffset == 6,
        "The Finale 3.7.2 four-option save was not read from selectors 15, 29, 30 and 67");
    for (const char* target : {"options.lyricOptions.maxHyphenSeparation", "options.lyricOptions.wordExtLineWidth",
             "options.lyricOptions.wordExtVertOffset", "options.lyricOptions.wordExtHorzOffset"}) {
        expect(
            field(f372opts, target).origin == ValueOrigin::LegacyMus, std::string("A Finale 3.7.2 lyric option was not reported as read: ") + target);
    }
    // The era stores no connection table, so the starting connection takes the dialog's own
    // values. The unrelated `oneEntryEnd` value remains the pinned baseline's 42.
    expect(f372Lyrics->wordExtConnectStyles.at(ConnectType::DefaultStart)->xOffset == 6
               && f372Lyrics->wordExtConnectStyles.at(ConnectType::DefaultStart)->yOffset == 5,
        "The Finale 3.7.2 starting connection did not take the recovered lift and push");
    expect(f372Lyrics->wordExtConnectStyles.at(ConnectType::OneEntryEnd)->xOffset == 42,
        "The reader reproduced a synthesized one-entry offset instead of keeping the baseline");

    // Selector 57 word 4 stores the inverse of "Ignore Syllable Edge Punctuation".
    const auto noIgnore = read("evidence/F2012/F2012-lyropts-noign-punct.mus");
    const auto noIgnoreLyrics = noIgnore.document->getOptions()->get<Lyrics>();
    expect(noIgnoreLyrics->lyricUseEdgePunctuation, "A Finale 2012 document with the ignore switch cleared did not read as not-ignored");
    expect(field(noIgnore, "options.lyricOptions.lyricUseEdgePunctuation").origin == ValueOrigin::LegacyMus
               && field(noIgnore, "options.lyricOptions.lyricUseEdgePunctuation").rawValue == 1,
        "The cleared ignore switch was not reported as read from selector 57 word 4");
    // Its list is the stock one, so the record carries no tail even though ignoring is off.
    expect(field(noIgnore, "options.lyricOptions.lyricPunctuationToIgnore").origin == ValueOrigin::Finale27Default,
        "A document with no punctuation tail was reported as carrying one");

    // Automatic lyric numbering arrives with Finale 2011 in a record that grew to hold it:
    // selector 58 is six words before it and twelve after, which is what selects the layout
    // rather than a version. Three saves carry a binary code across the four fields, so each
    // has a unique signature and none can be confused with another.
    using AutoNum = Lyrics::AutoNumberingAlign;
    struct AutoNumCase
    {
        const char* path;
        bool verses;
        bool choruses;
        bool sections;
        AutoNum type;
    };
    const AutoNumCase autoNumCases[] = {
        {"evidence/F2011/F2011-baseline.mus", false, false, false, AutoNum::Align},
        {"evidence/F2011/F2011-lyropts-autonum-type.mus", true, true, true, AutoNum::None},
        {"evidence/F2011/F2011-autonum-vs.mus", true, false, true, AutoNum::Align},
        {"evidence/F2011/F2011-autonum-cs.mus", false, true, true, AutoNum::Align},
    };
    for (const auto& c : autoNumCases) {
        const auto result = read(c.path);
        const auto ly = result.document->getOptions()->get<Lyrics>();
        const auto wrongAt = [&](const char* what) { return std::string("The lyric ") + what + " was wrong for " + c.path; };
        expect(ly->showAutoNumbersOnVerses == c.verses, wrongAt("verse auto-number switch"));
        expect(ly->showAutoNumbersOnChoruses == c.choruses, wrongAt("chorus auto-number switch"));
        expect(ly->showAutoNumbersOnSections == c.sections, wrongAt("section auto-number switch"));
        expect(ly->lyricAutoNumType == c.type, wrongAt("auto-number type"));
        for (const char* target : {"options.lyricOptions.showAutoNumbersOnVerses", "options.lyricOptions.showAutoNumbersOnChoruses",
                 "options.lyricOptions.showAutoNumbersOnSections", "options.lyricOptions.lyricAutoNumType"}) {
            expect(field(result, target).origin == ValueOrigin::LegacyMus, wrongAt("auto-number provenance"));
        }
    }
    // A Finale 2007 document carries the short record, so the same reader must assert the
    // three switches off rather than read words that are not there: the record is twelve bytes
    // through Finale 2010 and twenty-four from Finale 2011.
    const auto f2007auto = read("evidence/F2007/F2007-lyric-hyphens.mus");
    const auto f2007Lyrics = f2007auto.document->getOptions()->get<Lyrics>();
    expect(!f2007Lyrics->showAutoNumbersOnVerses && !f2007Lyrics->showAutoNumbersOnChoruses && !f2007Lyrics->showAutoNumbersOnSections,
        "A Finale 2007 document claimed automatic lyric numbers");
    expect(field(f2007auto, "options.lyricOptions.showAutoNumbersOnVerses").origin == ValueOrigin::LegacyBehavior,
        "A pre-Finale-2011 auto-number switch was not reported as era behavior");
    expect(f2007Lyrics->lyricAutoNumType == AutoNum::Align
               && field(f2007auto, "options.lyricOptions.lyricAutoNumType").origin == ValueOrigin::Finale27Default,
        "A pre-Finale-2011 document did not leave the numbering type to the baseline");

    // Finale 2011 stores the tail as packed 8-bit bytes in the saving platform's code page,
    // whereas Finale 2012 stores one 16-bit code unit per character. Both containers are
    // little-endian, so reading the 2011 byte string through the word path would transpose
    // every pair of characters. The two non-ASCII bytes are what make the code page a
    // significant: 0xc7 0xc8 is the guillemet pair in Mac Roman and `ÇÈ` in Windows-1252.
    const auto f2011punct = read("evidence/F2011/F2011-lyric-punct.mus");
    const auto f2011Lyrics = f2011punct.document->getOptions()->get<Lyrics>();
    expect(f2011Lyrics->lyricPunctuationToIgnore == "#@%&\u00ab\u00bb", "The Finale 2011 punctuation tail was not decoded as packed Mac Roman bytes");
    expect(field(f2011punct, "options.lyricOptions.lyricPunctuationToIgnore").origin == ValueOrigin::LegacyMus
               && field(f2011punct, "options.lyricOptions.lyricPunctuationToIgnore").rawValue == 6,
        "The Finale 2011 tail was not reported as read, with its byte count");
    // Its baseline sibling carries no tail and keeps musxdom's own default list, and both
    // read the switch from the same record: Finale 2011 is where that word becomes live.
    const auto f2011base = read("evidence/F2011/F2011-baseline.mus");
    expect(f2011base.document->getOptions()->get<Lyrics>()->lyricPunctuationToIgnore == ",.?!;:\'\"\u201c\u201d\u2018\u2019",
        "A Finale 2011 document with no tail did not keep musxdom's default list");
    for (const auto& r : {std::cref(f2011base), std::cref(f2011punct)}) {
        expect(!r.get().document->getOptions()->get<Lyrics>()->lyricUseEdgePunctuation
                   && field(r.get(), "options.lyricOptions.lyricUseEdgePunctuation").origin == ValueOrigin::LegacyMus,
            "A Finale 2011 document did not read the edge punctuation switch from its record");
    }

    // The ignore list is a variable-length tail on selector 57, written only when it is not
    // the stock set.
    const auto punct = read("evidence/F2012/F2012-lyric-punct.mus");
    const auto punctLyrics = punct.document->getOptions()->get<Lyrics>();
    expect(punctLyrics->lyricPunctuationToIgnore == "#@%&", "The custom punctuation list was not read from the selector 57 tail");
    expect(field(punct, "options.lyricOptions.lyricPunctuationToIgnore").origin == ValueOrigin::LegacyMus
               && field(punct, "options.lyricOptions.lyricPunctuationToIgnore").rawValue == 4,
        "The custom punctuation list was not reported as read, with its code unit count");
    // Its sibling keeps the stock list, which the record does not carry at all. musxdom's
    // integrityCheck owns that default, so the reader must leave the field empty rather than
    // state the set a second time.
    const auto stock = read("evidence/F2012/F2012-baseline.mus");
    expect(stock.document->getOptions()->get<Lyrics>()->lyricPunctuationToIgnore == ",.?!;:\'\"\u201c\u201d\u2018\u2019",
        "A document with no punctuation tail did not fall to musxdom's own default list");
    expect(field(stock, "options.lyricOptions.lyricPunctuationToIgnore").origin == ValueOrigin::Finale27Default,
        "An absent punctuation tail was reported as read");
    // This case also carries nonzero vertical offsets in its word-extension connection table.
    expect(punctLyrics->wordExtConnectStyles.at(ConnectType::DefaultEnd)->yOffset == 5
               && punctLyrics->wordExtConnectStyles.at(ConnectType::SystemStart)->yOffset == 1,
        "The rewritten connection table was not read from the punctuation fixture");

    // The connection points cover all six legacy numbers, and two of the six sit in different
    // places in musxdom's enum, so a straight cast would swap the system and dotted
    // attachments.
    const auto f2006 = read("evidence/F2006/F2006-embedded-tif.mus");
    const auto connect = f2006.document->getOptions()->get<Lyrics>()->wordExtConnectStyles;
    struct ExpectedConnection
    {
        ConnectType type;
        ConnectIndex index;
        int xOffset;
        int yOffset;
    };
    const ExpectedConnection connections[] = {
        {ConnectType::DefaultStart, ConnectIndex::LyricRightBottom, 8, 1},
        {ConnectType::DefaultEnd, ConnectIndex::HeadRightLyrBaseline, 0, 5},
        {ConnectType::SystemStart, ConnectIndex::SystemLeft, 12, 1},
        {ConnectType::SystemEnd, ConnectIndex::SystemRight, -12, 5},
        {ConnectType::DottedEnd, ConnectIndex::DotRightLyrBaseline, 0, 5},
        {ConnectType::DurationEnd, ConnectIndex::DurationLyrBaseline, -8, 5},
        {ConnectType::OneEntryEnd, ConnectIndex::LyricRightBottom, 46, 5},
        {ConnectType::ZeroLengthEnd, ConnectIndex::LyricRightBottom, 0, 0},
        {ConnectType::ZeroOffset, ConnectIndex::HeadRightLyrBaseline, 0, 0},
    };
    for (const auto& expected : connections) {
        const auto found = connect.find(expected.type);
        expect(found != connect.end() && found->second, "A word extension connection style is missing from the Finale 2006 fixture");
        expect(found->second->connectIndex == expected.index, "A word extension connection point was translated wrongly");
        expect(found->second->xOffset == expected.xOffset && found->second->yOffset == expected.yOffset,
            "A word extension connection offset was read from the wrong word");
    }

    // Provenance separates the eras where the values cannot. A Coda-banner document states
    // neither the line width nor the syllable positions, so both must be reported as era
    // behavior rather than claimed as read or left at a Finale 27 setting that says otherwise.
    const auto f100 = read("evidence/F100/F100-baseline.mus");
    expect(field(f100, "options.lyricOptions.wordExtLineWidth").origin == ValueOrigin::LegacyBehavior,
        "The Coda-era word extension line width was not reported as era behavior");
    expect(field(f100, "options.lyricOptions.syllablePosStyles[wordExt].on").origin == ValueOrigin::LegacyBehavior,
        "A Coda-era syllable positioning switch was not reported as era behavior");
    expect(field(f100, "options.lyricOptions.maxHyphenSeparation").origin == ValueOrigin::Finale27Default,
        "The Coda-era selector 15 word 1 was claimed as a hyphen separation");
    // Selector 55 exists in this era and is a different option; reading it as the connection
    // table would replace nine connection styles with another option's bytes. Those bytes are
    // 16128 and 16448 in this fixture, so the failure would be loud: no connection point at
    // all, and offsets of tens of thousands. Nothing is reported for the collection either,
    // because the capture pass reports only what it reads.
    const auto f100Connect = f100.document->getOptions()->get<Lyrics>()->wordExtConnectStyles;
    const auto f100Start = f100Connect.find(ConnectType::DefaultStart);
    expect(f100Start != f100Connect.end() && f100Start->second && f100Start->second->connectIndex == ConnectIndex::LyricRightBottom
               && f100Start->second->xOffset == 4 && f100Start->second->yOffset == 4,
        "The Coda-era selector 55 was read as the word extension connection table");
    expect(!anyReportedField(f100.report, [](const auto& member, const auto&) { return member.find("wordExtConnectStyles") != std::string::npos; }),
        "A Coda-era document reported a word extension connection it never stored");

    // Finale 2003 is the last release before the smart-lyric group. All three of its switches
    // are asserted false rather than read: the words that carry them are zero in that era
    // whether the option is off or has not been invented yet, so reading them would be reading
    // nothing, and inheriting the baseline would claim a rendering this reader cannot build.
    // Smart hyphens and smart word extensions are implemented as hyphen and word-extension
    // smart shapes, which Finale 27 manufactures on upgrade and this reader does not.
    const auto f2003 = read("evidence/F2003/F2003-baseline.mus");
    const auto f2003Lyrics = f2003.document->getOptions()->get<Lyrics>();
    expect(!f2003Lyrics->useSmartHyphens && !f2003Lyrics->useSmartWordExtensions && !f2003Lyrics->wordExtNeedUnderscore,
        "A Finale 2003 document claimed a smart-lyric feature its era did not have");
    for (const char* target :
        {"options.lyricOptions.useSmartHyphens", "options.lyricOptions.useSmartWordExtensions", "options.lyricOptions.wordExtNeedUnderscore"}) {
        expect(field(f2003, target).origin == ValueOrigin::LegacyBehavior, std::string("A pre-Finale-2004 smart-lyric switch was not reported as era "
                                                                                       "behavior: ")
                                                                               + target);
    }
    const auto f2004 = read("evidence/F2004/F2004-baseline.mus");
    expect(f2004.document->getOptions()->get<Lyrics>()->useSmartHyphens
               && field(f2004, "options.lyricOptions.useSmartHyphens").origin == ValueOrigin::LegacyMus,
        "The Finale 2004 smart-hyphen switch was not read from selector 35");
    expect(f2004.document->getOptions()->get<Lyrics>()->wordExtMinLength == 38
               && field(f2004, "options.lyricOptions.wordExtMinLength").origin == ValueOrigin::LegacyMus,
        "The Finale 2004 word extension minimum length was not read from selector 57");
}

TEST_CASE("Lyric options recovery", "[class][reader]")
{
    testLyricOptionsRecovery();
}

TEST_CASE("Lyric word extension connection layouts", "[class]")
{
    testLyricWordExtConnectionLayouts();
}
TEST_CASE("Lyric edge punctuation version gate", "[class]")
{
    testLyricEdgePunctuationVersionGate();
}
TEST_CASE("Lyric punctuation tail encoding gate", "[class]")
{
    testLyricPunctuationTailEncodingGate();
}
TEST_CASE("Lyric post-format assertions", "[class]")
{
    testLyricPostFormatAssertions();
}

} // namespace
} // namespace finale_mus_reader_tests
