// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using TempoTarget = musx::dom::others::TempoChange;

constexpr records::LegacyTag tempoTag = records::packTag("AC");
constexpr records::LegacyTag tempoClass = 0x00f0;
constexpr std::size_t tempoSize = 12;

void importTempoRecord(const ImportContext& context, const RecordFamilySource& source, const records::LegacyRow& row, musx::dom::Cmper cmper,
    musx::dom::Inci inci, std::span<const std::uint8_t> payload, std::size_t offset)
{
    auto target = createOthersRecordTarget<TempoTarget>(context.document, source, row, cmper, inci);
    if (!target) {
        return;
    }
    const auto order = nativeLongWordOrder(context.profile.byteOrder);
    target->eduPosition = payloadLong(payload, offset, context.profile.byteOrder, order);
    target->ratio = payloadLong(payload, offset + 4, context.profile.byteOrder, order);
    target->unit = static_cast<std::int16_t>(payloadWord(payload, offset + 8, context.profile.byteOrder));
    const auto flags = payloadWord(payload, offset + 10, context.profile.byteOrder);
    target->isRelative = (flags & 0x0001) != 0;

    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<TempoTarget>(row.partId, cmper, inci);
        const auto reportField = [&](const char* member, std::size_t at, std::int64_t value) {
            reportLegacyField(reporting, key, source, row, member, at, value);
        };
        reportField("eduPosition", offset, target->eduPosition);
        reportField("ratio", offset + 4, target->ratio);
        reportField("unit", offset + 8, target->unit);
        reportField("isRelative", offset + 10, target->isRelative);
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
    });
    context.document->getOthers()->add(TempoTarget::XmlNodeName, std::move(target));
}

} // namespace

void importTempoChanges(const ImportContext& context)
{
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), tempoTag, tempoClass);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        for (const auto& row : rows) {
            const auto payload = source->pool->effectivePayloadOf(row);
            const auto count = source->classRecords ? payload.size() / tempoSize : std::size_t{1};
            if (payload.size() < tempoSize || (source->classRecords && payload.size() % tempoSize != 0)) {
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Info, "Tempo change record for measure " + std::to_string(cmper) + " has an incomplete element."});
            }
            for (std::size_t element = 0; element < count; ++element) {
                const auto offset = source->classRecords ? element * tempoSize : 0;
                importTempoRecord(context, *source, row, cmper, static_cast<musx::dom::Inci>(row.inci + element), payload, offset);
            }
        }
    }
}

} // namespace others
} // namespace finale_mus_reader
