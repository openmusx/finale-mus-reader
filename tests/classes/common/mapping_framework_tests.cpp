// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

void testMusicSpacingOptionsLayouts()
{
    using ColUnisonsChoice = Spacing::ColUnisonsChoice;
    using GraceNoteSpacing = Spacing::GraceNoteSpacing;
    using ManualPositioning = Spacing::ManualPositioning;
    const std::vector<std::int16_t> words{
        0,
        216,
        1800,
        12,
        24,
        0x78ef,
        1,
        1024,
        0,
        96,
        0,
        14141,
        0,
        0,
        7,
        0,
        1,
    };

    const auto run = [&](FormatEpoch epoch) {
        finale_mus_reader::container::ParsedContainer parsed = epoch == FormatEpoch::ZlibLegacy
                                                                   ? makeClassContainer(
                                                                         {
                                                                             {finale_mus_reader::numericGlobalClass(39), {36, 12}},
                                                                             {finale_mus_reader::numericGlobalClass(94), words},
                                                                         },
                                                                         ByteOrder::LittleEndian)
                                                                   : makeContainer(
                                                                         {
                                                                             {GLOBALS_CMPER, "39", {36, 12, 0, 0, 0, 0}},
                                                                             {GLOBALS_CMPER, "94", {0, 216, 1800, 12, 24, 0x78ef}},
                                                                             {GLOBALS_CMPER, "94", {1, 1024, 0, 96, 0, 14141}},
                                                                             {GLOBALS_CMPER, "94", {0, 0, 7, 0, 1, 0}},
                                                                         },
                                                                         epoch);
        Spacing* spacing = nullptr;
        Spacing* referenceSpacing = nullptr;
        const auto document = makeDocument(&spacing);
        const auto reference = makeDocument(&referenceSpacing);
        auto profile = profileFor(epoch == FormatEpoch::ZlibLegacy ? 12 : 9);
        profile.epoch = epoch;
        profile.byteOrder = parsed.byteOrder;
        ImportReport report(epoch);
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{
            LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::options::importMusicSpacingOptions(context);

        expectMapping(spacing->minWidth == 216 && spacing->maxWidth == 1800 && spacing->minDistance == 12 && spacing->minDistTiedNotes == 24,
            "Music spacing dimensions were not recovered");
        expectMapping(spacing->avoidColNotes && spacing->avoidColLyrics && spacing->avoidColChords && spacing->avoidColArtics
                          && spacing->avoidColClefs && spacing->avoidColSeconds && spacing->avoidColLedgers
                          && spacing->avoidColUnisons == ColUnisonsChoice::All,
            "Music spacing collision flags were not decoded");
        expectMapping(spacing->manualPositioning == ManualPositioning::Incorporate && spacing->ignoreHidden && spacing->interpolateAllotments
                          && spacing->usePrinter && spacing->useAllottmentTables,
            "Music spacing mode flags were not decoded");
        expectMapping(!spacing->avoidColStems && field(report, "options.musicSpacing.avoidColStems").origin == ValueOrigin::LegacyBehavior,
            "Legacy stem-collision behavior did not override the seeded MUSX setting");
        expectMapping(
            spacing->referenceDuration == 1024 && spacing->referenceWidth == 96 && spacing->scalingFactor == 1.4141
                && spacing->minDistGrace == (epoch == FormatEpoch::ZlibLegacy ? 7 : 12)
                && spacing->graceNoteSpacing == (epoch == FormatEpoch::ZlibLegacy ? GraceNoteSpacing::KeepCurrent : GraceNoteSpacing::Automatic)
                && spacing->musFront == 36 && spacing->musBack == 12,
            "Music spacing scaling and grace-note fields were not decoded");
        expectMapping(field(report, "options.musicSpacing.minDistGrace").origin
                          == (epoch == FormatEpoch::ZlibLegacy ? ValueOrigin::LegacyMus : ValueOrigin::LegacyBehavior),
            "The grace-note minimum-distance origin did not follow its version behavior");
        expectMapping(field(report, "options.musicSpacing.graceNoteSpacing").origin
                          == (epoch == FormatEpoch::ZlibLegacy ? ValueOrigin::LegacyMus : ValueOrigin::LegacyBehavior),
            "The grace-note spacing origin did not follow its version behavior");
        expectMapping(field(report, "options.musicSpacing.scalingFactor").origin == ValueOrigin::LegacyMus,
            "The music spacing scaling factor was not reported as recovered");
        expectMapping(field(report, "options.musicSpacing.defaultAllotment").origin == ValueOrigin::Finale27Default,
            "The non-UI allotment default was not retained from the Finale 27 seed");
    };

    run(FormatEpoch::UncompressedLegacy);
    run(FormatEpoch::DclLegacy);
    run(FormatEpoch::ZlibLegacy);

    const auto parsed = makeContainer({}, FormatEpoch::CodaBanner);
    Spacing* spacing = nullptr;
    Spacing* referenceSpacing = nullptr;
    const auto document = makeDocument(&spacing);
    const auto reference = makeDocument(&referenceSpacing);
    SourceProfile profile(FormatEpoch::CodaBanner);
    profile.byteOrder = parsed.byteOrder;
    ImportReport report(FormatEpoch::CodaBanner);
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const auto index = LegacyRecordIndex::build(parsed);
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::options::importMusicSpacingOptions(context);
    expectMapping(spacing->minWidth == 360 && field(report, "options.musicSpacing.minWidth").origin == ValueOrigin::LegacyBehavior,
        "The pre-selector-94 minimum-width behavior was not applied");
    expectMapping(spacing->useAllottmentTables && field(report, "options.musicSpacing.useAllottmentTables").origin == ValueOrigin::LegacyBehavior,
        "The pre-selector-94 allotment-table behavior was not applied");
    expectMapping(spacing->avoidColUnisons == ColUnisonsChoice::None && spacing->ignoreHidden
                      && field(report, "options.musicSpacing.avoidColUnisons").origin == ValueOrigin::LegacyBehavior
                      && field(report, "options.musicSpacing.ignoreHidden").origin == ValueOrigin::LegacyBehavior,
        "The pre-selector-94 collision behaviors were not applied");

    spacing->avoidColStems = false;
    ImportReport falseSeedReport(FormatEpoch::CodaBanner);
    const finale_mus_reader::ImportContext falseSeedContext{index, profile, noSource, document, reference, falseSeedReport, pending, construction};
    finale_mus_reader::options::importMusicSpacingOptions(falseSeedContext);
    expectMapping(field(falseSeedReport, "options.musicSpacing.avoidColStems").origin == ValueOrigin::MusxOnly,
        "A false MUSX seed was incorrectly reported as a legacy override");
}

// A four-byte value whose words straddle an incidence boundary: the low word is the last
// slot of incidence 1 and the high word is the first slot of incidence 2. This is the
// shape the distilled framework mapping records for MusicSpacingPrefs.scalingFactor.
void testFourByteStraddlesIncidence()
{
    const FieldMapping fields[] = {
        MUS_LONG(Spacing, "94", GLOBALS_CMPER, /*incidence*/ 1, /*slot*/ 5, LongWordOrder::LowFirst, referenceDuration),
    };
    const auto table = makeTable("options.spacing", fields, std::size(fields));
    const std::vector<const MappingTable*> tables{&table};

    const auto parsed = makeContainer({
        {GLOBALS_CMPER, "94", {0, 0, 0, 0, 0, 0}},
        {GLOBALS_CMPER, "94", {0, 0, 0, 0, 0, static_cast<std::int16_t>(0x5678)}},
        {GLOBALS_CMPER, "94", {0x1234, 0, 0, 0, 0, 0}},
    });

    Spacing* spacing = nullptr;
    auto document = makeDocument(&spacing);
    ImportReport report(FormatEpoch::UncompressedLegacy);
    finale_mus_reader::applyMappingTables(tables, LegacyRecordIndex::build(parsed), profileFor(7), document, report);

    expectMapping(spacing->referenceDuration == 0x12345678, "Four-byte value straddling an incidence boundary was not assembled");
    const auto& recovered = field(report, "options.spacing.referenceDuration");
    expectMapping(recovered.origin == ValueOrigin::LegacyMus && recovered.sourceIdentity == finale_mus_reader::numericGlobalTag(94),
        "Straddling four-byte value was not reported with its source identity");
}

// The same two words, read with each word order, must disagree.
void testLongWordOrder()
{
    const FieldMapping highFirst[] = {
        MUS_LONG(Spacing, "94", GLOBALS_CMPER, 0, 0, LongWordOrder::HighFirst, referenceDuration),
    };
    const FieldMapping lowFirst[] = {
        MUS_LONG(Spacing, "94", GLOBALS_CMPER, 0, 0, LongWordOrder::LowFirst, referenceDuration),
    };
    const auto parsed = makeContainer({
        {GLOBALS_CMPER, "94", {0x0102, 0x0304, 0, 0, 0, 0}},
    });

    const auto readWith = [&](const FieldMapping* fields) {
        const auto table = makeTable("options.spacing", fields, 1);
        const std::vector<const MappingTable*> tables{&table};
        Spacing* spacing = nullptr;
        auto document = makeDocument(&spacing);
        ImportReport report(FormatEpoch::UncompressedLegacy);
        finale_mus_reader::applyMappingTables(tables, LegacyRecordIndex::build(parsed), profileFor(7), document, report);
        return spacing->referenceDuration;
    };

    expectMapping(readWith(highFirst) == 0x01020304, "High-first four-byte order is wrong");
    expectMapping(readWith(lowFirst) == 0x03040102, "Low-first four-byte order is wrong");
}

// Booleans live as bits of a flag word, so bit addressing has to work before any such
// mapping can be promoted.
void testBitExtraction()
{
    const FieldMapping fields[] = {
        MUS_BIT(Spacing, "94", GLOBALS_CMPER, 0, 5, /*bit*/ 3, avoidColNotes),
    };
    const auto table = makeTable("options.spacing", fields, std::size(fields));
    const std::vector<const MappingTable*> tables{&table};

    const auto readFlag = [&](std::int16_t flagWord) {
        const auto parsed = makeContainer({{GLOBALS_CMPER, "94", {0, 0, 0, 0, 0, flagWord}}});
        Spacing* spacing = nullptr;
        auto document = makeDocument(&spacing);
        ImportReport report(FormatEpoch::UncompressedLegacy);
        finale_mus_reader::applyMappingTables(tables, LegacyRecordIndex::build(parsed), profileFor(7), document, report);
        return spacing->avoidColNotes;
    };

    expectMapping(readFlag(0x0008), "Bit 3 set was not extracted as true");
    expectMapping(!readFlag(0x0004), "An unrelated bit was reported as the mapped bit");
    expectMapping(readFlag(static_cast<std::int16_t>(0xfff8)), "Bit 3 was lost among other bits");
}

// A file with no matching record keeps the seeded value and still reports the field.
void testAbsentRecordKeepsSeededDefault()
{
    const FieldMapping fields[] = {
        MUS_WORD(Spacing, "94", GLOBALS_CMPER, 0, 1, minWidth),
    };
    const auto table = makeTable("options.spacing", fields, std::size(fields));
    const std::vector<const MappingTable*> tables{&table};

    const auto parsed = makeContainer({{7, "ZZ", {0, 0, 0, 0, 0, 0}}});
    Spacing* spacing = nullptr;
    auto document = makeDocument(&spacing);
    ImportReport report(FormatEpoch::UncompressedLegacy);
    finale_mus_reader::applyMappingTables(tables, LegacyRecordIndex::build(parsed), profileFor(7), document, report);

    expectMapping(spacing->minWidth == 111, "An absent record overwrote the seeded default");
    const auto info = field(report, "options.spacing.minWidth");
    expectMapping(info.origin == ValueOrigin::Finale27Default, "An absent record was not reported as a synthesized default");
    expectMapping(info.rawValue == 111, "The reported default did not carry the seeded value");
}

// A row gated to a later version must not be applied to an earlier file, and a file whose
// version could not be recovered matches only ungated rows.
void testVersionGating()
{
    const FieldMapping fields[] = {
        MUS_WORD_IF_SOURCE(Spacing, "94", GLOBALS_CMPER, 0, 1, &sourceAtFinale2007, minWidth),
    };
    const auto table = makeTable("options.spacing", fields, std::size(fields));
    const std::vector<const MappingTable*> tables{&table};
    const auto parsed = makeContainer({{GLOBALS_CMPER, "94", {0, 777, 0, 0, 0, 0}}});

    const auto readWithProfile = [&](const SourceProfile& profile) {
        Spacing* spacing = nullptr;
        auto document = makeDocument(&spacing);
        ImportReport report(FormatEpoch::UncompressedLegacy);
        finale_mus_reader::applyMappingTables(tables, LegacyRecordIndex::build(parsed), profile, document, report);
        return spacing->minWidth;
    };

    expectMapping(readWithProfile(profileFor(7)) == 111, "A row gated to a later version was applied to an earlier file");
    expectMapping(readWithProfile(profileFor(12)) == 777, "A row gated to this version was not applied");

    SourceProfile unknownVersion(FormatEpoch::UncompressedLegacy);
    expectMapping(readWithProfile(unknownVersion) == 111, "A gated row was applied to a file with no recovered version");
}

// Minor participates in ordering, because major alone does not separate Finale 97 from
// the Finale 3.x line.
void testMinorVersionOrdering()
{
    const FieldMapping fields[] = {
        MUS_WORD_IF_SOURCE(Spacing, "94", GLOBALS_CMPER, 0, 1, &sourceAtFinale35, minWidth),
    };
    const auto table = makeTable("options.spacing", fields, std::size(fields));
    const std::vector<const MappingTable*> tables{&table};
    const auto parsed = makeContainer({{GLOBALS_CMPER, "94", {0, 888, 0, 0, 0, 0}}});

    const auto readWith = [&](std::uint8_t major, std::uint8_t minor) {
        Spacing* spacing = nullptr;
        auto document = makeDocument(&spacing);
        ImportReport report(FormatEpoch::UncompressedLegacy);
        finale_mus_reader::applyMappingTables(tables, LegacyRecordIndex::build(parsed), profileFor(major, minor), document, report);
        return spacing->minWidth;
    };

    expectMapping(readWith(3, 2) == 111, "Version 3.2 matched a gate starting at 3.5");
    expectMapping(readWith(3, 7) == 888, "Version 3.7 did not match a gate starting at 3.5");
}

// Maintenance participates where a capability arrives in a point release.
void testMaintenanceVersionOrdering()
{
    const FieldMapping fields[] = {
        MUS_WORD_IF_SOURCE(Spacing, "94", GLOBALS_CMPER, 0, 1, &sourceAtFinale351, minWidth),
    };
    const auto table = makeTable("options.spacing", fields, std::size(fields));
    const std::vector<const MappingTable*> tables{&table};
    const auto parsed = makeContainer({{GLOBALS_CMPER, "94", {0, 888, 0, 0, 0, 0}}});

    const auto readWith = [&](std::uint8_t maint) {
        Spacing* spacing = nullptr;
        auto document = makeDocument(&spacing);
        ImportReport report(FormatEpoch::UncompressedLegacy);
        finale_mus_reader::applyMappingTables(tables, LegacyRecordIndex::build(parsed), profileFor(3, 5, maint), document, report);
        return spacing->minWidth;
    };

    expectMapping(readWith(0) == 111, "Finale 3.5 matched a gate starting at 3.5.1");
    expectMapping(readWith(1) == 888, "Finale 3.5.1 did not match its version gate");
}

// A later table supersedes an earlier one for the same field, so a field that moves costs
// one override row rather than a restatement of the whole table.
void testTableLayering()
{
    const FieldMapping baseFields[] = {
        MUS_WORD(Spacing, "94", GLOBALS_CMPER, 0, 1, minWidth),
        MUS_WORD(Spacing, "94", GLOBALS_CMPER, 0, 2, maxWidth),
    };
    const FieldMapping overrideFields[] = {
        MUS_WORD(Spacing, "94", GLOBALS_CMPER, 0, 3, minWidth),
    };
    const auto base = makeTable("options.spacing", baseFields, std::size(baseFields));
    const auto override = makeTable("options.spacing", overrideFields, std::size(overrideFields), &sourceAtFinale2007);
    const std::vector<const MappingTable*> tables{&base, &override};

    const auto parsed = makeContainer({{GLOBALS_CMPER, "94", {0, 11, 22, 33, 0, 0}}});

    const auto readWith = [&](std::uint8_t major) {
        Spacing* spacing = nullptr;
        auto document = makeDocument(&spacing);
        ImportReport report(FormatEpoch::UncompressedLegacy);
        finale_mus_reader::applyMappingTables(tables, LegacyRecordIndex::build(parsed), profileFor(major), document, report);
        return std::pair<int, int>{spacing->minWidth, spacing->maxWidth};
    };

    const auto early = readWith(7);
    expectMapping(early.first == 11, "The base location was not used below the override version");
    expectMapping(early.second == 22, "The base table lost an unrelated field");

    const auto late = readWith(12);
    expectMapping(late.first == 33, "The override location did not supersede the base location");
    expectMapping(late.second == 22, "The override table dropped a field it does not restate");
}

// An epoch the tables do not cover still reports its supported fields as defaults.
void testUncoveredEpochStillReports()
{
    const FieldMapping fields[] = {
        MUS_WORD(Spacing, "94", GLOBALS_CMPER, 0, 1, minWidth),
    };
    const auto table = makeTable("options.spacing", fields, std::size(fields));
    const std::vector<const MappingTable*> tables{&table};
    const auto parsed = makeContainer({{GLOBALS_CMPER, "94", {0, 777, 0, 0, 0, 0}}});

    SourceProfile profile(FormatEpoch::ZlibLegacy);
    Spacing* spacing = nullptr;
    auto document = makeDocument(&spacing);
    ImportReport report(FormatEpoch::UncompressedLegacy);
    finale_mus_reader::applyMappingTables(tables, LegacyRecordIndex::build(parsed), profile, document, report);

    expectMapping(spacing->minWidth == 111, "A table was applied outside its epoch");
    expectMapping(field(report, "options.spacing.minWidth").origin == ValueOrigin::Finale27Default,
        "An uncovered epoch did not report its supported field as a default");
}

// Details carry a second comparator, which displaces the tag two bytes and leaves five
// payload words instead of six. The index normalizes both shapes into one row type.
// Two synthetic cases cover a sign-extended clef character and
// the payload size that both tuple widths divide evenly.

TEST_CASE("Music spacing options span the located layouts", "[class]")
{
    testMusicSpacingOptionsLayouts();
}
TEST_CASE("Four-byte incidence straddling", "[class]")
{
    testFourByteStraddlesIncidence();
}
TEST_CASE("Long word order", "[class]")
{
    testLongWordOrder();
}
TEST_CASE("Bit extraction", "[class]")
{
    testBitExtraction();
}
TEST_CASE("Absent record keeps seeded default", "[class]")
{
    testAbsentRecordKeepsSeededDefault();
}
TEST_CASE("Version gating", "[class]")
{
    testVersionGating();
}
TEST_CASE("Minor version ordering", "[class]")
{
    testMinorVersionOrdering();
}
TEST_CASE("Maintenance version ordering", "[class]")
{
    testMaintenanceVersionOrdering();
}
TEST_CASE("Table layering", "[class]")
{
    testTableLayering();
}
TEST_CASE("Uncovered epoch still reports", "[class]")
{
    testUncoveredEpochStillReports();
}

TEST_CASE("Deferred steps run stage by stage, and a reversed drain reverses only within a stage", "[mapping]")
{
    using finale_mus_reader::DeferredOrder;
    using finale_mus_reader::DeferredStage;
    const auto drain = [](DeferredOrder order) {
        std::vector<int> ran;
        finale_mus_reader::PendingReferences pending;
        // Registered out of stage order: stage, not registration, decides which runs first.
        pending.defer(DeferredStage::ReadComposites, [&ran] { ran.push_back(5); });
        pending.defer(DeferredStage::Synthesize, [&ran] { ran.push_back(3); });
        pending.defer(DeferredStage::CompletePools, [&ran] { ran.push_back(1); });
        pending.defer(DeferredStage::Synthesize, [&ran] { ran.push_back(4); });
        pending.defer(DeferredStage::CompletePools, [&ran] { ran.push_back(2); });
        finale_mus_reader::runDeferredChecks(pending, order);
        return ran;
    };
    CHECK(drain(DeferredOrder::AsRegistered) == std::vector<int>{1, 2, 3, 4, 5});
    CHECK(drain(DeferredOrder::ReversedWithinStage) == std::vector<int>{2, 1, 4, 3, 5});
}

} // namespace
} // namespace finale_mus_reader_tests
