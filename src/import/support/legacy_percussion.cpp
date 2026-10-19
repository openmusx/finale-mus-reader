// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/support/legacy_percussion.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "import/support/percussion_mappings.h"
#include "import/support/percussion_records.h"
#include "import/support/text_encoding.h"

namespace finale_mus_reader {
namespace percussion {
namespace {

using LegacyNoteType = musx::dom::PercussionNoteTypeId;
using LegacyLayoutNotes = std::vector<LegacyLayoutNote>;

constexpr LegacyNoteType legacyBaseNoteTypeMask = 0x0fff;
constexpr LegacyNoteType legacyMaxDuplicateType = 0xf000;
constexpr LegacyNoteType legacyDuplicateIncrement = 0x1000;
constexpr std::uint16_t legacyMidiNoteCount = 128;
constexpr std::size_t legacySelectionStart = 2;
constexpr std::size_t legacySelectionEnd = 18;

struct UpgradeNoteTypeParent
{
    LegacyNoteType type;
    LegacyNoteType parent;
};

/// The upgrade carries its own note-type data, which gives these types different parents than
/// musxdom's table does. Parent zero marks a type the upgrade's data does not define.
constexpr std::array<UpgradeNoteTypeParent, 17> upgradeNoteTypeParents{{
    {63, 53},
    {65, 53},
    {247, 1},
    {270, 251},
    {511, 0},
    {512, 0},
    {513, 0},
    {514, 0},
    {516, 0},
    {517, 0},
    {518, 0},
    {519, 0},
    {520, 0},
    {521, 0},
    {522, 0},
    {523, 0},
    {524, 0},
}};

LegacyNoteType upgradeParentType(LegacyNoteType type)
{
    for (const auto& entry : upgradeNoteTypeParents) {
        if (entry.type == type) {
            return entry.parent;
        }
    }
    const auto& data = musx::dom::percussion::getPercussionNoteTypeFromId(type);
    return data.instrumentId == type ? LegacyNoteType(data.parentInstrumentId) : LegacyNoteType(0);
}

std::optional<std::uint8_t> generalMidiNote(LegacyNoteType type)
{
    const auto& data = musx::dom::percussion::getPercussionNoteTypeFromId(type);
    if (data.instrumentId != type || data.generalMidi < 0 || data.generalMidi >= legacyMidiNoteCount) {
        return std::nullopt;
    }
    return static_cast<std::uint8_t>(data.generalMidi);
}

/// Finale's default percussion MIDI map lists each note type that has a General MIDI note, in
/// note-type order, and resolves an output MIDI note to the first type listed for it.
LegacyNoteType generalMidiNoteType(std::uint16_t midiNote)
{
    if (midiNote >= legacyMidiNoteCount) {
        return legacyBaseNoteTypeMask;
    }
    static const auto noteTypes = [] {
        std::array<LegacyNoteType, legacyMidiNoteCount> result{};
        for (std::uint32_t id = 1; id <= legacyBaseNoteTypeMask; ++id) {
            if (const auto midi = generalMidiNote(LegacyNoteType(id)); midi && result[*midi] == 0) {
                result[*midi] = LegacyNoteType(id);
            }
        }
        return result;
    }();
    return noteTypes[midiNote];
}

/// Advances the duplicate number in a type's high nibble, which saturates at 15. Returns false
/// when the type was already at the last duplicate number.
bool incrementDuplicateType(LegacyNoteType& type)
{
    const auto original = type;
    if ((type >> 12U) < 0xfU) {
        type = LegacyNoteType(type + legacyDuplicateIncrement);
    }
    return original < legacyMaxDuplicateType;
}

const LegacyLayoutNote* findLayoutNote(const LegacyLayoutNotes& notes, LegacyNoteType type)
{
    const auto found = std::find_if(notes.begin(), notes.end(), [type](const auto& note) { return note.type == type; });
    return found == notes.end() ? nullptr : &*found;
}

/// Adds a selected row under the General MIDI type of its output note. An identical existing
/// note absorbs it; a different note with that type moves it to the next duplicate number, and
/// past the last duplicate number to the highest free base types.
LegacyNoteType addUpgradedNote(LegacyLayoutNotes& notes, LegacyLayoutNote note)
{
    note.type = generalMidiNoteType(note.playbackKey);
    auto fallbackType = LegacyNoteType(legacyBaseNoteTypeMask - 1);
    for (auto existing = findLayoutNote(notes, note.type); existing; existing = findLayoutNote(notes, note.type)) {
        if (existing->sameAs(note)) {
            return note.type;
        }
        if (!incrementDuplicateType(note.type)) {
            note.type = fallbackType--;
        }
    }
    notes.push_back(note);
    return note.type;
}

/// Gives each later note that repeats an earlier note's type the next duplicate number.
void separateDuplicateTypes(LegacyLayoutNotes& notes)
{
    for (std::size_t first = 0; first < notes.size(); ++first) {
        const auto type = notes[first].type;
        for (std::size_t later = first + 1; later < notes.size(); ++later) {
            if (notes[later].type == type) {
                incrementDuplicateType(notes[later].type);
            }
        }
    }
}

bool sameLayout(const LegacyLayoutNotes& first, const LegacyLayoutNotes& second)
{
    return std::equal(first.begin(), first.end(), second.begin(), second.end(), [](const auto& a, const auto& b) { return a.sameAs(b); });
}

std::map<musx::dom::Cmper, std::string> legacyLayoutNames(const ImportContext& context)
{
    std::map<musx::dom::Cmper, std::string> result;
    const auto selected = selectRecordFamilySource(
        context, context.index.getOthers(), context.index.getClassOthers(), percussion_records::layoutNameTag, percussion_records::layoutNameClass);
    if (!selected) {
        return result;
    }
    for (const auto& [partId, layoutId] : recordKeys(*selected)) {
        if (partId != musx::dom::SCORE_PARTID) {
            continue;
        }
        const auto rows = selected->pool->getArray(selected->identity, layoutId, 0, partId);
        if (!rows.empty()) {
            result.try_emplace(layoutId, text::toUtf8(readRowText(*selected->pool, rows), context.profile.platform));
        }
    }
    return result;
}

struct UpgradeLayout
{
    LegacyLayoutNotes notes;
    std::string name;
};

class LegacyConversionBuilder
{
public:
    explicit LegacyConversionBuilder(const ImportContext& context)
        : m_context(context), m_names(legacyLayoutNames(context))
    {
        for (const auto& [layoutId, name] : m_names) {
            m_occupiedIds.insert(layoutId);
        }
        m_rows = selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), percussion_records::layoutRowTag,
            percussion_records::layoutRowClass, true);
    }

    LegacyConversion build()
    {
        collectAssignments(percussion_records::drumStaffTag, percussion_records::drumStaffClass, false);
        // Staff styles begin in Finale 2000.
        const bool styles = sourceAtOrAfter(m_context.profile, FormatEpoch::UncompressedLegacy, versions::finale2000);
        if (styles) {
            collectAssignments(percussion_records::drumStaffStyleTag, percussion_records::drumStaffStyleClass, true);
        }

        // Each percussion staff in the score's Scroll View, then each Staff Style that sets
        // percussion notation, receives its own layout at the lowest free layout comparator.
        using Staff = musx::dom::others::Staff;
        const auto& document = m_context.document;
        for (const auto& staff : document->getOthers()->getArray<Staff>(musx::dom::SCORE_PARTID)) {
            if (staff->notationStyle == Staff::NotationStyle::Percussion && inScrollView(staff->getCmper())) {
                convert({musx::dom::SCORE_PARTID, false, staff->getCmper()});
            }
        }
        if (styles) {
            for (const auto& style : document->getOthers()->getArray<musx::dom::others::StaffStyle>(musx::dom::SCORE_PARTID)) {
                if (style->masks && style->masks->notationStyle && style->notationStyle == Staff::NotationStyle::Percussion) {
                    convert({musx::dom::SCORE_PARTID, true, style->getCmper()});
                }
            }
        }

        mergeIdenticalLayouts();
        closeLayoutGaps();
        retypeConvertedLayouts();

        for (auto& [layoutId, layout] : m_layouts) {
            m_result.layouts.emplace(layoutId, std::move(layout.notes));
        }
        return std::move(m_result);
    }

private:
    void collectAssignments(records::LegacyTag tag, records::LegacyTag classId, bool style)
    {
        const auto source = selectRecordFamilySource(m_context, m_context.index.getOthers(), m_context.index.getClassOthers(), tag, classId);
        if (!source) {
            return;
        }
        for (const auto& [partId, cmper] : recordKeys(*source)) {
            const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
            if (rows.empty()) {
                continue;
            }
            const auto payload = collectRecordPayload(*source, rows);
            if (payload.size() < 2) {
                continue;
            }
            const auto storedLayoutId = payloadWord(payload, 0, m_context.profile.byteOrder);
            const LegacyAssignmentKey key{partId, style, cmper};
            m_result.assignments.emplace(key, LegacyAssignment{*source, rows.front(), storedLayoutId, storedLayoutId, {}, {}});
            // Before Finale 2010, eight words after the layout comparator select its rows: bit n
            // of the stream selects the row whose second comparator is n.
            auto& selected = m_selections[key];
            for (std::size_t offset = legacySelectionStart; offset + 2 <= payload.size() && offset < legacySelectionEnd; offset += 2) {
                const auto bitmap = payloadWord(payload, offset, m_context.profile.byteOrder);
                const auto firstKey = static_cast<int>((offset - legacySelectionStart) / 2U * 16U);
                for (unsigned bit = 0; bit < 16; ++bit) {
                    if ((bitmap & (std::uint16_t{1} << bit)) != 0) {
                        selected.insert(firstKey + static_cast<int>(bit));
                    }
                }
            }
        }
    }

    [[nodiscard]] bool inScrollView(musx::dom::Cmper staffId) const
    {
        const auto list = m_context.document->getOthers()->getArray<musx::dom::others::StaffUsed>(musx::dom::SCORE_PARTID, musx::dom::BASE_SYSTEM_ID);
        return std::any_of(list.begin(), list.end(), [staffId](const auto& item) { return item->staffId == staffId; });
    }

    /// Builds a new layout from an assignment's selected rows. An assignment that selects no
    /// layout or no rows is deleted.
    void convert(const LegacyAssignmentKey& key)
    {
        const auto found = m_result.assignments.find(key);
        if (found == m_result.assignments.end()) {
            return;
        }
        auto& assignment = found->second;
        const auto& selected = m_selections[key];
        if (!assignment.storedLayoutId || selected.empty()) {
            assignment.layoutId.reset();
            return;
        }

        // The upgrade numbers a new layout by the lowest layout-name comparator not in use, a
        // record family musxdom does not represent. These numbers are provisional: they decide
        // which stale assignments a later renumbering moves, and closeLayoutGaps() replaces them.
        musx::dom::Cmper layoutId = 1;
        while (m_occupiedIds.contains(layoutId)) {
            ++layoutId;
        }
        m_occupiedIds.insert(layoutId);
        assignment.layoutId = layoutId;

        auto& layout = m_layouts[layoutId];
        if (const auto name = m_names.find(assignment.storedLayoutId); name != m_names.end() && !name->second.empty()) {
            // A layout shared by several assignments gives each later copy a numbered name.
            const auto copy = ++m_copies[assignment.storedLayoutId];
            layout.name = copy > 1 ? name->second + "-" + std::to_string(copy) : name->second;
        }
        for (const auto inputKey : selected) {
            LegacyLayoutNote note;
            if (m_rows) {
                const auto rows = m_rows->pool->getArray(
                    m_rows->identity, assignment.storedLayoutId, static_cast<musx::dom::Cmper>(inputKey), musx::dom::SCORE_PARTID);
                if (!rows.empty()) {
                    const auto payload = m_rows->pool->effectivePayloadOf(rows.front());
                    if (payload.size() >= records::detailWordCount * 2U) {
                        const auto word = [&](std::size_t offset) { return payloadWord(payload, offset, m_context.profile.byteOrder); };
                        note.playbackKey = word(0);
                        note.staffPosition = static_cast<std::int16_t>(word(2));
                        note.closedNotehead = word(4);
                        note.openNotehead = word(6);
                        note.row = rows.front();
                    }
                }
            }
            assignment.inputTypes[inputKey] = addUpgradedNote(layout.notes, note);
        }
        assignment.convertedNotes = layout.notes;
    }

    void replaceAssignments(musx::dom::Cmper from, musx::dom::Cmper to)
    {
        for (auto& [key, assignment] : m_result.assignments) {
            if (key.partId == musx::dom::SCORE_PARTID && assignment.layoutId == from) {
                assignment.layoutId = to;
            }
        }
    }

    /// A layout identical to an earlier one is deleted, and its assignments use the earlier one.
    void mergeIdenticalLayouts()
    {
        for (auto layout = m_layouts.begin(); layout != m_layouts.end();) {
            const auto earlier = std::find_if(
                m_layouts.begin(), layout, [&](const auto& candidate) { return sameLayout(candidate.second.notes, layout->second.notes); });
            if (earlier == layout) {
                ++layout;
                continue;
            }
            replaceAssignments(layout->first, earlier->first);
            layout = m_layouts.erase(layout);
        }
    }

    /// Renumbers the remaining layouts from one in comparator order. Any assignment naming a
    /// moved comparator follows it, including one the upgrade did not convert.
    void closeLayoutGaps()
    {
        std::map<musx::dom::Cmper, UpgradeLayout> renumbered;
        musx::dom::Cmper next = 1;
        for (auto& [layoutId, layout] : m_layouts) {
            if (layoutId != next) {
                replaceAssignments(layoutId, next);
            }
            renumbered.emplace(next++, std::move(layout));
        }
        m_layouts = std::move(renumbered);
    }

    /// A layout whose name has a conversion-table entry takes the target map's note type for
    /// each note's default MIDI note.
    void retypeConvertedLayouts()
    {
        const auto* mappings = m_context.profile.percussionMappings;
        if (!mappings) {
            return;
        }
        for (auto& [layoutId, layout] : m_layouts) {
            const auto* target = mappings->conversionTarget(layout.name);
            if (!target) {
                continue;
            }
            auto& retypes = m_result.retypes[layoutId];
            for (auto& note : layout.notes) {
                const auto midi = defaultMidiNote(note.type);
                const auto converted = midi ? target->find(*midi) : target->end();
                if (converted == target->end() || converted->second == 0) {
                    continue;
                }
                retypes.try_emplace(note.type, converted->second);
                note.type = converted->second;
            }
            separateDuplicateTypes(layout.notes);
        }
    }

    const ImportContext& m_context;
    std::map<musx::dom::Cmper, std::string> m_names;
    std::set<musx::dom::Cmper> m_occupiedIds;
    std::optional<RecordFamilySource> m_rows;
    std::map<LegacyAssignmentKey, std::set<int>> m_selections;
    std::map<musx::dom::Cmper, int> m_copies;
    std::map<musx::dom::Cmper, UpgradeLayout> m_layouts;
    LegacyConversion m_result;
};

} // namespace

bool LegacyLayoutNote::sameAs(const LegacyLayoutNote& other) const
{
    return type == other.type && staffPosition == other.staffPosition && closedNotehead == other.closedNotehead && openNotehead == other.openNotehead;
}

bool storesLegacyLayouts(const ImportContext& context)
{
    // Percussion maps begin after the Coda-banner epoch. Finale 2010 development build 56
    // replaced the selected-row layouts with stored note collections, whose presence decides.
    const auto& profile = context.profile;
    return profile.epoch != FormatEpoch::CodaBanner && sourcePredatesVersion(profile, FormatEpoch::ZlibLegacy, VersionBound{15, 0, 0, 0, 56})
           && context.index.getClassOthers().partIdsForTag(percussion_records::noteCollectionClass).empty();
}

const LegacyConversion& legacyConversion(const ImportContext& context)
{
    if (!context.pending.legacyPercussion) {
        context.pending.legacyPercussion = std::make_shared<const LegacyConversion>(LegacyConversionBuilder(context).build());
    }
    return *context.pending.legacyPercussion;
}

std::optional<std::uint8_t> defaultMidiNote(musx::dom::PercussionNoteTypeId type)
{
    if (const auto midi = generalMidiNote(type)) {
        return midi;
    }
    auto current = LegacyNoteType(type & legacyBaseNoteTypeMask);
    if (current != type) {
        if (const auto midi = generalMidiNote(current)) {
            return midi;
        }
    }
    // A root type is its own parent. The bound only guards against a cyclic chain.
    for (std::uint32_t depth = 0; depth <= legacyBaseNoteTypeMask; ++depth) {
        const auto parent = upgradeParentType(current);
        if (parent == current) {
            break;
        }
        if (const auto midi = generalMidiNote(parent)) {
            return midi;
        }
        current = parent;
    }
    return std::nullopt;
}

} // namespace percussion
} // namespace finale_mus_reader
