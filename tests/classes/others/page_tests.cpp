// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

using Page = musx::dom::others::Page;

ImportReport pageImport(
    const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile, const musx::dom::DocumentPtr& document)
{
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    auto referenceSession = musx::factory::DocumentFactory::begin();
    const auto reference = std::move(referenceSession).finish();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::others::importPages(context);
    return report;
}

musx::dom::DocumentPtr emptyPageDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    return session.getDocument();
}

void expectCompletePage(const Page& page)
{
    expect(page.height == 3168 && page.width == 2448 && page.firstSystemId == 7, "Page dimensions or first-system reference were not recovered");
    expect(page.holdMargins, "Page Hold Margins was not recovered");
    expect(page.margTop == -141 && page.margLeft == 142 && page.margBottom == 143 && page.margRight == -145, "Page margins were not recovered");
    expect(page.percent == 91, "Page scaling was not recovered");
}

TEST_CASE("Page recovers the complete fixed-row layout")
{
    const auto parsed = makeContainer({{1, "PS", {0, 3168, 0, 2448, 7, 2}}, {1, "PS", {-141, 142, 143, -145, 91, 0}}});
    const auto document = emptyPageDocument();
    auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    const auto report = pageImport(parsed, profile, document);
    const auto page = document->getOthers()->get<Page>(musx::dom::SCORE_PARTID, 1);
    expect(page != nullptr, "The fixed-row Page was not constructed");
    expectCompletePage(*page);
    for (const auto* member : {"height", "width", "percent", "firstSystemId", "holdMargins", "margTop", "margLeft", "margBottom", "margRight"}) {
        const auto* source = report.findField<Page>(member, musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        expect(source && source->origin == ValueOrigin::LegacyMus, std::string("A Page member has the wrong origin: ") + member);
    }
    expect(reportedFieldCount(report) == Page::xmlMappingArray().size(), "The Page report does not exhaust its persisted field manifest");
}

TEST_CASE("Page recovers compact auxiliary records in either byte order")
{
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        const auto pageWords = byteOrder == ByteOrder::BigEndian ? std::array<std::int16_t, 6>{0, 3168, 0, 2448, 7, 2048}
                                                                 : std::array<std::int16_t, 6>{3168, 0, 2448, 0, 7, 2048};
        const auto parsed = makeContainer({{1, "PO", {-141, 142, 143, -145, 0, 0}}, {1, "PP", {91, 91, 0, 0, 0, 16384}}, {1, "PS", pageWords}},
            FormatEpoch::UncompressedLegacy, byteOrder);
        const auto document = emptyPageDocument();
        auto profile = SourceProfile(FormatEpoch::UncompressedLegacy);
        profile.byteOrder = byteOrder;
        const auto report = pageImport(parsed, profile, document);
        const auto page = document->getOthers()->get<Page>(musx::dom::SCORE_PARTID, 1);
        REQUIRE(page);
        expectCompletePage(*page);
        for (const auto* member : {"percent", "holdMargins", "margTop", "margLeft", "margBottom", "margRight"}) {
            const auto* source = report.findField<Page>(member, musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
            CHECK(source);
            CHECK(source->origin == ValueOrigin::LegacyMus);
        }
    }
}

TEST_CASE("Page recovers zlib class records in either byte order")
{
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        const auto words = byteOrder == ByteOrder::BigEndian ? std::vector<std::int16_t>{0, 3168, 0, 2448, 7, 2, -141, 142, 143, -145, 91, 0}
                                                             : std::vector<std::int16_t>{3168, 0, 2448, 0, 7, 2, -141, 142, 143, -145, 91, 0};
        const auto parsed = makeClassContainer({SyntheticClassRow{0x00bb, words, 1, 0}}, byteOrder);
        const auto document = emptyPageDocument();
        auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
        profile.byteOrder = byteOrder;
        const auto report = pageImport(parsed, profile, document);
        const auto page = document->getOthers()->get<Page>(musx::dom::SCORE_PARTID, 1);
        expect(page != nullptr, "The zlib Page was not constructed");
        expectCompletePage(*page);
        expect(reportedFieldCount(report) == Page::xmlMappingArray().size(), "A zlib Page did not report every persisted field");
    }
}

TEST_CASE("Page preserves independent score and part pagination")
{
    using ShareMode = musx::dom::EnigmaBase::ShareMode;
    const auto parsed = makeClassContainer({SyntheticClassRow{0x00bb, {0, 3168, 0, 2448, 1, 2, -144, 144, 144, -144, 100, 0}, 1, 0},
                                               SyntheticClassRow{0x00bb, {0, 3200, 0, 2500, 4, 0, -120, 130, 140, -150, 85, 0}, 1, 3}},
        ByteOrder::BigEndian);
    const auto document = emptyPageDocument();
    auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    const auto report = pageImport(parsed, profile, document);
    const auto score = document->getOthers()->get<Page>(musx::dom::SCORE_PARTID, 1);
    const auto part = document->getOthers()->get<Page>(3, 1);

    REQUIRE(score);
    REQUIRE(part);
    CHECK(score->getShareMode() == ShareMode::All);
    CHECK(part->getSourcePartId() == 3);
    CHECK(part->getShareMode() == ShareMode::None);
    CHECK(part->height == 3200);
    CHECK(part->width == 2500);
    CHECK(part->firstSystemId == 4);
    CHECK(part->percent == 85);
    const auto* source = report.findField<Page>("width", musx::dom::Cmper(3), musx::dom::Cmper(1));
    REQUIRE(source);
    CHECK(source->origin == ValueOrigin::LegacyMus);
}

TEST_CASE("An unrecognized Page payload is rejected without a partial object")
{
    for (const auto& words : {std::vector<std::int16_t>{1, 2, 3, 4, 5}, std::vector<std::int16_t>{1, 2, 3, 4, 5, 6, 7}}) {
        const auto parsed = makeClassContainer({SyntheticClassRow{0x00bb, words, 1, 0}}, ByteOrder::BigEndian);
        const auto document = emptyPageDocument();
        auto profile = SourceProfile(FormatEpoch::ZlibLegacy);
        profile.byteOrder = ByteOrder::BigEndian;
        const auto report = pageImport(parsed, profile, document);
        expect(document->getOthers()->getAllSources<Page>().empty(), "A truncated Page record created a partial object");
        expect(report.diagnostics.size() == 1, "A truncated Page record was not diagnosed once");
    }
}

TEST_CASE("Page recovers controlled fixtures across every epoch")
{
    const auto coda100 = readFixture("evidence/F100/F100-baseline-recalc.mus");
    const auto page100 = coda100.document->getOthers()->get<Page>(musx::dom::SCORE_PARTID, 1);
    expect(page100 && page100->height == 3168 && page100->width == 2448 && page100->firstSystemId == 1 && page100->percent == 100
               && page100->holdMargins && page100->margTop == -144 && page100->margLeft == 144 && page100->margBottom == 144
               && page100->margRight == -144,
        "Finale 1.0 Page behavior was not recovered");

    const auto coda100Edited = readFixture("evidence/F100/F100-page-margin-percent.mus");
    const auto page100Edited = coda100Edited.document->getOthers()->get<Page>(musx::dom::SCORE_PARTID, 1);
    expect(page100Edited && page100Edited->percent == 91 && !page100Edited->holdMargins && page100Edited->margTop == -145
               && page100Edited->margLeft == 146 && page100Edited->margBottom == 148 && page100Edited->margRight == -147,
        "Finale 1.0 auxiliary Page fields were not recovered");
    for (const auto* member : {"percent", "holdMargins", "margTop", "margLeft", "margBottom", "margRight"}) {
        const auto* source = coda100Edited.report.findField<Page>(member, musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        expect(source && source->origin == ValueOrigin::LegacyMus, std::string("An auxiliary Page member has the wrong origin: ") + member);
    }

    const auto coda100Held = readFixture("evidence/F100/F100-page-91hold.mus");
    const auto page100Held = coda100Held.document->getOthers()->get<Page>(musx::dom::SCORE_PARTID, 1);
    expect(page100Held && page100Held->percent == 91 && page100Held->holdMargins && page100Held->margTop == -145 && page100Held->margLeft == 146
               && page100Held->margBottom == 148 && page100Held->margRight == -147,
        "Finale 1.0 Page Hold Margins was not recovered independently");
    const auto* heldMargins = coda100Held.report.findField<Page>("holdMargins", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    expect(heldMargins && heldMargins->origin == ValueOrigin::LegacyMus, "Finale 1.0 Page Hold Margins has the wrong origin");

    const auto* unscaledPercent = coda100.report.findField<Page>("percent", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    expect(unscaledPercent && unscaledPercent->origin == ValueOrigin::LegacyBehavior,
        "A Page without a percentage record did not report its unscaled behavior");

    const auto coda263 = readFixture("evidence/F263/F263-altnotation.mus");
    const auto page263 = coda263.document->getOthers()->get<Page>(musx::dom::SCORE_PARTID, 1);
    expect(page263 && page263->percent == 80 && page263->holdMargins && page263->margTop == -144 && page263->margLeft == 144
               && page263->margBottom == 144 && page263->margRight == -144,
        "Finale 2.6.3 Page behavior was not recovered");
    for (const auto* member : {"percent", "margTop", "margLeft", "margBottom", "margRight"}) {
        const auto* source = coda263.report.findField<Page>(member, musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
        expect(source && source->origin == ValueOrigin::LegacyMus, std::string("A compact Page member has the wrong origin: ") + member);
    }
    const auto* holdMargins = coda263.report.findField<Page>("holdMargins", musx::dom::SCORE_PARTID, musx::dom::Cmper(1));
    expect(holdMargins && holdMargins->origin == ValueOrigin::LegacyMus, "Compact Page Hold Margins has the wrong origin");

    for (const auto* relative : {"evidence/F372/F372-4systems.mus", "evidence/F2002/F2002-empty.mus", "evidence/F2007/F2007-lyric-hyphens.mus",
             "evidence/F2012/F2012-baseline.mus"}) {
        const auto result = readFixture(relative);
        const auto page = result.document->getOthers()->get<Page>(musx::dom::SCORE_PARTID, 1);
        expect(page && page->height == 3168 && page->width == 2448 && page->firstSystemId == 1 && page->percent == 100 && page->holdMargins
                   && page->margTop == -144 && page->margLeft == 144 && page->margBottom == 144 && page->margRight == -144,
            std::string("Page disagrees with the controlled fixture: ") + relative);
    }
}

} // namespace
} // namespace finale_mus_reader_tests
