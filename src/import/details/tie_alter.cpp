// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/details.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace details {
namespace {

using TieBase = musx::dom::details::TieAlterBase;
constexpr std::size_t tieAlterWords = 15;

TieBase::DirectionContext tieDirectionContext(std::uint16_t flags)
{
    return (flags & 0x3000U) == 0x2000U   ? TieBase::DirectionContext::Over
           : (flags & 0x3000U) == 0x1000U ? TieBase::DirectionContext::Under
                                          : TieBase::DirectionContext::None;
}

template <typename Target>
void importTieAlterFamily(const ImportContext& context, const char* tag, records::LegacyTag classId, const char* earlyVerticalTag)
{
    const auto selected =
        selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), records::packTag(tag), classId, true);
    if (!selected) {
        return;
    }
    const auto& source = *selected;
    // Three old five-word alterations can have the same size as one later record.
    // Version selects the migration because the record length is ambiguous.
    const bool early = sourcePredatesVersion(context.profile, FormatEpoch::UncompressedLegacy, VersionBound{3, 8, 0, 1, 4});
    const std::size_t stride = early ? records::detailWordCount : tieAlterWords;
    const RecordFamilySource verticalSource{&context.index.getDetails(), records::packTag(earlyVerticalTag), false, true};
    for (const auto& [partId, high] : recordKeys(source)) {
        for (const auto low : source.pool->secondCmpersForTag(source.identity, high, partId)) {
            const auto rows = source.pool->getArray(source.identity, high, low, partId);
            const auto words = collectRecordWords(source, rows, context.profile.byteOrder);
            if (words.size() % stride != 0) {
                context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info, "Tie alteration has an incomplete record."});
            }
            const auto verticalRows =
                early ? verticalSource.pool->getArray(verticalSource.identity, high, low, partId) : std::span<const records::LegacyRow>{};
            const auto verticalWords = collectRecordWords(verticalSource, verticalRows, context.profile.byteOrder);
            EmittedIncidence incidence;
            for (std::size_t at = 0; at + stride <= words.size(); at += stride) {
                auto target = createDetailsRecordTarget<Target>(context.document, source, source.rowOfWord(rows, at), high, low, incidence.current());
                std::optional<std::size_t> verticalAt;
                if (early) {
                    for (std::size_t candidate = 0; candidate + records::detailWordCount <= verticalWords.size();
                        candidate += records::detailWordCount) {
                        if (verticalWords[candidate] == words[at]) {
                            verticalAt = candidate + 1;
                            break;
                        }
                    }
                }
                // Migration keeps the original direction flags and duplicates the old height.
                constexpr std::array<int, tieAlterWords> earlySlots{0, 1, 2, -1, 4, -1, -1, -1, -1, -1, -1, 3, -1, 3, -1};
                const auto word = [&](std::size_t slot) -> std::uint16_t {
                    if (!early) {
                        return static_cast<std::uint16_t>(words[at + slot]);
                    }
                    if ((slot == 6 || slot == 7) && verticalAt) {
                        return static_cast<std::uint16_t>(verticalWords[*verticalAt]);
                    }
                    return earlySlots[slot] < 0 ? 0 : static_cast<std::uint16_t>(words[at + static_cast<std::size_t>(earlySlots[slot])]);
                };
                const auto signedWord = [&](std::size_t slot) { return static_cast<std::int16_t>(word(slot)); };
                target->noteId = word(0);
                target->xStart = signedWord(1);
                target->xEnd = signedWord(2);
                target->yStart = signedWord(6);
                target->yEnd = signedWord(7);
                target->outerLocal = (word(3) & 0x0001U) != 0;
                target->outerOn = (word(3) & 0x0002U) != 0;
                target->afterSingleDotLocal = (word(3) & 0x0004U) != 0;
                target->afterSingleDotOn = (word(3) & 0x0008U) != 0;
                target->afterMultiDotsLocal = (word(3) & 0x0010U) != 0;
                target->afterMultiDotsOn = (word(3) & 0x0020U) != 0;
                target->shiftForSecondsLocal = (word(3) & 0x0040U) != 0;
                target->shiftForSecondsOn = (word(3) & 0x0080U) != 0;
                target->beforeSingleAcciLocal = (word(3) & 0x0100U) != 0;
                target->beforeSingleAcciOn = (word(3) & 0x0200U) != 0;
                target->down = (word(4) & 0x0001U) != 0;
                target->breakTimeLocal = (word(4) & 0x0002U) != 0;
                target->breakTimeOn = (word(4) & 0x0004U) != 0;
                target->breakKeyLocal = (word(4) & 0x0008U) != 0;
                target->breakKeyOn = (word(4) & 0x0010U) != 0;
                target->freezeDirection = (word(4) & 0x0800U) != 0;
                target->noSpecialArc = (word(4) & 0x1000U) != 0;
                target->stPtAdjOn = (word(8) & 0x4000U) != 0;
                target->stPtEntCnct = static_cast<TieBase::ConnectionType>(word(8) & 0x00ffU);
                target->stPtContext = tieDirectionContext(word(8));
                target->enPtAdjOn = (word(9) & 0x4000U) != 0;
                target->enPtEntCnct = static_cast<TieBase::ConnectionType>(word(9) & 0x00ffU);
                target->enPtContext = tieDirectionContext(word(9));
                target->insetRatio1 = signedWord(10);
                target->height1 = signedWord(11);
                target->insetRatio2 = signedWord(12);
                target->height2 = signedWord(13);
                target->ctlPtAdjOn = (word(14) & 0x4000U) != 0;
                target->ctlPtFixed = (word(14) & 0x2000U) != 0;
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                    const auto key = reporting.template instanceKey<Target>(partId, high, target->getInci(), low);
                    reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
                    const auto stored = [&](const char* name, std::size_t slot) {
                        if (early && (slot == 6 || slot == 7) && verticalAt) {
                            reportLegacyField(reporting, key, verticalSource, verticalSource.rowOfWord(verticalRows, *verticalAt), name,
                                verticalSource.byteOffsetInRow(*verticalAt * 2), word(slot));
                        } else if (early && earlySlots[slot] < 0) {
                            reportFallbackField(reporting, key, name, Reporting::Origin::Finale27Default, 0);
                        } else {
                            const auto sourceSlot = at + (early ? static_cast<std::size_t>(earlySlots[slot]) : slot);
                            reportLegacyField(
                                reporting, key, source, source.rowOfWord(rows, sourceSlot), name, source.byteOffsetInRow(sourceSlot * 2), word(slot));
                        }
                    };
                    stored("noteId", 0);
                    stored("xStart", 1);
                    stored("xEnd", 2);
                    stored("yStart", 6);
                    stored("yEnd", 7);
                    stored("outerLocal", 3);
                    stored("outerOn", 3);
                    stored("afterSingleDotLocal", 3);
                    stored("afterSingleDotOn", 3);
                    stored("afterMultiDotsLocal", 3);
                    stored("afterMultiDotsOn", 3);
                    stored("shiftForSecondsLocal", 3);
                    stored("shiftForSecondsOn", 3);
                    stored("beforeSingleAcciLocal", 3);
                    stored("beforeSingleAcciOn", 3);
                    stored("down", 4);
                    stored("breakTimeLocal", 4);
                    stored("breakTimeOn", 4);
                    stored("breakKeyLocal", 4);
                    stored("breakKeyOn", 4);
                    stored("freezeDirection", 4);
                    stored("noSpecialArc", 4);
                    stored("stPtAdjOn", 8);
                    stored("stPtEntCnct", 8);
                    stored("stPtContext", 8);
                    stored("enPtAdjOn", 9);
                    stored("enPtEntCnct", 9);
                    stored("enPtContext", 9);
                    stored("insetRatio1", 10);
                    stored("height1", 11);
                    stored("insetRatio2", 12);
                    stored("height2", 13);
                    stored("ctlPtAdjOn", 14);
                    stored("ctlPtFixed", 14);
                });
                registerNoteIdRemap(
                    context, target, static_cast<musx::dom::NoteNumber Target::*>(&Target::noteId), combinedNoteReferenceMode(context.profile));
                incidence.add(*context.document->getDetails(), std::move(target));
            }
        }
    }
}

} // namespace

void importTieAlterEnd(const ImportContext& context)
{
    importTieAlterFamily<musx::dom::details::TieAlterEnd>(context, "Te", 0x042e, "te");
}

void importTieAlterStart(const ImportContext& context)
{
    importTieAlterFamily<musx::dom::details::TieAlterStart>(context, "Ts", 0x042f, "ts");
}

} // namespace details
} // namespace finale_mus_reader
