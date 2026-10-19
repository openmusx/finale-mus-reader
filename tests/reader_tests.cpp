// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "container/mus_container.h"
#include "container/product_banner.h"
#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <zlib.h>

#include "finale_mus_reader/reader.h"
#include "import/support/playback_plugin_names.h"
#include "musx/musx.h"
#include "reader/resources.h"
#include "support/finale_version.h"

#ifndef MUSX_USE_PUGIXML
#define MUSX_USE_PUGIXML
#define FINALE_MUS_READER_TEST_UNDEFINE_MUSX_USE_PUGIXML
#endif // !defined(MUSX_USE_PUGIXML)

#include "musx/xml/PugiXmlImpl.h"

#ifdef FINALE_MUS_READER_TEST_UNDEFINE_MUSX_USE_PUGIXML
#undef MUSX_USE_PUGIXML
#undef FINALE_MUS_READER_TEST_UNDEFINE_MUSX_USE_PUGIXML
#endif // defined(FINALE_MUS_READER_TEST_UNDEFINE_MUSX_USE_PUGIXML)

namespace {

using finale_mus_reader::BlockInfo;
using finale_mus_reader::ByteOrder;
using finale_mus_reader::FieldInfo;
using finale_mus_reader::FormatEpoch;
using finale_mus_reader::ImportResult;
using finale_mus_reader::Reader;
using finale_mus_reader::SourcePlatform;
using finale_mus_reader::ValueOrigin;
using musx::dom::MUSX_GLOBALS_CMPER;
using TestXmlDocument = musx::xml::pugi::Document;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void write16(std::vector<std::uint8_t>& output, std::uint16_t value, ByteOrder byteOrder)
{
    if (byteOrder == ByteOrder::BigEndian) {
        output.push_back(static_cast<std::uint8_t>(value >> 8U));
        output.push_back(static_cast<std::uint8_t>(value));
    } else {
        output.push_back(static_cast<std::uint8_t>(value));
        output.push_back(static_cast<std::uint8_t>(value >> 8U));
    }
}

void write32(std::vector<std::uint8_t>& output, std::uint32_t value, ByteOrder byteOrder)
{
    if (byteOrder == ByteOrder::BigEndian) {
        output.push_back(static_cast<std::uint8_t>(value >> 24U));
        output.push_back(static_cast<std::uint8_t>(value >> 16U));
        output.push_back(static_cast<std::uint8_t>(value >> 8U));
        output.push_back(static_cast<std::uint8_t>(value));
    } else {
        output.push_back(static_cast<std::uint8_t>(value));
        output.push_back(static_cast<std::uint8_t>(value >> 8U));
        output.push_back(static_cast<std::uint8_t>(value >> 16U));
        output.push_back(static_cast<std::uint8_t>(value >> 24U));
    }
}

void writeFixed(std::vector<std::uint8_t>& output, std::size_t offset, std::string_view value, std::size_t capacity)
{
    expect(offset + capacity <= output.size(), "Synthetic fixed field exceeds its buffer");
    const auto count = (std::min)(value.size(), capacity);
    std::copy_n(value.begin(), static_cast<std::ptrdiff_t>(count), output.begin() + static_cast<std::ptrdiff_t>(offset));
}

// The synthetic Enigma version: major 12, minor 3, maintenance 4, build 5. It is
// written in the file's own byte order, as Finale writes it.
constexpr std::uint32_t syntheticVersion = (12U << 24U) | (3U << 20U) | (4U << 16U) | 5U;

void writeVersion(std::vector<std::uint8_t>& output, std::size_t offset, std::uint32_t value, ByteOrder byteOrder)
{
    for (std::size_t i = 0; i < 4; ++i) {
        const auto shift = byteOrder == ByteOrder::BigEndian ? (3 - i) * 8 : i * 8;
        output[offset + i] = static_cast<std::uint8_t>(value >> shift);
    }
}

std::vector<std::uint8_t> makeBanner(std::string_view product, std::string_view platform, ByteOrder byteOrder)
{
    std::vector<std::uint8_t> result(0x200);
    writeFixed(result, 0, "ENIGMA BINARY FILE", 19);
    writeFixed(result, 0x20, "Finale(R) " + std::string(product) + " Copyright synthetic", 0x40);
    // The header flags byte: bit 0 states big-endian, and the other bits are set as an ordinary
    // document sets them so that only bit 0 can be what is read.
    result[0x62] = byteOrder == ByteOrder::BigEndian ? 0x07 : 0x06;
    result[0x66] = 126;
    result[0x67] = 8;
    result[0x68] = 8;
    result[0x8c] = 126;
    result[0x8d] = 8;
    result[0x8e] = 8;
    for (const auto tupleOffset : {std::size_t{0x6c}, std::size_t{0x92}}) {
        writeVersion(result, tupleOffset, syntheticVersion, byteOrder);
        writeFixed(result, tupleOffset + 4, "FIN", 4);
        writeFixed(result, tupleOffset + 8, platform, 4);
        writeVersion(result, tupleOffset + 12, syntheticVersion, byteOrder);
        writeVersion(result, tupleOffset + 16, syntheticVersion, byteOrder);
    }
    return result;
}

std::array<std::int16_t, 6> words(
    std::int16_t first, std::int16_t second, std::int16_t third, std::int16_t fourth, std::int16_t fifth, std::int16_t sixth)
{
    return {first, second, third, fourth, fifth, sixth};
}

void appendOther(
    std::vector<std::uint8_t>& output, std::uint16_t cmper, std::string_view tag, const std::array<std::int16_t, 6>& payload, ByteOrder byteOrder)
{
    expect(tag.size() == 2, "Synthetic other tag must be two bytes");
    write16(output, cmper, byteOrder);
    // The tag is a 16-bit value, so a little-endian file stores "LA" as the bytes "AL".
    // Writing the characters raw would encode a file Finale never produces.
    write16(output, static_cast<std::uint16_t>((static_cast<unsigned char>(tag[0]) << 8U) | static_cast<unsigned char>(tag[1])), byteOrder);
    for (const auto value : payload) {
        write16(output, static_cast<std::uint16_t>(value), byteOrder);
    }
}

void appendUncompressedBlock(std::vector<std::uint8_t>& output, std::uint16_t type, const std::vector<std::uint8_t>& payload, ByteOrder byteOrder)
{
    write16(output, type, byteOrder);
    write32(output, static_cast<std::uint32_t>(payload.size() + 6), byteOrder);
    output.insert(output.end(), payload.begin(), payload.end());
}

std::vector<std::uint8_t> makeUncompressedMus()
{
    constexpr auto byteOrder = ByteOrder::LittleEndian;
    auto result = makeBanner("2000", "WIN", byteOrder);
    std::vector<std::uint8_t> others;
    // The flag word carries every layer boolean. Layer 0 takes the group the layer dialog has
    // always had, layer 3 takes the low pair and the hidden-notes test, which no tracked
    // fixture sets, and layers 1 and 2 leave it clear.
    appendOther(others, 0, "LA", words(11, 0, 0, 0, 0, 0x0f80), byteOrder);
    appendOther(others, 1, "LA", words(-12, 0, 0, 0, 0, 0), byteOrder);
    appendOther(others, 2, "LA", words(13, 0, 0, 0, 0, 0), byteOrder);
    appendOther(others, 3, "LA", words(-14, 0, 0, 0, 0, 0x4003), byteOrder);
    // The first comparator past the modern layer range, which musxdom reads no meaning into and
    // the baseline never seeded. It is still something the file says, so the import must carry it.
    appendOther(others, musx::dom::MAX_LAYERS, "LA", words(15, 0, 0, 0, 0, 0x0080), byteOrder);
    appendOther(others, MUSX_GLOBALS_CMPER, "94", words(2, 361, 1801, 13, 49, 0), byteOrder);
    appendUncompressedBlock(result, 1, others, byteOrder);
    appendUncompressedBlock(result, 2, {}, byteOrder);
    appendUncompressedBlock(result, 3, {}, byteOrder);
    appendUncompressedBlock(result, 4, {}, byteOrder);
    return result;
}

// An uncompressed-era file carrying a selector 24 default-font array. Its banner version is
// major 12, deliberately outside the Finale 3.0 through 2002 range, so that the epoch has to
// carry the semantic layout rather than the version.
std::vector<std::uint8_t> makeUncompressedMusWithFontOptions()
{
    constexpr auto byteOrder = ByteOrder::LittleEndian;
    auto result = makeBanner("2000", "WIN", byteOrder);
    std::vector<std::uint8_t> others;
    // One font definition so the recovered ids resolve.
    appendOther(others, 0, "FN", words(0x1fff, 0, 0, 0, 0, 0), byteOrder);
    appendOther(others, 0, "FN", words(0x4d61, 0x6573, 0x7472, 0x6f00, 0, 0), byteOrder);
    // Two incidences of selector 24: four three-word tuples, music/key/clef/time.
    appendOther(others, MUSX_GLOBALS_CMPER, "24", words(0, 28, 0, 0, 26, 0), byteOrder);
    appendOther(others, MUSX_GLOBALS_CMPER, "24", words(0, 24, 0, 0, 22, 1), byteOrder);
    appendUncompressedBlock(result, 1, others, byteOrder);
    appendUncompressedBlock(result, 2, {}, byteOrder);
    appendUncompressedBlock(result, 3, {}, byteOrder);
    appendUncompressedBlock(result, 4, {}, byteOrder);
    return result;
}

// A Coda-banner document in either byte order. The era has no block framing to trial, so the
// container takes its order from the banner product: a `PC` product is a Windows document and
// little-endian, anything else is big-endian. Both are built here from one description so the
// two paths cannot drift apart, and so that neither needs a committed fixture -- the only real
// Windows documents of this era are Coda's own installer templates.
std::vector<std::uint8_t> makeCodaBannerMus(ByteOrder byteOrder, std::string_view product, bool includeBlankShape = false)
{
    std::vector<std::uint8_t> result(0x200, 0);
    const std::string banner = "Finale(TM) " + std::string(product) + " Copyright 1987 by Coda. All rights reserved.";
    writeFixed(result, 0, banner, banner.size());
    // The header's two non-zero bytes as the era stores them: the Mac releases after 1.0.0 carry
    // 01 03, Windows documents 00 07. Byte 0x81 holds the format flags.
    result[0x80] = byteOrder == ByteOrder::BigEndian ? 0x01 : 0x00;
    result[0x81] = byteOrder == ByteOrder::BigEndian ? 0x03 : 0x07;

    // The era's eight clefs are one single-incidence global each, selectors 28 through 35,
    // holding middle-C position, an unexplained word, the clef character and the staff
    // position. These are the era's stock values.
    std::vector<std::uint8_t> pool;
    const std::array<std::array<std::int16_t, 4>, 8> clefs{{{-10, 6, 38, -6}, {-4, 0, 66, -4}, {-2, -2, 66, -2}, {2, -6, 63, -2}, {-10, 6, 214, -4},
        {-3, -1, 86, -6}, {9, -13, 116, -2}, {0, -4, 63, -4}}};
    for (std::size_t i = 0; i < clefs.size(); ++i) {
        const char tag[2] = {static_cast<char>('0' + (28 + i) / 10), static_cast<char>('0' + (28 + i) % 10)};
        appendOther(pool, MUSX_GLOBALS_CMPER, std::string_view(tag, 2), words(clefs[i][0], clefs[i][1], clefs[i][2], clefs[i][3], 0, 0), byteOrder);
    }
    // Scalars the era does record: the default clef, the end-of-measure percent and offset,
    // and the spacing before and after a clef.
    appendOther(pool, MUSX_GLOBALS_CMPER, "01", words(0, 0, 0, 0, 0, 0), byteOrder);
    appendOther(pool, MUSX_GLOBALS_CMPER, "13", words(4, 24, 75, -12, 0, 0), byteOrder);
    appendOther(pool, MUSX_GLOBALS_CMPER, "19", words(24, 0, 0, 0, 0, 0), byteOrder);
    if (includeBlankShape) {
        appendOther(pool, 19, "SD", words(19, 19, 0, 0, -584, 868), byteOrder);
        appendOther(pool, 19, "SL", words(0, 0, 0, 0, 0, 0), byteOrder);
    }
    pool.resize(0x200, 0);

    // One pool: a page count and a page size, then that many 512-byte pages. The page size is
    // what confirms the era, and it reads 0x200 only in the file's own order.
    write32(result, 1, byteOrder);
    write32(result, 0x200, byteOrder);
    result.insert(result.end(), pool.begin(), pool.end());
    return result;
}

std::vector<std::uint8_t> compressZlib(const std::vector<std::uint8_t>& input)
{
    uLongf compressedSize = compressBound(static_cast<uLong>(input.size()));
    std::vector<std::uint8_t> result(compressedSize);
    const auto code = compress2(result.data(), &compressedSize, input.data(), static_cast<uLong>(input.size()), Z_BEST_COMPRESSION);
    expect(code == Z_OK, "Unable to create synthetic zlib member");
    result.resize(compressedSize);
    return result;
}

void appendZlibBlock(std::vector<std::uint8_t>& output, std::uint16_t type, const std::vector<std::uint8_t>& payload, ByteOrder byteOrder)
{
    const auto compressed = compressZlib(payload);
    write16(output, type, byteOrder);
    write32(output, static_cast<std::uint32_t>(compressed.size() + 10), byteOrder);
    const auto checksum = static_cast<std::uint32_t>(crc32(crc32(0L, Z_NULL, 0), payload.data(), static_cast<uInt>(payload.size())));
    write32(output, checksum, byteOrder);
    output.insert(output.end(), compressed.begin(), compressed.end());
}

std::vector<std::uint8_t> makeZlibMus()
{
    constexpr auto byteOrder = ByteOrder::LittleEndian;
    auto result = makeBanner("2012", "MAC", byteOrder);
    appendZlibBlock(result, 0x001a, {1, 2, 3}, byteOrder);
    appendZlibBlock(result, 0x001b, {4, 5}, byteOrder);
    appendZlibBlock(result, 0x0016, {6}, byteOrder);
    appendZlibBlock(result, 0x0017, {7, 8, 9}, byteOrder);
    write16(result, 0x001d, byteOrder);
    write32(result, 6, byteOrder);
    return result;
}

// The same file, but its terminal block carries an embedded graphic instead of being an
// empty marker. A stored block has no checksum word and its payload starts right after the
// six-byte header, so writing one is not appendZlibBlock with compression turned off.
std::vector<std::uint8_t> makeZlibMusWithGraphic(std::uint16_t terminalType, std::string_view graphic)
{
    constexpr auto byteOrder = ByteOrder::LittleEndian;
    auto result = makeBanner("2012", "MAC", byteOrder);
    appendZlibBlock(result, 0x001a, {1, 2, 3}, byteOrder);
    appendZlibBlock(result, 0x001b, {4, 5}, byteOrder);
    appendZlibBlock(result, 0x0016, {6}, byteOrder);
    appendZlibBlock(result, 0x0017, {7, 8, 9}, byteOrder);
    std::vector<std::uint8_t> stored;
    if (terminalType == 0x0013) {
        // Each item is type, byte length, raw file bytes, footer version, and one
        // still-opaque footer byte. The embedded cmper is its one-based encounter order.
        write16(stored, 9, byteOrder);
        write32(stored, static_cast<std::uint32_t>(graphic.size()), byteOrder);
        stored.insert(stored.end(), graphic.begin(), graphic.end());
        write32(stored, 1, byteOrder);
        stored.push_back(0);
    } else {
        stored.insert(stored.end(), graphic.begin(), graphic.end());
    }
    write16(result, terminalType, byteOrder);
    write32(result, static_cast<std::uint32_t>(stored.size() + 6), byteOrder);
    result.insert(result.end(), stored.begin(), stored.end());
    return result;
}

// Taken by value rather than by const reference: see the note on the equivalent helper in
// mapping_tests.cpp about -Wdangling-reference.
const FieldInfo& field(const ImportResult& result, std::string_view target)
{
    std::string desiredMember(target);
    std::optional<musx::dom::Cmper> cmper;
    if (target.starts_with("options.fontOptions[")) {
        desiredMember = "fonts[" + std::string(target.substr(std::string_view("options.fontOptions[").size()));
    } else if (target.starts_with("options.fontOptionsPhysical[")) {
        desiredMember = "physical[" + std::string(target.substr(std::string_view("options.fontOptionsPhysical[").size()));
    } else if (target.starts_with("options.")) {
        const auto dot = target.find('.', std::string_view("options.").size());
        if (dot != std::string_view::npos) {
            desiredMember = target.substr(dot + 1);
        }
    } else if (const auto open = target.find('['); open != std::string_view::npos) {
        const auto close = target.find(']', open);
        const auto dot = close == std::string_view::npos ? close : target.find('.', close);
        if (close != std::string_view::npos && dot != std::string_view::npos) {
            cmper = static_cast<musx::dom::Cmper>(std::stoul(std::string(target.substr(open + 1, close - open - 1))));
            desiredMember = target.substr(dot + 1);
        }
    }
    for (const auto& [instance, fields] : result.report.fields) {
        if (cmper && instance.cmper1 != cmper) {
            continue;
        }
        for (const auto& [member, value] : fields) {
            if (member == desiredMember) {
                return value;
            }
        }
    }
    throw std::runtime_error(std::string("Missing field report for ").append(target));
}

template <typename Predicate>
bool anyReportedField(const finale_mus_reader::ImportReport& report, Predicate predicate)
{
    for (const auto& [instance, fields] : report.fields) {
        (void)instance;
        for (const auto& [member, info] : fields) {
            if (predicate(member, info)) {
                return true;
            }
        }
    }
    return false;
}

// Whether the report carries any diagnostic at the given level. Tests assert the level a
// message was raised at, not merely that some message exists: the whole point of the level
// is that a routine fallback and an unreadable document must not look alike to a host.
bool hasDiagnostic(const finale_mus_reader::ImportReport& report, musx::util::Logger::LogLevel level)
{
    return std::any_of(
        report.diagnostics.begin(), report.diagnostics.end(), [&](const finale_mus_reader::Diagnostic& entry) { return entry.level == level; });
}

template <typename T>
void expectOption(const ImportResult& result)
{
    expect(static_cast<bool>(result.document->getOptions()->get<T>()), "Pinned default omitted an expected options instance");
}

// The imported options pool must be structurally complete whatever era the source is.
// Most of these instances are seeded from the pinned baseline; FontOptions and ClefOptions
// are filtered out of that seeding and rebuilt, so their presence here is a check that the
// rebuild ran rather than that the baseline was copied.
void expectCompleteOptionsPool(const ImportResult& result)
{
    using namespace musx::dom::options;
    expectOption<AccidentalOptions>(result);
    expectOption<AlternateNotationOptions>(result);
    expectOption<AugmentationDotOptions>(result);
    expectOption<BarlineOptions>(result);
    expectOption<BeamOptions>(result);
    expectOption<ChordOptions>(result);
    expectOption<ClefOptions>(result);
    expectOption<FlagOptions>(result);
    expectOption<GraceNoteOptions>(result);
    expectOption<KeySignatureOptions>(result);
    expectOption<LineCurveOptions>(result);
    expectOption<LyricOptions>(result);
    expectOption<MiscOptions>(result);
    expectOption<MultimeasureRestOptions>(result);
    expectOption<MusicSpacingOptions>(result);
    expectOption<MusicSymbolOptions>(result);
    expectOption<NoteRestOptions>(result);
    expectOption<PageFormatOptions>(result);
    expectOption<PianoBraceBracketOptions>(result);
    expectOption<RepeatOptions>(result);
    expectOption<SmartShapeOptions>(result);
    expectOption<StaffOptions>(result);
    expectOption<StemOptions>(result);
    expectOption<TextOptions>(result);
    expectOption<TieOptions>(result);
    expectOption<TimeSignatureOptions>(result);
    expectOption<TupletOptions>(result);
}

void expectNoScoreContent(const ImportResult& result)
{
    using namespace musx::dom;
    // Measures are recovered, so their presence is not fallback content. What would be is a
    // measure the baseline supplied, and the baseline supplies none: its others allowlist admits
    // layer attributes and nothing else. Every measure in an imported document therefore has to
    // have been built from a source record, which is what its instance origin states.
    for (const auto& measure : result.document->getOthers()->getArray<others::Measure>(SCORE_PARTID)) {
        const auto* origin =
            result.report.findInstanceOrigin(finale_mus_reader::instanceKey<others::Measure>(measure->getSourcePartId(), measure->getCmper()));
        expect(origin != nullptr && *origin == finale_mus_reader::ValueOrigin::LegacyMus, "Output contains a measure no source record built");
    }
    for (const auto& staff : result.document->getOthers()->getArray<others::Staff>(SCORE_PARTID)) {
        const auto* origin =
            result.report.findInstanceOrigin(finale_mus_reader::instanceKey<others::Staff>(staff->getSourcePartId(), staff->getCmper()));
        expect(origin != nullptr && *origin == finale_mus_reader::ValueOrigin::LegacyMus, "Output contains a staff no source record built");
    }
    for (const auto& system : result.document->getOthers()->getArray<others::StaffSystem>(SCORE_PARTID)) {
        const auto* origin =
            result.report.findInstanceOrigin(finale_mus_reader::instanceKey<others::StaffSystem>(system->getSourcePartId(), system->getCmper()));
        expect(origin != nullptr && *origin == finale_mus_reader::ValueOrigin::LegacyMus, "Output contains a staff system no source record built");
    }
    for (const auto& page : result.document->getOthers()->getArray<others::Page>(SCORE_PARTID)) {
        const auto* origin =
            result.report.findInstanceOrigin(finale_mus_reader::instanceKey<others::Page>(page->getSourcePartId(), page->getCmper()));
        expect(origin != nullptr && *origin == finale_mus_reader::ValueOrigin::LegacyMus, "Output contains a page no source record built");
    }
    // Part definitions are not absent: musxdom requires a score part, and every era has one
    // whether or not it stored a record. What must not appear is the baseline's, which names a
    // text block this document does not have -- so the reader's own score part carries no name.
    {
        const auto parts = result.document->getOthers()->getArray<others::PartDefinition>(SCORE_PARTID);
        expect(!parts.empty(), "Output has no score part definition");
        const auto score = others::PartDefinition::getScore(result.document);
        expect(score && score->partOrder == 0 && score->copies == 1, "The score part definition is not the one this reader builds");
        for (const auto& part : parts) {
            expect(part->nameId == 0 || result.document->getOthers()->get<others::TextBlock>(SCORE_PARTID, part->nameId) != nullptr,
                "A part definition names a text block the document does not contain");
        }
    }
    // The pinned <others> element is not copied wholesale. The seven canned marking
    // categories are imported deliberately because pre-Finale-2009 expressions need the
    // category identities that Finale supplies when upgrading them.
    expect(result.document->getOthers()->getArray<others::MarkingCategory>(SCORE_PARTID).size() == 7,
        "Output does not contain exactly the canned marking categories");
    // Shapes are no longer absent. The two tablature clef definitions completed from the
    // baseline are drawn as shapes, and those shapes are copied in so the clefs render. Nothing
    // else may ride along, so every shape present must be one a clef definition names.
    {
        std::set<musx::dom::Cmper> referencedShapes;
        if (const auto clefs = result.document->getOptions()->get<musx::dom::options::ClefOptions>()) {
            for (const auto& def : clefs->clefDefs) {
                if (def->isShape && def->shapeId != 0) {
                    referencedShapes.insert(def->shapeId);
                }
            }
        }
        for (const auto& shape : result.document->getOthers()->getArray<others::ShapeDef>(SCORE_PARTID)) {
            const auto* shapeField = result.report.findField<others::ShapeDef>("instructionList", SCORE_PARTID, shape->getCmper());
            const auto recovered = shapeField && shapeField->origin == ValueOrigin::LegacyMus;
            expect(recovered || referencedShapes.count(shape->getCmper()) != 0, "Output contains a baseline shape no clef definition references");
        }
    }
    expect(result.document->getOthers()->getArray<others::TextBlock>(SCORE_PARTID).empty(), "Output contains fallback text blocks");
    expect(
        result.document->getOthers()->getArray<others::MeasureNumberRegion>(SCORE_PARTID).empty(), "Output contains fallback measure number regions");
    if (const auto entry = result.document->getEntries()->get(1)) {
        const auto* origin = result.report.findInstanceOrigin(finale_mus_reader::instanceKey<Entry>(
            SCORE_PARTID, static_cast<Cmper>(entry->getEntryNumber() >> 16U), std::nullopt, static_cast<Cmper>(entry->getEntryNumber())));
        expect(origin != nullptr && *origin == finale_mus_reader::ValueOrigin::LegacyMus, "Output contains an entry no source row built");
    }
    {
        const auto scrollView = result.document->getScrollViewStaves(SCORE_PARTID);
        std::set<StaffCmper> recoveredStaffIds;
        for (const auto& staffUsed : scrollView) {
            recoveredStaffIds.insert(staffUsed->staffId);
        }
        std::set<StaffCmper> instrumentStaffIds;
        for (const auto& [instrumentId, instrument] : result.document->getInstruments()) {
            static_cast<void>(instrumentId);
            for (const auto& [staffId, staffIndex] : instrument.staves) {
                static_cast<void>(staffIndex);
                instrumentStaffIds.insert(staffId);
            }
        }
        expect(instrumentStaffIds == recoveredStaffIds, "Computed instruments do not match the recovered scroll-view staves");
    }
    // The four option-like layer attributes are always present, seeded or recovered. The count
    // is not asserted: a source is free to carry comparators beyond them, and those are imported
    // rather than discarded, so a total above four is source content and not a fallback leak.
    for (musx::dom::Cmper layer = 0; layer < musx::dom::MAX_LAYERS; ++layer) {
        expect(static_cast<bool>(result.document->getOthers()->get<others::LayerAttributes>(SCORE_PARTID, layer)),
            "Output is missing option-like layer attributes " + std::to_string(layer));
    }
}

void testControlledDclFile()
{
    const auto path = std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F2002/F2002-baseline.mus";
    const auto result = Reader::readWithReport<TestXmlDocument>(path);
    expect(result.report.formatEpoch == FormatEpoch::DclLegacy, "F2002 fixture was not classified as DCL");
    expect(result.report.byteOrder == ByteOrder::BigEndian, "F2002 fixture byte order was not recovered");
    expect(result.report.sourcePlatform == SourcePlatform::MacOS, "F2002 fixture platform was not recovered");
    expect(result.report.defaultsPlatform == SourcePlatform::MacOS, "F2002 fixture was not seeded from the macOS baseline");
    expect(result.report.savingProduct == "2002", "F2002 product was not recovered");
    expect(result.report.blocks.size() == 4, "F2002 block count is incorrect");
    expect(std::all_of(result.report.blocks.begin(), result.report.blocks.begin() + 3,
               [](const BlockInfo& block) { return block.checksumPresent && block.checksumValid; }),
        "F2002 compressed block checksum validation failed");

    const auto& header = *result.document->getHeader();
    expect(header.created.year == 2026 && header.created.month == 8 && header.created.day == 5, "F2002 creation date was not recovered");
    expect(header.created.application == "FIN", "F2002 creator application was not recovered");
    expect(header.created.finaleVersion.major == finale_mus_reader::versions::finale2002.major
               && header.created.finaleVersion.minor == finale_mus_reader::versions::finale2002.minor && header.created.finaleVersion.maint == 1,
        "F2002 internal creator version was not recovered");
    expect(result.document->getSourcePath() == path, "Source path was not retained");

    const auto spacing = result.document->getOptions()->get<musx::dom::options::MusicSpacingOptions>();
    expect(spacing->minWidth == 360 && spacing->maxWidth == 1800, "F2002 music spacing width overlay failed");
    expect(spacing->minDistance == 12 && spacing->minDistTiedNotes == 48, "F2002 music spacing distance overlay failed");
    expect(
        field(result, "options.musicSpacing.minWidth").origin == ValueOrigin::LegacyMus, "F2002 music spacing overlay was not reported as recovered");
    expectCompleteOptionsPool(result);
    expectNoScoreContent(result);
}

void testIndependentImportedDocuments()
{
    // The reader builds each document with its own construction session, so no pinned
    // fallback document can remain the owner of options placed in an imported document.
    const auto path = std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F2002/F2002-baseline.mus";
    const auto first = Reader::readWithReport<TestXmlDocument>(path);
    const auto second = Reader::readWithReport<TestXmlDocument>(path);
    expect(first.document != second.document, "Both reads returned the same document");

    const auto firstSpacing = first.document->getOptions()->get<musx::dom::options::MusicSpacingOptions>();
    const auto secondSpacing = second.document->getOptions()->get<musx::dom::options::MusicSpacingOptions>();
    expect(firstSpacing && secondSpacing, "Music spacing options were not seeded");
    expect(firstSpacing.get() != secondSpacing.get(), "Imported documents share a music spacing options instance");

    const auto firstLayer = first.document->getOthers()->get<musx::dom::others::LayerAttributes>(musx::dom::SCORE_PARTID, 0);
    const auto secondLayer = second.document->getOthers()->get<musx::dom::others::LayerAttributes>(musx::dom::SCORE_PARTID, 0);
    expect(firstLayer && secondLayer, "Layer attributes were not seeded");
    expect(firstLayer.get() != secondLayer.get(), "Imported documents share a layer attributes instance");
}

// Selector 24 is the default-font array from well before the DCL era, but the reader used to
// gate that layout to DCL alone, so every Finale 3.0 through 2000 document reported all 45
// font options as Finale 27 defaults while its source held 40 of them.
void testUncompressedFontOptionsEpochGate()
{
    using FontOptions = musx::dom::options::FontOptions;
    using FontType = FontOptions::FontType;
    // The epoch alone must carry the semantic layout, because three real Finale 3.0
    // documents recover a major version far outside the era's own range.
    const auto synthetic = Reader::readWithReport<TestXmlDocument>(makeUncompressedMusWithFontOptions());
    const auto options = synthetic.document->getOptions()->get<FontOptions>();
    expect(options->getFontInfo(FontType::Music)->fontSize == 28 && options->getFontInfo(FontType::Key)->fontSize == 26
               && options->getFontInfo(FontType::Clef)->fontSize == 24 && options->getFontInfo(FontType::Time)->fontSize == 22,
        "An uncompressed file with an out-of-range major version recovered no font options");
    expect(options->getFontInfo(FontType::Time)->bold, "A recovered effects mask was not expanded for the uncompressed era");
}

// The uncompressed era's tracked fixtures.
void testUncompressedFixtures()
{
    const auto read = [](const char* relative) {
        return Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / relative);
    };
    using musx::dom::others::FontDefinition;

    const auto f2000 = read("evidence/F2000/F2000-multilayer.mus");
    expect(f2000.report.formatEpoch == FormatEpoch::UncompressedLegacy, "Finale 2000 fixture was not classified as uncompressed");
    expect(f2000.report.savingProduct == "2000", "Finale 2000 product was not recovered");
    expect(f2000.report.sourceVersion && f2000.report.sourceVersion->major == finale_mus_reader::versions::finale2000.major,
        "Finale 2000 should record internal major version 5");
    expect(field(f2000, "options.musicSpacing.minWidth").origin == ValueOrigin::LegacyMus, "Finale 2000 music spacing was not recovered");
    expect(!f2000.document->getOthers()->getArray<FontDefinition>(musx::dom::SCORE_PARTID).empty(), "Finale 2000 fonts were not recovered");

    // These two differ only in the length of their file-info text. The long one overruns the
    // customary 0x200 body boundary, so it frames only when the body offset is read from the
    // header field at 0x60 rather than assumed.
    const auto shortInfo = read("evidence/F97/F97-fileinfo-short.mus");
    const auto longInfo = read("evidence/F97/F97-fileinfo-long.mus");
    for (const auto* result : {&shortInfo, &longInfo}) {
        expect(result->report.formatEpoch == FormatEpoch::UncompressedLegacy, "A Finale 97 fixture was not classified as uncompressed");
        expect(result->report.sourceVersion
                   && finale_mus_reader::VersionBound{result->report.sourceVersion->major, result->report.sourceVersion->minor}
                          == finale_mus_reader::versions::finale97,
            "Finale 97 should record internal version 3.8");
    }
    expect(longInfo.report.blocks.size() == shortInfo.report.blocks.size(), "The long file-info variant did not frame like its short counterpart");

    const auto shortFonts = shortInfo.document->getOthers()->getArray<FontDefinition>(musx::dom::SCORE_PARTID);
    const auto longFonts = longInfo.document->getOthers()->getArray<FontDefinition>(musx::dom::SCORE_PARTID);
    expect(!shortFonts.empty() && shortFonts.size() == longFonts.size(), "The two file-info variants disagree about their font table");
    // Finale 97 is internal 3.8, so its font records carry the header incidence.
    expect(shortFonts[0]->charsetBank == FontDefinition::CharacterSetBank::MacOS, "Finale 97 font character set bank was not recovered");
}

// The 2007 encoding is a separate pathway: class-identified, length-governed records rather
// than fixed 16-byte rows. Nothing else in the suite exercises it.
void testClassRecordEra()
{
    const auto result =
        Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F2012/F2012-upstem-flags.mus");
    expect(result.report.formatEpoch == FormatEpoch::ZlibLegacy, "The Finale 2012 fixture was not classified as zlib era");
    expect(result.report.sourceVersion && result.report.sourceVersion->major == finale_mus_reader::versions::finale2012.major,
        "Finale 2012 should record internal major version 17");

    using musx::dom::others::FontDefinition;
    const auto fonts = result.document->getOthers()->getArray<FontDefinition>(musx::dom::SCORE_PARTID);
    expect(fonts.size() == 16, "The zlib-era font table size is incorrect");
    const auto maestro = result.document->getOthers()->get<FontDefinition>(musx::dom::SCORE_PARTID, 0);
    expect(maestro && maestro->name == "Maestro", "A zlib-era font name was not recovered");
    // The character set encoding survived the 2007 serialization change unchanged.
    expect(maestro->charsetBank == FontDefinition::CharacterSetBank::MacOS && maestro->charsetVal == 0xfff && maestro->calcIsSymbolFont(),
        "The zlib-era character set was not decoded like the earlier eras");
}

// Most Finale 2007 documents are big-endian, and every numeric field of the class-record
// encoding follows that byte order, including the payload length in the record header.
void testBigEndianClassRecords()
{
    const auto result =
        Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F2007/F2007-lyric-hyphens.mus");
    expect(result.report.formatEpoch == FormatEpoch::ZlibLegacy, "The Finale 2007 fixture was not classified as zlib era");
    expect(result.report.byteOrder == ByteOrder::BigEndian, "The Finale 2007 fixture should be big-endian");
    expect(result.report.sourceVersion && result.report.sourceVersion->major == finale_mus_reader::versions::finale2007.major,
        "Finale 2007 should record internal major version 12");

    using musx::dom::others::FontDefinition;
    const auto fonts = result.document->getOthers()->getArray<FontDefinition>(musx::dom::SCORE_PARTID);
    // Nine source definitions plus the two typefaces needed by the pre-2009 categories.
    expect(fonts.size() == 11, "The big-endian font table size is incorrect: " + std::to_string(fonts.size()));
    const auto maestro = result.document->getOthers()->get<FontDefinition>(musx::dom::SCORE_PARTID, 0);
    expect(maestro && maestro->name == "Maestro" && maestro->charsetVal == 0xfff, "A big-endian class record was not decoded");
    expect(result.document->getOthers()->get<FontDefinition>(musx::dom::SCORE_PARTID, 5)->name == "Maestro Percussion",
        "A long name in a big-endian class record was not decoded");
}

// Finale 2006 is the last release to write ETF, but it did not begin mixing the later
// class-identified records into its blocks: the fixed-row model is unchanged.
void testFinale2006RemainsFixedRow()
{
    const auto result =
        Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F2006/F2006-single-title.mus");
    expect(result.report.formatEpoch == FormatEpoch::DclLegacy, "The Finale 2006 fixture was not classified as DCL");
    expect(field(result, "options.musicSpacing.minWidth").origin == ValueOrigin::LegacyMus,
        "Finale 2006 music spacing was not recovered through the fixed-row path");
    // Nine source definitions plus the two typefaces needed by the pre-2009 categories.
    expect(result.document->getOthers()->getArray<musx::dom::others::FontDefinition>(musx::dom::SCORE_PARTID).size() == 11,
        "The Finale 2006 font table size is incorrect: "
            + std::to_string(result.document->getOthers()->getArray<musx::dom::others::FontDefinition>(musx::dom::SCORE_PARTID).size()));
    // The fifth block is present and empty, which is what distinguishes this era.
    expect(result.report.blocks.size() == 5, "Finale 2006 should carry a fifth block");
    expect(result.report.blocks.back().decodedSize == 0, "The Finale 2006 fifth block should be empty");
}

void testControlledDclVersions()
{
    // The embedded Enigma version, decoded from the last-saver tuple. Finale's internal
    // majors run 3 for the 3.x line and Finale 97, then 5 for Finale 2000 onward.
    struct Expected
    {
        std::string_view version;
        std::string_view savingProduct;
        std::uint8_t major;
        std::uint8_t minor;
        std::uint8_t maint;
        std::uint8_t build;
    };
    const std::array<Expected, 4> versions{
        {{"F2002", "2002", 7, 0, 1, 2}, {"F2003", "2003", 8, 0, 0, 5}, {"F2004", "2004b", 9, 0, 0, 58}, {"F2005", "2005", 10, 0, 0, 10}}};
    for (const auto& [version, savingProduct, major, minor, maint, build] : versions) {
        const auto path = std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence" / version / (std::string(version) + "-baseline.mus");
        const auto result = Reader::readWithReport<TestXmlDocument>(path);
        expect(result.report.formatEpoch == FormatEpoch::DclLegacy, std::string(version) + " fixture was not classified as DCL");
        expect(result.report.byteOrder == ByteOrder::BigEndian, std::string(version) + " fixture byte order was not recovered");
        expect(result.report.sourcePlatform == SourcePlatform::MacOS, std::string(version) + " fixture platform was not recovered");
        expect(result.report.savingProduct == savingProduct, std::string(version) + " saving product was not recovered");
        expect(result.report.sourceVersion && result.report.sourceVersion->major == major && result.report.sourceVersion->minor == minor
                   && result.report.sourceVersion->maint == maint && result.report.sourceVersion->build == build,
            std::string(version) + " embedded Finale version was not recovered");
        expect(field(result, "options.musicSpacing.minWidth").origin == ValueOrigin::LegacyMus,
            std::string(version) + " music spacing options were not recovered");
        expectNoScoreContent(result);
    }
}

void testUncompressedEpochAndOverlays()
{
    const auto result = Reader::readWithReport<TestXmlDocument>(makeUncompressedMus());
    expect(result.report.formatEpoch == FormatEpoch::UncompressedLegacy, "Synthetic Finale 2000 file was not classified as uncompressed legacy");
    expect(result.report.byteOrder == ByteOrder::LittleEndian, "Synthetic Windows byte order was not detected");
    expect(result.report.sourcePlatform == SourcePlatform::Windows, "Synthetic Windows platform was not recovered");
    expect(result.report.defaultsPlatform == SourcePlatform::Windows, "Windows-sourced file was not seeded from the Windows baseline");
    // The tuple is stored in the file's byte order, so a little-endian file holds it
    // reversed. Reading it in file order would report the build as the major version.
    expect(result.report.sourceVersion && result.report.sourceVersion->major == 12 && result.report.sourceVersion->minor == 3
               && result.report.sourceVersion->maint == 4 && result.report.sourceVersion->build == 5,
        "Little-endian header version was not decoded in the file's byte order");
    const auto layer = result.document->getOthers()->get<musx::dom::others::LayerAttributes>(musx::dom::SCORE_PARTID, 3);
    expect(layer && layer->restOffset == -14, "Legacy layer attribute overlay failed");
    const auto spacing = result.document->getOptions()->get<musx::dom::options::MusicSpacingOptions>();
    expect(spacing->minWidth == 361 && spacing->maxWidth == 1801, "Uncompressed music spacing overlay failed");
    expect(field(result, "others.layerAtts[3].restOffset").origin == ValueOrigin::LegacyMus, "Layer overlay was not reported as recovered");
    const auto fallbackText =
        result.document->getOptions()->get<musx::dom::options::FontOptions>()->getFontInfo(musx::dom::options::FontOptions::FontType::TextBlock);
    const auto fallbackDefinition =
        result.document->getOthers()->get<musx::dom::others::FontDefinition>(musx::dom::SCORE_PARTID, fallbackText->fontId);
    // This target recovers no font definitions of its own, so every one below was introduced
    // from the Windows reference. Cmpers 0 and 1 both hold the music font: 0 is the
    // default-music-font sentinel, materialized so that anything storing 0 resolves, and 1 makes
    // the same typeface addressable concretely, since matching never selects 0.
    const auto zeroDefinition = result.document->getOthers()->get<musx::dom::others::FontDefinition>(musx::dom::SCORE_PARTID, 0);
    expect(static_cast<bool>(zeroDefinition), "A document referencing font id 0 was left with no definition at 0");
    expect(fallbackText->fontId != 0 && fallbackDefinition && fallbackDefinition->getCmper() == fallbackText->fontId
               && fallbackDefinition->name == "Times New Roman"
               && fallbackDefinition->charsetBank == musx::dom::others::FontDefinition::CharacterSetBank::Windows && fallbackDefinition->pitch == 2,
        "A cloned Windows fallback font retained its reference cmper or lost its definition");
    expectCompleteOptionsPool(result);
    expectNoScoreContent(result);
}

// Files older than the ENIGMA signature open with a plain-text product banner and
// reserve the same 0x200 header; the body's second word repeats the body offset.
std::vector<std::uint8_t> makeCodaBannerMus()
{
    std::vector<std::uint8_t> result(0x220);
    constexpr std::string_view banner = "Finale(TM) 2.6 Copyright 1987 by Coda. All rights reserved.";
    writeFixed(result, 0, banner, banner.size());
    // Files of this era carry this constant pair and nothing else in the 0x60-0x200
    // region; there is no version tuple to recover.
    result[0x80] = 0x01;
    result[0x81] = 0x03;
    result[0x203] = 0x2b;
    result[0x206] = 0x02;
    return result;
}

void testCodaBannerEpoch()
{
    const auto result = Reader::readWithReport<TestXmlDocument>(makeCodaBannerMus());
    expect(result.report.formatEpoch == FormatEpoch::CodaBanner, "Synthetic Coda-banner file was not classified");
    expect(result.report.savingProduct == "2.6", "Coda-banner product was not recovered from the banner text");
    expect(result.report.sourceVersion
               && finale_mus_reader::VersionBound{result.report.sourceVersion->major, result.report.sourceVersion->minor}
                      == finale_mus_reader::versions::finale2_6,
        "Coda-banner version was not recovered from the banner text");
    expect(result.report.sourceVersion->raw == 0, "A Coda-banner version was reported as though it came from a header tuple");
    expect(result.report.byteOrder == ByteOrder::BigEndian, "Coda-banner byte order was not classified");
    // This era records its version only in the product banner, and that names the
    // application that wrote the file, so it belongs to the last saver. Nothing identifies
    // the creator, so that block stays empty rather than repeating a version it never held.
    const auto& header = *result.document->getHeader();
    expect(header.modified.finaleVersion.major == finale_mus_reader::versions::finale2_6.major
               && header.modified.finaleVersion.minor == finale_mus_reader::versions::finale2_6.minor,
        "The Coda-banner version did not reach the document header");
    expect(header.created.finaleVersion.major == 0, "A creator version was invented for a file that records none");
    expect(result.report.sourcePlatform == SourcePlatform::Unknown && result.report.defaultsPlatform == SourcePlatform::MacOS,
        "An unknown source platform did not fall back to the macOS baseline");
    expect(field(result, "options.musicSpacing.minWidth").origin == ValueOrigin::LegacyBehavior,
        "The Coda-banner minimum-width behavior was not applied");
    const auto fonts = result.document->getOptions()->get<musx::dom::options::FontOptions>();
    expect(fonts && fonts->fontOptions.size() == 45, "Unsupported Coda-banner FontOptions were not safely completed from the baseline");
    expectCompleteOptionsPool(result);
    expectNoScoreContent(result);
}

void testZlibEpoch()
{
    const auto result = Reader::readWithReport<TestXmlDocument>(makeZlibMus());
    expect(result.report.formatEpoch == FormatEpoch::ZlibLegacy, "Synthetic Finale 2012 file was not classified as zlib legacy");
    expect(result.report.blocks.size() == 5, "Synthetic zlib block count is incorrect");
    expect(result.report.blocks.front().decodedSize == 3, "Synthetic zlib decoded size is incorrect");
    expect(field(result, "options.musicSpacing.minWidth").origin == ValueOrigin::LegacyBehavior,
        "Selector-94-absent music spacing behavior was not applied");
    expectNoScoreContent(result);
}

// A document that embeds a graphic puts its bytes in one of the two terminal blocks, stored
// rather than deflated. Inflating it used to fail and abandon every block already decoded,
// so a score with a picture in it lost its entire options pool. Both terminal types are
// exercised because a file may use either.
void testEmbeddedGraphics()
{
    // A PNG signature and an EPS header: the two shapes embedded graphics take.
    for (const auto& [terminalType, graphic] : {std::pair<std::uint16_t, std::string_view>{std::uint16_t(0x0013), "\x89PNG\r\n\x1a\n padding"},
             std::pair<std::uint16_t, std::string_view>{std::uint16_t(0x001d), "%!PS-Adobe-3.0 EPSF-3.0"}}) {
        const auto result = Reader::readWithReport<TestXmlDocument>(makeZlibMusWithGraphic(terminalType, graphic));
        expect(result.report.formatEpoch == FormatEpoch::ZlibLegacy, "A file with an embedded graphic was not classified as zlib legacy");
        // The point of the fix: the four real blocks survive rather than being discarded.
        expect(result.report.blocks.size() == 5, "A stored terminal block cost the file its already-decoded blocks");
        expect(result.report.blocks.front().decodedSize == 3, "The record block was not decoded alongside a stored terminal block");

        const auto& terminal = result.report.blocks.back();
        expect(terminal.type == terminalType && terminal.stored, "The graphics block was not reported as stored");
        const auto expectedStoredSize = graphic.size() + (terminalType == 0x0013 ? 11 : 0);
        expect(terminal.decodedSize == expectedStoredSize, "A stored block's payload begins after six header bytes, not ten");
        expect(!terminal.checksumPresent, "A stored block was reported as carrying a checksum");
        if (terminalType == 0x0013) {
            const auto& embedded = result.document->getEmbeddedGraphics();
            expect(embedded.size() == 1 && embedded.at(1).bytes.size() == graphic.size() && embedded.at(1).extension == "png",
                "A framed embedded graphic was not installed in the musxdom document");
        } else {
            expect(result.document->getEmbeddedGraphics().empty(), "An unrelated stored terminal block was mistaken for embedded graphics");
        }
        expectCompleteOptionsPool(result);
        expectNoScoreContent(result);
    }
}

// The Coda-banner era in both byte orders. Its Windows documents are little-endian, which the
// container learns from the first pool's page-size word, and the only real
// specimens are Coda's own installer templates, which cannot be committed here. A synthetic
// pair covers the path and keeps the two orders honest against each other.
void testCodaBannerByteOrder()
{
    using ClefOptions = musx::dom::options::ClefOptions;
    struct Case
    {
        ByteOrder byteOrder;
        const char* product;
        SourcePlatform platform;
        SourcePlatform baseline;
    };
    // A `PC` product states Windows, which is little-endian and seeds from the Windows
    // baseline. A numeric product says nothing about platform, so it stays Unknown and falls
    // back to macOS, exactly as before this era gained a second platform.
    const std::array<Case, 2> cases{{{ByteOrder::LittleEndian, "PC 1.0+", SourcePlatform::Windows, SourcePlatform::Windows},
        {ByteOrder::BigEndian, "2.6", SourcePlatform::Unknown, SourcePlatform::MacOS}}};

    for (const auto& testCase : cases) {
        const auto result = Reader::readWithReport<TestXmlDocument>(makeCodaBannerMus(testCase.byteOrder, testCase.product));
        const std::string what = std::string("Coda-banner ") + testCase.product;
        expect(result.report.formatEpoch == FormatEpoch::CodaBanner, what + " was not classified as the Coda-banner era");
        expect(result.report.byteOrder == testCase.byteOrder, what + " byte order was not taken from the pool prologue");
        expect(result.report.sourcePlatform == testCase.platform && result.report.defaultsPlatform == testCase.baseline,
            what + " selected the wrong platform or baseline");
        expect(result.report.savingProduct == testCase.product, what + " did not report its product");

        // Both orders must yield the same logical document, which is what shows the order is
        // being applied rather than the bytes merely being accepted.
        const auto clefs = result.document->getOptions()->get<ClefOptions>();
        expect(clefs && clefs->clefDefs.size() == 18, what + " did not complete the clef collection");
        expect(clefs->getClefDef(0)->middleCPos == -10 && clefs->getClefDef(0)->clefChar == 38 && clefs->getClefDef(0)->staffPosition == -6,
            what + " did not recover its first clef");
        expect(clefs->getClefDef(4)->clefChar == 214 && clefs->getClefDef(7)->staffPosition == -4, what + " did not recover its later clefs");
        expect(field(result, "options.clefOptions.clefDefs[7].middleCPos").origin == ValueOrigin::LegacyMus,
            what + " reported a stored clef as synthesized");
        expect(clefs->clefChangePercent == 75 && clefs->clefChangeOffset == -12 && clefs->clefFrontSepar == 24,
            what + " did not recover its clef scalars");
        // The era has no such option, so this is behavior rather than a record or a default.
        expect(clefs->cautionaryClefChanges && field(result, "options.clefOptions.cautionaryClefChanges").origin == ValueOrigin::LegacyBehavior,
            what + " lost the era's unconditional courtesy clef");
        // These three do not exist in the era either, but the baseline already carries what
        // Finale 27 produces for them, so they are ordinary defaults.
        expect(clefs->clefKeySepar == 0 && clefs->clefTimeSepar == 0 && !clefs->showClefFirstSystemOnly,
            what + " invented a value for an option the era does not have");
        expectNoScoreContent(result);
    }
}

void testMalformedInput()
{
    bool rejected = false;
    try {
        static_cast<void>(Reader::readWithReport<TestXmlDocument>(std::vector<std::uint8_t>{1, 2, 3, 4}));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    expect(rejected, "Arbitrary input was accepted as a MUS file");
    // Error is the one level with an absolute meaning, so it must not appear when a
    // document was produced. Every other level accompanies a usable result.
    const auto good =
        Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F2012/F2012-upstem-flags.mus");
    expect(good.document != nullptr, "A known-good fixture failed to import");
    expect(!hasDiagnostic(good.report, musx::util::Logger::LogLevel::Error), "A successful import reported an error-level diagnostic");
}

void testCodaBlankShapeDefinition()
{
    using namespace musx::dom;
    const auto blank = Reader::readWithReport<TestXmlDocument>(makeCodaBannerMus(ByteOrder::LittleEndian, "PC 1.0+", true));
    const auto blankShape = blank.document->getOthers()->get<others::ShapeDef>(SCORE_PARTID, 19);
    expect(blankShape && blankShape->instructionList == 19 && blankShape->dataList == 19 && blankShape->recognize() == KnownShapeDefType::Blank,
        "An intentionally blank Coda shape was not recognized as Blank without losing its references");
    expect(field(blank, "others.shapeDef[19].instructionList").origin == ValueOrigin::LegacyMus
               && field(blank, "others.shapeDef[19].instructionList").rawValue == 19
               && field(blank, "others.shapeDef[19].dataList").origin == ValueOrigin::LegacyMus
               && field(blank, "others.shapeDef[19].dataList").rawValue == 19,
        "A blank Coda shape did not retain its source references in the report");
}

// A clef and a stem connection both store their character as a byte in the encoding of the
// font that draws them, not as a code point. In a music font the byte is a glyph number and
// survives unchanged, so the distinction is only visible once one of them is pointed at a text
// font.
void testLegacyCharacterFonts()
{
    using namespace musx::dom;

    const auto source =
        Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F2002/F2002-clef-stem-font.mus");

    // Font 9 is Arial, a Mac-bank text font, so Mac Roman 199 is the code point 171.
    const auto clefs = source.document->getOptions()->get<options::ClefOptions>();
    REQUIRE(clefs != nullptr);
    REQUIRE(clefs->clefDefs.size() > 4);
    const auto& owned = clefs->clefDefs[4];
    CHECK(owned->useOwnFont);
    REQUIRE(owned->font != nullptr);
    CHECK(owned->font->fontId == 9);
    CHECK(owned->font->fontSize == 23);
    CHECK(owned->font->bold);
    CHECK(owned->font->italic);
    CHECK(owned->clefChar == U'\u00ab');
    // The report keeps the byte the source stored, which is the only place it survives.
    expect(field(source, "options.clefOptions.clefDefs[4].clefChar").rawValue == 199,
        "The clef report did not keep the byte the source actually stored");

    const auto stems = source.document->getOptions()->get<options::StemOptions>();
    REQUIRE(stems != nullptr);
    REQUIRE(stems->stemConnections.size() > 1);
    // Connection 0 names font 0, the document's default music font: a symbol font, whose byte
    // is a glyph number and must come through untouched.
    CHECK(stems->stemConnections[0]->fontId == 0);
    CHECK(stems->stemConnections[0]->symbol == 192);
    CHECK(stems->stemConnections[1]->fontId == 9);
    CHECK(stems->stemConnections[1]->symbol == U'\u00ab');
    CHECK(stems->stemConnections[1]->upStemVert == 320);
    CHECK(stems->stemConnections[1]->downStemHorz == -448);
    expect(field(source, "options.stemOptions.stemConnections[1].symbol").rawValue == 199,
        "The stem report did not keep the byte the source actually stored");

    // The flat symbol insert of Text Options, the third record type that stores a bare
    // character. The four inserts beside it are the control: they keep font 0, so their bytes
    // are glyph numbers and must read back exactly as stored.
    const auto texts = source.document->getOptions()->get<options::TextOptions>();
    REQUIRE(texts != nullptr);
    const auto flat = texts->symbolInserts.find(options::AccidentalInsertSymbolType::Flat);
    REQUIRE(flat != texts->symbolInserts.end());
    REQUIRE(flat->second != nullptr);
    REQUIRE(flat->second->symFont != nullptr);
    CHECK(flat->second->symFont->fontId == 9);
    CHECK(flat->second->symChar == U'\u00ab');
    expect(field(source, "options.textOptions.symbolInserts[flat].symChar").rawValue == 199,
        "The symbol-insert report did not keep the byte the source actually stored");
    const auto natural = texts->symbolInserts.find(options::AccidentalInsertSymbolType::Natural);
    REQUIRE(natural != texts->symbolInserts.end());
    REQUIRE(natural->second != nullptr);
    REQUIRE(natural->second->symFont != nullptr);
    CHECK(natural->second->symFont->fontId == 0);
    CHECK(natural->second->symChar == 110);

    // Its parent, which differs only by the three edits under test, has neither.
    const auto parent =
        Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F2002/F2002-empty.mus");
    const auto parentStems = parent.document->getOptions()->get<options::StemOptions>();
    REQUIRE(parentStems != nullptr);
    CHECK(parentStems->stemConnections.size() == 1);
}

} // namespace

TEST_CASE("Controlled DCL file", "[reader]")
{
    testControlledDclFile();
}

TEST_CASE("Legacy character fonts", "[reader]")
{
    testLegacyCharacterFonts();
}

TEST_CASE("Uncompressed FontOptions epoch gate", "[reader]")
{
    testUncompressedFontOptionsEpochGate();
}
TEST_CASE("Uncompressed fixtures", "[reader]")
{
    testUncompressedFixtures();
}
TEST_CASE("Class record era", "[reader]")
{
    testClassRecordEra();
}
TEST_CASE("Big-endian class records", "[reader]")
{
    testBigEndianClassRecords();
}
TEST_CASE("Finale 2006 remains fixed-row", "[reader]")
{
    testFinale2006RemainsFixedRow();
}

TEST_CASE("Independent imported documents", "[reader]")
{
    testIndependentImportedDocuments();
}
TEST_CASE("Controlled DCL versions", "[reader]")
{
    testControlledDclVersions();
}
TEST_CASE("Uncompressed epoch and overlays", "[reader]")
{
    testUncompressedEpochAndOverlays();
}
TEST_CASE("Coda banner epoch", "[reader]")
{
    testCodaBannerEpoch();
}
TEST_CASE("Zlib epoch", "[reader]")
{
    testZlibEpoch();
}
TEST_CASE("Embedded graphics", "[reader]")
{
    testEmbeddedGraphics();
}
// A Coda-banner document's pools, built directly so the walk can be given an empty one. The
// era chains pools nose to tail: an 8-byte prologue of page count and page size, then that
// many 512-byte pages, and the chain ends where the next four bytes are not the page size.
std::vector<std::uint8_t> makeCodaBannerPools(const std::vector<std::uint32_t>& pageCounts)
{
    std::vector<std::uint8_t> result(0x200, 0);
    const std::string banner = "Finale(TM) 2.6 Copyright 1987 by Coda. All rights reserved.";
    writeFixed(result, 0, banner, banner.size());
    result[0x80] = 0x01;
    result[0x81] = 0x03;
    for (const auto pages : pageCounts) {
        write32(result, pages, ByteOrder::BigEndian);
        write32(result, 0x200, ByteOrder::BigEndian);
        result.resize(result.size() + std::size_t{pages} * 0x200, 0);
    }
    // The text region that follows the last pool. Its first four bytes are a chunk length, so
    // they end the walk on their own without a terminator of the era's making.
    const std::string text = "^text()";
    write32(result, static_cast<std::uint32_t>(text.size()), ByteOrder::BigEndian);
    result.insert(result.end(), text.begin(), text.end());
    return result;
}

// Every persisted LayerAttributes member, paired with the flag bit that supplies it. The
// rest offset has no bit and is checked separately.
struct LayerFlagMember
{
    const char* member;
    bool musx::dom::others::LayerAttributes::* field;
    std::uint16_t mask;
};

const LayerFlagMember layerFlagMembers[] = {
    {"ignoreHiddenLayers", &musx::dom::others::LayerAttributes::ignoreHiddenLayers, 0x0001},
    {"hideLayer", &musx::dom::others::LayerAttributes::hideLayer, 0x0002},
    {"freezTiesToStems", &musx::dom::others::LayerAttributes::freezTiesToStems, 0x0080},
    {"onlyIfOtherLayersHaveNotes", &musx::dom::others::LayerAttributes::onlyIfOtherLayersHaveNotes, 0x0100},
    {"useRestOffset", &musx::dom::others::LayerAttributes::useRestOffset, 0x0200},
    {"freezeStemsUp", &musx::dom::others::LayerAttributes::freezeStemsUp, 0x0400},
    {"freezeLayer", &musx::dom::others::LayerAttributes::freezeLayer, 0x0800},
    {"playback", &musx::dom::others::LayerAttributes::playback, 0x1000},
    {"affectSpacing", &musx::dom::others::LayerAttributes::affectSpacing, 0x2000},
    {"ignoreHiddenNotesOnly", &musx::dom::others::LayerAttributes::ignoreHiddenNotesOnly, 0x4000},
};

// Asserts one layer against the record it came from, and asserts that the report carries an
// entry for every member whatever this file supplied. The second half is what keeps a member
// from quietly leaving the recovery model: a field with no entry is invisible to anything
// that walks the report, and its absence would otherwise look exactly like a default.
// Members whose origin differs from the rest of their layer.
//
// A layer with no record reaches ignoreHiddenLayers and hideLayer without asserting anything,
// because the baseline already holds false for both, so those stay Finale27Default while the
// members that disagree become LegacyBehavior.
const std::map<std::string, ValueOrigin> noRecordOrigins = {{"ignoreHiddenLayers", ValueOrigin::Finale27Default},
    {"hideLayer", ValueOrigin::Finale27Default}, {"playback", ValueOrigin::Finale27Default}, {"affectSpacing", ValueOrigin::Finale27Default}};

// Playback and music spacing are decided by the era below Finale 2002 whatever the file stores,
// but the baseline already holds the value the era implies, so nothing is asserted for them.
const std::map<std::string, ValueOrigin> preFinale2002Origins = {
    {"playback", ValueOrigin::Finale27Default}, {"affectSpacing", ValueOrigin::Finale27Default}};

void expectLayer(const ImportResult& result, musx::dom::Cmper cmper, int restOffset, std::uint16_t flags, ValueOrigin origin,
    const std::string& label, const std::map<std::string, ValueOrigin>& overrides = {})
{
    const auto layer = result.document->getOthers()->get<musx::dom::others::LayerAttributes>(musx::dom::SCORE_PARTID, cmper);
    const auto where = label + " layer " + std::to_string(cmper);
    expect(static_cast<bool>(layer), where + " is missing");
    expect(layer->restOffset == restOffset, where + " rest offset is wrong");
    expect(field(result, "others.layerAtts[" + std::to_string(cmper) + "].restOffset").origin == origin, where + " rest offset origin is wrong");
    for (const auto& flag : layerFlagMembers) {
        const bool expected = (flags & flag.mask) != 0;
        expect(layer.get()->*flag.field == expected, where + " " + flag.member + " is wrong");
        const auto found = overrides.find(flag.member);
        const auto expectedOrigin = found != overrides.end() ? found->second : origin;
        expect(field(result, "others.layerAtts[" + std::to_string(cmper) + "]." + flag.member).origin == expectedOrigin,
            where + " " + flag.member + " origin is wrong");
    }
}

// One fixture per claimed physical layout and byte order, plus the era that stores no record
// at all. The flag word is the whole boolean surface of the class, so a layout that reached
// the wrong word would show up as ten wrong members rather than one.
void testLayerAttributes()
{
    const auto read = [](const char* relative) {
        return Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / relative);
    };

    // Fixed rows, uncompressed, big-endian. The four flags of the original layer dialog.
    // Finale 97 has no playback or music-spacing setting, so both bits are clear in the file and
    // the era supplies them instead: 0x0f80 becomes 0x3f80.
    const auto f97 = read("evidence/F97/Fin97-baseline.mus");
    expectLayer(f97, 0, 4, 0x3f80, ValueOrigin::LegacyMus, "Finale 97", preFinale2002Origins);
    expectLayer(f97, 1, -4, 0x3b80, ValueOrigin::LegacyMus, "Finale 97", preFinale2002Origins);
    expectLayer(f97, 2, 0, 0x3000, ValueOrigin::LegacyMus, "Finale 97", preFinale2002Origins);
    expectLayer(f97, 3, 0, 0x3000, ValueOrigin::LegacyMus, "Finale 97", preFinale2002Origins);

    // Fixed rows, DCL, big-endian. Playback, spacing, and the hidden-notes test are set here
    // and clear in every earlier release.
    const auto f2002 = read("evidence/F2002/F2002-baseline.mus");
    expectLayer(f2002, 0, 6, 0x7f80, ValueOrigin::LegacyMus, "Finale 2002");
    expectLayer(f2002, 1, -6, 0x7b80, ValueOrigin::LegacyMus, "Finale 2002");
    expectLayer(f2002, 2, 0, 0x3000, ValueOrigin::LegacyMus, "Finale 2002");
    expectLayer(f2002, 3, 0, 0x3000, ValueOrigin::LegacyMus, "Finale 2002");

    // Class records, both byte orders. The payload keeps the six-word stream the rows carried,
    // so the flag word sits at byte offset 10.
    const auto f2007 = read("evidence/F2007/F2007-lyric-hyphens.mus");
    expect(f2007.report.byteOrder == ByteOrder::BigEndian, "The Finale 2007 fixture is not big-endian");
    expectLayer(f2007, 0, 6, 0x7f80, ValueOrigin::LegacyMus, "Finale 2007");
    expectLayer(f2007, 1, -6, 0x7b80, ValueOrigin::LegacyMus, "Finale 2007");
    expectLayer(f2007, 3, 0, 0x3000, ValueOrigin::LegacyMus, "Finale 2007");

    const auto f2012 = read("evidence/F2012/F2012-baseline.mus");
    expect(f2012.report.byteOrder == ByteOrder::LittleEndian, "The Finale 2012 fixture is not little-endian");
    expectLayer(f2012, 0, 6, 0x7f80, ValueOrigin::LegacyMus, "Finale 2012");
    expectLayer(f2012, 1, -6, 0x7b80, ValueOrigin::LegacyMus, "Finale 2012");
    expectLayer(f2012, 3, 0, 0x3000, ValueOrigin::LegacyMus, "Finale 2012");

    // Finale 3.7.2 writes the record only once a layer setting leaves its default, and it is
    // the same six-word row every later fixed-row release uses. Layers 2 and 3 carry a stored
    // rest offset with the checkbox clear.
    const auto adjrests = read("evidence/F372/F372-layer-adjrests.mus");
    expect(adjrests.report.formatEpoch == FormatEpoch::UncompressedLegacy, "The Finale 3.7.2 layer fixture was not classified as uncompressed");
    expectLayer(adjrests, 0, 2, 0x3200, ValueOrigin::LegacyMus, "Finale 3.7.2 adjusted rests", preFinale2002Origins);
    expectLayer(adjrests, 1, -3, 0x3200, ValueOrigin::LegacyMus, "Finale 3.7.2 adjusted rests", preFinale2002Origins);
    expectLayer(adjrests, 2, 4, 0x3000, ValueOrigin::LegacyMus, "Finale 3.7.2 adjusted rests", preFinale2002Origins);
    expectLayer(adjrests, 3, -5, 0x3000, ValueOrigin::LegacyMus, "Finale 3.7.2 adjusted rests", preFinale2002Origins);

    // Its unmodified sibling stores no record at all. Every layer then takes the era's own
    // behavior -- nothing set but playback and music spacing -- rather than the pinned baseline,
    // which would assert freeze settings and a rest offset the document never had. Layers 2 and
    // 3 reach the same values the baseline already holds, so only 0 and 1 assert anything.
    const auto f372 = read("evidence/F372/F372-baseline.mus");
    expectLayer(f372, 0, 0, 0x3000, ValueOrigin::LegacyBehavior, "Finale 3.7.2", noRecordOrigins);
    expectLayer(f372, 1, 0, 0x3000, ValueOrigin::LegacyBehavior, "Finale 3.7.2", noRecordOrigins);
    // Finale 2002 and later store both bits, so nothing is asserted and every member is read.
    const auto f2002flags = read("evidence/F2002/F2002-baseline.mus");
    expectLayer(f2002flags, 2, 0, 0x3000, ValueOrigin::LegacyMus, "Finale 2002 layer 2");
    expectLayer(f372, 2, 0, 0x3000, ValueOrigin::Finale27Default, "Finale 3.7.2");
    expectLayer(f372, 3, 0, 0x3000, ValueOrigin::Finale27Default, "Finale 3.7.2");

    // The Coda-banner era stores none either, and its epoch is covered rather than gated out,
    // so the same behavior applies there.
    const auto f263 = read("evidence/F263/F263-baseline.mus");
    expect(f263.report.formatEpoch == FormatEpoch::CodaBanner, "The Finale 2.6.3 fixture was not classified as Coda-banner");
    expectLayer(f263, 0, 0, 0x3000, ValueOrigin::LegacyBehavior, "Finale 2.6.3", noRecordOrigins);
    expectLayer(f263, 3, 0, 0x3000, ValueOrigin::Finale27Default, "Finale 2.6.3");

    // The synthetic little-endian fixed-row file, which is the only place the low two bits and
    // the hidden-notes test are set without the dialog's own group beside them.
    // The uncompressed epoch is wholly below the Finale 2002 boundary, so playback and music
    // spacing are supplied here too, whatever the banner version claims.
    const auto synthetic = Reader::readWithReport<TestXmlDocument>(makeUncompressedMus());
    expectLayer(synthetic, 0, 11, 0x3f80, ValueOrigin::LegacyMus, "Synthetic", preFinale2002Origins);
    expectLayer(synthetic, 1, -12, 0x3000, ValueOrigin::LegacyMus, "Synthetic", preFinale2002Origins);
    expectLayer(synthetic, 3, -14, 0x7003, ValueOrigin::LegacyMus, "Synthetic", preFinale2002Origins);
    // The comparator the baseline has no object for is pooled from the record itself, so every
    // member is source-owned and nothing about it can report a retained baseline default.
    expect(synthetic.document->getOthers()->getAllSources<musx::dom::others::LayerAttributes>().size() == musx::dom::MAX_LAYERS + 1,
        "The layer record past the modern range did not reach the document");
    expectLayer(synthetic, musx::dom::MAX_LAYERS, 15, 0x3080, ValueOrigin::LegacyMus, "Synthetic",
        {{"playback", ValueOrigin::LegacyBehavior}, {"affectSpacing", ValueOrigin::LegacyBehavior}});
}

// An empty pool is an ordinary pool, not the end of the chain. The earliest documents store
// nothing in their details pool and still carry an entries pool behind it, so a walk that
// stopped on the first zero-page prologue would never reach it.
void testCodaBannerEmptyPool()
{
    const auto full = finale_mus_reader::container::parse(makeCodaBannerPools({1, 1, 1}).data(), makeCodaBannerPools({1, 1, 1}).size());
    expect(full.formatEpoch == FormatEpoch::CodaBanner && full.blocks.size() == 3, "Three non-empty pools were not all found");

    const auto bytes = makeCodaBannerPools({1, 0, 1});
    const auto parsed = finale_mus_reader::container::parse(bytes.data(), bytes.size());
    expect(parsed.formatEpoch == FormatEpoch::CodaBanner, "A document with an empty pool was not classified as the Coda-banner era");
    expect(parsed.blocks.size() == 3, "An empty second pool ended the walk instead of being reported as empty");
    expect(parsed.blocks[1].data.empty() && parsed.blocks[1].info.decodedSize == 0, "The empty pool was not reported as empty");
    expect(parsed.blocks[2].data.size() == 0x200, "The pool behind the empty one was not reached");
    expect(parsed.blocks[0].info.type == 1 && parsed.blocks[1].info.type == 2 && parsed.blocks[2].info.type == 3,
        "An empty pool did not take its place in the type numbering");

    // A run of empty pools advances by the prologue each time rather than spinning, and the
    // text region still ends the chain behind them.
    const auto sparse = makeCodaBannerPools({0, 0, 0});
    const auto walked = finale_mus_reader::container::parse(sparse.data(), sparse.size());
    expect(walked.blocks.size() == 3, "A run of empty pools was not walked to its end");
}

TEST_CASE("Coda-banner byte order", "[reader]")
{
    testCodaBannerByteOrder();
}

// Recognition separates scores from other files without importing them, and the reader refuses
// exactly what recognition does not accept.
TEST_CASE("Recognition classifies buffers before import", "[reader][recognize]")
{
    using finale_mus_reader::MusRecognition;
    const auto verdictOf = [](const std::vector<std::uint8_t>& bytes) { return finale_mus_reader::recognize(bytes).verdict; };

    const auto score = makeUncompressedMus();
    const auto scoreResult = finale_mus_reader::recognize(score);
    CHECK(scoreResult.verdict == MusRecognition::Candidate);
    CHECK(scoreResult.epoch == FormatEpoch::UncompressedLegacy);
    CHECK(scoreResult.detail.empty());
    CHECK(verdictOf(makeZlibMus()) == MusRecognition::Candidate);
    CHECK(finale_mus_reader::recognize(makeCodaBannerMus(ByteOrder::BigEndian, "2.6")).epoch == FormatEpoch::CodaBanner);

    // A tail shorter than a block header is padding.
    auto padded = score;
    padded.insert(padded.end(), {0, 0, 0});
    CHECK(verdictOf(padded) == MusRecognition::Candidate);

    // A Finale binary without an entries pool is not a score.
    constexpr auto byteOrder = ByteOrder::LittleEndian;
    auto library = makeBanner("2000", "WIN", byteOrder);
    appendUncompressedBlock(library, 1, {}, byteOrder);
    appendUncompressedBlock(library, 2, {}, byteOrder);
    appendUncompressedBlock(library, 4, {}, byteOrder);
    CHECK(verdictOf(library) == MusRecognition::FinaleNonScore);
    bool rejected = false;
    try {
        static_cast<void>(finale_mus_reader::container::parse(library.data(), library.size()));
    } catch (const std::invalid_argument& error) {
        rejected = std::string(error.what()).find("not a Finale MUS score") != std::string::npos;
    }
    CHECK(rejected);

    // A block that claims more bytes than the file holds.
    auto damaged = score;
    damaged[0x202] = 0xff;
    damaged[0x203] = 0xff;
    CHECK(verdictOf(damaged) == MusRecognition::Damaged);

    const auto bytesOf = [](std::string_view text) { return std::vector<std::uint8_t>(text.begin(), text.end()); };
    CHECK(verdictOf(bytesOf("ENIGMA TRANSPORTABLE FILE")) == MusRecognition::FinaleNonScore);
    CHECK(verdictOf(bytesOf(std::string("PK\x03\x04", 4) + std::string(26, '\0') + "mimetypeapplication/vnd.makemusic.notation"))
          == MusRecognition::FinaleNonScore);
    CHECK(verdictOf(bytesOf("[Lesson 1]")) == MusRecognition::FinaleNonScore);
    CHECK(verdictOf(bytesOf("FINALE PERFORMANCE ASSESSMENT")) == MusRecognition::FinaleNonScore);
    CHECK(verdictOf(bytesOf("%PDF-1.4")) == MusRecognition::NotFinale);
    CHECK(verdictOf({}) == MusRecognition::NotFinale);
}

// A signature-bearing file is read in the order its header flags state, and in no other. The
// flags byte at 0x62 carries other bits too, so a big-endian file and a little-endian one differ
// only in bit 0.
TEST_CASE("Stated byte order", "[reader]")
{
    using finale_mus_reader::container::statedByteOrder;
    auto bytes = makeUncompressedMus();
    CHECK(statedByteOrder(bytes.data(), bytes.size()) == ByteOrder::LittleEndian);
    CHECK(finale_mus_reader::container::parse(bytes.data(), bytes.size()).byteOrder == ByteOrder::LittleEndian);

    // The same little-endian framing under a header claiming big-endian is not a document.
    bytes[0x62] |= 0x01;
    CHECK(statedByteOrder(bytes.data(), bytes.size()) == ByteOrder::BigEndian);
    CHECK_THROWS_AS(finale_mus_reader::container::parse(bytes.data(), bytes.size()), std::invalid_argument);

    // A header too short to hold the flags states nothing.
    CHECK_FALSE(statedByteOrder(bytes.data(), 0x62).has_value());

    // The Coda-banner era states its order through the first pool's page size.
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        const auto coda = makeCodaBannerMus(byteOrder, "2.6");
        CHECK(statedByteOrder(coda.data(), coda.size()) == byteOrder);
        // The format flags are a byte, read the same in either order.
        CHECK(finale_mus_reader::container::parse(coda.data(), coda.size()).codaFormatFlags == (byteOrder == ByteOrder::BigEndian ? 0x03 : 0x07));
    }
}
TEST_CASE("Coda-banner empty pool", "[reader]")
{
    testCodaBannerEmptyPool();
}
TEST_CASE("Layer attributes", "[reader]")
{
    testLayerAttributes();
}
TEST_CASE("Malformed input", "[reader]")
{
    testMalformedInput();
}
TEST_CASE("Coda blank shape definition", "[reader]")
{
    testCodaBlankShapeDefinition();
}

// Every banner spelling is recognized through the one parser, so a file that carries the
// Finale 1.0.0 spelling reports a product and a version like any other era. Before the
// parser was unified this file read as an error, because the container and the identity
// code each carried their own copy of the spellings and neither knew the third.
TEST_CASE("Finale 1.0.0 banner spelling", "[banner]")
{
    // `Finale` + the MacRoman trademark sign + a version, terminated by `ENIGA Structures`
    // (sic) where every later era puts a copyright notice.
    std::vector<std::uint8_t> data(0x400, 0);
    const std::string banner = "Finale\xaa 1.0.0 ENIGA Structures Copyright 1987 by Coda.";
    std::copy(banner.begin(), banner.end(), data.begin());
    // A body prologue: the record count, then the body offset itself, which is what
    // confirms the era.
    data[0x203] = 0x01;
    data[0x205] = 0x00;
    data[0x206] = 0x02;

    const auto parsed = finale_mus_reader::banner::parse(data.data(), data.size());
    REQUIRE(parsed.spelling == finale_mus_reader::banner::Spelling::MacTrademark);
    CHECK(parsed.offset == 0);
    CHECK(parsed.product == "1.0.0");

    const auto version = finale_mus_reader::banner::versionFromProduct(parsed.product);
    REQUIRE(version.has_value());
    CHECK(version->major == finale_mus_reader::versions::finale1_0.major);
    CHECK(version->minor == finale_mus_reader::versions::finale1_0.minor);
    CHECK(version->maint == 0);
}

TEST_CASE("The other two banner spellings still parse", "[banner]")
{
    const auto coda = [] {
        std::vector<std::uint8_t> data(0x100, 0);
        const std::string text = "Finale(TM) 2.6 Copyright 1987 by Coda.";
        std::copy(text.begin(), text.end(), data.begin());
        return finale_mus_reader::banner::parse(data.data(), data.size());
    }();
    CHECK(coda.spelling == finale_mus_reader::banner::Spelling::Trademark);
    CHECK(coda.product == "2.6");
    CHECK(coda.isPreSignature());

    // The registered spelling sits at 0x20, after the ENIGMA signature.
    const auto signature = [] {
        std::vector<std::uint8_t> data(0x100, 0);
        const std::string sig = "ENIGMA BINARY FILE";
        std::copy(sig.begin(), sig.end(), data.begin());
        const std::string text = "Finale(R) 2003 Copyright (c) 1987-2002 Coda Music Technology";
        std::copy(text.begin(), text.end(), data.begin() + 0x20);
        return finale_mus_reader::banner::parse(data.data(), data.size());
    }();
    CHECK(signature.spelling == finale_mus_reader::banner::Spelling::Registered);
    CHECK(signature.offset == 0x20);
    CHECK(signature.product == "2003");
    CHECK_FALSE(signature.isPreSignature());
}

TEST_CASE("Sound maps retain complete XML and caller order independently of input buffers", "[reader][soundmaps]")
{
    static unsigned soundmapParseCount;
    soundmapParseCount = 0;
    const auto parser = +[](const char* data, std::size_t size) -> std::unique_ptr<musx::xml::IXmlDocument> {
        ++soundmapParseCount;
        auto xml = std::make_unique<TestXmlDocument>();
        xml->loadFromBuffer(data, size);
        return xml;
    };
    auto resources = [&] {
        std::string first = R"(<Soundmap><Description><Name>Example MIDI</Name><PluginID>TEST</PluginID></Description>
            <Sound id="second" uuid="00000000-0000-0000-0000-000000000002"><PatchMIDICommands><ProgramChange Number="1"/></PatchMIDICommands></Sound>
            <Sound id="first" uuid="00000000-0000-0000-0000-000000000001"><SampleFile>example/sample</SampleFile></Sound>
            <MapSounds><FinInstrument uuid="example"/><Sound ref="first"/></MapSounds><Extension>retained</Extension></Soundmap>)";
        std::string second = R"(<Soundmap><Description><Name>Example Samples</Name></Description></Soundmap>)";
        finale_mus_reader::ReaderOptions options;
        options.soundmapXml = {{reinterpret_cast<const std::uint8_t*>(first.data()), first.size()}, {},
            {reinterpret_cast<const std::uint8_t*>(second.data()), second.size()},
            {reinterpret_cast<const std::uint8_t*>(first.data()), first.size()}};
        return finale_mus_reader::detail::prepareReaderResources(options, parser);
    }();
    REQUIRE(soundmapParseCount == 3);
    REQUIRE(resources.soundmaps.size() == 3);
    const auto root = resources.soundmaps[0]->getRootElement();
    REQUIRE(root->getFirstChildElement("Description")->getFirstChildElement("Name")->getText() == "Example MIDI");
    const auto sound = root->getFirstChildElement("Sound");
    REQUIRE(sound->findAttribute("id")->getValue() == "second");
    REQUIRE(sound->getFirstChildElement("PatchMIDICommands")->getFirstChildElement("ProgramChange")->findAttribute("Number")->getValue() == "1");
    REQUIRE(sound->getNextSibling("Sound")->getFirstChildElement("SampleFile")->getText() == "example/sample");
    REQUIRE(root->getFirstChildElement("MapSounds")->getFirstChildElement("Sound")->findAttribute("ref")->getValue() == "first");
    REQUIRE(root->getFirstChildElement("Extension")->getText() == "retained");
    REQUIRE(
        resources.soundmaps[1]->getRootElement()->getFirstChildElement("Description")->getFirstChildElement("Name")->getText() == "Example Samples");
    REQUIRE(resources.soundmaps[2]->getRootElement()->getFirstChildElement("Description")->getFirstChildElement("Name")->getText() == "Example MIDI");
}

TEST_CASE("Reader creation validates optional sound-map XML", "[reader][soundmaps]")
{
    finale_mus_reader::ReaderOptions options;
    SECTION("No maps")
    {
        REQUIRE_NOTHROW(Reader::create<TestXmlDocument>(options));
    }
    SECTION("Empty buffer")
    {
        options.soundmapXml.emplace_back();
        REQUIRE_NOTHROW(Reader::create<TestXmlDocument>(options));
    }
    SECTION("Incorrect root")
    {
        const std::string xml = "<Other/>";
        options.soundmapXml.emplace_back(reinterpret_cast<const std::uint8_t*>(xml.data()), xml.size());
        REQUIRE_THROWS_AS(Reader::create<TestXmlDocument>(options), std::invalid_argument);
    }
    SECTION("Malformed XML")
    {
        const std::string xml = "<Soundmap><Sound>";
        options.soundmapXml.emplace_back(reinterpret_cast<const std::uint8_t*>(xml.data()), xml.size());
        REQUIRE_THROWS(Reader::create<TestXmlDocument>(options));
    }
    SECTION("Reusable reader owns resource data")
    {
        const auto reader = [] {
            const std::string xml = "<Soundmap><Description><Name>Example</Name></Description></Soundmap>";
            finale_mus_reader::ReaderOptions inputs;
            inputs.soundmapXml.emplace_back(reinterpret_cast<const std::uint8_t*>(xml.data()), xml.size());
            return Reader::create<TestXmlDocument>(inputs);
        }();
        const auto path = std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F2002/F2002-baseline.mus";
        REQUIRE(reader.read(path));
        REQUIRE(reader.read(path));
    }
}

TEST_CASE("Reader creation parses a portable instrument catalog", "[reader][playback]")
{
    const std::string xml = R"(<InstrumentCatalog>
        <Instrument uuid="instrument-1" name="Horn in F" fullName="Horn in F" clef="treble" notation="standard" transposition="-7"/>
    </InstrumentCatalog>)";
    finale_mus_reader::ReaderOptions options;
    options.instrumentCatalogXml = {reinterpret_cast<const std::uint8_t*>(xml.data()), xml.size()};
    const auto parser = +[](const char* data, std::size_t size) -> std::unique_ptr<musx::xml::IXmlDocument> {
        auto result = std::make_unique<TestXmlDocument>();
        result->loadFromBuffer(data, size);
        return result;
    };
    const auto resources = finale_mus_reader::detail::prepareReaderResources(options, parser);
    REQUIRE(resources.instrumentCatalog.size() == 1);
    CHECK(resources.instrumentCatalog.front().uuid == "instrument-1");
    CHECK(resources.instrumentCatalog.front().fullName == "Horn in F");
    CHECK(resources.instrumentCatalog.front().transposition == -7);

    const std::string invalid = "<Soundmap/>";
    options.instrumentCatalogXml = {reinterpret_cast<const std::uint8_t*>(invalid.data()), invalid.size()};
    CHECK_THROWS_AS(finale_mus_reader::detail::prepareReaderResources(options, parser), std::invalid_argument);
}

TEST_CASE("Available sound resources do not select a player for an unassigned legacy route", "[reader][playback]")
{
    const std::string catalog = R"(<InstrumentCatalog>
        <Instrument uuid="instrument-1" name="Flute" fullName="Flute" clef="treble" notation="standard" transposition="0"/>
    </InstrumentCatalog>)";
    const std::string soundmap = R"(<Soundmap><Description><PluginID>GARA</PluginID><Name>Example</Name></Description>
        <Sound id="first" uuid="00000000-0000-0000-0000-000000000001"/>
        <MapSounds><FinInstrument uuid="instrument-1"/><Sound ref="first"/></MapSounds>
    </Soundmap>)";
    finale_mus_reader::ReaderOptions options;
    options.instrumentCatalogXml = {reinterpret_cast<const std::uint8_t*>(catalog.data()), catalog.size()};
    options.soundmapXml.emplace_back(reinterpret_cast<const std::uint8_t*>(soundmap.data()), soundmap.size());
    const auto path = std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / "evidence/F100/F100-quartet.mus";
    const auto result = Reader::readWithReport<TestXmlDocument>(path, options);
    REQUIRE(result.document);
    const auto staff = result.document->getOthers()->get<musx::dom::others::Staff>(0, 1);
    REQUIRE(staff);
    CHECK(musx::dom::others::TextBlock::getText(result.document, staff->fullNameTextId, 0, true) == "Flute");
    CHECK(staff->defaultClef == 0);
    CHECK(staff->notationStyle == musx::dom::others::Staff::NotationStyle::Standard);
    CHECK(staff->calcTranspositionInterval() == std::pair{0, 0});
    const auto route = result.document->getOthers()->get<musx::dom::others::PlaybackRoute>(0, 1);
    const auto name = result.document->getOthers()->get<musx::dom::others::PlaybackRouteName>(0, 1);
    REQUIRE(route);
    REQUIRE(name);
    CHECK(route->soundUuid.empty());
    CHECK(name->name == "Instrument 1");
}

TEST_CASE("Playback plugin names override sound-map descriptions with a fallback", "[reader][soundmaps]")
{
    using finale_mus_reader::playback::resolvePluginName;
    REQUIRE(resolvePluginName("GARA", "Garritan Instruments for Finale") == "ARIA Player");
    REQUIRE(resolvePluginName("GARA", "") == "ARIA Player");
    REQUIRE(resolvePluginName("SMSS", "SmartMusic SoftSynth") == "SmartMusic SoftSynth");
    REQUIRE(resolvePluginName("SMSS", "Alternative SoftSynth Label") == "Alternative SoftSynth Label");
    REQUIRE(resolvePluginName("TEST", "Example Library") == "Example Library");
    REQUIRE(resolvePluginName("", "General MIDI") == "General MIDI");
    REQUIRE(resolvePluginName("gara", "Example Library") == "Example Library");
    REQUIRE(resolvePluginName("TEST", "").empty());
    REQUIRE(resolvePluginName("", "").empty());
}
