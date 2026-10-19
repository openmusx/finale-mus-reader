// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

musx::dom::DocumentPtr makeAugmentationDotOptionsDocument()
{
    using Target = musx::dom::options::AugmentationDotOptions;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<Target>(document);
    options->dotUpFlagOffset = 7;
    options->dotOffset = 8;
    options->dotNoteOffset = 9;
    options->dotLift = 10;
    options->adjMultipleVoices = true;
    options->useLegacyFlippedStemPositioning = false;
    document->getOptions()->add(Target::XmlNodeName, options);
    return std::move(session).finish();
}

void testAugmentationDotOptionsLegacyFlippedStemPositioning()
{
    using Target = musx::dom::options::AugmentationDotOptions;
    const auto runImport = [](const SourceProfile& profile, ImportReport& report) {
        const auto parsed = finale_mus_reader::container::ParsedContainer(profile.epoch);
        const auto document = makeAugmentationDotOptionsDocument();
        const auto reference = makeAugmentationDotOptionsDocument();
        auto index = LegacyRecordIndex::build(parsed);
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::options::importAugmentationDotOptions(context);
        return document->getOptions()->get<Target>();
    };

    const auto expectLegacyBehavior = [](const auto& options, const ImportReport& report, const char* label, bool codaBanner) {
        expectMapping(options->useLegacyFlippedStemPositioning, std::string(label).append(" did not force true"));
        expectMapping(field(report, "options.augmentationDotOptions.useLegacyFlippedStemPositioning").origin == ValueOrigin::LegacyBehavior,
            std::string(label).append(" did not report LegacyBehavior"));
        expectMapping(field(report, "options.augmentationDotOptions.useLegacyFlippedStemPositioning").rawValue == 1,
            std::string(label).append(" did not report the legacy-flipped-stem value"));

        expectMapping(options->adjMultipleVoices != codaBanner, std::string(label).append(" applied the wrong multiple-voice behavior"));
        expectMapping(field(report, "options.augmentationDotOptions.adjMultipleVoices").origin
                          == (codaBanner ? ValueOrigin::LegacyBehavior : ValueOrigin::Finale27Default),
            std::string(label).append(" reported the wrong multiple-voice origin"));
    };

    {
        auto profile = profileFor(3, 7);
        profile.epoch = FormatEpoch::CodaBanner;
        profile.version.reset();
        ImportReport report(FormatEpoch::CodaBanner);
        expectLegacyBehavior(runImport(profile, report), report, "The Coda banner epoch", true);
    }

    {
        auto profile = profileFor(3, 7);
        profile.epoch = FormatEpoch::UncompressedLegacy;
        ImportReport report(FormatEpoch::UncompressedLegacy);
        expectLegacyBehavior(runImport(profile, report), report, "The uncompressed epoch", false);
    }

    {
        auto profile = profileFor(4, 0);
        profile.epoch = FormatEpoch::DclLegacy;
        ImportReport report(FormatEpoch::DclLegacy);
        expectLegacyBehavior(runImport(profile, report), report, "The DCL epoch", false);
    }

    {
        auto profile = profileFor(15, 1);
        profile.epoch = FormatEpoch::ZlibLegacy;
        ImportReport report(FormatEpoch::ZlibLegacy);
        expectLegacyBehavior(runImport(profile, report), report, "The zlib epoch", false);
    }
}

void testCodaAugmentationDotOffset()
{
    using Target = musx::dom::options::AugmentationDotOptions;
    const auto parsed = makeContainer({{GLOBALS_CMPER, "21", {4, 13, 18, 6, 4, 4}}}, FormatEpoch::CodaBanner);
    auto profile = profileFor(2, 6);
    profile.epoch = FormatEpoch::CodaBanner;
    profile.version.reset();
    const auto document = makeAugmentationDotOptionsDocument();
    const auto reference = makeAugmentationDotOptionsDocument();
    ImportReport report(FormatEpoch::CodaBanner);
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{
        LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};

    finale_mus_reader::options::importAugmentationDotOptions(context);

    const auto options = document->getOptions()->get<Target>();
    expectMapping(options->dotOffset == 13, "The Coda epoch did not recover dotOffset from selector 21 word 1");
    expectMapping(field(report, "options.augmentationDotOptions.dotOffset").origin == ValueOrigin::LegacyMus
                      && field(report, "options.augmentationDotOptions.dotOffset").rawValue == 13,
        "The Coda dot offset reported incorrect source provenance");
    expectMapping(options->dotUpFlagOffset == 7 && options->dotNoteOffset == 9 && options->dotLift == 10,
        "The Coda dot-offset mapping disturbed unresolved augmentation-dot fields");
}

void testCodaAugmentationDotOffsetFixture()
{
    using musx::dom::options::AugmentationDotOptions;
    const auto result =
        Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F263/F263-dotoff-13.mus");
    const auto options = result.document->getOptions()->get<AugmentationDotOptions>();
    expect(options && options->dotOffset == 13, "The controlled Finale 2.6.3 augmentation-dot offset was not recovered");

    const auto& source = field(result, "options.augmentationDotOptions.dotOffset");
    expect(source.origin == ValueOrigin::LegacyMus && source.rawValue == 13 && source.blockOffset == 0x208 && source.decodedOffset == 0x22d0,
        "The Finale 2.6.3 dot offset reported the wrong selector-row provenance");
    expect(field(result, "options.augmentationDotOptions.dotUpFlagOffset").origin == ValueOrigin::Finale27Default
               && field(result, "options.augmentationDotOptions.dotNoteOffset").origin == ValueOrigin::Finale27Default
               && field(result, "options.augmentationDotOptions.dotLift").origin == ValueOrigin::Finale27Default,
        "The single located Coda augmentation-dot field widened into unresolved words");
}

TEST_CASE("Coda augmentation-dot offset", "[class][reader]")
{
    testCodaAugmentationDotOffsetFixture();
}

TEST_CASE("Augmentation-dot options apply legacy flipped-stem positioning as LegacyBehavior", "[class]")
{
    testAugmentationDotOptionsLegacyFlippedStemPositioning();
}

TEST_CASE("Augmentation-dot options recover the Coda-era dot offset", "[class]")
{
    testCodaAugmentationDotOffset();
}

} // namespace
} // namespace finale_mus_reader_tests
