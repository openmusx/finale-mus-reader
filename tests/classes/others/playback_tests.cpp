// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT
#include "class_test_support.h"
#include "reader/resources.h"
#include <set>

namespace finale_mus_reader_tests {
namespace {
using namespace classes;
using TestPlaybackRoute = musx::dom::others::PlaybackRoute;
using TestPlaybackName = musx::dom::others::PlaybackRouteName;
using TestStaffPlayback = musx::dom::others::StaffPlayData;

ImportReport importPlaybackTest(const finale_mus_reader::container::ParsedContainer& parsed, musx::dom::DocumentPtr& document,
    std::optional<SourceVersion> version = std::nullopt, const finale_mus_reader::soundmap::Documents* soundmaps = nullptr)
{
    const auto index = LegacyRecordIndex::build(parsed);
    auto session = musx::factory::DocumentFactory::begin();
    document = session.getDocument();
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    ImportReport report(parsed.formatEpoch);
    finale_mus_reader::PendingReferences pending;
    SourceProfile profile(parsed.formatEpoch);
    profile.byteOrder = parsed.byteOrder;
    profile.version = version;
    profile.platform = finale_mus_reader::SourcePlatform::Windows;
    profile.soundmaps = soundmaps;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importPlaybackRoutes(context);
    finale_mus_reader::others::importPlaybackRouteNames(context);
    finale_mus_reader::others::importStaffPlayData(context);
    finale_mus_reader::runDeferredChecks(pending);
    return report;
}

TEST_CASE("Playback migrates Coda packed routes and two-layer staff data", "[class][playback]")
{
    for (auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        musx::dom::DocumentPtr doc;
        const auto report =
            importPlaybackTest(makeContainer({{0, "OC", {0, 1, 2, 3, 4, 5}}, {0, "MP", {20, 21, 22, 23, 24, 25}}, {7, "PL", {0, 1, 2, 3, 0, 0}}},
                                   FormatEpoch::CodaBanner, order),
                doc);
        const auto routes = doc->getOthers()->getArray<TestPlaybackRoute>(0);
        REQUIRE(routes.size() == 6);
        CHECK(routes.at(4)->virtualChannel == 4);
        CHECK(routes.at(4)->patch == 24);
        const auto staff = doc->getOthers()->get<TestStaffPlayback>(0, 7);
        REQUIRE(staff);
        CHECK(staff->chords->playbackRouteIndex == 2);
        CHECK(staff->midiExpressions->playbackRouteIndex == 3);
        const std::array expected{0, 1, 0, 0};
        for (std::size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(staff->layers[i]);
            CHECK(staff->layers[i]->playbackRouteIndex == expected[i]);
            CHECK(staff->layers[i]->play);
            CHECK_FALSE(staff->layers[i]->solo);
            CHECK(staff->layers[i]->velocityControlLevel == 101);
            CHECK(staff->layers[i]->velocityControl->controlOn);
            CHECK(staff->layers[i]->velocityControl->function == TestStaffPlayback::VelocityControlFunction::Relative);
        }
        const auto key = finale_mus_reader::instanceKey<TestStaffPlayback>(musx::dom::Cmper(0), musx::dom::Cmper(7));
        REQUIRE(report.fields.at(key).size() == 36);
        CHECK(report.fields.at(key).at("layers[0].play").origin == ValueOrigin::LegacyBehavior);
    }
}

TEST_CASE("Playback ignores extra incidences in legacy staff records", "[class][playback]")
{
    for (auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        for (auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
            musx::dom::DocumentPtr doc;
            const auto version = epoch == FormatEpoch::CodaBanner
                                     ? std::optional<SourceVersion>{}
                                     : std::optional<SourceVersion>{epoch == FormatEpoch::DclLegacy ? SourceVersion{.major = 11, .build = 6}
                                                                                                    : SourceVersion{.major = 3, .minor = 7}};
            const auto report = importPlaybackTest(
                makeContainer({{7, "PL", {1, 2, 3, 4, 5, 6}}, {7, "PL", {0x5055, 0, 0, 0, 0, 0}}, {7, "PL", {}}, {7, "PL", {}}}, epoch, order), doc,
                version);
            const auto staff = doc->getOthers()->get<TestStaffPlayback>(0, 7);
            REQUIRE(staff);
            const bool coda = epoch == FormatEpoch::CodaBanner;
            CHECK(staff->chords->playbackRouteIndex == (coda ? 3 : 5));
            CHECK(staff->midiExpressions->playbackRouteIndex == (coda ? 4 : 6));
            const std::array expectedLayers{1, 2, coda ? 1 : 3, coda ? 1 : 4};
            for (std::size_t i = 0; i < staff->layers.size(); ++i) {
                CHECK(staff->layers[i]->playbackRouteIndex == expectedLayers[i]);
            }
            const std::array settings{staff->chords, staff->midiExpressions, staff->layers[0], staff->layers[1], staff->layers[2], staff->layers[3]};
            for (const auto& item : settings) {
                CHECK(item->play);
                CHECK_FALSE(item->solo);
                CHECK(item->velocityControlLevel == 101);
                CHECK(item->velocityControl->controlOn);
                CHECK(item->velocityControl->function == TestStaffPlayback::VelocityControlFunction::Relative);
            }
            const auto key = finale_mus_reader::instanceKey<TestStaffPlayback>(musx::dom::Cmper(0), musx::dom::Cmper(7));
            CHECK(report.fields.at(key).at("chords.velocityControlLevel").origin == ValueOrigin::LegacyBehavior);
            CHECK(report.fields.at(key).at("chords.play").origin == (coda ? ValueOrigin::LegacyBehavior : ValueOrigin::LegacyMus));
        }
    }
}

TEST_CASE("Playback staff layout changes at Finale 2006 development build 7", "[class][playback]")
{
    for (auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        for (auto build : {6, 7}) {
            musx::dom::DocumentPtr doc;
            const SourceVersion version{.major = 11, .build = static_cast<std::uint8_t>(build)};
            const auto report = importPlaybackTest(
                makeContainer({{7, "PL", {}}, {7, "PL", {}}, {7, "PL", {}}, {7, "PL", {}}}, FormatEpoch::DclLegacy, order), doc, version);
            const auto staff = doc->getOthers()->get<TestStaffPlayback>(0, 7);
            REQUIRE(staff);
            CHECK(staff->chords->velocityControlLevel == (build == 6 ? 101 : 0));
            CHECK(staff->chords->velocityControl->controlOn == (build == 6));
            CHECK(staff->chords->velocityControl->function
                  == (build == 6 ? TestStaffPlayback::VelocityControlFunction::Relative : TestStaffPlayback::VelocityControlFunction::Absolute));
            const auto key = finale_mus_reader::instanceKey<TestStaffPlayback>(musx::dom::Cmper(0), musx::dom::Cmper(7));
            CHECK(report.fields.at(key).at("chords.velocityControlLevel").origin
                  == (build == 6 ? ValueOrigin::LegacyBehavior : ValueOrigin::LegacyMus));
        }
        musx::dom::DocumentPtr docForTruncation;
        const auto shortReport = importPlaybackTest(
            makeContainer({{7, "PL", {}}, {7, "PL", {}}}, FormatEpoch::DclLegacy, order), docForTruncation, SourceVersion{.major = 11, .build = 7});
        // Classic rows load into a zeroed buffer, so a short record keeps zero for its absent words.
        const auto truncated = docForTruncation->getOthers()->get<TestStaffPlayback>(0, 7);
        REQUIRE(truncated);
        CHECK(shortReport.diagnostics.empty());
        CHECK(truncated->layers[3]->playbackRouteIndex == 0);
        const auto truncatedKey = finale_mus_reader::instanceKey<TestStaffPlayback>(musx::dom::Cmper(0), musx::dom::Cmper(7));
        CHECK(shortReport.fields.at(truncatedKey).at("chords.playbackRouteIndex").origin == ValueOrigin::LegacyMus);
        CHECK(shortReport.fields.at(truncatedKey).at("layers[3].playbackRouteIndex").origin == ValueOrigin::LegacyBehavior);
    }
}

TEST_CASE("Playback synthesizes names for assigned Coda routes without replacing stored names", "[class][playback]")
{
    for (auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        musx::dom::DocumentPtr doc;
        const auto report = importPlaybackTest(makeContainer({{7, "PL", {0, 1, 1, -1, 0, 0}}, {8, "PL", {0, 9, -1, -1, 0, 0}},
                                                                 {2, "oc", {0, 0, 0, 0, 0, 0}}, {3, "OC", {0, 0, 0, 0, 0, 0}}},
                                                   FormatEpoch::CodaBanner, order),
            doc);
        const auto names = doc->getOthers()->getArray<TestPlaybackName>(0);
        REQUIRE(names.size() == 3);
        REQUIRE(doc->getOthers()->get<TestPlaybackName>(0, 1));
        CHECK(doc->getOthers()->get<TestPlaybackName>(0, 1)->name == "Instrument 1");
        REQUIRE(doc->getOthers()->get<TestPlaybackName>(0, 2));
        CHECK(doc->getOthers()->get<TestPlaybackName>(0, 2)->name.empty());
        CHECK_FALSE(doc->getOthers()->get<TestPlaybackName>(0, 3));
        REQUIRE(doc->getOthers()->get<TestPlaybackName>(0, 10));
        CHECK(doc->getOthers()->get<TestPlaybackName>(0, 10)->name == "Instrument 10");
        CHECK_FALSE(doc->getOthers()->get<TestPlaybackName>(0, 0));
        const auto key = finale_mus_reader::instanceKey<TestPlaybackName>(musx::dom::Cmper(0), musx::dom::Cmper(1));
        CHECK(report.fields.at(key).at("name").origin == ValueOrigin::LegacyBehavior);
        REQUIRE(report.findInstanceOrigin(key));
        CHECK(*report.findInstanceOrigin(key) == ValueOrigin::LegacyBehavior);
        const auto storedKey = finale_mus_reader::instanceKey<TestPlaybackName>(musx::dom::Cmper(0), musx::dom::Cmper(2));
        CHECK(report.fields.at(storedKey).at("name").origin == ValueOrigin::LegacyMus);
    }
    for (auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        musx::dom::DocumentPtr doc;
        importPlaybackTest(makeContainer({{1, "PL", {}}, {1, "PL", {0x5055, 0, 0, 0, 0, 0}}}, epoch, ByteOrder::BigEndian), doc);
        CHECK(doc->getOthers()->getArray<TestPlaybackName>(0).empty());
    }
}

TEST_CASE("Playback retains routes for staves outside Studio View", "[class][playback]")
{
    musx::dom::DocumentPtr doc;
    importPlaybackTest(makeContainer({{1, "OC", {0, 0, 0, 0, 0, 0}}, {2, "OC", {1, 0, 0, 0, 0, 0}}, {0, "MP", {0, 0, 32, 0, 0, 0}},
                                         {2, "PL", {1, 1, 1, 1, 1, 1}}, {2, "PL", {0x5055, 0, 0, 0, 0, 0}}},
                           FormatEpoch::DclLegacy, ByteOrder::BigEndian),
        doc, SourceVersion{.major = 7});
    const auto staff = doc->getOthers()->get<TestStaffPlayback>(0, 2);
    REQUIRE(staff);
    const auto route = doc->getOthers()->get<TestPlaybackRoute>(0, staff->layers[0]->getPlaybackRouteId());
    REQUIRE(route);
    CHECK(route->virtualChannel == 1);
    CHECK(route->patch == 32);
    CHECK(doc->getOthers()->getArray<TestPlaybackRoute>(0).size() == 2);
}

TEST_CASE("Playback reads separate channel patches and packed stream flags", "[class][playback]")
{
    for (auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        for (auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            musx::dom::DocumentPtr doc;
            const auto report = importPlaybackTest(
                makeContainer({{1, "OC", {static_cast<std::int16_t>(0x8001), 0, 0, 9, 0, 0}}, {0, "MP", {0, 0, 0x0234, 0x5678, 0, 0}},
                                  {7, "PL", {10, 11, 12, 13, 14, 15}}, {7, "PL", {0x6039, 0, 0, 0, 0, 0}}},
                    epoch, order),
                doc, SourceVersion{.major = static_cast<std::uint8_t>(epoch == FormatEpoch::UncompressedLegacy ? 5 : 10)});
            const auto route = doc->getOthers()->get<TestPlaybackRoute>(0, 1);
            REQUIRE(route);
            CHECK(route->virtualChannel == 1);
            CHECK(route->patch == 0x34);
            CHECK(route->patchType == "C0_PC");
            CHECK(route->midiBankSelectLeastSignificantByte == 0x78);
            CHECK(route->midiBankSelectMostSignificantByte == 0x56);
            CHECK(route->percussionMapId == 9);
            const auto staff = doc->getOthers()->get<TestStaffPlayback>(0, 7);
            REQUIRE(staff);
            CHECK(staff->chords->playbackRouteIndex == 14);
            CHECK_FALSE(staff->chords->play);
            CHECK(staff->chords->solo);
            CHECK(staff->midiExpressions->play);
            CHECK_FALSE(staff->midiExpressions->solo);
            CHECK(staff->layers[0]->play);
            CHECK_FALSE(staff->layers[0]->solo);
            CHECK_FALSE(staff->layers[1]->play);
            CHECK(staff->layers[1]->solo);
            CHECK(staff->layers[2]->play);
            CHECK(staff->layers[2]->solo);
            CHECK_FALSE(report.fields.empty());
        }
    }
}

TEST_CASE("Playback modern layouts expose every persisted leaf and raw flags", "[class][playback]")
{
    for (auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        for (auto epoch : {FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
            const std::vector<std::int16_t> settings{
                2, -3, 0x21, 3, 3, 127, 0x10, 1, 4, 100, 0x11, 2, 5, 90, 0x20, 0, 6, 80, 0x21, 1, 7, 70, 0x10, 2};
            auto parsed = makeClassContainer({{0xb9, {31, 0x0142, 0x1234, 6, 0, 0}, 9}, {0xe6, settings, 7}}, order);
            if (epoch == FormatEpoch::DclLegacy) {
                std::vector<SyntheticRow> rows{{9, "OC", {31, 0x0142, 0x1234, 6, 0, 0}}};
                for (std::size_t i = 0; i < settings.size(); i += 6) {
                    SyntheticRow row{7, "PL", {}};
                    std::copy_n(settings.begin() + i, 6, row.words.begin());
                    rows.push_back(row);
                }
                parsed = makeContainer(rows, epoch, order);
            }
            musx::dom::DocumentPtr doc;
            const auto report = importPlaybackTest(parsed, doc, SourceVersion{.major = 11, .build = 7});
            const auto route = doc->getOthers()->get<TestPlaybackRoute>(0, 9);
            REQUIRE(route);
            CHECK(route->patchType == "C0_C32_PC");
            CHECK(route->patch == 0x42);
            const auto routeKey = finale_mus_reader::instanceKey<TestPlaybackRoute>(musx::dom::Cmper(0), musx::dom::Cmper(9));
            std::set<std::string> names;
            for (const auto& [name, info] : report.fields.at(routeKey)) {
                (void)info;
                names.insert(name);
            }
            CHECK(names
                  == std::set<std::string>{"virtualChannel", "patchType", "patch", "midiBankSelectLeastSignificantByte",
                      "midiBankSelectMostSignificantByte", "percussionMapId", "soundUuid"});
            CHECK(TestPlaybackRoute::xmlMappingArray().size() == names.size());
            const auto staff = doc->getOthers()->get<TestStaffPlayback>(0, 7);
            REQUIRE(staff);
            CHECK(staff->chords->velocityControlLevel == -3);
            CHECK(staff->chords->solo);
            CHECK(staff->layers[3]->velocityControl->function == TestStaffPlayback::VelocityControlFunction::Absolute);
            const auto key = finale_mus_reader::instanceKey<TestStaffPlayback>(musx::dom::Cmper(0), musx::dom::Cmper(7));
            std::set<std::string> expected;
            for (const auto* prefix : {"chords", "midiExpressions", "layers[0]", "layers[1]", "layers[2]", "layers[3]"}) {
                for (const auto* leaf :
                    {"playbackRouteIndex", "velocityControlLevel", "velocityControl.controlOn", "velocityControl.function", "play", "solo"}) {
                    expected.insert(std::string(prefix) + "." + leaf);
                }
            }
            names.clear();
            for (const auto& [name, info] : report.fields.at(key)) {
                names.insert(name);
                CHECK(info.origin == ValueOrigin::LegacyMus);
            }
            CHECK(names == expected);
            CHECK(report.fields.at(key).at("chords.velocityControl.function").rawValue == 0x21);
        }
    }
}

TEST_CASE("Playback names change encoding at Finale 2012 beta 60 and routes retain UUIDs", "[class][playback]")
{
    for (auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        musx::dom::DocumentPtr doc;
        const auto report = importPlaybackTest(
            makeClassContainer({{0xba, {0x00e9, static_cast<std::int16_t>(0xd834), static_cast<std::int16_t>(0xdd1e), 0, 65, 66}, 2},
                                   {0xb9, {0, 0, 0, 0, 0x0101, 0x0202, 0x0303, 0x0404, 0x0505, 0x0606, 0x0707, 0x0808}, 2}},
                order),
            doc, SourceVersion{.major = 17, .devStatus = 2, .build = 60});
        REQUIRE(doc->getOthers()->get<TestPlaybackName>(0, 2));
        CHECK(doc->getOthers()->get<TestPlaybackName>(0, 2)->name == "é𝄞");
        CHECK(doc->getOthers()->get<TestPlaybackRoute>(0, 2)->soundUuid == "01010202-0303-0404-0505-060607070808");
        CHECK_FALSE(report.fields.empty());
    }
    musx::dom::DocumentPtr doc;
    importPlaybackTest(
        makeClassContainer(0xba, {0x00e9, 65}, ByteOrder::LittleEndian, 2), doc, SourceVersion{.major = 17, .devStatus = 2, .build = 59});
    CHECK(doc->getOthers()->get<TestPlaybackName>(0, 2)->name == "é");
}

TEST_CASE("Playback observes controlled baselines across all epochs", "[class][playback]")
{
    for (const auto* path :
        {"evidence/F100/F100-baseline.mus", "evidence/F263/F263-baseline.mus", "evidence/F300/F300-baseline.mus", "evidence/F372/F372-baseline.mus",
            "evidence/F2003/F2003Win-empty.mus", "evidence/F2006/F2006-empty.mus", "evidence/F2012/F2012-baseline.mus"}) {
        INFO(path);
        const auto result = readFixture(path);
        const auto staff = result.document->getOthers()->get<TestStaffPlayback>(0, 1);
        REQUIRE(staff);
        REQUIRE(staff->layers[0]);
        CHECK(staff->layers[0]->play);
        const auto route = result.document->getOthers()->get<TestPlaybackRoute>(0, staff->layers[0]->getPlaybackRouteId());
        REQUIRE(route);
        CHECK(route->virtualChannel == 0);
        CHECK(route->patch == 0);
        if (std::string_view(path).starts_with("evidence/F100/") || std::string_view(path).starts_with("evidence/F263/")) {
            const auto name = result.document->getOthers()->get<TestPlaybackName>(0, staff->layers[0]->getPlaybackRouteId());
            REQUIRE(name);
            CHECK(name->name == "Instrument 1");
        }
    }
}

TEST_CASE("Playback part records retain their effective payload and ownership", "[class][playback]")
{
    musx::dom::DocumentPtr doc;
    std::vector<std::int16_t> settings(24);
    for (std::size_t i = 0; i < 24; i += 4) {
        settings[i] = 1;
        settings[i + 3] = 1;
    }
    const auto report =
        importPlaybackTest(makeClassContainer({{0xb9, {0, 0, 0, 0, 0, 0}, 2}, {0xb9, {15, 0x0304, 0x0506, 0, 0, 0}, 2, 5}, {0xe6, settings, 7, 5}},
                               ByteOrder::LittleEndian),
            doc);
    const auto route = doc->getOthers()->get<TestPlaybackRoute>(5, 2);
    REQUIRE(route);
    CHECK(route->getSourcePartId() == 5);
    CHECK(route->virtualChannel == 15);
    const auto staff = doc->getOthers()->get<TestStaffPlayback>(5, 7);
    REQUIRE(staff);
    CHECK(staff->getSourcePartId() == 5);
    CHECK(staff->layers[0]->getPlaybackRouteId() == 2);
    CHECK_FALSE(report.fields.empty());
}

TEST_CASE("Playback rejects short records and reports unknown or missing data", "[class][playback]")
{
    musx::dom::DocumentPtr doc;
    const auto empty = importPlaybackTest(makeContainer({}), doc);
    CHECK(empty.fields.empty());
    const auto shortReport = importPlaybackTest(makeClassContainer({{0xb9, {1}, 2}, {0xe6, {1, 2}, 7}}, ByteOrder::LittleEndian), doc);
    CHECK(doc->getOthers()->getAllSources<TestPlaybackRoute>().empty());
    CHECK(doc->getOthers()->getAllSources<TestStaffPlayback>().empty());
    CHECK(shortReport.diagnostics.size() == 2);
    const auto unknown = importPlaybackTest(makeClassContainer(0xb9, {0, 0x7f01, 0, 0, 0, 0}, ByteOrder::LittleEndian, 2), doc);
    const auto key = finale_mus_reader::instanceKey<TestPlaybackRoute>(musx::dom::Cmper(0), musx::dom::Cmper(2));
    CHECK(unknown.fields.at(key).at("patchType").origin == ValueOrigin::Unmapped);
}

TEST_CASE("Playback upgrades bank-selected names and SoftSynth sounds from supplied maps", "[class][playback][soundmaps]")
{
    finale_mus_reader::soundmap::Documents maps;
    auto addMap = [&](const std::string& xml) {
        auto document = std::make_unique<TestXmlDocument>();
        document->loadFromString(xml);
        maps.push_back(std::move(document));
    };
    addMap(R"(<Soundmap><Description><PluginID>GARA</PluginID><Name>Example sample library</Name></Description></Soundmap>)");
    addMap(R"(<Soundmap><Description><PluginID>SMSS</PluginID><Name>Example synth</Name></Description>
        <Sound uuid="11111111-1111-1111-1111-111111111111"><PatchMIDICommands><ProgramChange Number="0"/></PatchMIDICommands></Sound>
        <Sound uuid="22222222-2222-2222-2222-222222222222"><PatchMIDICommands><ControlChange Control="0" Value="99"/><ProgramChange Number="42"/></PatchMIDICommands></Sound>
        <Sound uuid="33333333-3333-3333-3333-333333333333"><PatchMIDICommands><ProgramChange Number="42"/></PatchMIDICommands></Sound>
        </Soundmap>)");
    for (auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        for (auto epoch : {FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
            const auto bank = [&](std::uint32_t id) {
                std::vector<std::int16_t> words(0x214 / 2);
                const auto put = [&](std::size_t at, std::uint32_t value) {
                    words[at] = static_cast<std::int16_t>(order == ByteOrder::BigEndian ? value >> 16 : value);
                    words[at + 1] = static_cast<std::int16_t>(order == ByteOrder::BigEndian ? value : value >> 16);
                };
                put(0, 0x210);
                put(2, id);
                return words;
            };
            std::vector<SyntheticClassRow> rows{
                {0xb9, {0, 0x012a, 0, 0, 0, 0}, 1},
                {0xba, {0x4e4e, 0}, 1},
                {0xb9, {16, 0x0100, 0, 0, 0, 0}, 2},
                {0xba, {0x4e4e, 0}, 2},
                {0x115, bank(0x534d5353), 1},
                {0x115, bank(0x47415241), 2},
            };
            auto parsed = makeClassContainer(rows, order);
            if (epoch == FormatEpoch::DclLegacy) {
                std::vector<SyntheticRow> fixed;
                for (const auto& row : rows) {
                    const auto tag = row.classId == 0xb9 ? "OC" : row.classId == 0xba ? "oc" : "Ai";
                    for (std::size_t at = 0; at < row.words.size(); at += 6) {
                        SyntheticRow item{row.cmper, tag, {}};
                        std::copy_n(row.words.begin() + at, (std::min)(std::size_t(6), row.words.size() - at), item.words.begin());
                        fixed.push_back(item);
                    }
                }
                parsed = makeContainer(fixed, epoch, order);
            }
            musx::dom::DocumentPtr doc;
            const auto report = importPlaybackTest(parsed, doc, SourceVersion{.major = 11, .build = 7}, &maps);
            CHECK(doc->getOthers()->get<TestPlaybackName>(0, 1)->name == "Example synth");
            CHECK(doc->getOthers()->get<TestPlaybackName>(0, 2)->name == "ARIA Player");
            CHECK(doc->getOthers()->get<TestPlaybackRoute>(0, 1)->soundUuid == "22222222-2222-2222-2222-222222222222");
            CHECK(doc->getOthers()->get<TestPlaybackRoute>(0, 2)->soundUuid.empty());
            CHECK(report.findField<TestPlaybackName>("name", musx::dom::Cmper(0), musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMusAdjusted);
            CHECK(
                report.findField<TestPlaybackRoute>("soundUuid", musx::dom::Cmper(0), musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMusAdjusted);

            importPlaybackTest(parsed, doc, SourceVersion{.major = 11, .build = 7});
            CHECK(doc->getOthers()->get<TestPlaybackName>(0, 1)->name == "NN");
            CHECK(doc->getOthers()->get<TestPlaybackName>(0, 2)->name == "ARIA Player");

            if (epoch == FormatEpoch::ZlibLegacy) {
                rows[0].words[1] = 0x017f;
                rows[1].words = {78, 78, 0};
                rows[3].words = {78, 78, 0};
                parsed = makeClassContainer(rows, order);
                importPlaybackTest(parsed, doc, SourceVersion{.major = 17, .devStatus = 2, .build = 111}, &maps);
                CHECK(doc->getOthers()->get<TestPlaybackRoute>(0, 1)->soundUuid == "11111111-1111-1111-1111-111111111111");
                importPlaybackTest(parsed, doc, SourceVersion{.major = 17, .devStatus = 2, .build = 112}, &maps);
                CHECK(doc->getOthers()->get<TestPlaybackName>(0, 1)->name == "NN");
                CHECK(doc->getOthers()->get<TestPlaybackName>(0, 2)->name == "NN");
                CHECK(doc->getOthers()->get<TestPlaybackRoute>(0, 1)->soundUuid.empty());

                rows[1].words = {0x4e4e, 0};
                rows[3].words = {0x4e4e, 0};
                rows[4].classId = 0x116;
                rows[5].classId = 0x116;
                parsed = makeClassContainer(rows, order);
                importPlaybackTest(parsed, doc, SourceVersion{.major = 13}, &maps);
                CHECK(doc->getOthers()->get<TestPlaybackName>(0, 1)->name == "Example synth");
                CHECK(doc->getOthers()->get<TestPlaybackName>(0, 2)->name == "NN");

                rows[4].words.resize(4);
                parsed = makeClassContainer(rows, order);
                importPlaybackTest(parsed, doc, SourceVersion{.major = 13}, &maps);
                CHECK(doc->getOthers()->get<TestPlaybackName>(0, 1)->name == "NN");
            }
        }
    }
}

TEST_CASE("Playback name upgrades can resolve a stored sound UUID without replacing it", "[class][playback][soundmaps]")
{
    finale_mus_reader::soundmap::Documents maps;
    auto xml = std::make_unique<TestXmlDocument>();
    xml->loadFromString(R"(<Soundmap><Description><PluginID>GARA</PluginID><Name>Example</Name></Description>
        <Sound uuid="11111111-1111-1111-1111-111111111111"/></Soundmap>)");
    maps.push_back(std::move(xml));
    for (auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        const std::vector<std::int16_t> route{0, 0, 0, 0, 0x1111, 0x1111, 0x1111, 0x1111, 0x1111, 0x1111, 0x1111, 0x1111};
        const auto parsed = makeClassContainer({{0xb9, route, 1}, {0xba, {78, 78, 0}, 1}, {0xb9, route, 1, 5}, {0xba, {78, 78, 0}, 1, 5},
                                                   {0xb9, {0, 0, 0, 0, 0, 0}, 2}, {0xba, {78, 78, 0}, 2}},
            order);
        musx::dom::DocumentPtr doc;
        const auto report = importPlaybackTest(parsed, doc, SourceVersion{.major = 17, .devStatus = 2, .build = 111}, &maps);
        CHECK(doc->getOthers()->get<TestPlaybackName>(0, 1)->name == "ARIA Player");
        CHECK(doc->getOthers()->get<TestPlaybackName>(5, 1)->name == "NN");
        CHECK(doc->getOthers()->get<TestPlaybackName>(0, 2)->name == "NN");
        CHECK(doc->getOthers()->get<TestPlaybackRoute>(0, 1)->soundUuid == "11111111-1111-1111-1111-111111111111");
        CHECK(report.findField<TestPlaybackRoute>("soundUuid", musx::dom::Cmper(0), musx::dom::Cmper(1))->origin == ValueOrigin::LegacyMus);
    }
}
} // namespace
} // namespace finale_mus_reader_tests
