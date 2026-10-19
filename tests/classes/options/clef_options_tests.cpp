// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

/// FontOptions precede every other importer, so a clef without its own font always has a clef
/// font to decode through.
void seedClefFont(const musx::dom::DocumentPtr& document)
{
    auto fontOptions = std::make_shared<musx::dom::options::FontOptions>(document);
    auto clefFont = std::make_shared<musx::dom::FontInfo>(document);
    clefFont->fontId = musx::dom::DEFAULT_MUSIC_FONT_ID;
    fontOptions->fontOptions.emplace(musx::dom::options::FontOptions::FontType::Clef, std::move(clefFont));
    document->getOptions()->add(musx::dom::options::FontOptions::XmlNodeName, std::move(fontOptions));
}

void testClefTupleDecoding()
{
    using ClefOptions = musx::dom::options::ClefOptions;
    const auto captured = [](const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile) {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        seedClefFont(document);
        ImportReport report(FormatEpoch::UncompressedLegacy);
        finale_mus_reader::PendingReferences pending;
        finale_mus_reader::options::captureClefOptions(
            LegacyRecordIndex::build(parsed), profile, document, makeClefReferenceDocument(), report, pending, session.getConstructionContext());
        return document->getOptions()->get<ClefOptions>();
    };

    // One nine-word tuple whose character is the sign-extended spelling of 139, and whose
    // flags claim an own font. Padded out to a whole sixteen-definition table.
    std::vector<SyntheticRow> rows;
    std::vector<std::int16_t> stream{-10, static_cast<std::int16_t>(0xff8b), -6, 7, 0, 11, 24, 1, 0x0002};
    stream.resize(16 * 9, 0);
    for (std::size_t first = 0; first < stream.size(); first += 6) {
        SyntheticRow row;
        row.cmper = GLOBALS_CMPER;
        row.tag = "95";
        for (std::size_t slot = 0; slot < 6; ++slot) {
            row.words[slot] = stream[first + slot];
        }
        rows.push_back(row);
    }
    const auto narrow = captured(makeContainer(rows), profileFor(7));
    expectMapping(narrow && narrow->clefDefs.size() == 18, "The synthetic sixteen-clef table was not completed from the reference");
    const auto first = narrow->getClefDef(0);
    expectMapping(first->clefChar == 139, "A sign-extended clef character was not narrowed to its stored byte");
    expectMapping(first->useOwnFont && first->font && first->font->fontId == 11 && first->font->fontSize == 24 && first->font->bold,
        "The own-font flag bit did not bring across the tuple's font triple");
    expectMapping(!first->isShape && !first->scaleToStaffHeight, "Unset clef flag bits were treated as set");
    expectMapping(first->baselineAdjust == 7, "The 2001-and-later baseline word was not assigned as Efix");
    expectMapping(narrow->clefFrontSepar == 24, "The reference scalars were not copied onto the rebuilt clef options");

    // A DCL file with no clef table must not fall back on the pre-2001 selectors. Those
    // selectors still exist in that era and hold unrelated option words, so reading them
    // would fabricate eight clef definitions and report them as recovered from the file.
    {
        std::vector<SyntheticRow> dclRows;
        std::array<std::array<char, 3>, 8> dclTags{};
        // The values a real Finale 2002 file holds at selectors 28 through 35.
        const std::array<std::array<std::int16_t, 6>, 8> notClefs{{{0, 0, 0, 0, 0, 24}, {0, 0, 0, 0, 0, 1}, {0, 0, 0, 0, 0, 4}, {0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 128}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}}};
        for (int selector = 28; selector <= 35; ++selector) {
            auto& tag = dclTags[static_cast<std::size_t>(selector - 28)];
            tag = {static_cast<char>('0' + selector / 10), static_cast<char>('0' + selector % 10), '\0'};
            SyntheticRow row;
            row.cmper = GLOBALS_CMPER;
            row.tag = tag.data();
            row.words = notClefs[static_cast<std::size_t>(selector - 28)];
            dclRows.push_back(row);
        }
        auto profile = profileFor(7);
        profile.epoch = FormatEpoch::DclLegacy;
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        seedClefFont(document);
        ImportReport report(FormatEpoch::UncompressedLegacy);
        finale_mus_reader::PendingReferences pending;
        finale_mus_reader::options::captureClefOptions(LegacyRecordIndex::build(makeContainer(dclRows)), profile, document,
            makeClefReferenceDocument(), report, pending, session.getConstructionContext());
        std::size_t recovered = 0;
        anyMappingReportedField(report, [&](const auto& member, const auto& info) {
            if (info.origin == ValueOrigin::LegacyMus && member.find("clefDefs") != std::string::npos) {
                ++recovered;
            }
            return false;
        });
        expectMapping(recovered == 0, "A DCL file with no clef table read the pre-2001 selectors as clefs");
        expectMapping(
            document->getOptions()->get<ClefOptions>()->clefDefs.size() == 18, "A DCL file with no clef table was not completed from the reference");
    }

    // An out-of-range default clef index is warned about and left alone. Clamping it would
    // turn a damaged file into a plausible document and hide the fact worth knowing.
    {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        auto options = std::make_shared<ClefOptions>(document);
        options->defaultClef = 99;
        for (int i = 0; i < 18; ++i) {
            options->clefDefs.push_back(std::make_shared<ClefOptions::ClefDef>(options));
        }
        document->getOptions()->add(ClefOptions::XmlNodeName, options);
        ImportReport report(FormatEpoch::UncompressedLegacy);
        finale_mus_reader::options::validateClefOptions(document, report);
        expectMapping(options->defaultClef == 99, "An out-of-range default clef index was silently corrected");
        expectMapping(
            std::any_of(report.diagnostics.begin(), report.diagnostics.end(),
                [](const finale_mus_reader::Diagnostic& entry) { return entry.message.find("default clef index 99") != std::string::npos; }),
            "An out-of-range default clef index was accepted without a warning");
        ImportReport clean(FormatEpoch::UncompressedLegacy);
        options->defaultClef = 17;
        finale_mus_reader::options::validateClefOptions(document, clean);
        expectMapping(clean.diagnostics.empty(), "A valid default clef index warned");
    }

    // Pre-2001 clefs are eight separate globals whose baseline word is in harmonic levels,
    // so it is scaled into Efix on the way in while the report keeps the stored number.
    std::vector<SyntheticRow> earlyRows;
    // Fixed storage: SyntheticRow keeps a pointer, so the tag text must outlive the vector
    // and must not move when rows are appended.
    std::array<std::array<char, 3>, 8> earlyTags{};
    for (int selector = 28; selector <= 35; ++selector) {
        auto& tag = earlyTags[static_cast<std::size_t>(selector - 28)];
        tag = {static_cast<char>('0' + selector / 10), static_cast<char>('0' + selector % 10), '\0'};
        SyntheticRow row;
        row.cmper = GLOBALS_CMPER;
        row.tag = tag.data();
        // Slot 4 is the baseline adjustment in this era; slot 1 holds something else.
        row.words = {static_cast<std::int16_t>(-selector), 7, static_cast<std::int16_t>(0xff8b), -6, 2, 0};
        earlyRows.push_back(row);
    }
    const auto early = captured(makeContainer(earlyRows), profileFor(5));
    expectMapping(early && early->clefDefs.size() == 18, "The eight pre-2001 clef globals were not completed from the reference");
    expectMapping(early->getClefDef(0)->middleCPos == -28 && early->getClefDef(0)->clefChar == 139 && early->getClefDef(0)->staffPosition == -6,
        "The pre-2001 per-clef record slots were misread");
    expectMapping(early->getClefDef(0)->baselineAdjust == 2 * 768, "The pre-2001 harmonic-level baseline was not converted to Efix");
    expectMapping(early->getClefDef(7)->middleCPos == -35 && early->getClefDef(8)->middleCPos == -1,
        "The boundary between eight stored and ten synthesized clefs moved");

    // 360 bytes divides by both tuple widths. Only the version separates them, and only
    // the wide reading yields the eighteen definitions Finale actually stores.
    std::vector<std::int16_t> wide(18 * 10, 0);
    wide[0] = -10;
    wide[1] = 38;   // low half of the long character
    wide[2] = 0;    // high half
    wide[3] = -6;
    wide[9] = 0x0005;
    const auto unicodeEra = captured(makeClassContainer(0x006d, wide, ByteOrder::LittleEndian), [] {
        auto profile = profileFor(17);
        profile.epoch = FormatEpoch::ZlibLegacy;
        profile.byteOrder = ByteOrder::LittleEndian;
        return profile;
    }());
    expectMapping(unicodeEra && unicodeEra->clefDefs.size() == 18, "A 360-byte payload was not read as eighteen wide clef definitions");
    expectMapping(unicodeEra->getClefDef(0)->clefChar == 38 && unicodeEra->getClefDef(0)->staffPosition == -6,
        "The wide tuple's long character or shifted slots were misread");
    expectMapping(unicodeEra->getClefDef(0)->isShape && unicodeEra->getClefDef(0)->scaleToStaffHeight,
        "The wide tuple's flags word was not read at its shifted slot");

    // A whole class-record field is signed, like the fixed-row word that carries the same
    // logical option. Read unsigned, this Evpu of -12 arrives as 65524.
    {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        auto profile = profileFor(13);
        profile.epoch = FormatEpoch::ZlibLegacy;
        profile.byteOrder = ByteOrder::BigEndian;
        // Class 0x1b is globals selector 13; word 2 is the percent and word 3 the offset.
        auto options = std::make_shared<ClefOptions>(document);
        document->getOptions()->add(ClefOptions::XmlNodeName, options);
        ImportReport report(FormatEpoch::UncompressedLegacy);
        finale_mus_reader::applyMappingTables({&finale_mus_reader::options::classClefOptionsTable()},
            LegacyRecordIndex::build(makeClassContainer(0x001b, {4, 1024, 75, -12, 0, 1}, ByteOrder::BigEndian)), profile, document, report);
        expectMapping(options->clefChangeOffset == -12, "A negative class-record word was read as an unsigned value");
        expectMapping(options->clefChangePercent == 75, "A positive class-record word was not read");
    }

    // A big-endian file using the Finale 2012 layout announces itself: the long character's
    // word order in that combination is unverified. The values still decode as the symmetric
    // counterpart of the little-endian case.
    {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        seedClefFont(document);
        auto profile = profileFor(17);
        profile.epoch = FormatEpoch::ZlibLegacy;
        profile.byteOrder = ByteOrder::BigEndian;
        ImportReport report(FormatEpoch::UncompressedLegacy);
        finale_mus_reader::PendingReferences pending;
        finale_mus_reader::options::captureClefOptions(LegacyRecordIndex::build(makeClassContainer(0x006d, wide, ByteOrder::BigEndian)), profile,
            document, makeClefReferenceDocument(), report, pending, session.getConstructionContext());
        expectMapping(std::any_of(report.diagnostics.begin(), report.diagnostics.end(),
                          [](const finale_mus_reader::Diagnostic& entry) { return entry.message.find("unverified") != std::string::npos; }),
            "A big-endian Finale 2012 clef layout was decoded without saying it is unverified");
        expectMapping(
            document->getOptions()->get<ClefOptions>()->getClefDef(0)->clefChar == 38, "The big-endian wide tuple did not decode symmetrically");
    }

    // The same payload from a pre-Unicode file is the narrow tuple, and twenty definitions
    // is more than Finale stores, so the reader must say so rather than pass it off.
    ImportReport report(FormatEpoch::UncompressedLegacy);
    {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        seedClefFont(document);
        auto profile = profileFor(13);
        profile.epoch = FormatEpoch::ZlibLegacy;
        profile.byteOrder = ByteOrder::LittleEndian;
        finale_mus_reader::PendingReferences pending;
        finale_mus_reader::options::captureClefOptions(LegacyRecordIndex::build(makeClassContainer(0x006d, wide, ByteOrder::LittleEndian)), profile,
            document, makeClefReferenceDocument(), report, pending, session.getConstructionContext());
        expectMapping(document->getOptions()->get<ClefOptions>()->clefDefs.size() == 20,
            "The pre-Unicode reading of an ambiguous payload did not use the narrow tuple");
    }
    expectMapping(std::any_of(report.diagnostics.begin(), report.diagnostics.end(),
                      [](const finale_mus_reader::Diagnostic& entry) { return entry.message.find("more than the 18") != std::string::npos; }),
        "An over-long clef collection was accepted without a warning");
}

void testClefOptionsCapture()
{
    using ClefOptions = musx::dom::options::ClefOptions;
    const auto read = [](const char* relative) { return readFixture(relative, fixtureLegacySymbolFonts); };
    const auto clefs = [](const ImportResult& result) {
        const auto options = result.document->getOptions()->get<ClefOptions>();
        expect(static_cast<bool>(options), "The imported document has no clef options");
        expect(options->clefDefs.size() == 18, "Clef definitions were not completed to the modern collection size");
        return options;
    };
    // Every era stores these three for its first clef, so agreement across all of them is
    // what shows the four physical layouts describe one logical table.
    const auto expectTreble = [](const auto& options, const char* era) {
        const auto treble = options->getClefDef(0);
        expect(treble->middleCPos == -10 && treble->clefChar == 38 && treble->staffPosition == -6,
            std::string("The treble clef definition was not recovered from ") + era);
    };

    // Finale 2001: selector 95 already stores the baseline in Efix. This little-endian
    // fixture puts -4608 in the treble-clef word, a quarter inch below the default.
    const auto f2001 = read("evidence/F2001/F2001Win-tclef-baseline.mus");
    const auto f2001Clefs = clefs(f2001);
    expectTreble(f2001Clefs, "Finale 2001");
    expect(f2001.report.formatEpoch == FormatEpoch::DclLegacy && f2001.report.byteOrder == ByteOrder::LittleEndian
               && f2001Clefs->getClefDef(0)->baselineAdjust == -4608
               && field(f2001, "options.clefOptions.clefDefs[0].baselineAdjust").origin == ValueOrigin::LegacyMus,
        "The Windows Finale 2001 clef baseline was not recovered in Efix");

    // Finale 2002: selector 95, 24 incidences, sixteen nine-word tuples. The last two
    // definitions did not exist yet and come from the baseline.
    const auto f2002 = read("evidence/F2002/F2002-baseline.mus");
    const auto f2002Clefs = clefs(f2002);
    expectTreble(f2002Clefs, "Finale 2002");
    expect(field(f2002, "options.clefOptions.clefDefs[0].middleCPos").origin == ValueOrigin::LegacyMus,
        "The Finale 2002 clef table was not reported as recovered");
    expect(
        f2002Clefs->getClefDef(15)->middleCPos == -10 && f2002Clefs->getClefDef(15)->clefChar == 0 && f2002Clefs->getClefDef(15)->staffPosition == -6,
        "The last stored Finale 2002 clef definition was not recovered");
    expect(field(f2002, "options.clefOptions.clefDefs[16].shapeId").origin == ValueOrigin::Finale27Default
               && field(f2002, "options.clefOptions.clefDefs[17].shapeId").origin == ValueOrigin::Finale27Default,
        "The two clef definitions Finale 2002 lacks were not reported as synthesized");
    expect(f2002Clefs->getClefDef(16)->isShape && f2002Clefs->getClefDef(16)->scaleToStaffHeight, "A synthesized shape clef lost its shape flags");
    // The scalars surrounding the collection.
    expect(f2002Clefs->clefChangePercent == 75 && f2002Clefs->clefChangeOffset == -8 && f2002Clefs->clefFrontSepar == 24
               && f2002Clefs->clefBackSepar == 0 && f2002Clefs->clefKeySepar == 0 && f2002Clefs->clefTimeSepar == 0 && f2002Clefs->defaultClef == 0
               && !f2002Clefs->showClefFirstSystemOnly,
        "The Finale 2002 clef scalars were not recovered");
    expect(field(f2002, "options.clefOptions.clefFrontSepar").origin == ValueOrigin::LegacyMus
               && field(f2002, "options.clefOptions.clefChangePercent").origin == ValueOrigin::LegacyMus,
        "Recovered clef scalars were reported as synthesized defaults");
    expect(f2002Clefs->cautionaryClefChanges && field(f2002, "options.clefOptions.cautionaryClefChanges").origin == ValueOrigin::LegacyMus,
        "cautionaryClefChanges was not recovered from the courtesy-flags word");

    // Finale 2005: the same tuple, but 27 incidences, so the collection is complete in
    // the source and nothing is synthesized.
    const auto f2005 = read("evidence/F2005/F2005-baseline.mus");
    const auto f2005Clefs = clefs(f2005);
    expectTreble(f2005Clefs, "Finale 2005");
    expect(field(f2005, "options.clefOptions.clefDefs[17].shapeId").origin == ValueOrigin::LegacyMus,
        "The Finale 2005 source stores eighteen clefs and should synthesize none");
    expect(f2005Clefs->getClefDef(16)->isShape && f2005Clefs->getClefDef(16)->shapeId == 2 && f2005Clefs->getClefDef(16)->scaleToStaffHeight
               && !f2005Clefs->getClefDef(16)->useOwnFont,
        "The Finale 2005 shape-clef flags were not expanded from the packed word");

    // Finale 2007: the same nine-word tuple carried by a big-endian class record.
    const auto f2007 = read("evidence/F2007/F2007-lyric-hyphens.mus");
    const auto f2007Clefs = clefs(f2007);
    expectTreble(f2007Clefs, "Finale 2007");
    expect(f2007Clefs->getClefDef(13)->middleCPos == -17 && f2007Clefs->getClefDef(13)->clefChar == 160,
        "A later big-endian class-record clef definition was not recovered");
    expect(
        f2007Clefs->getClefDef(17)->isShape && f2007Clefs->getClefDef(17)->shapeId == 3, "The big-endian class-record shape clef was not recovered");

    // Finale 2012: little-endian, and the clef character is a long because that release
    // introduced Unicode text. Reading it as the narrow tuple would shift every slot after
    // the character and yield twenty definitions instead of eighteen.
    const auto f2012 = read("evidence/F2012/F2012-upstem-flags.mus");
    const auto f2012Clefs = clefs(f2012);
    expectTreble(f2012Clefs, "Finale 2012");
    expect(f2012Clefs->getClefDef(12)->clefChar == 139 && f2012Clefs->getClefDef(12)->staffPosition == -6,
        "The Finale 2012 long clef character was not assembled from its two words");
    expect(f2012Clefs->getClefDef(16)->isShape && f2012Clefs->getClefDef(16)->shapeId == 2 && f2012Clefs->getClefDef(17)->shapeId == 3,
        "The wide-tuple shape clefs were not read at their shifted slots");

    // Finale 2000: eight separate globals, selectors 28 through 35. The fourth clef stores
    // character 32 rather than the 214 used by the other covered eras.
    const auto f2000 = read("evidence/F2000/F2000-multilayer.mus");
    const auto f2000Clefs = clefs(f2000);
    expectTreble(f2000Clefs, "Finale 2000");
    expect(
        f2000Clefs->getClefDef(4)->clefChar == 32 && f2000Clefs->getClefDef(4)->middleCPos == -10 && f2000Clefs->getClefDef(4)->staffPosition == -4,
        "The distinctive Finale 2000 alto clef character was not recovered");
    expect(field(f2000, "options.clefOptions.clefDefs[7].middleCPos").origin == ValueOrigin::LegacyMus
               && field(f2000, "options.clefOptions.clefDefs[8].middleCPos").origin == ValueOrigin::Finale27Default,
        "The pre-2001 boundary between eight stored and ten synthesized clefs moved");

    // Finale 1.0.0: the same eight selectors. Its seventh clef stores -5 rather than the 9
    // used by later covered eras.
    const auto f100 = read("evidence/F100/F100-baseline.mus");
    const auto f100Clefs = clefs(f100);
    expectTreble(f100Clefs, "Finale 1.0.0");
    expect(f100Clefs->getClefDef(6)->middleCPos == -5 && f100Clefs->getClefDef(6)->clefChar == 116,
        "The Finale 1.0.0 percussion clef definition was not recovered");
    expect(f100Clefs->getClefDef(4)->clefChar == 214, "The Finale 1.0.0 alto clef character was not recovered");

    // Finale 2005 stores the clef baseline adjustment directly in Efix. The negative case does
    // not fit a signed word, so the stored value saturated. Recovering -32768 rather than
    // -36864 is the file being read correctly, not a decoding error.
    const auto f2005Baseline = read("evidence/F2005/F2005-clef-baseline.mus");
    const auto f2005BaselineClefs = clefs(f2005Baseline);
    expect(f2005BaselineClefs->getClefDef(0)->baselineAdjust == 18432, "The Finale 2005 clef baseline adjustment was not recovered as Efix");
    expect(f2005BaselineClefs->getClefDef(1)->baselineAdjust == -32768, "The saturated Finale 2005 baseline adjustment was not recovered verbatim");
    expect(f2005BaselineClefs->getClefDef(2)->baselineAdjust == 0, "An unedited clef did not keep a zero baseline adjustment");
    expect(field(f2005Baseline, "options.clefOptions.clefDefs[0].baselineAdjust").rawValue == 18432,
        "The stored baseline word was not reported for the Efix era");

    // The pre-2001 eras store the same setting as a small signed count of harmonic levels,
    // in word 4 of the clef's own selector, and scale it into Efix. Word 5 of the first
    // clef's selector is a document-wide switch: with it clear the stored counts are inert.
    constexpr int efixPerHarmonicLevel = 768;
    const auto expectEarlyBaseline = [&](const char* path, const std::vector<int>& expected, const char* era) {
        const auto result = read(path);
        const auto options = clefs(result);
        for (std::size_t i = 0; i < expected.size(); ++i) {
            expect(options->getClefDef(musx::dom::ClefIndex(i))->baselineAdjust == expected[i],
                std::string("The clef baseline adjustment was wrong for ") + era);
        }
        // The edit must not disturb the fields that share the record.
        expect(options->getClefDef(0)->middleCPos == -10 && options->getClefDef(0)->clefChar == 38 && options->getClefDef(0)->staffPosition == -6,
            std::string("A baseline edit changed neighboring clef fields in ") + era);
        return result;
    };

    // Finale 3.7.2 with the switch on. All eight clefs convert, including ones this save
    // never touched, which is what shows the switch is per document rather than per clef.
    const auto f372Baseline = expectEarlyBaseline("evidence/F372/F372-clef-baseline.mus",
        {1 * efixPerHarmonicLevel, -2 * efixPerHarmonicLevel, -5 * efixPerHarmonicLevel}, "Finale 3.7.2 with the switch on");
    expect(field(f372Baseline, "options.clefOptions.clefDefs[0].baselineAdjust").rawValue == 1,
        "The raw harmonic-level count was not reported for Finale 3.7.2");
    // The same document with the switch off. Its clefs still carry -2, -4 and -5, so a
    // reader that ignored the switch would produce three offsets Finale never applied.
    expectEarlyBaseline("evidence/F372/F372-baseline.mus", {0, 0, 0}, "Finale 3.7.2 with the switch off");
    expect(field(read("evidence/F372/F372-baseline.mus"), "options.clefOptions.clefDefs[0].baselineAdjust").rawValue == -2,
        "A disabled baseline count was not still reported as stored evidence");

    // Finale 97 is internally 3.8 and dropped the checkbox, so it adjusts unconditionally.
    // Its word 5 is 30 here, which has bit 0 clear: a reader that tested the word for
    // non-zero, or that tested the bit without the version, would get this file wrong.
    const auto f97Baseline =
        expectEarlyBaseline("evidence/F97/Fin97-clef-baseline.mus", {1 * efixPerHarmonicLevel, -2 * efixPerHarmonicLevel, 0}, "Finale 97");

    // The Coda era is excluded outright, not merely left switched off: its word 4 is a
    // mid-measure-clef baseline rather than the general one, and Finale 27 discards it.
    const auto f100Baseline = expectEarlyBaseline("evidence/F100/F100-clef-baseline.mus", {0, 0, 0}, "Finale 1.0.0");
    const auto f263Baseline = expectEarlyBaseline("evidence/F263/F263-clef-baseline.mus", {0, 0, 0}, "Finale 2.6.3");
    expect(field(f100Baseline, "options.clefOptions.clefDefs[0].baselineAdjust").rawValue == -4,
        "The Finale 1.0.0 stored baseline count was not reported");

    // The courtesy flags pack clef, key and time in one word; a stock document has all three
    // set, so a fixture that clears one bit is what distinguishes them.
    const auto clefCourtesyOff = read("evidence/F2005/F2005-courtesy-clef-off.mus");
    expect(!clefs(clefCourtesyOff)->cautionaryClefChanges, "Turning off the courtesy clef alone was not recovered");
    // Turning off the key signature's courtesy instead must leave the clef's alone. Without
    // this the test would pass for any bit that happens to be clear.
    const auto keyCourtesyOff = read("evidence/F2005/F2005-courtesy-key-off.mus");
    expect(clefs(keyCourtesyOff)->cautionaryClefChanges, "A courtesy key-signature edit was misread as the clef bit");

    // The Coda era has no courtesy-clef option and always shows one, so the reader asserts
    // that rather than reading selector 44, which is zero throughout the era and would say
    // the opposite. Both a plain Coda document and one whose key courtesy was turned off
    // must come out true.
    for (const char* path : {"evidence/F263/F263-baseline.mus", "evidence/F263/F263-courtesy-key-off.mus", "evidence/F100/F100-baseline.mus"}) {
        const auto coda = read(path);
        expect(clefs(coda)->cautionaryClefChanges, std::string("A Coda document did not always show a courtesy clef: ") + path);
        // Neither read from the file nor a baseline default: the era had no option, so the
        // behavior determines it. Reported once, and as behavior.
        const auto* courtesy = coda.report.findField<musx::dom::options::ClefOptions>("cautionaryClefChanges");
        expect(courtesy != nullptr, std::string("cautionaryClefChanges was reported more than once from ") + path);
        expect(field(coda, "options.clefOptions.cautionaryClefChanges").origin == ValueOrigin::LegacyBehavior,
            std::string("A Coda courtesy clef was not reported as legacy behavior: ") + path);
    }
    // Every other era reads it, so nothing else may claim behavior.
    for (const auto* result : {&f2002, &f2007, &f2000}) {
        expect(field(*result, "options.clefOptions.cautionaryClefChanges").origin == ValueOrigin::LegacyMus,
            "A recorded courtesy clef was reported as legacy behavior");
    }
    // Finale 3.0 through 3.5 predate the option too, but already carry the bit set, so the
    // epoch is the boundary and reading the bit gives the right answer there.
    expect(clefs(read("evidence/F372/F372-baseline.mus"))->cautionaryClefChanges, "A pre-3.6.2 uncompressed document lost its courtesy clef");

    for (const auto* result :
        {&f2002, &f2005, &f2007, &f2012, &f2000, &f100, &f2005Baseline, &f100Baseline, &f263Baseline, &f372Baseline, &f97Baseline}) {
        const auto options = result->document->getOptions()->get<ClefOptions>();
        for (std::size_t index = 0; index < options->clefDefs.size(); ++index) {
            const auto& def = options->clefDefs[index];
            // musxdom's own resolver rejects this combination, so it must never be built.
            expect(!def->useOwnFont || static_cast<bool>(def->font), "A clef claims its own font without carrying one");
            if (def->useOwnFont) {
                expect(static_cast<bool>(
                           result->document->getOthers()->get<musx::dom::others::FontDefinition>(musx::dom::SCORE_PARTID, def->font->fontId)),
                    "A recovered clef font has a dangling font id");
            }
        }
    }
}

TEST_CASE("Clef options capture", "[class][reader]")
{
    testClefOptionsCapture();
}

TEST_CASE("Clef tuple decoding", "[class]")
{
    testClefTupleDecoding();
}

} // namespace
} // namespace finale_mus_reader_tests
