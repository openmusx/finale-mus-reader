// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "class_test_support.h"
#include <set>

namespace finale_mus_reader_tests {
namespace {

using namespace classes;

TEST_CASE("Graphic file locators preserve controlled source values", "[file_path]")
{
    using namespace musx::dom::others;
    const auto classic = readFixture("evidence/F372/F372-page-graphic.mus");
    REQUIRE(classic.document);
    const auto desc = classic.document->getOthers()->get<FileDescription>(0, 1);
    REQUIRE(desc);
    CHECK(desc->version == 256);
    CHECK(desc->pathType == FileDescription::PathType::MacFsSpec);
    CHECK(desc->pathId == 1);
    CHECK(desc->volRefNum == -3);
    CHECK(desc->dirId == 16);
#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
    const auto& fields = classic.report.fields.at(finale_mus_reader::instanceKey<FileDescription>(0, musx::dom::Cmper(1)));
    std::set<std::string> names;
    for (const auto& [name, info] : fields) {
        names.insert(name);
        CHECK(info.origin == ValueOrigin::LegacyMus);
        CHECK(info.blockOffset == 0x200);
        CHECK(info.decodedOffset == 0x13b0);
        CHECK(info.sourceIdentity == finale_mus_reader::records::packTag("Fd"));
    }
    CHECK(names == std::set<std::string>{"version", "pathType", "pathId", "volRefNum", "dirId"});
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
    const auto path = classic.document->getOthers()->get<FilePath>(0, 1);
    REQUIRE(path);
    CHECK(path->path == "Photo_tiff.tiff");

    const auto dcl = readFixture("evidence/F2006/F2006-linked-tiff.mus");
    REQUIRE(dcl.document);
    const auto alias = dcl.document->getOthers()->get<FileAlias>(0, 1);
    REQUIRE(alias);
    CHECK(alias->length == 462);
    REQUIRE(alias->aliasHandle.size() == alias->length);
    CHECK(
        std::vector<std::uint8_t>(alias->aliasHandle.begin(), alias->aliasHandle.begin() + 8) == std::vector<std::uint8_t>{0, 0, 0, 0, 206, 1, 2, 0});
    CHECK(dcl.document->getOthers()->get<FileDescription>(0, 1)->dirId == 380256);
    CHECK(dcl.document->getOthers()->getArray<FileUrlBookmark>(0).empty());

    const auto zlib = readFixture("evidence/F2012/F2012-graphics-types.mus");
    REQUIRE(zlib.document);
    CHECK(zlib.document->getOthers()->getArray<FileDescription>(0).size() == 6);
    CHECK(zlib.document->getOthers()->get<FileDescription>(0, 1)->pathType == FileDescription::PathType::MacAlias);
    CHECK(zlib.document->getOthers()->get<FileDescription>(0, 1)->dirId == 341052);
    CHECK(zlib.document->getOthers()->get<FilePath>(0, 1)->path == "GifSample.gif");
    CHECK(zlib.document->getOthers()->get<FileAlias>(0, 1)->length == 392);
    CHECK(zlib.document->getOthers()->getArray<FileUrlBookmark>(0).empty());
}

TEST_CASE("File locators bound opaque lengths and report all fields", "[file_path]")
{
    using namespace musx::dom::others;
    for (const auto epoch : {FormatEpoch::CodaBanner, FormatEpoch::UncompressedLegacy, FormatEpoch::DclLegacy, FormatEpoch::ZlibLegacy}) {
        for (const auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
            const auto pathType = order == ByteOrder::LittleEndian ? std::int16_t{1} : std::int16_t{3};
            const std::vector<std::int16_t> description{256, pathType, 7, -5, order == ByteOrder::BigEndian ? std::int16_t{1} : std::int16_t{2},
                order == ByteOrder::BigEndian ? std::int16_t{2} : std::int16_t{1}};
            const auto parsed =
                epoch == FormatEpoch::ZlibLegacy
                    ? makeClassContainer({SyntheticClassRow{0x008a, description, 7}, SyntheticClassRow{0x0089, {3, 0, 0x1234, 0x5678}, 7},
                                             SyntheticClassRow{0x0089, {100, 0, 0, 0}, 8}, SyntheticClassRow{0x008a, {256}, 8}},
                          order)
                    : makeContainer({{7, "Fd", {description[0], description[1], description[2], description[3], description[4], description[5]}},
                                        {7, "Fa", {3, 0, 0x1234, 0x5678, 0, 0}}, {8, "Fa", {100, 0, 0, 0, 0, 0}}},
                          epoch, order);
            const auto index = LegacyRecordIndex::build(parsed);
            auto session = musx::factory::DocumentFactory::begin();
            const auto document = session.getDocument();
            auto referenceSession = musx::factory::DocumentFactory::begin();
            const auto reference = std::move(referenceSession).finish();
            ImportReport report(epoch);
            finale_mus_reader::PendingReferences pending;
            SourceProfile profile(epoch);
            profile.byteOrder = order;
            musx::factory::ConstructionContext construction;
            const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
            finale_mus_reader::others::importFilePath(context);
            if (epoch == FormatEpoch::CodaBanner) {
                CHECK(document->getOthers()->getArray<FileDescription>(0).empty());
                CHECK(document->getOthers()->getArray<FileAlias>(0).empty());
                continue;
            }
            const auto desc = document->getOthers()->get<FileDescription>(0, 7);
            REQUIRE(desc);
            CHECK(desc->dirId == 65538);
            CHECK(desc->volRefNum == -5);
            CHECK(desc->pathType == (pathType == 1 ? FileDescription::PathType::DosPath : FileDescription::PathType::MacAlias));
            const auto alias = document->getOthers()->get<FileAlias>(0, 7);
            REQUIRE(alias);
            CHECK(alias->length == 3);
            CHECK(alias->aliasHandle
                  == (order == ByteOrder::BigEndian ? std::vector<std::uint8_t>{0x34, 0x12, 0x56} : std::vector<std::uint8_t>{0x12, 0x34, 0x78}));
#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
            CHECK(report.fields.at(finale_mus_reader::instanceKey<FileAlias>(0, musx::dom::Cmper(7))).at("aliasHandle").origin
                  == ValueOrigin::LegacyMusAdjusted);
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
            CHECK_FALSE(document->getOthers()->get<FileAlias>(0, 8));
            CHECK(reportedFieldCount(report) == 7);
            CHECK_FALSE(report.diagnostics.empty());
        }
    }
}
} // namespace
} // namespace finale_mus_reader_tests
