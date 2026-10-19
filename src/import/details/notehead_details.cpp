// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/details.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <string>

#include "import/support/legacy_percussion.h"
#include "import/support/text_encoding.h"
#include "musx/musx.h"

namespace finale_mus_reader::details {
namespace {

using Accidental = musx::dom::details::AccidentalAlterations;
using Notehead = musx::dom::details::NoteAlterations;

constexpr auto combinedTag = records::packTag("CN");
constexpr auto accidentalTag = records::packTag("Ah");
constexpr auto noteheadTag = records::packTag("cn");
constexpr records::LegacyTag accidentalClass = 0x03ef;
constexpr records::LegacyTag noteheadClass = 0x0422;
constexpr std::size_t fontOffset = 10;
constexpr std::size_t fontSizeOffset = fontOffset + 2;
constexpr std::size_t fontEffectsOffset = fontOffset + 4;

template <typename T>
constexpr std::size_t characterOffset = std::is_same_v<T, Notehead> ? 6 : 8;

template <typename T>
constexpr std::size_t flagOffset = std::is_same_v<T, Notehead> ? 8 : 16;

template <typename T>
void reportImportedDetail(const ImportContext& context, const RecordFamilySource& source, std::span<const records::LegacyRow> rows,
    std::span<const std::uint8_t> payload, const std::shared_ptr<T>& target, std::size_t at, bool early, bool normalizePercent = false)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto entryNumber = target->getEntryNumber();
        const auto key = reporting.template instanceKey<T>(target->getSourcePartId(), static_cast<musx::dom::Cmper>(entryNumber >> 16U),
            target->getInci(), static_cast<musx::dom::Cmper>(entryNumber));
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        const auto word = [&](std::size_t offset) { return payloadWord(payload, at + offset, context.profile.byteOrder); };
        const auto reportField = [&](const char* name, std::size_t offset, std::int64_t raw, bool adjusted = false) {
            const auto& row = source.rowOfByte(rows, at + offset);
            reporting.report().setField(key, name,
                {adjusted ? Reporting::Origin::LegacyMusAdjusted : Reporting::Origin::LegacyMus, row.blockOffset,
                    row.decodedOffset + source.byteOffsetInRow(at + offset), raw, source.identity});
        };
        const auto reportUnmapped = [&](const char* name) { reporting.report().setField(key, name, {Reporting::Origin::Unmapped, 0, 0, 0}); };

        reportField("noteId", 0, target->noteId);
        if constexpr (std::is_same_v<T, Accidental>) {
            if (early) {
                reportFallbackField(reporting, key, "percent", Reporting::Origin::LegacyBehavior, target->percent);
                reportField("hOffset", 6, target->hOffset);
                for (const auto* name : {"vOffset", "altChar", "customFont.fontId", "customFont.fontSize", "customFont.efx", "useOwnFont",
                         "allowVertPos", "customFont.bold", "customFont.italic", "customFont.underline", "customFont.strikeout",
                         "customFont.absolute", "customFont.hidden"}) {
                    reportUnmapped(name);
                }
                return;
            }
        }

        reportField("percent", 2, static_cast<std::int16_t>(word(2)), normalizePercent);
        if constexpr (std::is_same_v<T, Notehead>) {
            reportField("nxdisp", 4, target->nxdisp);
        } else {
            reportField("vOffset", 4, target->vOffset);
            reportField("hOffset", 6, target->hOffset);
        }
        if (early) {
            if constexpr (std::is_same_v<T, Notehead>) {
                reportField("altNhead", 8, target->altNhead);
                for (const auto* name : {"useOwnFont", "customFont.fontId", "customFont.fontSize", "customFont.efx", "allowVertPos", "nydisp",
                         "enharmonic", "customFont.bold", "customFont.italic", "customFont.underline", "customFont.strikeout", "customFont.absolute",
                         "customFont.hidden"}) {
                    reportUnmapped(name);
                }
            }
            return;
        }

        const auto flags = word(flagOffset<T>);
        if constexpr (std::is_same_v<T, Notehead>) {
            reportField("altNhead", characterOffset<T>, word(characterOffset<T>));
            reportField("nydisp", 16, target->nydisp);
            reportField("enharmonic", flagOffset<T>, flags);
        } else {
            reportField("altChar", characterOffset<T>, word(characterOffset<T>));
        }
        reportField("useOwnFont", flagOffset<T>, flags);
        reportField("allowVertPos", flagOffset<T>, flags);
        reportField("customFont.fontId", fontOffset, word(fontOffset));
        reportField("customFont.fontSize", fontSizeOffset, word(fontSizeOffset));
        reportField("customFont.efx", fontEffectsOffset, word(fontEffectsOffset));
        for (const auto* name :
            {"customFont.bold", "customFont.italic", "customFont.underline", "customFont.strikeout", "customFont.absolute", "customFont.hidden"}) {
            reportField(name, fontEffectsOffset, word(fontEffectsOffset));
        }
    });
}

template <typename T>
void importFamily(const ImportContext& context, const RecordFamilySource& source, bool early)
{
    for (const auto& [partId, entryHigh] : recordKeys(source)) {
        for (const auto entryLow : source.pool->secondCmpersForTag(source.identity, entryHigh, partId)) {
            const auto rows = source.pool->getArray(source.identity, entryHigh, entryLow, partId);
            if (rows.empty()) {
                continue;
            }
            const auto payload = collectRecordPayload(source, rows);
            constexpr std::size_t stride = 20;
            const std::size_t recordSize = early ? 10 : stride;
            if (payload.size() % recordSize != 0) {
                context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info, "Notehead detail has an incomplete record."});
            }
            EmittedIncidence incidence;
            for (std::size_t at = 0; at + recordSize <= payload.size(); at += recordSize) {
                const auto inci = incidence.current();
                const auto word = [&](std::size_t offset) { return payloadWord(payload, at + offset, context.profile.byteOrder); };
                const auto signedWord = [&](std::size_t offset) { return static_cast<std::int16_t>(word(offset)); };
                auto target = createDetailsRecordTarget<T>(context.document, source, source.rowOfByte(rows, at), entryHigh, entryLow, inci);
                if (!target) {
                    continue;
                }
                target->noteId = word(0);
                const auto storedPercent = signedWord(2);
                const bool normalizePercent = early && std::is_same_v<T, Notehead> && storedPercent == 0;
                target->percent = normalizePercent ? 100 : storedPercent;
                if constexpr (std::is_same_v<T, Notehead>) {
                    target->nxdisp = signedWord(4);
                } else {
                    target->vOffset = signedWord(4);
                    target->hOffset = signedWord(6);
                }

                if (early) {
                    if constexpr (std::is_same_v<T, Notehead>) {
                        const auto fallback = musx::dom::options::FontOptions::getFontInfoOrNull(
                            context.document, musx::dom::options::FontOptions::FontType::Noteheads);
                        target->altNhead =
                            word(8) == 0 ? 0
                                         : text::codepointFromByte(static_cast<std::uint8_t>(word(8)), context.document,
                                               static_cast<musx::dom::Cmper>(fallback ? fallback->fontId : 0), text::UnresolvedFontFallback::Symbol);
                        if (target->percent == 100 && target->nxdisp == 0 && target->altNhead == 0) {
                            continue;
                        }
                    }
                } else {
                    // The ten-word field order is established for Finale 2012 as well. Its character word stores
                    // a Unicode code point directly; earlier versions encode an unsigned byte through the font.
                    const bool unicodeCharacter =
                        context.profile.epoch == FormatEpoch::ZlibLegacy && versions::storesUnicodeCodepoints(context.profile.version);
                    const auto storedChar = static_cast<char32_t>(word(characterOffset<T>));
                    const auto flags = word(flagOffset<T>);
                    target->useOwnFont = (flags & 0x0001U) != 0;
                    target->allowVertPos = (flags & 0x0002U) != 0;
                    target->customFont->fontId = context.construction.assignFontId(word(fontOffset));
                    target->customFont->fontSize = signedWord(fontSizeOffset);
                    target->customFont->setEnigmaStyles(word(fontEffectsOffset));
                    const auto fallbackType = std::is_same_v<T, Notehead> ? musx::dom::options::FontOptions::FontType::Noteheads
                                                                          : musx::dom::options::FontOptions::FontType::Accis;
                    const auto fallback = musx::dom::options::FontOptions::getFontInfoOrNull(context.document, fallbackType);
                    const auto fontId =
                        static_cast<musx::dom::Cmper>(target->useOwnFont ? target->customFont->fontId : (fallback ? fallback->fontId : 0));
                    const auto character = unicodeCharacter || storedChar == 0 ? storedChar
                                                                               : text::codepointFromByte(static_cast<std::uint8_t>(storedChar),
                                                                                     context.document, fontId, text::UnresolvedFontFallback::Symbol);
                    if constexpr (std::is_same_v<T, Notehead>) {
                        target->altNhead = character;
                        target->nydisp = signedWord(16);
                        target->enharmonic = (flags & 0x0004U) != 0;
                    } else {
                        target->altChar = character;
                    }
                }
                reportImportedDetail(context, source, rows, payload, target, at, early, normalizePercent);
                registerNoteIdRemap(context, target, &T::noteId, early ? combinedNoteReferenceMode(context.profile) : NoteReferenceMode::NoteIdOnly);
                incidence.add(*context.document->getDetails(), std::move(target));
            }
        }
    }
}

void importCombinedAccidentals(const ImportContext& context)
{
    const RecordFamilySource source{&context.index.getDetails(), combinedTag, false, true};
    // The combined record's accidental displacement is separate from its notehead fields.
    for (const auto& [partId, entryHigh] : recordKeys(source)) {
        for (const auto entryLow : source.pool->secondCmpersForTag(source.identity, entryHigh, partId)) {
            const auto rows = source.pool->getArray(source.identity, entryHigh, entryLow, partId);
            const auto payload = collectRecordPayload(source, rows);
            EmittedIncidence incidence;
            for (std::size_t at = 0; at + 10 <= payload.size(); at += 10) {
                const auto displacement = static_cast<std::int16_t>(payloadWord(payload, at + 6, context.profile.byteOrder));
                if (displacement == 0) {
                    continue;
                }
                auto target = createDetailsRecordTarget<Accidental>(
                    context.document, source, source.rowOfByte(rows, at), entryHigh, entryLow, incidence.current());
                target->noteId = payloadWord(payload, at, context.profile.byteOrder);
                target->percent = 100;
                target->hOffset = displacement;
                reportImportedDetail(context, source, rows, payload, target, at, true);
                registerNoteIdRemap(context, target, &Accidental::noteId, combinedNoteReferenceMode(context.profile));
                incidence.add(*context.document->getDetails(), std::move(target));
            }
        }
    }
}

// Single-incidence details preserve Note ID references; they do not use combined-notehead TCD conversion.
template <typename T>
void importSingleIncidenceNoteDetails(const ImportContext& context, records::LegacyTag tag, records::LegacyTag classId)
{
    const auto selected = selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), tag, classId, true);
    if (!selected) {
        return;
    }
    const auto& source = *selected;
    for (const auto& [partId, entryHigh] : recordKeys(source)) {
        for (const auto entryLow : source.pool->secondCmpersForTag(source.identity, entryHigh, partId)) {
            const auto rows = source.pool->getArray(source.identity, entryHigh, entryLow, partId);
            const auto payload = collectRecordPayload(source, rows);
            constexpr std::size_t stride = 10;
            if (payload.size() % stride != 0) {
                context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info, "Note detail has an incomplete trailing record."});
            }
            EmittedIncidence incidence;
            for (std::size_t at = 0; at + stride <= payload.size(); at += stride) {
                auto target =
                    createDetailsRecordTarget<T>(context.document, source, source.rowOfByte(rows, at), entryHigh, entryLow, incidence.current());
                if (!target) {
                    continue;
                }
                const auto word = [&](std::size_t slot) { return payloadWord(payload, at + slot * 2, context.profile.byteOrder); };
                target->noteId = word(0);
                if constexpr (std::is_same_v<T, musx::dom::details::CrossStaff>) {
                    target->staff = word(1);
                } else if constexpr (std::is_same_v<T, musx::dom::details::PercussionNoteCode>) {
                    target->noteCode = word(1);
                } else if constexpr (std::is_same_v<T, musx::dom::details::TablatureNoteMods>) {
                    target->stringNumber = static_cast<std::int16_t>(word(1));
                } else {
                    target->hOffset = static_cast<std::int16_t>(word(1));
                    target->vOffset = static_cast<std::int16_t>(word(2));
                    target->interdotSpacing = static_cast<std::int16_t>(word(3));
                }
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                    const auto key = reporting.template instanceKey<T>(partId, entryHigh, target->getInci(), entryLow);
                    reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
                    const auto stored = [&](const char* member, std::size_t slot, std::int64_t raw) {
                        const auto offset = at + slot * 2;
                        reportLegacyField(reporting, key, source, source.rowOfByte(rows, offset), member, source.byteOffsetInRow(offset), raw);
                    };
                    stored("noteId", 0, target->noteId);
                    if constexpr (std::is_same_v<T, musx::dom::details::CrossStaff>) {
                        stored("staff", 1, target->staff);
                    } else if constexpr (std::is_same_v<T, musx::dom::details::PercussionNoteCode>) {
                        stored("noteCode", 1, target->noteCode);
                    } else if constexpr (std::is_same_v<T, musx::dom::details::TablatureNoteMods>) {
                        stored("stringNumber", 1, target->stringNumber);
                    } else {
                        stored("hOffset", 1, target->hOffset);
                        stored("vOffset", 2, target->vOffset);
                        stored("interdotSpacing", 3, target->interdotSpacing);
                    }
                });
                if constexpr (std::is_same_v<T, musx::dom::details::PercussionNoteCode>) {
                    if (word(2) || word(3) || word(4)) {
                        context.report.diagnostics.push_back(
                            {musx::util::Logger::LogLevel::Info, "Percussion note-code detail has nonzero trailing words."});
                    }
                }
                registerNoteIdRemap(context, target, &T::noteId);
                if constexpr (std::is_same_v<T, musx::dom::details::DotAlterations>) {
                    if (context.profile.epoch != FormatEpoch::ZlibLegacy && target->vOffset != 0) {
                        context.pending.defer(DeferredStage::NormalizeStoredValues, [&context, target] {
                            const auto entry = context.document->getEntries()->get(target->getEntryNumber());
                            if (!entry) {
                                return;
                            }
                            for (const auto& note : entry->notes) {
                                if (note->getNoteId() != target->noteId || !note->crossStaff) {
                                    continue;
                                }
                                // Keep default-valued details so later incidences retain their identity.
                                target->vOffset = 0;
                                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                                    const auto number = target->getEntryNumber();
                                    const auto key = reporting.template instanceKey<T>(target->getSourcePartId(),
                                        static_cast<musx::dom::Cmper>(number >> 16U), target->getInci(), static_cast<musx::dom::Cmper>(number));
                                    if (auto* field = reporting.report().findField(key, "vOffset")) {
                                        field->origin = Reporting::Origin::LegacyMusAdjusted;
                                    }
                                });
                                break;
                            }
                        });
                    }
                }
                incidence.add(*context.document->getDetails(), std::move(target));
            }
        }
    }
}

std::optional<int> legacyPercussionInputKey(const musx::dom::KeySignature& key, const musx::dom::Note& note)
{
    using KeyContext = musx::dom::KeySignature::KeyContext;
    const auto document = key.getDocument();
    const auto mode = key.getKeyMode();
    const auto attributes = document->getOthers()->get<musx::dom::others::KeyAttributes>(musx::dom::SCORE_PARTID, mode);
    const int reference = attributes ? attributes->harmRefer : 0;
    const int middleC = attributes && attributes->middleCKey ? attributes->middleCKey : music_theory::calcMidiKey({music_theory::NoteName::C, 4});
    const int unit = attributes && attributes->gotoKey ? attributes->gotoKey : 1;
    if (mode < 2) {
        auto pitch = key.calcPitch(note.harmLev + reference, 0, KeyContext::Concert);
        pitch.alteration = pitch.alteration / unit + note.harmAlt / unit;
        return music_theory::calcMidiKey(pitch) + middleC - music_theory::calcMidiKey({music_theory::NoteName::C, 4});
    }
    const auto format = document->getOthers()->get<musx::dom::others::KeyFormat>(musx::dom::SCORE_PARTID, mode);
    const int scaleTones = format && format->scaleTones ? int(format->scaleTones) : music_theory::STANDARD_DIATONIC_STEPS;
    const int divisions = format && format->semitones ? int(format->semitones) : music_theory::STANDARD_12EDO_STEPS;
    const int level = key.calcTonalCenterIndex(KeyContext::Concert) + note.harmLev + reference;
    int octave = level / scaleTones;
    int degree = level % scaleTones;
    if (degree < 0) {
        degree += scaleTones;
        --octave;
    }
    int chromatic = 0;
    const auto map = document->getOthers()->get<musx::dom::others::KeyMapArray>(musx::dom::SCORE_PARTID, mode);
    if (map && !map->steps.empty()) {
        const auto found = std::find_if(
            map->steps.begin(), map->steps.end(), [&](const auto& step) { return step->diatonic && step->hlevel == static_cast<unsigned>(degree); });
        if (found == map->steps.end()) {
            return std::nullopt;
        }
        chromatic = static_cast<int>(std::distance(map->steps.begin(), found));
    } else {
        chromatic = music_theory::calc12EdoHalfstepsInInterval(degree, 0);
    }
    return middleC + octave * divisions + chromatic + key.calcAlterationOnNote(degree, KeyContext::Concert) / unit + note.harmAlt / unit;
}

/// Returns the stored assignment that supplies an entry's percussion layout: the last Staff Style
/// at the entry that sets notation, otherwise the staff's own.
percussion::LegacyAssignmentKey effectivePercussionAssignment(
    const musx::dom::DocumentPtr& document, musx::dom::Cmper partId, const musx::dom::EntryInfoPtr& entryInfo, const musx::dom::others::Staff& staff)
{
    percussion::LegacyAssignmentKey result{musx::dom::SCORE_PARTID, false, musx::dom::Cmper(entryInfo.getStaff())};
    if (!staff.hasStyles) {
        return result;
    }
    for (const auto& style : musx::dom::others::StaffStyle::findAllOverlappingStyles(
             document, partId, entryInfo.getStaff(), entryInfo.getMeasure(), entryInfo->elapsedDuration.calcEduDuration())) {
        if (style->masks && style->masks->notationStyle) {
            result = {musx::dom::SCORE_PARTID, true, style->getCmper()};
        }
    }
    return result;
}

/// Returns the virtual MIDI channel that plays a staff layer, as the percussion upgrade looks it
/// up: through a Staff Style changing the instrument in measure 1, otherwise through the staff's
/// playback route for the layer. A channel outside 0 through 127 is none.
std::optional<int> percussionVirtualChannel(
    const musx::dom::DocumentPtr& document, musx::dom::Cmper partId, musx::dom::StaffCmper staffId, musx::dom::LayerIndex layer)
{
    using namespace musx::dom::others;
    for (const auto& assign : document->getOthers()->getArray<StaffStyleAssign>(partId, staffId)) {
        if (assign->startMeas > 1 || assign->endMeas < 1) {
            continue;
        }
        // A pre-Finale 2012 note-shape style keeps its notation-style override as useNoteShapes.
        const auto style = document->getOthers()->get<StaffStyle>(partId, assign->styleId);
        if (style && style->masks
            && (style->masks->notationStyle || style->masks->useNoteShapes || style->masks->defaultClef || style->masks->showNoteColors)) {
            // The style's own playback data, which begins in Finale 2012, would supply the channel.
            return std::nullopt;
        }
    }
    const auto playData = document->getOthers()->get<StaffPlayData>(partId, staffId);
    if (!playData || layer >= playData->layers.size() || !playData->layers[layer]) {
        return std::nullopt;
    }
    const auto route = document->getOthers()->get<PlaybackRoute>(partId, playData->layers[layer]->getPlaybackRouteId());
    if (!route || route->virtualChannel < 0 || route->virtualChannel > 127) {
        return std::nullopt;
    }
    return route->virtualChannel;
}

/// Sets the pitch Finale gives a note of a percussion type: the type's note in the default
/// percussion MIDI map, spelled in C major, when a virtual channel plays the layer; otherwise
/// the staff position of the first layout note with that type.
void setPercussionNotePitch(
    musx::dom::Note& note, musx::dom::PercussionNoteTypeId type, bool played, const std::vector<percussion::LegacyLayoutNote>& layout)
{
    if (const auto midi = played ? percussion::defaultMidiNote(type) : std::nullopt) {
        music_theory::Transposer transposer(0, 0);
        transposer.stepwiseTranspose(int(*midi) - music_theory::calcMidiKey({music_theory::NoteName::C, 4}));
        // Isolated chromatic pitches in C major prefer flats except the raised fourth.
        const bool sharp = *midi % music_theory::STANDARD_12EDO_STEPS == music_theory::calcPitchClass(music_theory::NoteName::F, 1);
        if ((sharp && transposer.alteration() < 0) || (!sharp && transposer.alteration() > 0)) {
            transposer.enharmonicTranspose(music_theory::sign(transposer.alteration()));
        }
        note.harmLev = transposer.displacement();
        note.harmAlt = transposer.alteration();
        return;
    }
    const auto found = std::find_if(layout.begin(), layout.end(), [type](const auto& item) { return item.type == type; });
    note.harmLev = found == layout.end() ? 0 : found->staffPosition;
    note.harmAlt = 0;
}

template <typename Reporting>
void reportPercussionPitch(Reporting& reporting, musx::dom::EntryNumber number, musx::dom::NoteNumber noteId)
{
    const auto high = static_cast<musx::dom::Cmper>(number >> 16U);
    const auto low = static_cast<musx::dom::Cmper>(number);
    const auto noteKey = reporting.template instanceKey<musx::dom::Note>(musx::dom::SCORE_PARTID, high, noteId, low);
    for (const auto* member : {"harmLev", "harmAlt"}) {
        if (auto* field = reporting.report().findField(noteKey, member)) {
            field->origin = Reporting::Origin::LegacyMusAdjusted;
        }
    }
}

/// Assigns legacy percussion notes their note types and pitches. A note without a stored code
/// takes the type its input MIDI key selects in its assignment's layout; then a layout with a
/// conversion-table entry replaces each code it retyped. Either step resets the note's pitch.
void synthesizePercussionNoteCodes(const ImportContext& context)
{
    using Code = musx::dom::details::PercussionNoteCode;
    using Staff = musx::dom::others::Staff;
    using StaffStyle = musx::dom::others::StaffStyle;
    if (!percussion::storesLegacyLayouts(context)) {
        return;
    }
    const auto& conversion = percussion::legacyConversion(context);
    if (conversion.layouts.empty()) {
        return;
    }
    Staff::calcAllRuntimeValues<Staff>(context.document);
    Staff::calcAllRuntimeValues<StaffStyle>(context.document);
    auto holds = context.document->getDetails()->getAllSources<musx::dom::details::GFrameHold>();
    // The score converts first; a part reaches only entries the score did not.
    std::stable_sort(holds.begin(), holds.end(), [](const auto& a, const auto& b) { return a->getSourcePartId() < b->getSourcePartId(); });
    // A percussion staff reads its notes in C major whatever key signature is in effect.
    const musx::dom::KeySignature percussionKey(context.document);
    std::set<musx::dom::EntryNumber> converted;
    for (const auto& hold : holds) {
        const auto partId = hold->getSourcePartId();
        const auto staffList = context.document->getOthers()->getArray<musx::dom::others::StaffUsed>(partId, musx::dom::BASE_SYSTEM_ID);
        if (std::none_of(staffList.begin(), staffList.end(), [&](const auto& item) { return item->staffId == hold->getStaff(); })) {
            continue;
        }
        musx::dom::details::GFrameHoldContext(hold).iterateEntries([&](const musx::dom::EntryInfoPtr& entryInfo) {
            const auto entry = entryInfo->getEntry();
            if (!entry->isNote || converted.contains(entry->getEntryNumber())) {
                return true;
            }
            const auto staff = entryInfo.createCurrentStaff();
            if (!staff || staff->notationStyle != Staff::NotationStyle::Percussion) {
                return true;
            }
            const auto assignment = conversion.assignments.find(effectivePercussionAssignment(context.document, partId, entryInfo, *staff));
            if (assignment == conversion.assignments.end() || assignment->second.inputTypes.empty()) {
                return true;
            }
            const auto number = entry->getEntryNumber();
            converted.insert(number);
            const bool played = percussionVirtualChannel(context.document, partId, hold->getStaff(), entryInfo.getLayerIndex()).has_value();
            const auto& inputTypes = assignment->second.inputTypes;
            const auto layout = assignment->second.layoutId ? conversion.layouts.find(*assignment->second.layoutId) : conversion.layouts.end();
            const auto retypes = assignment->second.layoutId ? conversion.retypes.find(*assignment->second.layoutId) : conversion.retypes.end();
            auto codes = context.document->getDetails()->getArray<Code>(partId, number);
            musx::dom::Inci nextInci = 0;
            for (const auto& code : codes) {
                nextInci = (std::max)(nextInci, musx::dom::Inci(code->getInci().value() + 1));
            }
            bool changed = false;
            for (const auto& note : entry->notes) {
                const auto stored = std::find_if(codes.begin(), codes.end(), [&](const auto& code) { return code->noteId == note->getNoteId(); });
                Code* code = stored == codes.end() ? nullptr : const_cast<Code*>(stored->get());
                if (!code) {
                    const auto inputKey = legacyPercussionInputKey(percussionKey, *note);
                    if (!inputKey) {
                        context.report.diagnostics.push_back(
                            {musx::util::Logger::LogLevel::Warning, "Legacy percussion note has no diatonic step in its key map."});
                        continue;
                    }
                    const auto type = inputTypes.find(*inputKey);
                    if (type == inputTypes.end()) {
                        continue;
                    }
                    auto created = std::make_shared<Code>(context.document, partId, musx::dom::EnigmaBase::ShareMode::All, number, nextInci++);
                    created->noteId = note->getNoteId();
                    created->noteCode = type->second;
                    setPercussionNotePitch(*note, created->noteCode, played, assignment->second.convertedNotes);
                    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                        const auto instance = reporting.template instanceKey<Code>(
                            partId, static_cast<musx::dom::Cmper>(number >> 16U), created->getInci(), static_cast<musx::dom::Cmper>(number));
                        reporting.report().setInstanceOrigin(instance, Reporting::Origin::LegacyBehavior);
                        reportFallbackField(reporting, instance, "noteId", Reporting::Origin::LegacyBehavior, created->noteId);
                        reportFallbackField(reporting, instance, "noteCode", Reporting::Origin::LegacyBehavior, created->noteCode);
                        reportPercussionPitch(reporting, number, note->getNoteId());
                    });
                    code = created.get();
                    context.document->getDetails()->add(Code::XmlNodeName, std::move(created));
                    changed = true;
                }
                if (retypes == conversion.retypes.end() || layout == conversion.layouts.end()) {
                    continue;
                }
                const auto retyped = retypes->second.find(code->noteCode);
                if (retyped == retypes->second.end()) {
                    continue;
                }
                code->noteCode = retyped->second;
                setPercussionNotePitch(*note, code->noteCode, played, layout->second);
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                    const auto instance = reporting.template instanceKey<Code>(
                        partId, static_cast<musx::dom::Cmper>(number >> 16U), code->getInci(), static_cast<musx::dom::Cmper>(number));
                    if (auto* field = reporting.report().findField(instance, "noteCode")) {
                        if (field->origin != Reporting::Origin::LegacyBehavior) {
                            field->origin = Reporting::Origin::LegacyMusAdjusted;
                        }
                    }
                    reportPercussionPitch(reporting, number, note->getNoteId());
                });
                changed = true;
            }
            if (changed) {
                const_cast<musx::dom::Entry*>(entry.get())->noteDetail = true;
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                    const auto entryKey = reporting.template instanceKey<musx::dom::Entry>(
                        musx::dom::SCORE_PARTID, static_cast<musx::dom::Cmper>(number >> 16U), std::nullopt, static_cast<musx::dom::Cmper>(number));
                    if (auto* field = reporting.report().findField(entryKey, "noteDetail")) {
                        field->origin = Reporting::Origin::LegacyMusAdjusted;
                    }
                });
            }
            return true;
        });
    }
}

} // namespace

void importAccidentalAlterations(const ImportContext& context)
{
    if (context.profile.epoch == FormatEpoch::CodaBanner || context.profile.epoch == FormatEpoch::UncompressedLegacy) {
        importCombinedAccidentals(context);
        return;
    }
    const auto source =
        selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), accidentalTag, accidentalClass, true);
    if (source) {
        importFamily<Accidental>(context, *source, false);
    }
}

void importCrossStaff(const ImportContext& context)
{
    importSingleIncidenceNoteDetails<musx::dom::details::CrossStaff>(context, records::packTag("CD"), 0x040c);
}

void importDotAlterations(const ImportContext& context)
{
    const auto tag = records::packTag(context.profile.epoch == FormatEpoch::CodaBanner ? "DO" : "do");
    importSingleIncidenceNoteDetails<musx::dom::details::DotAlterations>(context, tag, 0x040d);
}

void importNoteAlterations(const ImportContext& context)
{
    if (context.profile.epoch == FormatEpoch::CodaBanner || context.profile.epoch == FormatEpoch::UncompressedLegacy) {
        const RecordFamilySource source{&context.index.getDetails(), combinedTag, false, true};
        importFamily<Notehead>(context, source, true);
        return;
    }
    const auto source =
        selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), noteheadTag, noteheadClass, true);
    if (source) {
        importFamily<Notehead>(context, *source, false);
    }
}

void importPercussionNoteCodes(const ImportContext& context)
{
    // Believed: the named selector uses the native five-word layout when present.
    importSingleIncidenceNoteDetails<musx::dom::details::PercussionNoteCode>(context, records::packTag("nC"), 0x0451);
    context.pending.defer(DeferredStage::ReadComposites, [&context] { synthesizePercussionNoteCodes(context); });
}

void importTablatureNoteMods(const ImportContext& context)
{
    importSingleIncidenceNoteDetails<musx::dom::details::TablatureNoteMods>(context, records::packTag("GN"), 0x042d);
}

} // namespace finale_mus_reader::details
