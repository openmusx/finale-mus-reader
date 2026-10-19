// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

using KeySignatureTarget = musx::dom::options::KeySignatureOptions;

musx::dom::DocumentPtr makeKeySignatureOptionsDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<KeySignatureTarget>(document);
    options->doKeyCancel = true;
    options->doCStart = false;
    options->redisplayOnModeChange = true;
    options->keyFront = 91;
    options->keyMid = 92;
    options->keyBack = 93;
    options->acciAdd = 94;
    options->showKeyFirstSystemOnly = false;
    options->keyTimeSepar = 95;
    options->simplifyKeyHoldOctave = false;
    options->cautionaryKeyChanges = true;
    options->doKeyCancelBetweenSharpsFlats = true;
    document->getOptions()->add(KeySignatureTarget::XmlNodeName, options);
    return std::move(session).finish();
}

std::shared_ptr<const KeySignatureTarget> importKeySignatureOptions(
    const finale_mus_reader::container::ParsedContainer& parsed, FormatEpoch epoch, ImportReport& report)
{
    const auto document = makeKeySignatureOptionsDocument();
    const auto reference = makeKeySignatureOptionsDocument();
    auto profile = profileFor(epoch == FormatEpoch::CodaBanner ? 2 : 7);
    profile.epoch = epoch;
    profile.byteOrder = parsed.byteOrder;
    if (epoch == FormatEpoch::CodaBanner) {
        profile.version.reset();
    }
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{
        LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::options::importKeySignatureOptions(context);
    return document->getOptions()->get<KeySignatureTarget>();
}

void verifyCommonFields(const std::shared_ptr<const KeySignatureTarget>& options, const ImportReport& report)
{
    expect(options->doCStart && !options->redisplayOnModeChange && options->keyFront == -11 && options->keyMid == -12 && options->keyBack == -13
               && options->acciAdd == 14 && options->showKeyFirstSystemOnly && options->keyTimeSepar == -15,
        "Key-signature options did not recover the common legacy fields");
    expect(field(report, "options.keySignatureOptions.keyFront").origin == ValueOrigin::LegacyMus
               && field(report, "options.keySignatureOptions.keyTimeSepar").origin == ValueOrigin::LegacyMus
               && field(report, "options.keySignatureOptions.doKeyCancelBetweenSharpsFlats").origin == ValueOrigin::LegacyBehavior,
        "Key-signature options reported an incorrect common field origin");
}

TEST_CASE("Key signature options span every legacy epoch", "[class]")
{
    const std::vector<SyntheticRow> fixedRows{
        {GLOBALS_CMPER, "12", {0, 0, 1, 0, 0, 0}},
        {GLOBALS_CMPER, "18", {-11, -12, -13, 0, 0, 0}},
        {GLOBALS_CMPER, "21", {0, 0, 0, 0, 14, 0}},
        {GLOBALS_CMPER, "27", {0, 0, 1, 0, 0, 0}},
        {GLOBALS_CMPER, "39", {0, 0, 0, 0, 0, -15}},
        {GLOBALS_CMPER, "41", {0, 0, 0, 0, 0, 0}},
        {GLOBALS_CMPER, "41", {0, 0, 0, 0, 0, 0}},
        {GLOBALS_CMPER, "41", {0, 1, 0, 0, 0, 0}},
        {GLOBALS_CMPER, "44", {0, 0, 0, 0, 0, 0}},
    };

    {
        const auto epoch = FormatEpoch::UncompressedLegacy;
        ImportReport report(epoch);
        const auto options = importKeySignatureOptions(makeContainer(fixedRows, epoch), epoch, report);
        verifyCommonFields(options, report);
        expect(!options->doKeyCancel && !options->simplifyKeyHoldOctave && !options->cautionaryKeyChanges,
            "An uncompressed key-signature field was decoded incorrectly");
        expect(field(report, "options.keySignatureOptions.doKeyCancel").origin == ValueOrigin::LegacyMus
                   && field(report, "options.keySignatureOptions.simplifyKeyHoldOctave").origin == ValueOrigin::LegacyBehavior
                   && field(report, "options.keySignatureOptions.cautionaryKeyChanges").origin == ValueOrigin::LegacyMus,
            "An uncompressed key-signature field reported an incorrect origin");
    }

    {
        const auto epoch = FormatEpoch::DclLegacy;
        ImportReport report(epoch);
        const auto options = importKeySignatureOptions(makeContainer(fixedRows, epoch), epoch, report);
        verifyCommonFields(options, report);
        expect(!options->doKeyCancel && options->simplifyKeyHoldOctave && !options->cautionaryKeyChanges,
            "A DCL key-signature field was decoded incorrectly");
        expect(field(report, "options.keySignatureOptions.simplifyKeyHoldOctave").origin == ValueOrigin::LegacyMus,
            "A DCL key-signature field reported an incorrect origin");
    }

    {
        ImportReport report(FormatEpoch::CodaBanner);
        const auto options = importKeySignatureOptions(makeContainer(fixedRows, FormatEpoch::CodaBanner), FormatEpoch::CodaBanner, report);
        expect(!options->doKeyCancel && options->doCStart && !options->redisplayOnModeChange && options->keyFront == -11 && options->keyMid == -12
                   && options->keyBack == -13 && options->acciAdd == 14 && !options->showKeyFirstSystemOnly && options->keyTimeSepar == 95
                   && !options->simplifyKeyHoldOctave && options->cautionaryKeyChanges && options->doKeyCancelBetweenSharpsFlats,
            "The Coda-banner key-signature layout reused a later location or meaning");
        expect(field(report, "options.keySignatureOptions.doKeyCancel").origin == ValueOrigin::LegacyMus
                   && field(report, "options.keySignatureOptions.showKeyFirstSystemOnly").origin == ValueOrigin::Finale27Default
                   && field(report, "options.keySignatureOptions.keyTimeSepar").origin == ValueOrigin::Finale27Default
                   && field(report, "options.keySignatureOptions.simplifyKeyHoldOctave").origin == ValueOrigin::LegacyBehavior
                   && field(report, "options.keySignatureOptions.cautionaryKeyChanges").origin == ValueOrigin::LegacyBehavior,
            "The Coda-banner key-signature fields reported incorrect origins");
    }

    const std::vector<SyntheticClassRow> classRows{
        {finale_mus_reader::numericGlobalClass(12), {0, 0, 1, 0, 0, 0}},
        {finale_mus_reader::numericGlobalClass(18), {-11, -12, -13, 0, 0, 0}},
        {finale_mus_reader::numericGlobalClass(21), {0, 0, 0, 0, 14, 0}},
        {finale_mus_reader::numericGlobalClass(27), {0, 0, 1, 0, 0, 0}},
        {finale_mus_reader::numericGlobalClass(39), {0, 0, 0, 0, 0, -15}},
        {finale_mus_reader::numericGlobalClass(41), {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}},
        {finale_mus_reader::numericGlobalClass(44), {0, 0, 0, 0, 0, 0}},
    };
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        ImportReport report(FormatEpoch::ZlibLegacy);
        const auto options = importKeySignatureOptions(makeClassContainer(classRows, byteOrder), FormatEpoch::ZlibLegacy, report);
        verifyCommonFields(options, report);
        expect(!options->doKeyCancel && options->simplifyKeyHoldOctave && !options->cautionaryKeyChanges,
            "A class-record key-signature field was decoded incorrectly");
    }
}

TEST_CASE("Controlled Coda key behaviors recover from selector 12", "[class]")
{
    const auto baseline = readFixture("evidence/F263/F263-baseline.mus");
    const auto restrike = readFixture("evidence/F263/F263-key-restrikeC.mus");
    const auto noCancel = readFixture("evidence/F263/F263-nokeycxl.mus");
    const auto baselineOptions = baseline.document->getOptions()->get<KeySignatureTarget>();
    const auto restrikeOptions = restrike.document->getOptions()->get<KeySignatureTarget>();
    const auto noCancelOptions = noCancel.document->getOptions()->get<KeySignatureTarget>();

    expect(baselineOptions->doKeyCancel && !baselineOptions->doCStart && baselineOptions->redisplayOnModeChange && !noCancelOptions->doKeyCancel
               && restrikeOptions->doCStart,
        "Controlled Coda key behaviors were not recovered");
    expect(field(noCancel, "options.keySignatureOptions.doKeyCancel").origin == ValueOrigin::LegacyMus
               && field(restrike, "options.keySignatureOptions.doCStart").origin == ValueOrigin::LegacyMus
               && field(baseline, "options.keySignatureOptions.cautionaryKeyChanges").origin == ValueOrigin::LegacyBehavior,
        "Controlled Coda key behaviors reported incorrect origins");
}

TEST_CASE("Controlled Coda key distances recover from selector 18", "[class]")
{
    const auto result = readFixture("evidence/F100/F100-keyopts-vals.mus");
    const auto options = result.document->getOptions()->get<KeySignatureTarget>();
    expect(options->keyFront == 23 && options->keyMid == -7 && options->keyBack == 11 && options->acciAdd == 4,
        "Controlled Coda key distances were not recovered");
    expect(field(result, "options.keySignatureOptions.keyFront").origin == ValueOrigin::LegacyMus,
        "Controlled Coda key distances reported incorrect origins");
}

TEST_CASE("Controlled Coda clef and time edits do not supply key-time spacing", "[class]")
{
    const auto result = readFixture("evidence/F100/F100-cleftime-separs.mus");
    const auto options = result.document->getOptions()->get<KeySignatureTarget>();
    expect(options->keyFront == 24 && options->keyMid == 0 && options->keyBack == 12 && options->keyTimeSepar == 0,
        "Controlled Coda clef and time edits leaked into key-signature spacing");
    expect(field(result, "options.keySignatureOptions.keyTimeSepar").origin == ValueOrigin::Finale27Default,
        "The unlocated Coda key-time spacing field reported an incorrect origin");
}

TEST_CASE("Controlled later courtesy-key edit recovers its packed bit", "[class]")
{
    const auto baseline = readFixture("evidence/F2005/F2005-clef-baseline.mus");
    const auto disabled = readFixture("evidence/F2005/F2005-courtesy-key-off.mus");
    const auto baselineOptions = baseline.document->getOptions()->get<KeySignatureTarget>();
    const auto disabledOptions = disabled.document->getOptions()->get<KeySignatureTarget>();
    expect(
        baselineOptions->cautionaryKeyChanges && !disabledOptions->cautionaryKeyChanges, "The controlled packed courtesy-key edit was not recovered");
    expect(field(disabled, "options.keySignatureOptions.cautionaryKeyChanges").origin == ValueOrigin::LegacyMus,
        "The controlled packed courtesy-key edit reported an incorrect origin");
}

} // namespace
} // namespace finale_mus_reader_tests
