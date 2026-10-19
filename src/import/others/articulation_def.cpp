// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "import/support/text_encoding.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using ArticulationTarget = musx::dom::others::ArticulationDef;
constexpr auto articulationTag = records::packTag("IX");
constexpr records::LegacyTag articulationClass = 0x0079;
constexpr std::size_t articulationShortBytes = 36;
constexpr std::size_t articulationLegacyBytes = 48;
constexpr std::size_t articulationWideBytes = 60;
constexpr std::size_t verticalPositionSlot = 10;
constexpr std::uint16_t defaultVerticalPosition = 24;

std::array<std::uint8_t, 2> payloadBytes(std::uint16_t value, ByteOrder byteOrder)
{
    const auto high = static_cast<std::uint8_t>(value >> 8U);
    const auto low = static_cast<std::uint8_t>(value);
    return byteOrder == ByteOrder::BigEndian ? std::array<std::uint8_t, 2>{high, low} : std::array<std::uint8_t, 2>{low, high};
}

} // namespace

void importArticulationDefs(const ImportContext& context)
{
    const auto source =
        selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), articulationTag, articulationClass);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const bool codaBanner = context.profile.epoch == FormatEpoch::CodaBanner;
        const bool firstRowOnly = codaBanner && !source->classRecords && rows.size() == 1;
        const bool codaOptions = firstRowOnly;
        auto payload = collectRecordPayload(*source, rows);
        const bool shortLegacy = payload.size() == articulationShortBytes && (source->classRecords || rows.size() == 3);
        // Finale loads a classic definition as four rows and leaves absent ones zero.
        const bool padded = !codaBanner && !source->classRecords && rows.size() < 3;
        bool contiguous = source->classRecords || rows.size() == 4 || shortLegacy || firstRowOnly || padded;
        for (std::size_t i = 0; i < rows.size() && !source->classRecords; ++i) {
            contiguous = contiguous && rows[i].inci == i;
        }
        if (padded && contiguous && payload.size() == rows.size() * records::otherInciByteCount) {
            payload.resize(articulationLegacyBytes, 0);
            if (rows.size() == 1 && sourcePredatesVersion(context.profile, FormatEpoch::UncompressedLegacy, VersionBound{3, 3})) {
                // Before Finale 3.3, a single-row definition reuses its main symbol as the
                // alternate and takes the default vertical position.
                std::copy_n(payload.begin(), 4, payload.begin() + records::otherInciByteCount);
                const auto position = payloadBytes(defaultVerticalPosition, context.profile.byteOrder);
                std::copy(position.begin(), position.end(), payload.begin() + verticalPositionSlot * 2);
            }
        }
        const bool wide = source->classRecords && payload.size() == articulationWideBytes;
        if ((!source->classRecords
                && (!contiguous
                    || payload.size()
                           != (firstRowOnly  ? 12
                               : shortLegacy ? articulationShortBytes
                                             : articulationLegacyBytes)))
            || (source->classRecords && !shortLegacy && payload.size() != articulationLegacyBytes && !wide)) {
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Info, "Articulation definition " + std::to_string(cmper) + " has an unsupported layout."});
            continue;
        }
        auto target = createOthersRecordTarget<ArticulationTarget>(context.document, *source, rows.front(), cmper);
        if (!target) {
            continue;
        }
        const auto word = [&](std::size_t slot) { return payloadWord(payload, slot * 2, context.profile.byteOrder); };
        const auto signedWord = [&](std::size_t slot) { return static_cast<std::int16_t>(word(slot)); };
        // Multi-row Coda definitions carry the later placement fields but retain first-row playback controls.
        const bool codaExpanded = codaBanner && !firstRowOnly;
        const bool earlyPlayback =
            codaExpanded || (!codaBanner && sourcePredatesVersion(context.profile, FormatEpoch::UncompressedLegacy, VersionBound{3, 3}));
        std::uint16_t convertedPlaybackFlags = word(5);
        std::uint16_t convertedBottomValue = word(2);
        std::uint16_t convertedTopValue = word(3);
        if (codaExpanded) {
            const auto oldFlags = convertedPlaybackFlags;
            convertedPlaybackFlags &= 0xf0ffU;
            if (convertedBottomValue != 0 || convertedTopValue != 0) {
                convertedPlaybackFlags |= (oldFlags & 0x0800U) == 0 ? 0x0200U : (oldFlags & 0x0200U) != 0 ? 0x0400U : 0x0600U;
                if ((oldFlags & 0x0100U) != 0 && (convertedBottomValue & 0xffU) != 0) {
                    const auto numerator = convertedBottomValue >> 8;
                    const auto denominator = convertedBottomValue & 0xffU;
                    const auto percent = static_cast<std::uint16_t>((numerator * 100U + denominator / 2U) / denominator);
                    convertedBottomValue = percent;
                    convertedTopValue = percent;
                    convertedPlaybackFlags |= 0x0100U;
                }
            }
        }
        const auto oldPlaybackFlags = earlyPlayback ? convertedPlaybackFlags : std::uint16_t{};
        const auto oldPlaybackMode = oldPlaybackFlags & 0x0600U;
        const bool oldPlaybackPercent = (oldPlaybackFlags & 0x0100U) != 0;
        const bool oldPlaybackActive =
            oldPlaybackMode != 0
            && (oldPlaybackPercent ? convertedBottomValue != 100 || convertedTopValue != 100 : convertedBottomValue != 0 || convertedTopValue != 0);
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            const auto key = reporting.template instanceKey<ArticulationTarget>(partId, cmper);
            reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
            reportFallbackField(reporting, key, "autoStack", Reporting::Origin::MusxOnly, 0);
            reportFallbackField(reporting, key, "centerOnStem", Reporting::Origin::MusxOnly, 0);
            reportFallbackField(reporting, key, "distanceFromStemEnd", Reporting::Origin::MusxOnly, 0);
            reportFallbackField(reporting, key, "isStemSideWhenMultipleLayers", Reporting::Origin::MusxOnly, 0);
        });
        // A padded slot has no stored row; Finale supplies its value.
        const auto stored = [&](std::size_t slot) { return source->classRecords || slot / source->fixedRowWords() < rows.size(); };
        const auto assign = [&](auto member, const char* name, auto value, std::size_t slot, std::optional<std::int64_t> raw = std::nullopt,
                                bool adjusted = false) {
            target.get()->*member = value;
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<ArticulationTarget>(partId, cmper);
                if (!stored(slot)) {
                    reportFallbackField(reporting, key, name, Reporting::Origin::LegacyBehavior, raw.value_or(word(slot)));
                    return;
                }
                reportLegacyField(reporting, key, *source, source->rowOfWord(rows, slot), name, source->byteOffsetInRow(slot * 2),
                    raw.value_or(word(slot)), adjusted ? Reporting::Origin::LegacyMusAdjusted : Reporting::Origin::LegacyMus);
            });
        };
        const auto assignFont = [&](const auto& font, const char* prefix, std::uint16_t storedId, musx::dom::Cmper resolvedId, int size,
                                    std::uint16_t effects, std::size_t idSlot, std::size_t sizeSlot, std::size_t effectsSlot, bool adjusted = false) {
            font->fontId = resolvedId;
            font->fontSize = size;
            font->setEnigmaStyles(effects);
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<ArticulationTarget>(partId, cmper);
                const auto field = [&](const char* member, std::size_t slot, std::int64_t raw) {
                    const auto name = std::string(prefix) + "." + member;
                    if (!stored(slot)) {
                        reportFallbackField(reporting, key, name.c_str(), Reporting::Origin::LegacyBehavior, raw);
                        return;
                    }
                    reportLegacyField(reporting, key, *source, source->rowOfWord(rows, slot), name.c_str(), source->byteOffsetInRow(slot * 2), raw,
                        adjusted ? Reporting::Origin::LegacyMusAdjusted : Reporting::Origin::LegacyMus);
                };
                field("absolute", effectsSlot, font->absolute);
                field("bold", effectsSlot, font->bold);
                field("fontId", idSlot, storedId);
                field("fontSize", sizeSlot, size);
                field("hidden", effectsSlot, font->hidden);
                field("italic", effectsSlot, font->italic);
                field("strikeout", effectsSlot, font->strikeout);
                field("underline", effectsSlot, font->underline);
            });
        };
        if (wide) {
            // The stored 32-bit characters use high-word-first order even in little-endian class records.
            const auto mainChar = payloadLong(payload, 0, context.profile.byteOrder, LongWordOrder::HighFirst);
            const auto altChar = payloadLong(payload, 12, context.profile.byteOrder, LongWordOrder::HighFirst);
            assign(&ArticulationTarget::charMain, "charMain", static_cast<char32_t>(mainChar), 0, mainChar);
            assign(&ArticulationTarget::charAlt, "charAlt", static_cast<char32_t>(altChar), 6, altChar);
            assignFont(target->fontMain, "fontMain", word(2), context.construction.assignFontId(word(2)), word(3), word(4), 2, 3, 4);
            assignFont(target->fontAlt, "fontAlt", word(8), context.construction.assignFontId(word(8)), word(13), word(14), 8, 13, 14);
        } else {
            const auto mainSizeFont = word(1);
            const auto altSizeFont = codaOptions ? std::uint16_t{} : word(7);
            const auto mainId = context.construction.assignFontId(mainSizeFont & 0xffU);
            const auto altId = codaOptions ? musx::dom::Cmper{} : context.construction.assignFontId(altSizeFont & 0xffU);
            assign(&ArticulationTarget::charMain, "charMain",
                text::codepointFromByte(static_cast<std::uint8_t>(word(0) & 0xffU), context.document, mainId, text::UnresolvedFontFallback::Symbol),
                0);
            if (!codaOptions) {
                assign(&ArticulationTarget::charAlt, "charAlt",
                    text::codepointFromByte(
                        static_cast<std::uint8_t>(word(6) & 0xffU), context.document, altId, text::UnresolvedFontFallback::Symbol),
                    6);
            }
            assignFont(target->fontMain, "fontMain", mainSizeFont & 0xffU, mainId, mainSizeFont >> 8, word(0) >> 8, 1, 1, 0);
            if (codaOptions) {
                assign(&ArticulationTarget::charAlt, "charAlt", target->charMain, 0, word(0), true);
                assignFont(target->fontAlt, "fontAlt", mainSizeFont & 0xffU, mainId, mainSizeFont >> 8, word(0) >> 8, 1, 1, 0, true);
            } else {
                assignFont(target->fontAlt, "fontAlt", altSizeFont & 0xffU, altId, altSizeFont >> 8, word(6) >> 8, 7, 7, 6);
            }
        }
        const auto flags = earlyPlayback ? convertedPlaybackFlags & ~0x0700U : word(5);
        {
            assign(&ArticulationTarget::copyMode, "copyMode",
                (flags & 0x0040U) == 0   ? ArticulationTarget::CopyMode::None
                : (flags & 0x0020U) != 0 ? ArticulationTarget::CopyMode::Horizontal
                                         : ArticulationTarget::CopyMode::Vertical,
                5);
            assign(&ArticulationTarget::useTopNote, "useTopNote", (flags & 0x0080U) != 0, 5);
            if (codaOptions) {
                const auto legacyBehavior = [&](auto member, const char* name, auto value) {
                    target.get()->*member = value;
                    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                        const auto key = reporting.template instanceKey<ArticulationTarget>(partId, cmper);
                        reportFallbackField(reporting, key, name, Reporting::Origin::LegacyBehavior, value);
                    });
                };
                legacyBehavior(&ArticulationTarget::xOffsetMain, "xOffsetMain", 0);
                legacyBehavior(&ArticulationTarget::yOffsetMain, "yOffsetMain", 0);
                legacyBehavior(&ArticulationTarget::xOffsetAlt, "xOffsetAlt", 0);
                legacyBehavior(&ArticulationTarget::yOffsetAlt, "yOffsetAlt", 0);
                legacyBehavior(&ArticulationTarget::defVertPos, "defVertPos", 24);
                legacyBehavior(&ArticulationTarget::mainShape, "mainShape", musx::dom::Cmper{});
                legacyBehavior(&ArticulationTarget::altShape, "altShape", musx::dom::Cmper{});
                const bool hasPlaybackValues = word(2) != 0 || word(3) != 0;
                assign(&ArticulationTarget::playArtic, "playArtic", hasPlaybackValues || (flags & 0x0c00U) != 0, 5);
                const bool alterDuration = (flags & 0x0200U) != 0;
                const bool timeBased = (flags & 0x0800U) != 0;
                const bool useFraction = (flags & 0x0100U) != 0;
                const auto numerator = word(2) >> 8;
                const auto denominator = word(2) & 0xffU;
                const bool validFraction = useFraction && denominator != 0;
                const bool invalidFraction = useFraction && denominator == 0;
                const auto fractionPercent = validFraction ? (numerator * 100U + denominator / 2U) / denominator : 0U;
                if (hasPlaybackValues && timeBased && !alterDuration && !useFraction) {
                    assign(&ArticulationTarget::startTopNoteDelta, "startTopNoteDelta", signedWord(3), 3);
                    assign(&ArticulationTarget::startBotNoteDelta, "startBotNoteDelta", signedWord(2), 2);
                } else {
                    legacyBehavior(&ArticulationTarget::startTopNoteDelta, "startTopNoteDelta", 0);
                    legacyBehavior(&ArticulationTarget::startBotNoteDelta, "startBotNoteDelta", 0);
                }
                legacyBehavior(&ArticulationTarget::startTopNotePercent, "startTopNotePercent", 0);
                legacyBehavior(&ArticulationTarget::startBotNotePercent, "startBotNotePercent", 0);
                if (hasPlaybackValues && alterDuration && (!useFraction || invalidFraction)) {
                    assign(&ArticulationTarget::durTopNoteDelta, "durTopNoteDelta", signedWord(3), 3, std::nullopt, invalidFraction);
                    assign(&ArticulationTarget::durBotNoteDelta, "durBotNoteDelta", signedWord(2), 2, std::nullopt, invalidFraction);
                } else {
                    legacyBehavior(&ArticulationTarget::durTopNoteDelta, "durTopNoteDelta", 0);
                    legacyBehavior(&ArticulationTarget::durBotNoteDelta, "durBotNoteDelta", 0);
                }
                if (hasPlaybackValues && alterDuration && validFraction) {
                    assign(&ArticulationTarget::durTopNotePercent, "durTopNotePercent", static_cast<int>(fractionPercent), 2, word(2), true);
                    assign(&ArticulationTarget::durBotNotePercent, "durBotNotePercent", static_cast<int>(fractionPercent), 2, word(2), true);
                } else if (!useFraction || !alterDuration || invalidFraction) {
                    legacyBehavior(&ArticulationTarget::durTopNotePercent, "durTopNotePercent", 0);
                    legacyBehavior(&ArticulationTarget::durBotNotePercent, "durBotNotePercent", 0);
                }
                if (hasPlaybackValues && !timeBased && !alterDuration && (!useFraction || invalidFraction)) {
                    assign(&ArticulationTarget::ampTopNoteDelta, "ampTopNoteDelta", signedWord(3), 3, std::nullopt, invalidFraction);
                    assign(&ArticulationTarget::ampBotNoteDelta, "ampBotNoteDelta", signedWord(2), 2, std::nullopt, invalidFraction);
                } else {
                    legacyBehavior(&ArticulationTarget::ampTopNoteDelta, "ampTopNoteDelta", 0);
                    legacyBehavior(&ArticulationTarget::ampBotNoteDelta, "ampBotNoteDelta", 0);
                }
                if (hasPlaybackValues && !timeBased && !alterDuration && validFraction) {
                    assign(&ArticulationTarget::ampTopNotePercent, "ampTopNotePercent", static_cast<int>(fractionPercent), 2, word(2), true);
                    assign(&ArticulationTarget::ampBotNotePercent, "ampBotNotePercent", static_cast<int>(fractionPercent), 2, word(2), true);
                } else if (!useFraction || timeBased || alterDuration || invalidFraction) {
                    legacyBehavior(&ArticulationTarget::ampTopNotePercent, "ampTopNotePercent", 0);
                    legacyBehavior(&ArticulationTarget::ampBotNotePercent, "ampBotNotePercent", 0);
                }
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                    const auto key = reporting.template instanceKey<ArticulationTarget>(partId, cmper);
                    reportFallbackField(reporting, key, "aboveSymbolAlt", Reporting::Origin::LegacyBehavior, 0);
                    reportFallbackField(reporting, key, "altIsShape", Reporting::Origin::LegacyBehavior, 0);
                    reportFallbackField(reporting, key, "autoHorz", Reporting::Origin::LegacyBehavior, 0);
                    reportFallbackField(reporting, key, "autoVert", Reporting::Origin::LegacyBehavior, 0);
                    reportFallbackField(reporting, key, "autoVertMode", Reporting::Origin::LegacyBehavior, 0);
                    reportFallbackField(reporting, key, "avoidStaffLines", Reporting::Origin::LegacyBehavior, 0);
                    reportFallbackField(reporting, key, "belowSymbolAlt", Reporting::Origin::LegacyBehavior, 0);
                    reportFallbackField(reporting, key, "insideSlur", Reporting::Origin::LegacyBehavior, 0);
                    reportFallbackField(reporting, key, "mainIsShape", Reporting::Origin::LegacyBehavior, 0);
                    reportFallbackField(reporting, key, "noPrint", Reporting::Origin::LegacyBehavior, 0);
                    reportFallbackField(reporting, key, "outsideStaff", Reporting::Origin::LegacyBehavior, 0);
                    reportFallbackField(reporting, key, "slurInteractionMode", Reporting::Origin::LegacyBehavior, 0);
                });
                context.document->getOthers()->add(ArticulationTarget::XmlNodeName, std::move(target));
                continue;
            }
            assign(&ArticulationTarget::autoHorz, "autoHorz", (flags & 0x0010U) != 0, 5);
            assign(&ArticulationTarget::autoVert, "autoVert", (flags & 0x0008U) != 0, 5);
            const auto verticalMode = flags & 0x0007U;
            if (verticalMode <= 5) {
                assign(&ArticulationTarget::autoVertMode, "autoVertMode",
                    verticalMode == 1   ? ArticulationTarget::AutoVerticalMode::StemSide
                    : verticalMode == 2 ? ArticulationTarget::AutoVerticalMode::AboveEntry
                    : verticalMode == 3 ? ArticulationTarget::AutoVerticalMode::BelowEntry
                    : verticalMode == 4 ? ArticulationTarget::AutoVerticalMode::AutoNoteStem
                    : verticalMode == 5 ? ArticulationTarget::AutoVerticalMode::AlwaysOnStem
                                        : ArticulationTarget::AutoVerticalMode::AlwaysNoteheadSide,
                    5);
            } else {
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                    const auto key = reporting.template instanceKey<ArticulationTarget>(partId, cmper);
                    reportFallbackField(reporting, key, "autoVertMode", Reporting::Origin::Unmapped, 0);
                });
            }
            assign(&ArticulationTarget::insideSlur, "insideSlur", (flags & 0x0200U) != 0, 5);
            assign(&ArticulationTarget::slurInteractionMode, "slurInteractionMode",
                (flags & 0x0200U) != 0 ? ArticulationTarget::SlurInteractionMode::InsideSlur : ArticulationTarget::SlurInteractionMode::Ignore, 5);
            assign(&ArticulationTarget::noPrint, "noPrint", (flags & 0x0100U) != 0, 5);
            assign(&ArticulationTarget::outsideStaff, "outsideStaff", (flags & 0x1000U) != 0, 5);
            assign(&ArticulationTarget::aboveSymbolAlt, "aboveSymbolAlt", (flags & 0x2000U) != 0, 5);
            assign(&ArticulationTarget::belowSymbolAlt, "belowSymbolAlt", (flags & 0x4000U) != 0, 5);
            const auto xMainSlot = wide ? 9U : 8U;
            const auto yMainSlot = wide ? 10U : 9U;
            const auto verticalSlot = wide ? 11U : 10U;
            const auto flag2Slot = wide ? 12U : 11U;
            const auto xAltSlot = wide ? 15U : 14U;
            const auto yAltSlot = wide ? 16U : 15U;
            const auto mainShapeSlot = wide ? 17U : 16U;
            const auto altShapeSlot = wide ? 18U : 17U;
            const auto playbackSlot = wide ? 19U : 18U;
            assign(&ArticulationTarget::xOffsetMain, "xOffsetMain", signedWord(xMainSlot), xMainSlot);
            assign(&ArticulationTarget::yOffsetMain, "yOffsetMain", signedWord(yMainSlot), yMainSlot);
            assign(&ArticulationTarget::defVertPos, "defVertPos", signedWord(verticalSlot), verticalSlot);
            const auto oldPlaybackPercentMask = oldPlaybackMode == 0x0600U ? 0x0010U : oldPlaybackMode == 0x0400U ? 0x0020U : 0x0008U;
            const auto flags2 =
                earlyPlayback ? (word(flag2Slot) & 0xff87U) | (oldPlaybackActive ? 0x0040U | oldPlaybackPercentMask : 0U) : word(flag2Slot);
            assign(&ArticulationTarget::avoidStaffLines, "avoidStaffLines", (flags2 & 0x0001U) != 0, flag2Slot);
            assign(&ArticulationTarget::mainIsShape, "mainIsShape", (flags2 & 0x0002U) != 0, flag2Slot);
            assign(&ArticulationTarget::altIsShape, "altIsShape", (flags2 & 0x0004U) != 0, flag2Slot);
            assign(&ArticulationTarget::playArtic, "playArtic", (flags2 & 0x0040U) != 0, earlyPlayback ? 5U : flag2Slot, std::nullopt, earlyPlayback);
            assign(&ArticulationTarget::xOffsetAlt, "xOffsetAlt", signedWord(xAltSlot), xAltSlot);
            assign(&ArticulationTarget::yOffsetAlt, "yOffsetAlt", signedWord(yAltSlot), yAltSlot);
            assign(&ArticulationTarget::mainShape, "mainShape", word(mainShapeSlot), mainShapeSlot);
            assign(&ArticulationTarget::altShape, "altShape", word(altShapeSlot), altShapeSlot);
            const auto playback = [&](auto deltaMember, auto percentMember, const char* deltaName, const char* percentName, std::size_t slot,
                                      std::uint16_t percentMask, std::uint16_t oldMode) {
                if (earlyPlayback || shortLegacy) {
                    const bool selected = earlyPlayback && oldPlaybackActive && oldPlaybackMode == oldMode;
                    const auto fallback = [&](const char* name) {
                        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                            const auto key = reporting.template instanceKey<ArticulationTarget>(partId, cmper);
                            reportFallbackField(reporting, key, name, Reporting::Origin::LegacyBehavior, 0);
                        });
                    };
                    if (selected) {
                        const auto sourceSlot = (slot - playbackSlot) % 2 == 0 ? 3U : 2U;
                        if (oldPlaybackPercent) {
                            const auto value = sourceSlot == 3U ? convertedTopValue : convertedBottomValue;
                            assign(percentMember, percentName, static_cast<std::int16_t>(value), sourceSlot, std::nullopt, true);
                            fallback(deltaName);
                        } else {
                            const auto value = sourceSlot == 3U ? convertedTopValue : convertedBottomValue;
                            assign(deltaMember, deltaName, static_cast<std::int16_t>(value), sourceSlot, std::nullopt, true);
                            fallback(percentName);
                        }
                    } else {
                        fallback(deltaName);
                        fallback(percentName);
                    }
                    return;
                }
                const bool percent = (flags2 & percentMask) != 0;
                if (percent) {
                    assign(percentMember, percentName, signedWord(slot), slot);
                } else {
                    assign(deltaMember, deltaName, signedWord(slot), slot);
                }
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                    const auto key = reporting.template instanceKey<ArticulationTarget>(partId, cmper);
                    reportFallbackField(reporting, key, percent ? deltaName : percentName, Reporting::Origin::LegacyBehavior, 0);
                });
            };
            playback(&ArticulationTarget::startTopNoteDelta, &ArticulationTarget::startTopNotePercent, "startTopNoteDelta", "startTopNotePercent",
                playbackSlot, 0x0010U, 0x0600U);
            playback(&ArticulationTarget::startBotNoteDelta, &ArticulationTarget::startBotNotePercent, "startBotNoteDelta", "startBotNotePercent",
                playbackSlot + 1, 0x0010U, 0x0600U);
            playback(&ArticulationTarget::durTopNoteDelta, &ArticulationTarget::durTopNotePercent, "durTopNoteDelta", "durTopNotePercent",
                playbackSlot + 2, 0x0020U, 0x0400U);
            playback(&ArticulationTarget::durBotNoteDelta, &ArticulationTarget::durBotNotePercent, "durBotNoteDelta", "durBotNotePercent",
                playbackSlot + 3, 0x0020U, 0x0400U);
            playback(&ArticulationTarget::ampTopNoteDelta, &ArticulationTarget::ampTopNotePercent, "ampTopNoteDelta", "ampTopNotePercent",
                playbackSlot + 4, 0x0008U, 0x0200U);
            playback(&ArticulationTarget::ampBotNoteDelta, &ArticulationTarget::ampBotNotePercent, "ampBotNoteDelta", "ampBotNotePercent",
                playbackSlot + 5, 0x0008U, 0x0200U);
        }
        context.document->getOthers()->add(ArticulationTarget::XmlNodeName, std::move(target));
    }
}

} // namespace others
} // namespace finale_mus_reader
