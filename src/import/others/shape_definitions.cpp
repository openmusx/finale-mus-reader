// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using ShapeDataTarget = musx::dom::others::ShapeData;
using ShapeDefTarget = musx::dom::others::ShapeDef;
using ShapeInstructionTarget = musx::dom::others::ShapeInstructionList;
using ShapeInstructionType = musx::dom::ShapeDefInstructionType;

constexpr auto shapeDefinitionTag = records::packTag("SD");
constexpr auto earlyShapeInstructionTag = records::packTag("SL");
constexpr auto shapeInstructionTag = records::packTag("sL");
constexpr auto earlyShapeDataTag = records::packTag("SB");
constexpr auto shapeDataTag = records::packTag("sb");

// The zlib serialization gives the three former tagged families consecutive class ids.
// Exact Finale 2007 and 2012 source/conversion pairs establish which payload is which:
// 0x00d5 is the long-valued data stream, 0x00d6 is the six-word definition, and 0x00d7
// is the packed instruction stream.
constexpr records::LegacyTag shapeDataClass = 0x00d5;
constexpr records::LegacyTag shapeDefinitionClass = 0x00d6;
constexpr records::LegacyTag shapeInstructionClass = 0x00d7;

struct ShapeSourceFamily
{
    const records::LegacyRowPool* pool{};
    records::LegacyTag definition{};
    records::LegacyTag instructions{};
    records::LegacyTag data{};
    bool earlyData{};
    bool classRecords{};
};

ShapeSourceFamily sourceFamily(const ImportContext& context)
{
    if (context.profile.epoch == FormatEpoch::ZlibLegacy) {
        return {&context.index.getClassOthers(), shapeDefinitionClass, shapeInstructionClass, shapeDataClass, false, true};
    }
    if (context.profile.epoch == FormatEpoch::CodaBanner) {
        return {&context.index.getOthers(), shapeDefinitionTag, earlyShapeInstructionTag, earlyShapeDataTag, true, false};
    }
    // The uncompressed and DCL epochs deliberately share the same fixed-row spelling.
    return {&context.index.getOthers(), shapeDefinitionTag, shapeInstructionTag, shapeDataTag, false, false};
}

std::uint32_t combineShapeWords(std::int16_t first, std::int16_t second, SourcePlatform platform)
{
    const auto a = static_cast<std::uint16_t>(first);
    const auto b = static_cast<std::uint16_t>(second);
    // Fixed-row four-byte values use the platform's historical word order after the
    // record index has normalized each word. The Windows form is low-word first; the Mac
    // form is high-word first. Zlib payloads are read separately as ordinary four-byte
    // numbers in the container's byte order and never reach this branch.
    return platform == SourcePlatform::Windows ? (static_cast<std::uint32_t>(b) << 16U) | a : (static_cast<std::uint32_t>(a) << 16U) | b;
}

std::int32_t readShapeLong(const std::uint8_t* bytes, ByteOrder order)
{
    const auto value = order == ByteOrder::BigEndian
                           ? (static_cast<std::uint32_t>(bytes[0]) << 24U) | (static_cast<std::uint32_t>(bytes[1]) << 16U)
                                 | (static_cast<std::uint32_t>(bytes[2]) << 8U) | bytes[3]
                           : static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8U)
                                 | (static_cast<std::uint32_t>(bytes[2]) << 16U) | (static_cast<std::uint32_t>(bytes[3]) << 24U);
    return static_cast<std::int32_t>(value);
}

std::vector<std::int32_t> fixedLongs(std::span<const records::LegacyRow> rows, SourcePlatform platform)
{
    std::vector<std::int32_t> result;
    result.reserve(rows.size() * 3);
    for (const auto& row : rows) {
        for (std::size_t i = 0; i + 1 < row.wordCount; i += 2) {
            result.push_back(static_cast<std::int32_t>(combineShapeWords(row.words[i], row.words[i + 1], platform)));
        }
    }
    return result;
}

std::vector<std::int32_t> classLongs(const records::LegacyRowPool& pool, const records::LegacyRow& row, ByteOrder order)
{
    const auto payload = pool.effectivePayloadOf(row);
    std::vector<std::int32_t> result;
    result.reserve(payload.size() / 4);
    for (std::size_t at = 0; at + 4 <= payload.size(); at += 4) {
        result.push_back(readShapeLong(payload.data() + at, order));
    }
    return result;
}

std::vector<std::int32_t> shapeLongs(
    const ShapeSourceFamily& source, records::LegacyTag identity, std::uint16_t cmper, std::uint16_t partId, const ImportContext& context)
{
    const auto rows = source.pool->getArray(identity, cmper, 0, partId);
    if (rows.empty()) {
        return {};
    }
    if (context.profile.epoch == FormatEpoch::ZlibLegacy) {
        return classLongs(*source.pool, rows.front(), context.profile.byteOrder);
    }
    return fixedLongs(rows, context.profile.platform);
}

template <typename Target, typename Reporting>
void reportShapeValue(
    Reporting& reporting, musx::dom::Cmper cmper, std::string member, std::uint16_t partId, std::int64_t value, const records::LegacyRow& row)
{
    reporting.report().setField(reporting.template instanceKey<Target>(partId, cmper), std::move(member),
        {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset, value});
}

ShapeInstructionType instructionType(records::LegacyTag tag, bool early)
{
    using records::packTag;
    switch (tag) {
    case packTag("br"): return ShapeInstructionType::Bracket;
    case packTag("cc"): return ShapeInstructionType::CloneChar;
    case packTag("cp"): return ShapeInstructionType::ClosePath;
    case packTag("cv"): return ShapeInstructionType::CurveTo;
    case packTag("dc"): return ShapeInstructionType::DrawChar;
    case packTag("el"): return ShapeInstructionType::Ellipse;
    case packTag("er"): return ShapeInstructionType::EndGroup;
    case packTag("xg"): return ShapeInstructionType::ExternalGraphic;
    case packTag("fl"): return ShapeInstructionType::FillAlt;
    case packTag("fs"): return ShapeInstructionType::FillSolid;
    case packTag("go"): return ShapeInstructionType::GoToOrigin;
    // Before Finale 3.0 this instruction meant origin, not the beginning of the path.
    case packTag("gs"): return early ? ShapeInstructionType::GoToOrigin : ShapeInstructionType::GoToStart;
    case packTag("lw"): return ShapeInstructionType::LineWidth;
    case packTag("sw"): return ShapeInstructionType::LineWidth;
    case packTag("re"): return ShapeInstructionType::Rectangle;
    case packTag("rl"): return ShapeInstructionType::RLineTo;
    case packTag("rm"): return ShapeInstructionType::RMoveTo;
    case packTag("sa"): return ShapeInstructionType::SetArrowhead;
    case packTag("bl"): return ShapeInstructionType::SetBlack;
    case packTag("sd"): return ShapeInstructionType::SetDash;
    case packTag("sf"): return ShapeInstructionType::SetFont;
    case packTag("sg"): return ShapeInstructionType::SetGray;
    case packTag("wo"): return ShapeInstructionType::SetWhite;
    case packTag("sl"):
    case packTag("ti"): return ShapeInstructionType::Slur;
    case packTag("sr"): return ShapeInstructionType::StartGroup;
    case packTag("so"): return ShapeInstructionType::StartObject;
    case packTag("st"): return ShapeInstructionType::Stroke;
    case packTag("vm"): return ShapeInstructionType::VerticalMode;
    default: return ShapeInstructionType::Undocumented;
    }
}

void importShapeData(const ShapeSourceFamily& source, const ImportContext& context)
{
    const RecordFamilySource recordSource{source.pool, source.data, source.classRecords};
    for (const auto& [partId, cmper] : recordKeys(recordSource)) {
        if (cmper == 0) {
            // Comparator 0 means "no data" wherever a ShapeDef stores it, so nothing ever
            // resolves this comparator; a physical record here is leftover bytes from a
            // deleted or never-populated slot, not content any real shape can reach.
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Verbose, "Skipped ShapeData comparator 0, which Finale reserves to mean \"no data\"."});
            continue;
        }
        const auto rows = source.pool->getArray(source.data, cmper, 0, partId);
        auto target = createOthersRecordTarget<ShapeDataTarget>(context.document, recordSource, rows.front(), cmper);
        if (!target) {
            continue;
        }
        std::vector<std::size_t> earlyLineWidths;
        if (source.earlyData) {
            // The three pools have independent comparators. Find every definition that
            // names this data list instead of assuming its instruction-list id matches.
            for (const auto& shape : context.document->getOthers()->getArray<ShapeDefTarget>(partId)) {
                if (shape->dataList != cmper) {
                    continue;
                }
                if (const auto instructions = context.document->getOthers()->get<ShapeInstructionTarget>(partId, shape->instructionList)) {
                    std::size_t dataIndex = 0;
                    for (const auto& instruction : instructions->instructions) {
                        if (instruction->type == ShapeInstructionType::LineWidth && instruction->numData == 1) {
                            earlyLineWidths.push_back(dataIndex);
                        }
                        dataIndex += static_cast<std::size_t>(instruction->numData);
                    }
                }
            }
        }
        const auto values = shapeLongs(source, source.data, cmper, partId, context);
        for (std::size_t index = 0; index < values.size(); ++index) {
            auto value = values[index];
            if (source.earlyData && std::find(earlyLineWidths.begin(), earlyLineWidths.end(), index) != earlyLineWidths.end()) {
                // Revision 1's `sw` stored hundredths of a point. The modern LineWidth
                // instruction consumes Efix: one point becomes 256.
                value = (value * 256 + (value >= 0 ? 50 : -50)) / 100;
            }
            target->values.push_back(value);
            const auto& row = rows[context.profile.epoch == FormatEpoch::ZlibLegacy ? 0 : (index / 3)];
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                reporting.report().setField(reporting.template instanceKey<ShapeDataTarget>(partId, cmper), "values[" + std::to_string(index) + "]",
                    {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset, values[index]});
            });
        }
        context.document->getOthers()->add(ShapeDataTarget::XmlNodeName, std::move(target));
    }
}

void importShapeInstructions(const ShapeSourceFamily& source, const ImportContext& context)
{
    const RecordFamilySource recordSource{source.pool, source.instructions, source.classRecords};
    for (const auto& [partId, cmper] : recordKeys(recordSource)) {
        if (cmper == 0) {
            // Comparator 0 means "no instructions" wherever a ShapeDef stores it (see
            // ShapeDef::isBlank(), which returns true on sight without resolving it), so
            // nothing ever resolves this comparator; a physical record here is leftover
            // bytes from a deleted or never-populated slot, not content any real shape can
            // reach.
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Verbose, "Skipped ShapeInstructionList comparator 0, which Finale reserves to mean "
                                                        "\"no instructions\"."});
            continue;
        }
        const auto rows = source.pool->getArray(source.instructions, cmper, 0, partId);
        const auto packed = shapeLongs(source, source.instructions, cmper, partId, context);
        auto target = createOthersRecordTarget<ShapeInstructionTarget>(context.document, recordSource, rows.front(), cmper);
        if (!target) {
            continue;
        }
        for (std::size_t index = 0; index < packed.size(); ++index) {
            const auto raw = static_cast<std::uint32_t>(packed[index]);
            if (raw == 0) {
                // Zero terminates the logical list. Real files retain stale packed
                // instructions after it; Finale's own upgrade omits that tail, and
                // consuming it makes the declared data counts exceed ShapeData.
                break;
            }
            const auto tag = static_cast<records::LegacyTag>(raw & 0xffffU);
            const auto numData = static_cast<int>((raw >> 16U) & 0xffU);
            const auto revision = static_cast<unsigned>((raw >> 24U) & 0xffU);
            auto instruction = std::make_shared<ShapeInstructionTarget::InstructionInfo>();
            instruction->numData = numData;
            instruction->type = instructionType(tag, source.earlyData);
            target->instructions.push_back(instruction);
            const auto& row = rows[context.profile.epoch == FormatEpoch::ZlibLegacy ? 0 : (index / 3)];
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto reportPrefix = "instructions[" + std::to_string(target->instructions.size() - 1) + "].";
                reportShapeValue<ShapeInstructionTarget>(reporting, cmper, reportPrefix + "numData", partId, numData, row);
                // The destination is an enum, but the report's raw value preserves the
                // two-byte source tag that selected it.
                reportShapeValue<ShapeInstructionTarget>(reporting, cmper, reportPrefix + "type", partId, tag, row);
            });

            if (instruction->type == ShapeInstructionType::Undocumented || revision > 2) {
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Info, "Shape " + std::to_string(cmper) + " contains unsupported instruction tag "
                                                             + records::tagText(tag) + " revision " + std::to_string(revision) + "."});
            }
        }
        context.document->getOthers()->add(ShapeInstructionTarget::XmlNodeName, std::move(target));
    }
}

void importShapeDefs(const ShapeSourceFamily& source, const ImportContext& context)
{
    const RecordFamilySource recordSource{source.pool, source.definition, source.classRecords};
    for (const auto& [partId, cmper] : recordKeys(recordSource)) {
        if (cmper == 0) {
            // Comparator 0 means "no shape" wherever it is stored (ClefDef::shapeId,
            // MultimeasureRestOptions::shapeDef, and so on all treat 0 as absent before
            // ever resolving it), so nothing ever resolves this comparator; a physical
            // record here is leftover bytes from a deleted or never-populated slot, not a
            // shape any reference can reach.
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Verbose, "Skipped ShapeDef comparator 0, which Finale reserves to mean \"no shape\"."});
            continue;
        }
        const auto rows = source.pool->getArray(source.definition, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto words = context.profile.epoch == FormatEpoch::ZlibLegacy
                               ? payloadWords(source.pool->effectivePayloadOf(rows.front()), context.profile.byteOrder)
                               : std::vector<std::int16_t>(rows.front().words.begin(), rows.front().words.begin() + rows.front().wordCount);
        if (words.size() < 2) {
            continue;
        }
        auto target = createOthersRecordTarget<ShapeDefTarget>(context.document, recordSource, rows.front(), cmper);
        if (!target) {
            continue;
        }
        target->instructionList = static_cast<musx::dom::Cmper>(words[0]);
        target->dataList = static_cast<musx::dom::Cmper>(words[1]);
        // Coda SD carries a bounding rectangle after the two list ids. Later SD and
        // zlib class 0x00d6 put the semantic enum in word 2; out-of-range values are
        // retained as Other rather than constructing an invalid enum.
        const bool hasStoredShapeType = context.profile.epoch == FormatEpoch::UncompressedLegacy || context.profile.epoch == FormatEpoch::DclLegacy
                                        || context.profile.epoch == FormatEpoch::ZlibLegacy;
        if (hasStoredShapeType && words.size() >= 3 && words[2] >= static_cast<int>(ShapeDefTarget::ShapeType::Other)
            && words[2] <= static_cast<int>(ShapeDefTarget::ShapeType::Clef)) {
            target->shapeType = static_cast<ShapeDefTarget::ShapeType>(words[2]);
        }
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            reportShapeValue<ShapeDefTarget>(reporting, cmper, "instructionList", partId, target->instructionList, rows.front());
            reportShapeValue<ShapeDefTarget>(reporting, cmper, "dataList", partId, target->dataList, rows.front());
        });
        if (!hasStoredShapeType) {
            // This layout carries a bounding rectangle in this position. `Other` is
            // the behavior represented by an absent modern type, not a recovered value.
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                reporting.report().setField(reporting.template instanceKey<ShapeDefTarget>(partId, cmper), "shapeType",
                    {Reporting::Origin::LegacyBehavior, 0, 0, static_cast<int>(target->shapeType)});
            });
        } else {
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                reportShapeValue<ShapeDefTarget>(reporting, cmper, "shapeType", partId, static_cast<int>(target->shapeType), rows.front());
            });
        }
        context.document->getOthers()->add(ShapeDefTarget::XmlNodeName, std::move(target));
    }
}

void validateShapeDefinitions(const ShapeSourceFamily& source, const ImportContext& context)
{
    std::size_t unresolved = 0;
    std::size_t insufficient = 0;
    std::size_t externalGraphics = 0;
    std::size_t unresolvedGraphicAssignments = 0;
    const RecordFamilySource recordSource{source.pool, source.definition, source.classRecords};
    for (const auto& [partId, cmper] : recordKeys(recordSource)) {
        const auto shape = context.document->getOthers()->get<ShapeDefTarget>(partId, cmper);
        if (!shape || shape->isBlank()) {
            continue;
        }
        const auto instructions = context.document->getOthers()->get<ShapeInstructionTarget>(partId, shape->instructionList);
        const auto data = context.document->getOthers()->get<ShapeDataTarget>(partId, shape->dataList);
        if (!instructions || !data) {
            ++unresolved;
            continue;
        }
        std::size_t required = 0;
        for (const auto& instruction : instructions->instructions) {
            if (instruction->type == ShapeInstructionType::ExternalGraphic) {
                ++externalGraphics;
                // ExternalGraphic consumes width, height, then the ShapeGraphicAssign
                // cmper. Resolve that intermediate assignment rather than treating the
                // operand as an embedded-file id.
                if (required + 2 >= data->values.size()) {
                    ++unresolvedGraphicAssignments;
                } else {
                    const auto assignmentCmper = static_cast<musx::dom::Cmper>(data->values[required + 2]);
                    const auto assignment = musx::dom::others::ShapeGraphicAssign::findForGraphic(context.document, partId, assignmentCmper);
                    if (!assignment) {
                        ++unresolvedGraphicAssignments;
                    }
                }
            }
            required += static_cast<std::size_t>(instruction->numData);
        }
        if (required > data->values.size()) {
            ++insufficient;
        }
    }
    if (unresolved != 0 || insufficient != 0) {
        context.report.diagnostics.push_back(
            {musx::util::Logger::LogLevel::Warning, std::to_string(unresolved) + " nonblank shape definition(s) have a missing list, and "
                                                        + std::to_string(insufficient) + " have less data than their instructions consume."});
    }
    if (externalGraphics != 0) {
        const auto level = unresolvedGraphicAssignments == 0 ? musx::util::Logger::LogLevel::Verbose : musx::util::Logger::LogLevel::Info;
        context.report.diagnostics.push_back(
            {level, std::to_string(externalGraphics) + " shape external-graphic instruction(s) were " + "checked; "
                        + std::to_string(unresolvedGraphicAssignments) + " do not resolve to a ShapeGraphicAssign."});
    }
}

} // namespace

void importShapeDefinitions(const ImportContext& context)
{
    const auto source = sourceFamily(context);
    // Definitions precede data only because the Coda line-width conversion must follow
    // their independent instruction-list and data-list references.
    importShapeInstructions(source, context);
    importShapeDefs(source, context);
    importShapeData(source, context);
    // External graphics resolve against ShapeGraphicAssign, which another importer fills.
    context.pending.defer(DeferredStage::ResolveStoredRefs, [source, &context] { validateShapeDefinitions(source, context); });
}

} // namespace others
} // namespace finale_mus_reader
