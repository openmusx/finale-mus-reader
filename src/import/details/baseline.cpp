// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/details.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace details {
namespace {

constexpr std::size_t baselineStructSize = 10;
constexpr std::size_t baselineDisplacementOffset = 0;
constexpr std::size_t baselineLyricNumberOffset = 4;
constexpr records::LegacyTag extendedVerseBase = 0x8000;
constexpr records::LegacyTag extendedChorusBase = 0x9000;
constexpr records::LegacyTag extendedSectionBase = 0xa000;
constexpr records::LegacyTag extendedLyricNumberMask = 0x0fff;

template <typename T>
constexpr bool baselineHasLyricNumber =
    std::is_same_v<T, musx::dom::details::BaselineLyricsChorus> || std::is_same_v<T, musx::dom::details::BaselineLyricsSection>
    || std::is_same_v<T, musx::dom::details::BaselineLyricsVerse> || std::is_same_v<T, musx::dom::details::BaselineSystemLyricsChorus>
    || std::is_same_v<T, musx::dom::details::BaselineSystemLyricsSection> || std::is_same_v<T, musx::dom::details::BaselineSystemLyricsVerse>;

template <typename T>
void reportBaseline(const ImportContext& context, const T& target, const records::LegacyRow& row, std::size_t payloadOffset)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<T>(target.getSourcePartId(), target.getCmper1(), target.getInci(), target.getCmper2());
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        reporting.report().setField(key, "baselineDisplacement",
            {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset + payloadOffset + baselineDisplacementOffset,
                target.baselineDisplacement, row.tag});
        if constexpr (baselineHasLyricNumber<T>) {
            reporting.report().setField(key, "lyricNumber",
                {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset + payloadOffset + baselineLyricNumberOffset,
                    target.lyricNumber.value_or(0), row.tag});
        } else {
            reporting.report().setField(key, "lyricNumber", {Reporting::Origin::LegacyBehavior, 0, 0, 0});
        }
    });
}

template <typename T, typename FixedSystemT = T>
void importBaselineFamily(const ImportContext& context, records::LegacyTag fixedTag, records::LegacyTag classId)
{
    const auto source = selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), fixedTag, classId, true);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper1] : recordKeys(*source)) {
        for (const auto cmper2 : source->pool->secondCmpersForTag(source->identity, cmper1, partId)) {
            const auto rows = source->pool->getArray(source->identity, cmper1, cmper2, partId);
            if (rows.empty()) {
                continue;
            }
            const auto payload = collectRecordPayload(*source, rows);
            if (payload.empty() || payload.size() % baselineStructSize != 0) {
                context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
                    "Baseline detail for " + std::to_string(cmper1) + ", " + std::to_string(cmper2) + " has an incomplete struct."});
                continue;
            }
            const auto itemCount = payload.size() / baselineStructSize;
            if constexpr (!baselineHasLyricNumber<T>) {
                if (itemCount != 1) {
                    context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
                        "Baseline detail for " + std::to_string(cmper1) + ", " + std::to_string(cmper2) + " has multiple structs."});
                    continue;
                }
            }
            for (std::size_t index = 0; index < itemCount; ++index) {
                const auto offset = index * baselineStructSize;
                const auto inci = static_cast<musx::dom::Inci>(index);
                const auto& row = source->classRecords ? rows.front() : rows[index];
                if constexpr (!std::is_same_v<T, FixedSystemT>) {
                    // Believed: fixed-row baseline families use a nonzero first comparator for a system baseline.
                    if (!source->classRecords && cmper1 != 0) {
                        auto target = createDetailsRecordTarget<FixedSystemT>(context.document, *source, row, cmper1, cmper2, inci);
                        target->baselineDisplacement = payloadLong(
                            payload, offset + baselineDisplacementOffset, context.profile.byteOrder, nativeLongWordOrder(context.profile.byteOrder));
                        if constexpr (baselineHasLyricNumber<FixedSystemT>) {
                            target->lyricNumber = payloadWord(payload, offset + baselineLyricNumberOffset, context.profile.byteOrder);
                        }
                        reportBaseline(context, *target, row, 0);
                        context.document->getDetails()->add(FixedSystemT::XmlNodeName, std::move(target));
                        continue;
                    }
                }
                auto target = createDetailsRecordTarget<T>(context.document, *source, row, cmper1, cmper2, inci);
                target->baselineDisplacement = payloadLong(
                    payload, offset + baselineDisplacementOffset, context.profile.byteOrder, nativeLongWordOrder(context.profile.byteOrder));
                if constexpr (baselineHasLyricNumber<T>) {
                    target->lyricNumber = payloadWord(payload, offset + baselineLyricNumberOffset, context.profile.byteOrder);
                }
                reportBaseline(context, *target, row, source->classRecords ? offset : 0);
                context.document->getDetails()->add(T::XmlNodeName, std::move(target));
            }
        }
    }
}

template <typename T, typename FixedSystemT = T>
void importExtendedLyricBaselineFamily(const ImportContext& context, records::LegacyTag familyBase)
{
    if (context.profile.epoch == FormatEpoch::ZlibLegacy) {
        return;
    }
    const auto& pool = context.index.getDetails();
    std::map<std::tuple<std::uint16_t, musx::dom::Cmper, musx::dom::Cmper>, musx::dom::Inci> nextInci;
    const auto firstTag = static_cast<records::LegacyTag>(familyBase | 1);
    const auto lastTag = static_cast<records::LegacyTag>(familyBase | extendedLyricNumberMask);
    for (const auto tag : pool.tagsInRange(firstTag, lastTag)) {
        const auto storedNumber = static_cast<records::LegacyTag>(tag & extendedLyricNumberMask);
        const RecordFamilySource source{&pool, tag, false, true};
        for (const auto& [partId, cmper1] : recordKeys(source)) {
            for (const auto cmper2 : pool.secondCmpersForTag(tag, cmper1, partId)) {
                const auto rows = pool.getArray(tag, cmper1, cmper2, partId);
                if (rows.size() != 1 || rows.front().payloadSize != baselineStructSize) {
                    context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
                        "Extended lyric baseline for " + std::to_string(cmper1) + ", " + std::to_string(cmper2) + " has an incomplete struct."});
                    continue;
                }
                const auto& row = rows.front();
                const auto payload = pool.effectivePayloadOf(row);
                const auto key = std::tuple{partId, cmper1, cmper2};
                const auto inci = nextInci[key]++;
                if constexpr (!std::is_same_v<T, FixedSystemT>) {
                    if (cmper1 != 0) {
                        auto target = createDetailsRecordTarget<FixedSystemT>(context.document, source, row, cmper1, cmper2, inci);
                        target->baselineDisplacement = payloadLong(
                            payload, baselineDisplacementOffset, context.profile.byteOrder, nativeLongWordOrder(context.profile.byteOrder));
                        target->lyricNumber = storedNumber;
                        reportBaseline(context, *target, row, 0);
                        context.document->getDetails()->add(FixedSystemT::XmlNodeName, std::move(target));
                        continue;
                    }
                }
                auto target = createDetailsRecordTarget<T>(context.document, source, row, cmper1, cmper2, inci);
                target->baselineDisplacement =
                    payloadLong(payload, baselineDisplacementOffset, context.profile.byteOrder, nativeLongWordOrder(context.profile.byteOrder));
                target->lyricNumber = storedNumber;
                reportBaseline(context, *target, row, 0);
                context.document->getDetails()->add(T::XmlNodeName, std::move(target));
            }
        }
    }
}

template <typename T>
void synthesizeMissingGlobalBaselines(const ImportContext& context)
{
    constexpr musx::dom::Cmper global = 0;
    if (!context.document->getDetails()->getArray<T>(musx::dom::SCORE_PARTID, global, global).empty()) {
        return;
    }
    const auto defaults = context.referenceDocument->getDetails()->getArray<T>(musx::dom::SCORE_PARTID, global, global);
    if (defaults.empty()) {
        throw std::logic_error("Pinned defaults contain no requested global baselines");
    }
    for (const auto& source : defaults) {
        std::shared_ptr<T> target;
        if constexpr (baselineHasLyricNumber<T>) {
            target = std::make_shared<T>(
                context.document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, global, global, source->getInci());
        } else {
            target = std::make_shared<T>(context.document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, global, global);
        }
        target->baselineDisplacement = source->baselineDisplacement;
        target->lyricNumber = source->lyricNumber;
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            const auto key =
                reporting.template instanceKey<T>(target->getSourcePartId(), target->getCmper1(), target->getInci(), target->getCmper2());
            reporting.report().setInstanceOrigin(key, Reporting::Origin::Finale27Default);
            reporting.report().setField(key, "baselineDisplacement", {Reporting::Origin::Finale27Default, 0, 0, target->baselineDisplacement});
            if constexpr (baselineHasLyricNumber<T>) {
                reporting.report().setField(key, "lyricNumber", {Reporting::Origin::Finale27Default, 0, 0, target->lyricNumber.value_or(0)});
            } else {
                reporting.report().setField(key, "lyricNumber", {Reporting::Origin::LegacyBehavior, 0, 0, 0});
            }
        });
        context.document->getDetails()->add(T::XmlNodeName, std::move(target));
    }
}

} // namespace

void importBaselines(const ImportContext& context)
{
    // Believed: a selector uses the same single-incidence struct whenever it is present in a fixed-row epoch.
    importBaselineFamily<musx::dom::details::BaselineChords, musx::dom::details::BaselineSystemChords>(context, records::packTag("CL"), 0x03f2);
    importBaselineFamily<musx::dom::details::BaselineExpressionsAbove, musx::dom::details::BaselineSystemExpressionsAbove>(
        context, records::packTag("XA"), 0x03f3);
    importBaselineFamily<musx::dom::details::BaselineExpressionsBelow, musx::dom::details::BaselineSystemExpressionsBelow>(
        context, records::packTag("XB"), 0x03f4);
    importBaselineFamily<musx::dom::details::BaselineFretboards, musx::dom::details::BaselineSystemFretboards>(
        context, records::packTag("GL"), 0x03f5);
    importBaselineFamily<musx::dom::details::BaselineLyricsChorus, musx::dom::details::BaselineSystemLyricsChorus>(
        context, records::packTag("Bc"), 0x03f6);
    importBaselineFamily<musx::dom::details::BaselineLyricsSection, musx::dom::details::BaselineSystemLyricsSection>(
        context, records::packTag("Bs"), 0x03f7);
    importBaselineFamily<musx::dom::details::BaselineLyricsVerse, musx::dom::details::BaselineSystemLyricsVerse>(
        context, records::packTag("Bv"), 0x03f8);
    importBaselineFamily<musx::dom::details::BaselineSystemChords>(context, records::packTag("Cl"), 0x0443);
    importBaselineFamily<musx::dom::details::BaselineSystemExpressionsAbove>(context, records::packTag("Xa"), 0x0444);
    importBaselineFamily<musx::dom::details::BaselineSystemExpressionsBelow>(context, records::packTag("Xb"), 0x0445);
    importBaselineFamily<musx::dom::details::BaselineSystemFretboards>(context, records::packTag("Gl"), 0x0446);
    importBaselineFamily<musx::dom::details::BaselineSystemLyricsChorus>(context, records::packTag("bc"), 0x0447);
    importBaselineFamily<musx::dom::details::BaselineSystemLyricsSection>(context, records::packTag("BS"), 0x0448);
    importBaselineFamily<musx::dom::details::BaselineSystemLyricsVerse>(context, records::packTag("bv"), 0x0449);
    importExtendedLyricBaselineFamily<musx::dom::details::BaselineLyricsVerse, musx::dom::details::BaselineSystemLyricsVerse>(
        context, extendedVerseBase);
    importExtendedLyricBaselineFamily<musx::dom::details::BaselineLyricsChorus, musx::dom::details::BaselineSystemLyricsChorus>(
        context, extendedChorusBase);
    importExtendedLyricBaselineFamily<musx::dom::details::BaselineLyricsSection, musx::dom::details::BaselineSystemLyricsSection>(
        context, extendedSectionBase);
    synthesizeMissingGlobalBaselines<musx::dom::details::BaselineExpressionsAbove>(context);
    synthesizeMissingGlobalBaselines<musx::dom::details::BaselineExpressionsBelow>(context);
}

} // namespace details
} // namespace finale_mus_reader
