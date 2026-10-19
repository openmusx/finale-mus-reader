// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace custom_keys_internal {

// Named rather than anonymous: ReportState<CustomKeyMapReportData<Reporting>> holds this by
// value and has external linkage, and under the unity build GCC rejects an anonymous-namespace
// member there (-Wsubobject-linkage). The name stays distinctive for the same reason every
// file-local name does.

template <typename Reporting>
struct CustomKeyMapReportData
{
    RecordFamilySource source;
    std::vector<records::LegacyRow> rows;
};

} // namespace custom_keys_internal

namespace {

using custom_keys_internal::CustomKeyMapReportData;

template <typename Target>
void reportCustomKeyValue(const ImportContext& context, const Target& target, const RecordFamilySource& source, const records::LegacyRow& row,
    std::string_view member, std::size_t byteOffset, std::int64_t rawValue)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<Target>(target.getSourcePartId(), target.getCmper());
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        reporting.report().setField(
            key, std::string(member), {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset + byteOffset, rawValue, source.identity});
    });
}

template <typename Target>
void reportCustomKeyArray(
    const ImportContext& context, const Target& target, const RecordFamilySource& source, std::span<const records::LegacyRow> rows, std::size_t count)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting&) {
        for (std::size_t index = 0; index < count; ++index) {
            reportCustomKeyValue(context, target, source, source.rowOfWord(rows, index), "values[" + std::to_string(index) + "]",
                source.byteOffsetInRow(index * 2), target.values[index]);
        }
    });
}

template <typename Target, std::size_t Capacity>
void importCustomKeyArray(const ImportContext& context, records::LegacyTag tag, records::LegacyTag classId)
{
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), tag, classId);
    if (!source) {
        return;
    }
    using Element = typename std::remove_reference_t<decltype(std::declval<Target>().values)>::value_type;
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto words = collectRecordWords(*source, rows, context.profile.byteOrder);
        if (words.size() < Capacity) {
            continue;
        }
        auto target = createOthersRecordTarget<Target>(context.document, *source, rows.front(), cmper);
        if (!target) {
            continue;
        }
        target->values.reserve(Capacity);
        for (std::size_t index = 0; index < Capacity; ++index) {
            target->values.push_back(static_cast<Element>(words[index]));
        }
        reportCustomKeyArray(context, *target, *source, rows, Capacity);
        context.document->getOthers()->add(Target::XmlNodeName, std::move(target));
    }
}

constexpr auto acciAmountFlatsTag = records::packTag("An");
constexpr auto acciAmountSharpsTag = records::packTag("Ap");
constexpr auto acciOrderFlatsTag = records::packTag("On");
constexpr auto acciOrderSharpsTag = records::packTag("Op");
constexpr auto tonalCenterFlatsTag = records::packTag("Tn");
constexpr auto tonalCenterSharpsTag = records::packTag("Tp");
constexpr auto keyAttributesTag = records::packTag("KA");
constexpr auto keyFormatTag = records::packTag("KF");
constexpr auto keyMapTag = records::packTag("KM");

constexpr records::LegacyTag acciAmountFlatsClass = 0x0072;
constexpr records::LegacyTag acciAmountSharpsClass = 0x0073;
constexpr records::LegacyTag acciOrderFlatsClass = 0x0074;
constexpr records::LegacyTag acciOrderSharpsClass = 0x0075;
constexpr records::LegacyTag tonalCenterFlatsClass = 0x009a;
constexpr records::LegacyTag tonalCenterSharpsClass = 0x009b;
constexpr records::LegacyTag keyFormatClass = 0x00a0;
constexpr records::LegacyTag keyMapClass = 0x00a1;
constexpr records::LegacyTag keyAttributesClass = 0x00a2;

struct CustomKeyMapWordIndices
{
    std::size_t hlevel;
    std::size_t flag;
};

CustomKeyMapWordIndices customKeyMapWordIndices(ByteOrder byteOrder, std::size_t firstWord)
{
  // Believed: key-map pair order follows source byte order across record
  // framings. Big-endian records put the flag first; little-endian records
  // put the harmonic level first.
    if (byteOrder == ByteOrder::LittleEndian) {
        return {firstWord, firstWord + 1};
    }
    return {firstWord + 1, firstWord};
}

void normalizeClefOctaveFlag(const ImportContext& context, const std::shared_ptr<musx::dom::others::KeyAttributes>& target)
{
  // Matching detail records prove the flag is active. Their absence does not
  // disprove a stored true flag, so normalization is deliberately one-way.
    if (target->hasClefOctv) {
        return;
    }
    const auto details = context.document->getDetails();
    const auto partId = target->getSourcePartId();
    const auto cmper = target->getCmper();
    const auto hasClefOctaves = !details->getArray<musx::dom::details::ClefOctaveFlats>(partId, cmper).empty()
                                || !details->getArray<musx::dom::details::ClefOctaveSharps>(partId, cmper).empty();
    if (!hasClefOctaves) {
        return;
    }
    target->hasClefOctv = true;
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<musx::dom::others::KeyAttributes>(partId, cmper);
        if (auto* info = reporting.report().findField(key, "hasClefOctv")) {
            info->origin = Reporting::Origin::LegacyMusAdjusted;
        }
    });
}

} // namespace

void importAcciAmountFlats(const ImportContext& context)
{
    importCustomKeyArray<musx::dom::others::AcciAmountFlats, 7>(context, acciAmountFlatsTag, acciAmountFlatsClass);
}

void importAcciAmountSharps(const ImportContext& context)
{
    importCustomKeyArray<musx::dom::others::AcciAmountSharps, 7>(context, acciAmountSharpsTag, acciAmountSharpsClass);
}

void importAcciOrderFlats(const ImportContext& context)
{
    importCustomKeyArray<musx::dom::others::AcciOrderFlats, 7>(context, acciOrderFlatsTag, acciOrderFlatsClass);
}

void importAcciOrderSharps(const ImportContext& context)
{
    importCustomKeyArray<musx::dom::others::AcciOrderSharps, 7>(context, acciOrderSharpsTag, acciOrderSharpsClass);
}

void importKeyAttributes(const ImportContext& context)
{
    using Target = musx::dom::others::KeyAttributes;
    const auto source =
        selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), keyAttributesTag, keyAttributesClass);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto words = collectRecordWords(*source, rows, context.profile.byteOrder);
        if (words.size() < 6) {
            continue;
        }
        auto target = createOthersRecordTarget<Target>(context.document, *source, rows.front(), cmper);
        if (!target) {
            continue;
        }
        target->harmRefer = words[0];
        target->middleCKey = words[1];
        target->fontSym = static_cast<musx::dom::Cmper>(words[2]);
        target->gotoKey = words[3];
        target->symbolList = static_cast<musx::dom::Cmper>(words[4]);
        target->hasClefOctv = (static_cast<std::uint16_t>(words[5]) & 0x0001U) != 0;
        const auto& row = rows.front();
        const auto report = [&](const char* member, std::size_t index, std::int64_t value) {
            reportCustomKeyValue(context, *target, *source, row, member, index * 2, value);
        };
        report("harmRefer", 0, target->harmRefer);
        report("middleCKey", 1, target->middleCKey);
        report("fontSym", 2, target->fontSym);
        report("gotoKey", 3, target->gotoKey);
        report("symbolList", 4, target->symbolList);
        report("hasClefOctv", 5, target->hasClefOctv);
        context.document->getOthers()->add(Target::XmlNodeName, target);
        context.pending.defer(DeferredStage::ResolveStoredRefs, [&context, target] { normalizeClefOctaveFlag(context, target); });
    }
}

void importKeyFormats(const ImportContext& context)
{
    using Target = musx::dom::others::KeyFormat;
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), keyFormatTag, keyFormatClass);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto words = collectRecordWords(*source, rows, context.profile.byteOrder);
        if (words.size() < 2) {
            continue;
        }
        auto target = createOthersRecordTarget<Target>(context.document, *source, rows.front(), cmper);
        if (!target) {
            continue;
        }
        target->semitones = static_cast<std::uint16_t>(words[0]);
        target->scaleTones = static_cast<std::uint16_t>(words[1]);
        reportCustomKeyValue(context, *target, *source, rows.front(), "semitones", 0, target->semitones);
        reportCustomKeyValue(context, *target, *source, rows.front(), "scaleTones", 2, target->scaleTones);
        context.document->getOthers()->add(Target::XmlNodeName, std::move(target));
    }
}

void importKeyMapArrays(const ImportContext& context)
{
    using Target = musx::dom::others::KeyMapArray;
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), keyMapTag, keyMapClass);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto words = collectRecordWords(*source, rows, context.profile.byteOrder);
        auto target = createOthersRecordTarget<Target>(context.document, *source, rows.front(), cmper);
        if (!target) {
            continue;
        }
        for (std::size_t index = 0; index + 1 < words.size(); index += 2) {
            auto step = std::make_shared<Target::StepElement>();
            const auto indices = customKeyMapWordIndices(context.profile.byteOrder, index);
            step->hlevel = static_cast<std::uint16_t>(words[indices.hlevel]);
            step->diatonic = (static_cast<std::uint16_t>(words[indices.flag]) & 0x8000U) != 0;
            target->steps.push_back(std::move(step));
        }
        context.document->getOthers()->add(Target::XmlNodeName, target);
        ReportState<CustomKeyMapReportData> reportRows;
        withReporting(context.report, [&](auto& reporting) {
            auto& data = reporting.state(reportRows);
            data.source = *source;
            data.rows.assign(rows.begin(), rows.end());
        });
        context.pending.defer(DeferredStage::ResolveStoredRefs, [&context, target, reportRows = std::move(reportRows)] {
            const auto format = context.document->getOthers()->get<musx::dom::others::KeyFormat>(target->getSourcePartId(), target->getCmper());
            if (format && format->semitones < target->steps.size()) {
                target->steps.resize(format->semitones);
            }
            withReporting(context.report, [&](auto& reporting) {
                const auto& data = reporting.state(reportRows);
                const auto& source = data.source;
                const auto& rows = data.rows;
                for (std::size_t stepIndex = 0; stepIndex < target->steps.size(); ++stepIndex) {
                    const auto firstWord = stepIndex * 2;
                    const auto indices = customKeyMapWordIndices(context.profile.byteOrder, firstWord);
                    const auto reportStepWord = [&](std::string member, std::size_t wordIndex, std::int64_t value) {
                        reportCustomKeyValue(context, *target, source, source.rowOfWord(rows, wordIndex), std::move(member),
                            source.byteOffsetInRow(wordIndex * 2), value);
                    };
                    reportStepWord("steps[" + std::to_string(stepIndex) + "].hlevel", indices.hlevel, target->steps[stepIndex]->hlevel);
                    reportStepWord("steps[" + std::to_string(stepIndex) + "].diatonic", indices.flag, target->steps[stepIndex]->diatonic);
                }
            });
        });
    }
}

void importTonalCenterFlats(const ImportContext& context)
{
    importCustomKeyArray<musx::dom::others::TonalCenterFlats, 8>(context, tonalCenterFlatsTag, tonalCenterFlatsClass);
}

void importTonalCenterSharps(const ImportContext& context)
{
    importCustomKeyArray<musx::dom::others::TonalCenterSharps, 8>(context, tonalCenterSharpsTag, tonalCenterSharpsClass);
}

} // namespace others
} // namespace finale_mus_reader
