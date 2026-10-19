// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

musx::dom::DocumentPtr makeSmartShapeOptionsDocument()
{
    using SmartShape = musx::dom::options::SmartShapeOptions;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<SmartShape>(document);
    options->shortHairpinOpeningWidth = 901;
    options->crescHeight = 902;
    options->maximumShortHairpinLength = 927;
    options->crescLineWidth = 903;
    options->hookLength = 904;
    options->smartLineWidth = 905;
    options->showOctavaAsText = false;
    options->smartDashOn = 906;
    options->smartDashOff = 907;
    options->crescHorizontal = false;
    options->slurThicknessCp1X = 908;
    options->slurThicknessCp1Y = 909;
    options->slurThicknessCp2X = 910;
    options->slurThicknessCp2Y = 911;
    options->slurAvoidAccidentals = false;
    options->slurAvoidStaffLinesAmt = 912;
    options->maxSlurStretch = 913;
    options->maxSlurLift = 914;
    options->slurSymmetry = 915;
    options->useEngraverSlurs = false;
    options->slurLeftBreakHorzAdj = 916;
    options->slurRightBreakHorzAdj = 917;
    options->slurBreakVertAdj = 918;
    options->slurAvoidStaffLines = false;
    options->slurPadding = 919;
    options->maxSlurAngle = 919;
    options->slurAcciPadding = 920;
    options->slurDoStretchFirst = true;
    options->slurStretchByPercent = false;
    options->maxSlurStretchPercent = 921;
    options->articAvoidSlurAmt = 928;
    options->ssLineStyleCmpCustom = 922;
    options->ssLineStyleCmpGlissando = 923;
    options->ssLineStyleCmpTabSlide = 924;
    options->ssLineStyleCmpTabBendCurve = 925;
    options->smartSlurTipWidth = 9.26;
    options->guitarBendUseParens = false;
    options->guitarBendHideBendTo = true;
    options->guitarBendGenText = false;
    options->guitarBendUseFull = true;
    constexpr std::array controlTypes{
        SmartShape::SlurControlStyleType::ShortSpan,
        SmartShape::SlurControlStyleType::MediumSpan,
        SmartShape::SlurControlStyleType::LongSpan,
        SmartShape::SlurControlStyleType::ExtraLongSpan,
    };
    for (std::size_t index = 0; index < controlTypes.size(); ++index) {
        auto control = std::make_shared<SmartShape::ControlStyle>();
        control->span = 927 + static_cast<int>(index) * 3;
        control->inset = 928 + static_cast<int>(index) * 3;
        control->height = 929 + static_cast<int>(index) * 3;
        options->slurControlStyles.emplace(controlTypes[index], std::move(control));
    }
    const auto addConnections = []<typename Map>(Map& map, std::size_t count) {
        for (std::size_t index = 0; index < count; ++index) {
            auto connection = std::make_shared<SmartShape::ConnectionStyle>();
            connection->connectIndex = SmartShape::ConnectionIndex::HeadRightTop;
            connection->xOffset = 929 + static_cast<int>(index);
            connection->yOffset = 930 + static_cast<int>(index);
            map.emplace(static_cast<typename Map::key_type>(index), std::move(connection));
        }
    };
    addConnections(options->slurConnectStyles, static_cast<std::size_t>(SmartShape::SlurConnectStyleType::UnderTabNumEnd) + 1);
    addConnections(options->tabSlideConnectStyles, static_cast<std::size_t>(SmartShape::TabSlideConnectStyleType::SameLevelPitchSameEnd) + 1);
    addConnections(options->glissandoConnectStyles, static_cast<std::size_t>(SmartShape::GlissandoConnectStyleType::DefaultEnd) + 1);
    addConnections(options->bendCurveConnectStyles, static_cast<std::size_t>(SmartShape::BendCurveConnectStyleType::StaffFromTopEndOffset) + 1);
    document->getOptions()->add(SmartShape::XmlNodeName, options);
    return std::move(session).finish();
}

void testSmartShapeOptionsAcrossEpochs()
{
    using SmartShape = musx::dom::options::SmartShapeOptions;
    const auto connectionWords = [](std::size_t count, bool appendTerminal = false) {
        std::vector<std::int16_t> words;
        words.reserve((count + (appendTerminal ? 1 : 0)) * 3);
        for (std::size_t index = 0; index < count; ++index) {
            words.push_back(static_cast<std::int16_t>(index % 14));
            words.push_back(static_cast<std::int16_t>(1000 + index));
            words.push_back(static_cast<std::int16_t>(-1000 - index));
        }
        if (appendTerminal) {
            words.insert(words.end(), 3, 0);
        }
        return words;
    };
    const auto slurConnectionWords = connectionWords(29, true);
    const auto tabSlideConnectionWords = connectionWords(18);
    const auto glissandoConnectionWords = connectionWords(2);
    const auto bendCurveConnectionWords = connectionWords(8);

    std::vector<SyntheticRow> rows{
        {GLOBALS_CMPER, "50", {10, 11, 12, 13, 2, 9}},
        {GLOBALS_CMPER, "51", {0, 1536, 0, 2048, 8500, 1}},
        {GLOBALS_CMPER, "52", {36, 614, 16, 288, 512, 60}},
        {GLOBALS_CMPER, "52", {864, 410, 72, 1152, 369, 80}},
        {GLOBALS_CMPER, "53", {21, 22, 23, 1, 24, 4500}},
        {GLOBALS_CMPER, "53", {3, 1, 1, 1500, 0, 0}},
        {GLOBALS_CMPER, "92", {4, 5, 6, 7, 0, 0}},
        {GLOBALS_CMPER, "93", {0, 10000, 0, 0, 0, 0}},
        {GLOBALS_CMPER, "97", {0, 0, 0, 0, 0, 0}},
        {GLOBALS_CMPER, "97", {1, 0, 1, 0, 0, 0}},
        {11, "FI", {40, 224, 12, 0, 225, 1}},
        {12, "FI", {0, 18, 0, 19, 1, 0}},
    };
    const auto appendFixedFamily = [](std::vector<SyntheticRow>& destination, const char* tag, const std::vector<std::int16_t>& words) {
        expectMapping(words.size() % 6 == 0, "A synthetic fixed-row family did not end on an incidence boundary");
        for (std::size_t first = 0; first < words.size(); first += 6) {
            std::array<std::int16_t, 6> incidence{};
            std::ranges::copy(
                words.begin() + static_cast<std::ptrdiff_t>(first), words.begin() + static_cast<std::ptrdiff_t>(first + 6), incidence.begin());
            destination.push_back({GLOBALS_CMPER, tag, incidence});
        }
    };
    appendFixedFamily(rows, "26", slurConnectionWords);
    appendFixedFamily(rows, "90", tabSlideConnectionWords);
    appendFixedFamily(rows, "91", glissandoConnectionWords);
    appendFixedFamily(rows, "98", bendCurveConnectionWords);
    const auto runImport = [](const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile, ImportReport& report) {
        const auto document = makeSmartShapeOptionsDocument();
        const auto reference = makeSmartShapeOptionsDocument();
        const auto index = LegacyRecordIndex::build(parsed);
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::options::importSmartShapeOptions(context);
        return document->getOptions()->get<SmartShape>();
    };
    const auto expectRecovered = [](const auto& options, const ImportReport& report, const std::string& epoch) {
        expectMapping(options->crescHeight == 40 && options->shortHairpinOpeningWidth == 40 && options->crescLineWidth == 224
                          && options->hookLength == 12 && options->smartLineWidth == 225 && options->showOctavaAsText && options->smartDashOn == 18
                          && options->smartDashOff == 19 && options->crescHorizontal,
            epoch + " did not recover the Smart Shape line settings");
        expectMapping(options->slurThicknessCp1X == 10 && options->slurThicknessCp1Y == 11 && options->slurThicknessCp2X == 12
                          && options->slurThicknessCp2Y == 13 && options->slurAvoidAccidentals && options->slurAvoidStaffLinesAmt == 8,
            epoch + " did not recover the Smart Shape slur thickness settings");
        expectMapping(options->maxSlurStretch == 1536 && options->maxSlurLift == 2048 && options->slurSymmetry == 8500 && options->useEngraverSlurs,
            epoch + " did not recover the engraver-slur settings");
        expectMapping(options->slurLeftBreakHorzAdj == 21 && options->slurRightBreakHorzAdj == 22 && options->slurBreakVertAdj == 23
                          && options->slurAvoidStaffLines && options->slurPadding == 24 && options->maxSlurAngle == 4500
                          && options->slurAcciPadding == 3 && options->slurDoStretchFirst && options->slurStretchByPercent
                          && options->maxSlurStretchPercent == 1500,
            epoch + " did not recover the slur adjustment settings");
        expectMapping(options->ssLineStyleCmpCustom == 4 && options->ssLineStyleCmpGlissando == 5 && options->ssLineStyleCmpTabSlide == 6
                          && options->ssLineStyleCmpTabBendCurve == 7 && options->smartSlurTipWidth == 1.0,
            epoch + " did not recover the Smart Shape line references");
        expectMapping(options->guitarBendUseParens && !options->guitarBendHideBendTo && options->guitarBendGenText && !options->guitarBendUseFull,
            epoch + " did not recover the guitar-bend settings");
        expectMapping(options->slurControlStyles.size() == 4, epoch + " did not replace the seeded slur contours");
        const auto extraLong = options->slurControlStyles.at(SmartShape::SlurControlStyleType::ExtraLongSpan);
        expectMapping(
            extraLong->span == 1152 && extraLong->inset == 369 && extraLong->height == 80, epoch + " did not recover the extra-long slur contour");
        const auto lastSlur = options->slurConnectStyles.at(SmartShape::SlurConnectStyleType::UnderTabNumEnd);
        const auto lastTabSlide = options->tabSlideConnectStyles.at(SmartShape::TabSlideConnectStyleType::SameLevelPitchSameEnd);
        const auto glissandoEnd = options->glissandoConnectStyles.at(SmartShape::GlissandoConnectStyleType::DefaultEnd);
        const auto lastBend = options->bendCurveConnectStyles.at(SmartShape::BendCurveConnectStyleType::StaffFromTopEndOffset);
        expectMapping(options->slurConnectStyles.size() == 29 && lastSlur->connectIndex == SmartShape::ConnectionIndex::HeadLeftTop
                          && lastSlur->xOffset == 1028 && lastSlur->yOffset == -1028 && options->tabSlideConnectStyles.size() == 18
                          && lastTabSlide->connectIndex == SmartShape::ConnectionIndex::HeadLeftBottom && lastTabSlide->xOffset == 1017
                          && options->glissandoConnectStyles.size() == 2 && glissandoEnd->connectIndex == SmartShape::ConnectionIndex::HeadRightTop
                          && options->bendCurveConnectStyles.size() == 8 && lastBend->connectIndex == SmartShape::ConnectionIndex::StemLeftBottom,
            epoch + " did not recover every Smart Shape connection style");
        expectMapping(field(report, "options.smartShapeOptions.crescHeight").origin == ValueOrigin::LegacyMus
                          && field(report, "options.smartShapeOptions.shortHairpinOpeningWidth").origin == ValueOrigin::LegacyBehavior
                          && field(report, "options.smartShapeOptions.slurControlStyles[3].height").rawValue == 80,
            epoch + " reported incorrect Smart Shape origins");
        expectMapping(field(report, "options.smartShapeOptions.maximumShortHairpinLength").origin == ValueOrigin::MusxOnly
                          && field(report, "options.smartShapeOptions.articAvoidSlurAmt").origin == ValueOrigin::MusxOnly,
            epoch + " reported incorrect unresolved Smart Shape scalar origins");
        expectMapping(field(report, "options.smartShapeOptions.slurConnectStyles[28].xOffset").origin == ValueOrigin::LegacyMus
                          && field(report, "options.smartShapeOptions.tabSlideConnectStyles[17].yOffset").origin == ValueOrigin::LegacyMus
                          && field(report, "options.smartShapeOptions.glissandoConnectStyles[1].connectIndex").origin == ValueOrigin::LegacyMus
                          && field(report, "options.smartShapeOptions.bendCurveConnectStyles[7].xOffset").origin == ValueOrigin::LegacyMus,
            epoch + " reported incorrect Smart Shape connection-style origins");
    };

    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        auto profile = profileFor(epoch == FormatEpoch::DclLegacy ? 12 : 4, 0);
        profile.epoch = epoch;
        ImportReport report(FormatEpoch::UncompressedLegacy);
        expectRecovered(runImport(makeContainer(rows, epoch), profile, report), report,
            epoch == FormatEpoch::DclLegacy ? "The DCL epoch" : "The uncompressed epoch");
    }

    struct FigureSemanticsCase
    {
        FormatEpoch epoch;
        std::optional<SourceVersion> version;
        int expectedCrescLineWidth;
        ValueOrigin crescLineWidthOrigin;
        std::int64_t crescLineWidthRawValue;
        int expectedHookLength;
        ValueOrigin hookLengthOrigin;
    };
    const std::array figureCases{
        FigureSemanticsCase{
            FormatEpoch::UncompressedLegacy, profileFor(3, 6).version, 225, ValueOrigin::LegacyBehavior, 225, 904, ValueOrigin::Finale27Default},
        FigureSemanticsCase{FormatEpoch::UncompressedLegacy, profileFor(3, 7).version, 224, ValueOrigin::LegacyMus, 224, 12, ValueOrigin::LegacyMus},
        FigureSemanticsCase{FormatEpoch::UncompressedLegacy, std::nullopt, 224, ValueOrigin::LegacyMus, 224, 12, ValueOrigin::LegacyMus},
        FigureSemanticsCase{FormatEpoch::DclLegacy, profileFor(3, 6).version, 224, ValueOrigin::LegacyMus, 224, 12, ValueOrigin::LegacyMus},
    };
    for (const auto& test : figureCases) {
        auto profile = profileFor(4, 0);
        profile.epoch = test.epoch;
        profile.version = test.version;
        ImportReport report(FormatEpoch::UncompressedLegacy);
        const auto options = runImport(makeContainer(rows, test.epoch), profile, report);
        const auto& recoveredWidth = field(report, "options.smartShapeOptions.crescLineWidth");
        const auto& recoveredHook = field(report, "options.smartShapeOptions.hookLength");
        expectMapping(options->crescLineWidth == test.expectedCrescLineWidth && recoveredWidth.origin == test.crescLineWidthOrigin
                          && recoveredWidth.rawValue == test.crescLineWidthRawValue && options->hookLength == test.expectedHookLength
                          && recoveredHook.origin == test.hookLengthOrigin,
            "The pre-Finale-3.7 figure gate crossed an epoch or version boundary");
    }

    auto zeroAvoidanceRows = rows;
    zeroAvoidanceRows.front().words[5] = 0;
    auto zeroAvoidanceProfile = profileFor(7, 0);
    zeroAvoidanceProfile.epoch = FormatEpoch::DclLegacy;
    ImportReport zeroAvoidanceReport(FormatEpoch::UncompressedLegacy);
    const auto zeroAvoidanceOptions = runImport(makeContainer(zeroAvoidanceRows, FormatEpoch::DclLegacy), zeroAvoidanceProfile, zeroAvoidanceReport);
    expectMapping(zeroAvoidanceOptions->slurAvoidStaffLinesAmt == 912
                      && field(zeroAvoidanceReport, "options.smartShapeOptions.slurAvoidStaffLinesAmt").origin == ValueOrigin::Finale27Default
                      && field(zeroAvoidanceReport, "options.smartShapeOptions.slurAvoidStaffLinesAmt").rawValue == 912,
        "A zero stored staff-line avoidance amount replaced the seeded default");

    auto singleAdjustmentRows = rows;
    singleAdjustmentRows.erase(singleAdjustmentRows.begin() + 5);
    auto singleAdjustmentProfile = profileFor(7, 0);
    singleAdjustmentProfile.epoch = FormatEpoch::DclLegacy;
    ImportReport singleAdjustmentReport(FormatEpoch::UncompressedLegacy);
    const auto singleAdjustmentOptions =
        runImport(makeContainer(singleAdjustmentRows, FormatEpoch::DclLegacy), singleAdjustmentProfile, singleAdjustmentReport);
    expectMapping(
        singleAdjustmentOptions->slurPadding == 24 && singleAdjustmentOptions->slurAcciPadding == 24 && !singleAdjustmentOptions->slurDoStretchFirst,
        "A single-incidence enhanced slur layout did not apply its shared padding behavior");
    expectMapping(field(singleAdjustmentReport, "options.smartShapeOptions.slurAcciPadding").origin == ValueOrigin::LegacyBehavior
                      && field(singleAdjustmentReport, "options.smartShapeOptions.slurAcciPadding").rawValue == 24
                      && field(singleAdjustmentReport, "options.smartShapeOptions.slurDoStretchFirst").origin == ValueOrigin::LegacyBehavior,
        "A single-incidence enhanced slur layout reported incorrect behavior origins");

    std::vector<SyntheticClassRow> classRows{
        {finale_mus_reader::numericGlobalClass(50), {10, 11, 12, 13, 2, 9}},
        {finale_mus_reader::numericGlobalClass(51), {0, 1536, 0, 2048, 8500, 1}},
        {finale_mus_reader::numericGlobalClass(52), {36, 614, 16, 288, 512, 60, 864, 410, 72, 1152, 369, 80}},
        {finale_mus_reader::numericGlobalClass(53), {21, 22, 23, 1, 24, 4500, 3, 1, 1, 1500, 0, 0}},
        {finale_mus_reader::numericGlobalClass(92), {4, 5, 6, 7, 0, 0}},
        {finale_mus_reader::numericGlobalClass(93), {0, 10000, 0, 0, 0, 0}},
        {finale_mus_reader::numericGlobalClass(97), {0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0}},
        {0x008d, {40, 224, 12, 0, 225, 1}, 11},
        {0x008d, {0, 18, 0, 19, 1, 0}, 12},
        {0x0028, slurConnectionWords},
        {0x0068, tabSlideConnectionWords},
        {0x0069, glissandoConnectionWords},
        {0x0070, bendCurveConnectionWords},
    };
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        auto profile = profileFor(16, 0);
        profile.epoch = FormatEpoch::ZlibLegacy;
        profile.byteOrder = byteOrder;
        ImportReport report(FormatEpoch::UncompressedLegacy);
        expectRecovered(runImport(makeClassContainer(classRows, byteOrder), profile, report), report,
            byteOrder == ByteOrder::BigEndian ? "The big-endian zlib epoch" : "The little-endian zlib epoch");
    }

    auto coda = profileFor(2, 6);
    coda.epoch = FormatEpoch::CodaBanner;
    coda.version.reset();
    ImportReport codaReport(FormatEpoch::UncompressedLegacy);
    const std::vector<SyntheticRow> codaRows{
        {GLOBALS_CMPER, "51", {-13, 17, -15, 19, 3, 5}},
        {GLOBALS_CMPER, "26", {13, 31, -32, 0, 33, -34}},
    };
    const auto codaOptions = runImport(makeContainer(codaRows, FormatEpoch::CodaBanner), coda, codaReport);
    expectMapping(codaOptions->crescHeight == 902 && codaOptions->shortHairpinOpeningWidth == 902 && codaOptions->slurThicknessCp1X == -13
                      && codaOptions->slurThicknessCp1Y == -17 && codaOptions->slurThicknessCp2X == -15 && codaOptions->slurThicknessCp2Y == -19
                      && codaOptions->slurControlStyles.size() == 4 && codaOptions->slurConnectStyles.size() == 29
                      && codaOptions->slurConnectStyles.at(SmartShape::SlurConnectStyleType::OverNoteStart)->connectIndex
                             == SmartShape::ConnectionIndex::NoteRightCenter
                      && codaOptions->slurConnectStyles.at(SmartShape::SlurConnectStyleType::OverNoteStart)->xOffset == 31
                      && codaOptions->slurConnectStyles.at(SmartShape::SlurConnectStyleType::OverNoteEnd)->yOffset == -34,
        "The Coda epoch did not recover its SmartShapeOptions control-point pairs");
    expectMapping(field(codaReport, "options.smartShapeOptions.crescHeight").origin == ValueOrigin::Finale27Default
                      && field(codaReport, "options.smartShapeOptions.slurThicknessCp1Y").origin == ValueOrigin::LegacyMus
                      && field(codaReport, "options.smartShapeOptions.slurThicknessCp2Y").rawValue == 19
                      && field(codaReport, "options.smartShapeOptions.shortHairpinOpeningWidth").origin == ValueOrigin::LegacyBehavior
                      && field(codaReport, "options.smartShapeOptions.slurConnectStyles[1].yOffset").origin == ValueOrigin::LegacyMus
                      && field(codaReport, "options.smartShapeOptions.slurConnectStyles[2].yOffset").origin == ValueOrigin::Finale27Default,
        "The Coda epoch reported incorrect SmartShapeOptions origins");

    ImportReport codaDefaultReport(FormatEpoch::UncompressedLegacy);
    const std::vector<SyntheticRow> codaDefaultRows{
        {GLOBALS_CMPER, "51", {0, -6, 0, -6, 0, 8}},
    };
    const auto codaDefaults = runImport(makeContainer(codaDefaultRows, FormatEpoch::CodaBanner), coda, codaDefaultReport);
    expectMapping(codaDefaults->slurThicknessCp1Y == 6 && codaDefaults->slurThicknessCp2Y == 6 && codaDefaults->slurThicknessCp1X == 0
                      && codaDefaults->slurThicknessCp2X == 0,
        "The Coda defaults did not recover both thickness-control pairs");

    auto earlyRows = rows;
    earlyRows.erase(std::remove_if(earlyRows.begin(), earlyRows.end(),
                        [](const auto& row) { return std::string_view(row.tag) == "97" || std::string_view(row.tag) == "FI"; }),
        earlyRows.end());
    earlyRows.erase(earlyRows.begin() + 3);
    auto earlyProfile = profileFor(3, 7);
    earlyProfile.epoch = FormatEpoch::UncompressedLegacy;
    ImportReport earlyReport(FormatEpoch::UncompressedLegacy);
    const auto earlyOptions = runImport(makeContainer(earlyRows), earlyProfile, earlyReport);
    expectMapping(earlyOptions->slurThicknessCp1X == 908 && earlyOptions->slurControlStyles.size() == 4,
        "An older six-word slur layout was interpreted as SmartShapeOptions");

    const std::vector<SyntheticRow> preEngraverRows{
        {GLOBALS_CMPER, "59", {118, 6, 6, 8, 8, 17}},
        {GLOBALS_CMPER, "52", {36, 532, 13, 288, 553, 43}},
        {GLOBALS_CMPER, "52", {864, 358, 73, 0, 0, 0}},
        {GLOBALS_CMPER, "53", {3, 5, 7, 0, 0, 0}},
    };
    ImportReport preEngraverReport(FormatEpoch::UncompressedLegacy);
    const auto preEngraverOptions = runImport(makeContainer(preEngraverRows), earlyProfile, preEngraverReport);
    const auto longStyle = preEngraverOptions->slurControlStyles.at(SmartShape::SlurControlStyleType::LongSpan);
    const auto extraLongStyle = preEngraverOptions->slurControlStyles.at(SmartShape::SlurControlStyleType::ExtraLongSpan);
    expectMapping(
        longStyle->span == 864 && longStyle->inset == 358 && longStyle->height == 73 && extraLongStyle->span == 936 && extraLongStyle->inset == 358
            && extraLongStyle->height == 73 && preEngraverOptions->slurLeftBreakHorzAdj == 3 && preEngraverOptions->slurRightBreakHorzAdj == 5
            && preEngraverOptions->slurBreakVertAdj == 7 && preEngraverOptions->slurThicknessCp1Y == 17 && preEngraverOptions->slurThicknessCp2Y == 17
            && !preEngraverOptions->slurAvoidStaffLines && preEngraverOptions->slurAcciPadding == 920 && preEngraverOptions->slurDoStretchFirst,
        "A pre-Engraver-Slur file did not preserve its three contours and seeded extra-long span");
    expectMapping(field(preEngraverReport, "options.smartShapeOptions.slurControlStyles[2].height").origin == ValueOrigin::LegacyMus
                      && field(preEngraverReport, "options.smartShapeOptions.slurControlStyles[3].height").origin == ValueOrigin::LegacyBehavior
                      && field(preEngraverReport, "options.smartShapeOptions.slurControlStyles[3].height").rawValue == 73
                      && field(preEngraverReport, "options.smartShapeOptions.slurThicknessCp1Y").origin == ValueOrigin::LegacyMus
                      && field(preEngraverReport, "options.smartShapeOptions.slurThicknessCp2Y").rawValue == 17
                      && field(preEngraverReport, "options.smartShapeOptions.slurAvoidStaffLines").origin == ValueOrigin::Finale27Default,
        "A pre-Engraver-Slur file reported incorrect contour origins");
}

void testSmartShapeCustomLineFallbackGate()
{
    const auto queuedReferences = [](FormatEpoch epoch, std::uint8_t major) {
        const auto parsed = makeContainer(std::vector<SyntheticRow>{}, epoch);
        const auto index = LegacyRecordIndex::build(parsed);
        auto profile = profileFor(major);
        profile.epoch = epoch;
        const auto document = makeSmartShapeOptionsDocument();
        const auto reference = makeSmartShapeOptionsDocument();
        ImportReport report(FormatEpoch::UncompressedLegacy);
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::options::importSmartShapeOptions(context);
        std::vector<musx::dom::Cmper> result;
        for (const auto& request : pending.customLines) {
            result.push_back(request.referenceLineId);
        }
        return result;
    };

    expectMapping(queuedReferences(FormatEpoch::CodaBanner, 99) == std::vector<musx::dom::Cmper>{923, 924, 925},
        "The Coda epoch did not request baseline lines independently of its version");
    expectMapping(queuedReferences(FormatEpoch::UncompressedLegacy, 4) == std::vector<musx::dom::Cmper>{923, 924, 925},
        "A pre-major-5 uncompressed file did not request baseline lines in tool order");
    expectMapping(queuedReferences(FormatEpoch::UncompressedLegacy, 5) == std::vector<musx::dom::Cmper>{925},
        "A Finale 2000 profile did not request only the unavailable bend curve");
    expectMapping(queuedReferences(FormatEpoch::DclLegacy, 7) == std::vector<musx::dom::Cmper>{925},
        "A pre-Finale-2003 DCL profile did not request the bend curve");
    expectMapping(queuedReferences(FormatEpoch::DclLegacy, 8).empty(), "A Finale 2003 DCL profile requested a baseline bend curve");
    expectMapping(queuedReferences(FormatEpoch::ZlibLegacy, 7).empty(), "A zlib file was accepted by the DCL bend-curve version gate");
}

void testSmartShapeDirectionGate()
{
    using Direction = musx::dom::ShapeDirection;
    using SmartShape = musx::dom::options::SmartShapeOptions;
    const auto importFixed = [](FormatEpoch epoch, std::uint8_t major, bool keepVersion, std::int16_t raw, ImportReport& report) {
        const std::vector<SyntheticRow> rows{
            {GLOBALS_CMPER, "10", {raw, 75, 239, 229, 0, 0}},
        };
        const auto parsed = makeContainer(rows, epoch);
        const auto index = LegacyRecordIndex::build(parsed);
        auto profile = profileFor(major);
        profile.epoch = epoch;
        if (!keepVersion) {
            profile.version.reset();
        }
        const auto document = makeSmartShapeOptionsDocument();
        const auto reference = makeSmartShapeOptionsDocument();
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::options::importSmartShapeOptions(context);
        return document->getOptions()->get<SmartShape>()->direction;
    };

    ImportReport finale2001Report(FormatEpoch::UncompressedLegacy);
    expectMapping(importFixed(FormatEpoch::DclLegacy, 6, true, -1, finale2001Report) == Direction::Automatic
                      && field(finale2001Report, "options.smartShapeOptions.direction").origin == ValueOrigin::Finale27Default,
        "A pre-Finale-2002 DCL word replaced the seeded Automatic direction");

    ImportReport finale2002Report(FormatEpoch::UncompressedLegacy);
    expectMapping(importFixed(FormatEpoch::DclLegacy, 7, true, -1, finale2002Report) == Direction::Under
                      && field(finale2002Report, "options.smartShapeOptions.direction").origin == ValueOrigin::LegacyMus
                      && field(finale2002Report, "options.smartShapeOptions.direction").rawValue == -1,
        "Finale 2002 did not recover its default slur direction");

    ImportReport finale2006Report(FormatEpoch::UncompressedLegacy);
    expectMapping(importFixed(FormatEpoch::DclLegacy, 11, true, 1, finale2006Report) == Direction::Over,
        "The Finale 2002 direction range ended before the DCL epoch");

    for (const auto invalid : std::array<std::int16_t, 2>{-2, 103}) {
        ImportReport report(FormatEpoch::UncompressedLegacy);
        expectMapping(importFixed(FormatEpoch::DclLegacy, 11, true, invalid, report) == Direction::Automatic
                          && field(report, "options.smartShapeOptions.direction").origin == ValueOrigin::Finale27Default,
            "An invalid DCL direction replaced the seeded Automatic direction");
    }

    for (const auto& test : std::array{std::pair{FormatEpoch::UncompressedLegacy, true}, std::pair{FormatEpoch::DclLegacy, false}}) {
        ImportReport report(FormatEpoch::UncompressedLegacy);
        expectMapping(importFixed(test.first, 7, test.second, -1, report) == Direction::Automatic,
            "The direction gate crossed an epoch or accepted a missing version");
    }

    const auto importClass = [](std::int16_t raw, ImportReport& report) {
        const std::vector<SyntheticClassRow> classRows{
            {finale_mus_reader::numericGlobalClass(10), {raw, 75, 239, 229, 0, 0}},
        };
        auto profile = profileFor(12);
        profile.epoch = FormatEpoch::ZlibLegacy;
        const auto parsed = makeClassContainer(classRows, ByteOrder::BigEndian);
        const auto index = LegacyRecordIndex::build(parsed);
        const auto document = makeSmartShapeOptionsDocument();
        const auto reference = makeSmartShapeOptionsDocument();
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::options::importSmartShapeOptions(context);
        return document->getOptions()->get<SmartShape>()->direction;
    };

    ImportReport zlibReport(FormatEpoch::UncompressedLegacy);
    expectMapping(
        importClass(1, zlibReport) == Direction::Over && field(zlibReport, "options.smartShapeOptions.direction").origin == ValueOrigin::LegacyMus,
        "The zlib direction class did not recover the stored direction");

    ImportReport invalidZlibReport(FormatEpoch::UncompressedLegacy);
    expectMapping(importClass(103, invalidZlibReport) == Direction::Automatic
                      && field(invalidZlibReport, "options.smartShapeOptions.direction").origin == ValueOrigin::Finale27Default,
        "An invalid zlib direction replaced the seeded Automatic direction");
}

void testSmartShapeHookLengthBehaviorGate()
{
    using SmartShape = musx::dom::options::SmartShapeOptions;
    const auto importHookLength = [](FormatEpoch epoch, std::uint8_t major, std::uint8_t minor, bool keepVersion, ImportReport& report) {
        const auto parsed = makeContainer(std::vector<SyntheticRow>{}, epoch);
        const auto index = LegacyRecordIndex::build(parsed);
        auto profile = profileFor(major, minor);
        profile.epoch = epoch;
        if (!keepVersion) {
            profile.version.reset();
        }
        const auto document = makeSmartShapeOptionsDocument();
        const auto reference = makeSmartShapeOptionsDocument();
        finale_mus_reader::PendingReferences pending;
        musx::factory::ConstructionContext construction;
        const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
        finale_mus_reader::options::importSmartShapeOptions(context);
        return document->getOptions()->get<SmartShape>()->hookLength;
    };

    ImportReport finale26Report(FormatEpoch::UncompressedLegacy);
    expectMapping(importHookLength(FormatEpoch::CodaBanner, 2, 6, true, finale26Report) == 8
                      && field(finale26Report, "options.smartShapeOptions.hookLength").origin == ValueOrigin::LegacyBehavior,
        "Finale 2.6 did not receive its fixed hook length as legacy behavior");

    struct ProfileCase
    {
        FormatEpoch epoch;
        std::uint8_t major;
        std::uint8_t minor;
        bool keepVersion;
    };
    for (const auto& profile : std::array{ProfileCase{FormatEpoch::CodaBanner, 1, 0, true}, ProfileCase{FormatEpoch::CodaBanner, 2, 6, false},
             ProfileCase{FormatEpoch::UncompressedLegacy, 2, 6, true}}) {
        ImportReport report(FormatEpoch::UncompressedLegacy);
        expectMapping(importHookLength(profile.epoch, profile.major, profile.minor, profile.keepVersion, report) == 904,
            "The Finale 2.6 hook-length behavior escaped its epoch and version gate");
    }
}

TEST_CASE("Smart Shape options span the located epochs", "[class]")
{
    testSmartShapeOptionsAcrossEpochs();
}
TEST_CASE("Smart Shape custom-line fallback gate", "[class]")
{
    testSmartShapeCustomLineFallbackGate();
}
TEST_CASE("Smart Shape direction gate", "[class]")
{
    testSmartShapeDirectionGate();
}
TEST_CASE("Smart Shape hook-length behavior gate", "[class]")
{
    testSmartShapeHookLengthBehaviorGate();
}

} // namespace
} // namespace finale_mus_reader_tests
