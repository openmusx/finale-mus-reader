// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/details.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "import/shared/graphic_assignment.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace details {
namespace {

using MeasureGraphicTarget = musx::dom::details::MeasureGraphicAssign;
constexpr auto measureGraphicAssignTag = records::packTag("mg");
constexpr records::LegacyTag measureGraphicAssignClass = 0x041d;
constexpr std::size_t measureGraphicAssignWordCount =
    ((graphicAssignmentWordCount + records::detailWordCount - 1) / records::detailWordCount) * records::detailWordCount;

// Called from inside a reporting callback, so it takes the callback's writer rather than opening
// a nested one.
template <typename Reporting>
void reportMeasureGraphicValue(Reporting& reporting, musx::dom::Cmper staffId, musx::dom::Cmper meas, musx::dom::Inci inci, std::uint16_t partId,
    const char* name, std::int64_t value, const records::LegacyRow& row)
{
    reporting.report().setField(reporting.template instanceKey<MeasureGraphicTarget>(partId, staffId, inci, meas), name,
        {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset, value});
}

void reportMeasureGraphicTuple(const ImportContext& context, const RecordFamilySource& source, std::span<const records::LegacyRow> rows,
    std::span<const std::int16_t> tuple, std::size_t at, std::uint16_t partId, musx::dom::Cmper staffId, musx::dom::Cmper meas, musx::dom::Inci inci)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        constexpr std::size_t slots[] = {0, 1, 2, 3, 4, 5, 7, 11, 12, 13, 17};
        constexpr const char* names[] = {
            "version", "left", "bottom", "width", "height", "fDescId", "hidden", "savedRecord", "origWidth", "origHeight", "graphicCmper"};
        for (std::size_t index = 0; index < std::size(slots); ++index) {
            const auto slot = slots[index];
            const auto& sourceRow = source.rowOfWord(rows, at + slot);
            reportMeasureGraphicValue(reporting, staffId, meas, inci, partId, names[index], tuple[slot], sourceRow);
        }
        const auto reportInstance = reporting.template instanceKey<MeasureGraphicTarget>(partId, staffId, inci, meas);
        reporting.report().setInstanceOrigin(reportInstance, Reporting::Origin::LegacyMus);
        const auto& positionRow = source.rowOfWord(rows, at + 8);
        for (const auto* member : {"hAlign", "vAlign", "posFrom", "fixedPerc"}) {
            reportMeasureGraphicValue(reporting, staffId, meas, inci, partId, member, tuple[8], positionRow);
        }
    });
}

void importMeasureGraphicFamily(const ImportContext& context, const RecordFamilySource& source)
{
    for (const auto& [partId, staffId] : recordKeys(source)) {
        for (const auto meas : source.pool->secondCmpersForTag(source.identity, staffId, partId)) {
            const auto rows = source.pool->getArray(source.identity, staffId, meas, partId);
            const auto words = collectRecordWords(source, rows, context.profile.byteOrder);
            if (words.size() % measureGraphicAssignWordCount != 0) {
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Info, "Measure graphic assignment for staff " + std::to_string(staffId) + ", measure "
                                                             + std::to_string(meas) + " has an incomplete trailing tuple."});
            }
            for (std::size_t at = 0; at + measureGraphicAssignWordCount <= words.size(); at += measureGraphicAssignWordCount) {
                const auto inci = static_cast<musx::dom::Inci>(at / measureGraphicAssignWordCount);
                auto target = createDetailsRecordTarget<MeasureGraphicTarget>(context.document, source, rows.front(), staffId, meas, inci);
                if (!target) {
                    continue;
                }
                const std::span<const std::int16_t> tuple(words.data() + at, measureGraphicAssignWordCount);
                populateGraphicAssignmentCommon(*target, tuple);
                populateGraphicAssignmentPosition<true>(*target, static_cast<std::uint16_t>(tuple[8]));
                reportMeasureGraphicTuple(context, source, rows, tuple, at, partId, staffId, meas, inci);
                context.document->getDetails()->add(MeasureGraphicTarget::XmlNodeName, std::move(target));
            }
        }
    }
}

} // namespace

void importMeasureGraphicAssignments(const ImportContext& context)
{
    // The fixed-row selection also accepts a normalized Coda-banner mg family when present.
    const auto source = selectRecordFamilySource(
        context, context.index.getDetails(), context.index.getClassDetails(), measureGraphicAssignTag, measureGraphicAssignClass, true);
    if (source) {
        importMeasureGraphicFamily(context, *source);
    }
}

} // namespace details
} // namespace finale_mus_reader
