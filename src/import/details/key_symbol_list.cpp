// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/details.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "import/support/text_encoding.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace details {
namespace {

using KeySymbolListTarget = musx::dom::details::KeySymbolListElement;
constexpr auto keySymbolListTag = records::packTag("KS");
constexpr records::LegacyTag keySymbolListClass = 0x0416;

musx::dom::Cmper keySymbolFont(const musx::dom::DocumentPtr& document, const KeySymbolListTarget& target)
{
    for (const auto& attributes : document->getOthers()->getAllSources<musx::dom::others::KeyAttributes>()) {
        if (attributes->symbolList == target.getCmper1() && attributes->fontSym != 0) {
            return attributes->fontSym;
        }
    }
    const auto font = musx::dom::options::FontOptions::getFontInfoOrNull(document, musx::dom::options::FontOptions::FontType::Key);
    return font ? font->fontId : musx::dom::DEFAULT_MUSIC_FONT_ID;
}

} // namespace

void importKeySymbolListElements(const ImportContext& context)
{
    using Target = musx::dom::details::KeySymbolListElement;
    const auto source =
        selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), keySymbolListTag, keySymbolListClass, true);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper1] : recordKeys(*source)) {
        for (const auto cmper2 : source->pool->secondCmpersForTag(source->identity, cmper1, partId)) {
            const auto rows = source->pool->getArray(source->identity, cmper1, cmper2, partId);
            if (rows.empty()) {
                continue;
            }
            auto payload = collectRecordPayload(*source, rows);
            constexpr std::size_t storedStringSize = records::detailWordCount * 2;
            if (payload.size() < storedStringSize) {
                continue;
            }
            // Coda stores this particular byte string low-byte-first within logical words.
            // Big-endian containers therefore need each pair restored before null termination;
            // ordinary text payloads and every later KeySymbolList layout remain byte streams.
            if (context.profile.epoch == FormatEpoch::CodaBanner && context.profile.byteOrder == ByteOrder::BigEndian) {
                for (std::size_t at = 0; at < storedStringSize; at += 2) {
                    std::swap(payload[at], payload[at + 1]);
                }
            }
            auto target = createDetailsRecordTarget<Target>(context.document, *source, rows.front(), cmper1, cmper2);
            if (!target) {
                continue;
            }
            const auto stored = payloadString(payload, 0, storedStringSize);
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<Target>(partId, cmper1, std::nullopt, cmper2);
                reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
                reporting.report().setField(key, "accidentalString",
                    {Reporting::Origin::LegacyMus, rows.front().blockOffset, rows.front().decodedOffset, 0, source->identity});
            });
            context.document->getDetails()->add(Target::XmlNodeName, target);
            context.pending.defer(DeferredStage::ResolveStoredRefs, [&context, target, stored] {
                target->accidentalString =
                    text::toUtf8(stored, context.document, keySymbolFont(context.document, *target), text::UnresolvedFontFallback::Symbol);
            });
        }
    }
}

} // namespace details
} // namespace finale_mus_reader
