// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

constexpr std::size_t namePositionPayloadSize = 12;
constexpr std::size_t namePositionHorzOffset = 0;
constexpr std::size_t namePositionVertOffset = 2;
constexpr std::size_t namePositionFlagsOffset = 10;
constexpr std::uint16_t earlyNamePositionJustificationMask = 0x0003;
constexpr std::uint16_t earlyNamePositionIndividualMask = 0x0004;
constexpr std::uint16_t namePositionJustificationMask = 0x0007;
constexpr std::uint16_t namePositionIndividualMask = 0x0008;
constexpr std::uint16_t namePositionAlignmentMask = 0x0030;
constexpr std::uint16_t namePositionHiddenMask = 0x0040;
constexpr std::uint16_t namePositionExpandMask = 0x8000;

bool sourceUsesPreFinale37NamePositionLayout(const SourceProfile& profile)
{
    return sourcePredatesVersion(profile, FormatEpoch::UncompressedLegacy, versions::finale3_7);
}

template <typename Target>
void reportNamePosition(
    const ImportContext& context, const Target& target, const records::LegacyRow& row, records::LegacyTag identity, bool preFinale37Layout)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<Target>(target.getSourcePartId(), target.getCmper());
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        const auto report = [&](const char* member, std::size_t offset, std::int64_t value) {
            reporting.report().setField(key, member,
                typename Reporting::FieldInfo{Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset + offset, value, identity});
        };
        report("horzOff", namePositionHorzOffset, target.horzOff);
        report("vertOff", namePositionVertOffset, target.vertOff);
        report("justify", namePositionFlagsOffset, static_cast<std::int64_t>(target.justify));
        report("indivPos", namePositionFlagsOffset, target.indivPos);
        report("hAlign", namePositionFlagsOffset, static_cast<std::int64_t>(target.hAlign));
        if (preFinale37Layout) {
            reporting.report().setField(key, "expand", {Reporting::Origin::LegacyBehavior, 0, 0, target.expand});
        } else {
            report("expand", namePositionFlagsOffset, target.expand);
        }
        if (preFinale37Layout) {
            reporting.report().setField(key, "hidden", {Reporting::Origin::LegacyBehavior, 0, 0, target.hidden});
        } else {
            report("hidden", namePositionFlagsOffset, target.hidden);
        }
    });
}

template <typename Target>
void importNamePosition(const ImportContext& context, const char* fixedTag, records::LegacyTag classId)
{
    const auto source =
        selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), records::packTag(fixedTag), classId);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto payload = collectRecordPayload(*source, rows);
        if (payload.size() < namePositionPayloadSize) {
            context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
                "Name positioning record " + std::to_string(cmper) + " is shorter than its 12-byte layout and was ignored."});
            continue;
        }
        const auto& row = rows.front();
        auto target = createOthersRecordTarget<Target>(context.document, *source, row, cmper);
        target->horzOff = static_cast<std::int16_t>(payloadWord(payload, namePositionHorzOffset, context.profile.byteOrder));
        target->vertOff = static_cast<std::int16_t>(payloadWord(payload, namePositionVertOffset, context.profile.byteOrder));
        const auto flags = payloadWord(payload, namePositionFlagsOffset, context.profile.byteOrder);
        const auto preFinale37Layout = sourceUsesPreFinale37NamePositionLayout(context.profile);
        if (preFinale37Layout) {
            target->justify = static_cast<musx::dom::AlignJustify>(flags & earlyNamePositionJustificationMask);
            target->indivPos = (flags & earlyNamePositionIndividualMask) != 0;
            target->hAlign = target->justify;
            target->expand = true;
            target->hidden = false;
        } else {
            target->justify = static_cast<musx::dom::AlignJustify>(flags & namePositionJustificationMask);
            target->indivPos = (flags & namePositionIndividualMask) != 0;
            target->hAlign = static_cast<musx::dom::AlignJustify>((flags & namePositionAlignmentMask) >> 4U);
            target->hidden = (flags & namePositionHiddenMask) != 0;
            target->expand = (flags & namePositionExpandMask) != 0;
        }
        reportNamePosition(context, *target, row, source->identity, preFinale37Layout);
        context.document->getOthers()->add(Target::XmlNodeName, std::move(target));
    }
}

} // namespace

void importNamePositionAbbreviated(const ImportContext& context)
{
    importNamePosition<musx::dom::others::NamePositionAbbreviated>(context, "ns", 0x00b3);
}

void importNamePositionFull(const ImportContext& context)
{
    importNamePosition<musx::dom::others::NamePositionFull>(context, "NS", 0x00b5);
}

void importNamePositionStyleAbbreviated(const ImportContext& context)
{
    importNamePosition<musx::dom::others::NamePositionStyleAbbreviated>(context, "NY", 0x00b4);
}

void importNamePositionStyleFull(const ImportContext& context)
{
    importNamePosition<musx::dom::others::NamePositionStyleFull>(context, "Ny", 0x00b6);
}

} // namespace others
} // namespace finale_mus_reader
