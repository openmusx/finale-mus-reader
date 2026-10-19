// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;
using IndependentStaff = musx::dom::details::IndependentStaffDetails;

constexpr musx::dom::Cmper testStaffId = 3;
constexpr musx::dom::Cmper testMeas = 9;

const std::set<std::string> persistedFields{"keySig.key", "keySig.keyless", "keySig.hideKeySigShowAccis", "hasKey", "beats", "divBeat", "dispBeats",
    "dispDivBeat", "displayAltNumTsig", "displayAltDenTsig", "altNumTsig", "altDenTsig", "displayAbbrvTime", "hasDispTime", "hasTime"};

ImportReport importIndependentStaff(const finale_mus_reader::container::ParsedContainer& parsed, musx::dom::DocumentPtr& document)
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
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::details::importIndependentStaffDetails(context);
    return report;
}

finale_mus_reader::container::ParsedContainer independentStaffContainer(
    FormatEpoch epoch, ByteOrder byteOrder, const std::vector<std::int16_t>& words)
{
    return epoch == FormatEpoch::ZlibLegacy ? makeDetailClassContainer(testStaffId, testMeas, musx::dom::SCORE_PARTID, words, byteOrder, 0x0411)
                                            : makeDetailContainer(epoch, testStaffId, testMeas, words, "FL", byteOrder);
}

const ImportReport::InstanceFields& reportedFields(const ImportReport& report, musx::dom::Cmper staffId, musx::dom::Cmper meas)
{
    const auto key = finale_mus_reader::instanceKey<IndependentStaff>(musx::dom::SCORE_PARTID, staffId, std::nullopt, meas);
    REQUIRE(report.fields.contains(key));
    return report.fields.at(key);
}

TEST_CASE("IndependentStaffDetails reports every persisted field", "[class][independent-staff]")
{
    CHECK(IndependentStaff::xmlMappingArray().size() == 13);
    CHECK(musx::dom::KeySignature::xmlMappingArray().size() == 3);

    for (const auto& words : {std::vector<std::int16_t>{0, 4, 1024, 0, 0x0c00}, std::vector<std::int16_t>{0, 4, 1024, 0, 0x0e00, 3, 512, 0, 0, 0}}) {
        musx::dom::DocumentPtr document;
        const auto report = importIndependentStaff(independentStaffContainer(FormatEpoch::DclLegacy, ByteOrder::BigEndian, words), document);
        std::set<std::string> reported;
        for (const auto& [member, info] : reportedFields(report, testStaffId, testMeas)) {
            reported.insert(member);
        }
        CHECK(reported == persistedFields);
        const auto& fields = reportedFields(report, testStaffId, testMeas);
        CHECK(fields.at("keySig.keyless").origin == ValueOrigin::LegacyBehavior);
        CHECK(fields.at("keySig.hideKeySigShowAccis").origin == ValueOrigin::LegacyBehavior);
    }
}

TEST_CASE("IndependentStaffDetails decodes the single-incidence layout in every epoch", "[class][independent-staff]")
{
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            // Key 2, 5/8 time, both composite bits, and every later bit set so that they are shown
            // to be ignored where no second incidence exists.
            musx::dom::DocumentPtr document;
            const auto report = importIndependentStaff(independentStaffContainer(epoch, byteOrder, {2, 5, 512, 0, 0x0ec7}), document);

            const auto details = document->getDetails()->get<IndependentStaff>(musx::dom::SCORE_PARTID, testStaffId, testMeas);
            REQUIRE(details);
            REQUIRE(details->keySig);
            CHECK(details->keySig->key == 2);
            CHECK_FALSE(details->keySig->keyless);
            CHECK_FALSE(details->keySig->hideKeySigShowAccis);
            CHECK(details->beats == 5);
            CHECK(details->divBeat == 512);
            CHECK(details->hasKey);
            CHECK(details->hasTime);
            CHECK(details->altNumTsig);
            CHECK(details->altDenTsig);
            CHECK_FALSE(details->hasDispTime);
            CHECK_FALSE(details->displayAltNumTsig);
            CHECK_FALSE(details->displayAltDenTsig);
            CHECK_FALSE(details->displayAbbrvTime);
            CHECK(details->dispBeats == 0);
            CHECK(details->dispDivBeat == 0);

            const auto& fields = reportedFields(report, testStaffId, testMeas);
            CHECK(fields.at("beats").origin == ValueOrigin::LegacyMus);
            CHECK(fields.at("beats").rawValue == 5);
            CHECK(fields.at("hasTime").rawValue == 0x0ec7);
            for (const auto* member : {"dispBeats", "dispDivBeat", "hasDispTime", "displayAltNumTsig", "displayAltDenTsig", "displayAbbrvTime"}) {
                CHECK(fields.at(member).origin == ValueOrigin::LegacyBehavior);
            }
        }
    }
}

TEST_CASE("IndependentStaffDetails decodes the display time signature of the two-incidence layout", "[class][independent-staff]")
{
    for (const auto epoch : {FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            musx::dom::DocumentPtr document;
            const auto report =
                importIndependentStaff(independentStaffContainer(epoch, byteOrder, {0, 2, 768, 0, 0x0607, 2, 512, 7, 8, 9}), document);

            const auto details = document->getDetails()->get<IndependentStaff>(musx::dom::SCORE_PARTID, testStaffId, testMeas);
            REQUIRE(details);
            CHECK_FALSE(details->hasKey);
            CHECK(details->hasTime);
            CHECK(details->beats == 2);
            CHECK(details->divBeat == 768);
            CHECK_FALSE(details->altNumTsig);
            CHECK_FALSE(details->altDenTsig);
            CHECK(details->hasDispTime);
            CHECK(details->dispBeats == 2);
            CHECK(details->dispDivBeat == 512);
            CHECK(details->displayAltNumTsig);
            CHECK(details->displayAltDenTsig);
            CHECK(details->displayAbbrvTime);

            const auto& fields = reportedFields(report, testStaffId, testMeas);
            CHECK(fields.at("displayAbbrvTime").origin == ValueOrigin::LegacyMus);
            CHECK(fields.at("dispBeats").origin == ValueOrigin::LegacyMus);
            CHECK(fields.at("dispDivBeat").rawValue == 512);
            CHECK(fields.at("hasDispTime").rawValue == 0x0607);
            if (epoch == FormatEpoch::ZlibLegacy) {
                CHECK(fields.at("dispBeats").sourceIdentity == 0x0411);
            } else {
                // The display words sit in the second row.
                CHECK(fields.at("dispBeats").decodedOffset == 16);
                CHECK(fields.at("dispBeats").sourceIdentity == finale_mus_reader::records::packTag("FL"));
            }
        }
    }
}

TEST_CASE("IndependentStaffDetails keeps display words the flag word does not enable", "[class][independent-staff]")
{
    musx::dom::DocumentPtr document;
    const auto report = importIndependentStaff(
        independentStaffContainer(FormatEpoch::DclLegacy, ByteOrder::BigEndian, {255, 0, 0, 0, 0x0800, -17992, 0, 5, 1277, 16128}), document);

    const auto details = document->getDetails()->get<IndependentStaff>(musx::dom::SCORE_PARTID, testStaffId, testMeas);
    REQUIRE(details);
    CHECK(details->hasKey);
    CHECK(details->keySig->key == 255);
    CHECK_FALSE(details->hasTime);
    CHECK_FALSE(details->hasDispTime);
    CHECK(details->dispBeats == 47544);
    CHECK(details->dispDivBeat == 0);
    CHECK(reportedFields(report, testStaffId, testMeas).at("dispBeats").origin == ValueOrigin::LegacyMus);
}

TEST_CASE("IndependentStaffDetails rejects a truncated class payload", "[class][independent-staff]")
{
    musx::dom::DocumentPtr document;
    const auto report = importIndependentStaff(independentStaffContainer(FormatEpoch::ZlibLegacy, ByteOrder::LittleEndian, {0, 4, 1024}), document);
    CHECK(document->getDetails()->getAllSources<IndependentStaff>().empty());
    CHECK(report.diagnostics.size() == 1);
}

TEST_CASE("Controlled fixtures recover independent key and time signatures", "[class][independent-staff]")
{
    SECTION("Coda-banner single incidence")
    {
        const auto result = readFixture("evidence/F100/staffopts/F100-floattime.mus");
        const auto details = result.document->getDetails()->get<IndependentStaff>(musx::dom::SCORE_PARTID, 1, 1);
        REQUIRE(details);
        CHECK(details->hasKey);
        CHECK(details->hasTime);
        CHECK(details->beats == 4);
        CHECK(details->divBeat == 1024);
        CHECK(details->keySig->key == 0);
        CHECK_FALSE(details->hasDispTime);
        CHECK(result.document->getDetails()->getAllSources<IndependentStaff>().size() == 1);
    }
    SECTION("Coda-banner key only")
    {
        const auto result = readFixture("evidence/F100/staffopts/F100-floatkey.mus");
        const auto details = result.document->getDetails()->get<IndependentStaff>(musx::dom::SCORE_PARTID, 1, 1);
        REQUIRE(details);
        CHECK(details->hasKey);
        CHECK_FALSE(details->hasTime);
    }
    SECTION("DCL two incidences")
    {
        const auto result = readFixture("evidence/F2002/F2002-fileinfo-text.mus");
        for (const auto staffId : {musx::dom::Cmper(5), musx::dom::Cmper(6)}) {
            const auto details = result.document->getDetails()->get<IndependentStaff>(musx::dom::SCORE_PARTID, staffId, 1);
            REQUIRE(details);
            CHECK(details->hasKey);
            CHECK(details->keySig->key == 255);
            CHECK_FALSE(details->hasTime);
            CHECK_FALSE(details->hasDispTime);
            CHECK(details->dispBeats == 47544);
            CHECK(details->dispDivBeat == 0);
        }
    }
    SECTION("DCL abbreviated display time signature")
    {
        const auto result = readFixture("evidence/F2001/F2001Win-indtime-abrv.mus");
        const auto details = result.document->getDetails()->get<IndependentStaff>(musx::dom::SCORE_PARTID, 2, 1);
        REQUIRE(details);
        CHECK_FALSE(details->hasKey);
        CHECK(details->hasTime);
        CHECK(details->beats == 3);
        CHECK(details->divBeat == 1024);
        CHECK(details->hasDispTime);
        CHECK(details->dispBeats == 4);
        CHECK(details->dispDivBeat == 1024);
        CHECK(details->displayAbbrvTime);
        CHECK_FALSE(details->displayAltNumTsig);
        CHECK_FALSE(details->displayAltDenTsig);
        const auto* abbreviated = result.report.findField<IndependentStaff>(
            "displayAbbrvTime", musx::dom::SCORE_PARTID, musx::dom::Cmper(2), std::nullopt, musx::dom::Cmper(1));
        REQUIRE(abbreviated);
        CHECK(abbreviated->origin == ValueOrigin::LegacyMus);
        CHECK(abbreviated->rawValue == 0x0604);
    }
    SECTION("DCL composite display time signature")
    {
        const auto result = readFixture("evidence/F2001/F2001Win-indtime-comp.mus");
        const auto details = result.document->getDetails()->get<IndependentStaff>(musx::dom::SCORE_PARTID, 2, 1);
        REQUIRE(details);
        CHECK(details->hasTime);
        CHECK(details->beats == 4);
        CHECK(details->divBeat == 1024);
        CHECK_FALSE(details->altNumTsig);
        CHECK_FALSE(details->altDenTsig);
        CHECK(details->hasDispTime);
        CHECK(details->displayAltNumTsig);
        CHECK(details->displayAltDenTsig);
        CHECK_FALSE(details->displayAbbrvTime);
        // With both display bits set, the display words are composite-list comparators.
        CHECK(details->dispBeats == 2);
        CHECK(details->dispDivBeat == 2);
        CHECK(result.document->getOthers()->get<musx::dom::others::TimeCompositeUpper>(musx::dom::SCORE_PARTID, details->dispBeats));
        CHECK(result.document->getOthers()->get<musx::dom::others::TimeCompositeLower>(musx::dom::SCORE_PARTID, details->dispDivBeat));
        CHECK_FALSE(details->createDisplayTimeSignature()->isSame(*details->createTimeSignature()));
    }
    SECTION("zlib class record")
    {
        const auto result = readFixture("evidence/F2011/F2011-mixedstyle.mus");
        CHECK(result.document->getDetails()->getAllSources<IndependentStaff>().size() == 5);
        for (musx::dom::Cmper meas = 5; meas <= 9; ++meas) {
            const auto details = result.document->getDetails()->get<IndependentStaff>(musx::dom::SCORE_PARTID, 1, meas);
            REQUIRE(details);
            CHECK(details->hasKey);
            CHECK(details->hasTime);
            CHECK(details->beats == 4);
            CHECK(details->divBeat == 1024);
            CHECK_FALSE(details->hasDispTime);
            CHECK(details->getShareMode() == musx::dom::EnigmaBase::ShareMode::All);
        }
    }
    SECTION("absent when no record is stored")
    {
        for (const auto* fixture : {"evidence/F2002/F2002-baseline.mus", "evidence/F2012/F2012-baseline.mus"}) {
            CHECK(readFixture(fixture).document->getDetails()->getAllSources<IndependentStaff>().empty());
        }
    }
}

} // namespace
} // namespace finale_mus_reader_tests
