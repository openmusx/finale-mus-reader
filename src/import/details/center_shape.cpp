// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/details.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "import/shared/early_smart_shape.h"
#include "import/shared/skipped_smart_shapes.h"
#include "import/shared/smart_shape_adjustment_flags.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace details {
namespace {

using CenterShapeTarget = musx::dom::details::CenterShape;
using SmartShapeTarget = musx::dom::others::SmartShape;
using MeasureAssignTarget = musx::dom::others::SmartShapeMeasureAssign;
constexpr auto centerShapeTag = records::packTag("Cx");
constexpr records::LegacyTag centerShapeClass = 0x0406;
constexpr std::size_t centerShapeWords = 15;

template <typename Reporting>
void reportCenterShape(Reporting& reporting, const RecordFamilySource& source, std::span<const records::LegacyRow> rows,
    std::span<const std::int16_t> words, const CenterShapeTarget& target)
{
    const auto key =
        reporting.template instanceKey<CenterShapeTarget>(target.getSourcePartId(), target.getCmper1(), std::nullopt, target.getCmper2());
    reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
    const auto stored = [&](const char* member, std::size_t slot, std::int64_t value) {
        reportLegacyField(reporting, key, source, source.rowOfWord(rows, slot), member, source.byteOffsetInRow(slot * 2), value);
    };
    stored("startBreakAdj.horzOffset", 0, target.startBreakAdj->horzOffset);
    stored("startBreakAdj.vertOffset", 1, target.startBreakAdj->vertOffset);
    for (const auto* member : {"startBreakAdj.active", "startBreakAdj.contextDir", "startBreakAdj.contextEntCnct"}) {
        stored(member, 2, words[2]);
    }
    stored("endBreakAdj.horzOffset", 3, target.endBreakAdj->horzOffset);
    stored("endBreakAdj.vertOffset", 4, target.endBreakAdj->vertOffset);
    for (const auto* member : {"endBreakAdj.active", "endBreakAdj.contextDir", "endBreakAdj.contextEntCnct"}) {
        stored(member, 5, words[5]);
    }
    stored("ctlPtAdj.startCtlPtX", 6, target.ctlPtAdj->startCtlPtX);
    stored("ctlPtAdj.startCtlPtY", 7, target.ctlPtAdj->startCtlPtY);
    stored("ctlPtAdj.endCtlPtX", 8, target.ctlPtAdj->endCtlPtX);
    stored("ctlPtAdj.endCtlPtY", 9, target.ctlPtAdj->endCtlPtY);
    stored("ctlPtAdj.active", 10, words[10]);
    stored("ctlPtAdj.contextDir", 10, words[10]);
}

void importCenterShapeFamily(const ImportContext& context, const RecordFamilySource& source)
{
    // A center of a shape the reader skips belongs to a shape that does not exist.
    const auto& skipped = findSkippedSmartShapes(context);
    for (const auto& [partId, shapeId] : recordKeys(source)) {
        if (namesSkippedSmartShape(skipped, partId, shapeId)) {
            continue;
        }
        for (const auto centerId : source.pool->secondCmpersForTag(source.identity, shapeId, partId)) {
            const auto rows = source.pool->getArray(source.identity, shapeId, centerId, partId);
            const auto words = collectRecordWords(source, rows, context.profile.byteOrder);
            if (words.size() < centerShapeWords) {
                context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
                    "Center shape for shape " + std::to_string(shapeId) + ", center " + std::to_string(centerId) + " is truncated."});
                continue;
            }
            auto target = createDetailsRecordTarget<CenterShapeTarget>(context.document, source, rows.front(), shapeId, centerId);
            target->integrityCheck(target);
            target->startBreakAdj->horzOffset = words[0];
            target->startBreakAdj->vertOffset = words[1];
            target->startBreakAdj->active = smart_shape_adjustment_flags::active(words[2]);
            target->startBreakAdj->contextDir = smart_shape_adjustment_flags::direction(words[2]);
            target->startBreakAdj->contextEntCnct = smart_shape_adjustment_flags::entryConnection(words[2]);
            target->endBreakAdj->horzOffset = words[3];
            target->endBreakAdj->vertOffset = words[4];
            target->endBreakAdj->active = smart_shape_adjustment_flags::active(words[5]);
            target->endBreakAdj->contextDir = smart_shape_adjustment_flags::direction(words[5]);
            target->endBreakAdj->contextEntCnct = smart_shape_adjustment_flags::entryConnection(words[5]);
            target->ctlPtAdj->startCtlPtX = words[6];
            target->ctlPtAdj->startCtlPtY = words[7];
            target->ctlPtAdj->endCtlPtX = words[8];
            target->ctlPtAdj->endCtlPtY = words[9];
            target->ctlPtAdj->active = smart_shape_adjustment_flags::active(words[10]);
            target->ctlPtAdj->contextDir = smart_shape_adjustment_flags::direction(words[10]);
            withReporting(
                context.report, [&]<typename Reporting>(Reporting& reporting) { reportCenterShape(reporting, source, rows, words, *target); });
            context.document->getDetails()->add(CenterShapeTarget::XmlNodeName, std::move(target));
        }
    }
}

struct EarlyCenterSource
{
    const RecordFamilySource& source;
    const records::LegacyRow* row{};
};

template <typename Reporting>
void reportEarlyCenterShape(
    Reporting& reporting, const CenterShapeTarget& target, EarlyCenterSource center, EarlyCenterSource breaks, EarlyCenterSource curve)
{
    const auto key =
        reporting.template instanceKey<CenterShapeTarget>(target.getSourcePartId(), target.getCmper1(), std::nullopt, target.getCmper2());
    reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMusAdjusted);
    const auto behavior = [&](const char* member, std::int64_t value) {
        reportFallbackField(reporting, key, member, Reporting::Origin::LegacyBehavior, value);
    };
    const auto stored = [&](const char* member, EarlyCenterSource from, std::size_t word, std::int64_t value, typename Reporting::Origin origin) {
        if (!from.row) {
            behavior(member, value);
            return;
        }
        reportLegacyField(
            reporting, key, from.source, *from.row, member, word * 2, origin == Reporting::Origin::LegacyMus ? value : from.row->words[word], origin);
    };
    // With break details, each vertical offset adds the detail's value to the center row's.
    const auto adjusted = breaks.row ? Reporting::Origin::LegacyMusAdjusted : Reporting::Origin::LegacyMus;
    stored("startBreakAdj.horzOffset", breaks, 0, target.startBreakAdj->horzOffset, Reporting::Origin::LegacyMus);
    stored("startBreakAdj.vertOffset", breaks.row ? breaks : center, breaks.row ? 1 : 2, target.startBreakAdj->vertOffset, adjusted);
    stored("endBreakAdj.horzOffset", breaks, 2, target.endBreakAdj->horzOffset, Reporting::Origin::LegacyMus);
    stored("endBreakAdj.vertOffset", breaks.row ? breaks : center, breaks.row ? 3 : 2, target.endBreakAdj->vertOffset, adjusted);
    for (const char* member : {"startBreakAdj", "endBreakAdj"}) {
        const std::string prefix(member);
        behavior((prefix + ".active").c_str(), true);
        behavior((prefix + ".contextDir").c_str(), 0);
        behavior((prefix + ".contextEntCnct").c_str(), 0);
    }
    const bool computed = curve.row && target.ctlPtAdj->active;
    const bool slur = computed && (target.ctlPtAdj->startCtlPtX != 0 || target.ctlPtAdj->endCtlPtX != 0);
    const auto point = [&](const char* member, bool fromCurve, std::size_t word, std::int64_t value) {
        stored(member, fromCurve ? curve : EarlyCenterSource{curve.source}, word, value, Reporting::Origin::LegacyMusAdjusted);
    };
    point("ctlPtAdj.startCtlPtX", slur, 0, target.ctlPtAdj->startCtlPtX);
    point("ctlPtAdj.startCtlPtY", computed, 3, target.ctlPtAdj->startCtlPtY);
    point("ctlPtAdj.endCtlPtX", slur, 1, target.ctlPtAdj->endCtlPtX);
    point("ctlPtAdj.endCtlPtY", slur, 3, target.ctlPtAdj->endCtlPtY);
    behavior("ctlPtAdj.active", target.ctlPtAdj->active);
    behavior("ctlPtAdj.contextDir", 0);
}

template <typename Reporting>
void reportSynthesizedCenterAssignment(Reporting& reporting, const MeasureAssignTarget& target)
{
    const auto key = reporting.template instanceKey<MeasureAssignTarget>(target.getSourcePartId(), target.getCmper(), target.getInci());
    reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMusAdjusted);
    reportFallbackField(reporting, key, "shapeNum", Reporting::Origin::LegacyMusAdjusted, target.shapeNum);
    reportFallbackField(reporting, key, "centerShapeNum", Reporting::Origin::LegacyBehavior, target.centerShapeNum);
}

// Before the fixed shape record, a shape's center in a measure comes from that measure's center
// row with nonzero break details, or, in an interior measure without them, from the center row's
// or else the start row's vertical offset. An endpoint measure without break details has no center.
void synthesizeEarlyCenterShapes(const ImportContext& context)
{
    namespace early = early_smart_shape;
    const auto& others = context.index.getOthers();
    const auto& details = context.index.getDetails();
    const RecordFamilySource rowSource{&others, early::endpointTag, false};
    const RecordFamilySource breakSource{&details, early::centerBreakTag, false};
    const RecordFamilySource curveSource{&details, early::centerCurveTag, false};
    std::map<std::pair<musx::dom::Cmper, musx::dom::Cmper>, musx::dom::Cmper> nextCenterId;
    for (const auto& center : context.document->getDetails()->getAllSources<CenterShapeTarget>()) {
        auto& next = nextCenterId[{center->getSourcePartId(), center->getCmper1()}];
        next = (std::max)(next, static_cast<musx::dom::Cmper>(center->getCmper2() + 1));
    }
    std::map<std::pair<musx::dom::Cmper, musx::dom::Cmper>, musx::dom::Inci> nextInci;
    for (const auto& assignment : context.document->getOthers()->getAllSources<MeasureAssignTarget>()) {
        auto& next = nextInci[{assignment->getSourcePartId(), assignment->getCmper()}];
        next = (std::max)(next, static_cast<musx::dom::Inci>(assignment->getInci().value_or(0) + 1));
    }
    for (const auto& shape : context.document->getOthers()->getAllSources<SmartShapeTarget>()) {
        const auto partId = shape->getSourcePartId();
        const auto shapeId = shape->getCmper();
        const auto records = partId == musx::dom::SCORE_PARTID ? early::find(others, context.profile.epoch, shapeId) : std::nullopt;
        if (!records) {
            continue;
        }
        auto& centerId = nextCenterId[{partId, shapeId}];
        if (centerId == 0) {
            centerId = 1;
        }
        for (auto measure = records->startMeasure(); measure <= records->endMeasure(); ++measure) {
            const auto* row = early::findCenter(others, context.profile.epoch, shapeId, measure);
            const bool hasDetails = row && row->words[1] != 0;
            const bool interior = measure > records->startMeasure() && measure < records->endMeasure();
            if (!hasDetails && !interior) {
                continue;
            }
            const auto detailId = hasDetails ? static_cast<std::uint16_t>(row->words[1]) : std::uint16_t{0};
            const auto* breaks = hasDetails ? details.get(early::centerBreakTag, shapeId, detailId, 0) : nullptr;
            const auto* curve = hasDetails ? details.get(early::centerCurveTag, shapeId, detailId, 0) : nullptr;
            const auto& vertical = row ? *row : *records->startEndpoint;
            const auto word = [](const records::LegacyRow* from, std::size_t at) -> std::int16_t { return from ? from->words[at] : 0; };

            auto center = std::make_shared<CenterShapeTarget>(context.document, partId, shape->getShareMode(), shapeId, centerId);
            center->integrityCheck(center);
            center->startBreakAdj->horzOffset = word(breaks, 0);
            center->startBreakAdj->vertOffset = static_cast<std::int16_t>(vertical.words[2] + word(breaks, 1));
            center->startBreakAdj->active = true;
            center->endBreakAdj->horzOffset = word(breaks, 2);
            center->endBreakAdj->vertOffset = static_cast<std::int16_t>(vertical.words[2] + word(breaks, 3));
            center->endBreakAdj->active = true;
            early_smart_shape::convertControlPoints(*center->ctlPtAdj, shape->shapeType, word(curve, 0), word(curve, 1), word(curve, 3));
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                reportEarlyCenterShape(reporting, *center, {rowSource, &vertical}, {breakSource, breaks}, {curveSource, curve});
            });
            context.document->getDetails()->add(CenterShapeTarget::XmlNodeName, std::move(center));

            const auto measureId = static_cast<musx::dom::Cmper>(measure);
            if (!interior) {
                // The endpoint assignment already exists; it now names this center.
                for (const auto& assignment : context.document->getOthers()->getArray<MeasureAssignTarget>(partId, measureId)) {
                    if (assignment->shapeNum == shapeId && assignment->centerShapeNum == 0) {
                        std::const_pointer_cast<MeasureAssignTarget>(assignment)->centerShapeNum = centerId;
                        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                            const auto key = reporting.template instanceKey<MeasureAssignTarget>(partId, measureId, assignment->getInci());
                            reportFallbackField(reporting, key, "centerShapeNum", Reporting::Origin::LegacyBehavior, centerId);
                        });
                        break;
                    }
                }
                ++centerId;
                continue;
            }
            const auto inci = nextInci[{partId, measureId}]++;
            auto assignment = std::make_shared<MeasureAssignTarget>(context.document, partId, shape->getShareMode(), measureId, inci);
            assignment->shapeNum = shapeId;
            assignment->centerShapeNum = centerId++;
            withReporting(
                context.report, [&]<typename Reporting>(Reporting& reporting) { reportSynthesizedCenterAssignment(reporting, *assignment); });
            context.document->getOthers()->add(MeasureAssignTarget::XmlNodeName, std::move(assignment));
        }
    }
}

} // namespace

void importCenterShapes(const ImportContext& context)
{
    const auto source =
        selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), centerShapeTag, centerShapeClass, true);
    if (source) {
        // Centers wait for the decision about which stored shapes are recovered.
        context.pending.defer(DeferredStage::ClaimStoredIds, [&context, source = *source] { importCenterShapeFamily(context, source); });
    }
    context.pending.defer(DeferredStage::Synthesize, [&context] { synthesizeEarlyCenterShapes(context); });
}

} // namespace details
} // namespace finale_mus_reader
