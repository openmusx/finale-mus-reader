// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <memory>
#include <string>

#include "import/support/legacy_mapping.h"
#include "import/support/text_encoding.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using FretboardStyleTarget = musx::dom::others::FretboardStyle;
constexpr records::LegacyTag fretboardStyleClass = 0x0097;
constexpr records::LegacyTag fretboardStyleTag = records::packTag("ft");
constexpr std::size_t fretboardStyleRecordSize = 156;
constexpr std::size_t fretboardStyleNameOffset = 84;
constexpr std::size_t fretboardStyleNameSize = 48;
constexpr std::size_t fretboardStyleNumberTextOffset = 132;
constexpr std::size_t fretboardStyleNumberTextSize = 24;

void populateFretboardStyleFont(
    const ImportContext& context, const std::span<const std::uint8_t> payload, std::size_t at, std::shared_ptr<musx::dom::FontInfo>& target)
{
    target = std::make_shared<musx::dom::FontInfo>(context.document);
    target->fontId = context.construction.assignFontId(payloadWord(payload, at, context.profile.byteOrder));
    target->fontSize = static_cast<std::int16_t>(payloadWord(payload, at + 2, context.profile.byteOrder));
    target->setEnigmaStyles(payloadWord(payload, at + 4, context.profile.byteOrder));
}

void reportFretboardStyle(
    ImportReport& report, const FretboardStyleTarget& target, const records::LegacyRow& row, std::uint16_t partId, std::uint16_t cmper)
{
    withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<FretboardStyleTarget>(partId, cmper);
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        const auto report = [&](std::string member, auto value) {
            reporting.report().setField(key, std::move(member), {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset, value});
        };
        report("showLastFret", target.showLastFret);
        report("rotate", target.rotate);
        report("fingNumWhite", target.fingNumWhite);
        report("fingStrShapeId", target.fingStrShapeId);
        report("openStrShapeId", target.openStrShapeId);
        report("muteStrShapeId", target.muteStrShapeId);
        report("barreShapeId", target.barreShapeId);
        report("customShapeId", target.customShapeId);
        report("defNumFrets", target.defNumFrets);
        report("stringGap", target.stringGap);
        report("fretGap", target.fretGap);
        report("stringWidth", target.stringWidth);
        report("fretWidth", target.fretWidth);
        report("nutWidth", target.nutWidth);
        report("vertTextOff", target.vertTextOff);
        report("horzTextOff", target.horzTextOff);
        report("horzHandleOff", target.horzHandleOff);
        report("vertHandleOff", target.vertHandleOff);
        report("whiteout", target.whiteout);
        report("fretNumFont.fontId", target.fretNumFont->fontId);
        report("fretNumFont.fontSize", target.fretNumFont->fontSize);
        report("fretNumFont.efx", target.fretNumFont->getEnigmaStyles());
        report("fingNumFont.fontId", target.fingNumFont->fontId);
        report("fingNumFont.fontSize", target.fingNumFont->fontSize);
        report("fingNumFont.efx", target.fingNumFont->getEnigmaStyles());
        report("horzFingNumOff", target.horzFingNumOff);
        report("vertFingNumOff", target.vertFingNumOff);
    });
}

} // namespace

void importFretboardStyles(const ImportContext& context)
{
    const auto source =
        selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), fretboardStyleTag, fretboardStyleClass);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto payload = collectRecordPayload(*source, rows);
        if (payload.size() < fretboardStyleRecordSize) {
            continue;
        }
        auto target = createOthersRecordTarget<FretboardStyleTarget>(context.document, *source, rows.front(), cmper);
        if (!target) {
            continue;
        }
        target->showLastFret = payloadWord(payload, 0, context.profile.byteOrder);
        target->rotate = payloadWord(payload, 2, context.profile.byteOrder);
        target->fingNumWhite = payloadWord(payload, 4, context.profile.byteOrder);
        target->fingStrShapeId = payloadWord(payload, 6, context.profile.byteOrder);
        target->openStrShapeId = payloadWord(payload, 8, context.profile.byteOrder);
        target->muteStrShapeId = payloadWord(payload, 10, context.profile.byteOrder);
        target->barreShapeId = payloadWord(payload, 12, context.profile.byteOrder);
        target->customShapeId = payloadWord(payload, 14, context.profile.byteOrder);
        target->defNumFrets = payloadWord(payload, 16, context.profile.byteOrder);
        target->stringGap = payloadLong(payload, 18, context.profile.byteOrder, LongWordOrder::HighFirst);
        target->fretGap = payloadLong(payload, 22, context.profile.byteOrder, LongWordOrder::HighFirst);
        target->stringWidth = payloadLong(payload, 26, context.profile.byteOrder, LongWordOrder::HighFirst);
        target->fretWidth = payloadLong(payload, 30, context.profile.byteOrder, LongWordOrder::HighFirst);
        target->nutWidth = payloadLong(payload, 34, context.profile.byteOrder, LongWordOrder::HighFirst);
        target->vertTextOff = payloadLong(payload, 38, context.profile.byteOrder, LongWordOrder::HighFirst);
        target->horzTextOff = payloadLong(payload, 42, context.profile.byteOrder, LongWordOrder::HighFirst);
        target->horzHandleOff = payloadLong(payload, 46, context.profile.byteOrder, LongWordOrder::HighFirst);
        target->vertHandleOff = payloadLong(payload, 50, context.profile.byteOrder, LongWordOrder::HighFirst);
        target->whiteout = payloadLong(payload, 54, context.profile.byteOrder, LongWordOrder::HighFirst);
        populateFretboardStyleFont(context, payload, 58, target->fretNumFont);
        populateFretboardStyleFont(context, payload, 64, target->fingNumFont);
        target->horzFingNumOff = payloadLong(payload, 70, context.profile.byteOrder, LongWordOrder::HighFirst);
        target->vertFingNumOff = payloadLong(payload, 74, context.profile.byteOrder, LongWordOrder::HighFirst);
        target->name = text::toUtf8(payloadString(payload, fretboardStyleNameOffset, fretboardStyleNameSize), context.profile.platform);
        target->fretNumText =
            text::toUtf8(payloadString(payload, fretboardStyleNumberTextOffset, fretboardStyleNumberTextSize), context.profile.platform);
        reportFretboardStyle(context.report, *target, rows.front(), partId, cmper);
        context.document->getOthers()->add(FretboardStyleTarget::XmlNodeName, std::move(target));
    }
}

} // namespace others
} // namespace finale_mus_reader
