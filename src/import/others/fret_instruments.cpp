// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <memory>
#include <string>

#include "import/support/field_manifest.h"
#include "import/support/legacy_mapping.h"
#include "import/support/text_encoding.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using FretInstrumentTarget = musx::dom::others::FretInstrument;
constexpr records::LegacyTag fretInstrumentClass = 0x0095;
constexpr records::LegacyTag fretInstrumentTag = records::packTag("fI");
constexpr std::size_t fretInstrumentHeaderSize = 12;
constexpr std::size_t fretInstrumentNameSize = 48;
constexpr std::size_t fretInstrumentStringsOffset = fretInstrumentHeaderSize + fretInstrumentNameSize;

void reportFretInstrument(
    ImportReport& report, const FretInstrumentTarget& target, const records::LegacyRow& row, std::uint16_t partId, std::uint16_t cmper)
{
    withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<FretInstrumentTarget>(partId, cmper);
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        const auto stored = [&](std::string member, auto value) {
            reporting.report().setField(key, std::move(member), {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset, value});
        };
        const auto behavior = [&](std::string member, auto value) {
            reporting.report().setField(key, std::move(member), {Reporting::Origin::LegacyBehavior, row.blockOffset, row.decodedOffset, value});
        };
        reportFretInstrumentFields(target, true, stored, behavior);
    });
}

} // namespace

void importFretInstruments(const ImportContext& context)
{
    const auto source =
        selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), fretInstrumentTag, fretInstrumentClass);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto payload = collectRecordPayload(*source, rows);
        if (payload.size() < fretInstrumentStringsOffset) {
            continue;
        }
        auto target = createOthersRecordTarget<FretInstrumentTarget>(context.document, *source, rows.front(), cmper);
        if (!target) {
            continue;
        }
        const auto stepBits = static_cast<std::uint32_t>(payloadLong(payload, 0, context.profile.byteOrder, LongWordOrder::LowFirst));
        target->numFrets = payloadWord(payload, 4, context.profile.byteOrder);
        target->numStrings = payloadWord(payload, 6, context.profile.byteOrder);
        target->speedyClef = payloadWord(payload, 8, context.profile.byteOrder);
        target->name = text::toUtf8(payloadString(payload, fretInstrumentHeaderSize, fretInstrumentNameSize), context.profile.platform);
        const auto storedStrings = (payload.size() - fretInstrumentStringsOffset) / 2;
        const auto stringCount = (std::min)(static_cast<std::size_t>(target->numStrings), storedStrings);
        for (std::size_t index = 0; index < stringCount; ++index) {
            auto stringInfo = std::make_shared<FretInstrumentTarget::StringInfo>();
            // Old and class-record string tunings retain their little-endian byte encoding even
            // when the surrounding payload's numeric fields follow big-endian container order.
            stringInfo->pitch = static_cast<std::int16_t>(payloadWord(payload, fretInstrumentStringsOffset + index * 2, ByteOrder::LittleEndian));
            target->strings.push_back(std::move(stringInfo));
        }
        for (std::size_t bit = 0; bit < 32; ++bit) {
            if (stepBits & (std::uint32_t{1} << bit)) {
                target->fretSteps.push_back(static_cast<int>(bit + 1));
            }
        }
        reportFretInstrument(context.report, *target, rows.front(), partId, cmper);
        context.document->getOthers()->add(FretInstrumentTarget::XmlNodeName, std::move(target));
    }
}

} // namespace others
} // namespace finale_mus_reader
