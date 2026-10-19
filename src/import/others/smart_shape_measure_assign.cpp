// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <tuple>

#include "import/shared/skipped_smart_shapes.h"
#include "musx/musx.h"

namespace finale_mus_reader::others {
namespace {

using Target = musx::dom::others::SmartShapeMeasureAssign;
using Shape = musx::dom::others::SmartShape;
constexpr auto assignmentTag = records::packTag("Mx");
constexpr records::LegacyTag assignmentClass = 0x00da;
constexpr auto earlyAssignmentStartTag = records::packTag("sX");
constexpr std::size_t wordsPerAssignment = 6;

void reportStored(
    const ImportContext& context, const RecordFamilySource& source, std::span<const records::LegacyRow> rows, const Target& target, std::size_t at)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<Target>(target.getSourcePartId(), target.getCmper(), target.getInci());
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        reportLegacyField(reporting, key, source, source.rowOfWord(rows, at), "shapeNum", source.byteOffsetInRow(at * 2), target.shapeNum);
        reportLegacyField(
            reporting, key, source, source.rowOfWord(rows, at + 1), "centerShapeNum", source.byteOffsetInRow((at + 1) * 2), target.centerShapeNum);
    });
}

void importStored(const ImportContext& context)
{
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), assignmentTag, assignmentClass);
    if (!source) {
        return;
    }
    // An assignment of a shape the reader skips would name a shape that does not exist.
    const auto& skipped = findSkippedSmartShapes(context);
    for (const auto& [partId, measure] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, measure, 0, partId);
        const auto words = collectRecordWords(*source, rows, context.profile.byteOrder);
        if (words.size() % wordsPerAssignment != 0) {
            context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info, "SmartShape measure assignment has an incomplete tuple."});
        }
        musx::dom::Inci inci = 0;
        for (std::size_t at = 0; at + wordsPerAssignment <= words.size(); at += wordsPerAssignment) {
            if (namesSkippedSmartShape(skipped, partId, static_cast<std::uint16_t>(words[at]))) {
                continue;
            }
            auto target = createOthersRecordTarget<Target>(context.document, *source, source->rowOfWord(rows, at), measure, inci);
            target->shapeNum = static_cast<std::uint16_t>(words[at]);
            target->centerShapeNum = static_cast<std::uint16_t>(words[at + 1]);
            reportStored(context, *source, rows, *target, at);
            context.document->getOthers()->add(Target::XmlNodeName, std::move(target));
            ++inci;
        }
    }
}

void synthesizeEarly(const ImportContext& context)
{
    const auto& pool = context.index.getOthers();
    const RecordFamilySource source{&pool, earlyAssignmentStartTag, false};
    std::map<std::pair<musx::dom::Cmper, musx::dom::Cmper>, musx::dom::Inci> nextInci;
    std::set<std::tuple<musx::dom::Cmper, musx::dom::Cmper, musx::dom::Cmper>> assigned;
    for (const auto& shape : context.document->getOthers()->getAllSources<Shape>()) {
        const auto partId = shape->getSourcePartId();
        const auto shapeId = shape->getCmper();
        const auto starts = pool.getArray(earlyAssignmentStartTag, shapeId, 0, partId);
        if (starts.empty()) {
            continue;
        }
        for (const auto measure : {shape->startTermSeg->endPoint->measId, shape->endTermSeg->endPoint->measId}) {
            if (measure == 0 || !assigned.emplace(partId, measure, shapeId).second) {
                continue;
            }
            const auto inci = nextInci[{partId, measure}]++;
            auto target = createOthersRecordTarget<Target>(context.document, source, starts.front(), measure, inci);
            target->shapeNum = shapeId;
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<Target>(partId, measure, inci);
                reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMusAdjusted);
                reportLegacyField(reporting, key, source, starts.front(), "shapeNum", 0, shapeId);
                reportFallbackField(reporting, key, "centerShapeNum", Reporting::Origin::LegacyBehavior, 0);
            });
            context.document->getOthers()->add(Target::XmlNodeName, std::move(target));
        }
    }
}

} // namespace

void importSmartShapeMeasureAssigns(const ImportContext& context)
{
    // Assignments wait for the decision about which stored shapes are recovered.
    context.pending.defer(DeferredStage::ClaimStoredIds, [&context] { importStored(context); });
    // Earlier shapes name endpoint measures in sX/eX; their assignments have no Mx rows.
    context.pending.defer(DeferredStage::CompletePools, [&context] { synthesizeEarly(context); });
}

} // namespace finale_mus_reader::others
