// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using StaffOptionsTestTarget = musx::dom::options::StaffOptions;
using StaffNamePositioningTestTarget = musx::dom::others::NamePositioning;

musx::dom::DocumentPtr makeStaffOptionsDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<StaffOptionsTestTarget>(document);
    options->staffSeparation = 91;
    options->staffSeparIncr = 92;
    options->autoAdjustStaffSepar = true;
    const auto makePosition = [&] {
        auto result = std::make_shared<StaffNamePositioningTestTarget>(document);
        result->horzOff = 81;
        result->vertOff = 82;
        result->justify = musx::dom::AlignJustify::Left;
        result->indivPos = false;
        result->hAlign = musx::dom::AlignJustify::Left;
        result->expand = false;
        result->hidden = false;
        return result;
    };
    options->namePos = makePosition();
    options->namePosAbbrv = makePosition();
    options->groupNameFullPos = makePosition();
    options->groupNameAbbrvPos = makePosition();
    document->getOptions()->add(StaffOptionsTestTarget::XmlNodeName, options);
    return std::move(session).finish();
}

std::shared_ptr<const StaffOptionsTestTarget> importStaffOptionsForTest(
    const finale_mus_reader::container::ParsedContainer& parsed, FormatEpoch epoch, ImportReport& report)
{
    const auto document = makeStaffOptionsDocument();
    const auto reference = makeStaffOptionsDocument();
    SourceProfile profile(epoch);
    profile.byteOrder = parsed.byteOrder;
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{
        LegacyRecordIndex::build(parsed), profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::options::importStaffOptions(context);
    return document->getOptions()->get<StaffOptionsTestTarget>();
}

void expectStaffNamePosition(const StaffNamePositioningTestTarget& position, musx::dom::Evpu horz, musx::dom::Evpu vert,
    musx::dom::AlignJustify justify, musx::dom::AlignJustify align, bool expand)
{
    REQUIRE(position.horzOff == horz);
    REQUIRE(position.vertOff == vert);
    REQUIRE(position.justify == justify);
    REQUIRE(position.hAlign == align);
    REQUIRE(position.expand == expand);
    REQUIRE_FALSE(position.indivPos);
    REQUIRE_FALSE(position.hidden);
}

void expectCompleteStaffOptionsReport(
    const ImportReport& report, ValueOrigin locatedOrigin, ValueOrigin staffSeparationOrigin, ValueOrigin otherScalarOrigin)
{
    const auto instance = finale_mus_reader::instanceKey<StaffOptionsTestTarget>();
    REQUIRE(report.fields.at(instance).size() == 31);
    REQUIRE(report.findField<StaffOptionsTestTarget>("staffSeparation")->origin == staffSeparationOrigin);
    for (const auto* member : {"staffSeparIncr", "autoAdjustStaffSepar"}) {
        REQUIRE(report.findField<StaffOptionsTestTarget>(member)->origin == otherScalarOrigin);
    }
    for (const auto* member : {"namePos.indivPos", "namePos.hidden", "namePosAbbrv.indivPos", "namePosAbbrv.hidden", "groupNameFullPos.indivPos",
             "groupNameFullPos.hidden", "groupNameAbbrvPos.indivPos", "groupNameAbbrvPos.hidden"}) {
        REQUIRE(report.findField<StaffOptionsTestTarget>(member)->origin == ValueOrigin::Finale27Default);
    }
    for (const auto* prefix : {"namePos", "namePosAbbrv", "groupNameFullPos", "groupNameAbbrvPos"}) {
        for (const auto* leaf : {"horzOff", "vertOff", "justify", "hAlign", "expand"}) {
            REQUIRE(report.findField<StaffOptionsTestTarget>(std::string(prefix).append(".").append(leaf))->origin == locatedOrigin);
        }
    }
}

void expectCodaStaffOptionsReport(const ImportReport& report)
{
    const auto instance = finale_mus_reader::instanceKey<StaffOptionsTestTarget>();
    REQUIRE(report.fields.at(instance).size() == 31);
    REQUIRE(report.findField<StaffOptionsTestTarget>("staffSeparation")->origin == ValueOrigin::LegacyBehavior);
    for (const auto* member : {"staffSeparIncr", "autoAdjustStaffSepar"}) {
        REQUIRE(report.findField<StaffOptionsTestTarget>(member)->origin == ValueOrigin::Finale27Default);
    }
    for (const auto* prefix : {"namePos", "namePosAbbrv"}) {
        for (const auto* leaf : {"horzOff", "vertOff", "justify", "hAlign"}) {
            REQUIRE(report.findField<StaffOptionsTestTarget>(std::string(prefix).append(".").append(leaf))->origin == ValueOrigin::LegacyBehavior);
        }
        for (const auto* leaf : {"expand", "indivPos", "hidden"}) {
            REQUIRE(report.findField<StaffOptionsTestTarget>(std::string(prefix).append(".").append(leaf))->origin == ValueOrigin::Finale27Default);
        }
    }
    for (const auto* prefix : {"groupNameFullPos", "groupNameAbbrvPos"}) {
        for (const auto* leaf : {"horzOff", "vertOff", "justify", "hAlign", "expand", "indivPos", "hidden"}) {
            REQUIRE(report.findField<StaffOptionsTestTarget>(std::string(prefix).append(".").append(leaf))->origin == ValueOrigin::Finale27Default);
        }
    }
}

TEST_CASE("Staff scalar tail is selected by the class-record payload", "[class]")
{
    const std::vector<std::int16_t> scalarWords{0, 10000, 0, 10000, 0, 0, 1, 1, 1, 1, 0, 0, -289, 71, 0};
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        ImportReport report(FormatEpoch::ZlibLegacy);
        const auto options = importStaffOptionsForTest(
            makeClassContainer(finale_mus_reader::numericGlobalClass(97), scalarWords, byteOrder, GLOBALS_CMPER), FormatEpoch::ZlibLegacy, report);
        REQUIRE(options->staffSeparation == -289);
        REQUIRE(options->staffSeparIncr == 71);
        REQUIRE_FALSE(options->autoAdjustStaffSepar);
        expectCompleteStaffOptionsReport(report, ValueOrigin::Finale27Default, ValueOrigin::LegacyMus, ValueOrigin::LegacyMus);
        for (const auto* member : {"staffSeparation", "staffSeparIncr", "autoAdjustStaffSepar"}) {
            REQUIRE(report.findField<StaffOptionsTestTarget>(member)->sourceIdentity == 0x006f);
        }
    }

    ImportReport shortReport(FormatEpoch::ZlibLegacy);
    const auto shortOptions = importStaffOptionsForTest(
        makeClassContainer(finale_mus_reader::numericGlobalClass(97), std::vector<std::int16_t>(12, 0), ByteOrder::BigEndian, GLOBALS_CMPER),
        FormatEpoch::ZlibLegacy, shortReport);
    REQUIRE(shortOptions->staffSeparation == -320);
    REQUIRE(shortOptions->staffSeparIncr == 92);
    REQUIRE(shortOptions->autoAdjustStaffSepar);
    expectCompleteStaffOptionsReport(shortReport, ValueOrigin::Finale27Default, ValueOrigin::LegacyBehavior, ValueOrigin::Finale27Default);

    ImportReport fixedRowReport(FormatEpoch::UncompressedLegacy);
    const std::vector<SyntheticRow> fixedRows{
        {GLOBALS_CMPER, "97", {0, 10000, 0, 10000, 0, 0}},
        {GLOBALS_CMPER, "97", {1, 1, 1, 1, 0, 0}},
        {GLOBALS_CMPER, "97", {-289, 71, 0, 0, 0, 0}},
    };
    const auto fixedRowOptions =
        importStaffOptionsForTest(makeContainer(fixedRows, FormatEpoch::UncompressedLegacy), FormatEpoch::UncompressedLegacy, fixedRowReport);
    REQUIRE(fixedRowOptions->staffSeparation == -320);
    REQUIRE(fixedRowOptions->staffSeparIncr == 92);
    REQUIRE(fixedRowOptions->autoAdjustStaffSepar);
    expectCompleteStaffOptionsReport(fixedRowReport, ValueOrigin::Finale27Default, ValueOrigin::LegacyBehavior, ValueOrigin::Finale27Default);
}

TEST_CASE("Controlled Finale 2012 staff scalars recover from the stored tail", "[class][reader]")
{
    const auto result = readFixture("evidence/F2012/F2012-staffopts-scalars.mus");
    const auto options = result.document->getOptions()->get<StaffOptionsTestTarget>();
    REQUIRE(options);
    REQUIRE(options->staffSeparation == -289);
    REQUIRE(options->staffSeparIncr == 71);
    REQUIRE_FALSE(options->autoAdjustStaffSepar);
    for (const auto* member : {"staffSeparation", "staffSeparIncr", "autoAdjustStaffSepar"}) {
        const auto* info = result.report.findField<StaffOptionsTestTarget>(member);
        REQUIRE(info);
        REQUIRE(info->origin == ValueOrigin::LegacyMus);
        REQUIRE(info->sourceIdentity == 0x006f);
    }
}

TEST_CASE("Controlled Coda staff names use fixed positioning behavior", "[class][reader]")
{
    const auto result = readFixture("evidence/F100/F100-baseline.mus");
    const auto options = result.document->getOptions()->get<StaffOptionsTestTarget>();
    REQUIRE(options);
    expectStaffNamePosition(*options->namePos, -192, -27, musx::dom::AlignJustify::Left, musx::dom::AlignJustify::Left, true);
    expectStaffNamePosition(*options->namePosAbbrv, -192, -27, musx::dom::AlignJustify::Left, musx::dom::AlignJustify::Left, true);
    expectStaffNamePosition(*options->groupNameFullPos, -72, 0, musx::dom::AlignJustify::Right, musx::dom::AlignJustify::Right, true);
    expectStaffNamePosition(*options->groupNameAbbrvPos, -72, 0, musx::dom::AlignJustify::Right, musx::dom::AlignJustify::Right, true);
    expectCodaStaffOptionsReport(result.report);
}

TEST_CASE("Staff name positions recover across all container epochs", "[class]")
{
    const std::vector<SyntheticRow> fixedRows{
        {GLOBALS_CMPER, "04", {-216, -24, 0, 0, 0, -32751}},
        {GLOBALS_CMPER, "66", {-144, -22, 0, 0, 0, 34}},
        {GLOBALS_CMPER, "79", {-187, 7, -32759, 0, 0, 0}},
        {GLOBALS_CMPER, "80", {-72, 9, 18, 0, 0, 0}},
    };
    const auto verify = [](const auto& options, const ImportReport& report, std::uint16_t firstIdentity) {
        expectStaffNamePosition(*options->namePos, -216, -24, musx::dom::AlignJustify::Right, musx::dom::AlignJustify::Right, true);
        expectStaffNamePosition(*options->namePosAbbrv, -144, -22, musx::dom::AlignJustify::Center, musx::dom::AlignJustify::Center, false);
        expectStaffNamePosition(*options->groupNameFullPos, -187, 7, musx::dom::AlignJustify::Right, musx::dom::AlignJustify::Right, true);
        expectStaffNamePosition(*options->groupNameAbbrvPos, -72, 9, musx::dom::AlignJustify::Center, musx::dom::AlignJustify::Center, false);
        expectCompleteStaffOptionsReport(report, ValueOrigin::LegacyMus, ValueOrigin::LegacyBehavior, ValueOrigin::Finale27Default);
        REQUIRE(report.findField<StaffOptionsTestTarget>("namePos.horzOff")->sourceIdentity == firstIdentity);
    };

    ImportReport codaReport(FormatEpoch::CodaBanner);
    const auto codaOptions = importStaffOptionsForTest(makeContainer(fixedRows, FormatEpoch::CodaBanner), FormatEpoch::CodaBanner, codaReport);
    expectStaffNamePosition(*codaOptions->namePos, -192, -27, musx::dom::AlignJustify::Left, musx::dom::AlignJustify::Left, false);
    expectStaffNamePosition(*codaOptions->namePosAbbrv, -192, -27, musx::dom::AlignJustify::Left, musx::dom::AlignJustify::Left, false);
    expectStaffNamePosition(*codaOptions->groupNameFullPos, 81, 82, musx::dom::AlignJustify::Left, musx::dom::AlignJustify::Left, false);
    expectStaffNamePosition(*codaOptions->groupNameAbbrvPos, 81, 82, musx::dom::AlignJustify::Left, musx::dom::AlignJustify::Left, false);
    expectCodaStaffOptionsReport(codaReport);

    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy}) {
        ImportReport report(epoch);
        verify(importStaffOptionsForTest(makeContainer(fixedRows, epoch), epoch, report), report, finale_mus_reader::numericGlobalTag(4));
    }

    const std::vector<SyntheticClassRow> classRows{
        {finale_mus_reader::numericGlobalClass(4), {-216, -24, 0, 0, 0, -32751}},
        {finale_mus_reader::numericGlobalClass(66), {-144, -22, 0, 0, 0, 34}},
        {finale_mus_reader::numericGlobalClass(79), {-187, 7, -32759}},
        {finale_mus_reader::numericGlobalClass(80), {-72, 9, 18}},
    };
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        ImportReport report(FormatEpoch::ZlibLegacy);
        verify(importStaffOptionsForTest(makeClassContainer(classRows, byteOrder), FormatEpoch::ZlibLegacy, report), report,
            finale_mus_reader::numericGlobalClass(4));
    }
}

TEST_CASE("Early staff name positions approximate the font-metric baseline", "[class]")
{
    const std::vector<SyntheticRow> rows{
        {GLOBALS_CMPER, "04", {-80, -63, 4, 14, 1, 1}},
        {GLOBALS_CMPER, "66", {-64, -64, 4, 12, 1, 0}},
    };
    ImportReport report(FormatEpoch::UncompressedLegacy);
    const auto options = importStaffOptionsForTest(makeContainer(rows, FormatEpoch::UncompressedLegacy), FormatEpoch::UncompressedLegacy, report);

    expectStaffNamePosition(*options->namePos, -80, -21, musx::dom::AlignJustify::Right, musx::dom::AlignJustify::Right, false);
    expectStaffNamePosition(*options->namePosAbbrv, -64, -28, musx::dom::AlignJustify::Left, musx::dom::AlignJustify::Left, false);
    for (const auto* prefix : {"groupNameFullPos", "groupNameAbbrvPos"}) {
        const auto& position = prefix == std::string_view("groupNameFullPos") ? options->groupNameFullPos : options->groupNameAbbrvPos;
        expectStaffNamePosition(*position, 81, 82, musx::dom::AlignJustify::Left, musx::dom::AlignJustify::Left, false);
        for (const auto* leaf : {"horzOff", "vertOff", "justify", "hAlign", "expand", "indivPos", "hidden"}) {
            REQUIRE(report.findField<StaffOptionsTestTarget>(std::string(prefix).append(".").append(leaf))->origin == ValueOrigin::Finale27Default);
        }
    }

    for (const auto* prefix : {"namePos", "namePosAbbrv"}) {
        REQUIRE(report.findField<StaffOptionsTestTarget>(std::string(prefix).append(".horzOff"))->origin == ValueOrigin::LegacyMus);
        REQUIRE(report.findField<StaffOptionsTestTarget>(std::string(prefix).append(".vertOff"))->origin == ValueOrigin::LegacyMusAdjusted);
        REQUIRE(report.findField<StaffOptionsTestTarget>(std::string(prefix).append(".justify"))->origin == ValueOrigin::LegacyMus);
        REQUIRE(report.findField<StaffOptionsTestTarget>(std::string(prefix).append(".expand"))->origin == ValueOrigin::Finale27Default);
    }
    REQUIRE(report.findField<StaffOptionsTestTarget>("namePos.hAlign")->origin == ValueOrigin::LegacyBehavior);
    REQUIRE(report.findField<StaffOptionsTestTarget>("namePosAbbrv.hAlign")->origin == ValueOrigin::Finale27Default);
    REQUIRE(report.findField<StaffOptionsTestTarget>("namePos.vertOff")->rawValue == -21);
    REQUIRE(report.findField<StaffOptionsTestTarget>("namePosAbbrv.vertOff")->rawValue == -28);
}

TEST_CASE("Coda name positions combine fixed behavior with seeded values", "[class]")
{
    ImportReport report(FormatEpoch::CodaBanner);
    const auto options = importStaffOptionsForTest(makeContainer({}, FormatEpoch::CodaBanner), FormatEpoch::CodaBanner, report);
    expectStaffNamePosition(*options->namePos, -192, -27, musx::dom::AlignJustify::Left, musx::dom::AlignJustify::Left, false);
    expectStaffNamePosition(*options->namePosAbbrv, -192, -27, musx::dom::AlignJustify::Left, musx::dom::AlignJustify::Left, false);
    expectStaffNamePosition(*options->groupNameFullPos, 81, 82, musx::dom::AlignJustify::Left, musx::dom::AlignJustify::Left, false);
    expectCodaStaffOptionsReport(report);
}

TEST_CASE("Truncated staff name position records do not partially overlay defaults", "[class]")
{
    const std::vector<SyntheticClassRow> rows{
        {finale_mus_reader::numericGlobalClass(4), {-216, -24}},
    };
    ImportReport report(FormatEpoch::ZlibLegacy);
    const auto options = importStaffOptionsForTest(makeClassContainer(rows, ByteOrder::LittleEndian), FormatEpoch::ZlibLegacy, report);
    expectStaffNamePosition(*options->namePos, 81, 82, musx::dom::AlignJustify::Left, musx::dom::AlignJustify::Left, false);
    expectCompleteStaffOptionsReport(report, ValueOrigin::Finale27Default, ValueOrigin::LegacyBehavior, ValueOrigin::Finale27Default);
}

} // namespace
} // namespace finale_mus_reader_tests
