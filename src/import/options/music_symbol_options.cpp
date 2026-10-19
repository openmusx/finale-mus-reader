// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "import/support/field_manifest.h"
#include "import/support/text_encoding.h"

namespace finale_mus_reader {
namespace options {
namespace {

constexpr std::uint16_t musicSymbolStraightFlagSelector = 75;
constexpr records::LegacyTag unicodeMusicSymbolsClass = numericGlobalClass(musicSymbolStraightFlagSelector);
constexpr std::size_t unicodeCodepointSize = 4;
constexpr std::size_t unicodeMusicSymbolsOffset = 12;
constexpr std::size_t unicodeMusicSymbolsTrailerSize = 4;

constexpr MusicSymbolOptionsField musicSymbolField(std::string_view memberName, std::string_view leafName,
    char32_t MusicSymbolOptionsTarget::* member, MusicSymbolFontType fontType, NarrowMusicSymbolSource narrowSource,
    NarrowMusicSymbolEra narrowEra = NarrowMusicSymbolEra::Any, std::optional<char32_t MusicSymbolOptionsTarget::*> sharedSource = std::nullopt,
    SharedMusicSymbolEra sharedEra = SharedMusicSymbolEra::None)
{
    return {memberName, leafName, member, fontType, sharedSource, sharedEra, narrowSource, narrowEra};
}

// This order is both musxdom's persisted field order and the Finale 2012 array order.
const std::array musicSymbolOptionFieldTable{
    musicSymbolField("noteheadQuarter", "notehead_quarter", &MusicSymbolOptionsTarget::noteheadQuarter, MusicSymbolFontType::Noteheads,
        NarrowMusicSymbolSource{6, 1}),
    musicSymbolField(
        "noteheadHalf", "notehead_half", &MusicSymbolOptionsTarget::noteheadHalf, MusicSymbolFontType::Noteheads, NarrowMusicSymbolSource{6, 2}),
    musicSymbolField(
        "noteheadWhole", "notehead_whole", &MusicSymbolOptionsTarget::noteheadWhole, MusicSymbolFontType::Noteheads, NarrowMusicSymbolSource{6, 3}),
    musicSymbolField("noteheadDblWhole", "notehead_dbl_whole", &MusicSymbolOptionsTarget::noteheadDblWhole, MusicSymbolFontType::Noteheads,
        NarrowMusicSymbolSource{6, 4}),
    musicSymbolField("natural", "natural", &MusicSymbolOptionsTarget::natural, MusicSymbolFontType::Accis, NarrowMusicSymbolSource{6, 5}),
    musicSymbolField("flat", "flat", &MusicSymbolOptionsTarget::flat, MusicSymbolFontType::Accis, NarrowMusicSymbolSource{7, 0}),
    musicSymbolField("sharp", "sharp", &MusicSymbolOptionsTarget::sharp, MusicSymbolFontType::Accis, NarrowMusicSymbolSource{7, 1}),
    musicSymbolField("dblFlat", "dbl_flat", &MusicSymbolOptionsTarget::dblFlat, MusicSymbolFontType::Accis, NarrowMusicSymbolSource{7, 3}),
    musicSymbolField("dblSharp", "dbl_sharp", &MusicSymbolOptionsTarget::dblSharp, MusicSymbolFontType::Accis, NarrowMusicSymbolSource{7, 2}),
    musicSymbolField(
        "parenNatural", "paren_natural", &MusicSymbolOptionsTarget::parenNatural, MusicSymbolFontType::Accis, NarrowMusicSymbolSource{38, 0}),
    musicSymbolField("parenFlat", "paren_flat", &MusicSymbolOptionsTarget::parenFlat, MusicSymbolFontType::Accis, NarrowMusicSymbolSource{38, 1}),
    musicSymbolField("parenSharp", "paren_sharp", &MusicSymbolOptionsTarget::parenSharp, MusicSymbolFontType::Accis, NarrowMusicSymbolSource{38, 2}),
    musicSymbolField(
        "parenDblFlat", "paren_dbl_flat", &MusicSymbolOptionsTarget::parenDblFlat, MusicSymbolFontType::Accis, NarrowMusicSymbolSource{38, 4}),
    musicSymbolField(
        "parenDblSharp", "paren_dbl_sharp", &MusicSymbolOptionsTarget::parenDblSharp, MusicSymbolFontType::Accis, NarrowMusicSymbolSource{38, 3}),
    musicSymbolField(
        "chordNatural", "chord_natural", &MusicSymbolOptionsTarget::chordNatural, MusicSymbolFontType::ChordAcci, NarrowMusicSymbolSource{11, 0}),
    musicSymbolField("chordFlat", "chord_flat", &MusicSymbolOptionsTarget::chordFlat, MusicSymbolFontType::ChordAcci, NarrowMusicSymbolSource{11, 1}),
    musicSymbolField(
        "chordSharp", "chord_sharp", &MusicSymbolOptionsTarget::chordSharp, MusicSymbolFontType::ChordAcci, NarrowMusicSymbolSource{11, 2}),
    musicSymbolField(
        "chordDblFlat", "chord_dbl_flat", &MusicSymbolOptionsTarget::chordDblFlat, MusicSymbolFontType::ChordAcci, NarrowMusicSymbolSource{11, 3}),
    musicSymbolField(
        "chordDblSharp", "chord_dbl_sharp", &MusicSymbolOptionsTarget::chordDblSharp, MusicSymbolFontType::ChordAcci, NarrowMusicSymbolSource{11, 4}),
    musicSymbolField("keySigNatural", "key_sig_natural", &MusicSymbolOptionsTarget::keySigNatural, MusicSymbolFontType::Key,
        NarrowMusicSymbolSource{46, 0}, NarrowMusicSymbolEra::AfterCoda, &MusicSymbolOptionsTarget::natural, SharedMusicSymbolEra::CodaOnly),
    musicSymbolField("keySigFlat", "key_sig_flat", &MusicSymbolOptionsTarget::keySigFlat, MusicSymbolFontType::Key, NarrowMusicSymbolSource{46, 1},
        NarrowMusicSymbolEra::AfterCoda, &MusicSymbolOptionsTarget::flat, SharedMusicSymbolEra::CodaOnly),
    musicSymbolField("keySigSharp", "key_sig_sharp", &MusicSymbolOptionsTarget::keySigSharp, MusicSymbolFontType::Key, NarrowMusicSymbolSource{46, 2},
        NarrowMusicSymbolEra::AfterCoda, &MusicSymbolOptionsTarget::sharp, SharedMusicSymbolEra::CodaOnly),
    musicSymbolField("keySigDblFlat", "key_sig_dbl_flat", &MusicSymbolOptionsTarget::keySigDblFlat, MusicSymbolFontType::Key,
        NarrowMusicSymbolSource{46, 3}, NarrowMusicSymbolEra::AfterCoda, &MusicSymbolOptionsTarget::dblFlat, SharedMusicSymbolEra::CodaOnly),
    musicSymbolField("keySigDblSharp", "key_sig_dbl_sharp", &MusicSymbolOptionsTarget::keySigDblSharp, MusicSymbolFontType::Key,
        NarrowMusicSymbolSource{46, 4}, NarrowMusicSymbolEra::AfterCoda, &MusicSymbolOptionsTarget::dblSharp, SharedMusicSymbolEra::CodaOnly),
    musicSymbolField("restLonga", "rest_longa", &MusicSymbolOptionsTarget::restLonga, MusicSymbolFontType::Rests, NarrowMusicSymbolSource{8, 1}),
    musicSymbolField(
        "restDblWhole", "rest_dbl_whole", &MusicSymbolOptionsTarget::restDblWhole, MusicSymbolFontType::Rests, NarrowMusicSymbolSource{8, 2}),
    musicSymbolField("restWhole", "rest_whole", &MusicSymbolOptionsTarget::restWhole, MusicSymbolFontType::Rests, NarrowMusicSymbolSource{8, 3}),
    musicSymbolField("restHalf", "rest_half", &MusicSymbolOptionsTarget::restHalf, MusicSymbolFontType::Rests, NarrowMusicSymbolSource{8, 4}),
    musicSymbolField(
        "restQuarter", "rest_quarter", &MusicSymbolOptionsTarget::restQuarter, MusicSymbolFontType::Rests, NarrowMusicSymbolSource{8, 5}),
    musicSymbolField("restEighth", "rest_eighth", &MusicSymbolOptionsTarget::restEighth, MusicSymbolFontType::Rests, NarrowMusicSymbolSource{9, 0}),
    musicSymbolField("rest16th", "rest16th", &MusicSymbolOptionsTarget::rest16th, MusicSymbolFontType::Rests, NarrowMusicSymbolSource{9, 1}),
    musicSymbolField("rest32nd", "rest32nd", &MusicSymbolOptionsTarget::rest32nd, MusicSymbolFontType::Rests, NarrowMusicSymbolSource{9, 2}),
    musicSymbolField("rest64th", "rest64th", &MusicSymbolOptionsTarget::rest64th, MusicSymbolFontType::Rests, NarrowMusicSymbolSource{9, 3}),
    musicSymbolField("rest128th", "rest128th", &MusicSymbolOptionsTarget::rest128th, MusicSymbolFontType::Rests, NarrowMusicSymbolSource{10, 3},
        NarrowMusicSymbolEra::Finale97AndLater),
    musicSymbolField("restDefMeas", "rest_def_meas", &MusicSymbolOptionsTarget::restDefMeas, MusicSymbolFontType::Rests,
        NarrowMusicSymbolSource{9, 4}, NarrowMusicSymbolEra::Finale97AndLater),
    musicSymbolField("oneBarRepeat", "one_bar_repeat", &MusicSymbolOptionsTarget::oneBarRepeat, MusicSymbolFontType::AltNotSlash,
        NarrowMusicSymbolSource{43, 0}, NarrowMusicSymbolEra::Finale35AndLater),
    musicSymbolField("twoBarRepeat", "two_bar_repeat", &MusicSymbolOptionsTarget::twoBarRepeat, MusicSymbolFontType::AltNotSlash,
        NarrowMusicSymbolSource{43, 1}, NarrowMusicSymbolEra::Finale35AndLater),
    musicSymbolField("slashBar", "slash_bar", &MusicSymbolOptionsTarget::slashBar, MusicSymbolFontType::AltNotSlash, NarrowMusicSymbolSource{42, 0},
        NarrowMusicSymbolEra::Finale35AndLater),
    musicSymbolField("quarterSlash", "quarter_slash", &MusicSymbolOptionsTarget::quarterSlash, MusicSymbolFontType::AltNotSlash,
        NarrowMusicSymbolSource{42, 1}, NarrowMusicSymbolEra::Finale35AndLater),
    musicSymbolField(
        "halfSlash", "half_slash", &MusicSymbolOptionsTarget::halfSlash, MusicSymbolFontType::AltNotSlash, NarrowMusicSymbolSource{42, 2}),
    musicSymbolField(
        "wholeSlash", "whole_slash", &MusicSymbolOptionsTarget::wholeSlash, MusicSymbolFontType::AltNotSlash, NarrowMusicSymbolSource{42, 3}),
    musicSymbolField("dblWholeSlash", "dbl_whole_slash", &MusicSymbolOptionsTarget::dblWholeSlash, MusicSymbolFontType::AltNotSlash,
        NarrowMusicSymbolSource{42, 4}, NarrowMusicSymbolEra::Finale35AndLater),
    musicSymbolField(
        "timeSigPlus", "time_sig_plus", &MusicSymbolOptionsTarget::timeSigPlus, MusicSymbolFontType::TimePlus, NarrowMusicSymbolSource{42, 5}),
    musicSymbolField("timeSigPlusParts", "time_sig_plus_parts", &MusicSymbolOptionsTarget::timeSigPlusParts, MusicSymbolFontType::TimePlusParts,
        NarrowMusicSymbolSource{18, 11}, NarrowMusicSymbolEra::ZlibOnly, &MusicSymbolOptionsTarget::timeSigPlus, SharedMusicSymbolEra::PreZlib),
    musicSymbolField("timeSigAbrvCommon", "time_sig_abrv_common", &MusicSymbolOptionsTarget::timeSigAbrvCommon, MusicSymbolFontType::Time,
        NarrowMusicSymbolSource{19, 4}),
    musicSymbolField(
        "timeSigAbrvCut", "time_sig_abrv_cut", &MusicSymbolOptionsTarget::timeSigAbrvCut, MusicSymbolFontType::Time, NarrowMusicSymbolSource{19, 5}),
    musicSymbolField("timeSigAbrvCommonParts", "time_sig_abrv_common_parts", &MusicSymbolOptionsTarget::timeSigAbrvCommonParts,
        MusicSymbolFontType::TimeParts, NarrowMusicSymbolSource{18, 12}, NarrowMusicSymbolEra::ZlibOnly, &MusicSymbolOptionsTarget::timeSigAbrvCommon,
        SharedMusicSymbolEra::PreZlib),
    musicSymbolField("timeSigAbrvCutParts", "time_sig_abrv_cut_parts", &MusicSymbolOptionsTarget::timeSigAbrvCutParts, MusicSymbolFontType::TimeParts,
        NarrowMusicSymbolSource{18, 13}, NarrowMusicSymbolEra::ZlibOnly, &MusicSymbolOptionsTarget::timeSigAbrvCut, SharedMusicSymbolEra::PreZlib),
    musicSymbolField("augDot", "aug_dot", &MusicSymbolOptionsTarget::augDot, MusicSymbolFontType::AugDots, NarrowMusicSymbolSource{7, 4}),
    musicSymbolField("forwardRepeatDot", "forward_repeat_dot", &MusicSymbolOptionsTarget::forwardRepeatDot, MusicSymbolFontType::ReptDots,
        NarrowMusicSymbolSource{23, 2}, NarrowMusicSymbolEra::Finale35AndLater),
    musicSymbolField("backRepeatDot", "back_repeat_dot", &MusicSymbolOptionsTarget::backRepeatDot, MusicSymbolFontType::ReptDots,
        NarrowMusicSymbolSource{23, 3}, NarrowMusicSymbolEra::Finale35AndLater),
    musicSymbolField("eightVaUp", "eight_va_up", &MusicSymbolOptionsTarget::eightVaUp, MusicSymbolFontType::SmartShape8va,
        NarrowMusicSymbolSource{12, 5}, NarrowMusicSymbolEra::Finale35AndLater),
    musicSymbolField("eightVbDown", "eight_vb_down", &MusicSymbolOptionsTarget::eightVbDown, MusicSymbolFontType::SmartShape8vb,
        NarrowMusicSymbolSource{11, 5}, NarrowMusicSymbolEra::Finale35AndLater),
    musicSymbolField("fifteenMaUp", "fifteen_ma_up", &MusicSymbolOptionsTarget::fifteenMaUp, MusicSymbolFontType::SmartShape15ma,
        NarrowMusicSymbolSource{69, 4}, NarrowMusicSymbolEra::Finale351AndLater),
    musicSymbolField("fifteenMbDown", "fifteen_mb_down", &MusicSymbolOptionsTarget::fifteenMbDown, MusicSymbolFontType::SmartShape15mb,
        NarrowMusicSymbolSource{69, 5}, NarrowMusicSymbolEra::Finale351AndLater),
    musicSymbolField("trillChar", "trill_char", &MusicSymbolOptionsTarget::trillChar, MusicSymbolFontType::SmartShapeTrill,
        NarrowMusicSymbolSource{69, 2}, NarrowMusicSymbolEra::Finale351AndLater),
    musicSymbolField("wiggleChar", "wiggle_char", &MusicSymbolOptionsTarget::wiggleChar, MusicSymbolFontType::SmartShapeWiggle,
        NarrowMusicSymbolSource{69, 3}, NarrowMusicSymbolEra::Finale351AndLater),
    musicSymbolField("flagUp", "flag_up", &MusicSymbolOptionsTarget::flagUp, MusicSymbolFontType::Flags, NarrowMusicSymbolSource{7, 5}),
    musicSymbolField("flagDown", "flag_down", &MusicSymbolOptionsTarget::flagDown, MusicSymbolFontType::Flags, NarrowMusicSymbolSource{8, 0}),
    musicSymbolField("flag16Up", "flag16_up", &MusicSymbolOptionsTarget::flag16Up, MusicSymbolFontType::Flags, NarrowMusicSymbolSource{5, 0},
        NarrowMusicSymbolEra::Finale351AndLater),
    musicSymbolField("flag16Down", "flag16_down", &MusicSymbolOptionsTarget::flag16Down, MusicSymbolFontType::Flags, NarrowMusicSymbolSource{5, 1},
        NarrowMusicSymbolEra::Finale351AndLater),
    musicSymbolField("flag2Up", "flag2_up", &MusicSymbolOptionsTarget::flag2Up, MusicSymbolFontType::Flags, NarrowMusicSymbolSource{10, 1}),
    musicSymbolField("flag2Down", "flag2_down", &MusicSymbolOptionsTarget::flag2Down, MusicSymbolFontType::Flags, NarrowMusicSymbolSource{10, 2}),
    musicSymbolField("flagStraightUp", "flag_straight_up", &MusicSymbolOptionsTarget::flagStraightUp, MusicSymbolFontType::Flags,
        NarrowMusicSymbolSource{musicSymbolStraightFlagSelector, 0}, NarrowMusicSymbolEra::Finale35AndLater),
    musicSymbolField("flagStraightDown", "flag_straight_down", &MusicSymbolOptionsTarget::flagStraightDown, MusicSymbolFontType::Flags,
        NarrowMusicSymbolSource{musicSymbolStraightFlagSelector, 1}, NarrowMusicSymbolEra::Finale35AndLater),
};

struct NarrowSelectorValues
{
    std::vector<std::int16_t> words;
    std::size_t blockOffset{};
    std::size_t decodedOffset{};
    records::LegacyTag identity{};
};

NarrowSelectorValues readNarrowSelector(const ImportContext& context, std::uint16_t selector)
{
    const auto source = readGlobalWords(context.index, context.profile, selector);
    if (!source.present) {
        return {};
    }
    return {source.words, source.blockOffset, source.decodedOffset,
        sourceMatches(context.profile, EpochMask::Zlib) ? numericGlobalClass(selector) : numericGlobalTag(selector)};
}

bool narrowSourceApplies(NarrowMusicSymbolEra era, const SourceProfile& profile)
{
    switch (era) {
    case NarrowMusicSymbolEra::Any: return true;
    case NarrowMusicSymbolEra::AfterCoda: return !sourceMatches(profile, EpochMask::CodaBanner);
    case NarrowMusicSymbolEra::Finale35AndLater: return sourceAtOrAfter(profile, FormatEpoch::UncompressedLegacy, versions::finale3_5);
    case NarrowMusicSymbolEra::Finale351AndLater: return sourceAtOrAfter(profile, FormatEpoch::UncompressedLegacy, versions::finale3_5_1);
    case NarrowMusicSymbolEra::Finale97AndLater: return sourceAtOrAfter(profile, FormatEpoch::UncompressedLegacy, versions::finale97);
    case NarrowMusicSymbolEra::ZlibOnly: return sourceMatches(profile, EpochMask::Zlib);
    }
    return false;
}

char32_t decodeNarrowMusicSymbol(const ImportContext& context, const MusicSymbolOptionsField& field, std::int16_t stored)
{
    musx::dom::Cmper fontId = musx::dom::DEFAULT_MUSIC_FONT_ID;
    if (const auto font = musx::dom::options::FontOptions::getFontInfoOrNull(context.document, field.fontType)) {
        fontId = font->fontId;
    }
    return text::codepointFromByte(static_cast<std::uint8_t>(stored), context.document, fontId, text::UnresolvedFontFallback::Symbol);
}

void adjustMusicSymbolDefaultMeasureRest(const ImportContext& context, MusicSymbolOptionsTarget& target, bool recovered)
{
    if (!recovered || target.restDefMeas != 0) {
        return;
    }
    // Zero does not identify a rest glyph. Finale renders the document's configured
    // whole-rest glyph instead.
    target.restDefMeas = target.restWhole;
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        if (auto* info = reporting.report().findField(reporting.template instanceKey<MusicSymbolOptionsTarget>(), "restDefMeas");
            info && info->origin == Reporting::Origin::LegacyMus) {
            info->origin = Reporting::Origin::LegacyMusAdjusted;
        }
    });
}

} // namespace

std::span<const MusicSymbolOptionsField> musicSymbolOptionsFields()
{
    return musicSymbolOptionFieldTable;
}

void importMusicSymbolOptions(const ImportContext& context)
{
    const auto pooled = context.document->getOptions()->get<MusicSymbolOptionsTarget>();
    if (!pooled) {
        return;
    }
    const auto target = std::const_pointer_cast<MusicSymbolOptionsTarget>(pooled);
    std::map<std::uint16_t, NarrowSelectorValues> selectors;
    const auto& fields = musicSymbolOptionFieldTable;
    bool recoveredDefaultMeasureRest = false;
    for (const auto& field : fields) {
        if (!narrowSourceApplies(field.narrowEra, context.profile)) {
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                reporting.template defaultField<MusicSymbolOptionsTarget>(
                    std::string(field.memberName), static_cast<std::int64_t>(target.get()->*field.member));
            });
            continue;
        }
        const auto selector = field.narrowSource.selector;
        const auto [found, inserted] = selectors.try_emplace(selector);
        if (inserted) {
            found->second = readNarrowSelector(context, selector);
        }

        const auto& source = found->second;
        if (field.narrowSource.word < source.words.size()) {
            const auto stored = source.words[field.narrowSource.word];
            target.get()->*field.member = decodeNarrowMusicSymbol(context, field, stored);
            if (field.member == &MusicSymbolOptionsTarget::restDefMeas) {
                recoveredDefaultMeasureRest = true;
            }
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                reporting.report().setField(reporting.template instanceKey<MusicSymbolOptionsTarget>(), std::string(field.memberName),
                    {Reporting::Origin::LegacyMus, source.blockOffset, source.decodedOffset, stored, source.identity});
            });
        } else {
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                reporting.template defaultField<MusicSymbolOptionsTarget>(
                    std::string(field.memberName), static_cast<std::int64_t>(target.get()->*field.member));
            });
        }
    }

    for (const auto& field : fields) {
        if (!field.sharedSource) {
            continue;
        }
        const auto applies = field.sharedEra == SharedMusicSymbolEra::CodaOnly
                                 ? sourceMatches(context.profile, EpochMask::CodaBanner)
                                 : field.sharedEra == SharedMusicSymbolEra::PreZlib && !sourceMatches(context.profile, EpochMask::Zlib);
        if (!applies) {
            continue;
        }
        // Shared time symbols keep the score setting's decoding. Coda key-signature
        // characters share the accidental byte but render it through the key font.
        target.get()->*field.member = target.get()->*(*field.sharedSource);
        std::int64_t rawValue = static_cast<std::int64_t>(target.get()->*field.member);
        if (field.sharedEra == SharedMusicSymbolEra::CodaOnly) {
            const auto sourceField = std::ranges::find(fields, *field.sharedSource, &MusicSymbolOptionsField::member);
            if (sourceField != fields.end()) {
                const auto found = selectors.find(sourceField->narrowSource.selector);
                if (found != selectors.end() && sourceField->narrowSource.word < found->second.words.size()) {
                    const auto stored = found->second.words[sourceField->narrowSource.word];
                    target.get()->*field.member = decodeNarrowMusicSymbol(context, field, stored);
                    rawValue = stored;
                }
            }
        }
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            reporting.template behaviorField<MusicSymbolOptionsTarget>(std::string(field.memberName), rawValue);
        });
    }

    if (sourceMatches(context.profile, EpochMask::Zlib)) {
        const auto rows = context.index.getClassOthers().getArray(unicodeMusicSymbolsClass, GLOBALS_CMPER);
        if (rows.size() == 1) {
            const auto payload = context.index.getClassOthers().effectivePayloadOf(rows.front());
            const auto expectedSize = unicodeMusicSymbolsOffset + fields.size() * unicodeCodepointSize + unicodeMusicSymbolsTrailerSize;
            // Class 0x0059 is selector 75's six-word row before the Unicode extension. The
            // exact prefix-plus-65-longs-plus-trailer payload identifies the extended layout
            // directly, even when a source version cannot be recovered.
            if (payload.size() == expectedSize) {
                const auto words = payloadWords(payload, context.profile.byteOrder);
                for (std::size_t index = 0; index < fields.size(); ++index) {
                    const auto& field = fields[index];
                    const auto wordIndex = unicodeMusicSymbolsOffset / 2 + index * 2;
                    const auto value = wideCodepoint(words[wordIndex], words[wordIndex + 1]);
                    target.get()->*field.member = static_cast<char32_t>(value);
                    if (field.member == &MusicSymbolOptionsTarget::restDefMeas) {
                        recoveredDefaultMeasureRest = true;
                    }
                    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                        reporting.report().setField(reporting.template instanceKey<MusicSymbolOptionsTarget>(), std::string(field.memberName),
                            {Reporting::Origin::LegacyMus, rows.front().blockOffset,
                                rows.front().decodedOffset + unicodeMusicSymbolsOffset + index * unicodeCodepointSize,
                                static_cast<std::int64_t>(value), unicodeMusicSymbolsClass});
                    });
                }
            }
        }
    }

    adjustMusicSymbolDefaultMeasureRest(context, *target, recoveredDefaultMeasureRest);
}

} // namespace options
} // namespace finale_mus_reader
