// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <utility>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using TimeUpperTarget = musx::dom::others::TimeCompositeUpper;
using TimeLowerTarget = musx::dom::others::TimeCompositeLower;

constexpr records::LegacyTag timeUpperTag = records::packTag("TU");
constexpr records::LegacyTag timeUpperClass = 0x00ee;
constexpr records::LegacyTag timeLowerTag = records::packTag("TL");
constexpr records::LegacyTag timeLowerClass = 0x00ed;

/// Decodes one packed fraction word: numerator in the high byte, denominator in the low byte.
/// Zero means no fraction.
musx::util::Fraction timeUpperFraction(std::uint16_t packed)
{
    const auto denominator = packed & 0xffU;
    return denominator == 0 ? musx::util::Fraction(0) : musx::util::Fraction(packed >> 8U, static_cast<int>(denominator));
}

// A composite list is an open-ended array of fixed-size items filling whole incidences. An
// all-zero item pads out the last incidence and is not an item.
//
// From Finale 3.5, which introduced groups, an upper item is three words (beats, packed
// fraction, startGroup) and a lower item two (Edu unit, startGroup). Earlier releases store no
// startGroup word: an upper item is two words (beats, packed fraction) and a lower item one.
// Their lists form a single group, so the first item starts it.
template <typename Target>
void importTimeCompositeFamily(const ImportContext& context, records::LegacyTag tag, records::LegacyTag classId)
{
    constexpr bool upper = std::is_same_v<Target, TimeUpperTarget>;
    const bool storesGroups = !sourcePredatesVersion(context.profile, FormatEpoch::UncompressedLegacy, versions::finale3_5);
    const std::size_t itemWords = (upper ? 2 : 1) + (storesGroups ? 1 : 0);
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), tag, classId);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto words = collectRecordWords(*source, rows, context.profile.byteOrder);
        if (words.size() % itemWords != 0) {
            context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
                std::string(Target::XmlNodeName) + " " + std::to_string(cmper) + " has an incomplete trailing item."});
        }
        auto target = createOthersRecordTarget<Target>(context.document, *source, rows.front(), cmper);
        for (std::size_t at = 0; at + itemWords <= words.size(); at += itemWords) {
            const std::span<const std::int16_t> item(words.data() + at, itemWords);
            if (std::ranges::all_of(item, [](std::int16_t word) { return word == 0; })) {
                continue;
            }
            auto next = std::make_shared<typename Target::CompositeItem>();
            if constexpr (upper) {
                next->beats = item[0];
                next->fraction = timeUpperFraction(static_cast<std::uint16_t>(item[1]));
            } else {
                next->unit = item[0];
            }
            next->startGroup = storesGroups ? item[itemWords - 1] != 0 : target->items.empty();
            target->items.push_back(next);

            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<Target>(partId, cmper);
                const auto& row = source->rowOfWord(rows, at);
                const auto prefix = "items[" + std::to_string(target->items.size() - 1) + "].";
                const auto reportField = [&](const char* member, std::size_t word, std::int64_t value) {
                    reporting.report().setField(key, prefix + member,
                        {Reporting::Origin::LegacyMus, row.blockOffset,
                            row.decodedOffset + source->byteOffsetInRow((at + word) * sizeof(std::uint16_t)), value, source->identity});
                };
                if constexpr (upper) {
                    reportField("beats", 0, item[0]);
                    reportField("fraction", 1, static_cast<std::uint16_t>(item[1]));
                } else {
                    reportField("unit", 0, item[0]);
                }
                if (storesGroups) {
                    reportField("startGroup", itemWords - 1, item[itemWords - 1]);
                } else {
                    reporting.report().setField(key, prefix + "startGroup", {Reporting::Origin::LegacyBehavior, 0, 0, next->startGroup});
                }
            });
        }
        if (target->items.empty()) {
            continue;
        }
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            reporting.report().setInstanceOrigin(reporting.template instanceKey<Target>(partId, cmper), Reporting::Origin::LegacyMus);
        });
        context.document->getOthers()->add(Target::XmlNodeName, std::move(target));
    }
}

} // namespace

void importTimeCompositeLowers(const ImportContext& context)
{
    importTimeCompositeFamily<TimeLowerTarget>(context, timeLowerTag, timeLowerClass);
}

void importTimeCompositeUppers(const ImportContext& context)
{
    importTimeCompositeFamily<TimeUpperTarget>(context, timeUpperTag, timeUpperClass);
}

} // namespace others
} // namespace finale_mus_reader
