// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/details.h"
#include "import/shared/measure_records.h"

#include "musx/musx.h"
#include <array>
#include <cstdint>
#include <set>
#include <span>
#include <tuple>

namespace finale_mus_reader {
namespace details {
namespace {

using ChordTarget = musx::dom::details::ChordAssign;
constexpr records::LegacyTag chordMeasureClass = 0x0450;
enum class ChordLayout {
    CompactEntry,
    ExtendedEntry,
    Measure
};

int chordAlteration(std::uint16_t flags)
{
    const auto value = (flags >> 8U) & 15U;
    return static_cast<int>(value) - ((value & 8U) ? 16 : 0);
}

void reportChord(const ImportContext& context, const RecordFamilySource& source, std::span<const records::LegacyRow> rows,
    std::span<const std::int16_t> words, std::size_t at, const ChordTarget& target, bool compact, bool modern, bool before2001, bool relativeToEntry,
    bool invalidBassPosition, std::size_t capoSlot, std::size_t fbSlot, std::size_t styleSlot, std::size_t groupSlot)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key =
            reporting.template instanceKey<ChordTarget>(target.getSourcePartId(), target.getCmper1(), target.getInci(), target.getCmper2());
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        const auto stored = [&](const char* name, std::size_t slot) {
            reportLegacyField(reporting, key, source, source.rowOfWord(rows, at + slot), name, source.byteOffsetInRow((at + slot) * 2),
                words[at + slot], compact && slot < 2 ? Reporting::Origin::LegacyMusAdjusted : Reporting::Origin::LegacyMus);
        };
        for (const auto* name :
            {"rootScaleNum", "rootAlter", "rootLowerCase", "playSuffix", "showRoot", "playRoot", "showFretboard", "showSuffix", "playFretboard"}) {
            stored(name, 0);
        }
        for (const auto* name : {"bassScaleNum", "bassAlter", "bassLowerCase", "showAltBass"}) {
            stored(name, 1);
        }
        if (modern) {
            stored("playAltBass", 1);
        } else {
            reportLegacyField(reporting, key, source, source.rowOfWord(rows, at + 1), "playAltBass", source.byteOffsetInRow((at + 1) * 2),
                words[at + 1], Reporting::Origin::LegacyMusAdjusted);
        }
        if (invalidBassPosition) {
            reportFallbackField(reporting, key, "bassPosition", Reporting::Origin::Unmapped, static_cast<std::uint16_t>(words[at + 1]));
        } else {
            stored("bassPosition", 1);
        }
        stored("suffixId", 2);
        stored("horzOff", 3);
        stored("vertOff", 4);
        const auto behavior = [&](const char* name, std::int64_t value) {
            reportFallbackField(reporting, key, name, Reporting::Origin::LegacyBehavior, value);
        };
        if (compact || before2001) {
            behavior("capoValue", target.capoValue);
            behavior("useLocalCapo", target.useLocalCapo);
            behavior("fbStyleId", target.fbStyleId);
            behavior("fretboardGroupInci", target.fretboardGroupInci);
            behavior("useFretboardFont", target.useFretboardFont);
        } else {
            stored("capoValue", capoSlot);
            stored("useLocalCapo", capoSlot);
            stored("fbStyleId", styleSlot);
            stored("fretboardGroupInci", groupSlot);
            stored("useFretboardFont", groupSlot);
        }
        if (relativeToEntry) {
            const auto horizontalSlot = at + (compact ? 3 : fbSlot);
            const auto verticalSlot = at + (compact ? 4 : fbSlot + 1);
            reportLegacyField(reporting, key, source, source.rowOfWord(rows, horizontalSlot), "fbHorzOff", source.byteOffsetInRow(horizontalSlot * 2),
                words[horizontalSlot], Reporting::Origin::LegacyMusAdjusted);
            reportLegacyField(reporting, key, source, source.rowOfWord(rows, verticalSlot), "fbVertOff", source.byteOffsetInRow(verticalSlot * 2),
                words[verticalSlot], Reporting::Origin::LegacyMusAdjusted);
        } else {
            stored("fbHorzOff", fbSlot);
            stored("fbVertOff", fbSlot + 1);
        }
        if (compact || before2001) {
            reportLegacyField(reporting, key, source, source.rowOfWord(rows, at + 3), "horzOff", source.byteOffsetInRow((at + 3) * 2), words[at + 3],
                Reporting::Origin::LegacyMusAdjusted);
        }
        if (modern) {
            reportLegacyField(
                reporting, key, source, source.rowOfWord(rows, at + 10), "horzEdu", source.byteOffsetInRow((at + 10) * 2), target.horzEdu);
            stored("chPercent", 12);
            stored("fbPercent", 13);
        } else {
            reportFallbackField(reporting, key, "horzEdu", Reporting::Origin::LegacyMusAdjusted, target.horzEdu);
            reportFallbackField(reporting, key, "chPercent", Reporting::Origin::LegacyMusAdjusted, target.chPercent);
            behavior("fbPercent", target.fbPercent);
        }
    });
}

void decodeChord(const ImportContext& context, const RecordFamilySource& source, std::span<const records::LegacyRow> rows,
    std::span<const std::int16_t> words, std::size_t at, ChordTarget& target, ChordLayout layout = ChordLayout::Measure)
{
    auto root = static_cast<std::uint16_t>(words[at]);
    auto bass = static_cast<std::uint16_t>(words[at + 1]);
    const bool compact = layout == ChordLayout::CompactEntry;
    const bool modern = layout == ChordLayout::Measure;
    const bool before2001 = !sourceAtOrAfter(context.profile, FormatEpoch::DclLegacy);
    if (compact) {
        if (!sourceAtOrAfter(context.profile, FormatEpoch::UncompressedLegacy, VersionBound{3, 0, 0, 2, 21})) {
            root ^= 0xe0;
            if (!(root & 0x80)) {
                root &= ~0x20;
            }
        }
        if (!sourceAtOrAfter(context.profile, FormatEpoch::UncompressedLegacy, VersionBound{3, 6, 0, 1, 8})) {
            root = (root & 0x0fff) | 0x2000;
            bass &= 0x0fff;
        }
        root ^= 0x4000;
    }
    target.rootScaleNum = root & 31;
    target.rootAlter = chordAlteration(root);
    target.rootLowerCase = (root & 0x8000) != 0;
    target.playSuffix = (root & 0x80) != 0;
    target.showRoot = (root & 0x40) != 0;
    target.playRoot = (root & 0x20) != 0;
    target.showFretboard = (root & 0x4000) != 0;
    target.showSuffix = (root & 0x2000) != 0;
    target.playFretboard = (root & 0x1000) != 0;
    target.bassScaleNum = bass & 31;
    target.bassAlter = chordAlteration(bass);
    target.bassLowerCase = (bass & 0x8000) != 0;
    const bool invalidBassPosition = (bass & 0x4080) == 0x4080;
    if (invalidBassPosition) {
        context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info, "Chord assignment has an undefined bass placement."});
    }
    target.bassPosition = invalidBassPosition ? ChordTarget::BassPosition::AfterRoot
                          : (bass & 0x4000)   ? ChordTarget::BassPosition::Subtext
                          : (bass & 0x80)     ? ChordTarget::BassPosition::UnderRoot
                                              : ChordTarget::BassPosition::AfterRoot;
    target.showAltBass = (bass & 0x40) != 0;
    // Entry-attached assignments lose alternate-bass playback during conversion.
    target.playAltBass = modern && (bass & 0x20) != 0;
    target.suffixId = static_cast<std::uint16_t>(words[at + 2]);
    target.horzOff = words[at + 3];
    target.vertOff = words[at + 4];
    const std::size_t capoSlot = modern ? 5 : 9;
    const std::size_t fbSlot = modern ? 6 : 5;
    const std::size_t styleSlot = modern ? 8 : 7;
    const std::size_t groupSlot = modern ? 9 : 8;
    if (!compact) {
        target.capoValue = words[at + capoSlot] & 63;
        target.useLocalCapo = (words[at + capoSlot] & 0x40) != 0;
        target.fbHorzOff = words[at + fbSlot];
        target.fbVertOff = words[at + fbSlot + 1];
        target.fbStyleId = static_cast<std::uint16_t>(words[at + styleSlot]);
        target.fretboardGroupInci = words[at + groupSlot] & 0x7fff;
        target.useFretboardFont = (words[at + groupSlot] & 0x8000) != 0;
    } else {
        target.horzOff += 12;
        target.fbHorzOff = 12;
        target.useFretboardFont = true;
    }
    if (before2001) {
        target.horzOff += 12;
        target.fbHorzOff += 12;
        target.fbStyleId = 1;
        target.fretboardGroupInci = 0;
        target.capoValue = 0;
        target.useLocalCapo = false;
        using Fonts = musx::dom::options::FontOptions;
        const auto font = Fonts::getFontInfoOrNull(context.document, Fonts::FontType::Fretboard);
        target.useFretboardFont = !font || font->getName() != "Seville";
        if (font && !target.useFretboardFont) {
            target.fbVertOff += static_cast<int>(font->fontSize * (17.0 / 6.0) + 0.5);
        }
    }
    // Earlier fretboard coordinates are entry-relative; later coordinates are chord-relative.
    const bool relativeToEntry = !modern || sourcePredatesVersion(context.profile, FormatEpoch::ZlibLegacy, VersionBound{15, 0, 0, 2, 7});
    if (relativeToEntry) {
        target.fbHorzOff -= target.horzOff;
        target.fbVertOff -= target.vertOff;
    }
    if (modern) {
        target.chPercent = words[at + 12];
        target.fbPercent = words[at + 13];
    }
    reportChord(context, source, rows, words, at, target, compact, modern, before2001, relativeToEntry, invalidBassPosition, capoSlot, fbSlot,
        styleSlot, groupSlot);
}

bool chordFretCoordinateLinked(
    const ImportContext& context, const RecordFamilySource& source, std::span<const records::LegacyRow> rows, std::size_t wordSlot)
{
    const auto bits = partUnlinkedBits(source, rows, wordSlot, context.profile.byteOrder);
    return bits && *bits == 0;
}

// Finale's conversion keeps a part's own chord only when its show flags or chord offsets are unlinked.
bool chordPartUnlinked(const ImportContext& context, const RecordFamilySource& source, std::span<const records::LegacyRow> rows, std::size_t at)
{
    constexpr std::array<std::uint16_t, 5> comparedBits{0x6040, 0, 0, 0xffff, 0xffff};
    return partRecordUnlinked(source, rows, at, comparedBits, context.profile.byteOrder);
}

void importEntryChords(const ImportContext& context, const RecordFamilySource& source, ChordLayout layout)
{
    const std::size_t stride = layout == ChordLayout::CompactEntry ? 5 : 10;
    std::set<musx::dom::Cmper> parts;
    std::set<std::tuple<musx::dom::Cmper, std::uint16_t, std::uint16_t>> visited;
    for (const auto& [part, high] : recordKeys(source)) {
        parts.insert(part);
    }
    const auto entryRows = [&](musx::dom::EntryNumber number, musx::dom::Cmper part) {
        return source.pool->getArray(source.identity, static_cast<std::uint16_t>(number >> 16U), static_cast<std::uint16_t>(number), part);
    };
    const auto keepsPartChord = [&](std::span<const records::LegacyRow> rows) {
        const auto count = collectRecordPayload(source, rows).size() / (stride * 2);
        for (std::size_t at = 0; at < count * stride; at += stride) {
            if (chordPartUnlinked(context, source, rows, at)) {
                return true;
            }
        }
        return false;
    };
    for (const auto part : parts) {
        const bool isScore = part == musx::dom::SCORE_PARTID;
        for (const auto& frame : context.document->getDetails()->getArray<musx::dom::details::GFrameHold>(part)) {
            const auto measure = context.document->getOthers()->get<musx::dom::others::Measure>(part, frame->getMeasure());
            if (!measure) {
                continue;
            }
            bool hasChord = false;
            frame->iterateRawEntries([&](const musx::dom::MusxInstance<musx::dom::Entry>& entry, musx::dom::LayerIndex) {
                const auto rows = entryRows(entry->getEntryNumber(), part);
                hasChord = !rows.empty() && (isScore || keepsPartChord(rows));
                return !hasChord;
            });
            if (!hasChord) {
                continue;
            }
            // A part that keeps any chord of its own in a measure holds the measure's complete list,
            // with its linked chords copied from the score.
            EmittedIncidence incidence;
            musx::dom::details::GFrameHoldContext frameContext(context.document, part, frame->getStaff(), frame->getMeasure());
            frameContext.iterateEntries([&](const musx::dom::EntryInfoPtr& info) {
                const auto entry = info->getEntry();
                const auto number = entry->getEntryNumber();
                const auto ownRows = entryRows(number, part);
                if (!ownRows.empty()) {
                    visited.emplace(part, static_cast<std::uint16_t>(number >> 16U), static_cast<std::uint16_t>(number));
                }
                const auto scoreRows = isScore ? std::span<const records::LegacyRow>{} : entryRows(number, musx::dom::SCORE_PARTID);
                const auto ownPayload = collectRecordPayload(source, ownRows);
                const auto ownWords = payloadWords(ownPayload, context.profile.byteOrder);
                const auto scoreWords = payloadWords(collectRecordPayload(source, scoreRows), context.profile.byteOrder);
                if (ownPayload.size() % (stride * 2) != 0) {
                    context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info, "Entry chord assignment has an incomplete incidence."});
                }
                const auto count = (ownRows.empty() ? scoreWords.size() : ownWords.size()) / stride;
                for (std::size_t at = 0; at < count * stride; at += stride) {
                    const bool copiedFromScore =
                        !isScore && (ownRows.empty() || (!chordPartUnlinked(context, source, ownRows, at) && at + stride <= scoreWords.size()));
                    const auto rows = copiedFromScore ? scoreRows : ownRows;
                    const auto& words = copiedFromScore ? scoreWords : ownWords;
                    const auto inci = incidence.current();
                    auto target = copiedFromScore ? std::make_shared<ChordTarget>(context.document, part, musx::dom::EnigmaBase::ShareMode::Partial,
                                                        frame->getStaff(), frame->getMeasure(), inci)
                                                  : createDetailsRecordTarget<ChordTarget>(context.document, source, source.rowOfWord(rows, at),
                                                        frame->getStaff(), frame->getMeasure(), inci);
                    target->horzEdu = entry->isPossibleFullMeasureRest() ? measure->calcDuration().calcEduDuration() / 2 - 512
                                                                         : info->elapsedDuration.calcEduDuration();
                    target->chPercent = 100;
                    if (entry->noteDetail) {
                        if (const auto size = context.document->getDetails()->get<musx::dom::details::EntrySize>(part, number)) {
                            target->chPercent = size->percent;
                        }
                    }
                    target->fbPercent = 100;
                    decodeChord(context, source, rows, words, at, *target, layout);
                    if (layout == ChordLayout::ExtendedEntry && !isScore) {
                        const auto score =
                            context.document->getDetails()->get<ChordTarget>(musx::dom::SCORE_PARTID, frame->getStaff(), frame->getMeasure(), inci);
                        if (score && chordFretCoordinateLinked(context, source, rows, at + 5)
                            && chordFretCoordinateLinked(context, source, rows, at + 6)) {
                            // Linked fretboard coordinates are converted from the score value before the part's chord offsets are subtracted.
                            target->fbHorzOff = score->fbHorzOff - target->horzOff;
                            target->fbVertOff = score->fbVertOff - target->vertOff;
                        }
                    }
                    incidence.add(*context.document->getDetails(), std::move(target));
                }
                return true;
            });
        }
    }
    for (const auto& [part, high] : recordKeys(source)) {
        for (const auto low : source.pool->secondCmpersForTag(source.identity, high, part)) {
            if (!visited.contains({part, high, low})) {
                // Ordinarily an entry no frame reaches, which Finale also discards.
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Verbose, "Chord assignment's entry has no retained staff/measure context; assignment skipped."});
            }
        }
    }
}

/// @brief Sets each measure's chord flag the way Finale's entry-chord conversion does.
/// @details The conversion writes the score measure's flag, set when it moved a chord there and
/// clear otherwise, whatever the stored bit said. A part measure follows it unless the part's own
/// record unlinks that bit.
void refreshMeasureChordFlags(const ImportContext& context)
{
    using musx::dom::others::Measure;
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), measureTag, measureClass);
    std::set<musx::dom::MeasCmper> withChords;
    for (const auto& chord : context.document->getDetails()->getArray<ChordTarget>(musx::dom::SCORE_PARTID)) {
        withChords.insert(chord->getCmper2());
    }
    const auto follows = [&](const Measure& measure) {
        if (measure.getSourcePartId() == musx::dom::SCORE_PARTID) {
            return true;
        }
        if (!source || measure.getShareMode() != musx::dom::EnigmaBase::ShareMode::Partial) {
            return false;
        }
        const auto rows = source->pool->getArray(source->identity, measure.getCmper(), 0, measure.getSourcePartId());
        return !(partUnlinkedBits(*source, rows, measureExtendedFlagsSlot, context.profile.byteOrder).value_or(0) & extendedHasChordMask);
    };
    for (const auto& measure : context.document->getOthers()->getAllSources<Measure>()) {
        const bool hasChord = withChords.contains(measure->getCmper());
        if (measure->hasChord == hasChord || !follows(*measure)) {
            continue;
        }
        std::const_pointer_cast<Measure>(measure)->hasChord = hasChord;
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            const auto key = reporting.template instanceKey<Measure>(measure->getSourcePartId(), measure->getCmper());
            reporting.report().fieldProvenance(key, "hasChord").origin = Reporting::Origin::LegacyMusAdjusted;
        });
    }
}

} // namespace

void importChordAssigns(const ImportContext& context)
{
    // The measure-attached class identity distinguishes this layout from entry details.
    // Both entry identities remain entry-attached regardless of the header version.
    context.pending.defer(DeferredStage::ReadComposites, [&context] {
        const auto compact =
            selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), records::packTag("CH"), 0x0437, true);
        const auto extended =
            selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), records::packTag("Ch"), 0x0407, true);
        if (compact) {
            importEntryChords(context, *compact, ChordLayout::CompactEntry);
        }
        if (extended) {
            importEntryChords(context, *extended, ChordLayout::ExtendedEntry);
        }
        if (sourcePredatesVersion(context.profile, FormatEpoch::ZlibLegacy, versions::finale2010)) {
            refreshMeasureChordFlags(context);
        }
    });
    if (context.profile.epoch != FormatEpoch::ZlibLegacy) {
        return;
    }
    const auto source = selectRecordFamilySource(
        context, context.index.getDetails(), context.index.getClassDetails(), records::packTag("Ch"), chordMeasureClass, true);
    if (!source) {
        return;
    }
    for (const auto& [part, staff] : recordKeys(*source)) {
        for (const auto measure : source->pool->secondCmpersForTag(source->identity, staff, part)) {
            const auto rows = source->pool->getArray(source->identity, staff, measure, part);
            const auto payload = collectRecordPayload(*source, rows);
            const auto words = payloadWords(payload, context.profile.byteOrder);
            if (payload.size() % 30 != 0) {
                context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info, "Chord assignment has an incomplete incidence."});
            }
            for (std::size_t at = 0; at + 15 <= words.size(); at += 15) {
                auto target = createDetailsRecordTarget<ChordTarget>(
                    context.document, *source, source->rowOfWord(rows, at), staff, measure, static_cast<musx::dom::Inci>(at / 15));
                target->horzEdu = payloadLong(payload, (at + 10) * 2, context.profile.byteOrder, nativeLongWordOrder(context.profile.byteOrder));
                decodeChord(context, *source, rows, words, at, *target);
                context.document->getDetails()->add(ChordTarget::XmlNodeName, std::move(target));
            }
        }
    }
}

} // namespace details
} // namespace finale_mus_reader
