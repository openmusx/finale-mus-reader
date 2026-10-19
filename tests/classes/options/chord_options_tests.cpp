// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

using ChordOptions = musx::dom::options::ChordOptions;
using FretboardStyle = musx::dom::others::FretboardStyle;

constexpr musx::dom::Cmper referenceFretStyleId = 91;
constexpr musx::dom::Cmper referenceFretInstId = 92;
constexpr musx::dom::Cmper storedFretStyleId = 2;

/// @brief A reference document carrying the two fret comparators the pinned baseline supplies.
musx::dom::DocumentPtr chordReferenceDocument(musx::factory::DocumentFactory::ConstructionSession& session)
{
    const auto document = session.getDocument();
    auto options = std::make_shared<ChordOptions>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All);
    options->fretStyleId = referenceFretStyleId;
    options->fretInstId = referenceFretInstId;
    document->getOptions()->add(ChordOptions::XmlNodeName, options);
    return document;
}

/// @brief The rows a Finale 2006 document stores for the two fields under test.
/// @details Selector 41 incidence 1 slot 0 is the fret style comparator. The `ft` family is one
/// fretboard style; its comparator is what the option has to resolve against.
std::vector<SyntheticRow> chordRows(bool withFretboardStyle)
{
    std::vector<SyntheticRow> rows{
        {GLOBALS_CMPER, "41", {0, 0, 0, 0, 0, 0}},
        {GLOBALS_CMPER, "41", {storedFretStyleId, 0, 0, 0, 0, 0}},
    };
    if (withFretboardStyle) {
        for (int inci = 0; inci < 26; ++inci) {
            rows.push_back({storedFretStyleId, "ft", {}});
        }
    }
    return rows;
}

/// @brief Runs the two importers in the given order, then drains the deferred checks.
/// @details The point of the test: the registry may list these in either order, so the result
/// must not depend on which of them runs first.
musx::dom::Cmper importedFretStyleId(bool chordOptionsFirst, bool withFretboardStyle)
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<ChordOptions>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All);
    document->getOptions()->add(ChordOptions::XmlNodeName, options);

    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = chordReferenceDocument(referenceSession);

    const auto parsed = makeContainer(chordRows(withFretboardStyle), FormatEpoch::DclLegacy);
    SourceProfile profile(FormatEpoch::DclLegacy);
    profile.byteOrder = parsed.byteOrder;
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};

    if (chordOptionsFirst) {
        finale_mus_reader::options::importChordOptions(context);
        finale_mus_reader::others::importFretboardStyles(context);
    } else {
        finale_mus_reader::others::importFretboardStyles(context);
        finale_mus_reader::options::importChordOptions(context);
    }
    finale_mus_reader::runDeferredChecks(pending);
    return document->getOptions()->get<ChordOptions>()->fretStyleId;
}

TEST_CASE("Chord options resolve their fret references whatever order the registry runs")
{
    // The definition exists, so the source's own comparator survives either way. Consulting the
    // fretboard pool inline instead would make the first ordering yield the pinned default.
    expect(importedFretStyleId(/*chordOptionsFirst*/ false, /*withFretboardStyle*/ true) == storedFretStyleId,
        "A recovered fret style comparator was lost when its definitions imported first");
    expect(importedFretStyleId(/*chordOptionsFirst*/ true, /*withFretboardStyle*/ true) == storedFretStyleId,
        "A recovered fret style comparator was lost when chord options imported first");

    // No definition of that comparator: the pinned default replaces it, again either way.
    expect(importedFretStyleId(/*chordOptionsFirst*/ false, /*withFretboardStyle*/ false) == referenceFretStyleId,
        "A fret style comparator naming no definition did not take the pinned default");
    expect(importedFretStyleId(/*chordOptionsFirst*/ true, /*withFretboardStyle*/ false) == referenceFretStyleId,
        "A fret style comparator naming no definition did not take the pinned default when chord"
        " options imported first");
}

TEST_CASE("Chord accidental lifts recover from Finale 3.7 onward")
{
    const auto import = [](std::uint8_t minor) {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        auto options = std::make_shared<ChordOptions>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All);
        document->getOptions()->add(ChordOptions::XmlNodeName, options);

        auto referenceSession = musx::factory::DocumentFactory::begin();
        const auto reference = chordReferenceDocument(referenceSession);
        const auto parsed = makeContainer(
            {
                {GLOBALS_CMPER, "37", {0, 0, 0, 24, 25, 26}},
                {GLOBALS_CMPER, "41", {0, 0, 0, 0, 0, 0}},
            },
            FormatEpoch::UncompressedLegacy);
        auto profile = profileFor(3, minor);
        ImportReport report(profile.epoch);
        const auto index = LegacyRecordIndex::build(parsed);
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::options::importChordOptions(context);
        return std::pair{document->getOptions()->get<ChordOptions>(), std::move(report)};
    };

    const auto [finale35, finale35Report] = import(5);
    CHECK(finale35->chordSharpLift == 12);
    CHECK(finale35->chordFlatLift == 12);
    CHECK(finale35->chordNaturalLift == 12);
    CHECK(field(finale35Report, "options.chordOptions.chordSharpLift").origin == ValueOrigin::LegacyBehavior);

    const auto [finale37, finale37Report] = import(7);
    CHECK(finale37->chordSharpLift == 24);
    CHECK(finale37->chordFlatLift == 25);
    CHECK(finale37->chordNaturalLift == 26);
    CHECK(field(finale37Report, "options.chordOptions.chordSharpLift").origin == ValueOrigin::LegacyMus);
}

TEST_CASE("Finale 2.6.3 recovers the unambiguous fretboard display states", "[class][reader]")
{
    for (const auto& [fixture, expected] : {
             std::pair{"evidence/F263/F263-baseline.mus", false},
             std::pair{"evidence/F263/F263-fretboards.mus", true},
             std::pair{"evidence/F263/F263-fretboards-cleared.mus", false},
         }) {
        const auto result = readFixture(fixture);
        const auto options = result.document->getOptions()->get<ChordOptions>();
        REQUIRE(options);
        INFO(fixture);
        CHECK(options->showFretboards == expected);
        CHECK(field(result, "options.chordOptions.showFretboards").origin == ValueOrigin::LegacyMus);
    }

    for (const auto mixedState : {0x0004, 0x0008}) {
        auto session = musx::factory::DocumentFactory::begin();
        const auto document = session.getDocument();
        auto options = std::make_shared<ChordOptions>(document);
        options->showFretboards = true;
        document->getOptions()->add(ChordOptions::XmlNodeName, options);

        auto referenceSession = musx::factory::DocumentFactory::begin();
        const auto reference = chordReferenceDocument(referenceSession);
        const auto parsed = makeContainer({{GLOBALS_CMPER, "41", {0, 0, static_cast<std::int16_t>(mixedState), 0, 0, 0}}}, FormatEpoch::CodaBanner);
        SourceProfile profile(FormatEpoch::CodaBanner);
        profile.byteOrder = parsed.byteOrder;
        ImportReport report(profile.epoch);
        const auto index = LegacyRecordIndex::build(parsed);
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};

        finale_mus_reader::options::importChordOptions(context);
        INFO(mixedState);
        CHECK(options->showFretboards);
    }
}
} // namespace
} // namespace finale_mus_reader_tests
