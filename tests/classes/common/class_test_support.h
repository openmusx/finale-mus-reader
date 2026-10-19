// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "container/mus_container.h"
#include "finale_mus_reader/reader.h"
#include "import/details.h"
#include "import/options.h"
#include "import/options/test_access.h"
#include "import/others.h"
#include "import/support/embedded_graphics.h"
#include "import/support/legacy_mapping.h"
#include "records/legacy_record_index.h"

#include "musx/musx.h"

#ifndef MUSX_USE_PUGIXML
#define MUSX_USE_PUGIXML
#define FINALE_MUS_READER_MAPPING_UNDEFINE_MUSX_USE_PUGIXML
#endif // !defined(MUSX_USE_PUGIXML)

#include "musx/xml/PugiXmlImpl.h"

#ifdef FINALE_MUS_READER_MAPPING_UNDEFINE_MUSX_USE_PUGIXML
#undef MUSX_USE_PUGIXML
#undef FINALE_MUS_READER_MAPPING_UNDEFINE_MUSX_USE_PUGIXML
#endif // defined(FINALE_MUS_READER_MAPPING_UNDEFINE_MUSX_USE_PUGIXML)

namespace finale_mus_reader_tests {
namespace classes {

using finale_mus_reader::ByteOrder;
using finale_mus_reader::EpochMask;
using finale_mus_reader::FieldInfo;
using finale_mus_reader::FieldMapping;
using finale_mus_reader::FormatEpoch;
using finale_mus_reader::GLOBALS_CMPER;
using finale_mus_reader::ImportReport;
using finale_mus_reader::ImportResult;
using finale_mus_reader::LongWordOrder;
using finale_mus_reader::MappingTable;
using finale_mus_reader::Reader;
using finale_mus_reader::SourceGate;
using finale_mus_reader::SourceProfile;
using finale_mus_reader::SourceVersion;
using finale_mus_reader::TargetKind;
using finale_mus_reader::ValueOrigin;
using finale_mus_reader::records::LegacyRecordIndex;
using Spacing = musx::dom::options::MusicSpacingOptions;
using TestXmlDocument = musx::xml::pugi::Document;

constexpr auto sourceMatchTestProfile = [] {
    SourceProfile result(FormatEpoch::ZlibLegacy);
    result.version = SourceVersion{.major = 15, .minor = 1};
    return result;
}();
static_assert(finale_mus_reader::sourceMatches(sourceMatchTestProfile, EpochMask::Zlib));
static_assert(!finale_mus_reader::sourceMatches(sourceMatchTestProfile, EpochMask::Dcl));
static_assert(finale_mus_reader::sourceAtOrAfter(sourceMatchTestProfile, FormatEpoch::DclLegacy));
static_assert(finale_mus_reader::sourceMatchesVersion(sourceMatchTestProfile, FormatEpoch::ZlibLegacy, {15, 1}));
static_assert(!finale_mus_reader::sourceMatchesVersion(sourceMatchTestProfile, FormatEpoch::ZlibLegacy, {15, 0}));
static_assert(!finale_mus_reader::sourceMatchesVersion(sourceMatchTestProfile, FormatEpoch::DclLegacy, {15, 1}));
static_assert(!finale_mus_reader::sourceMatchesVersion(FormatEpoch::ZlibLegacy, nullptr, FormatEpoch::ZlibLegacy, {15, 1}));
constexpr SourceProfile earlierSourceMatchTestProfile(FormatEpoch::DclLegacy);
static_assert(!finale_mus_reader::sourceAtOrAfter(earlierSourceMatchTestProfile, FormatEpoch::ZlibLegacy));
static_assert(finale_mus_reader::sourceAtOrAfter(sourceMatchTestProfile, FormatEpoch::DclLegacy, finale_mus_reader::versions::finale2007));
static_assert(finale_mus_reader::sourceAtOrAfter(sourceMatchTestProfile, FormatEpoch::ZlibLegacy, {15, 1}));
static_assert(!finale_mus_reader::sourceAtOrAfter(sourceMatchTestProfile, FormatEpoch::ZlibLegacy, finale_mus_reader::versions::finale2011));
static_assert(finale_mus_reader::sourcePredatesVersion(
    FormatEpoch::CodaBanner, nullptr, FormatEpoch::UncompressedLegacy, finale_mus_reader::versions::finale97));
static_assert(!finale_mus_reader::sourcePredatesVersion(
    FormatEpoch::UncompressedLegacy, nullptr, FormatEpoch::UncompressedLegacy, finale_mus_reader::versions::finale97));
static_assert(
    finale_mus_reader::sourceAtOrAfter(FormatEpoch::DclLegacy, nullptr, FormatEpoch::UncompressedLegacy, finale_mus_reader::versions::finale2000));
static_assert(finale_mus_reader::sourcePredatesVersion(FormatEpoch::UncompressedLegacy, &finale_mus_reader::versions::finale3_3_v1_source,
    FormatEpoch::UncompressedLegacy, finale_mus_reader::versions::finale3_3_v2));
static_assert(!finale_mus_reader::sourcePredatesVersion(FormatEpoch::UncompressedLegacy, &finale_mus_reader::versions::finale3_3_v2_source,
    FormatEpoch::UncompressedLegacy, finale_mus_reader::versions::finale3_3_v2));

inline bool sourceAtFinale2007(const SourceProfile& profile)
{
    return finale_mus_reader::sourceAtOrAfter(profile, FormatEpoch::UncompressedLegacy, finale_mus_reader::versions::finale2007);
}

inline bool sourceAtFinale35(const SourceProfile& profile)
{
    return finale_mus_reader::sourceAtOrAfter(profile, FormatEpoch::UncompressedLegacy, finale_mus_reader::versions::finale3_5);
}

inline bool sourceAtFinale351(const SourceProfile& profile)
{
    return finale_mus_reader::sourceAtOrAfter(profile, FormatEpoch::UncompressedLegacy, finale_mus_reader::versions::finale3_5_1);
}

// These tests drive one importer at a time against a synthesized record set, so none of them
// has a source file for the importer to read the header out of.
constexpr std::span<const std::uint8_t> noSource{};

inline void expectMapping(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

inline void expect(bool condition, const std::string& message)
{
    expectMapping(condition, message);
}

inline constexpr std::string_view fixtureLegacySymbolFonts = "Maestro\nPetrucci\nPmusic\nSonata\n";

inline ImportResult readFixture(const std::filesystem::path& relativePath, std::string_view macSymbolFonts = {})
{
    const auto* symbolFontBytes = reinterpret_cast<const std::uint8_t*>(macSymbolFonts.data());
    const finale_mus_reader::ReaderOptions options{std::span<const std::uint8_t>(symbolFontBytes, macSymbolFonts.size())};
    return Reader::readWithReport<TestXmlDocument>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / relativePath, options);
}

/// @brief One synthetic 16-byte legacy row.
struct SyntheticRow
{
    std::uint16_t cmper{};
    const char* tag{};
    std::array<std::int16_t, 6> words{};
};

/// @brief Builds a parsed container holding the given rows as an uncompressed other pool.
inline finale_mus_reader::container::ParsedContainer makeContainer(
    const std::vector<SyntheticRow>& rows, FormatEpoch epoch = FormatEpoch::UncompressedLegacy, ByteOrder byteOrder = ByteOrder::BigEndian)
{
    finale_mus_reader::container::ParsedContainer parsed(epoch);
    parsed.byteOrder = byteOrder;

    finale_mus_reader::container::DecodedBlock block;
    block.info.type = epoch == FormatEpoch::DclLegacy ? 0x000f : 0x0001;
    const auto appendWord = [byteOrder, &block](std::uint16_t value) {
        if (byteOrder == ByteOrder::BigEndian) {
            block.data.push_back(static_cast<std::uint8_t>(value >> 8U));
            block.data.push_back(static_cast<std::uint8_t>(value));
        } else {
            block.data.push_back(static_cast<std::uint8_t>(value));
            block.data.push_back(static_cast<std::uint8_t>(value >> 8U));
        }
    };
    for (const auto& row : rows) {
        appendWord(row.cmper);
        block.data.push_back(static_cast<std::uint8_t>(row.tag[byteOrder == ByteOrder::BigEndian ? 0 : 1]));
        block.data.push_back(static_cast<std::uint8_t>(row.tag[byteOrder == ByteOrder::BigEndian ? 1 : 0]));
        for (const auto word : row.words) {
            appendWord(static_cast<std::uint16_t>(word));
        }
    }
    block.info.decodedSize = block.data.size();
    parsed.blocks.push_back(std::move(block));
    return parsed;
}

/// @brief A bare document carrying one music spacing options instance.
inline musx::dom::DocumentPtr makeDocument(Spacing** instanceOut)
{
    auto session = musx::factory::DocumentFactory::begin();
    auto document = session.getDocument();
    auto spacing = std::make_shared<Spacing>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All);
    spacing->minWidth = 111;
    spacing->maxWidth = 222;
    spacing->referenceDuration = 333;
    spacing->avoidColNotes = false;
    spacing->avoidColStems = true;
    spacing->avoidColUnisons = Spacing::ColUnisonsChoice::DiffNoteheads;
    spacing->ignoreHidden = false;
    *instanceOut = spacing.get();
    document->getOptions()->add(Spacing::XmlNodeName, spacing);
    return std::move(session).finish();
}

struct SyntheticClassRow
{
    std::uint16_t classId{};
    std::vector<std::int16_t> words;
    std::uint16_t cmper{GLOBALS_CMPER};
    std::uint16_t partId{};
    bool hasContinuation{};
    std::vector<std::uint16_t> continuationMasks;
    std::uint16_t continuationTrailerFirst{};
    std::uint16_t continuationTrailerSecond{};

    SyntheticClassRow(std::uint16_t classIdValue, std::vector<std::int16_t> wordValues, std::uint16_t cmperValue = GLOBALS_CMPER,
        std::uint16_t partIdValue = 0, bool continuation = false, std::vector<std::uint16_t> maskValues = {}, std::uint16_t trailerFirst = 0,
        std::uint16_t trailerSecond = 0)
        : classId(classIdValue),
          words(std::move(wordValues)),
          cmper(cmperValue),
          partId(partIdValue),
          hasContinuation(continuation),
          continuationMasks(std::move(maskValues)),
          continuationTrailerFirst(trailerFirst),
          continuationTrailerSecond(trailerSecond)
    {}
};

/// @brief Builds a parsed container holding class-identified records, the 2007+ framing.
inline finale_mus_reader::container::ParsedContainer makeClassContainer(const std::vector<SyntheticClassRow>& rows, ByteOrder byteOrder)
{
    finale_mus_reader::container::ParsedContainer parsed(FormatEpoch::ZlibLegacy);
    parsed.byteOrder = byteOrder;

    finale_mus_reader::container::DecodedBlock block;
    block.info.type = 0x001a;
    const auto push16 = [&](std::uint16_t value) {
        if (byteOrder == ByteOrder::BigEndian) {
            block.data.push_back(static_cast<std::uint8_t>(value >> 8U));
            block.data.push_back(static_cast<std::uint8_t>(value));
        } else {
            block.data.push_back(static_cast<std::uint8_t>(value));
            block.data.push_back(static_cast<std::uint8_t>(value >> 8U));
        }
    };
    for (const auto& row : rows) {
        push16(row.classId);
        push16(row.cmper);
        push16(row.partId);
        const auto length = static_cast<std::uint32_t>(row.words.size() * 2);
        if (byteOrder == ByteOrder::BigEndian) {
            for (int shift = 24; shift >= 0; shift -= 8) {
                block.data.push_back(static_cast<std::uint8_t>(length >> shift));
            }
        } else {
            for (int shift = 0; shift <= 24; shift += 8) {
                block.data.push_back(static_cast<std::uint8_t>(length >> shift));
            }
        }
        for (const auto word : row.words) {
            push16(static_cast<std::uint16_t>(word));
        }
        if (row.hasContinuation) {
            push16(byteOrder == ByteOrder::BigEndian ? static_cast<std::uint16_t>(length >> 16U) : static_cast<std::uint16_t>(length));
            push16(byteOrder == ByteOrder::BigEndian ? static_cast<std::uint16_t>(length) : static_cast<std::uint16_t>(length >> 16U));
            for (std::size_t slot = 0; slot + 2 < row.words.size(); ++slot) {
                push16(slot < row.continuationMasks.size() ? row.continuationMasks[slot] : 0);
            }
        }
        push16(row.continuationTrailerFirst);
        push16(row.continuationTrailerSecond);
    }
    block.info.decodedSize = block.data.size();
    parsed.blocks.push_back(std::move(block));
    return parsed;
}

/// @brief Builds a parsed container holding one class-identified record.
inline finale_mus_reader::container::ParsedContainer makeClassContainer(
    std::uint16_t classId, const std::vector<std::int16_t>& words, ByteOrder byteOrder, std::uint16_t cmper = GLOBALS_CMPER)
{
    return makeClassContainer({SyntheticClassRow{classId, words, cmper}}, byteOrder);
}

inline finale_mus_reader::container::ParsedContainer makeDetailContainer(FormatEpoch epoch, std::uint16_t staffId, std::uint16_t meas,
    const std::vector<std::int16_t>& words, const char* tag = "mg", ByteOrder byteOrder = ByteOrder::BigEndian)
{
    finale_mus_reader::container::ParsedContainer parsed(epoch);
    parsed.byteOrder = byteOrder;
    finale_mus_reader::container::DecodedBlock block;
    block.info.type = epoch == FormatEpoch::DclLegacy ? 0x0010 : 0x0002;
    for (std::size_t at = 0; at < words.size(); at += finale_mus_reader::records::detailWordCount) {
        const auto push16 = [&](std::uint16_t value) {
            if (byteOrder == ByteOrder::BigEndian) {
                block.data.push_back(static_cast<std::uint8_t>(value >> 8U));
                block.data.push_back(static_cast<std::uint8_t>(value));
            } else {
                block.data.push_back(static_cast<std::uint8_t>(value));
                block.data.push_back(static_cast<std::uint8_t>(value >> 8U));
            }
        };
        push16(staffId);
        push16(meas);
        push16(finale_mus_reader::records::packTag(tag));
        for (std::size_t slot = 0; slot < finale_mus_reader::records::detailWordCount; ++slot) {
            push16(static_cast<std::uint16_t>(words[at + slot]));
        }
    }
    block.info.decodedSize = block.data.size();
    parsed.blocks.push_back(std::move(block));
    return parsed;
}

inline finale_mus_reader::container::ParsedContainer makeDetailClassContainer(std::uint16_t staffId, std::uint16_t meas, std::uint16_t partId,
    const std::vector<std::int16_t>& words, ByteOrder byteOrder, std::uint16_t classId = 0x041d, bool hasContinuation = false,
    const std::vector<std::uint16_t>& continuationMasks = {})
{
    auto parsed = makeClassContainer(classId, {}, byteOrder, staffId);
    auto& block = parsed.blocks.front();
    block.info.type = 0x001b;
    block.data.clear();
    const auto push16 = [&](std::uint16_t value) {
        if (byteOrder == ByteOrder::BigEndian) {
            block.data.push_back(static_cast<std::uint8_t>(value >> 8U));
            block.data.push_back(static_cast<std::uint8_t>(value));
        } else {
            block.data.push_back(static_cast<std::uint8_t>(value));
            block.data.push_back(static_cast<std::uint8_t>(value >> 8U));
        }
    };
    push16(classId);
    push16(staffId);
    push16(meas);
    push16(partId);
    const auto length = static_cast<std::uint32_t>(words.size() * 2);
    if (byteOrder == ByteOrder::BigEndian) {
        push16(static_cast<std::uint16_t>(length >> 16U));
    }
    push16(static_cast<std::uint16_t>(length));
    if (byteOrder == ByteOrder::LittleEndian) {
        push16(static_cast<std::uint16_t>(length >> 16U));
    }
    for (const auto word : words) {
        push16(static_cast<std::uint16_t>(word));
    }
    if (hasContinuation) {
        if (byteOrder == ByteOrder::BigEndian) {
            push16(static_cast<std::uint16_t>(length >> 16U));
            push16(static_cast<std::uint16_t>(length));
        } else {
            push16(static_cast<std::uint16_t>(length));
            push16(static_cast<std::uint16_t>(length >> 16U));
        }
        for (std::size_t slot = 0; slot + 2 < words.size(); ++slot) {
            push16(slot < continuationMasks.size() ? continuationMasks[slot] : 0);
        }
    }
    block.data.insert(block.data.end(), 4, 0);
    block.info.decodedSize = block.data.size();
    return parsed;
}

/// @brief A reference document whose ClefOptions has the modern collection and no fonts.
inline musx::dom::DocumentPtr makeClefReferenceDocument()
{
    using ClefOptions = musx::dom::options::ClefOptions;
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    auto options = std::make_shared<ClefOptions>(document);
    options->clefFrontSepar = 24;
    options->cautionaryClefChanges = true;
    for (int index = 0; index < 18; ++index) {
        auto def = std::make_shared<ClefOptions::ClefDef>(options);
        def->middleCPos = -1;
        def->clefChar = static_cast<char32_t>(900 + index);
        options->clefDefs.push_back(std::move(def));
    }
    document->getOptions()->add(ClefOptions::XmlNodeName, options);
    return std::move(session).finish();
}

/// @brief Gives score staves percussion notation and lists them in the Scroll View, as the
/// legacy percussion-layout upgrade requires of a staff it converts.
inline void addScrollViewPercussionStaves(const musx::dom::DocumentPtr& document, std::initializer_list<musx::dom::Cmper> staffIds)
{
    using namespace musx::dom;
    auto inci = Inci(document->getOthers()->getArray<others::StaffUsed>(SCORE_PARTID, BASE_SYSTEM_ID).size());
    for (const auto staffId : staffIds) {
        if (const auto existing = document->getOthers()->get<others::Staff>(SCORE_PARTID, staffId)) {
            const_cast<others::Staff*>(existing.get())->notationStyle = others::Staff::NotationStyle::Percussion;
        } else {
            auto staff = std::make_shared<others::Staff>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, staffId);
            staff->notationStyle = others::Staff::NotationStyle::Percussion;
            document->getOthers()->add(others::Staff::XmlNodeName, std::move(staff));
        }
        auto used = std::make_shared<others::StaffUsed>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, BASE_SYSTEM_ID, inci++);
        used->staffId = StaffCmper(staffId);
        document->getOthers()->add(others::StaffUsed::XmlNodeName, std::move(used));
    }
}

/// @brief Adds a score Staff Style that sets percussion notation.
inline void addPercussionStaffStyle(const musx::dom::DocumentPtr& document, musx::dom::Cmper styleId)
{
    using namespace musx::dom;
    auto style = std::make_shared<others::StaffStyle>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, styleId);
    style->masks = std::make_shared<others::StaffStyle::Masks>(style);
    style->masks->notationStyle = true;
    style->notationStyle = others::Staff::NotationStyle::Percussion;
    document->getOthers()->add(others::StaffStyle::XmlNodeName, std::move(style));
}

inline SourceProfile profileFor(std::uint8_t major, std::uint8_t minor = 0, std::uint8_t maint = 0)
{
    SourceProfile profile(FormatEpoch::UncompressedLegacy);
    profile.byteOrder = ByteOrder::BigEndian;
    SourceVersion version;
    version.major = major;
    version.minor = minor;
    version.maint = maint;
    profile.version = version;
    return profile;
}

// The target is taken by value rather than by const reference so that a literal call site
// does not create a temporary bound to a reference parameter. GCC cannot prove the returned
// reference does not alias such a temporary and rejects the binding under
// -Wdangling-reference.
struct ExpectedReportField
{
    std::string member;
    std::optional<musx::dom::Cmper> cmper;
    std::optional<musx::dom::Inci> inci;
};

inline ExpectedReportField expectedReportField(std::string_view target)
{
    ExpectedReportField result{std::string(target), std::nullopt, std::nullopt};
    if (target.starts_with("options.fontOptions[")) {
        result.member = "fonts[" + std::string(target.substr(std::string_view("options.fontOptions[").size()));
    } else if (target.starts_with("options.fontOptionsPhysical[")) {
        result.member = "physical[" + std::string(target.substr(std::string_view("options.fontOptionsPhysical[").size()));
    } else if (target.starts_with("options.")) {
        const auto dot = target.find('.', std::string_view("options.").size());
        if (dot != std::string_view::npos) {
            result.member = target.substr(dot + 1);
        }
    } else if (const auto open = target.find('['); open != std::string_view::npos) {
        const auto close = target.find(']', open);
        const auto dot = close == std::string_view::npos ? close : target.find('.', close);
        if (close != std::string_view::npos && dot != std::string_view::npos) {
            const auto comma = target.find(',', open);
            const auto cmperEnd = comma == std::string_view::npos || comma > close ? close : comma;
            result.cmper = musx::dom::Cmper(std::stoul(std::string(target.substr(open + 1, cmperEnd - open - 1))));
            if (comma != std::string_view::npos && comma < close) {
                const auto nextComma = target.find(',', comma + 1);
                const auto inciEnd = nextComma == std::string_view::npos || nextComma > close ? close : nextComma;
                result.inci = musx::dom::Inci(std::stol(std::string(target.substr(comma + 1, inciEnd - comma - 1))));
            }
            result.member = target.substr(dot + 1);
        }
    }
    return result;
}

inline const finale_mus_reader::FieldInfo& field(const ImportReport& report, std::string_view target)
{
    const auto expected = expectedReportField(target);
    for (const auto& [instance, fields] : report.fields) {
        if (expected.cmper && instance.cmper1 != expected.cmper) {
            continue;
        }
        if (expected.inci && instance.inci != expected.inci) {
            continue;
        }
        for (const auto& [member, info] : fields) {
            if (member == expected.member) {
                return info;
            }
        }
    }
    std::string message = std::string("Missing mapping report for ").append(target).append("; available members:");
    for (const auto& [instance, fields] : report.fields) {
        static_cast<void>(instance);
        for (const auto& [member, info] : fields) {
            static_cast<void>(info);
            message.append(" ").append(member);
        }
    }
    throw std::runtime_error(std::move(message));
}

inline const finale_mus_reader::FieldInfo& field(const ImportResult& result, std::string_view target)
{
    return field(result.report, target);
}

template <typename Target>
inline const finale_mus_reader::FieldInfo& fieldFor(const ImportReport& report, std::string_view target)
{
    const auto expected = expectedReportField(target);
    for (const auto& [instance, fields] : report.fields) {
        if (instance.classType != std::type_index(typeid(Target))) {
            continue;
        }
        if (expected.cmper && instance.cmper1 != expected.cmper) {
            continue;
        }
        if (expected.inci && instance.inci != expected.inci) {
            continue;
        }
        for (const auto& [member, info] : fields) {
            if (member == expected.member) {
                return info;
            }
        }
    }
    throw std::runtime_error(std::string("Missing mapping report for ").append(target));
}

template <typename Target>
inline const finale_mus_reader::FieldInfo& fieldFor(const ImportResult& result, std::string_view target)
{
    return fieldFor<Target>(result.report, target);
}

/// @brief Whether the report names a target at all, for a field a record is not expected to have.
inline bool fieldPresent(const ImportReport& report, std::string_view target)
{
    const auto expected = expectedReportField(target);
    for (const auto& [instance, fields] : report.fields) {
        if (expected.cmper && instance.cmper1 != expected.cmper) {
            continue;
        }
        for (const auto& [member, info] : fields) {
            (void)info;
            if (member == expected.member) {
                return true;
            }
        }
    }
    return false;
}

template <typename Predicate>
inline bool anyMappingReportedField(const ImportReport& report, Predicate predicate)
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

template <typename Predicate>
inline bool anyReportedField(const ImportReport& report, Predicate predicate)
{
    return anyMappingReportedField(report, std::move(predicate));
}

inline std::size_t reportedFieldCount(const ImportReport& report)
{
    std::size_t result = 0;
    for (const auto& [instance, fields] : report.fields) {
        (void)instance;
        result += fields.size();
    }
    return result;
}

inline MappingTable makeTable(
    const char* prefix, const FieldMapping* fields, std::size_t count, SourceGate sourceApplies = nullptr, EpochMask epochs = EpochMask::FixedRow)
{
    return MappingTable{.reportPrefix = prefix,
        .epochs = epochs,
        .sourceApplies = sourceApplies,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &finale_mus_reader::enumerateOptionsTarget<Spacing>,
        .fields = fields,
        .fieldCount = count};
}

} // namespace classes
} // namespace finale_mus_reader_tests
