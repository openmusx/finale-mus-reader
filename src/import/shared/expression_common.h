// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include "import/support/expression_alignment.h"
#include "import/support/legacy_mapping.h"
#include "import/support/text_encoding.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace finale_mus_reader {

inline constexpr auto textExpressionTag = records::packTag("DT");
inline constexpr records::LegacyTag textExpressionClass = 0x00f1;
inline constexpr auto shapeExpressionTag = records::packTag("DO");
inline constexpr records::LegacyTag shapeExpressionClass = 0x00eb;

enum class LegacyExpressionNoteHorizontal : std::int16_t {
    Left = 0,
    HorizontalClickPosition = 1,
    Stem = 2,
    CenterOfPrimaryNotehead = 3,
    CenterOfAllNoteheads = 4,
    Right = 5,
    LeftOfPrimaryNotehead = 6
};

enum class LegacyExpressionNoteVertical : std::int16_t {
    VerticalClickPosition = 0,
    AboveStaffBaseline = 1,
    BelowStaffBaseline = 2,
    TopNote = 3,
    BottomNote = 4,
    AboveEntry = 5,
    BelowEntry = 6,
    AboveStaffBaselineOrEntry = 7,
    BelowStaffBaselineOrEntry = 8
};

inline std::optional<musx::dom::others::HorizontalMeasExprAlign> expressionNoteHorizontalAlignment(std::int16_t stored)
{
    using A = musx::dom::others::HorizontalMeasExprAlign;
    switch (static_cast<LegacyExpressionNoteHorizontal>(stored)) {
    case LegacyExpressionNoteHorizontal::Left:
    case LegacyExpressionNoteHorizontal::HorizontalClickPosition: return A::LeftOfPrimaryNotehead;
    case LegacyExpressionNoteHorizontal::Stem: return A::Stem;
    case LegacyExpressionNoteHorizontal::CenterOfPrimaryNotehead: return A::CenterPrimaryNotehead;
    case LegacyExpressionNoteHorizontal::CenterOfAllNoteheads: return A::CenterAllNoteheads;
    case LegacyExpressionNoteHorizontal::Right: return A::RightOfAllNoteheads;
    case LegacyExpressionNoteHorizontal::LeftOfPrimaryNotehead: return A::LeftOfPrimaryNotehead;
    }
    return std::nullopt;
}

inline std::optional<musx::dom::others::VerticalMeasExprAlign> expressionNoteVerticalAlignment(std::int16_t stored)
{
    using A = musx::dom::others::VerticalMeasExprAlign;
    switch (static_cast<LegacyExpressionNoteVertical>(stored)) {
    case LegacyExpressionNoteVertical::VerticalClickPosition: return A::Manual;
    case LegacyExpressionNoteVertical::AboveStaffBaseline: return A::AboveStaff;
    case LegacyExpressionNoteVertical::BelowStaffBaseline: return A::BelowStaff;
    case LegacyExpressionNoteVertical::TopNote: return A::TopNote;
    case LegacyExpressionNoteVertical::BottomNote: return A::BottomNote;
    case LegacyExpressionNoteVertical::AboveEntry: return A::AboveEntry;
    case LegacyExpressionNoteVertical::BelowEntry: return A::BelowEntry;
    case LegacyExpressionNoteVertical::AboveStaffBaselineOrEntry: return A::AboveStaffOrEntry;
    case LegacyExpressionNoteVertical::BelowStaffBaselineOrEntry: return A::BelowStaffOrEntry;
    }
    return std::nullopt;
}

inline std::optional<musx::dom::others::PlaybackType> expressionPlayback(std::uint16_t stored)
{
    using P = musx::dom::others::PlaybackType;
    switch (stored) {
    case 0: return P::None;
    case 1: return P::Tempo;
    case 2: return P::KeyVelocity;
    case 3: return P::Transpose;
    case 4: return P::Dump;
    case 5: return P::Channel;
    case 6: return P::RestrikeKeys;
    case 7: return P::PlayTempoToolChanges;
    case 8: return P::IgnoreTempoToolChanges;
    case 0x0e: return P::Swing;
    case 0x10: return P::SmartPlaybackOn;
    case 0x11: return P::SmartPlaybackOff;
    case 0x13: return P::PercussionMidiMap;
    case 0xb0: return P::MidiController;
    case 0xc0: return P::MidiPatchChange;
    case 0xd0: return P::ChannelPressure;
    case 0xe0: return P::MidiPitchWheel;
    default: return std::nullopt;
    }
}

inline std::optional<musx::dom::others::RehearsalMarkStyle> expressionRehearsalStyle(std::int16_t stored)
{
    using Style = musx::dom::others::RehearsalMarkStyle;
    switch (stored) {
    case 0: return Style::None;
    case 1: return Style::Letters;
    case 2: return Style::LetterNumbers;
    case 3: return Style::LettersLowerCase;
    case 4: return Style::LettersNumbersLowerCase;
    case 5: return Style::Numbers;
    case 6: return Style::MeasureNumber;
    default: return std::nullopt;
    }
}

template <typename Target, typename Assign>
void assignExpressionPlayback(Assign&& assign, const std::vector<std::int16_t>& words)
{
    const auto flags = static_cast<std::uint16_t>(words[5]);
    const bool smartMusic = (flags & 0xffU) == 0x0fU;
    assign(&Target::value, "value", (flags & 0x2000U) ? 0 : words[2], 2);
    assign(&Target::execShape, "execShape", static_cast<musx::dom::Cmper>((flags & 0x2000U) ? words[2] : 0), 2);
    // Removed SmartMusic playback has no modern playback type or auxiliary selector.
    assign(&Target::auxData1, "auxData1", smartMusic ? 0 : words[3], 3, smartMusic);
    assign(&Target::playPass, "playPass", words[4], 4);
    assign(&Target::useAuxData, "useAuxData", bool(flags & 0x1000U), 5);
    if (auto playback = smartMusic ? musx::dom::others::PlaybackType::None : expressionPlayback(flags & 0xffU)) {
        assign(&Target::playbackType, "playbackType", *playback, 5, smartMusic);
    }
}

template <typename Target>
void scheduleExpressionMiscCategory(const ImportContext& context, std::shared_ptr<Target> target)
{
    context.pending.defer(DeferredStage::ResolveStoredRefs, [&context, target] {
        using Category = musx::dom::others::MarkingCategory;
        for (const auto& category : context.document->getOthers()->getArray<Category>(musx::dom::SCORE_PARTID)) {
            if (category->categoryType != Category::CategoryType::Misc) {
                continue;
            }
            target->categoryId = category->getCmper();
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                reporting.report().setField(reporting.template instanceKey<Target>(target->getSourcePartId(), target->getCmper()), "categoryId",
                    typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, target->categoryId});
            });
            break;
        }
    });
}

/// @brief The baseline adjustment Finale's upgrade gives a definition stored before Finale 2004.
/// @details The upgrade writes the expression's category, 7, into the word that later upgrades
/// copy into the baseline adjustment.
inline constexpr musx::dom::Evpu earlyExpressionBaselineAdjustment = 7;

/// @brief Applies the measure positioning Finale's upgrade gives a definition stored before Finale 2004.
/// @details Such a definition stores no positioning. The upgrade gives it manual alignment both
/// ways, left justification, no horizontal or entry adjustment, and
/// @ref earlyExpressionBaselineAdjustment.
template <typename Target>
void applyEarlyExpressionPositioning(ImportReport& report, const ReportInstance& instance, Target& target)
{
    target.horzMeasExprAlign = musx::dom::others::HorizontalMeasExprAlign::Manual;
    target.vertMeasExprAlign = musx::dom::others::VerticalMeasExprAlign::Manual;
    target.horzExprJustification = musx::dom::AlignJustify::Left;
    target.measXAdjust = 0;
    target.yAdjustBaseline = earlyExpressionBaselineAdjustment;
    target.yAdjustEntry = 0;
    withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.instanceKey(instance);
        for (const auto& [member, value] : {std::pair{"horzMeasExprAlign", static_cast<std::int64_t>(target.horzMeasExprAlign)},
                 {"vertMeasExprAlign", static_cast<std::int64_t>(target.vertMeasExprAlign)},
                 {"horzExprJustification", static_cast<std::int64_t>(target.horzExprJustification)}, {"measXAdjust", std::int64_t(0)},
                 {"yAdjustBaseline", std::int64_t(earlyExpressionBaselineAdjustment)}, {"yAdjustEntry", std::int64_t(0)}}) {
            reporting.report().setField(key, member, typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, value});
        }
    });
}

template <typename Target, typename Assign>
void assignExpressionPositioning(Target&, Assign&& assign, const std::vector<std::int16_t>& words, bool hasCategory)
{
    const std::size_t horizontalSlot = 6;
    if (auto align = expressionHorizontalAlignment(words[horizontalSlot])) {
        assign(&Target::horzMeasExprAlign, "horzMeasExprAlign", *align, horizontalSlot);
    }
    if (auto justify = expressionJustification(words[7])) {
        assign(&Target::horzExprJustification, "horzExprJustification", *justify, 7);
    }
    assign(&Target::measXAdjust, "measXAdjust", words[8], 8);
    const std::size_t verticalSlot = 12;
    if (auto align = expressionVerticalAlignment(words[verticalSlot])) {
        assign(&Target::vertMeasExprAlign, "vertMeasExprAlign", *align, verticalSlot);
    }
    const std::size_t baselineSlot = hasCategory ? 15 : 13;
    assign(&Target::yAdjustBaseline, "yAdjustBaseline", words[baselineSlot], baselineSlot);
    assign(&Target::yAdjustEntry, "yAdjustEntry", words[16], 16);
    if (hasCategory) {
        const auto category = static_cast<std::uint16_t>(words[13]);
        const auto categoryId = static_cast<musx::dom::Cmper>(category & 0x3fffU);
        // Category zero denotes the miscellaneous category.
        assign(&Target::categoryId, "categoryId", categoryId ? categoryId : musx::dom::Cmper(7), 13, categoryId == 0);
        assign(&Target::useCategoryFonts, "useCategoryFonts", bool(category & 0x8000U), 13);
        assign(&Target::useCategoryPos, "useCategoryPos", bool(category & 0x4000U), 13);
    }
}

template <typename Target, typename Source>
void recoverExpressionDescription(const ImportContext& context, const Source& source, std::span<const records::LegacyRow> rows,
    std::span<const std::uint8_t> payload, const std::shared_ptr<Target>& target, const ReportInstance& reportInstance, std::size_t headerSize)
{
    const auto trailer = payload.subspan(headerSize);
    target->description = versions::storesUnicodeCodepoints(context.profile.version)
                              ? text::utf16ToUtf8(payloadWords(trailer, context.profile.byteOrder))
                              : text::toUtf8(payloadString(trailer, 0, trailer.size()), context.profile.platform);
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto& descriptionRow = rows[source.classRecords || trailer.empty() ? 0 : headerSize / (2 * records::otherWordCount)];
        reporting.report().setField(reporting.instanceKey(reportInstance), "description",
            typename Reporting::FieldInfo{Reporting::Origin::LegacyMus, descriptionRow.blockOffset,
                descriptionRow.decodedOffset + (source.classRecords ? headerSize : 0), static_cast<std::int64_t>(trailer.size()), source.identity});
    });
}

} // namespace finale_mus_reader
