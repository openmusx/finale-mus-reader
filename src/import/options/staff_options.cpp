// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/options.h"

#include <array>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace options {
namespace {

using StaffOptionsTarget = musx::dom::options::StaffOptions;
using StaffNamePositioning = musx::dom::others::NamePositioning;
using StaffNamePositionMember = std::shared_ptr<StaffNamePositioning> StaffOptionsTarget::*;

constexpr std::uint16_t staffScalarSelector = 97;
constexpr std::size_t staffScalarWordCount = 15;
constexpr musx::dom::Evpu preFinale2008StaffSeparation = -320;
constexpr musx::dom::Evpu codaStaffNameHorzOff = -192;
constexpr musx::dom::Evpu codaStaffNameVertOff = -27;

bool sourceStoresStaffScalars(const records::LegacyRecordIndex& index, const SourceProfile& profile)
{
    return profile.epoch == FormatEpoch::ZlibLegacy && readGlobalWords(index, profile, staffScalarSelector).words.size() >= staffScalarWordCount;
}

const FieldMapping staffScalarFields[]{
    MUS_CLASS_WORD(StaffOptionsTarget, numericGlobalClass(staffScalarSelector), GLOBALS_CMPER, classWordOffset(12), staffSeparation),
    MUS_CLASS_WORD(StaffOptionsTarget, numericGlobalClass(staffScalarSelector), GLOBALS_CMPER, classWordOffset(13), staffSeparIncr),
    MUS_CLASS_WORD(StaffOptionsTarget, numericGlobalClass(staffScalarSelector), GLOBALS_CMPER, classWordOffset(14), autoAdjustStaffSepar),
};

const MappingTable& staffScalarTable()
{
    // The record payload states whether this zlib-era layout carries the scalar tail.
    static const MappingTable table{.reportPrefix = "options.staffOptions",
        .epochs = EpochMask::Zlib,
        .applies = &sourceStoresStaffScalars,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OptionsSingleton,
        .enumerateTargets = &enumerateOptionsTarget<StaffOptionsTarget>,
        .fields = staffScalarFields,
        .fieldCount = std::size(staffScalarFields)};
    return table;
}

struct StaffNamePositionSource
{
    std::uint16_t selector;
    StaffNamePositionMember target;
    std::string_view member;
    std::size_t flagSlot;
    std::uint8_t justificationBitCount;
    std::uint8_t alignmentFirstBit;
};

constexpr std::array staffNamePositionSources{
    StaffNamePositionSource{4, &StaffOptionsTarget::namePos, "namePos", 5, 2, 4},
    StaffNamePositionSource{66, &StaffOptionsTarget::namePosAbbrv, "namePosAbbrv", 5, 2, 4},
    StaffNamePositionSource{79, &StaffOptionsTarget::groupNameFullPos, "groupNameFullPos", 2, 3, 3},
    StaffNamePositionSource{80, &StaffOptionsTarget::groupNameAbbrvPos, "groupNameAbbrvPos", 2, 3, 3},
};

constexpr std::uint8_t namePositionAlignmentBitCount = 2;
constexpr std::uint8_t namePositionExpandBit = 15;
constexpr std::int16_t earlyStaffNameVerticalEfixPerPoint = 3;

std::int64_t extractStaffNamePositionBits(std::uint16_t value, std::uint8_t firstBit, std::uint8_t bitCount)
{
    const auto mask = (std::uint16_t{1} << bitCount) - 1U;
    return static_cast<std::int64_t>((value >> firstBit) & mask);
}

template <typename Reporting>
void reportStaffNamePositionField(Reporting& reporting, const ImportContext& context, std::string member, typename Reporting::Origin origin,
    std::int64_t rawValue, const GlobalSelectorWords* source = nullptr, std::uint16_t selector = 0)
{
    typename Reporting::FieldInfo info{origin, source ? source->blockOffset : 0, source ? source->decodedOffset : 0, rawValue};
    if (source) {
        info.sourceIdentity = context.profile.epoch == FormatEpoch::ZlibLegacy ? numericGlobalClass(selector) : numericGlobalTag(selector);
    }
    reporting.report().setField(reporting.template instanceKey<StaffOptionsTarget>(), std::move(member), std::move(info));
}

void reportStaffNamePositionUnavailable(const ImportContext& context, const StaffNamePositionSource& descriptor, const StaffNamePositioning& position)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto reportDefault = [&](std::string_view leaf, std::int64_t value) {
            reportStaffNamePositionField(
                reporting, context, std::string(descriptor.member).append(".").append(leaf), Reporting::Origin::Finale27Default, value);
        };
        reportDefault("horzOff", position.horzOff);
        reportDefault("vertOff", position.vertOff);
        reportDefault("justify", static_cast<std::int64_t>(position.justify));
        reportDefault("hAlign", static_cast<std::int64_t>(position.hAlign));
        reportDefault("expand", position.expand);
    });
}

void reportRecoveredStaffNamePosition(const ImportContext& context, const StaffNamePositionSource& descriptor, const GlobalSelectorWords& source,
    const StaffNamePositioning& position, bool earlyStaffLayout, std::int64_t justification, std::int64_t alignment, bool validJustification,
    bool validAlignment, musx::dom::AlignJustify previousAlignment)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto reportRecovered = [&](std::string_view leaf, std::int64_t value) {
            reportStaffNamePositionField(reporting, context, std::string(descriptor.member).append(".").append(leaf), Reporting::Origin::LegacyMus,
                value, &source, descriptor.selector);
        };
        reportRecovered("horzOff", source.words[0]);
        reportStaffNamePositionField(reporting, context, std::string(descriptor.member).append(".vertOff"),
            earlyStaffLayout ? Reporting::Origin::LegacyMusAdjusted : Reporting::Origin::LegacyMus, position.vertOff, &source, descriptor.selector);
        if (validJustification) {
            reportRecovered("justify", justification);
        } else {
            reportStaffNamePositionField(reporting, context, std::string(descriptor.member).append(".justify"), Reporting::Origin::Finale27Default,
                static_cast<std::int64_t>(position.justify));
        }
        if (validAlignment) {
            auto alignmentOrigin = Reporting::Origin::LegacyMus;
            if (earlyStaffLayout) {
                alignmentOrigin = previousAlignment == position.hAlign ? Reporting::Origin::Finale27Default : Reporting::Origin::LegacyBehavior;
            }
            reportStaffNamePositionField(reporting, context, std::string(descriptor.member).append(".hAlign"), alignmentOrigin, alignment,
                earlyStaffLayout ? nullptr : &source, earlyStaffLayout ? 0 : descriptor.selector);
        } else {
            reportStaffNamePositionField(reporting, context, std::string(descriptor.member).append(".hAlign"), Reporting::Origin::Finale27Default,
                static_cast<std::int64_t>(position.hAlign));
        }
        if (earlyStaffLayout) {
            reportStaffNamePositionField(
                reporting, context, std::string(descriptor.member).append(".expand"), Reporting::Origin::Finale27Default, position.expand);
        } else {
            reportRecovered("expand", position.expand);
        }
    });
}

void importStaffNamePosition(const ImportContext& context, StaffOptionsTarget& target, const StaffNamePositionSource& descriptor,
    bool recoverSource = true, bool earlyStaffLayout = false)
{
    auto& position = target.*descriptor.target;
    if (!position) {
        position = std::make_shared<StaffNamePositioning>(context.document);
    }

    const auto source = recoverSource ? readGlobalWords(context.index, context.profile, descriptor.selector) : GlobalSelectorWords{};
    if (!source.present || source.words.size() <= descriptor.flagSlot) {
        reportStaffNamePositionUnavailable(context, descriptor, *position);
    } else {
        position->horzOff = source.words[0];
        const auto flags = static_cast<std::uint16_t>(source.words[descriptor.flagSlot]);
        const auto justification = extractStaffNamePositionBits(flags, 0, descriptor.justificationBitCount);
        const auto lastAlignJustify = static_cast<std::int64_t>(musx::dom::AlignJustify::Center);
        const bool validJustification = justification <= lastAlignJustify;
        const auto previousAlignment = position->hAlign;
        const auto alignment =
            earlyStaffLayout ? justification : extractStaffNamePositionBits(flags, descriptor.alignmentFirstBit, namePositionAlignmentBitCount);
        const bool validAlignment = alignment <= lastAlignJustify;
        if (earlyStaffLayout) {
            // The earlier record stores a font tuple rather than packed alignment and expand
            // flags. Its vertical conversion depends on unavailable font metrics, so use a
            // uniform point-size approximation.
            position->vertOff = source.words[1] + earlyStaffNameVerticalEfixPerPoint * source.words[3];
        } else {
            position->vertOff = source.words[1];
        }
        if (validJustification) {
            position->justify = static_cast<musx::dom::AlignJustify>(justification);
        }
        if (validAlignment) {
            position->hAlign = static_cast<musx::dom::AlignJustify>(alignment);
        }
        if (!earlyStaffLayout) {
            position->expand = extractStaffNamePositionBits(flags, namePositionExpandBit, 1) != 0;
        }

        reportRecoveredStaffNamePosition(context, descriptor, source, *position, earlyStaffLayout, justification, alignment, validJustification,
            validAlignment, previousAlignment);
    }

    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        reportStaffNamePositionField(
            reporting, context, std::string(descriptor.member).append(".indivPos"), Reporting::Origin::Finale27Default, position->indivPos);
        reportStaffNamePositionField(
            reporting, context, std::string(descriptor.member).append(".hidden"), Reporting::Origin::Finale27Default, position->hidden);
    });
}

void applyCodaStaffNameBehavior(const ImportContext& context, StaffOptionsTarget& target)
{
    // Coda has no editable name-position preferences. The fixed staff-name
    // behavior replaces the apparent selector values; matching seeded leaves remain defaults.
    for (const auto member : {&StaffOptionsTarget::namePos, &StaffOptionsTarget::namePosAbbrv}) {
        auto& position = target.*member;
        position->horzOff = codaStaffNameHorzOff;
        position->vertOff = codaStaffNameVertOff;
        position->justify = musx::dom::AlignJustify::Left;
        position->hAlign = musx::dom::AlignJustify::Left;

        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            const auto prefix = member == &StaffOptionsTarget::namePos ? "namePos." : "namePosAbbrv.";
            reportStaffNamePositionField(
                reporting, context, std::string(prefix).append("horzOff"), Reporting::Origin::LegacyBehavior, position->horzOff);
            reportStaffNamePositionField(
                reporting, context, std::string(prefix).append("vertOff"), Reporting::Origin::LegacyBehavior, position->vertOff);
            reportStaffNamePositionField(reporting, context, std::string(prefix).append("justify"), Reporting::Origin::LegacyBehavior,
                static_cast<std::int64_t>(position->justify));
            reportStaffNamePositionField(reporting, context, std::string(prefix).append("hAlign"), Reporting::Origin::LegacyBehavior,
                static_cast<std::int64_t>(position->hAlign));
        });
    }
}

} // namespace

void importStaffOptions(const ImportContext& context)
{
    const auto pooled = context.document->getOptions()->get<StaffOptionsTarget>();
    if (!pooled) {
        return;
    }
    const auto target = std::const_pointer_cast<StaffOptionsTarget>(pooled);

    applyMappingTables({&staffScalarTable()}, context.index, context.profile, context.document, context.report);

    // Before the scalar tail exists, Finale uses a fixed staff separation. A
    // structurally present tail supersedes this source-era behavior.
    if (!sourceStoresStaffScalars(context.index, context.profile)) {
        target->staffSeparation = preFinale2008StaffSeparation;
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            reporting.template behaviorField<StaffOptionsTarget>("staffSeparation", preFinale2008StaffSeparation);
        });
    }

    const auto recoverNamePositionSource = context.profile.epoch != FormatEpoch::CodaBanner;
    const auto hasGroupNamePositionLayout =
        readGlobalWords(context.index, context.profile, 79).present && readGlobalWords(context.index, context.profile, 80).present;
    for (const auto& descriptor : staffNamePositionSources) {
        const auto earlyStaffLayout =
            context.profile.epoch == FormatEpoch::UncompressedLegacy && !hasGroupNamePositionLayout
            && (descriptor.target == &StaffOptionsTarget::namePos || descriptor.target == &StaffOptionsTarget::namePosAbbrv);
        importStaffNamePosition(context, *target, descriptor, recoverNamePositionSource, earlyStaffLayout);
    }
    if (!recoverNamePositionSource) {
        applyCodaStaffNameBehavior(context, *target);
    }
}

} // namespace options
} // namespace finale_mus_reader
