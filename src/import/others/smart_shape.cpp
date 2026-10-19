// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "import/shared/early_smart_shape.h"
#include "import/shared/skipped_smart_shapes.h"
#include "import/shared/smart_shape_adjustment_flags.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using Shape = musx::dom::others::SmartShape;
constexpr records::LegacyTag smartShapeTag = records::packTag("Sx");
constexpr records::LegacyTag smartShapeClass = 0x00d9;

void importEarlySmartShapes(const ImportContext& context)
{
    if (context.profile.epoch != FormatEpoch::CodaBanner && context.profile.epoch != FormatEpoch::UncompressedLegacy) {
        return;
    }
    namespace early = early_smart_shape;
    const auto& pool = context.index.getOthers();
    const RecordFamilySource startSource{&pool, early::startTag, false};
    const RecordFamilySource endSource{&pool, early::endTag, false};
    const RecordFamilySource endpointSource{&pool, early::endpointTag, false};
    const RecordFamilySource offsetSource{&pool, early::offsetTag, false};
    const RecordFamilySource curveSource{&pool, early::curveTag, false};
    for (const auto shapeId : pool.cmpersForTag(early::endTag)) {
        const auto records = early::find(pool, context.profile.epoch, shapeId);
        if (!records) {
            continue;
        }
        const auto* startRow = records->start;
        const auto* endRow = records->end;
        const auto startMeasure = records->startMeasure();
        const auto endMeasure = records->endMeasure();
        const auto& startEndpoint = *records->startEndpoint;
        const auto& endEndpoint = *records->endEndpoint;
        const auto* offsetRow = pool.get(early::offsetTag, shapeId, 0, 0);
        const auto* curveRow = pool.get(early::curveTag, shapeId, 0, 0);
        const auto offset = [&](std::size_t word) -> std::int16_t { return offsetRow ? offsetRow->words[word] : 0; };
        const auto curve = [&](std::size_t word) -> std::int16_t { return curveRow ? curveRow->words[word] : 0; };
        const auto layout = static_cast<std::uint16_t>(endRow->words[5]);

        auto shape = createOthersRecordTarget<Shape>(context.document, endSource, *endRow, shapeId);
        shape->integrityCheck(shape);
        shape->shapeType = static_cast<Shape::ShapeType>(endRow->words[1]);
        shape->makeHorz = (layout & 0x0400) != 0;
        shape->noPushEndStart = (layout & 0x0200) == 0;
        // Both endpoints take the start row's staff.
        const auto staff = early::staff(startEndpoint);
        shape->startTermSeg->endPoint->staffId = staff;
        shape->startTermSeg->endPoint->measId = startMeasure;
        shape->startTermSeg->endPoint->eduPosition = startEndpoint.words[1];
        shape->startTermSeg->endPointAdj->horzOffset = offset(0);
        shape->startTermSeg->endPointAdj->vertOffset = startEndpoint.words[2];
        shape->startTermSeg->breakAdj->horzOffset = offset(1);
        shape->startTermSeg->breakAdj->vertOffset = offset(2);
        shape->endTermSeg->endPoint->staffId = staff;
        shape->endTermSeg->endPoint->measId = endMeasure;
        shape->endTermSeg->endPoint->eduPosition = endEndpoint.words[1];
        shape->endTermSeg->endPointAdj->horzOffset = offset(3);
        shape->endTermSeg->endPointAdj->vertOffset = endEndpoint.words[2];
        shape->endTermSeg->breakAdj->horzOffset = offset(4);
        shape->endTermSeg->breakAdj->vertOffset = offset(5);
        for (auto* adjustment : {shape->startTermSeg->endPointAdj.get(), shape->startTermSeg->breakAdj.get(), shape->endTermSeg->endPointAdj.get(),
                 shape->endTermSeg->breakAdj.get()}) {
            adjustment->active = true;
        }
        const auto startHeight = endRow->words[4];
        const auto endHeight = startRow->words[4];
        early::convertControlPoints(*shape->fullCtlPtAdj, shape->shapeType, curve(0), curve(1), startHeight);
        early::convertControlPoints(*shape->startTermSeg->ctlPtAdj, shape->shapeType, curve(2), curve(3), startHeight);
        early::convertControlPoints(*shape->endTermSeg->ctlPtAdj, shape->shapeType, curve(4), curve(5), endHeight);

        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            const auto key = reporting.template instanceKey<Shape>(endRow->partId, shapeId);
            const auto field = [&](const RecordFamilySource& source, const records::LegacyRow& row, const char* member, std::size_t at,
                                   std::int64_t value) { reportLegacyField(reporting, key, source, row, member, at, value); };
            const auto behavior = [&](const char* member, std::int64_t value) {
                reportFallbackField(reporting, key, member, Reporting::Origin::LegacyBehavior, value);
            };
            field(endSource, *endRow, "shapeType", 2, endRow->words[1]);
            field(endSource, *endRow, "makeHorz", 10, shape->makeHorz);
            field(endSource, *endRow, "noPushEndStart", 10, shape->noPushEndStart);
            for (const char* member :
                {"entryBased", "rotate", "noPresetShape", "makeVert", "engraverSlurState", "slurAvoidAcciState", "yBreakType", "hidden", "direction",
                    "startNoteId", "endNoteId", "lineStyleId", "startLyricNum", "endLyricNum", "startLyricType", "endLyricType"}) {
                behavior(member, 0);
            }
            field(endpointSource, startEndpoint, "startTermSeg.endPoint.staffId", 10, staff);
            field(startSource, *startRow, "startTermSeg.endPoint.measId", 0, startMeasure);
            field(endpointSource, startEndpoint, "startTermSeg.endPoint.eduPosition", 2, startEndpoint.words[1]);
            field(endpointSource, startEndpoint, "startTermSeg.endPointAdj.vertOffset", 4, startEndpoint.words[2]);
            field(endpointSource, startEndpoint, "endTermSeg.endPoint.staffId", 10, staff);
            field(endSource, *endRow, "endTermSeg.endPoint.measId", 0, endMeasure);
            field(endpointSource, endEndpoint, "endTermSeg.endPoint.eduPosition", 2, endEndpoint.words[1]);
            field(endpointSource, endEndpoint, "endTermSeg.endPointAdj.vertOffset", 4, endEndpoint.words[2]);
            for (const auto& [member, word] : {std::pair{"startTermSeg.endPointAdj.horzOffset", 0}, std::pair{"startTermSeg.breakAdj.horzOffset", 1},
                     std::pair{"startTermSeg.breakAdj.vertOffset", 2}, std::pair{"endTermSeg.endPointAdj.horzOffset", 3},
                     std::pair{"endTermSeg.breakAdj.horzOffset", 4}, std::pair{"endTermSeg.breakAdj.vertOffset", 5}}) {
                if (offsetRow) {
                    field(offsetSource, *offsetRow, member, std::size_t(word) * 2, offset(word));
                } else {
                    behavior(member, 0);
                }
            }
            for (const char* segment : {"startTermSeg.endPointAdj", "startTermSeg.breakAdj", "endTermSeg.endPointAdj", "endTermSeg.breakAdj"}) {
                const std::string prefix(segment);
                behavior((prefix + ".active").c_str(), true);
                behavior((prefix + ".contextDir").c_str(), 0);
                behavior((prefix + ".contextEntCnct").c_str(), 0);
            }
            // A computed control point keeps the stored word it derives from as its raw value.
            const auto controlPoints = [&](const std::string& prefix, const musx::dom::smartshape::ControlPointAdjustment& adjustment,
                                           std::size_t leftWord, const RecordFamilySource& heightSource, const records::LegacyRow& heightRow) {
                behavior((prefix + ".active").c_str(), adjustment.active);
                behavior((prefix + ".contextDir").c_str(), 0);
                const auto computed = [&](const char* member, const RecordFamilySource& source, const records::LegacyRow* row, std::size_t word,
                                          int value) {
                    if (!row) {
                        behavior((prefix + member).c_str(), value);
                        return;
                    }
                    reporting.report().setField(key, prefix + member,
                        typename Reporting::FieldInfo{Reporting::Origin::LegacyMusAdjusted, row->blockOffset, row->decodedOffset + word * 2,
                            row->words[word], source.identity});
                };
                const bool slur = adjustment.active && shape->calcIsSlur();
                computed(".startCtlPtX", curveSource, slur ? curveRow : nullptr, leftWord, adjustment.startCtlPtX);
                computed(".endCtlPtX", curveSource, slur ? curveRow : nullptr, leftWord + 1, adjustment.endCtlPtX);
                computed(".startCtlPtY", heightSource, adjustment.active ? &heightRow : nullptr, 4, adjustment.startCtlPtY);
                computed(".endCtlPtY", heightSource, slur ? &heightRow : nullptr, 4, adjustment.endCtlPtY);
            };
            controlPoints("fullCtlPtAdj", *shape->fullCtlPtAdj, 0, endSource, *endRow);
            controlPoints("startTermSeg.ctlPtAdj", *shape->startTermSeg->ctlPtAdj, 2, endSource, *endRow);
            controlPoints("endTermSeg.ctlPtAdj", *shape->endTermSeg->ctlPtAdj, 4, startSource, *startRow);
            reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        });
        context.document->getOthers()->add(Shape::XmlNodeName, std::move(shape));
    }
}

bool smartShapeIsEntryBased(const void* instance)
{
    const auto* shape = static_cast<const Shape*>(instance);
    return shape->entryBased || shape->calcIsLyricShape();
}

bool smartShapeIsBeatBased(const void* instance)
{
    return !smartShapeIsEntryBased(instance);
}

std::optional<musx::dom::LyricTextType> lyricTypeFromBytes(std::span<const std::uint8_t> payload, std::size_t offset)
{
    // Fixed records use two-letter text tags; compressed records can encode the same kind numerically.
    if (payload.size() <= offset + 1) {
        return std::nullopt;
    }
    if ((payload[offset] == 0x04 && payload[offset + 1] == 0x1a) || (payload[offset] == 0x1a && payload[offset + 1] == 0x04)
        || (payload[offset] == 0x54 && payload[offset + 1] == 0x04)) {
        return musx::dom::LyricTextType::Verse;
    }
    if ((payload[offset] == 0x04 && payload[offset + 1] == 0x18) || (payload[offset] == 0x18 && payload[offset + 1] == 0x04)
        || (payload[offset] == 0x52 && payload[offset + 1] == 0x04)) {
        return musx::dom::LyricTextType::Chorus;
    }
    if ((payload[offset] == 0x04 && payload[offset + 1] == 0x19) || (payload[offset] == 0x19 && payload[offset + 1] == 0x04)
        || (payload[offset] == 0x53 && payload[offset + 1] == 0x04)) {
        return musx::dom::LyricTextType::Section;
    }
    if (payload[offset] == 'v' && payload[offset + 1] == 'e') {
        return musx::dom::LyricTextType::Verse;
    }
    if (payload[offset] == 'c' && payload[offset + 1] == 'h') {
        return musx::dom::LyricTextType::Chorus;
    }
    if (payload[offset] == 's' && payload[offset + 1] == 'e') {
        return musx::dom::LyricTextType::Section;
    }
    return std::nullopt;
}

MappingTarget createSmartShape(
    const musx::dom::DocumentPtr& document, const RecordFamilySource& source, const records::LegacyRow& row, std::uint16_t cmper)
{
    if (cmper == 0) {
        return {};
    }
    const auto payloadSize = source.pool->payloadOf(row).size();
    if (payloadSize != 84 && payloadSize != 96) {
        return {};
    }
    auto shape = createOthersRecordTarget<Shape>(document, source, row, cmper);
    shape->integrityCheck(shape);
    auto* raw = shape.get();
    document->getOthers()->add(Shape::XmlNodeName, std::move(shape));
    return makeMappingTarget(row.partId, cmper, raw);
}

MappingTarget createFixedSmartShape(
    const musx::dom::DocumentPtr& document, const RecordFamilySource& source, const records::LegacyRow& row, std::uint16_t cmper)
{
    if (cmper == 0) {
        return {};
    }
    const auto rows = source.pool->getArray(source.identity, cmper, 0, row.partId);
    if (rows.size() != 7 && rows.size() != 8) {
        return {};
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].inci != i || source.pool->payloadOf(rows[i]).size() != 12) {
            return {};
        }
    }
    auto shape = createOthersRecordTarget<Shape>(document, source, row, cmper);
    shape->integrityCheck(shape);
    auto* raw = shape.get();
    document->getOthers()->add(Shape::XmlNodeName, std::move(shape));
    return makeMappingTarget(row.partId, cmper, raw);
}

// The low half of each four-byte value precedes its high half in this layout.
// The record is 84 or 96 bytes; the latter has lyric fields and four bytes of row padding.
const FieldMapping smartShapeFields[] = {
    MUS_CLASS_WORD_AS_IF(Shape, smartShapeClass, CMPER_FROM_TARGET, 0, nullptr, shapeType, static_cast<Shape::ShapeType>(value)),
    MUS_CLASS_FIELD_IF(
        Shape, smartShapeClass, CMPER_FROM_TARGET, 2, ValueWidth::Long, LongWordOrder::LowFirst, (BitRange{30, 1}), nullptr, entryBased),
    MUS_CLASS_FIELD_IF(Shape, smartShapeClass, CMPER_FROM_TARGET, 2, ValueWidth::Long, LongWordOrder::LowFirst, (BitRange{29, 1}), nullptr, rotate),
    MUS_CLASS_FIELD_AS_IF(Shape, smartShapeClass, CMPER_FROM_TARGET, 2, ValueWidth::Long, LongWordOrder::LowFirst, (BitRange{15, 1}), nullptr,
        noPresetShape, value == 0),
    MUS_CLASS_BIT(Shape, smartShapeClass, CMPER_FROM_TARGET, 2, 10, makeHorz),
    MUS_CLASS_WORD_AS_IF(Shape, smartShapeClass, CMPER_FROM_TARGET, 2, nullptr, noPushEndStart, (value & 0x0200) == 0),
    MUS_CLASS_BIT(Shape, smartShapeClass, CMPER_FROM_TARGET, 2, 8, makeVert),
    MUS_CLASS_WORD_AS_IF(Shape, smartShapeClass, CMPER_FROM_TARGET, 2, nullptr, engraverSlurState,
        (value & 0x10)   ? Shape::EngraverSlurState::Off
        : (value & 0x20) ? Shape::EngraverSlurState::On
                         : Shape::EngraverSlurState::Auto),
    MUS_CLASS_WORD_AS_IF(Shape, smartShapeClass, CMPER_FROM_TARGET, 2, nullptr, slurAvoidAcciState,
        (value & 0x04)   ? Shape::SlurAvoidAccidentalsState::Off
        : (value & 0x08) ? Shape::SlurAvoidAccidentalsState::On
                         : Shape::SlurAvoidAccidentalsState::Auto),
    MUS_CLASS_WORD_AS_IF(Shape, smartShapeClass, CMPER_FROM_TARGET, 2, nullptr, yBreakType,
        (value & 0x01) ? Shape::SystemBreakType::Opposite : Shape::SystemBreakType::Same),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 6, startTermSeg->endPoint->staffId),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 8, startTermSeg->endPoint->measId),
    MUS_CLASS_LONG_IF(
        Shape, smartShapeClass, CMPER_FROM_TARGET, 10, LongWordOrder::LowFirst, &smartShapeIsEntryBased, startTermSeg->endPoint->entryNumber),
    MUS_CLASS_LONG_IF(
        Shape, smartShapeClass, CMPER_FROM_TARGET, 10, LongWordOrder::LowFirst, &smartShapeIsBeatBased, startTermSeg->endPoint->eduPosition),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 14, startTermSeg->endPointAdj->horzOffset),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 16, startTermSeg->endPointAdj->vertOffset),
    MUS_CLASS_BIT(Shape, smartShapeClass, CMPER_FROM_TARGET, 18, smart_shape_adjustment_flags::activeBit, startTermSeg->endPointAdj->active),
    MUS_CLASS_WORD_AS_IF(Shape, smartShapeClass, CMPER_FROM_TARGET, 18, nullptr, startTermSeg->endPointAdj->contextDir,
        smart_shape_adjustment_flags::direction(value)),
    MUS_CLASS_WORD_AS_IF(Shape, smartShapeClass, CMPER_FROM_TARGET, 18, nullptr, startTermSeg->endPointAdj->contextEntCnct,
        smart_shape_adjustment_flags::entryConnection(value)),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 20, startTermSeg->ctlPtAdj->startCtlPtX),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 22, startTermSeg->ctlPtAdj->startCtlPtY),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 24, startTermSeg->ctlPtAdj->endCtlPtX),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 26, startTermSeg->ctlPtAdj->endCtlPtY),
    MUS_CLASS_BIT(Shape, smartShapeClass, CMPER_FROM_TARGET, 28, smart_shape_adjustment_flags::activeBit, startTermSeg->ctlPtAdj->active),
    MUS_CLASS_WORD_AS_IF(
        Shape, smartShapeClass, CMPER_FROM_TARGET, 28, nullptr, startTermSeg->ctlPtAdj->contextDir, smart_shape_adjustment_flags::direction(value)),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 30, startTermSeg->breakAdj->horzOffset),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 32, startTermSeg->breakAdj->vertOffset),
    MUS_CLASS_BIT(Shape, smartShapeClass, CMPER_FROM_TARGET, 34, smart_shape_adjustment_flags::activeBit, startTermSeg->breakAdj->active),
    MUS_CLASS_WORD_AS_IF(
        Shape, smartShapeClass, CMPER_FROM_TARGET, 34, nullptr, startTermSeg->breakAdj->contextDir, smart_shape_adjustment_flags::direction(value)),
    MUS_CLASS_WORD_AS_IF(Shape, smartShapeClass, CMPER_FROM_TARGET, 34, nullptr, startTermSeg->breakAdj->contextEntCnct,
        smart_shape_adjustment_flags::entryConnection(value)),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 36, endTermSeg->endPoint->staffId),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 38, endTermSeg->endPoint->measId),
    MUS_CLASS_LONG_IF(
        Shape, smartShapeClass, CMPER_FROM_TARGET, 40, LongWordOrder::LowFirst, &smartShapeIsEntryBased, endTermSeg->endPoint->entryNumber),
    MUS_CLASS_LONG_IF(
        Shape, smartShapeClass, CMPER_FROM_TARGET, 40, LongWordOrder::LowFirst, &smartShapeIsBeatBased, endTermSeg->endPoint->eduPosition),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 44, endTermSeg->endPointAdj->horzOffset),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 46, endTermSeg->endPointAdj->vertOffset),
    MUS_CLASS_BIT(Shape, smartShapeClass, CMPER_FROM_TARGET, 48, smart_shape_adjustment_flags::activeBit, endTermSeg->endPointAdj->active),
    MUS_CLASS_WORD_AS_IF(
        Shape, smartShapeClass, CMPER_FROM_TARGET, 48, nullptr, endTermSeg->endPointAdj->contextDir, smart_shape_adjustment_flags::direction(value)),
    MUS_CLASS_WORD_AS_IF(Shape, smartShapeClass, CMPER_FROM_TARGET, 48, nullptr, endTermSeg->endPointAdj->contextEntCnct,
        smart_shape_adjustment_flags::entryConnection(value)),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 50, endTermSeg->ctlPtAdj->startCtlPtX),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 52, endTermSeg->ctlPtAdj->startCtlPtY),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 54, endTermSeg->ctlPtAdj->endCtlPtX),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 56, endTermSeg->ctlPtAdj->endCtlPtY),
    MUS_CLASS_BIT(Shape, smartShapeClass, CMPER_FROM_TARGET, 58, smart_shape_adjustment_flags::activeBit, endTermSeg->ctlPtAdj->active),
    MUS_CLASS_WORD_AS_IF(
        Shape, smartShapeClass, CMPER_FROM_TARGET, 58, nullptr, endTermSeg->ctlPtAdj->contextDir, smart_shape_adjustment_flags::direction(value)),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 60, endTermSeg->breakAdj->horzOffset),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 62, endTermSeg->breakAdj->vertOffset),
    MUS_CLASS_BIT(Shape, smartShapeClass, CMPER_FROM_TARGET, 64, smart_shape_adjustment_flags::activeBit, endTermSeg->breakAdj->active),
    MUS_CLASS_WORD_AS_IF(
        Shape, smartShapeClass, CMPER_FROM_TARGET, 64, nullptr, endTermSeg->breakAdj->contextDir, smart_shape_adjustment_flags::direction(value)),
    MUS_CLASS_WORD_AS_IF(Shape, smartShapeClass, CMPER_FROM_TARGET, 64, nullptr, endTermSeg->breakAdj->contextEntCnct,
        smart_shape_adjustment_flags::entryConnection(value)),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 66, fullCtlPtAdj->startCtlPtX),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 68, fullCtlPtAdj->startCtlPtY),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 70, fullCtlPtAdj->endCtlPtX),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 72, fullCtlPtAdj->endCtlPtY),
    MUS_CLASS_BIT(Shape, smartShapeClass, CMPER_FROM_TARGET, 74, smart_shape_adjustment_flags::activeBit, fullCtlPtAdj->active),
    MUS_CLASS_WORD_AS_IF(
        Shape, smartShapeClass, CMPER_FROM_TARGET, 74, nullptr, fullCtlPtAdj->contextDir, smart_shape_adjustment_flags::direction(value)),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 76, startNoteId),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 78, endNoteId),
    MUS_CLASS_WORD(Shape, smartShapeClass, CMPER_FROM_TARGET, 80, lineStyleId),
    MUS_CLASS_WORD_AS_IF(Shape, smartShapeClass, CMPER_FROM_TARGET, 82, nullptr, direction,
        (value & 0x2000)   ? musx::dom::ShapeDirection::Over
        : (value & 0x1000) ? musx::dom::ShapeDirection::Under
                           : musx::dom::ShapeDirection::Automatic),
    MUS_CLASS_BIT(Shape, smartShapeClass, CMPER_FROM_TARGET, 82, 0, hidden),
};

const auto fixedSmartShapeFields = [] {
    std::array<FieldMapping, std::size(smartShapeFields)> result{};
    std::copy(std::begin(smartShapeFields), std::end(smartShapeFields), result.begin());
    // Fixed rows address words in one incidence stream; class records address bytes.
    for (auto& field : result) {
        field.source.identity = smartShapeTag;
        field.source.wordSlot /= 2;
    }
    return result;
}();

const MappingTable& fixedSmartShapeTable()
{
    static const MappingTable table{.reportPrefix = "others.smartShape",
        .epochs = EpochMask::FixedRow,
        .targetKind = TargetKind::OthersFromRecords,
        .recordIdentity = smartShapeTag,
        .createTarget = &createFixedSmartShape,
        .fields = fixedSmartShapeFields.data(),
        .fieldCount = fixedSmartShapeFields.size()};
    return table;
}

const MappingTable& smartShapeTable()
{
    static const MappingTable table{.reportPrefix = "others.smartShape",
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OthersFromRecords,
        .recordIdentity = smartShapeClass,
        .createTarget = &createSmartShape,
        .fields = smartShapeFields,
        .fieldCount = std::size(smartShapeFields)};
    return table;
}

/// @brief Updates measure flags the way Finale's conversion of early shapes does.
/// @details Converting an early shape marks every measure it spans, setting each one's
/// SmartShape flag, and deletes the shape's endpoint and center rows. Those rows share the
/// selector of measure expressions, so a measure left with none has its expression flag cleared.
/// Expression conversion runs later and may set it again.
void refreshEarlyShapeMeasureFlags(const ImportContext& context)
{
    namespace early = early_smart_shape;
    using musx::dom::others::Measure;
    const auto& pool = context.index.getOthers();
    std::set<const records::LegacyRow*> deleted;
    std::set<musx::dom::MeasCmper> emptied;
    const auto adjust = [&](musx::dom::MeasCmper measureId, bool Measure::* member, bool value, const char* name) {
        const auto measure = context.document->getOthers()->get<Measure>(musx::dom::SCORE_PARTID, measureId);
        if (!measure || measure.get()->*member == value) {
            return;
        }
        std::const_pointer_cast<Measure>(measure).get()->*member = value;
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            const auto key = reporting.template instanceKey<Measure>(musx::dom::SCORE_PARTID, measureId);
            reporting.report().fieldProvenance(key, name).origin = Reporting::Origin::LegacyMusAdjusted;
        });
    };
    for (const auto shapeId : pool.cmpersForTag(early::endTag)) {
        const auto records = early::find(pool, context.profile.epoch, shapeId);
        if (!records) {
            continue;
        }
        deleted.insert(records->startEndpoint);
        deleted.insert(records->endEndpoint);
        emptied.insert(records->startMeasure());
        emptied.insert(records->endMeasure());
        for (auto measure = records->startMeasure(); measure <= records->endMeasure(); ++measure) {
            adjust(measure, &Measure::hasSmartShape, true, "hasSmartShape");
            if (measure > records->startMeasure() && measure < records->endMeasure()) {
                if (const auto* center = early::findCenter(pool, context.profile.epoch, shapeId, measure)) {
                    deleted.insert(center);
                    emptied.insert(measure);
                }
            }
            if (measure == (std::numeric_limits<std::uint16_t>::max)()) {
                break;
            }
        }
    }
    for (const auto measure : emptied) {
        const auto rows = pool.getArray(early::endpointTag, measure);
        if (std::ranges::all_of(rows, [&](const records::LegacyRow& row) { return deleted.contains(&row); })) {
            adjust(measure, &Measure::hasExpression, false, "hasExpression");
        }
    }
}

void importLyricTypes(const ImportContext& context)
{
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), smartShapeTag, smartShapeClass);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto payload = collectRecordPayload(*source, rows);
        if (payload.size() < 92) {
            continue;
        }
        auto shape = std::const_pointer_cast<Shape>(context.document->getOthers()->get<Shape>(partId, cmper));
        if (!shape) {
            continue;
        }
        // Hyphens and word extensions carry lyric data even when the lyric-based flag is clear.
        if (!shape->calcIsLyricShape()) {
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<Shape>(partId, cmper);
                for (const char* member : {"startLyricNum", "endLyricNum", "startLyricType", "endLyricType"}) {
                    reportFallbackField(reporting, key, member, Reporting::Origin::LegacyBehavior, 0);
                }
            });
            continue;
        }
        for (const auto& [offset, member, target] :
            {std::tuple{std::size_t{84}, "startLyricNum", &Shape::startLyricNum}, std::tuple{std::size_t{86}, "endLyricNum", &Shape::endLyricNum}}) {
            const auto value = payloadWord(payload, offset, context.profile.byteOrder);
            shape.get()->*target = value;
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<Shape>(partId, cmper);
                const auto& row = source->rowOfByte(rows, offset);
                reportLegacyField(reporting, key, *source, row, member, source->byteOffsetInRow(offset), value);
            });
        }
        for (const auto& [offset, member] : {std::pair{std::size_t{88}, "startLyricType"}, std::pair{std::size_t{90}, "endLyricType"}}) {
            const auto decoded = lyricTypeFromBytes(payload, offset);
            if (!decoded) {
                continue;
            }
            if (offset == 88) {
                shape->startLyricType = decoded;
            } else {
                shape->endLyricType = decoded;
            }
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<Shape>(partId, cmper);
                const auto& row = source->rowOfByte(rows, offset);
                reportLegacyField(reporting, key, *source, row, member, source->byteOffsetInRow(offset), static_cast<int>(*decoded));
            });
        }
    }
}

void inheritZeroPartShapeMetadata(const ImportContext& context)
{
    // The conversion writes no element for a part's zero context direction or entry connection,
    // whatever the continuation mask and the adjustment's activation, so the converted part takes
    // the score's. Believed: a zeroed part slur mode likewise inherits the score's even when the
    // continuation mask marks those bits editable.
    if (context.profile.epoch != FormatEpoch::ZlibLegacy) {
        return;
    }
    constexpr std::uint16_t directionBits = 0x0300;
    constexpr std::uint16_t connectionBits = 0x00ff;
    const auto& pool = context.index.getClassOthers();
    for (const auto& source : context.document->getOthers()->getAllSources<Shape>()) {
        if (source->getSourcePartId() == musx::dom::SCORE_PARTID || source->getShareMode() != Shape::ShareMode::Partial) {
            continue;
        }
        const auto partId = source->getSourcePartId();
        const auto cmper = source->getCmper();
        const auto* row = pool.get(smartShapeClass, cmper, 0, 0, partId);
        const auto score = context.document->getOthers()->get<Shape>(musx::dom::SCORE_PARTID, cmper);
        if (!row || !score || !row->continuationOverlayReady) {
            continue;
        }
        const auto payload = pool.payloadOf(*row);
        const auto continuation = pool.continuationOf(*row);
        // Bits the continuation leaves linked already hold the score's value.
        const auto zeroPartBits = [&](std::size_t offset, std::uint16_t bits) {
            return offset + 2 <= payload.size() && offset + 6 <= continuation.size()
                   && (payloadWord(payload, offset, context.profile.byteOrder) & bits) == 0
                   && (payloadWord(continuation, offset + 4, context.profile.byteOrder) & bits) != 0;
        };
        const auto reportAdjusted = [&]<typename Reporting>(Reporting& reporting, const char* member, std::size_t offset) {
            const auto key = reporting.template instanceKey<Shape>(partId, cmper);
            reporting.report().setField(key, member, {Reporting::Origin::LegacyMusAdjusted, row->blockOffset, row->decodedOffset + offset, 0});
        };
        auto part = std::const_pointer_cast<Shape>(source);
        const auto inheritDirection = [&](auto& partAdjustment, const auto& scoreAdjustment, std::size_t offset, const char* member) {
            if (zeroPartBits(offset, directionBits) && scoreAdjustment->contextDir != musx::dom::smartshape::DirectionType::None) {
                partAdjustment->contextDir = scoreAdjustment->contextDir;
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) { reportAdjusted(reporting, member, offset); });
            }
        };
        const auto inheritConnection = [&](auto& partAdjustment, const auto& scoreAdjustment, std::size_t offset, const char* member) {
            if (zeroPartBits(offset, connectionBits) && scoreAdjustment->contextEntCnct != musx::dom::smartshape::EntryConnectionType::HeadLeftTop) {
                partAdjustment->contextEntCnct = scoreAdjustment->contextEntCnct;
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) { reportAdjusted(reporting, member, offset); });
            }
        };
        inheritDirection(part->startTermSeg->endPointAdj, score->startTermSeg->endPointAdj, 18, "startTermSeg.endPointAdj.contextDir");
        inheritConnection(part->startTermSeg->endPointAdj, score->startTermSeg->endPointAdj, 18, "startTermSeg.endPointAdj.contextEntCnct");
        inheritDirection(part->startTermSeg->ctlPtAdj, score->startTermSeg->ctlPtAdj, 28, "startTermSeg.ctlPtAdj.contextDir");
        inheritDirection(part->startTermSeg->breakAdj, score->startTermSeg->breakAdj, 34, "startTermSeg.breakAdj.contextDir");
        inheritConnection(part->startTermSeg->breakAdj, score->startTermSeg->breakAdj, 34, "startTermSeg.breakAdj.contextEntCnct");
        inheritDirection(part->endTermSeg->endPointAdj, score->endTermSeg->endPointAdj, 48, "endTermSeg.endPointAdj.contextDir");
        inheritConnection(part->endTermSeg->endPointAdj, score->endTermSeg->endPointAdj, 48, "endTermSeg.endPointAdj.contextEntCnct");
        inheritDirection(part->endTermSeg->ctlPtAdj, score->endTermSeg->ctlPtAdj, 58, "endTermSeg.ctlPtAdj.contextDir");
        inheritDirection(part->endTermSeg->breakAdj, score->endTermSeg->breakAdj, 64, "endTermSeg.breakAdj.contextDir");
        inheritConnection(part->endTermSeg->breakAdj, score->endTermSeg->breakAdj, 64, "endTermSeg.breakAdj.contextEntCnct");
        inheritDirection(part->fullCtlPtAdj, score->fullCtlPtAdj, 74, "fullCtlPtAdj.contextDir");

        if (payload.size() >= 4 && continuation.size() >= 8 && part->engraverSlurState == Shape::EngraverSlurState::Auto
            && score->engraverSlurState != Shape::EngraverSlurState::Auto && (payloadWord(payload, 2, context.profile.byteOrder) & 0x30) == 0
            && (payloadWord(continuation, 6, context.profile.byteOrder) & 0x30) == 0x30) {
            part->engraverSlurState = score->engraverSlurState;
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) { reportAdjusted(reporting, "engraverSlurState", 2); });
        }
    }
}

/// @brief The entry numbers the source stores an entry for.
std::set<musx::dom::EntryNumber> storedEntryNumbers(const ImportContext& context)
{
    std::set<musx::dom::EntryNumber> result;
    for (const auto& row : context.index.getCodaEntryRows()) {
        result.insert(static_cast<musx::dom::EntryNumber>(row.number));
    }
    const auto byteOrder = context.profile.byteOrder;
    for (const auto& row : context.index.getEntryRows()) {
        const std::span<const std::uint8_t> bytes(row.bytes);
        if (payloadWord(bytes, 4, byteOrder) == 0) {
            result.insert(payloadLong(bytes, 0, byteOrder, nativeLongWordOrder(byteOrder)));
        }
    }
    return result;
}

/// @brief One entry endpoint of a stored SmartShape record.
struct EntryEndpoint
{
    musx::dom::StaffCmper staffId{};
    musx::dom::EntryNumber entry{};
};

/// @brief The entry endpoints of a stored SmartShape record, none when the shape is not
/// entry-based, or nothing when the record cannot be read.
std::optional<std::vector<EntryEndpoint>> storedEntryEndpoints(const ImportContext& context, musx::dom::Cmper partId, musx::dom::Cmper cmper)
{
    std::uint16_t shapeType{};
    std::uint32_t flags{};
    std::vector<EntryEndpoint> endpoints;
    if (context.profile.epoch == FormatEpoch::ZlibLegacy) {
        const RecordFamilySource source{&context.index.getClassOthers(), smartShapeClass, true};
        const auto payload = collectRecordPayload(source, source.pool->getArray(smartShapeClass, cmper, 0, partId));
        if (payload.size() < 44) {
            return std::nullopt;
        }
        const auto byteOrder = context.profile.byteOrder;
        shapeType = payloadWord(payload, 0, byteOrder);
        flags = static_cast<std::uint32_t>(payloadLong(payload, 2, byteOrder, LongWordOrder::LowFirst));
        for (const std::size_t at : {6, 36}) {
            endpoints.push_back({static_cast<musx::dom::StaffCmper>(payloadWord(payload, at, byteOrder)),
                static_cast<musx::dom::EntryNumber>(payloadLong(payload, at + 4, byteOrder, LongWordOrder::LowFirst))});
        }
    } else {
        const RecordFamilySource source{&context.index.getOthers(), smartShapeTag, false};
        const auto words = collectRecordWords(source, source.pool->getArray(smartShapeTag, cmper, 0, partId), context.profile.byteOrder);
        if (words.size() < 22) {
            return std::nullopt;
        }
        const auto pair = [&](std::size_t low) {
            return static_cast<std::uint32_t>(static_cast<std::uint16_t>(words[low]))
                   | (static_cast<std::uint32_t>(static_cast<std::uint16_t>(words[low + 1])) << 16U);
        };
        shapeType = static_cast<std::uint16_t>(words[0]);
        flags = pair(1);
        for (const std::size_t at : {3, 18}) {
            endpoints.push_back(
                {static_cast<musx::dom::StaffCmper>(static_cast<std::uint16_t>(words[at])), static_cast<musx::dom::EntryNumber>(pair(at + 2))});
        }
    }
    const auto type = static_cast<Shape::ShapeType>(shapeType);
    if ((flags & (1U << 30U)) == 0 && type != Shape::ShapeType::Hyphen && type != Shape::ShapeType::WordExtension) {
        endpoints.clear();
    }
    return endpoints;
}

/// @brief The entries the frames of each staff hold, collected on first request.
class StaffEntries
{
public:
    explicit StaffEntries(const musx::dom::DocumentPtr& document)
        : m_document(document)
    {}

    /// @brief Whether a frame of @p staffId holds @p entry.
    bool holds(musx::dom::StaffCmper staffId, musx::dom::EntryNumber entry)
    {
        auto [found, inserted] = m_entries.try_emplace(staffId);
        if (inserted) {
            for (const auto& hold : m_document->getDetails()->getArray<musx::dom::details::GFrameHold>(musx::dom::SCORE_PARTID, staffId)) {
                musx::dom::details::GFrameHoldContext(hold).iterateEntries([&](const musx::dom::EntryInfoPtr& entryInfo) {
                    found->second.insert(entryInfo->getEntry()->getEntryNumber());
                    return true;
                });
            }
        }
        return found->second.contains(entry);
    }

private:
    const musx::dom::DocumentPtr& m_document;
    std::map<musx::dom::StaffCmper, std::set<musx::dom::EntryNumber>> m_entries;
};

/// @brief The score's entry assignments, as the shape numbers on each entry in incidence order.
/// @details Lyric marks follow an entry's ordinary assignments, where Finale's conversion folds
/// them.
std::map<musx::dom::EntryNumber, std::vector<musx::dom::Cmper>> scoreEntryMarks(const ImportContext& context)
{
    std::map<musx::dom::EntryNumber, std::vector<musx::dom::Cmper>> result;
    constexpr std::size_t wordsPerMark = 5;
    for (const auto& [tag, classId] :
        {std::pair{records::packTag("Ex"), records::LegacyTag{0x0428}}, std::pair{records::packTag("Lx"), records::LegacyTag{0x0429}}}) {
        const auto source = selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), tag, classId, true);
        if (!source) {
            continue;
        }
        for (const auto& [partId, entryHigh] : recordKeys(*source)) {
            if (partId != musx::dom::SCORE_PARTID) {
                continue;
            }
            for (const auto entryLow : source->pool->secondCmpersForTag(source->identity, entryHigh, partId)) {
                const auto words =
                    collectRecordWords(*source, source->pool->getArray(source->identity, entryHigh, entryLow, partId), context.profile.byteOrder);
                auto& shapes = result[static_cast<musx::dom::EntryNumber>((static_cast<std::uint32_t>(entryHigh) << 16U) | entryLow)];
                for (std::size_t at = 0; at + wordsPerMark <= words.size(); at += wordsPerMark) {
                    shapes.push_back(static_cast<std::uint16_t>(words[at]));
                }
            }
        }
    }
    return result;
}

/// @brief The shapes Finale's file maintenance deletes for their marks on missing entries.
/// @details Finale visits each entry that has marks but does not exist, in entry order. When the
/// shape of its first mark also has a mark on an existing entry, it deletes only the missing
/// entry's marks; otherwise it deletes every shape marked on that entry.
std::set<musx::dom::Cmper> shapesDeletedForMissingEntries(const ImportContext& context, const std::set<musx::dom::EntryNumber>& entries)
{
    const auto marks = scoreEntryMarks(context);
    const auto exists = [&](musx::dom::EntryNumber number) { return number != 0 && entries.contains(number); };
    std::set<musx::dom::Cmper> anchored;
    for (const auto& [number, shapes] : marks) {
        if (exists(number)) {
            anchored.insert(shapes.begin(), shapes.end());
        }
    }
    std::set<musx::dom::Cmper> deleted;
    for (const auto& [number, shapes] : marks) {
        if (exists(number) || shapes.empty() || (anchored.contains(shapes.front()) && !deleted.contains(shapes.front()))) {
            continue;
        }
        deleted.insert(shapes.begin(), shapes.end());
    }
    return deleted;
}

/// @brief Reports every shape as recovered from the source, with the members no record supplies as unmapped.
void reportSmartShapes(const ImportContext& context)
{
    for (const auto& shape : context.document->getOthers()->getAllSources<Shape>()) {
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            const auto key = reporting.template instanceKey<Shape>(shape->getSourcePartId(), shape->getCmper());
            reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
            for (const char* name : {"shapeType", "entryBased", "rotate", "noPresetShape", "makeHorz", "noPushEndStart", "makeVert",
                     "engraverSlurState", "slurAvoidAcciState", "yBreakType", "hidden", "direction", "startNoteId", "endNoteId", "lineStyleId",
                     "startLyricNum", "endLyricNum", "startLyricType", "endLyricType"}) {
                reporting.unmappedField(key, name, 0);
            }
            for (const char* segment : {"startTermSeg", "endTermSeg"}) {
                for (const char* member : {"endPoint.staffId", "endPoint.measId", "endPoint.eduPosition", "endPoint.entryNumber",
                         "endPointAdj.horzOffset", "endPointAdj.vertOffset", "endPointAdj.active", "endPointAdj.contextDir",
                         "endPointAdj.contextEntCnct", "ctlPtAdj.startCtlPtX", "ctlPtAdj.startCtlPtY", "ctlPtAdj.endCtlPtX", "ctlPtAdj.endCtlPtY",
                         "ctlPtAdj.active", "ctlPtAdj.contextDir", "breakAdj.horzOffset", "breakAdj.vertOffset", "breakAdj.active",
                         "breakAdj.contextDir", "breakAdj.contextEntCnct"}) {
                    reporting.unmappedField(key, std::string(segment) + "." + member, 0);
                }
            }
            for (const char* member : {"startCtlPtX", "startCtlPtY", "endCtlPtX", "endCtlPtY", "active", "contextDir"}) {
                reporting.unmappedField(key, std::string("fullCtlPtAdj.") + member, 0);
            }
        });
    }
}

} // namespace

void importSmartShapes(const ImportContext& context)
{
    importEarlySmartShapes(context);
    if (context.profile.epoch == FormatEpoch::CodaBanner || context.profile.epoch == FormatEpoch::UncompressedLegacy) {
        context.pending.defer(DeferredStage::RefreshDerivedFlags, [&context] { refreshEarlyShapeMeasureFlags(context); });
    }
    // Which stored shapes Finale keeps depends on the staves their entries lie in, which the
    // document's frames settle once every importer has run.
    context.pending.defer(DeferredStage::ClaimStoredIds, [&context] {
        const auto& skipped = findSkippedSmartShapes(context);
        const auto skipRecord = [&](std::uint16_t partId, std::uint16_t cmper) { return skipped.contains({partId, cmper}); };
        applyMappingTables(
            {&fixedSmartShapeTable(), &smartShapeTable()}, context.index, context.profile, context.document, context.report, skipRecord);
        if (!skipped.empty()) {
            context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
                std::to_string(skipped.size())
                    + " entry-based SmartShape record(s) name an endpoint entry the file does not store, and Finale drops "
                      "them; they were skipped."});
        }
        importLyricTypes(context);
        inheritZeroPartShapeMetadata(context);
        reportSmartShapes(context);
    });
}

} // namespace others

const SmartShapeKeys& findSkippedSmartShapes(const ImportContext& context)
{
    if (context.pending.skippedSmartShapes) {
        return *context.pending.skippedSmartShapes;
    }
    auto& result = context.pending.skippedSmartShapes.emplace();
    const bool zlib = context.profile.epoch == FormatEpoch::ZlibLegacy;
    if (!zlib && !epochMatches(EpochMask::FixedRow, context.profile.epoch)) {
        return result;
    }
    const RecordFamilySource source = zlib ? RecordFamilySource{&context.index.getClassOthers(), others::smartShapeClass, true}
                                           : RecordFamilySource{&context.index.getOthers(), others::smartShapeTag, false};
    const auto entries = others::storedEntryNumbers(context);
    const auto deleted = others::shapesDeletedForMissingEntries(context, entries);
    others::StaffEntries staffEntries(context.document);
    for (const auto& [partId, cmper] : recordKeys(source)) {
        if (source.pool->getArray(source.identity, cmper, 0, partId).empty()) {
            continue;
        }
        auto endpoints = others::storedEntryEndpoints(context, musx::dom::SCORE_PARTID, cmper);
        if (!endpoints && partId != musx::dom::SCORE_PARTID) {
            endpoints = others::storedEntryEndpoints(context, partId, cmper);
        }
        const auto stored = [&](const others::EntryEndpoint& endpoint) { return endpoint.entry != 0 && entries.contains(endpoint.entry); };
        if (!endpoints || std::ranges::all_of(*endpoints, [&](const auto& endpoint) { return endpoint.entry == 0 || stored(endpoint); })) {
            continue;
        }
        // An early record that defines the same score shape keeps it, along with its objects.
        if (partId == musx::dom::SCORE_PARTID && context.profile.epoch == FormatEpoch::UncompressedLegacy
            && early_smart_shape::find(context.index.getOthers(), context.profile.epoch, cmper)) {
            continue;
        }
        // Finale also drops a shape whose stored entry lies outside the staff it names.
        const bool offStaff = std::ranges::any_of(
            *endpoints, [&](const auto& endpoint) { return stored(endpoint) && !staffEntries.holds(endpoint.staffId, endpoint.entry); });
        if (deleted.contains(cmper) || offStaff) {
            result.emplace(partId, cmper);
        }
    }
    return result;
}

} // namespace finale_mus_reader
