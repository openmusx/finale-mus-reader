// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/details.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "musx/musx.h"

namespace finale_mus_reader::details {
namespace {

constexpr std::size_t assignmentWords = 5;
constexpr std::size_t extendedAssignmentWords = 10;
constexpr std::uint16_t displayVerseNumberBit = 0x8000;
constexpr records::LegacyTag chorusClass = 0x0418;
constexpr records::LegacyTag sectionClass = 0x0419;
constexpr records::LegacyTag verseClass = 0x041a;
constexpr records::LegacyTag laterChorusClass = 0x0452;
constexpr records::LegacyTag laterSectionClass = 0x0453;
constexpr records::LegacyTag laterVerseClass = 0x0454;
constexpr records::LegacyTag entryInfoClass = 0x041b;

std::optional<musx::dom::AlignJustify> lyricAlignment(int value)
{
    switch (value) {
    case 1: return musx::dom::AlignJustify::Center;
    case 2: return musx::dom::AlignJustify::Left;
    case 3: return musx::dom::AlignJustify::Right;
    default: return std::nullopt;
    }
}

template <typename Target>
void importAssignments(const ImportContext& context, const RecordFamilySource& source, bool extended)
{
    const auto stride = extended ? extendedAssignmentWords : assignmentWords;
    // Finale's pre-2011 conversion keeps a part's own lyric only when its offsets are unlinked.
    constexpr std::array<std::uint16_t, 4> comparedBits{0, 0, 0xffff, 0xffff};
    for (const auto& [partId, high] : recordKeys(source)) {
        for (const auto low : source.pool->secondCmpersForTag(source.identity, high, partId)) {
            const auto ownRows = source.pool->getArray(source.identity, high, low, partId);
            const auto ownWords = collectRecordWords(source, ownRows, context.profile.byteOrder);
            if (ownWords.size() % stride != 0) {
                context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info, "Lyric assignment has an incomplete incidence."});
            }
            const bool converted = !extended && partId != musx::dom::SCORE_PARTID;
            const auto scoreRows =
                converted ? source.pool->getArray(source.identity, high, low, musx::dom::SCORE_PARTID) : std::span<const records::LegacyRow>{};
            const auto scoreWords = collectRecordWords(source, scoreRows, context.profile.byteOrder);
            const auto keepsOwn = [&](std::size_t at) {
                return at + stride <= ownWords.size()
                       && (!converted || partRecordUnlinked(source, ownRows, at, comparedBits, context.profile.byteOrder));
            };
            const auto count = (std::max)(ownWords.size(), scoreWords.size()) / stride;
            bool keepsList = !converted;
            for (std::size_t at = 0; !keepsList && at < count * stride; at += stride) {
                keepsList = keepsOwn(at);
            }
            if (!keepsList) {
                continue;
            }
            // A part that keeps any lyric of its own on an entry holds the entry's complete list,
            // with its linked lyrics copied from the score.
            for (std::size_t at = 0; at < count * stride; at += stride) {
                const bool copiedFromScore = converted && !keepsOwn(at) && at + stride <= scoreWords.size();
                const auto rows = copiedFromScore ? scoreRows : ownRows;
                const auto& words = copiedFromScore ? scoreWords : ownWords;
                const auto inci = static_cast<musx::dom::Inci>(at / stride);
                auto target = copiedFromScore
                                  ? std::make_shared<Target>(context.document, partId, musx::dom::EnigmaBase::ShareMode::Partial,
                                        (static_cast<musx::dom::EntryNumber>(high) << 16U) | low, inci)
                                  : createDetailsRecordTarget<Target>(context.document, source, source.rowOfWord(rows, at), high, low, inci);
                target->lyricNumber = static_cast<std::uint16_t>(words[at]);
                target->syllable = static_cast<std::uint16_t>(words[at + 1]);
                target->horzOffset = words[at + 2];
                target->vertOffset = words[at + 3];
                target->wext = words[at + 4];
                if (extended) {
                    target->floatingHorzOff = words[at + 5];
                    target->displayVerseNum = (static_cast<std::uint16_t>(words[at + 6]) & displayVerseNumberBit) != 0;
                }
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                    const auto key = reporting.template instanceKey<Target>(partId, high, inci, low);
                    reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
                    const auto field = [&](const char* name, std::size_t slot, std::int64_t value) {
                        const auto position = at + slot;
                        reportLegacyField(
                            reporting, key, source, source.rowOfWord(rows, position), name, source.byteOffsetInRow(position * 2), value);
                    };
                    field("lyricNumber", 0, target->lyricNumber);
                    field("syllable", 1, target->syllable);
                    field("horzOffset", 2, target->horzOffset);
                    field("vertOffset", 3, target->vertOffset);
                    field("wext", 4, target->wext);
                    if (extended) {
                        field("floatingHorzOff", 5, target->floatingHorzOff);
                        field("displayVerseNum", 6, static_cast<std::uint16_t>(words[at + 6]));
                    } else {
                        reportFallbackField(reporting, key, "floatingHorzOff", Reporting::Origin::Finale27Default, 0);
                        reportFallbackField(reporting, key, "displayVerseNum", Reporting::Origin::Finale27Default, false);
                    }
                });
                context.document->getDetails()->add(Target::XmlNodeName, std::move(target));
            }
        }
    }
}

template <typename Target>
void importAssignmentFamily(const ImportContext& context, records::LegacyTag selector, records::LegacyTag earlyClass, records::LegacyTag laterClass)
{
    // The ten-word classes replaced the five-word ones in Finale 2011, and a document stores one
    // set or the other, so the version selects which family is read and the other is never consulted.
    if (sourceAtOrAfter(context.profile, FormatEpoch::ZlibLegacy, versions::finale2011)) {
        importAssignments<Target>(context, RecordFamilySource{&context.index.getClassDetails(), laterClass, true, true}, true);
    } else if (const auto early =
                   selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), selector, earlyClass, true)) {
        importAssignments<Target>(context, *early, false);
    }
}

} // namespace

void importLyricAssignChorus(const ImportContext& context)
{
    importAssignmentFamily<musx::dom::details::LyricAssignChorus>(context, records::packTag("ch"), chorusClass, laterChorusClass);
}

void importLyricAssignSection(const ImportContext& context)
{
    importAssignmentFamily<musx::dom::details::LyricAssignSection>(context, records::packTag("se"), sectionClass, laterSectionClass);
}

void importLyricAssignVerse(const ImportContext& context)
{
    importAssignmentFamily<musx::dom::details::LyricAssignVerse>(context, records::packTag("ve"), verseClass, laterVerseClass);
}

void importLyricEntryInfo(const ImportContext& context)
{
    const auto source =
        selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), records::packTag("LE"), entryInfoClass, true);
    if (!source) {
        return;
    }
    for (const auto& [partId, high] : recordKeys(*source)) {
        for (const auto low : source->pool->secondCmpersForTag(source->identity, high, partId)) {
            const auto rows = source->pool->getArray(source->identity, high, low, partId);
            const auto words = collectRecordWords(*source, rows, context.profile.byteOrder);
            if (words.size() != assignmentWords) {
                context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info, "Lyric entry info requires one five-word record."});
                continue;
            }
            using Target = musx::dom::details::LyricEntryInfo;
            const auto shareMode = recordShareMode(*source, rows.front());
            const auto entryNumber = (static_cast<musx::dom::EntryNumber>(high) << 16U) | low;
            auto target = std::make_shared<Target>(context.document, partId, shareMode, entryNumber);
            if (shareMode == musx::dom::EnigmaBase::ShareMode::Partial) {
                initializePartialFromScore(target, context.document->getDetails()->get<Target>(musx::dom::SCORE_PARTID, entryNumber));
            }
            const auto flags = static_cast<std::uint16_t>(words[4]);
            const auto justify = flags & 0x0003;
            const auto align = (flags & 0x0300) >> 8;
            target->justify = lyricAlignment(justify);
            target->align = lyricAlignment(align);
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<Target>(partId, high, std::nullopt, low);
                reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
                const auto& row = source->rowOfWord(rows, 4);
                const auto offset = source->byteOffsetInRow(8);
                reportLegacyField(reporting, key, *source, row, "justify", offset, justify);
                reportLegacyField(reporting, key, *source, row, "align", offset, align);
            });
            context.document->getDetails()->add(Target::XmlNodeName, std::move(target));
        }
    }
}

} // namespace finale_mus_reader::details
