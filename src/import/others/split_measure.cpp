// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using SplitMeasureTarget = musx::dom::others::SplitMeasure;
constexpr records::LegacyTag splitMeasureTag = records::packTag("SM");
constexpr records::LegacyTag splitMeasureClass = 0x00dd;

} // namespace

void importSplitMeasures(const ImportContext& context)
{
    // Believed: the zlib class record retains the fixed-row array of signed 16-bit positions.
    // A small position followed by zero also fits a 32-bit element with a zero high word.
    const auto source =
        selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), splitMeasureTag, splitMeasureClass);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        auto target = createOthersRecordTarget<SplitMeasureTarget>(context.document, *source, rows.front(), cmper);
        bool terminated = false;
        for (const auto& row : rows) {
            const auto payload = source->pool->effectivePayloadOf(row);
            for (std::size_t offset = 0; offset + 2 <= payload.size() && !terminated; offset += 2) {
                const auto value = static_cast<std::int16_t>(payloadWord(payload, offset, context.profile.byteOrder));
                if (value == 0) {
                    terminated = true;
                    break;
                }
                target->values.push_back(value);
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                    const auto key = reporting.template instanceKey<SplitMeasureTarget>(partId, cmper);
                    reporting.report().setField(key, "values[" + std::to_string(target->values.size() - 1) + "]",
                        {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset + offset, value, source->identity});
                });
            }
        }
        if (target->values.empty()) {
            continue;
        }
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            reporting.report().setInstanceOrigin(reporting.template instanceKey<SplitMeasureTarget>(partId, cmper), Reporting::Origin::LegacyMus);
        });
        context.document->getOthers()->add(SplitMeasureTarget::XmlNodeName, std::move(target));
    }
}

} // namespace others
} // namespace finale_mus_reader
