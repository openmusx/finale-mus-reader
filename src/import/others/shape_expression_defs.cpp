// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"
#include "import/shared/expression_common.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using ShapeExpressionTarget = musx::dom::others::ShapeExpressionDef;
constexpr std::size_t extendedShapeHeaderSize = 36;

constexpr const char* shapeExpressionFields[] = {"shapeDef", "categoryId", "rehearsalMarkStyle", "value", "execShape", "auxData1", "playPass",
    "breakMmRest", "useAuxData", "masterShape", "noPrint", "noHorzStretch", "playbackType", "horzMeasExprAlign", "vertMeasExprAlign",
    "horzExprJustification", "measXAdjust", "yAdjustEntry", "yAdjustBaseline", "useCategoryFonts", "useCategoryPos", "description"};

void reportAbsentShapeExpressionFields(
    ImportReport& report, const ReportInstance& instance, bool hasCategory, bool hasRehearsalStyle, bool hasBreakMmRest, bool hasNoPrint)
{
    withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.instanceKey(instance);
        if (!hasCategory) {
            for (const auto* member : {"useCategoryFonts", "useCategoryPos"}) {
                reporting.report().setField(key, member, typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, 0});
            }
            reporting.report().setField(key, "description", typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, 0});
        }
        if (!hasRehearsalStyle) {
            reporting.report().setField(key, "rehearsalMarkStyle", typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, 0});
        }
        if (!hasBreakMmRest) {
            reporting.report().setField(key, "breakMmRest", typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, 0});
        }
        if (!hasNoPrint) {
            reporting.report().setField(key, "noPrint", typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, 0});
        }
    });
}

} // namespace

void importShapeExpressionDefs(const ImportContext& context)
{
    const bool extended = sourceAtOrAfter(context.profile, FormatEpoch::DclLegacy, versions::finale2004);
    const auto source =
        selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), shapeExpressionTag, shapeExpressionClass);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        bool completeRows = !rows.empty();
        for (std::size_t index = 0; index < rows.size() && !source->classRecords; ++index) {
            completeRows = completeRows && rows[index].inci == index;
        }
        const auto payload = collectRecordPayload(*source, rows);
        const auto words = payloadWords(payload, context.profile.byteOrder);
        if (!completeRows || payload.size() < (extended ? extendedShapeHeaderSize : 12)) {
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Warning, "Shape expression " + std::to_string(cmper) + " has an incomplete header."});
            continue;
        }
        auto target = createOthersRecordTarget<ShapeExpressionTarget>(context.document, *source, rows.front(), cmper);
        if (!target) {
            continue;
        }
        const auto reportInstance = ReportInstance::of<ShapeExpressionTarget>(partId, cmper);
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            const auto key = reporting.instanceKey(reportInstance);
            reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
            for (const auto* field : shapeExpressionFields) {
                reporting.report().setField(key, field, typename Reporting::FieldInfo{Reporting::Origin::Unmapped, 0, 0, 0});
            }
        });
        const auto assign = [&](auto member, const char* name, auto value, std::size_t slot, bool adjusted = false) {
            target.get()->*member = value;
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto& row = source->rowOfWord(rows, slot);
                const auto offset = source->byteOffsetInRow(slot * 2);
                reporting.report().setField(reporting.instanceKey(reportInstance), name,
                    typename Reporting::FieldInfo{adjusted ? Reporting::Origin::LegacyMusAdjusted : Reporting::Origin::LegacyMus, row.blockOffset,
                        row.decodedOffset + offset, words[slot], source->identity});
            });
        };
        const auto flags = static_cast<std::uint16_t>(words[5]);
        const bool hasCategory = sourceAtOrAfter(context.profile, FormatEpoch::ZlibLegacy, versions::finale2009);
        // Believed: shape and text expression rehearsal sequencing use the same introduction boundary.
        const bool hasRehearsalStyle = sourceAtOrAfter(context.profile, FormatEpoch::ZlibLegacy, versions::finale2010);
        const bool hasBreakMmRest = sourceAtOrAfter(context.profile, FormatEpoch::DclLegacy, versions::finale2002);
        const bool hasNoPrint = sourceAtOrAfter(context.profile, FormatEpoch::UncompressedLegacy, versions::finale97);
        reportAbsentShapeExpressionFields(context.report, reportInstance, hasCategory, hasRehearsalStyle, hasBreakMmRest, hasNoPrint);
        assign(&ShapeExpressionTarget::shapeDef, "shapeDef", static_cast<musx::dom::Cmper>(words[0]), 0);
        assignExpressionPlayback<ShapeExpressionTarget>(assign, words);
        if (hasBreakMmRest) {
            assign(&ShapeExpressionTarget::breakMmRest, "breakMmRest", bool(flags & 0x4000U), 5);
        }
        assign(&ShapeExpressionTarget::masterShape, "masterShape", bool(flags & 0x0800U), 5);
        if (hasNoPrint) {
            assign(&ShapeExpressionTarget::noPrint, "noPrint", bool(flags & 0x0400U), 5);
        }
        assign(&ShapeExpressionTarget::noHorzStretch, "noHorzStretch", bool(flags & 0x0100U), 5);
        if (hasRehearsalStyle) {
            if (auto style = expressionRehearsalStyle(words[1])) {
                assign(&ShapeExpressionTarget::rehearsalMarkStyle, "rehearsalMarkStyle", *style, 1);
            }
        }
        if (extended) {
            assignExpressionPositioning<ShapeExpressionTarget>(*target, assign, words, hasCategory);
            if (hasCategory) {
                recoverExpressionDescription(context, *source, rows, payload, target, reportInstance, extendedShapeHeaderSize);
            }
        } else {
            applyEarlyExpressionPositioning(context.report, reportInstance, *target);
        }
        if (!hasCategory) {
            scheduleExpressionMiscCategory(context, target);
        }
        context.document->getOthers()->add(ShapeExpressionTarget::XmlNodeName, std::move(target));
    }
}

} // namespace others
} // namespace finale_mus_reader
