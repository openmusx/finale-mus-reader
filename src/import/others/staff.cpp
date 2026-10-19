// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "import/shared/staff_defaults.h"
#include "import/support/enigma_text.h"
#include "import/support/field_manifest.h"
#include "import/support/legacy_font.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace staff_internal {

// Named rather than anonymous: these types are captured by the reporting lambdas of
// importStaffFamily<Target>, which have external linkage, and under the unity build GCC rejects
// an anonymous-namespace member there (-Wsubobject-linkage). The names stay distinctive for the
// same reason every file-local name does.

enum class StaffRepeatDotFallback {
    None,
    LegacyGeometry,
    Finale27Default,
};

struct StaffFallbackSelection
{
    bool lineSpace{};
    bool restOffsets{};
    bool stemReversal{};
    bool hideMode{};
    bool hideStaffLines{};
    bool fretInstId{};
    bool sixWordStaffFields{};
    bool extendedFlagFields{};
    bool stemOffsetFields{};
    std::size_t wordCount{};
    StaffRepeatDotFallback repeatDotOffsets{};
};

struct StaffLegacySemantics
{
    bool hasAlternateNotationProperties{};
    bool usesAggregateAlternateNotationItems{};
    bool synthesizesAltHideSmartShapes{};
    bool usesBooleanHideMode{};
    bool hideNoteAttachedItems{};
    bool noteShapeNotation{};
    std::optional<std::uint8_t> singleStringTabPitch{};
    bool legacySingleStringTabForm{};
};

} // namespace staff_internal

namespace {

using StaffTarget = musx::dom::others::Staff;
using StaffStyleTarget = musx::dom::others::StaffStyle;
using FretInstrumentTarget = musx::dom::others::FretInstrument;

using staff_internal::StaffFallbackSelection;
using staff_internal::StaffLegacySemantics;
using staff_internal::StaffRepeatDotFallback;

struct StaffBooleanLegacyDefault
{
    const char* name;
    bool StaffTarget::* member;
    bool value;
};

struct StaffBooleanField
{
    const char* name;
    bool StaffTarget::* member;
};

constexpr StaffBooleanLegacyDefault staffAlternateNotationBooleanDefaults[] = {
    {"altHideArtics", &StaffTarget::altHideArtics, false},
    {"altHideLyrics", &StaffTarget::altHideLyrics, false},
    {"altHideSmartShapes", &StaffTarget::altHideSmartShapes, false},
    {"altRhythmStemsUp", &StaffTarget::altRhythmStemsUp, false},
    {"altSlashDots", &StaffTarget::altSlashDots, true},
    {"altHideOtherNotes", &StaffTarget::altHideOtherNotes, false},
    {"altHideOtherArtics", &StaffTarget::altHideOtherArtics, false},
    {"altHideExpressions", &StaffTarget::altHideExpressions, false},
    {"altHideOtherLyrics", &StaffTarget::altHideOtherLyrics, false},
    {"altHideOtherSmartShapes", &StaffTarget::altHideOtherSmartShapes, false},
    {"altHideOtherExpressions", &StaffTarget::altHideOtherExpressions, false},
};

constexpr StaffBooleanLegacyDefault staffLegacyTabBooleanDefaults[] = {
    {"showTabClefAllSys", &StaffTarget::showTabClefAllSys, true},
    {"hideRests", &StaffTarget::hideRests, true},
    {"hideDots", &StaffTarget::hideDots, true},
    {"hideStems", &StaffTarget::hideStems, true},
    {"hideTuplets", &StaffTarget::hideTuplets, true},
};

constexpr StaffBooleanField staffSixWordFinale27DefaultFields[] = {
    {"showNameInParts", &StaffTarget::showNameInParts},
    {"showNoteColors", &StaffTarget::showNoteColors},
    {"hideRepeatBottomDot", &StaffTarget::hideRepeatBottomDot},
    {"hideRepeatTopDot", &StaffTarget::hideRepeatTopDot},
    {"flatBeams", &StaffTarget::flatBeams},
    {"hideFretboards", &StaffTarget::hideFretboards},
    {"hideLyrics", &StaffTarget::hideLyrics},
    {"noOptimize", &StaffTarget::noOptimize},
    {"hideBarlines", &StaffTarget::hideBarlines},
    {"hideRptBars", &StaffTarget::hideRptBars},
    {"hideChords", &StaffTarget::hideChords},
};

constexpr StaffBooleanField staffNoExtendedFlagFinale27DefaultFields[] = {
    {"hideTies", &StaffTarget::hideTies},
    {"stemStartFromStaff", &StaffTarget::stemStartFromStaff},
    {"stemsFixedEnd", &StaffTarget::stemsFixedEnd},
    {"useTabLetters", &StaffTarget::useTabLetters},
    {"hideBeams", &StaffTarget::hideBeams},
    {"stemsFixedStart", &StaffTarget::stemsFixedStart},
};

struct StaffEfixField
{
    const char* name;
    musx::dom::Efix StaffTarget::* member;
    std::size_t slot;
};

constexpr std::int16_t codaCenteredOneLineStaff = 1;
constexpr std::int16_t codaLegacyTabOneLineStaff = -1;
constexpr int evpusPerSpace = static_cast<int>(musx::dom::EVPU_PER_SPACE);

template <std::size_t N>
void applyStaffBooleanLegacyBehavior(StaffTarget& target, const StaffBooleanLegacyDefault (&fields)[N])
{
    for (const auto& field : fields) {
        target.*field.member = field.value;
    }
}

void applyStaffAlternateNotationLegacyBehavior(StaffTarget& target)
{
    target.altNotation = StaffTarget::AlternateNotation::Normal;
    target.altLayer = 0;
    applyStaffBooleanLegacyBehavior(target, staffAlternateNotationBooleanDefaults);
}

[[nodiscard]] std::string legacySingleStringFretName(std::uint8_t pitch)
{
    constexpr std::string_view names[]{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return std::string(names[pitch % 12]) + std::to_string(static_cast<int>(pitch) / 12 - 1);
}

constexpr records::LegacyTag staffTag = records::packTag("IS");
constexpr records::LegacyTag staffClass = 0x00e7;
constexpr records::LegacyTag staffStyleTag = records::packTag("SY");
constexpr records::LegacyTag staffStyleClass = 0x00e8;
constexpr records::LegacyTag codaStaffAttributesTag = records::packTag("IA");
constexpr records::LegacyTag legacyStaffFullNameTag = records::packTag("IN");
constexpr records::LegacyTag legacyStaffAbbreviatedNameTag = records::packTag("in");

constexpr std::size_t staffBaseWords = 18;
constexpr std::size_t staffBaseBytes = staffBaseWords * 2;
constexpr std::size_t staffCodaWords = 6;

constexpr std::size_t codaDefaultClefSlot = 0;
constexpr std::size_t codaVerticalTabOffsetSlot = 1;
constexpr std::size_t codaTranspositionSlot = 4;
constexpr std::size_t codaDisplayFlagsSlot = 5;
constexpr std::size_t codaStaffLineValueSlot = 2;
constexpr std::size_t codaNoteFontIdSlot = 3;
constexpr std::size_t codaNoteFontSizeEffectsSlot = 4;
constexpr std::size_t codaPrimaryFlagsSlot = 5;

constexpr std::size_t botBarlineSlot = 0;
constexpr std::size_t tablaturePositionsSlot = 1;
constexpr std::size_t altFlagsSlot = 2;
constexpr std::size_t noteFontIdSlot = 3;
constexpr std::size_t noteFontSizeEffectsSlot = 4;
constexpr std::size_t primaryFlagsSlot = 5;
constexpr std::size_t clefsSlot = 6;
constexpr std::size_t topStaffLinesSlot = 7;
constexpr std::size_t bottomStaffLinesSlot = 8;
constexpr std::size_t topBarlineSlot = 9;
constexpr std::size_t transpositionSlot = 10;
constexpr std::size_t displayFlagsSlot = 11;
constexpr std::size_t doubleWholeWholeRestSlot = 12;
constexpr std::size_t halfOtherRestSlot = 13;
constexpr std::size_t stemReversalSlot = 14;
constexpr std::size_t fullNameSlot = 15;
constexpr std::size_t abbreviatedNameSlot = 16;
constexpr std::size_t repeatDotOffsetsSlot = 17;
constexpr std::size_t lineSpaceSlot = 18;
constexpr std::size_t verticalTabOffsetSlot = 20;
constexpr std::size_t extendedFlagsSlot = 22;
constexpr std::size_t fretInstrumentSlot = 23;
constexpr std::size_t horizontalStemUpSlot = 24;
constexpr std::size_t horizontalStemDownSlot = 26;
constexpr std::size_t verticalStemStartUpSlot = 28;
constexpr std::size_t verticalStemStartDownSlot = 30;
constexpr std::size_t verticalStemEndUpSlot = 32;
constexpr std::size_t verticalStemEndDownSlot = 34;
constexpr std::size_t alternateFlags2Slot = 36;
constexpr std::size_t autoNumberingSlot = 37;
constexpr std::size_t instrumentUuidSlot = 40;
constexpr std::size_t instrumentUuidBytes = 16;
constexpr std::size_t finale2012StaffBytes = instrumentUuidSlot * 2 + instrumentUuidBytes;

constexpr std::size_t staffStyleMetadataWords = 6;
constexpr std::size_t staffStyleMetadataBytes = staffStyleMetadataWords * 2;
constexpr std::size_t staffStyleNarrowNameBytes = 48;
constexpr std::size_t staffStyleNarrowTrailerBytes = staffStyleMetadataBytes + staffStyleNarrowNameBytes;
constexpr std::size_t staffStyleLargestNarrowRecordBytes = 144;
constexpr std::size_t staffStyleUnicodeNameBytes = 192;
constexpr std::size_t staffStyleUnicodeTrailerBytes = staffStyleMetadataBytes + staffStyleUnicodeNameBytes;
constexpr std::size_t staffStyleSmallestUnicodeRecordBytes = finale2012StaffBytes + staffStyleUnicodeTrailerBytes;
constexpr std::size_t staffStyleFirstMaskSlot = 0;
constexpr std::size_t staffStyleSecondMaskSlot = 1;
constexpr std::size_t staffStyleThirdMaskSlot = 2;
constexpr std::size_t staffStyleFlagsSlot = 5;
constexpr std::uint16_t staffStyleAlternateNotationMask = 0x1000;

struct StaffStyleMaskField
{
    const char* name;
    bool StaffStyleTarget::Masks::* member;
    std::size_t slot;
    std::uint16_t bit;
    bool includedByAlternateNotation{};
};

struct StaffStyleTrailerLayout
{
    std::size_t trailerBytes;
    std::size_t nameBytes;
    bool unicodeName;
};

constexpr auto staffStyleNegTimeMaskMember = &StaffStyleTarget::Masks::negTime;
constexpr auto staffStyleNegTimePartsMaskMember = &StaffStyleTarget::Masks::negTimeParts;

[[nodiscard]] std::optional<StaffStyleTrailerLayout> staffStyleTrailerLayout(std::size_t recordBytes, const std::optional<SourceVersion>& version)
{
    if (recordBytes >= staffStyleSmallestUnicodeRecordBytes) {
        return StaffStyleTrailerLayout{staffStyleUnicodeTrailerBytes, staffStyleUnicodeNameBytes, true};
    }
    if (recordBytes <= staffStyleLargestNarrowRecordBytes) {
        return StaffStyleTrailerLayout{staffStyleNarrowTrailerBytes, staffStyleNarrowNameBytes, false};
    }
    return versions::storesUnicodeCodepoints(version)
               ? std::optional{StaffStyleTrailerLayout{staffStyleUnicodeTrailerBytes, staffStyleUnicodeNameBytes, true}}
               : std::optional{StaffStyleTrailerLayout{staffStyleNarrowTrailerBytes, staffStyleNarrowNameBytes, false}};
}

constexpr StaffStyleMaskField staffStyleMaskFields[] = {
    {"floatNoteheadFont", &StaffStyleTarget::Masks::floatNoteheadFont, staffStyleFirstMaskSlot, 0x0001},
    {"flatBeams", &StaffStyleTarget::Masks::flatBeams, staffStyleFirstMaskSlot, 0x0002},
    {"notationStyle", &StaffStyleTarget::Masks::notationStyle, staffStyleFirstMaskSlot, 0x0004},
    {"blankMeasureRest", &StaffStyleTarget::Masks::blankMeasureRest, staffStyleFirstMaskSlot, 0x0010},
    {"noOptimize", &StaffStyleTarget::Masks::noOptimize, staffStyleFirstMaskSlot, 0x0020},
    {"defaultClef", &StaffStyleTarget::Masks::defaultClef, staffStyleFirstMaskSlot, 0x0040},
    {"staffType", &StaffStyleTarget::Masks::staffType, staffStyleFirstMaskSlot, 0x0080},
    {"transposition", &StaffStyleTarget::Masks::transposition, staffStyleFirstMaskSlot, 0x0100},
    {"blineBreak", &StaffStyleTarget::Masks::blineBreak, staffStyleFirstMaskSlot, 0x0200},
    {"rbarBreak", &StaffStyleTarget::Masks::rbarBreak, staffStyleFirstMaskSlot, 0x0400},
    {"negMnumb", &StaffStyleTarget::Masks::negMnumb, staffStyleFirstMaskSlot, 0x1000},
    {"negRepeat", &StaffStyleTarget::Masks::negRepeat, staffStyleFirstMaskSlot, 0x2000},
    {"negNameScore", &StaffStyleTarget::Masks::negNameScore, staffStyleFirstMaskSlot, 0x4000},
    {"hideBarlines", &StaffStyleTarget::Masks::hideBarlines, staffStyleFirstMaskSlot, 0x8000},
    {"fullName", &StaffStyleTarget::Masks::fullName, staffStyleSecondMaskSlot, 0x0001},
    {"abrvName", &StaffStyleTarget::Masks::abrvName, staffStyleSecondMaskSlot, 0x0002},
    {"floatKeys", &StaffStyleTarget::Masks::floatKeys, staffStyleSecondMaskSlot, 0x0004},
    {"floatTime", &StaffStyleTarget::Masks::floatTime, staffStyleSecondMaskSlot, 0x0008},
    {"hideRptBars", &StaffStyleTarget::Masks::hideRptBars, staffStyleSecondMaskSlot, 0x0010},
    {"negKey", &StaffStyleTarget::Masks::negKey, staffStyleSecondMaskSlot, 0x0020},
    {"negTime", staffStyleNegTimeMaskMember, staffStyleSecondMaskSlot, 0x0040},
    {"negClef", &StaffStyleTarget::Masks::negClef, staffStyleSecondMaskSlot, 0x0080},
    {"hideStaff", &StaffStyleTarget::Masks::hideStaff, staffStyleSecondMaskSlot, 0x0100},
    {"noKey", &StaffStyleTarget::Masks::noKey, staffStyleSecondMaskSlot, 0x0200},
    {"fullNamePos", &StaffStyleTarget::Masks::fullNamePos, staffStyleSecondMaskSlot, 0x0400},
    {"abrvNamePos", &StaffStyleTarget::Masks::abrvNamePos, staffStyleSecondMaskSlot, 0x0800},
    {"altNotation", &StaffStyleTarget::Masks::altNotation, staffStyleSecondMaskSlot, staffStyleAlternateNotationMask},
    {"showTies", &StaffStyleTarget::Masks::showTies, staffStyleSecondMaskSlot, 0x2000},
    {"showDots", &StaffStyleTarget::Masks::showDots, staffStyleSecondMaskSlot, 0x4000},
    {"showRests", &StaffStyleTarget::Masks::showRests, staffStyleSecondMaskSlot, 0x8000},
    {"showStems", &StaffStyleTarget::Masks::showStems, staffStyleThirdMaskSlot, 0x0001},
    {"hideChords", &StaffStyleTarget::Masks::hideChords, staffStyleThirdMaskSlot, 0x0002, true},
    {"hideFretboards", &StaffStyleTarget::Masks::hideFretboards, staffStyleThirdMaskSlot, 0x0004, true},
    {"hideLyrics", &StaffStyleTarget::Masks::hideLyrics, staffStyleThirdMaskSlot, 0x0008},
    {"showNameParts", &StaffStyleTarget::Masks::showNameParts, staffStyleThirdMaskSlot, 0x0010},
    {"showNoteColors", &StaffStyleTarget::Masks::showNoteColors, staffStyleThirdMaskSlot, 0x0020},
    {"hideStaffLines", &StaffStyleTarget::Masks::hideStaffLines, staffStyleThirdMaskSlot, 0x0040},
    {"useNoteShapes", &StaffStyleTarget::Masks::useNoteShapes, staffStyleThirdMaskSlot, 0x0080},
    {"hideKeySigsShowAccis", &StaffStyleTarget::Masks::hideKeySigsShowAccis, staffStyleThirdMaskSlot, 0x0100},
    {"redisplayLayerAccis", &StaffStyleTarget::Masks::redisplayLayerAccis, staffStyleThirdMaskSlot, 0x0200},
    {"negTimeParts", staffStyleNegTimePartsMaskMember, staffStyleThirdMaskSlot, 0x0400},
};

constexpr std::uint16_t staffStyleCopyableBit = 0x0002;
constexpr std::uint16_t staffStyleAddToMenuBit = 0x0004;
constexpr std::uint16_t staffStyleExplicitControlsBit = 0x0001;

constexpr StaffEfixField staffNoStemOffsetFinale27DefaultFields[] = {
    {"horzStemOffUp", &StaffTarget::horzStemOffUp, horizontalStemUpSlot},
    {"horzStemOffDown", &StaffTarget::horzStemOffDown, horizontalStemDownSlot},
    {"vertStemStartOffUp", &StaffTarget::vertStemStartOffUp, verticalStemStartUpSlot},
    {"vertStemStartOffDown", &StaffTarget::vertStemStartOffDown, verticalStemStartDownSlot},
    {"vertStemEndOffUp", &StaffTarget::vertStemEndOffUp, verticalStemEndUpSlot},
    {"vertStemEndOffDown", &StaffTarget::vertStemEndOffDown, verticalStemEndDownSlot},
};

[[nodiscard]] bool hasFinale2012StaffLayout(std::span<const std::uint8_t> payload)
{
    return payload.size() >= finale2012StaffBytes;
}

[[nodiscard]] bool hasSecondAlternateNotationFlags(std::span<const std::uint8_t> payload)
{
    // The second word supplies independent other-layer controls; without it,
    // the first word's controls retain their aggregate semantics. Payload shape
    // states this distinction even when a source version is unavailable.
    return payload.size() >= (alternateFlags2Slot + 1) * 2;
}

[[nodiscard]] bool usesSixWordStaffLayout(const SourceProfile& profile, const RecordFamilySource& source, std::span<const std::uint8_t> payload)
{
    // The six-word Staff layout survives into early uncompressed containers. Its
    // exact payload shape distinguishes it from the later multi-incidence IS
    // layout.
    return profile.epoch == FormatEpoch::CodaBanner || (source.identity == staffTag && payload.size() == staffCodaWords * 2);
}

StaffFallbackSelection selectStaffFallbacks(bool sixWordLayout, std::span<const std::uint8_t> payload)
{
    const auto wordCount = payload.size() / 2;
    return {.lineSpace = sixWordLayout || wordCount < lineSpaceSlot + 2,
        .restOffsets = sixWordLayout,
        .stemReversal = sixWordLayout,
        .hideMode = sixWordLayout,
        .hideStaffLines = !hasFinale2012StaffLayout(payload),
        .fretInstId = sixWordLayout || wordCount <= fretInstrumentSlot,
        .sixWordStaffFields = sixWordLayout,
        .extendedFlagFields = sixWordLayout || wordCount <= extendedFlagsSlot,
        .stemOffsetFields = sixWordLayout || wordCount < verticalStemEndDownSlot + 2,
        .wordCount = wordCount,
        .repeatDotOffsets = sixWordLayout || wordCount <= staffBaseWords ? StaffRepeatDotFallback::LegacyGeometry : StaffRepeatDotFallback::None};
}

bool resolveStaffNoteFontSize(const ImportContext& context, StaffTarget& target)
{
    if (!target.useNoteFont || !target.noteFont || target.noteFont->fontSize != 0) {
        return false;
    }
    const auto font = musx::dom::options::FontOptions::getFontInfoOrNull(context.document, musx::dom::options::FontOptions::FontType::Noteheads);
    if (!font) {
        throw std::logic_error("Document is missing its notehead font default");
    }
    target.noteFont->fontSize = font->fontSize;
    return true;
}

void applyLegacyStaffRepeatDotOffsets(StaffTarget& target)
{
    auto middleLine = target.calcMiddleStaffPosition();
    if (middleLine % 2 != 0) {
        ++middleLine;
    }
    target.botRepeatDotOff = middleLine - 1;
    target.topRepeatDotOff = middleLine + 1;
}

void resolveStaffRepeatDotFallback(const StaffTarget& target, StaffFallbackSelection& selected)
{
    if (selected.repeatDotOffsets == StaffRepeatDotFallback::LegacyGeometry && !target.customStaff) {
        selected.repeatDotOffsets = StaffRepeatDotFallback::Finale27Default;
    }
}

void applyStaffFallbacks(StaffTarget& target, const StaffTarget& defaults, StaffFallbackSelection selected)
{
    if (selected.lineSpace) {
        target.lineSpace = defaults.lineSpace;
    }
    if (selected.restOffsets) {
        target.dwRestOffset = defaults.dwRestOffset;
        target.wRestOffset = defaults.wRestOffset;
        target.hRestOffset = defaults.hRestOffset;
        target.otherRestOffset = defaults.otherRestOffset;
    }
    if (selected.stemReversal) {
        target.stemReversal = defaults.stemReversal;
    }
    if (selected.hideMode) {
        target.hideMode = defaults.hideMode;
    }
    if (selected.hideStaffLines) {
        target.hideStaffLines = defaults.hideStaffLines;
    }
    if (selected.fretInstId) {
        target.fretInstId = defaults.fretInstId;
    }
    if (selected.sixWordStaffFields) {
        target.capoPos = defaults.capoPos;
        target.lowestFret = defaults.lowestFret;
        if (!target.transposition || !target.transposition->setToClef) {
            target.transposedClef = defaults.transposedClef;
        }
        for (const auto& field : staffSixWordFinale27DefaultFields) {
            target.*field.member = defaults.*field.member;
        }
    }
    if (selected.extendedFlagFields) {
        for (const auto& field : staffNoExtendedFlagFinale27DefaultFields) {
            target.*field.member = defaults.*field.member;
        }
        target.stemDirection = defaults.stemDirection;
        if (target.notationStyle != StaffTarget::NotationStyle::Tablature) {
            for (const auto& field : staffLegacyTabBooleanDefaults) {
                target.*field.member = defaults.*field.member;
            }
        }
        target.breakTabLinesAtNotes = defaults.breakTabLinesAtNotes;
    }
    if (selected.stemOffsetFields) {
        for (const auto& field : staffNoStemOffsetFinale27DefaultFields) {
            if (selected.wordCount < field.slot + 2) {
                target.*field.member = defaults.*field.member;
            }
        }
    }
    if (selected.repeatDotOffsets == StaffRepeatDotFallback::LegacyGeometry) {
        applyLegacyStaffRepeatDotOffsets(target);
    } else if (selected.repeatDotOffsets == StaffRepeatDotFallback::Finale27Default) {
        target.botRepeatDotOff = defaults.botRepeatDotOff;
        target.topRepeatDotOff = defaults.topRepeatDotOff;
    }
}

void synthesizeLegacyStaffName(const ImportContext& context, StaffTarget& staff, records::LegacyTag tag,
    musx::dom::options::FontOptions::FontType fontType, musx::dom::Cmper StaffTarget::* nameMember,
    text::EnigmaFontResolutionCache& fontResolutionCache)
{
    using BlockText = musx::dom::texts::BlockText;
    using TextBlock = musx::dom::others::TextBlock;

    if (staff.*nameMember != 0) {
        return;
    }

    const auto& pool = context.index.getOthers();
    const auto rows = pool.getArray(tag, staff.getCmper(), 0, staff.getSourcePartId());
    if (rows.empty()) {
        return;
    }
    const auto raw = readRowText(pool, rows);
    if (raw.empty()) {
        return;
    }

    const auto textNumber = context.document->getTexts()->nextFreeCmper<BlockText>();
    const auto blockNumber = context.document->getOthers()->nextFreeCmper<TextBlock>(staff.getSourcePartId());
    if (!textNumber || !blockNumber) {
        context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning, "Staff-name import exhausted the text identifier space."});
        return;
    }

    const text::EnigmaTextSource source{context.document,
        /*utf8*/ false, context.profile.platform, musx::dom::options::FontOptions::getFontInfoOrNull(context.document, fontType),
        &fontResolutionCache};
    auto converted = text::toModernEnigmaText(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(raw.data()), raw.size()), source);
    auto rawText = std::make_shared<BlockText>(context.document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, *textNumber);
    rawText->text = std::move(converted.text);

    auto block = std::make_shared<TextBlock>(context.document, staff.getSourcePartId(), musx::dom::EnigmaBase::ShareMode::All, *blockNumber);
    block->textId = *textNumber;
    block->textType = TextBlock::TextType::Block;
    block->lineSpacingPercentage = 100;
    const auto fullName = nameMember == &StaffTarget::fullNameTextId;
    std::shared_ptr<const musx::dom::others::NamePositioning> namePosition;
    if (fullName) {
        namePosition = context.document->getOthers()->get<musx::dom::others::NamePositionFull>(staff.getSourcePartId(), staff.getCmper());
    } else {
        namePosition = context.document->getOthers()->get<musx::dom::others::NamePositionAbbreviated>(staff.getSourcePartId(), staff.getCmper());
    }
    if (namePosition) {
        block->justify = static_cast<TextBlock::TextJustify>(legacyCenterOppositeOrder(static_cast<std::int64_t>(namePosition->justify)));
    }
    block->newPos36 = true;
    block->showShape = true;
    block->wordWrap = true;
    staff.*nameMember = *blockNumber;

    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto& row = rows.front();
        const auto rawKey = reporting.template instanceKey<BlockText>(musx::dom::SCORE_PARTID, *textNumber);
        const auto blockKey = reporting.template instanceKey<TextBlock>(staff.getSourcePartId(), *blockNumber);
        const auto staffKey = reporting.template instanceKey<StaffTarget>(staff.getSourcePartId(), staff.getCmper());
        reporting.report().setInstanceOrigin(rawKey, Reporting::Origin::LegacyMus);
        reporting.report().setInstanceOrigin(blockKey, Reporting::Origin::LegacyBehavior);
        reporting.textField(rawKey, "text", converted);
        reporting.report().setField(
            rawKey, "text", {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset, static_cast<std::int64_t>(rawText->text.size()), tag});
        reporting.report().setField(staffKey, nameMember == &StaffTarget::fullNameTextId ? "fullNameTextId" : "abbrvNameTextId",
            {Reporting::Origin::LegacyBehavior, row.blockOffset, row.decodedOffset, *blockNumber, tag});
        reporting.report().setField(blockKey, "textId", {Reporting::Origin::LegacyBehavior, row.blockOffset, row.decodedOffset, *textNumber, tag});
        const auto behavior = [&](const char* field, std::int64_t value) {
            reporting.report().setField(blockKey, field, {Reporting::Origin::LegacyBehavior, 0, 0, value});
        };
        behavior("lineSpacingPercentage", 100);
        behavior("shapeId", 0);
        if (namePosition) {
            const auto sourceKey =
                fullName ? reporting.template instanceKey<musx::dom::others::NamePositionFull>(staff.getSourcePartId(), staff.getCmper())
                         : reporting.template instanceKey<musx::dom::others::NamePositionAbbreviated>(staff.getSourcePartId(), staff.getCmper());
            if (const auto* source = reporting.report().findField(sourceKey, "justify")) {
                auto adjusted = *source;
                adjusted.origin = Reporting::Origin::LegacyMusAdjusted;
                reporting.report().setField(blockKey, "justify", adjusted);
            } else {
                behavior("justify", static_cast<std::int64_t>(block->justify));
            }
        } else {
            behavior("justify", static_cast<std::int64_t>(block->justify));
        }
        behavior("newPos36", 1);
        behavior("showShape", 1);
        behavior("noExpandSingleWord", 0);
        behavior("wordWrap", 1);
        behavior("roundCorners", 0);
        behavior("cornerRadius", 0);
    });

    context.document->getTexts()->add(BlockText::XmlNodeName, std::move(rawText));
    context.document->getOthers()->add(TextBlock::XmlNodeName, std::move(block));
}

[[nodiscard]] bool usesParallelStaffNames(const SourceProfile& profile)
{
    // Finale 3.7 moves Staff names from parallel rows to references in the Staff
    // record.
    return sourcePredatesVersion(profile, FormatEpoch::UncompressedLegacy, versions::finale3_7);
}

constexpr std::uint16_t altNotationMask = 0x000f;
constexpr std::uint16_t altLayerMask = 0x00f0;
constexpr std::uint16_t altShowArticulationsMask = 0x0100;
constexpr std::uint16_t altShowLyricsMask = 0x0200;
constexpr std::uint16_t altShowSmartShapesMask = 0x0400;
constexpr std::uint16_t altRhythmStemsUpMask = 0x0800;
constexpr std::uint16_t altSlashDotsMask = 0x1000;
constexpr std::uint16_t altShowOtherNotesMask = 0x2000;
constexpr std::uint16_t altShowOtherArticulationsMask = 0x4000;
constexpr std::uint16_t altShowExpressionsMask = 0x8000;

constexpr std::uint16_t hideRepeatBottomDotMask = 0x8000;
constexpr std::uint16_t flatBeamsMask = 0x4000;
constexpr std::uint16_t percussionNotationMask = 0x2000;
constexpr std::uint16_t hideFretboardsMask = 0x0800;
constexpr std::uint16_t tablatureNotationMask = 0x0200;
constexpr std::uint16_t blankMeasureMask = 0x0100;
constexpr std::uint16_t useNoteShapesMask = 0x0080;
constexpr std::uint16_t hideRepeatTopDotMask = 0x0040;
constexpr std::uint16_t useNoteFontMask = 0x0020;
constexpr std::uint16_t codaStaffLineOverrideMask = 0x0040;
constexpr std::uint16_t hideLyricsMask = 0x0010;
constexpr std::uint16_t showNameInPartsMask = 0x0008;
constexpr std::uint16_t showNoteColorsMask = 0x0004;
constexpr std::uint16_t noOptimizeMask = 0x0001;

constexpr std::uint16_t setToClefMask = 0x8000;
constexpr std::uint16_t noSimplifyKeyMask = 0x4000;
constexpr std::uint16_t chromaticTranspositionMask = 0x2000;
constexpr std::uint16_t codaTransposedClefMask = 0x7000;
constexpr unsigned codaTransposedClefShift = 12;

constexpr std::uint16_t floatKeysMask = 0x8000;
constexpr std::uint16_t floatTimeMask = 0x4000;
constexpr std::uint16_t breakBarlinesMask = 0x2000;
constexpr std::uint16_t breakRepeatBarlinesMask = 0x1000;
constexpr std::uint16_t hideMeasureNumbersMask = 0x0400;
constexpr std::uint16_t hideRepeatsMask = 0x0200;
constexpr std::uint16_t hideNameMask = 0x0100;
constexpr std::uint16_t hideBarlinesMask = 0x0080;
constexpr std::uint16_t hideRepeatBarsMask = 0x0040;
constexpr std::uint16_t hideKeySignaturesMask = 0x0020;
constexpr std::uint16_t hideTimeSignaturesMask = 0x0010;
constexpr std::uint16_t hideClefsMask = 0x0008;
constexpr std::uint16_t hideStaffOrLinesMask = 0x0004;
constexpr std::uint16_t hideChordsMask = 0x0002;
constexpr std::uint16_t noKeyMask = 0x0001;
constexpr std::uint16_t codaDefaultClefMask = 0x0007;

constexpr std::uint16_t clefFirstMeasureOnlyMask = 0x0001;
constexpr std::uint16_t showRestsMask = 0x0002;
constexpr std::uint16_t showTiesMask = 0x0004;
constexpr std::uint16_t showDotsMask = 0x0008;
constexpr std::uint16_t showStemsMask = 0x0010;
constexpr std::uint16_t stemDirectionMask = 0x0060;
constexpr std::uint16_t stemsAlwaysUpMask = 0x0040;
constexpr std::uint16_t stemsAlwaysDownMask = 0x0020;
constexpr std::uint16_t stemStartFromStaffMask = 0x0080;
constexpr std::uint16_t stemsFixedEndMask = 0x0100;
constexpr std::uint16_t useTabLettersMask = 0x0200;
constexpr std::uint16_t showBeamsMask = 0x0400;
constexpr std::uint16_t breakTabLinesMask = 0x0800;
constexpr std::uint16_t stemsFixedStartMask = 0x1000;
constexpr std::uint16_t showTupletsMask = 0x2000;
constexpr std::uint16_t hideModeMask = 0xc000;
constexpr std::uint16_t hideScoreMask = 0x4000;
constexpr std::uint16_t hideScorePartsMask = 0x8000;

constexpr std::uint16_t altShowOtherLyricsMask = 0x0001;
constexpr std::uint16_t altShowOtherSmartShapesMask = 0x0002;
constexpr std::uint16_t altShowOtherExpressionsMask = 0x0004;

constexpr std::uint16_t autoNumberingEnabledMask = 0x8000;
constexpr std::uint16_t autoNumberingStyleMask = 0x7fff;

[[nodiscard]] bool sourceHasStaffAlternateNotationProperties(const SourceProfile& profile)
{
    // Before Finale 2000, alternate notation is applied by range records rather
    // than Staff properties.
    return sourceAtOrAfter(profile, FormatEpoch::UncompressedLegacy, versions::finale2000);
}

[[nodiscard]] std::int8_t signedByte(std::uint16_t value, bool high)
{
    return static_cast<std::int8_t>(high ? value >> 8U : value & 0xffU);
}

[[nodiscard]] int signExtend(std::uint16_t value, unsigned bits)
{
    const auto mask = (std::uint16_t{1} << bits) - 1U;
    value &= mask;
    const auto sign = std::uint16_t{1} << (bits - 1U);
    return static_cast<int>((value ^ sign) - sign);
}

void decodeStaffTransposition(const std::shared_ptr<StaffTarget>& target, std::uint16_t value, bool sixWordLayout = false)
{
    if (!value) {
        return;
    }
    target->transposition = std::make_shared<StaffTarget::Transposition>(target);
    target->transposition->setToClef = value & setToClefMask;
    if (sixWordLayout && target->transposition->setToClef) {
        target->transposedClef = (value & codaTransposedClefMask) >> codaTransposedClefShift;
        target->transposition->keysig = std::make_shared<StaffTarget::KeySigTransposition>();
        return;
    }
    target->transposition->noSimplifyKey = value & noSimplifyKeyMask;
    if (value & chromaticTranspositionMask) {
        target->transposition->chromatic = std::make_shared<StaffTarget::ChromaticTransposition>();
        target->transposition->chromatic->alteration = signExtend(value >> 8U, 4);
        target->transposition->chromatic->diatonic = signExtend(value, 8);
    } else {
        target->transposition->keysig = std::make_shared<StaffTarget::KeySigTransposition>();
        target->transposition->keysig->interval = signExtend(value >> 6U, 6);
        target->transposition->keysig->adjust = signExtend(value, 6);
    }
}

[[nodiscard]] std::string instrumentUuid(std::span<const std::uint8_t> payload)
{
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(36);
    for (std::size_t index = 0; index < instrumentUuidBytes; ++index) {
        if (index == 4 || index == 6 || index == 8 || index == 10) {
            result.push_back('-');
        }
        const auto value = payload[instrumentUuidSlot * 2 + index];
        result.push_back(hex[value >> 4U]);
        result.push_back(hex[value & 0x0fU]);
    }
    return result;
}

[[nodiscard]] std::vector<int> customStaffLines(std::uint16_t top, std::uint16_t bottom)
{
    constexpr unsigned lineCount = 27;
    constexpr unsigned decodingRotation = 11;
    constexpr std::uint32_t lineMask = (std::uint32_t{1} << lineCount) - 1;
    const auto stored = (static_cast<std::uint32_t>(top) << 16U) | bottom;
    const auto decoded = std::rotl(stored, decodingRotation) & lineMask;

    std::vector<int> result;
    for (unsigned line = 0; line < lineCount; ++line) {
        if (decoded & (std::uint32_t{1} << line)) {
            result.push_back(static_cast<int>(line));
        }
    }
    return result;
}

struct DecodedStaffNotation
{
    StaffTarget::NotationStyle notationStyle{};
    bool useNoteShapes{};
    bool noteShapeNotation{};
};

[[nodiscard]] DecodedStaffNotation decodeStaffNotation(std::uint16_t flags, bool usesLegacyNotationStyles)
{
    const auto useNoteShapes = bool(flags & useNoteShapesMask);
    StaffTarget::NotationStyle notationStyle{};
    if (flags & percussionNotationMask) {
        notationStyle = StaffTarget::NotationStyle::Percussion;
    } else if (flags & tablatureNotationMask) {
        notationStyle = StaffTarget::NotationStyle::Tablature;
    }
    return {notationStyle, useNoteShapes, usesLegacyNotationStyles && notationStyle == StaffTarget::NotationStyle::Standard && useNoteShapes};
}

[[nodiscard]] musx::dom::StemDirection stemDirection(std::uint16_t flags)
{
    switch (flags & stemDirectionMask) {
    case stemsAlwaysUpMask: return musx::dom::StemDirection::AlwaysUp;
    case stemsAlwaysDownMask: return musx::dom::StemDirection::AlwaysDown;
    default: return musx::dom::StemDirection::Default;
    }
}

[[nodiscard]] StaffTarget::HideMode hideMode(std::uint16_t flags)
{
    switch (flags & hideModeMask) {
    case hideScoreMask: return StaffTarget::HideMode::Score;
    case hideScorePartsMask: return StaffTarget::HideMode::ScoreParts;
    case hideModeMask: return StaffTarget::HideMode::Cutaway;
    default: return StaffTarget::HideMode::None;
    }
}

template <typename Reporting, typename Value>
void reportStaffField(Reporting& reporting, const typename Reporting::InstanceKey& key, const RecordFamilySource& source,
    std::span<const records::LegacyRow> rows, const char* member, std::size_t slot, Value value,
    typename Reporting::Origin origin = Reporting::Origin::LegacyMus)
{
    const auto byteOffset = slot * 2;
    const auto& row = source.classRecords ? rows.front() : rows[(std::min)(slot / records::otherWordCount, rows.size() - 1)];
    const auto rowByteOffset = source.classRecords ? byteOffset : (slot % records::otherWordCount) * 2;
    reporting.report().setField(key, Reporting::memberName(member),
        typename Reporting::FieldInfo{origin, row.blockOffset, row.decodedOffset + rowByteOffset, static_cast<std::int64_t>(value), source.identity});
}

void decodeStaffStyleTrailer(const ImportContext& context, const std::shared_ptr<StaffStyleTarget>& target, std::span<const std::uint8_t> payload,
    std::size_t staffPayloadBytes, const StaffStyleTrailerLayout& layout)
{
    target->masks = std::make_shared<StaffStyleTarget::Masks>(target);
    const auto metadata = payload.subspan(staffPayloadBytes, staffStyleMetadataBytes);
    const auto word = [&](std::size_t slot) { return payloadWord(metadata, slot * 2, context.profile.byteOrder); };
    const auto alternateNotation = word(staffStyleSecondMaskSlot) & staffStyleAlternateNotationMask;
    for (const auto& field : staffStyleMaskFields) {
        target->masks.get()->*field.member = (word(field.slot) & field.bit) || (field.includedByAlternateNotation && alternateNotation);
    }
    target->masks.get()->*staffStyleNegTimePartsMaskMember = target->masks.get()->*staffStyleNegTimeMaskMember;
    const auto flags = word(staffStyleFlagsSlot);
    target->copyable = (flags & staffStyleCopyableBit) || (!(flags & staffStyleExplicitControlsBit) && alternateNotation);
    // Without the low marker, menu inclusion uses the legacy implicit-on
    // representation.
    target->addToMenu = !(flags & staffStyleExplicitControlsBit) || (flags & staffStyleAddToMenuBit);
    const auto name = payload.subspan(staffPayloadBytes + staffStyleMetadataBytes, layout.nameBytes);
    target->styleName = text::normalizeLineBreaks(
        layout.unicodeName ? text::utf16LeToUtf8(name) : text::toUtf8(payloadString(name, 0, name.size()), context.profile.platform));
}

[[nodiscard]] bool applyLegacyStaffStyleNoteShapeMasks(StaffStyleTarget& target, bool noteShapeNotation)
{
    if (!noteShapeNotation || !target.masks->notationStyle) {
        return false;
    }
    // The legacy source model groups Note Shapes with the notation styles. The
    // modern model separates that Standard-notation case from the
    // instrument-changing notation-style mask.
    target.masks->notationStyle = false;
    target.masks->useNoteShapes = true;
    return true;
}

template <typename Reporting>
void reportStaffStyleTrailer(Reporting& reporting, const StaffStyleTarget& target, const RecordFamilySource& source,
    std::span<const records::LegacyRow> rows, std::size_t staffPayloadBytes, bool adjustedNoteShapeMasks)
{
    const auto key = reporting.template instanceKey<StaffStyleTarget>(target.getSourcePartId(), target.getCmper());
    const auto metadataSlot = staffPayloadBytes / 2;
    for (const auto& field : staffStyleMaskFields) {
        const auto member = std::string("masks->") + field.name;
        if (field.member == staffStyleNegTimePartsMaskMember) {
            reporting.report().setField(key, Reporting::memberName(member.c_str()),
                {Reporting::Origin::LegacyBehavior, 0, 0, target.masks.get()->*staffStyleNegTimePartsMaskMember});
            continue;
        }
        const auto isAdjustedNoteShapeMask =
            adjustedNoteShapeMasks
            && (field.member == &StaffStyleTarget::Masks::notationStyle || field.member == &StaffStyleTarget::Masks::useNoteShapes);
        const auto sourceSlot = isAdjustedNoteShapeMask                                          ? staffStyleFirstMaskSlot
                                : field.includedByAlternateNotation && target.masks->altNotation ? staffStyleSecondMaskSlot
                                                                                                 : field.slot;
        reportStaffField(reporting, key, source, rows, member.c_str(), metadataSlot + sourceSlot, target.masks.get()->*field.member,
            isAdjustedNoteShapeMask ? Reporting::Origin::LegacyMusAdjusted : Reporting::Origin::LegacyMus);
    }
    reportStaffField(reporting, key, source, rows, "copyable", metadataSlot + staffStyleFlagsSlot, target.copyable);
    reportStaffField(reporting, key, source, rows, "addToMenu", metadataSlot + staffStyleFlagsSlot, target.addToMenu);
    reportStaffField(reporting, key, source, rows, "styleName", metadataSlot + staffStyleMetadataWords, target.styleName.size());
}

template <typename Reporting>
void reportStaffBooleanLegacyBehavior(
    Reporting& reporting, const StaffTarget& target, const typename Reporting::InstanceKey& key, std::span<const StaffBooleanLegacyDefault> fields)
{
    for (const auto& field : fields) {
        reporting.report().setField(key, field.name, {Reporting::Origin::LegacyBehavior, 0, 0, target.*field.member});
    }
}

template <typename Reporting>
void reportStaffAlternateNotationLegacyBehavior(Reporting& reporting, const StaffTarget& target, const typename Reporting::InstanceKey& key)
{
    reporting.report().setField(key, "altNotation", {Reporting::Origin::LegacyBehavior, 0, 0, static_cast<std::int64_t>(target.altNotation)});
    reporting.report().setField(key, "altLayer", {Reporting::Origin::LegacyBehavior, 0, 0, target.altLayer});
    reportStaffBooleanLegacyBehavior(reporting, target, key, staffAlternateNotationBooleanDefaults);
}

template <typename Reporting>
void reportStaffFont(Reporting& reporting, const StaffTarget& target, const typename Reporting::InstanceKey& key, const RecordFamilySource& source,
    std::span<const records::LegacyRow> rows, std::size_t fontIdSlot, std::size_t sizeEffectsSlot, bool adjustedFontSize = false)
{
    if (!target.noteFont) {
        return;
    }
    reportStaffField(reporting, key, source, rows, "noteFont.fontId", fontIdSlot, target.noteFont->fontId);
    reportStaffField(reporting, key, source, rows, "noteFont.fontSize", sizeEffectsSlot, target.noteFont->fontSize,
        adjustedFontSize ? Reporting::Origin::LegacyMusAdjusted : Reporting::Origin::LegacyMus);
    reportStaffField(reporting, key, source, rows, "noteFont.bold", sizeEffectsSlot, target.noteFont->bold);
    reportStaffField(reporting, key, source, rows, "noteFont.italic", sizeEffectsSlot, target.noteFont->italic);
    reportStaffField(reporting, key, source, rows, "noteFont.underline", sizeEffectsSlot, target.noteFont->underline);
    reportStaffField(reporting, key, source, rows, "noteFont.strikeout", sizeEffectsSlot, target.noteFont->strikeout);
    reportStaffField(reporting, key, source, rows, "noteFont.absolute", sizeEffectsSlot, target.noteFont->absolute);
    reportStaffField(reporting, key, source, rows, "noteFont.hidden", sizeEffectsSlot, target.noteFont->hidden);
}

void synthesizeLegacySingleStringFretInstrument(const ImportContext& context, const std::shared_ptr<StaffTarget>& staff, std::uint8_t pitch,
    const records::LegacyRow& row, records::LegacyTag sourceTag, std::size_t pitchByteOffset)
{
    auto& others = *context.document->getOthers();
    std::optional<musx::dom::Cmper> availableId;
    for (std::uint32_t candidate = 2; candidate <= (std::numeric_limits<musx::dom::Cmper>::max)(); ++candidate) {
        const auto instrumentId = static_cast<musx::dom::Cmper>(candidate);
        if (!others.get<FretInstrumentTarget>(musx::dom::SCORE_PARTID, instrumentId)) {
            availableId = instrumentId;
            break;
        }
    }
    if (!availableId) {
        context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning, "No comparator is available for a synthesized one-string fret "
                                                                                     "instrument."});
        return;
    }
    const auto instrumentId = *availableId;

    auto instrument =
        std::make_shared<FretInstrumentTarget>(context.document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, instrumentId);
    instrument->numFrets = 20;
    instrument->numStrings = 1;
    instrument->name = legacySingleStringFretName(pitch);
    auto string = std::make_shared<FretInstrumentTarget::StringInfo>();
    string->pitch = pitch;
    instrument->strings.push_back(std::move(string));
    staff->fretInstId = instrumentId;

    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto staffKey = reporting.template instanceKey<StaffTarget>(staff->getSourcePartId(), staff->getCmper());
        const auto instrumentKey = reporting.template instanceKey<FretInstrumentTarget>(musx::dom::SCORE_PARTID, instrumentId);
        reporting.report().setField(staffKey, "fretInstId",
            {Reporting::Origin::LegacyBehavior, row.blockOffset, row.decodedOffset + pitchByteOffset, instrumentId, sourceTag});
        reporting.report().setInstanceOrigin(instrumentKey, Reporting::Origin::LegacyBehavior);
        const auto stored = [&](std::string member, auto value) {
            reporting.report().setField(instrumentKey, std::move(member),
                {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset + pitchByteOffset, value, sourceTag});
        };
        const auto behavior = [&](std::string member, auto value) {
            reporting.report().setField(instrumentKey, std::move(member), {Reporting::Origin::LegacyBehavior, 0, 0, value});
        };
        reportFretInstrumentFields(*instrument, false, stored, behavior);
    });

    others.add(FretInstrumentTarget::XmlNodeName, std::move(instrument));
}

template <typename Reporting>
void reportStaffTransposition(Reporting& reporting, const StaffTarget& target, const typename Reporting::InstanceKey& key,
    const RecordFamilySource& source, std::span<const records::LegacyRow> rows, std::size_t slot)
{
    const auto transposition = target.transposition;
    reportStaffField(reporting, key, source, rows, "transposition.setToClef", slot, transposition ? transposition->setToClef : false);
    reportStaffField(reporting, key, source, rows, "transposition.noSimplifyKey", slot, transposition ? transposition->noSimplifyKey : false);
    const auto keysig = transposition ? transposition->keysig : nullptr;
    reportStaffField(reporting, key, source, rows, "transposition.keysig.interval", slot, keysig ? keysig->interval : 0);
    reportStaffField(reporting, key, source, rows, "transposition.keysig.adjust", slot, keysig ? keysig->adjust : 0);
    const auto chromatic = transposition ? transposition->chromatic : nullptr;
    reportStaffField(reporting, key, source, rows, "transposition.chromatic.alteration", slot, chromatic ? chromatic->alteration : 0);
    reportStaffField(reporting, key, source, rows, "transposition.chromatic.diatonic", slot, chromatic ? chromatic->diatonic : 0);
}

template <typename Reporting, typename Target>
void reportMappedStaff(Reporting& reporting, const Target& target, const RecordFamilySource& source, std::span<const records::LegacyRow> rows,
    std::span<const std::uint8_t> payload, ByteOrder byteOrder, StaffLegacySemantics legacySemantics, bool adjustedFontSize)
{
    const auto key = reporting.template instanceKey<Target>(target.getSourcePartId(), target.getCmper());
    reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
#define STAFF_MAPPED(member, slot) reportStaffField(reporting, key, source, rows, #member, slot, target.member)
    STAFF_MAPPED(botBarlineOffset, botBarlineSlot);
    if (legacySemantics.singleStringTabPitch) {
        reporting.unmappedField(key, "capoPos", target.capoPos);
        reporting.unmappedField(key, "lowestFret", target.lowestFret);
    } else {
        STAFF_MAPPED(capoPos, tablaturePositionsSlot);
        STAFF_MAPPED(lowestFret, tablaturePositionsSlot);
    }
    reportStaffField(reporting, key, source, rows, "notationStyle", primaryFlagsSlot, target.notationStyle,
        legacySemantics.noteShapeNotation ? Reporting::Origin::LegacyMusAdjusted : Reporting::Origin::LegacyMus);
    reportStaffField(reporting, key, source, rows, "useNoteShapes", primaryFlagsSlot, target.useNoteShapes,
        legacySemantics.noteShapeNotation ? Reporting::Origin::LegacyMusAdjusted : Reporting::Origin::LegacyMus);
    STAFF_MAPPED(useNoteFont, primaryFlagsSlot);
    STAFF_MAPPED(defaultClef, clefsSlot);
    STAFF_MAPPED(transposedClef, clefsSlot);
    if (legacySemantics.legacySingleStringTabForm) {
        reporting.report().setField(key, "staffLines", {Reporting::Origin::LegacyBehavior, 0, 0, target.staffLines.value_or(0)});
        reporting.report().setField(key, "customStaff",
            {Reporting::Origin::LegacyBehavior, 0, 0, target.customStaff ? static_cast<std::int64_t>(target.customStaff->size()) : 0});
    } else {
        reportStaffField(reporting, key, source, rows, "staffLines", bottomStaffLinesSlot, target.staffLines.value_or(0));
        reportStaffField(reporting, key, source, rows, "customStaff", topStaffLinesSlot, target.customStaff ? target.customStaff->size() : 0);
    }
    STAFF_MAPPED(floatKeys, displayFlagsSlot);
    STAFF_MAPPED(floatTime, displayFlagsSlot);
    STAFF_MAPPED(blineBreak, displayFlagsSlot);
    STAFF_MAPPED(rbarBreak, displayFlagsSlot);
    STAFF_MAPPED(showNameInParts, primaryFlagsSlot);
    STAFF_MAPPED(showNoteColors, primaryFlagsSlot);
    STAFF_MAPPED(hideNameInScore, displayFlagsSlot);
    if (legacySemantics.hasAlternateNotationProperties) {
        STAFF_MAPPED(altNotation, altFlagsSlot);
        STAFF_MAPPED(altLayer, altFlagsSlot);
        STAFF_MAPPED(altHideArtics, altFlagsSlot);
        STAFF_MAPPED(altHideLyrics, altFlagsSlot);
        reportStaffField(reporting, key, source, rows, "altHideSmartShapes", altFlagsSlot, target.altHideSmartShapes,
            legacySemantics.synthesizesAltHideSmartShapes ? Reporting::Origin::LegacyMusAdjusted : Reporting::Origin::LegacyMus);
        STAFF_MAPPED(altRhythmStemsUp, altFlagsSlot);
        STAFF_MAPPED(altSlashDots, altFlagsSlot);
        if (legacySemantics.usesAggregateAlternateNotationItems) {
            reporting.report().setField(key, "altHideExpressions", {Reporting::Origin::Finale27Default, 0, 0, target.altHideExpressions});
        } else {
            STAFF_MAPPED(altHideExpressions, altFlagsSlot);
        }
        STAFF_MAPPED(altHideOtherNotes, altFlagsSlot);
        STAFF_MAPPED(altHideOtherArtics, altFlagsSlot);
    } else {
        reportStaffAlternateNotationLegacyBehavior(reporting, target, key);
    }
    if (legacySemantics.legacySingleStringTabForm) {
        reporting.report().setField(key, "hideRepeatBottomDot", {Reporting::Origin::LegacyBehavior, 0, 0, target.hideRepeatBottomDot});
    } else {
        STAFF_MAPPED(hideRepeatBottomDot, primaryFlagsSlot);
    }
    STAFF_MAPPED(flatBeams, primaryFlagsSlot);
    STAFF_MAPPED(hideFretboards, legacySemantics.hideNoteAttachedItems ? altFlagsSlot : primaryFlagsSlot);
    STAFF_MAPPED(blankMeasure, primaryFlagsSlot);
    if (legacySemantics.legacySingleStringTabForm) {
        reporting.report().setField(key, "hideRepeatTopDot", {Reporting::Origin::LegacyBehavior, 0, 0, target.hideRepeatTopDot});
    } else {
        STAFF_MAPPED(hideRepeatTopDot, primaryFlagsSlot);
    }
    STAFF_MAPPED(hideLyrics, primaryFlagsSlot);
    STAFF_MAPPED(noOptimize, primaryFlagsSlot);
    STAFF_MAPPED(topBarlineOffset, topBarlineSlot);
    STAFF_MAPPED(hideMeasNums, displayFlagsSlot);
    STAFF_MAPPED(hideRepeats, displayFlagsSlot);
    STAFF_MAPPED(hideBarlines, displayFlagsSlot);
    STAFF_MAPPED(hideRptBars, displayFlagsSlot);
    STAFF_MAPPED(hideKeySigs, displayFlagsSlot);
    STAFF_MAPPED(hideTimeSigs, displayFlagsSlot);
    STAFF_MAPPED(hideClefs, displayFlagsSlot);
    if (legacySemantics.usesBooleanHideMode) {
        STAFF_MAPPED(hideMode, displayFlagsSlot);
    }
    if (hasFinale2012StaffLayout(payload)) {
        STAFF_MAPPED(hideStaffLines, displayFlagsSlot);
    }
    STAFF_MAPPED(hideChords, legacySemantics.hideNoteAttachedItems ? altFlagsSlot : displayFlagsSlot);
    STAFF_MAPPED(noKey, displayFlagsSlot);
    STAFF_MAPPED(dwRestOffset, doubleWholeWholeRestSlot);
    STAFF_MAPPED(wRestOffset, doubleWholeWholeRestSlot);
    STAFF_MAPPED(hRestOffset, halfOtherRestSlot);
    STAFF_MAPPED(otherRestOffset, halfOtherRestSlot);
    STAFF_MAPPED(stemReversal, stemReversalSlot);
    STAFF_MAPPED(fullNameTextId, fullNameSlot);
    STAFF_MAPPED(abbrvNameTextId, abbreviatedNameSlot);
    reportStaffFont(reporting, target, key, source, rows, noteFontIdSlot, noteFontSizeEffectsSlot, adjustedFontSize);
    reportStaffTransposition(reporting, target, key, source, rows, transpositionSlot);
    const auto wordCount = payload.size() / 2;
    if (wordCount > staffBaseWords) {
        STAFF_MAPPED(botRepeatDotOff, repeatDotOffsetsSlot);
        STAFF_MAPPED(topRepeatDotOff, repeatDotOffsetsSlot);
    }
    if (legacySemantics.singleStringTabPitch) {
        STAFF_MAPPED(vertTabNumOff, tablaturePositionsSlot);
    }
    const auto longOrder = nativeLongWordOrder(byteOrder);
    if (wordCount >= lineSpaceSlot + 2) {
        reportStaffField(reporting, key, source, rows, "lineSpace", lineSpaceSlot, payloadLong(payload, lineSpaceSlot * 2, byteOrder, longOrder));
    }
    if (wordCount >= verticalTabOffsetSlot + 2) {
        reportStaffField(reporting, key, source, rows, "vertTabNumOff", verticalTabOffsetSlot,
            payloadLong(payload, verticalTabOffsetSlot * 2, byteOrder, longOrder));
    }
    if (wordCount > extendedFlagsSlot) {
        STAFF_MAPPED(showTabClefAllSys, extendedFlagsSlot);
        STAFF_MAPPED(hideRests, extendedFlagsSlot);
        STAFF_MAPPED(hideTies, extendedFlagsSlot);
        STAFF_MAPPED(hideDots, extendedFlagsSlot);
        STAFF_MAPPED(hideStems, extendedFlagsSlot);
        STAFF_MAPPED(stemDirection, extendedFlagsSlot);
        STAFF_MAPPED(stemStartFromStaff, extendedFlagsSlot);
        STAFF_MAPPED(stemsFixedEnd, extendedFlagsSlot);
        STAFF_MAPPED(useTabLetters, extendedFlagsSlot);
        STAFF_MAPPED(hideBeams, extendedFlagsSlot);
        STAFF_MAPPED(breakTabLinesAtNotes, extendedFlagsSlot);
        STAFF_MAPPED(stemsFixedStart, extendedFlagsSlot);
        STAFF_MAPPED(hideTuplets, extendedFlagsSlot);
        if (!legacySemantics.usesBooleanHideMode) {
            STAFF_MAPPED(hideMode, extendedFlagsSlot);
        }
    }
    if (wordCount > fretInstrumentSlot) {
        STAFF_MAPPED(fretInstId, fretInstrumentSlot);
    }
    const auto reportLong = [&](const char* member, std::size_t slot) {
        if (wordCount >= slot + 2) {
            reportStaffField(reporting, key, source, rows, member, slot, payloadLong(payload, slot * 2, byteOrder, longOrder));
        }
    };
    reportLong("horzStemOffUp", horizontalStemUpSlot);
    reportLong("horzStemOffDown", horizontalStemDownSlot);
    reportLong("vertStemStartOffUp", verticalStemStartUpSlot);
    reportLong("vertStemStartOffDown", verticalStemStartDownSlot);
    reportLong("vertStemEndOffUp", verticalStemEndUpSlot);
    reportLong("vertStemEndOffDown", verticalStemEndDownSlot);
    if (hasSecondAlternateNotationFlags(payload)) {
        STAFF_MAPPED(altHideOtherLyrics, alternateFlags2Slot);
        STAFF_MAPPED(altHideOtherSmartShapes, alternateFlags2Slot);
        STAFF_MAPPED(altHideOtherExpressions, alternateFlags2Slot);
    } else if (legacySemantics.usesAggregateAlternateNotationItems) {
        STAFF_MAPPED(altHideOtherLyrics, altFlagsSlot);
        STAFF_MAPPED(altHideOtherSmartShapes, altFlagsSlot);
        STAFF_MAPPED(altHideOtherExpressions, altFlagsSlot);
    }
    if (hasFinale2012StaffLayout(payload)) {
        STAFF_MAPPED(autoNumbering, autoNumberingSlot);
        STAFF_MAPPED(useAutoNumbering, autoNumberingSlot);
        reportStaffField(reporting, key, source, rows, "instUuid", instrumentUuidSlot, instrumentUuidBytes);
    } else {
        reporting.report().setField(key, "autoNumbering", {Reporting::Origin::LegacyBehavior, 0, 0, static_cast<std::int64_t>(target.autoNumbering)});
        reporting.report().setField(key, "useAutoNumbering", {Reporting::Origin::LegacyBehavior, 0, 0, target.useAutoNumbering});
    }
#undef STAFF_MAPPED
}

template <typename Reporting, typename Target>
void reportRemainingStaffFields(Reporting& reporting, const Target& target)
{
    const auto key = reporting.template instanceKey<Target>(target.getSourcePartId(), target.getCmper());
#define STAFF_UNMAPPED(member) reporting.unmappedField(key, #member, static_cast<std::int64_t>(target.member))
    STAFF_UNMAPPED(useNoteShapes);
    STAFF_UNMAPPED(useNoteFont);
    STAFF_UNMAPPED(notationStyle);
    STAFF_UNMAPPED(defaultClef);
    STAFF_UNMAPPED(transposedClef);
    STAFF_UNMAPPED(lineSpace);
    STAFF_UNMAPPED(capoPos);
    STAFF_UNMAPPED(lowestFret);
    STAFF_UNMAPPED(floatKeys);
    STAFF_UNMAPPED(floatTime);
    STAFF_UNMAPPED(blineBreak);
    STAFF_UNMAPPED(rbarBreak);
    STAFF_UNMAPPED(hasStyles);
    STAFF_UNMAPPED(showNameInParts);
    STAFF_UNMAPPED(showNoteColors);
    STAFF_UNMAPPED(hideNameInScore);
    STAFF_UNMAPPED(botBarlineOffset);
    STAFF_UNMAPPED(altNotation);
    STAFF_UNMAPPED(altLayer);
    STAFF_UNMAPPED(altHideArtics);
    STAFF_UNMAPPED(altHideLyrics);
    STAFF_UNMAPPED(altHideSmartShapes);
    STAFF_UNMAPPED(altRhythmStemsUp);
    STAFF_UNMAPPED(altSlashDots);
    STAFF_UNMAPPED(altHideOtherNotes);
    STAFF_UNMAPPED(altHideOtherArtics);
    STAFF_UNMAPPED(altHideExpressions);
    STAFF_UNMAPPED(altHideOtherLyrics);
    STAFF_UNMAPPED(altHideOtherSmartShapes);
    STAFF_UNMAPPED(altHideOtherExpressions);
    STAFF_UNMAPPED(hideRepeatBottomDot);
    STAFF_UNMAPPED(flatBeams);
    STAFF_UNMAPPED(hideFretboards);
    STAFF_UNMAPPED(blankMeasure);
    STAFF_UNMAPPED(hideRepeatTopDot);
    STAFF_UNMAPPED(hideLyrics);
    STAFF_UNMAPPED(noOptimize);
    STAFF_UNMAPPED(topBarlineOffset);
    STAFF_UNMAPPED(hideMeasNums);
    STAFF_UNMAPPED(hideRepeats);
    STAFF_UNMAPPED(hideBarlines);
    STAFF_UNMAPPED(hideRptBars);
    STAFF_UNMAPPED(hideKeySigs);
    STAFF_UNMAPPED(hideTimeSigs);
    STAFF_UNMAPPED(hideClefs);
    STAFF_UNMAPPED(hideStaffLines);
    STAFF_UNMAPPED(hideChords);
    STAFF_UNMAPPED(noKey);
    STAFF_UNMAPPED(dwRestOffset);
    STAFF_UNMAPPED(wRestOffset);
    STAFF_UNMAPPED(hRestOffset);
    STAFF_UNMAPPED(otherRestOffset);
    STAFF_UNMAPPED(hideRests);
    STAFF_UNMAPPED(hideTies);
    STAFF_UNMAPPED(hideDots);
    STAFF_UNMAPPED(stemReversal);
    STAFF_UNMAPPED(fullNameTextId);
    STAFF_UNMAPPED(abbrvNameTextId);
    STAFF_UNMAPPED(botRepeatDotOff);
    STAFF_UNMAPPED(topRepeatDotOff);
    STAFF_UNMAPPED(vertTabNumOff);
    STAFF_UNMAPPED(showTabClefAllSys);
    STAFF_UNMAPPED(useTabLetters);
    STAFF_UNMAPPED(breakTabLinesAtNotes);
    STAFF_UNMAPPED(hideTuplets);
    STAFF_UNMAPPED(fretInstId);
    STAFF_UNMAPPED(hideStems);
    STAFF_UNMAPPED(stemDirection);
    STAFF_UNMAPPED(hideBeams);
    STAFF_UNMAPPED(stemStartFromStaff);
    STAFF_UNMAPPED(stemsFixedEnd);
    STAFF_UNMAPPED(stemsFixedStart);
    STAFF_UNMAPPED(horzStemOffUp);
    STAFF_UNMAPPED(horzStemOffDown);
    STAFF_UNMAPPED(vertStemStartOffUp);
    STAFF_UNMAPPED(vertStemStartOffDown);
    STAFF_UNMAPPED(vertStemEndOffUp);
    STAFF_UNMAPPED(vertStemEndOffDown);
    STAFF_UNMAPPED(hideMode);
#undef STAFF_UNMAPPED
    reporting.report().setField(key, "redisplayLayerAccis", {Reporting::Origin::LegacyBehavior, 0, 0, target.redisplayLayerAccis});
    reporting.report().setField(key, "hideTimeSigsInParts", {Reporting::Origin::LegacyBehavior, 0, 0, target.hideTimeSigsInParts});
    reporting.report().setField(key, "hideKeySigsShowAccis", {Reporting::Origin::LegacyBehavior, 0, 0, target.hideKeySigsShowAccis});
    reporting.unmappedField(key, "staffLines", target.staffLines.value_or(0));
    reporting.unmappedField(key, "customStaff", target.customStaff ? target.customStaff->size() : 0);
    reporting.unmappedField(key, "instUuid", 0);
    const auto font = target.noteFont;
    reporting.unmappedField(key, "noteFont.fontId", font ? font->fontId : 0);
    reporting.unmappedField(key, "noteFont.fontSize", font ? font->fontSize : 0);
    reporting.unmappedField(key, "noteFont.bold", font ? font->bold : 0);
    reporting.unmappedField(key, "noteFont.italic", font ? font->italic : 0);
    reporting.unmappedField(key, "noteFont.underline", font ? font->underline : 0);
    reporting.unmappedField(key, "noteFont.strikeout", font ? font->strikeout : 0);
    reporting.unmappedField(key, "noteFont.absolute", font ? font->absolute : 0);
    reporting.unmappedField(key, "noteFont.hidden", font ? font->hidden : 0);
    const auto transposition = target.transposition;
    reporting.unmappedField(key, "transposition.setToClef", transposition ? transposition->setToClef : 0);
    reporting.unmappedField(key, "transposition.noSimplifyKey", transposition ? transposition->noSimplifyKey : 0);
    reporting.unmappedField(key, "transposition.keysig.interval", transposition && transposition->keysig ? transposition->keysig->interval : 0);
    reporting.unmappedField(key, "transposition.keysig.adjust", transposition && transposition->keysig ? transposition->keysig->adjust : 0);
    reporting.unmappedField(
        key, "transposition.chromatic.alteration", transposition && transposition->chromatic ? transposition->chromatic->alteration : 0);
    reporting.unmappedField(
        key, "transposition.chromatic.diatonic", transposition && transposition->chromatic ? transposition->chromatic->diatonic : 0);
}

template <typename Reporting, typename Target>
void reportStaffFallbacks(Reporting& reporting, const Target& target, const StaffTarget& defaults, StaffFallbackSelection selected)
{
    const auto key = reporting.template instanceKey<Target>(target.getSourcePartId(), target.getCmper());
    const auto reportDefault = [&](const char* member, std::int64_t value) {
        reporting.report().setField(key, member, {Reporting::Origin::Finale27Default, 0, 0, value});
    };
    if (selected.lineSpace) {
        reporting.report().setField(key, "lineSpace", {Reporting::Origin::Finale27Default, 0, 0, target.lineSpace});
    }
    if (selected.restOffsets) {
        reporting.report().setField(key, "dwRestOffset", {Reporting::Origin::Finale27Default, 0, 0, target.dwRestOffset});
        reporting.report().setField(key, "wRestOffset", {Reporting::Origin::Finale27Default, 0, 0, target.wRestOffset});
        reporting.report().setField(key, "hRestOffset", {Reporting::Origin::Finale27Default, 0, 0, target.hRestOffset});
        reporting.report().setField(key, "otherRestOffset", {Reporting::Origin::Finale27Default, 0, 0, target.otherRestOffset});
    }
    if (selected.stemReversal) {
        reporting.report().setField(key, "stemReversal", {Reporting::Origin::Finale27Default, 0, 0, target.stemReversal});
    }
    if (selected.hideMode) {
        reporting.report().setField(key, "hideMode", {Reporting::Origin::Finale27Default, 0, 0, static_cast<std::int64_t>(target.hideMode)});
    }
    if (selected.hideStaffLines) {
        reportDefault("hideStaffLines", target.hideStaffLines);
    }
    if (selected.fretInstId) {
        reporting.report().setField(key, "fretInstId", {Reporting::Origin::Finale27Default, 0, 0, target.fretInstId});
    }
    if (selected.sixWordStaffFields) {
        reportDefault("capoPos", target.capoPos);
        reportDefault("lowestFret", target.lowestFret);
        if (!target.transposition || !target.transposition->setToClef) {
            reportDefault("transposedClef", target.transposedClef);
        }
        for (const auto& field : staffSixWordFinale27DefaultFields) {
            reportDefault(field.name, target.*field.member);
        }
    }
    if (selected.extendedFlagFields) {
        for (const auto& field : staffNoExtendedFlagFinale27DefaultFields) {
            reportDefault(field.name, target.*field.member);
        }
        reportDefault("stemDirection", static_cast<std::int64_t>(target.stemDirection));
        if (target.notationStyle == StaffTarget::NotationStyle::Tablature) {
            const auto reportBehaviorOrDefault = [&](const char* member, bool value, bool defaultValue) {
                const auto origin = value == defaultValue ? Reporting::Origin::Finale27Default : Reporting::Origin::LegacyBehavior;
                reporting.report().setField(key, member, {origin, 0, 0, value});
            };
            for (const auto& field : staffLegacyTabBooleanDefaults) {
                reportBehaviorOrDefault(field.name, target.*field.member, defaults.*field.member);
            }
        } else {
            for (const auto& field : staffLegacyTabBooleanDefaults) {
                reportDefault(field.name, target.*field.member);
            }
        }
        reportDefault("breakTabLinesAtNotes", target.breakTabLinesAtNotes);
    }
    if (selected.stemOffsetFields) {
        for (const auto& field : staffNoStemOffsetFinale27DefaultFields) {
            if (selected.wordCount < field.slot + 2) {
                reporting.report().setField(key, field.name, {Reporting::Origin::Finale27Default, 0, 0, target.*field.member});
            }
        }
    }
    if (selected.repeatDotOffsets != StaffRepeatDotFallback::None) {
        const auto origin = selected.repeatDotOffsets == StaffRepeatDotFallback::Finale27Default ? Reporting::Origin::Finale27Default
                                                                                                 : Reporting::Origin::LegacyBehavior;
        reporting.report().setField(key, "botRepeatDotOff", {origin, 0, 0, target.botRepeatDotOff});
        reporting.report().setField(key, "topRepeatDotOff", {origin, 0, 0, target.topRepeatDotOff});
    }
}

void decodeStaffBase(const std::shared_ptr<StaffTarget>& targetPtr, std::span<const std::uint8_t> payload, const SourceProfile& profile,
    StaffLegacySemantics& legacySemantics, musx::factory::ConstructionContext& construction)
{
    auto& target = *targetPtr;
    const auto byteOrder = profile.byteOrder;
    const auto word = [&](std::size_t slot) { return payloadWord(payload, slot * 2, byteOrder); };
    const auto signedWord = [&](std::size_t slot) { return static_cast<std::int16_t>(word(slot)); };
    const auto wordCount = payload.size() / 2;
    target.botBarlineOffset = signedWord(botBarlineSlot);
    const auto tablaturePositions = word(tablaturePositionsSlot);
    const auto alt = word(altFlagsSlot);
    if (legacySemantics.hasAlternateNotationProperties) {
        const auto altNotation = alt & altNotationMask;
        if (altNotation <= static_cast<std::uint16_t>(StaffTarget::AlternateNotation::Blank)) {
            target.altNotation = static_cast<StaffTarget::AlternateNotation>(altNotation);
        }
        target.altLayer = (alt & altLayerMask) >> 4U;
        target.altRhythmStemsUp = alt & altRhythmStemsUpMask;
        target.altSlashDots = alt & altSlashDotsMask;
        if (legacySemantics.usesAggregateAlternateNotationItems) {
            legacySemantics.hideNoteAttachedItems = !(alt & altShowArticulationsMask);
            const auto hideOtherAttachedItems = !(alt & altShowSmartShapesMask);
            target.altHideArtics = legacySemantics.hideNoteAttachedItems;
            target.altHideLyrics = legacySemantics.hideNoteAttachedItems;
            target.altHideOtherNotes = !(alt & altShowLyricsMask);
            target.altHideOtherArtics = hideOtherAttachedItems;
            target.altHideOtherLyrics = hideOtherAttachedItems;
            target.altHideOtherSmartShapes = hideOtherAttachedItems;
            target.altHideOtherExpressions = hideOtherAttachedItems;
        } else {
            target.altHideArtics = !(alt & altShowArticulationsMask);
            target.altHideLyrics = !(alt & altShowLyricsMask);
            target.altHideSmartShapes = !(alt & altShowSmartShapesMask);
            target.altHideOtherNotes = !(alt & altShowOtherNotesMask);
            target.altHideOtherArtics = !(alt & altShowOtherArticulationsMask);
            target.altHideExpressions = !(alt & altShowExpressionsMask);
        }
        if (legacySemantics.synthesizesAltHideSmartShapes) {
            target.altHideSmartShapes = legacySemantics.hideNoteAttachedItems || target.calcAlternateNotationHidesEntries(target.altLayer);
        }
    }

    target.noteFont = std::make_shared<musx::dom::FontInfo>(target.getDocument());
    target.noteFont->fontId = construction.assignFontId(word(noteFontIdSlot));
    const auto sizeEffects = word(noteFontSizeEffectsSlot);
    target.noteFont->fontSize = sizeEffects >> 8U;
    target.noteFont->setEnigmaStyles(sizeEffects & 0xffU);

    const auto primary = word(primaryFlagsSlot);
    const auto notation = decodeStaffNotation(primary, !hasFinale2012StaffLayout(payload));
    target.notationStyle = notation.notationStyle;
    target.useNoteShapes = notation.useNoteShapes;
    legacySemantics.noteShapeNotation = notation.noteShapeNotation;
    target.hideRepeatBottomDot = primary & hideRepeatBottomDotMask;
    target.flatBeams = primary & flatBeamsMask;
    target.hideFretboards = legacySemantics.hideNoteAttachedItems || (primary & hideFretboardsMask);
    target.blankMeasure = primary & blankMeasureMask;
    target.hideRepeatTopDot = primary & hideRepeatTopDotMask;
    target.useNoteFont = primary & useNoteFontMask;
    target.hideLyrics = primary & hideLyricsMask;
    target.showNameInParts = primary & showNameInPartsMask;
    target.showNoteColors = primary & showNoteColorsMask;
    target.noOptimize = primary & noOptimizeMask;
    const auto clefs = word(clefsSlot);
    target.defaultClef = clefs & 0xffU;
    target.transposedClef = clefs >> 8U;
    const auto topLines = word(topStaffLinesSlot);
    const auto bottomLines = word(bottomStaffLinesSlot);
    if (topLines & 1U) {
        target.staffLines.reset();
        target.customStaff = customStaffLines(topLines, bottomLines);
    } else {
        target.staffLines = bottomLines;
        target.customStaff.reset();
    }
    const auto oneLineStaff = target.staffLines == 1 || (target.customStaff && target.customStaff->size() == 1);
    const auto legacyTabPositions = wordCount == staffBaseWords && target.notationStyle == StaffTarget::NotationStyle::Tablature;
    if (legacyTabPositions) {
        legacySemantics.singleStringTabPitch = static_cast<std::uint8_t>(tablaturePositions & 0xffU);
        target.vertTabNumOff = static_cast<std::int16_t>(tablaturePositions & 0xff00U);
        target.capoPos = 0;
        target.lowestFret = 0;
        applyStaffBooleanLegacyBehavior(target, staffLegacyTabBooleanDefaults);
        if (profile.epoch == FormatEpoch::UncompressedLegacy && oneLineStaff && !legacySemantics.hasAlternateNotationProperties) {
            target.staffLines.reset();
            target.customStaff = std::vector<int>{11};
            target.hideRepeatBottomDot = true;
            target.hideRepeatTopDot = true;
            legacySemantics.legacySingleStringTabForm = true;
        }
    } else {
        target.capoPos = tablaturePositions & 0xffU;
        target.lowestFret = tablaturePositions >> 8U;
    }
    target.topBarlineOffset = signedWord(topBarlineSlot);

    decodeStaffTransposition(targetPtr, word(transpositionSlot));

    const auto display = word(displayFlagsSlot);
    target.floatKeys = display & floatKeysMask;
    target.floatTime = display & floatTimeMask;
    target.blineBreak = display & breakBarlinesMask;
    target.rbarBreak = display & breakRepeatBarlinesMask;
    target.hideMeasNums = display & hideMeasureNumbersMask;
    target.hideRepeats = display & hideRepeatsMask;
    target.hideNameInScore = display & hideNameMask;
    target.hideBarlines = display & hideBarlinesMask;
    target.hideRptBars = display & hideRepeatBarsMask;
    target.hideKeySigs = display & hideKeySignaturesMask;
    target.hideTimeSigs = display & hideTimeSignaturesMask;
    target.hideTimeSigsInParts = target.hideTimeSigs;
    target.hideClefs = display & hideClefsMask;
    if (legacySemantics.usesBooleanHideMode) {
        target.hideMode = display & hideStaffOrLinesMask ? StaffTarget::HideMode::Cutaway : StaffTarget::HideMode::None;
    }
    if (hasFinale2012StaffLayout(payload)) {
        target.hideStaffLines = display & hideStaffOrLinesMask;
    }
    target.hideChords = legacySemantics.hideNoteAttachedItems || (display & hideChordsMask);
    target.noKey = display & noKeyMask;

    const auto doubleWholeWhole = word(doubleWholeWholeRestSlot);
    target.dwRestOffset = signedByte(doubleWholeWhole, false);
    target.wRestOffset = signedByte(doubleWholeWhole, true);
    const auto halfOther = word(halfOtherRestSlot);
    target.hRestOffset = signedByte(halfOther, false);
    target.otherRestOffset = signedByte(halfOther, true);
    target.stemReversal = signedWord(stemReversalSlot);
    target.fullNameTextId = word(fullNameSlot);
    target.abbrvNameTextId = word(abbreviatedNameSlot);

    if (wordCount > staffBaseWords) {
        const auto repeatDots = word(repeatDotOffsetsSlot);
        target.botRepeatDotOff = signedByte(repeatDots, false);
        target.topRepeatDotOff = signedByte(repeatDots, true);
    }
    const auto longOrder = nativeLongWordOrder(byteOrder);
    if (wordCount >= lineSpaceSlot + 2) {
        target.lineSpace = payloadLong(payload, lineSpaceSlot * 2, byteOrder, longOrder) / 64;
    }
    if (wordCount >= verticalTabOffsetSlot + 2) {
        target.vertTabNumOff = payloadLong(payload, verticalTabOffsetSlot * 2, byteOrder, longOrder);
    }
    if (wordCount > extendedFlagsSlot) {
        const auto extended = word(extendedFlagsSlot);
        target.showTabClefAllSys = !(extended & clefFirstMeasureOnlyMask);
        target.hideRests = !(extended & showRestsMask);
        target.hideTies = !(extended & showTiesMask);
        target.hideDots = !(extended & showDotsMask);
        target.hideStems = !(extended & showStemsMask);
        target.stemDirection = stemDirection(extended);
        target.stemStartFromStaff = extended & stemStartFromStaffMask;
        target.stemsFixedEnd = extended & stemsFixedEndMask;
        target.useTabLetters = extended & useTabLettersMask;
        target.hideBeams = !(extended & showBeamsMask);
        target.breakTabLinesAtNotes = extended & breakTabLinesMask;
        target.stemsFixedStart = extended & stemsFixedStartMask;
        target.hideTuplets = !(extended & showTupletsMask);
        if (!legacySemantics.usesBooleanHideMode) {
            target.hideMode = hideMode(extended);
        }
    }
    if (wordCount > fretInstrumentSlot) {
        target.fretInstId = word(fretInstrumentSlot);
    }
    const auto decodeLong = [&](std::size_t slot) { return payloadLong(payload, slot * 2, byteOrder, longOrder); };
    if (wordCount >= horizontalStemUpSlot + 2) {
        target.horzStemOffUp = decodeLong(horizontalStemUpSlot);
    }
    if (wordCount >= horizontalStemDownSlot + 2) {
        target.horzStemOffDown = decodeLong(horizontalStemDownSlot);
    }
    if (wordCount >= verticalStemStartUpSlot + 2) {
        target.vertStemStartOffUp = decodeLong(verticalStemStartUpSlot);
    }
    if (wordCount >= verticalStemStartDownSlot + 2) {
        target.vertStemStartOffDown = decodeLong(verticalStemStartDownSlot);
    }
    if (wordCount >= verticalStemEndUpSlot + 2) {
        target.vertStemEndOffUp = decodeLong(verticalStemEndUpSlot);
    }
    if (wordCount >= verticalStemEndDownSlot + 2) {
        target.vertStemEndOffDown = decodeLong(verticalStemEndDownSlot);
    }
    if (hasSecondAlternateNotationFlags(payload)) {
        const auto alt2 = word(alternateFlags2Slot);
        target.altHideOtherLyrics = !(alt2 & altShowOtherLyricsMask);
        target.altHideOtherSmartShapes = !(alt2 & altShowOtherSmartShapesMask);
        target.altHideOtherExpressions = !(alt2 & altShowOtherExpressionsMask);
    }
    if (hasFinale2012StaffLayout(payload)) {
        const auto autoNumbering = word(autoNumberingSlot);
        const auto autoNumberingStyle = autoNumbering & autoNumberingStyleMask;
        if (autoNumberingStyle <= static_cast<std::uint16_t>(StaffTarget::AutoNumberingStyle::ArabicPrefix)) {
            target.autoNumbering = static_cast<StaffTarget::AutoNumberingStyle>(autoNumberingStyle);
        }
        target.useAutoNumbering = autoNumbering & autoNumberingEnabledMask;
        target.instUuid = instrumentUuid(payload);
    }
}

} // namespace

template <typename Target>
void importStaffFamily(const ImportContext& context, records::LegacyTag fixedTag, records::LegacyTag classId)
{
    const auto selected = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), fixedTag, classId);
    if (!selected) {
        return;
    }
    const auto& source = *selected;
    std::vector<std::function<void()>> synthesized;
    for (const auto& [partId, staffId] : recordKeys(source)) {
        const auto rows = source.pool->getArray(source.identity, staffId, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto recordPayload = collectRecordPayload(source, rows);
        auto staffPayloadBytes = recordPayload.size();
        std::optional<StaffStyleTrailerLayout> staffStyleLayout;
        if constexpr (std::is_same_v<Target, StaffStyleTarget>) {
            staffStyleLayout = staffStyleTrailerLayout(recordPayload.size(), context.profile.version);
            if (!staffStyleLayout || recordPayload.size() < staffBaseBytes + staffStyleLayout->trailerBytes) {
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Info, "StaffStyle " + std::to_string(staffId) + " is shorter than its base layout and trailer."});
                continue;
            }
            staffPayloadBytes -= staffStyleLayout->trailerBytes;
        }
        const auto payload = std::span(recordPayload).first(staffPayloadBytes);
        auto target = createOthersRecordTarget<Target>(context.document, source, rows.front(), staffId);
        if (!target) {
            continue;
        }

        if constexpr (std::is_same_v<Target, StaffStyleTarget>) {
            decodeStaffStyleTrailer(context, target, recordPayload, staffPayloadBytes, *staffStyleLayout);
        }
        bool adjustedNoteShapeMasks = false;

        target->autoNumbering = static_cast<StaffTarget::AutoNumberingStyle>(0);
        target->useAutoNumbering = false;
        target->instUuid = std::string(musx::dom::uuid::Unknown);
        const auto hasAlternateNotationProperties = sourceHasStaffAlternateNotationProperties(context.profile);
        const auto sixWordLayout = usesSixWordStaffLayout(context.profile, source, payload);
        StaffLegacySemantics legacySemantics{.hasAlternateNotationProperties = hasAlternateNotationProperties,
            .usesAggregateAlternateNotationItems = hasAlternateNotationProperties && !hasSecondAlternateNotationFlags(payload),
            // Believed: Finale 2000-2008 derives this property from the notation
            // type and aggregate note-attached-items control; Finale 2009 stores it
            // independently.
            .synthesizesAltHideSmartShapes =
                hasAlternateNotationProperties && sourcePredatesVersion(context.profile, FormatEpoch::ZlibLegacy, versions::finale2009),
            .usesBooleanHideMode = !sixWordLayout && sourcePredatesVersion(context.profile, FormatEpoch::ZlibLegacy, versions::finale2011)};
        if (!legacySemantics.hasAlternateNotationProperties) {
            applyStaffAlternateNotationLegacyBehavior(*target);
        }
        const auto hasStoredInstrumentUuid = hasFinale2012StaffLayout(payload);
        auto selectedDefaults = selectStaffFallbacks(sixWordLayout, payload);

        if (sixWordLayout) {
            if (payload.size() < staffCodaWords * 2) {
                continue;
            }
            const auto codaWord = [&](std::size_t slot) { return payloadWord(payload, slot * 2, context.profile.byteOrder); };
            decodeStaffTransposition(target, codaWord(codaTranspositionSlot), true);
            const auto display = codaWord(codaDisplayFlagsSlot);
            // The uncompressed six-word layout moves the default clef to word zero;
            // Coda-banner Staffs retain it in the display word.
            const auto defaultClefSlot = context.profile.epoch == FormatEpoch::CodaBanner ? codaDisplayFlagsSlot : codaDefaultClefSlot;
            target->defaultClef = codaWord(defaultClefSlot) & codaDefaultClefMask;
            target->floatKeys = display & floatKeysMask;
            target->floatTime = display & floatTimeMask;
            target->blineBreak = display & breakBarlinesMask;
            target->rbarBreak = display & breakRepeatBarlinesMask;
            target->hideMeasNums = display & hideMeasureNumbersMask;
            target->hideRepeats = display & hideRepeatsMask;
            target->hideNameInScore = display & hideNameMask;
            target->hideKeySigs = display & hideKeySignaturesMask;
            target->hideTimeSigs = display & hideTimeSignaturesMask;
            target->hideClefs = display & hideClefsMask;
            const auto combinesKeyControls = context.profile.epoch == FormatEpoch::CodaBanner;
            if (combinesKeyControls) {
                target->noKey = target->hideKeySigs;
            } else {
                target->noKey = finale27StaffDefaults(context).noKey;
            }
            target->hideTimeSigsInParts = target->hideTimeSigs;

            const RecordFamilySource codaAttributesSource{&context.index.getOthers(), codaStaffAttributesTag, false, false};
            const auto codaAttributesRows = context.index.getOthers().getArray(codaStaffAttributesTag, staffId, 0, partId);
            const auto codaAttributes = collectRecordPayload(codaAttributesSource, codaAttributesRows);
            const auto hasCodaAttributes = codaAttributes.size() >= staffCodaWords * 2;
            bool hasCodaStaffLineOverride = false;
            std::optional<std::int16_t> codaStaffLineValue;
            if (hasCodaAttributes) {
                const auto attributeWord = [&](std::size_t slot) { return payloadWord(codaAttributes, slot * 2, context.profile.byteOrder); };
                target->noteFont = std::make_shared<musx::dom::FontInfo>(target->getDocument());
                const auto storedNoteFontId = attributeWord(codaNoteFontIdSlot);
                target->noteFont->fontId = context.construction.assignFontId(storedNoteFontId);
                const auto sizeEffects = attributeWord(codaNoteFontSizeEffectsSlot);
                target->noteFont->fontSize = sizeEffects >> 8U;
                target->noteFont->setEnigmaStyles(sizeEffects & 0xffU);
                const auto primary = attributeWord(codaPrimaryFlagsSlot);
                const auto notation = decodeStaffNotation(primary, true);
                target->notationStyle = notation.notationStyle;
                target->useNoteShapes = notation.useNoteShapes;
                legacySemantics.noteShapeNotation = notation.noteShapeNotation;
                hasCodaStaffLineOverride = primary & codaStaffLineOverrideMask;
                codaStaffLineValue = static_cast<std::int16_t>(attributeWord(codaStaffLineValueSlot));
                if (hasCodaStaffLineOverride) {
                    if (*codaStaffLineValue > 0) {
                        selectedDefaults.repeatDotOffsets = StaffRepeatDotFallback::Finale27Default;
                    }
                    if (*codaStaffLineValue == codaCenteredOneLineStaff) {
                        target->staffLines.reset();
                        target->customStaff = std::vector<int>{13};
                    } else if (*codaStaffLineValue < 0) {
                        target->staffLines.reset();
                        target->customStaff = std::vector<int>{10 - *codaStaffLineValue};
                        target->botBarlineOffset = 0;
                        target->topBarlineOffset = (-*codaStaffLineValue - 1) * evpusPerSpace;
                    } else {
                        target->staffLines = *codaStaffLineValue;
                        target->customStaff.reset();
                    }
                } else {
                    target->staffLines = 5;
                    target->customStaff.reset();
                }
                // The low byte is the legacy Base Key control, which has no modern
                // Staff member; the signed high byte is the tablature-number offset.
                target->vertTabNumOff = static_cast<std::int16_t>(attributeWord(codaVerticalTabOffsetSlot) & 0xff00U);
                target->useNoteFont = primary & useNoteFontMask;
                if (target->notationStyle == StaffTarget::NotationStyle::Tablature) {
                    // Coda tablature always uses the Staff-local tablature font tuple.
                    target->useNoteFont = true;
                }
                target->blankMeasure = primary & blankMeasureMask;
                if (target->notationStyle == StaffTarget::NotationStyle::Tablature) {
                    applyStaffBooleanLegacyBehavior(*target, staffLegacyTabBooleanDefaults);
                }
                if (target->notationStyle == StaffTarget::NotationStyle::Tablature) {
                    // Coda tablature stores a MIDI base key rather than a modern
                    // fret-instrument reference. The referent is synthesized after all
                    // stored fret instruments exist.
                    legacySemantics.singleStringTabPitch = static_cast<std::uint8_t>(attributeWord(codaVerticalTabOffsetSlot) & 0xffU);
                    target->fretInstId = 0;
                }
            } else {
                // Without the optional attributes row, this layout has neither
                // editable staff lines nor a Staff-local notehead-font tuple.
                const auto& defaults = finale27StaffDefaults(context);
                target->staffLines = 5;
                target->customStaff = defaults.customStaff;
                target->notationStyle = defaults.notationStyle;
                target->useNoteShapes = defaults.useNoteShapes;
                target->useNoteFont = defaults.useNoteFont;
                target->blankMeasure = defaults.blankMeasure;
                target->vertTabNumOff = 0;
                target->noteFont = std::make_shared<musx::dom::FontInfo>(target->getDocument());
                target->noteFont->fontSize = finale27NoteheadFontSize(context);
            }
            const auto hasCenteredOneStaffLine = hasCodaStaffLineOverride && codaStaffLineValue == codaCenteredOneLineStaff;
            if (hasCenteredOneStaffLine) {
                target->botBarlineOffset = -2 * evpusPerSpace;
                target->topBarlineOffset = 2 * evpusPerSpace;
            }
            const auto adjustedFontSize = resolveStaffNoteFontSize(context, *target);
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<Target>(partId, staffId);
                reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
                reportStaffField(reporting, key, source, rows, "defaultClef", defaultClefSlot, target->defaultClef);
                if (target->transposition && target->transposition->setToClef) {
                    reportStaffField(reporting, key, source, rows, "transposedClef", codaTranspositionSlot, target->transposedClef);
                }
                reportStaffTransposition(reporting, *target, key, source, rows, codaTranspositionSlot);
                reportStaffField(reporting, key, source, rows, "floatKeys", codaDisplayFlagsSlot, target->floatKeys);
                reportStaffField(reporting, key, source, rows, "floatTime", codaDisplayFlagsSlot, target->floatTime);
                reportStaffField(reporting, key, source, rows, "blineBreak", codaDisplayFlagsSlot, target->blineBreak);
                reportStaffField(reporting, key, source, rows, "rbarBreak", codaDisplayFlagsSlot, target->rbarBreak);
                reportStaffField(reporting, key, source, rows, "hideMeasNums", codaDisplayFlagsSlot, target->hideMeasNums);
                reportStaffField(reporting, key, source, rows, "hideRepeats", codaDisplayFlagsSlot, target->hideRepeats);
                reportStaffField(reporting, key, source, rows, "hideNameInScore", codaDisplayFlagsSlot, target->hideNameInScore);
                reportStaffField(reporting, key, source, rows, "hideKeySigs", codaDisplayFlagsSlot, target->hideKeySigs);
                reportStaffField(reporting, key, source, rows, "hideTimeSigs", codaDisplayFlagsSlot, target->hideTimeSigs);
                reportStaffField(reporting, key, source, rows, "hideClefs", codaDisplayFlagsSlot, target->hideClefs);
                if (combinesKeyControls) {
                    const auto& codaStaffRow = rows.front();
                    reporting.report().setField(key, "noKey",
                        {Reporting::Origin::LegacyMusAdjusted, codaStaffRow.blockOffset, codaStaffRow.decodedOffset + codaDisplayFlagsSlot * 2,
                            target->noKey, source.identity});
                } else {
                    reporting.report().setField(key, "noKey", {Reporting::Origin::Finale27Default, 0, 0, target->noKey});
                }
                if (hasCodaStaffLineOverride) {
                    reportStaffField(reporting, key, codaAttributesSource, codaAttributesRows, "staffLines", codaStaffLineValueSlot,
                        target->staffLines.value_or(0));
                    reportStaffField(reporting, key, codaAttributesSource, codaAttributesRows, "customStaff", codaStaffLineValueSlot,
                        target->customStaff ? target->customStaff->size() : 0);
                } else {
                    reporting.report().setField(key, "staffLines", {Reporting::Origin::LegacyBehavior, 0, 0, 5});
                }
                if (hasCenteredOneStaffLine) {
                    reporting.report().setField(key, "botBarlineOffset", {Reporting::Origin::LegacyBehavior, 0, 0, target->botBarlineOffset});
                    reporting.report().setField(key, "topBarlineOffset", {Reporting::Origin::LegacyBehavior, 0, 0, target->topBarlineOffset});
                }
                if (hasCodaStaffLineOverride && *codaStaffLineValue < 0) {
                    const auto& row = codaAttributesRows.front();
                    const auto decodedOffset = row.decodedOffset + codaStaffLineValueSlot * 2;
                    const auto adjusted = [&](const char* member, std::int64_t value) {
                        reporting.report().setField(
                            key, member, {Reporting::Origin::LegacyMusAdjusted, row.blockOffset, decodedOffset, value, codaStaffAttributesTag});
                    };
                    adjusted("botBarlineOffset", target->botBarlineOffset);
                    adjusted("topBarlineOffset", target->topBarlineOffset);
                }
                if (hasCodaAttributes) {
                    const auto notationOrigin =
                        legacySemantics.noteShapeNotation ? Reporting::Origin::LegacyMusAdjusted : Reporting::Origin::LegacyMus;
                    reportStaffField(reporting, key, codaAttributesSource, codaAttributesRows, "notationStyle", codaPrimaryFlagsSlot,
                        target->notationStyle, notationOrigin);
                    reportStaffField(reporting, key, codaAttributesSource, codaAttributesRows, "useNoteShapes", codaPrimaryFlagsSlot,
                        target->useNoteShapes, notationOrigin);
                    const auto primary = payloadWord(codaAttributes, codaPrimaryFlagsSlot * 2, context.profile.byteOrder);
                    if (target->notationStyle == StaffTarget::NotationStyle::Tablature && !(primary & useNoteFontMask)) {
                        reporting.report().setField(key, "useNoteFont", {Reporting::Origin::LegacyBehavior, 0, 0, target->useNoteFont});
                    } else {
                        reportStaffField(
                            reporting, key, codaAttributesSource, codaAttributesRows, "useNoteFont", codaPrimaryFlagsSlot, target->useNoteFont);
                    }
                    reportStaffField(
                        reporting, key, codaAttributesSource, codaAttributesRows, "blankMeasure", codaPrimaryFlagsSlot, target->blankMeasure);
                    reportStaffField(
                        reporting, key, codaAttributesSource, codaAttributesRows, "vertTabNumOff", codaVerticalTabOffsetSlot, target->vertTabNumOff);
                    reportStaffFont(reporting, *target, key, codaAttributesSource, codaAttributesRows, codaNoteFontIdSlot,
                        codaNoteFontSizeEffectsSlot, adjustedFontSize);
                } else {
                    const auto reportDefault = [&](const char* member, std::int64_t value) {
                        reporting.report().setField(key, member, {Reporting::Origin::Finale27Default, 0, 0, value});
                    };
                    reportDefault("customStaff", 0);
                    reportDefault("notationStyle", static_cast<std::int64_t>(target->notationStyle));
                    reportDefault("useNoteShapes", target->useNoteShapes);
                    reportDefault("useNoteFont", target->useNoteFont);
                    reportDefault("blankMeasure", target->blankMeasure);
                    reporting.report().setField(key, "vertTabNumOff", {Reporting::Origin::LegacyBehavior, 0, 0, target->vertTabNumOff});
                    reporting.report().setField(key, "noteFont.fontSize", {Reporting::Origin::Finale27Default, 0, 0, target->noteFont->fontSize});
                }
                reporting.report().setField(
                    key, "autoNumbering", {Reporting::Origin::LegacyBehavior, 0, 0, static_cast<std::int64_t>(target->autoNumbering)});
                reporting.report().setField(key, "useAutoNumbering", {Reporting::Origin::LegacyBehavior, 0, 0, target->useAutoNumbering});
                reportStaffAlternateNotationLegacyBehavior(reporting, *target, key);
            });
            if (legacySemantics.singleStringTabPitch) {
                const auto pitch = *legacySemantics.singleStringTabPitch;
                const auto row = codaAttributesRows.front();
                const auto pitchByteOffset = codaVerticalTabOffsetSlot * 2 + (context.profile.byteOrder == ByteOrder::BigEndian);
                synthesized.push_back([&context, target, pitch, row, pitchByteOffset] {
                    synthesizeLegacySingleStringFretInstrument(context, target, pitch, row, codaStaffAttributesTag, pitchByteOffset);
                });
            }
        } else {
            if (payload.size() < staffBaseBytes) {
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Info, std::string(std::is_same_v<Target, StaffStyleTarget> ? "StaffStyle " : "Staff ")
                                                             + std::to_string(staffId) + " is shorter than its base layout."});
                continue;
            }
            decodeStaffBase(target, payload, context.profile, legacySemantics, context.construction);
            const auto adjustedFontSize = resolveStaffNoteFontSize(context, *target);
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                reportMappedStaff(reporting, *target, source, rows, payload, context.profile.byteOrder, legacySemantics, adjustedFontSize);
            });
            if (legacySemantics.singleStringTabPitch) {
                const auto pitch = *legacySemantics.singleStringTabPitch;
                const auto row = rows.front();
                const auto sourceTag = source.identity;
                const auto pitchByteOffset = tablaturePositionsSlot * 2 + (context.profile.byteOrder == ByteOrder::BigEndian);
                synthesized.push_back([&context, target, pitch, row, sourceTag, pitchByteOffset] {
                    synthesizeLegacySingleStringFretInstrument(context, target, pitch, row, sourceTag, pitchByteOffset);
                });
            }
        }
        if constexpr (std::is_same_v<Target, StaffStyleTarget>) {
            adjustedNoteShapeMasks = applyLegacyStaffStyleNoteShapeMasks(*target, legacySemantics.noteShapeNotation);
        }
        if constexpr (std::is_same_v<Target, StaffTarget>) {
            if (usesParallelStaffNames(context.profile)) {
                synthesized.push_back([&context, target] {
                    text::EnigmaFontResolutionCache fontResolutionCache;
                    synthesizeLegacyStaffName(context, *target, legacyStaffFullNameTag, musx::dom::options::FontOptions::FontType::StaffNames,
                        &StaffTarget::fullNameTextId, fontResolutionCache);
                    synthesizeLegacyStaffName(context, *target, legacyStaffAbbreviatedNameTag,
                        musx::dom::options::FontOptions::FontType::AbbrvStaffNames, &StaffTarget::abbrvNameTextId, fontResolutionCache);
                });
            }
        }
        resolveStaffRepeatDotFallback(*target, selectedDefaults);
        if (selectedDefaults.lineSpace || selectedDefaults.restOffsets || selectedDefaults.stemReversal || selectedDefaults.hideMode
            || selectedDefaults.hideStaffLines || selectedDefaults.fretInstId || selectedDefaults.sixWordStaffFields
            || selectedDefaults.extendedFlagFields || selectedDefaults.stemOffsetFields
            || selectedDefaults.repeatDotOffsets != StaffRepeatDotFallback::None) {
            applyStaffFallbacks(*target, finale27StaffDefaults(context), selectedDefaults);
        }
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            reportStaffFallbacks(reporting, *target, finale27StaffDefaults(context), selectedDefaults);
            if (!hasStoredInstrumentUuid) {
                const auto key = reporting.template instanceKey<Target>(partId, staffId);
                reporting.report().setField(key, "instUuid", {Reporting::Origin::LegacyBehavior, 0, 0, 0});
            }
            reportRemainingStaffFields(reporting, *target);
            if constexpr (std::is_same_v<Target, StaffStyleTarget>) {
                reportStaffStyleTrailer(reporting, *target, source, rows, staffPayloadBytes, adjustedNoteShapeMasks);
            }
        });
        context.document->getOthers()->add(Target::XmlNodeName, std::move(target));
    }
    context.pending.defer(DeferredStage::SynthesizeStaffParts, std::move(synthesized));
}

void importStaff(const ImportContext& context)
{
    importStaffFamily<StaffTarget>(context, staffTag, staffClass);
}

void importStaffStyles(const ImportContext& context)
{
    importStaffFamily<StaffStyleTarget>(context, staffStyleTag, staffStyleClass);
}

} // namespace others
} // namespace finale_mus_reader
