// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"
#include "import/support/legacy_font.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "import/support/legacy_mapping.h"
#include "import/support/text_encoding.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using ChordSuffixElementTarget = musx::dom::others::ChordSuffixElement;

constexpr auto chordSuffixElementTag = records::packTag("IV");
constexpr records::LegacyTag chordSuffixElementClass = 0x007d;
constexpr std::size_t chordSuffixNarrowTupleSize = 12;
constexpr std::size_t chordSuffixWideTupleSize = 16;
constexpr std::size_t chordSuffixWideStride = 24;

constexpr std::uint16_t chordSuffixIsNumberMask = 0x0800;
constexpr std::uint16_t chordSuffixPrefixFlatMask = 0x0080;
constexpr std::uint16_t chordSuffixPrefixSharpMask = 0x0040;
constexpr std::uint16_t chordSuffixPrefixPlusMask = 0x0020;
constexpr std::uint16_t chordSuffixPrefixMinusMask = 0x0010;

ChordSuffixElementTarget::Prefix chordSuffixPrefix(std::uint16_t flags)
{
    using Prefix = ChordSuffixElementTarget::Prefix;
    if (flags & chordSuffixPrefixFlatMask) {
        return Prefix::Flat;
    }
    if (flags & chordSuffixPrefixSharpMask) {
        return Prefix::Sharp;
    }
    if (flags & chordSuffixPrefixPlusMask) {
        return Prefix::Plus;
    }
    if (flags & chordSuffixPrefixMinusMask) {
        return Prefix::Minus;
    }
    return Prefix::None;
}

void reportChordSuffixElement(const ImportContext& context, const ChordSuffixElementTarget& target, const RecordFamilySource& source,
    const records::LegacyRow& row, std::size_t tupleOffset, bool wide)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<ChordSuffixElementTarget>(target.getSourcePartId(), target.getCmper(), target.getInci());
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        const auto report = [&](const char* member, std::size_t fieldOffset, auto value) {
            reporting.report().setField(
                key, member, {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset + tupleOffset + fieldOffset, value, source.identity});
        };
        const auto symbolOffset = std::size_t{0};
        const auto xOffset = wide ? std::size_t{4} : std::size_t{2};
        const auto yOffset = wide ? std::size_t{6} : std::size_t{4};
        const auto fontIdOffset = wide ? std::size_t{8} : std::size_t{6};
        const auto fontSizeOffset = wide ? std::size_t{10} : std::size_t{6};
        const auto effectsOffset = wide ? std::size_t{12} : std::size_t{8};
        const auto flagsOffset = wide ? std::size_t{14} : std::size_t{10};
        report("symbol", symbolOffset, static_cast<std::uint32_t>(target.symbol));
        report("xdisp", xOffset, target.xdisp);
        report("ydisp", yOffset, target.ydisp);
        report("font.fontId", fontIdOffset, target.font->fontId);
        report("font.fontSize", fontSizeOffset, target.font->fontSize);
        report("font.bold", effectsOffset, target.font->bold);
        report("font.italic", effectsOffset, target.font->italic);
        report("font.underline", effectsOffset, target.font->underline);
        report("font.strikeout", effectsOffset, target.font->strikeout);
        report("font.absolute", effectsOffset, target.font->absolute);
        report("font.hidden", effectsOffset, target.font->hidden);
        report("isNumber", flagsOffset, target.isNumber);
        report("prefix", flagsOffset, static_cast<std::int64_t>(target.prefix));
    });
}

} // namespace

void importChordSuffixElements(const ImportContext& context)
{
    const auto source =
        selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), chordSuffixElementTag, chordSuffixElementClass);
    if (!source) {
        return;
    }

    const bool wide = source->classRecords && versions::storesUnicodeCodepoints(context.profile.version);
    const auto stride = wide ? chordSuffixWideStride : chordSuffixNarrowTupleSize;
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto payload = collectRecordPayload(*source, rows);
        if (payload.size() % stride != 0) {
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Info, "Chord suffix " + std::to_string(cmper) + " has an incomplete trailing element."});
        }
        for (std::size_t at = 0; at + stride <= payload.size(); at += stride) {
            const auto inci = static_cast<musx::dom::Inci>(at / stride);
            const auto& row = source->classRecords ? rows.front() : rows[inci];
            const auto rowOffset = source->classRecords ? at : 0;
            auto target = createOthersRecordTarget<ChordSuffixElementTarget>(context.document, *source, row, cmper, inci);
            if (!target) {
                continue;
            }

            std::uint16_t effects{};
            std::uint16_t flags{};
            if (wide) {
                target->symbol = static_cast<char32_t>(payloadLong(payload, at, context.profile.byteOrder, LongWordOrder::HighFirst));
                target->xdisp = static_cast<std::int16_t>(payloadWord(payload, at + 4, context.profile.byteOrder));
                target->ydisp = static_cast<std::int16_t>(payloadWord(payload, at + 6, context.profile.byteOrder));
                target->font->fontId = context.construction.assignFontId(payloadWord(payload, at + 8, context.profile.byteOrder));
                target->font->fontSize = static_cast<std::int16_t>(payloadWord(payload, at + 10, context.profile.byteOrder));
                effects = payloadWord(payload, at + 12, context.profile.byteOrder);
                flags = payloadWord(payload, at + 14, context.profile.byteOrder);
                if (!std::all_of(payload.begin() + static_cast<std::ptrdiff_t>(at + chordSuffixWideTupleSize),
                        payload.begin() + static_cast<std::ptrdiff_t>(at + stride), [](std::uint8_t value) { return value == 0; })) {
                    context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
                        "Chord suffix " + std::to_string(cmper) + " has nonzero trailing words in a Finale 2012 element."});
                }
            } else {
                const auto storedSymbol = payloadWord(payload, at, context.profile.byteOrder);
                target->xdisp = static_cast<std::int16_t>(payloadWord(payload, at + 2, context.profile.byteOrder));
                target->ydisp = static_cast<std::int16_t>(payloadWord(payload, at + 4, context.profile.byteOrder));
                const auto sizeFont = payloadWord(payload, at + 6, context.profile.byteOrder);
                assignPackedFont(*target->font, context.construction, sizeFont);
                effects = payloadWord(payload, at + 8, context.profile.byteOrder);
                flags = payloadWord(payload, at + 10, context.profile.byteOrder);
                target->symbol = text::codepointFromByte(
                    static_cast<std::uint8_t>(storedSymbol), context.document, target->font->fontId, text::UnresolvedFontFallback::Symbol);
            }
            target->font->setEnigmaStyles(effects);
            target->isNumber = (flags & chordSuffixIsNumberMask) != 0;
            target->prefix = chordSuffixPrefix(flags);
            reportChordSuffixElement(context, *target, *source, row, rowOffset, wide);
            context.document->getOthers()->add(ChordSuffixElementTarget::XmlNodeName, std::move(target));
        }
    }
}

} // namespace others
} // namespace finale_mus_reader
