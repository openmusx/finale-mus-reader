// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

using GraceNoteOptionsTarget = musx::dom::options::GraceNoteOptions;

musx::dom::DocumentPtr makeGraceNoteOptionsDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<GraceNoteOptionsTarget>(document);
    options->tabGracePerc = 91;
    options->gracePerc = 92;
    options->playbackDuration = 93;
    options->entryOffset = 94;
    options->slashFlaggedGraceNotes = false;
    options->graceSlashWidth = 95;
    document->getOptions()->add(GraceNoteOptionsTarget::XmlNodeName, options);
    return std::move(session).finish();
}

std::shared_ptr<const GraceNoteOptionsTarget> importGraceNoteOptions(
    const finale_mus_reader::container::ParsedContainer& parsed, FormatEpoch epoch, ImportReport& report)
{
    const auto document = makeGraceNoteOptionsDocument();
    const auto reference = makeGraceNoteOptionsDocument();
    auto profile = profileFor(epoch == FormatEpoch::ZlibLegacy ? 12 : 5, 0);
    profile.epoch = epoch;
    profile.byteOrder = parsed.byteOrder;
    if (epoch == FormatEpoch::CodaBanner) {
        profile.version.reset();
    }
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{
        LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::options::importGraceNoteOptions(context);
    return document->getOptions()->get<GraceNoteOptionsTarget>();
}

void expectCompleteManifest(const ImportReport& report)
{
    const auto found = report.fields.find(finale_mus_reader::instanceKey<GraceNoteOptionsTarget>());
    expectMapping(found != report.fields.end() && found->second.size() == 6, "GraceNoteOptions did not report exactly its six musxdom fields");
    for (const auto* member : {"tabGracePerc", "gracePerc", "playbackDuration", "entryOffset", "slashFlaggedGraceNotes", "graceSlashWidth"}) {
        expectMapping(
            fieldPresent(report, std::string("options.graceNoteOptions.") + member), std::string("GraceNoteOptions omitted field ") + member);
    }
}

void expectRecovered(const GraceNoteOptionsTarget& options, const ImportReport& report)
{
    expectMapping(options.tabGracePerc == 81 && options.gracePerc == 61 && options.playbackDuration == 127 && options.entryOffset == -25
                      && options.slashFlaggedGraceNotes && options.graceSlashWidth == 116,
        "GraceNoteOptions did not recover every stored field");
    expectCompleteManifest(report);
    for (const auto* member : {"tabGracePerc", "gracePerc", "playbackDuration", "entryOffset", "slashFlaggedGraceNotes", "graceSlashWidth"}) {
        expectMapping(field(report, std::string("options.graceNoteOptions.") + member).origin == ValueOrigin::LegacyMus,
            std::string("GraceNoteOptions reported an incorrect origin for ") + member);
    }
}

const std::vector<SyntheticRow> fixedGraceNoteRows{
    {GLOBALS_CMPER, "14", {0, 0, 81, 0, 0, 0}},
    {GLOBALS_CMPER, "23", {61, 0, 0, 0, 0, 0}},
    {GLOBALS_CMPER, "27", {0, 0, 0, 0, 127, -25}},
    {GLOBALS_CMPER, "44", {0, 0, 0, 0, 1, 0}},
    {GLOBALS_CMPER, "64", {0, 116, 0, 0, 0, 0}},
};

const std::vector<SyntheticClassRow> classGraceNoteRows{
    {finale_mus_reader::numericGlobalClass(14), {0, 0, 81}},
    {finale_mus_reader::numericGlobalClass(23), {61}},
    {finale_mus_reader::numericGlobalClass(27), {0, 0, 0, 0, 127, -25}},
    {finale_mus_reader::numericGlobalClass(44), {0, 0, 0, 0, 1}},
    {finale_mus_reader::numericGlobalClass(64), {0, 116}},
};

TEST_CASE("Grace-note options recover fixed rows across their two epochs", "[class]")
{
    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        ImportReport report(epoch);
        const auto options = importGraceNoteOptions(makeContainer(fixedGraceNoteRows, epoch), epoch, report);
        expectRecovered(*options, report);
        expectMapping(field(report, "options.graceNoteOptions.tabGracePerc").decodedOffset == 0
                          && field(report, "options.graceNoteOptions.gracePerc").decodedOffset == 16
                          && field(report, "options.graceNoteOptions.playbackDuration").decodedOffset == 32
                          && field(report, "options.graceNoteOptions.entryOffset").decodedOffset == 32
                          && field(report, "options.graceNoteOptions.slashFlaggedGraceNotes").decodedOffset == 48
                          && field(report, "options.graceNoteOptions.graceSlashWidth").decodedOffset == 64,
            "GraceNoteOptions reported incorrect fixed-row offsets");
    }
}

TEST_CASE("Grace-note options recover zlib classes in either byte order", "[class]")
{
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        ImportReport report(FormatEpoch::ZlibLegacy);
        const auto options = importGraceNoteOptions(makeClassContainer(classGraceNoteRows, byteOrder), FormatEpoch::ZlibLegacy, report);
        expectRecovered(*options, report);
        expectMapping(field(report, "options.graceNoteOptions.tabGracePerc").decodedOffset == 0
                          && field(report, "options.graceNoteOptions.gracePerc").decodedOffset == 20
                          && field(report, "options.graceNoteOptions.playbackDuration").decodedOffset == 36
                          && field(report, "options.graceNoteOptions.entryOffset").decodedOffset == 36
                          && field(report, "options.graceNoteOptions.slashFlaggedGraceNotes").decodedOffset == 62
                          && field(report, "options.graceNoteOptions.graceSlashWidth").decodedOffset == 86,
            "GraceNoteOptions reported incorrect class-record offsets");
    }
}

TEST_CASE("Coda grace-note options recover only the located field", "[class]")
{
    ImportReport report(FormatEpoch::CodaBanner);
    const auto options =
        importGraceNoteOptions(makeContainer({{GLOBALS_CMPER, "23", {61, 0, 0, 0, 0, 0}}}, FormatEpoch::CodaBanner), FormatEpoch::CodaBanner, report);

    expectMapping(options->tabGracePerc == 91 && options->gracePerc == 61 && options->playbackDuration == 93 && options->entryOffset == 94
                      && !options->slashFlaggedGraceNotes && options->graceSlashWidth == 95,
        "The Coda grace-note layout disturbed an unlocated field");
    expectCompleteManifest(report);
    expectMapping(field(report, "options.graceNoteOptions.gracePerc").origin == ValueOrigin::LegacyMus,
        "The located Coda grace-note field reported an incorrect origin");
    for (const auto* member : {"tabGracePerc", "playbackDuration", "entryOffset", "slashFlaggedGraceNotes", "graceSlashWidth"}) {
        expectMapping(field(report, std::string("options.graceNoteOptions.") + member).origin == ValueOrigin::Finale27Default,
            std::string("The Coda grace-note field was not kept as a pinned default: ") + member);
    }
}

TEST_CASE("Missing grace-note records retain seeded defaults", "[class]")
{
    ImportReport report(FormatEpoch::UncompressedLegacy);
    const auto options = importGraceNoteOptions(makeContainer({}, FormatEpoch::UncompressedLegacy), FormatEpoch::UncompressedLegacy, report);
    expectMapping(options->tabGracePerc == 91 && options->gracePerc == 92 && options->playbackDuration == 93 && options->entryOffset == 94
                      && !options->slashFlaggedGraceNotes && options->graceSlashWidth == 95,
        "Missing grace-note records did not retain their seeded values");
    expectCompleteManifest(report);
    expectMapping(field(report, "options.graceNoteOptions.tabGracePerc").origin == ValueOrigin::Finale27Default
                      && field(report, "options.graceNoteOptions.graceSlashWidth").origin == ValueOrigin::Finale27Default,
        "Missing grace-note records reported an incorrect origin");
}

TEST_CASE("Grace-note options match controlled cross-epoch baselines", "[class]")
{
    struct ExpectedGraceNoteOptions
    {
        const char* path;
        int tabGracePerc;
        int gracePerc;
        int playbackDuration;
        int entryOffset;
        bool slashFlaggedGraceNotes;
        int graceSlashWidth;
        ValueOrigin tabGraceOrigin;
    };
    const ExpectedGraceNoteOptions expected[] = {
        {"evidence/F100/F100-baseline.mus", 85, 50, 128, 24, true, 128, ValueOrigin::Finale27Default},
        {"evidence/F97/Fin97-baseline.mus", 85, 70, 128, 30, true, 128, ValueOrigin::LegacyMus},
        {"evidence/F2003/F2003-baseline.mus", 85, 50, 128, 24, true, 224, ValueOrigin::LegacyMus},
        {"evidence/F2012/F2012-baseline.mus", 85, 50, 128, 24, true, 224, ValueOrigin::LegacyMus},
    };

    for (const auto& item : expected) {
        const auto result = readFixture(item.path);
        const auto options = result.document->getOptions()->get<GraceNoteOptionsTarget>();
        expectMapping(options->tabGracePerc == item.tabGracePerc && options->gracePerc == item.gracePerc
                          && options->playbackDuration == item.playbackDuration && options->entryOffset == item.entryOffset
                          && options->slashFlaggedGraceNotes == item.slashFlaggedGraceNotes && options->graceSlashWidth == item.graceSlashWidth,
            std::string("GraceNoteOptions disagreed with the controlled baseline: ") + item.path);
        expectCompleteManifest(result.report);
        expectMapping(field(result.report, "options.graceNoteOptions.tabGracePerc").origin == item.tabGraceOrigin
                          && field(result.report, "options.graceNoteOptions.gracePerc").origin == ValueOrigin::LegacyMus,
            std::string("GraceNoteOptions reported incorrect controlled-baseline "
                        "origins: ")
                + item.path);
    }
}

TEST_CASE("Finale 1 grace-note percentage is source owned", "[class]")
{
    const auto baseline = readFixture("evidence/F100/F100-baseline.mus");
    const auto changed = readFixture("evidence/F100/F100-grace-pct.mus");
    const auto baselineOptions = baseline.document->getOptions()->get<GraceNoteOptionsTarget>();
    const auto changedOptions = changed.document->getOptions()->get<GraceNoteOptionsTarget>();

    expectMapping(
        baselineOptions->gracePerc == 50 && changedOptions->gracePerc == 49, "The controlled Finale 1 grace-note percentage was not recovered");
    const auto& changedField = field(changed, "options.graceNoteOptions.gracePerc");
    expectMapping(changedField.origin == ValueOrigin::LegacyMus && changedField.blockOffset == 0x208 && changedField.decodedOffset == 0x5b0,
        "The controlled Finale 1 grace-note percentage reported incorrect provenance");
    expectMapping(
        changedOptions->graceSlashWidth == 128 && field(changed, "options.graceNoteOptions.graceSlashWidth").origin == ValueOrigin::LegacyMus,
        "The percentage edit was incorrectly interpreted as a grace-slash width");
}

TEST_CASE("Finale 1 default line width supplies the grace-slash width", "[class]")
{
    const auto baseline = readFixture("evidence/F100/F100-baseline.mus");
    const auto changed = readFixture("evidence/F100/F100-deflne-625.mus");
    const auto baselineOptions = baseline.document->getOptions()->get<GraceNoteOptionsTarget>();
    const auto changedOptions = changed.document->getOptions()->get<GraceNoteOptionsTarget>();

    expectMapping(baselineOptions->graceSlashWidth == 128 && changedOptions->graceSlashWidth == 160,
        "The Coda default-line width was not converted from points to Efix");
    const auto& changedWidth = field(changed, "options.graceNoteOptions.graceSlashWidth");
    expectMapping(changedWidth.origin == ValueOrigin::LegacyMus && changedWidth.sourceIdentity == finale_mus_reader::numericGlobalTag(54),
        "The Coda grace-slash width was not reported as source owned");
}

TEST_CASE("Coda selector 64 supersedes the original default-line float", "[class]")
{
    struct Expected
    {
        const char* path;
        int width;
    };
    for (const auto& item : {Expected{"evidence/F263/F263-clef-baseline.mus", 64}, Expected{"evidence/F263/F263-brace-psvs.mus", 83}}) {
        const auto result = readFixture(item.path);
        const auto options = result.document->getOptions()->get<GraceNoteOptionsTarget>();
        expectMapping(options->graceSlashWidth == item.width, std::string("The migrated Coda grace-slash width was not recovered: ") + item.path);
        const auto& width = field(result, "options.graceNoteOptions.graceSlashWidth");
        expectMapping(width.origin == ValueOrigin::LegacyMus && width.sourceIdentity == finale_mus_reader::numericGlobalTag(64),
            std::string("The migrated Coda grace-slash width was not source owned: ") + item.path);
    }
}

} // namespace
} // namespace finale_mus_reader_tests
