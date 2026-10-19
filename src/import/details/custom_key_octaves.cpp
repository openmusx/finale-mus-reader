// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/details.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace details {
namespace {

constexpr auto clefOctaveFlatsTag = records::packTag("Cn");
constexpr auto clefOctaveSharpsTag = records::packTag("Cp");
constexpr records::LegacyTag clefOctaveFlatsClass = 0x0408;
constexpr records::LegacyTag clefOctaveSharpsClass = 0x0409;

template <typename Target>
void importClefOctaveArrays(const ImportContext& context, records::LegacyTag tag, records::LegacyTag classId)
{
    const auto source = selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), tag, classId, true);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper1] : recordKeys(*source)) {
        for (const auto cmper2 : source->pool->secondCmpersForTag(source->identity, cmper1, partId)) {
            const auto rows = source->pool->getArray(source->identity, cmper1, cmper2, partId);
            if (rows.empty()) {
                continue;
            }
            const auto words = collectRecordWords(*source, rows, context.profile.byteOrder);
            if (words.size() < 7) {
                continue;
            }
            auto target = createDetailsRecordTarget<Target>(context.document, *source, rows.front(), cmper1, cmper2);
            if (!target) {
                continue;
            }
            target->values.assign(words.begin(), words.begin() + 7);
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<Target>(partId, cmper1, std::nullopt, cmper2);
                reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
                for (std::size_t index = 0; index < target->values.size(); ++index) {
                    const auto& row = source->rowOfWord(rows, index);
                    const auto byteOffset = source->byteOffsetInRow(index * 2);
                    reporting.report().setField(key, "values[" + std::to_string(index) + "]",
                        {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset + byteOffset, target->values[index], source->identity});
                }
            });
            context.document->getDetails()->add(Target::XmlNodeName, std::move(target));
        }
    }
}

} // namespace

void importClefOctaveFlats(const ImportContext& context)
{
    importClefOctaveArrays<musx::dom::details::ClefOctaveFlats>(context, clefOctaveFlatsTag, clefOctaveFlatsClass);
}

void importClefOctaveSharps(const ImportContext& context)
{
    importClefOctaveArrays<musx::dom::details::ClefOctaveSharps>(context, clefOctaveSharpsTag, clefOctaveSharpsClass);
}

} // namespace details
} // namespace finale_mus_reader
