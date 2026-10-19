// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <cstdint>
#include <string>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using FrameTarget = musx::dom::others::Frame;
constexpr auto frameTag = records::packTag("FR");
constexpr records::LegacyTag frameClass = 0x0092;
constexpr std::uint16_t frameSpacerMarker = 0x0800;

} // namespace

void importFrames(const ImportContext& context)
{
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), frameTag, frameClass);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        for (const auto& row : rows) {
            const auto payload = source->pool->effectivePayloadOf(row);
            if (payload.empty() || payload.size() % 12 != 0) {
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Warning, "Frame " + std::to_string(cmper) + " has an unsupported row length."});
                continue;
            }
            for (std::size_t offset = 0; offset < payload.size(); offset += 12) {
                const auto marker = payloadWord(payload, offset + 10, context.profile.byteOrder);
                if (marker != 0 && marker != frameSpacerMarker) {
                    context.report.diagnostics.push_back(
                        {musx::util::Logger::LogLevel::Warning, "Frame " + std::to_string(cmper) + " has an unknown incidence layout."});
                    continue;
                }
                const auto inci = static_cast<musx::dom::Inci>(row.inci + offset / 12);
                auto frame = createOthersRecordTarget<FrameTarget>(context.document, *source, row, cmper, inci);
                if (marker == frameSpacerMarker) {
                    frame->startTime = payloadLong(payload, offset, context.profile.byteOrder, nativeLongWordOrder(context.profile.byteOrder));
                } else {
                    frame->startEntry = payloadLong(payload, offset, context.profile.byteOrder, nativeLongWordOrder(context.profile.byteOrder));
                    frame->endEntry = payloadLong(payload, offset + 4, context.profile.byteOrder, nativeLongWordOrder(context.profile.byteOrder));
                }
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                    const auto key = reporting.template instanceKey<FrameTarget>(partId, cmper, inci);
                    reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
                    const auto legacy = [&](const char* name, std::size_t offset, std::int64_t value) {
                        reportLegacyField(reporting, key, *source, row, name, offset, value);
                    };
                    const auto fallback = [&](const char* name, std::int64_t value) {
                        reportFallbackField(reporting, key, name, Reporting::Origin::Finale27Default, value);
                    };
                    if (marker == frameSpacerMarker) {
                        legacy("startTime", offset, frame->startTime);
                        fallback("startEntry", frame->startEntry);
                        fallback("endEntry", frame->endEntry);
                    } else {
                        legacy("startEntry", offset, frame->startEntry);
                        legacy("endEntry", offset + 4, frame->endEntry);
                        fallback("startTime", frame->startTime);
                    }
                });
                context.document->getOthers()->add(FrameTarget::XmlNodeName, std::move(frame));
            }
        }
    }
}

} // namespace others
} // namespace finale_mus_reader
