// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/entries.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <vector>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace entries {
namespace {

using Entry = musx::dom::Entry;
using Note = musx::dom::Note;

constexpr std::uint32_t validMask = 0x80000000U;
constexpr std::uint32_t noteMask = 0x40000000U;
constexpr std::uint32_t createdByHpMask = 0x08000000U;
constexpr std::uint32_t playDisabledByHpMask = 0x02000000U;

template <typename Target>
struct FlagField
{
    const char* name;
    bool Target::* member;
    std::uint32_t mask;
};

constexpr auto entryMainFlags = std::to_array<FlagField<Entry>>({
    {"v2Launch", &Entry::v2Launch, 0x20000000U},
    {"voice2", &Entry::voice2, 0x10000000U},
    {"floatRest", &Entry::floatRest, 0x01000000U},
    {"graceNote", &Entry::graceNote, 0x00800000U},
    {"noteDetail", &Entry::noteDetail, 0x00400000U},
    {"articDetail", &Entry::articDetail, 0x00200000U},
    {"lyricDetail", &Entry::lyricDetail, 0x00100000U},
    {"tupletStart", &Entry::tupletStart, 0x00080000U},
    {"splitRest", &Entry::splitRest, 0x00040000U},
    {"performanceData", &Entry::performanceData, 0x00020000U},
    {"isHidden", &Entry::isHidden, 0x00008000U},
    {"beamExt", &Entry::beamExt, 0x00004000U},
    {"flipTie", &Entry::flipTie, 0x00002000U},
    {"dotTieAlt", &Entry::dotTieAlt, 0x00001000U},
    {"beam", &Entry::beam, 0x00000800U},
    {"secBeam", &Entry::secBeam, 0x00000400U},
    {"freezeStemScore", &Entry::freezeStemScore, 0x00000100U},
    {"stemDetail", &Entry::stemDetail, 0x00000080U},
    {"crossStaff", &Entry::crossStaff, 0x00000060U},
    {"reverseUpStem", &Entry::reverseUpStem, 0x00000010U},
    {"reverseDownStem", &Entry::reverseDownStem, 0x00000008U},
    {"doubleStem", &Entry::doubleStem, 0x00000004U},
    {"splitStem", &Entry::splitStem, 0x00000002U},
    {"upStemScore", &Entry::upStemScore, 0x00000001U},
});

constexpr auto entryExtendedFlags = std::to_array<FlagField<Entry>>({
    {"checkAccis", &Entry::checkAccis, 0x0004U},
    {"dummy", &Entry::dummy, 0x0010U},
    {"smartShapeDetail", &Entry::smartShapeDetail, 0x0020U},
    {"noLeger", &Entry::noLeger, 0x0040U},
    {"sorted", &Entry::sorted, 0x0080U},
    {"slashGrace", &Entry::slashGrace, 0x0100U},
    {"flatBeam", &Entry::flatBeam, 0x0200U},
    {"noPlayback", &Entry::noPlayback, 0x0400U},
    {"noSpacing", &Entry::noSpacing, 0x0800U},
    {"freezeBeam", &Entry::freezeBeam, 0x1000U},
});

constexpr auto noteFlagFields = std::to_array<FlagField<Note>>({
    {"tieStart", &Note::tieStart, 0x40000000U},
    {"tieEnd", &Note::tieEnd, 0x20000000U},
    {"crossStaff", &Note::crossStaff, 0x10000000U},
    {"upStemSecond", &Note::upStemSecond, 0x08000000U},
    {"downStemSecond", &Note::downStemSecond, 0x04000000U},
    {"upSplitStem", &Note::upSplitStem, 0x02000000U},
    {"showAcci", &Note::showAcci, 0x01000000U},
    {"parenAcci", &Note::parenAcci, 0x00800000U},
    {"noPlayback", &Note::noPlayback, 0x00400000U},
    {"noSpacing", &Note::noSpacing, 0x00200000U},
    {"freezeAcci", &Note::freezeAcci, 0x00000002U},
});

constexpr std::uint16_t supportedNoteCount = 12;

std::vector<musx::dom::NoteNumber> normalizedNoteIds(
    std::span<const std::uint32_t> noteFlags, const Entry& entry, std::uint32_t entryNumber, PendingReferences& pending, ImportReport& report)
{
    std::vector<musx::dom::NoteNumber> ids;
    ids.reserve(noteFlags.size());
    for (const auto flags : noteFlags) {
        ids.push_back(static_cast<musx::dom::NoteNumber>((flags >> 16U) & 0x001fU));
    }
    const auto originalIds = ids;
    bool changed = false;
    if (!entry.isNote) {
        if (ids.size() == 1) {
            changed = ids.front() != Note::RESTID;
            ids.front() = Note::RESTID;
        } else if (ids.size() > 1 && !entry.floatRest) {
            report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning,
                "Entry " + std::to_string(entryNumber) + " is a non-floating rest with multiple Notes; Note IDs were preserved."});
        }
    } else {
        std::set<musx::dom::NoteNumber> usedIds;
        for (const auto id : ids) {
            if (id != 0) {
                usedIds.insert(id);
            }
        }
        musx::dom::NoteNumber nextId = 1;
        for (auto& id : ids) {
            if (id == 0) {
                while (usedIds.contains(nextId)) {
                    ++nextId;
                }
                id = nextId++;
                changed = true;
            }
        }
    }
    if (changed) {
        pending.noteIdsByEntryIndex.insert_or_assign(entryNumber, PendingReferences::NoteIdsByIndex{originalIds, ids});
    }
    return ids;
}

struct NoteSlot
{
    const records::EntryRow* row{};
    std::size_t offset{};
};

template <typename Reporting>
void reportEntry(Reporting& reporting, const records::EntryRow& row, const Entry& entry, std::uint32_t number, std::uint32_t previous,
    std::uint32_t next, std::uint32_t duration, std::uint32_t flags, std::uint16_t extendedFlags, std::int16_t position, bool hasHpFlags,
    bool synthesizeNoPlayback)
{
    const auto key =
        reporting.template instanceKey<Entry>(musx::dom::SCORE_PARTID, musx::dom::Cmper(number >> 16U), std::nullopt, musx::dom::Cmper(number));
    const auto recovered = [&](const char* name, std::int64_t raw, std::size_t at) {
        reporting.report().setField(key, name, {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset + at, raw});
    };
    recovered("previousEntryNumber", previous, 6);
    recovered("nextEntryNumber", next, 10);
    recovered("duration", duration, 14);
    recovered("hOffsetScore", position, 16);
    recovered("isValid", flags, 18);
    recovered("isNote", flags, 18);
    recovered("numNotes", entry.numNotes, 24);
    for (const auto& field : entryMainFlags) {
        recovered(field.name, flags, 18);
    }
    for (const auto& field : entryExtendedFlags) {
        if (field.member == &Entry::noPlayback && synthesizeNoPlayback) {
            reporting.report().setField(key, field.name, {Reporting::Origin::LegacyBehavior, 0, 0, entry.noPlayback});
        } else {
            recovered(field.name, extendedFlags, 22);
        }
    }
    if (hasHpFlags) {
        recovered("createdByHP", flags, 18);
        recovered("playDisabledByHP", flags, 18);
    } else {
        reporting.report().setField(key, "createdByHP", {Reporting::Origin::LegacyBehavior, 0, 0, 0});
        reporting.report().setField(key, "playDisabledByHP", {Reporting::Origin::LegacyBehavior, 0, 0, 0});
    }
    reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
}

template <typename Reporting>
void reportNote(Reporting& reporting, const records::EntryRow& row, const Note& note, std::uint32_t entryNumber, std::uint16_t tcd,
    std::uint32_t flags, std::size_t offset)
{
    const auto key = reporting.template instanceKey<Note>(
        musx::dom::SCORE_PARTID, musx::dom::Cmper(entryNumber >> 16U), note.getNoteId(), musx::dom::Cmper(entryNumber));
    const auto recovered = [&](const char* name, std::int64_t raw, std::size_t at) {
        reporting.report().setField(key, name, {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset + at, raw});
    };
    recovered("harmLev", tcd, offset);
    recovered("harmAlt", tcd, offset);
    recovered("isValid", flags, offset + 2);
    for (const auto& field : noteFlagFields) {
        recovered(field.name, flags, offset + 2);
    }
    reporting.report().setField(key, "playDisabledByHP", {Reporting::Origin::LegacyBehavior, 0, 0, 0});
    reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
}

void setTcd(Note& note, std::uint16_t tcd)
{
    note.harmLev = static_cast<int>(tcd >> 4U) - ((tcd & 0x8000U) != 0 ? 4096 : 0);
    const auto alterationMagnitude = static_cast<int>(tcd & 0x0007U);
    note.harmAlt = (tcd & 0x0008U) != 0 ? -alterationMagnitude : alterationMagnitude;
}

template <typename Reporting>
void reportCodaEntry(Reporting& reporting, const records::CodaEntryRow& row, const Entry& entry, std::uint32_t previous, std::uint32_t next,
    std::uint32_t flags, std::uint16_t duration, std::int16_t position)
{
    const auto key = reporting.template instanceKey<Entry>(musx::dom::SCORE_PARTID, musx::dom::Cmper(0), std::nullopt, musx::dom::Cmper(row.number));
    const auto recovered = [&](const char* name, std::int64_t raw, std::size_t at) {
        reporting.report().setField(key, name, {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset + at, raw});
    };
    recovered("previousEntryNumber", previous, 0);
    recovered("nextEntryNumber", next, 4);
    recovered("duration", duration, 16);
    recovered("hOffsetScore", position, 18);
    recovered("isValid", flags, 20);
    recovered("isNote", flags, 20);
    reporting.report().setField(key, "numNotes", {Reporting::Origin::LegacyMusAdjusted, row.blockOffset, row.decodedOffset + 24, entry.numNotes});
    for (const auto& field : entryMainFlags) {
        if (field.member == &Entry::splitRest) {
            reporting.unmappedField(key, field.name, entry.*field.member);
        } else {
            recovered(field.name, flags, 20);
        }
    }
    for (const auto& field : entryExtendedFlags) {
        if (field.member == &Entry::sorted || field.member == &Entry::noSpacing) {
            reporting.report().setField(key, field.name, {Reporting::Origin::LegacyBehavior, 0, 0, entry.*field.member});
        } else if (field.member == &Entry::noPlayback) {
            reporting.report().setField(key, field.name, {Reporting::Origin::LegacyBehavior, 0, 0, entry.noPlayback});
        } else {
            reporting.unmappedField(key, field.name, entry.*field.member);
        }
    }
    reporting.report().setField(key, "createdByHP", {Reporting::Origin::LegacyBehavior, 0, 0, 0});
    reporting.report().setField(key, "playDisabledByHP", {Reporting::Origin::LegacyBehavior, 0, 0, 0});
    reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
}

template <typename Reporting>
void reportCodaNote(
    Reporting& reporting, const records::CodaEntryRow& row, const Note& note, std::uint32_t entryNumber, std::uint32_t flags, std::size_t offset)
{
    const auto key =
        reporting.template instanceKey<Note>(musx::dom::SCORE_PARTID, musx::dom::Cmper(0), note.getNoteId(), musx::dom::Cmper(entryNumber));
    const auto recovered = [&](const char* name, std::int64_t raw, std::size_t at) {
        reporting.report().setField(key, name, {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset + at, raw});
    };
    recovered("harmLev", flags & 0xffffU, offset + 2);
    recovered("harmAlt", flags & 0xffffU, offset + 2);
    recovered("isValid", flags, offset);
    for (const auto& field : noteFlagFields) {
        if (field.member == &Note::freezeAcci) {
            reporting.report().setField(key, field.name, {Reporting::Origin::LegacyBehavior, 0, 0, 0});
        } else if (field.member == &Note::noPlayback || field.member == &Note::noSpacing) {
            reporting.unmappedField(key, field.name, note.*field.member);
        } else {
            recovered(field.name, flags, offset);
        }
    }
    reporting.report().setField(key, "playDisabledByHP", {Reporting::Origin::LegacyBehavior, 0, 0, 0});
    reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
}

void importCodaEntries(const ImportContext& context)
{
    const auto rows = context.index.getCodaEntryRows();
    std::set<std::uint32_t> fragments;
    const auto readLong = [&](std::span<const std::uint8_t> bytes, std::size_t offset) {
        return static_cast<std::uint32_t>(payloadLong(bytes, offset, context.profile.byteOrder, nativeLongWordOrder(context.profile.byteOrder)));
    };
    for (const auto& row : rows) {
        if (row.number == 0 || fragments.contains(row.number)) {
            continue;
        }
        const std::span<const std::uint8_t> bytes(row.bytes);
        const auto flags = readLong(bytes, 20);
        if ((flags & validMask) == 0) {
            continue;
        }
        const auto previous = readLong(bytes, 0);
        const auto next = readLong(bytes, 4);
        const auto duration = payloadWord(bytes, 16, context.profile.byteOrder);
        const auto position = static_cast<std::int16_t>(payloadWord(bytes, 18, context.profile.byteOrder));
        std::vector<const records::CodaEntryRow*> noteRows{&row};
        auto previousFragment = row.number;
        auto nextFragment = readLong(bytes, 12);
        while (nextFragment != 0 && nextFragment < rows.size() && noteRows.size() * 2 < supportedNoteCount) {
            const auto& candidate = rows[nextFragment];
            const std::span<const std::uint8_t> candidateBytes(candidate.bytes);
            const auto candidateFlags = readLong(candidateBytes, 20);
            if (fragments.contains(nextFragment) || readLong(candidateBytes, 8) != previousFragment || readLong(candidateBytes, 0) != 0
                || readLong(candidateBytes, 4) != 0 || (candidateFlags & validMask) == 0
                || payloadWord(candidateBytes, 16, context.profile.byteOrder) != duration) {
                break;
            }
            fragments.insert(nextFragment);
            noteRows.push_back(&candidate);
            previousFragment = nextFragment;
            nextFragment = readLong(candidateBytes, 12);
        }
        auto entry =
            std::make_shared<Entry>(context.document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, row.number, previous, next);
        entry->duration = static_cast<std::int16_t>(duration);
        // Believed: Coda positions include horizontal stretching. The inverse scale factor is
        // unidentified, so preserve the stored score offset.
        entry->hOffsetScore = position;
        entry->isValid = true;
        entry->isNote = (flags & noteMask) != 0;
        entry->sorted = true;
        for (const auto& field : entryMainFlags) {
            if (field.member != &Entry::splitRest) {
                entry.get()->*field.member = (flags & field.mask) != 0;
            }
        }
        entry->noPlayback = entry->isHidden;
        struct CodaNoteSlot
        {
            const records::CodaEntryRow* row;
            std::size_t offset;
            std::uint32_t flags;
        };
        std::vector<CodaNoteSlot> noteSlots;
        for (const auto* noteRow : noteRows) {
            const std::span<const std::uint8_t> noteBytes(noteRow->bytes);
            for (const auto offset : {24U, 28U}) {
                const auto noteFlags = readLong(noteBytes, offset);
                if ((noteFlags & validMask) != 0) {
                    noteSlots.push_back({noteRow, offset, noteFlags});
                }
            }
        }
        std::vector<std::uint32_t> noteFlagsByIndex;
        noteFlagsByIndex.reserve(noteSlots.size());
        for (const auto& slot : noteSlots) {
            noteFlagsByIndex.push_back(slot.flags);
        }
        std::vector<std::uint16_t> storedTcd;
        storedTcd.reserve(noteFlagsByIndex.size());
        for (const auto noteFlags : noteFlagsByIndex) {
            storedTcd.push_back(static_cast<std::uint16_t>(noteFlags));
        }
        context.pending.noteTcdByEntryIndex.insert_or_assign(row.number, std::move(storedTcd));
        const auto noteIds = normalizedNoteIds(noteFlagsByIndex, *entry, row.number, context.pending, context.report);
        for (std::size_t noteIndex = 0; noteIndex < noteSlots.size(); ++noteIndex) {
            const auto& slot = noteSlots[noteIndex];
            const auto noteFlags = slot.flags;
            auto note = std::make_shared<Note>(context.document, noteIds[noteIndex]);
            setTcd(*note, static_cast<std::uint16_t>(noteFlags));
            note->isValid = true;
            note->freezeAcci = false;
            for (const auto& field : noteFlagFields) {
                if (field.member != &Note::freezeAcci && field.member != &Note::noPlayback && field.member != &Note::noSpacing) {
                    note.get()->*field.member = (noteFlags & field.mask) != 0;
                }
            }
            withReporting(context.report,
                [&]<typename Reporting>(Reporting& reporting) { reportCodaNote(reporting, *slot.row, *note, row.number, noteFlags, slot.offset); });
            entry->notes.push_back(std::move(note));
        }
        entry->numNotes = static_cast<int>(entry->notes.size());
        withReporting(context.report,
            [&]<typename Reporting>(Reporting& reporting) { reportCodaEntry(reporting, row, *entry, previous, next, flags, duration, position); });
        context.document->getEntries()->add(row.number, std::move(entry));
    }
}

} // namespace

void importEntries(const ImportContext& context)
{
    if (context.profile.epoch == FormatEpoch::CodaBanner) {
        importCodaEntries(context);
        return;
    }
    if (context.index.unsupportedEntryBlockCount() != 0) {
        context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info, "An entry block has an unsupported row length."});
    }
    const auto rows = context.index.getEntryRows();
    const bool hasHpFlags = sourceAtOrAfter(context.profile, FormatEpoch::DclLegacy, VersionBound{9, 0});
    const bool synthesizeNoPlayback = sourcePredatesVersion(context.profile, FormatEpoch::DclLegacy, versions::finale2002);
    for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        const auto& row = rows[rowIndex];
        const std::span<const std::uint8_t> bytes(row.bytes);
        const auto number =
            static_cast<std::uint32_t>(payloadLong(bytes, 0, context.profile.byteOrder, nativeLongWordOrder(context.profile.byteOrder)));
        const auto rowIncidence = payloadWord(bytes, 4, context.profile.byteOrder);
        if (rowIncidence != 0) {
            context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info, "Entry continuation row has no parent entry."});
            continue;
        }
        const auto previous =
            static_cast<std::uint32_t>(payloadLong(bytes, 6, context.profile.byteOrder, nativeLongWordOrder(context.profile.byteOrder)));
        const auto next =
            static_cast<std::uint32_t>(payloadLong(bytes, 10, context.profile.byteOrder, nativeLongWordOrder(context.profile.byteOrder)));
        const auto duration = payloadWord(bytes, 14, context.profile.byteOrder);
        const auto position = static_cast<std::int16_t>(payloadWord(bytes, 16, context.profile.byteOrder));
        const auto flags =
            static_cast<std::uint32_t>(payloadLong(bytes, 18, context.profile.byteOrder, nativeLongWordOrder(context.profile.byteOrder)));
        const auto extendedFlags = payloadWord(bytes, 22, context.profile.byteOrder);
        const auto noteCount = payloadWord(bytes, 24, context.profile.byteOrder);
        if (number == 0 || noteCount > supportedNoteCount) {
            context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info, "Entry row has an unsupported entry number or note count."});
            continue;
        }
        std::array<NoteSlot, supportedNoteCount> noteSlots{};
        const auto firstRowNotes = std::min<std::size_t>(noteCount, 2);
        for (std::size_t noteIndex = 0; noteIndex < firstRowNotes; ++noteIndex) {
            noteSlots[noteIndex] = {&row, 26 + noteIndex * 6};
        }
        std::size_t collected = firstRowNotes;
        std::uint16_t expectedIncidence = 1;
        while (collected < noteCount && rowIndex + 1 < rows.size()) {
            const auto& continuation = rows[rowIndex + 1];
            const std::span<const std::uint8_t> continuationBytes(continuation.bytes);
            const auto continuationNumber = static_cast<std::uint32_t>(
                payloadLong(continuationBytes, 0, context.profile.byteOrder, nativeLongWordOrder(context.profile.byteOrder)));
            const auto incidence = payloadWord(continuationBytes, 4, context.profile.byteOrder);
            if (continuation.blockOffset != row.blockOffset || continuation.decodedOffset != rows[rowIndex].decodedOffset + row.bytes.size()
                || continuationNumber != number || incidence != expectedIncidence) {
                break;
            }
            ++rowIndex;
            ++expectedIncidence;
            const auto continuationNotes = std::min<std::size_t>(noteCount - collected, 5);
            for (std::size_t noteIndex = 0; noteIndex < continuationNotes; ++noteIndex) {
                noteSlots[collected++] = {&continuation, 6 + noteIndex * 6};
            }
        }
        if (collected != noteCount) {
            context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info, "Entry row has incomplete note continuation."});
            continue;
        }
        auto entry =
            std::make_shared<Entry>(context.document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, number, previous, next);
        entry->duration = static_cast<std::int16_t>(duration);
        entry->hOffsetScore = position;
        entry->isValid = (flags & validMask) != 0;
        entry->isNote = (flags & noteMask) != 0;
        entry->numNotes = noteCount;
        if (hasHpFlags) {
            entry->createdByHP = (flags & createdByHpMask) != 0;
            entry->playDisabledByHP = (flags & playDisabledByHpMask) != 0;
        }
        for (const auto& field : entryMainFlags) {
            entry.get()->*field.member = (flags & field.mask) != 0;
        }
        for (const auto& field : entryExtendedFlags) {
            entry.get()->*field.member = (extendedFlags & field.mask) != 0;
        }
        if (synthesizeNoPlayback) {
            entry->noPlayback = entry->isHidden;
        }
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            reportEntry(reporting, row, *entry, number, previous, next, duration, flags, extendedFlags, position, hasHpFlags, synthesizeNoPlayback);
        });
        std::array<std::uint32_t, supportedNoteCount> noteFlagsByIndex{};
        for (std::size_t noteIndex = 0; noteIndex < noteCount; ++noteIndex) {
            const auto& noteRow = *noteSlots[noteIndex].row;
            const auto offset = noteSlots[noteIndex].offset;
            noteFlagsByIndex[noteIndex] = static_cast<std::uint32_t>(
                payloadLong(noteRow.bytes, offset + 2, context.profile.byteOrder, nativeLongWordOrder(context.profile.byteOrder)));
        }
        const auto noteIds =
            normalizedNoteIds(std::span<const std::uint32_t>(noteFlagsByIndex.data(), noteCount), *entry, number, context.pending, context.report);
        for (std::size_t noteIndex = 0; noteIndex < noteCount; ++noteIndex) {
            const auto& noteRow = *noteSlots[noteIndex].row;
            const auto offset = noteSlots[noteIndex].offset;
            const std::span<const std::uint8_t> noteBytes(noteRow.bytes);
            const auto tcd = payloadWord(noteBytes, offset, context.profile.byteOrder);
            const auto noteFlags = noteFlagsByIndex[noteIndex];
            auto note = std::make_shared<Note>(context.document, noteIds[noteIndex]);
            setTcd(*note, tcd);
            note->isValid = (noteFlags & validMask) != 0;
            for (const auto& field : noteFlagFields) {
                note.get()->*field.member = (noteFlags & field.mask) != 0;
            }
            withReporting(context.report,
                [&]<typename Reporting>(Reporting& reporting) { reportNote(reporting, noteRow, *note, number, tcd, noteFlags, offset); });
            entry->notes.push_back(std::move(note));
        }
        context.document->getEntries()->add(number, std::move(entry));
    }
}

} // namespace entries
} // namespace finale_mus_reader
