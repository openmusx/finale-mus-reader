// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

constexpr std::size_t placementSize = 12;
constexpr std::uint16_t placementHidden = 0x0001;

enum class PlacementKind {
    Back,
    EndingLine,
    EndingText,
    TextRepeat
};

template <typename Target>
void reportPlacement(const ImportContext& context, const RecordFamilySource& source, const records::LegacyRow& row, const Target& target,
    PlacementKind kind, bool modernFlags, std::size_t offset)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<Target>(row.partId, target.getCmper(), target.getInci());
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        const auto legacy = [&](const char* name, std::size_t at, std::int64_t value) {
            reportLegacyField(reporting, key, source, row, name, offset + at, value);
        };
        const auto fallback = [&](const char* name, std::int64_t value) {
            reportFallbackField(reporting, key, name, Reporting::Origin::LegacyBehavior, value);
        };
        legacy("staffId", 0, target.staffId);
        if (kind == PlacementKind::TextRepeat) {
            legacy("measureId", 2, target.measureId);
        } else {
            fallback("measureId", target.measureId);
        }
        // Ending text follows its associated repeat's visibility and has no independent flag.
        if (kind == PlacementKind::EndingText || !modernFlags) {
            fallback("hidden", target.hidden);
        } else {
            legacy("hidden", kind == PlacementKind::TextRepeat ? 10 : 2, target.hidden);
        }
        legacy("x1add", 4, target.x1add);
        legacy("y1add", 6, target.y1add);
        if (kind != PlacementKind::TextRepeat) {
            legacy("x2add", 8, target.x2add);
            legacy("y2add", 10, target.y2add);
        } else {
            fallback("x2add", target.x2add);
            fallback("y2add", target.y2add);
        }
    });
}

template <typename Target>
void importPlacements(const ImportContext& context, const char* tag, records::LegacyTag classId, PlacementKind kind)
{
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), records::packTag(tag), classId);
    if (!source) {
        return;
    }
    // Zlib placement rows reuse variant-specific words for visibility flags.
    const bool modernFlags = source->classRecords;
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        for (const auto& row : rows) {
            const auto payload = source->pool->effectivePayloadOf(row);
            if (payload.size() < placementSize || payload.size() % placementSize != 0) {
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Info, "Individual positioning record " + std::to_string(cmper) + " has an incomplete placement."});
                continue;
            }
            for (std::size_t offset = 0; offset < payload.size(); offset += placementSize) {
                const auto word = [&](std::size_t at) { return payloadWord(payload, offset + at, context.profile.byteOrder); };
                const auto signedWord = [&](std::size_t at) { return static_cast<std::int16_t>(word(at)); };
                const auto inci = static_cast<musx::dom::Inci>(row.inci + offset / placementSize);
                auto target = createOthersRecordTarget<Target>(context.document, *source, row, cmper, inci);
                target->staffId = word(0);
                if (kind == PlacementKind::TextRepeat) {
                    target->measureId = word(2);
                }
                target->x1add = signedWord(4);
                target->y1add = signedWord(6);
                if (kind != PlacementKind::TextRepeat) {
                    target->x2add = signedWord(8);
                    target->y2add = signedWord(10);
                }
                if (kind == PlacementKind::Back || kind == PlacementKind::EndingLine) {
                    target->hidden = modernFlags && (word(2) & placementHidden) != 0;
                } else if (kind == PlacementKind::TextRepeat) {
                    target->hidden = modernFlags && (word(10) & placementHidden) != 0;
                }
                reportPlacement(context, *source, row, *target, kind, modernFlags, offset);
                context.document->getOthers()->add(Target::XmlNodeName, std::move(target));
            }
        }
    }
}

} // namespace

void importRepeatBackIndividualPositioning(const ImportContext& context)
{
    importPlacements<musx::dom::others::RepeatBackIndividualPositioning>(context, "BI", 0x00d1, PlacementKind::Back);
}

void importRepeatEndingStartIndividualPositioning(const ImportContext& context)
{
    importPlacements<musx::dom::others::RepeatEndingStartIndividualPositioning>(context, "EI", 0x00d2, PlacementKind::EndingLine);
}

void importRepeatEndingTextIndividualPositioning(const ImportContext& context)
{
    importPlacements<musx::dom::others::RepeatEndingTextIndividualPositioning>(context, "LI", 0x00d3, PlacementKind::EndingText);
}

void importTextRepeatIndividualPositioning(const ImportContext& context)
{
    importPlacements<musx::dom::others::TextRepeatIndividualPositioning>(context, "RI", 0x00d4, PlacementKind::TextRepeat);
}

} // namespace others
} // namespace finale_mus_reader
