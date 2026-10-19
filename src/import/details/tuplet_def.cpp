// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/details.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "musx/musx.h"

namespace finale_mus_reader::details {
namespace {

using TupletTarget = musx::dom::details::TupletDef;
constexpr auto tupletTag = records::packTag("TP");
constexpr auto tupletAppearanceTag = records::packTag("TN");
constexpr records::LegacyTag tupletClassId = 0x0430;
constexpr std::size_t fullTupletWords = 15;
constexpr std::size_t earlyTupletWords = 5;

TupletTarget::NumberStyle tupletNumberStyle(std::uint16_t raw)
{
    if (raw == 3) {
        return TupletTarget::NumberStyle::RatioPlusBothNotes;
    }
    if (raw == 4) {
        return TupletTarget::NumberStyle::RatioPlusDenominatorNote;
    }
    return static_cast<TupletTarget::NumberStyle>(raw);
}

std::uint16_t convertEarlyAppearanceFlags(std::uint16_t oldFlags)
{
    const auto oldStyle = oldFlags & 0x000fU;
    auto flags = static_cast<std::uint16_t>((oldFlags & 0xf0ffU) | 0x0400U);
    if (oldStyle != 0) {
        flags = static_cast<std::uint16_t>(flags | ((oldFlags & 0x0400U) != 0 ? 2U : 1U));
        if (oldStyle == 2) {
            flags = static_cast<std::uint16_t>(flags | 0x1000U);
        } else if (oldStyle == 3) {
            flags = static_cast<std::uint16_t>(flags | 0x2000U);
        }
    }
    return flags;
}

void importTupletSource(const ImportContext& context, const RecordFamilySource& source, const RecordFamilySource* appearanceSource)
{
    const bool modernFlags = source.classRecords || sourceAtOrAfter(context.profile, FormatEpoch::DclLegacy, versions::finale2005);
    for (const auto& [partId, high] : recordKeys(source)) {
        for (const auto low : source.pool->secondCmpersForTag(source.identity, high, partId)) {
            const auto rows = source.pool->getArray(source.identity, high, low, partId);
            const auto words = collectRecordWords(source, rows, context.profile.byteOrder);
            const auto appearanceRows = appearanceSource ? appearanceSource->pool->getArray(appearanceSource->identity, high, low, partId)
                                                         : std::span<const records::LegacyRow>{};
            const auto appearanceWords =
                appearanceSource ? collectRecordWords(*appearanceSource, appearanceRows, context.profile.byteOrder) : std::vector<std::int16_t>{};
            // Matching TN rows distinguish multiple short tuplets from full records.
            const bool early = words.size() == earlyTupletWords || !appearanceRows.empty();
            const auto stride = early ? earlyTupletWords : fullTupletWords;
            if (words.size() % stride != 0) {
                context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info, "Tuplet detail has an incomplete record."});
            }
            for (std::size_t at = 0; at + stride <= words.size(); at += stride) {
                const auto inci = static_cast<musx::dom::Inci>(at / stride);
                auto target = createDetailsRecordTarget<TupletTarget>(context.document, source, source.rowOfWord(rows, at), high, low, inci);
                const auto word = [&](std::size_t slot) { return static_cast<std::uint16_t>(words[at + slot]); };
                const auto signedWord = [&](std::size_t slot) { return static_cast<std::int16_t>(word(slot)); };
                const bool hasAppearance = early && at + earlyTupletWords <= appearanceWords.size();
                const auto appearanceWord = [&](std::size_t slot) {
                    return hasAppearance ? static_cast<std::uint16_t>(appearanceWords[at + slot]) : std::uint16_t{0};
                };
                const auto signedAppearanceWord = [&](std::size_t slot) { return static_cast<std::int16_t>(appearanceWord(slot)); };
                target->displayNumber = word(0);
                target->displayDuration = word(1);
                target->referenceNumber = word(2);
                target->referenceDuration = word(3);
                if (early) {
                    target->tupOffY = signedAppearanceWord(0);
                    target->brackOffY = signedAppearanceWord(2);
                    target->leftHookLen = signedAppearanceWord(1) - signedAppearanceWord(2);
                    target->rightHookLen = target->leftHookLen;
                    target->manualSlopeAdj = signedAppearanceWord(3) - signedAppearanceWord(1);
                } else {
                    if (modernFlags) {
                        const auto flags = word(4);
                        target->alwaysFlat = (flags & 0x0001U) != 0;
                        target->fullDura = (flags & 0x0002U) != 0;
                        target->metricCenter = (flags & 0x0004U) != 0;
                        target->avoidStaff = (flags & 0x0008U) != 0;
                    }
                    target->tupOffX = signedWord(5);
                    target->tupOffY = signedWord(6);
                    target->brackOffX = signedWord(7);
                    target->brackOffY = signedWord(8);
                    target->leftHookLen = signedWord(10);
                    target->leftHookExt = signedWord(11);
                    target->rightHookLen = signedWord(12);
                    target->rightHookExt = signedWord(13);
                    target->manualSlopeAdj = signedWord(14);
                }
                const auto flags = early ? convertEarlyAppearanceFlags(appearanceWord(4)) : word(9);
                target->numStyle = early ? static_cast<TupletTarget::NumberStyle>(flags & 0x000fU) : tupletNumberStyle(flags & 0x000fU);
                target->posStyle = static_cast<TupletTarget::PositioningStyle>((flags >> 4U) & 0x0007U);
                target->allowHorz = (flags & 0x0080U) != 0;
                target->ignoreHorzNumOffset = (flags & 0x0100U) != 0;
                target->breakBracket = (flags & 0x0200U) != 0;
                target->matchHooks = (flags & 0x0400U) != 0;
                target->useBottomNote = (flags & 0x0800U) != 0;
                target->brackStyle = static_cast<TupletTarget::BracketStyle>((flags >> 12U) & 0x0003U);
                // Conversion clears the Engraver Tuplet bit from records predating Finale 2005.
                target->smartTuplet = modernFlags && (flags & 0x4000U) != 0;
                target->autoBracketStyle = (flags & 0x8000U) == 0                              ? TupletTarget::AutoBracketStyle::Always
                                           : !early && modernFlags && (word(4) & 0x0010U) != 0 ? TupletTarget::AutoBracketStyle::NeverBeamSide
                                                                                               : TupletTarget::AutoBracketStyle::UnbeamedOnly;
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                    const auto key = reporting.template instanceKey<TupletTarget>(partId, high, inci, low);
                    reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
                    const auto mapped = [&](const char* name, std::size_t slot, std::int64_t raw) {
                        const auto position = at + slot;
                        reportLegacyField(reporting, key, source, source.rowOfWord(rows, position), name, source.byteOffsetInRow(position * 2), raw);
                    };
                    const auto unmapped = [&](const char* name, auto value) {
                        reportFallbackField(reporting, key, name, Reporting::Origin::Unmapped, static_cast<std::int64_t>(value));
                    };
                    const auto behavior = [&](const char* name, auto value) {
                        reportFallbackField(reporting, key, name, Reporting::Origin::LegacyBehavior, static_cast<std::int64_t>(value));
                    };
                    const auto appearance = [&](const char* name, std::size_t slot, auto value, bool adjusted = false) {
                        if (hasAppearance) {
                            const auto position = at + slot;
                            reportLegacyField(reporting, key, *appearanceSource, appearanceSource->rowOfWord(appearanceRows, position), name,
                                appearanceSource->byteOffsetInRow(position * 2), appearanceWord(slot),
                                adjusted ? Reporting::Origin::LegacyMusAdjusted : Reporting::Origin::LegacyMus);
                        } else {
                            behavior(name, value);
                        }
                    };
                    const auto secondary = [&](const char* name, std::int64_t value) {
                        if (modernFlags) {
                            mapped(name, 4, word(4));
                        } else {
                            behavior(name, value);
                        }
                    };
                    mapped("displayNumber", 0, word(0));
                    mapped("displayDuration", 1, word(1));
                    mapped("referenceNumber", 2, word(2));
                    mapped("referenceDuration", 3, word(3));
                    unmapped("hidden", target->hidden);
                    secondary("alwaysFlat", target->alwaysFlat);
                    secondary("fullDura", target->fullDura);
                    secondary("metricCenter", target->metricCenter);
                    secondary("avoidStaff", target->avoidStaff);
                    if (early) {
                        appearance("autoBracketStyle", 4, static_cast<std::int64_t>(target->autoBracketStyle));
                        behavior("tupOffX", target->tupOffX);
                        appearance("tupOffY", 0, target->tupOffY);
                        behavior("brackOffX", target->brackOffX);
                        appearance("brackOffY", 2, target->brackOffY);
                        appearance("numStyle", 4, static_cast<std::int64_t>(target->numStyle), true);
                        appearance("posStyle", 4, static_cast<std::int64_t>(target->posStyle));
                        appearance("allowHorz", 4, target->allowHorz);
                        behavior("ignoreHorzNumOffset", target->ignoreHorzNumOffset);
                        behavior("breakBracket", target->breakBracket);
                        behavior("matchHooks", target->matchHooks);
                        behavior("useBottomNote", target->useBottomNote);
                        appearance("brackStyle", 4, static_cast<std::int64_t>(target->brackStyle), true);
                        behavior("smartTuplet", target->smartTuplet);
                        appearance("leftHookLen", 1, target->leftHookLen, true);
                        behavior("leftHookExt", target->leftHookExt);
                        appearance("rightHookLen", 1, target->rightHookLen, true);
                        behavior("rightHookExt", target->rightHookExt);
                        appearance("manualSlopeAdj", 3, target->manualSlopeAdj, true);
                    } else {
                        mapped("autoBracketStyle", 9, word(9));
                        mapped("tupOffX", 5, word(5));
                        mapped("tupOffY", 6, word(6));
                        mapped("brackOffX", 7, word(7));
                        mapped("brackOffY", 8, word(8));
                        mapped("numStyle", 9, word(9));
                        mapped("posStyle", 9, word(9));
                        mapped("allowHorz", 9, word(9));
                        mapped("ignoreHorzNumOffset", 9, word(9));
                        mapped("breakBracket", 9, word(9));
                        mapped("matchHooks", 9, word(9));
                        mapped("useBottomNote", 9, word(9));
                        mapped("brackStyle", 9, word(9));
                        if (modernFlags) {
                            mapped("smartTuplet", 9, word(9));
                        } else {
                            behavior("smartTuplet", target->smartTuplet);
                        }
                        mapped("leftHookLen", 10, word(10));
                        mapped("leftHookExt", 11, word(11));
                        mapped("rightHookLen", 12, word(12));
                        mapped("rightHookExt", 13, word(13));
                        mapped("manualSlopeAdj", 14, word(14));
                    }
                });
                context.document->getDetails()->add(TupletTarget::XmlNodeName, std::move(target));
            }
        }
    }
}

} // namespace

void importTupletDefs(const ImportContext& context)
{
    const auto source =
        selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), tupletTag, tupletClassId, true);
    if (source) {
        const auto appearance =
            selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), tupletAppearanceTag, 0x0439, true);
        importTupletSource(context, *source, appearance ? &*appearance : nullptr);
    }
}

} // namespace finale_mus_reader::details
