// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/details.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "import/shared/barline_type.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace details {
namespace {

using StaffGroupTarget = musx::dom::details::StaffGroup;
using BracketTarget = musx::dom::details::Bracket;

constexpr auto staffGroupTag = records::packTag("NG");
constexpr records::LegacyTag staffGroupClass = 0x0421;
constexpr auto spanStaffGroupTag = records::packTag("GS");
constexpr auto spanMemberTag = records::packTag("IU");
constexpr std::size_t spanStaffGroupWordCount = 6;
constexpr std::size_t spanMemberBytes = 6;
constexpr std::size_t spanMemberStaffOffset = 0;
constexpr std::size_t spanMemberIdOffset = 2;
constexpr std::size_t staffGroupBaseWordCount = 15;
constexpr std::size_t staffGroupExtendedWordCount = 18;

constexpr std::uint16_t bracketOnSingleMask = 0x0001;
constexpr std::uint16_t fullNameJustifyMask = 0x0007;
constexpr std::uint16_t abbrvNameJustifyMask = 0x0038;
constexpr unsigned abbrvNameJustifyShift = 3;
constexpr std::uint16_t barlineTypeMask = 0x03c0;
constexpr unsigned barlineTypeShift = 6;
constexpr std::uint16_t drawBarlinesMask = 0x0c00;
constexpr std::uint16_t throughStavesValue = 0x0400;
constexpr std::uint16_t mensurstricheValue = 0x0800;
constexpr std::uint16_t ownBarlineMask = 0x1000;
constexpr std::uint16_t fullNameIndivPosMask = 0x2000;
constexpr std::uint16_t abbrvNameIndivPosMask = 0x4000;
constexpr std::uint16_t hideNameMask = 0x8000;
constexpr std::uint16_t fullNameAlignMask = 0x0003;
constexpr std::uint16_t abbrvNameAlignMask = 0x000c;
constexpr unsigned abbrvNameAlignShift = 2;
constexpr std::uint16_t abbrvNameExpandMask = 0x4000;
constexpr std::uint16_t fullNameExpandMask = 0x8000;
constexpr std::uint16_t hideStavesMask = 0x1800;
constexpr std::uint16_t hideStavesAsGroupValue = 0x0800;
constexpr std::uint16_t hideStavesNeverValue = 0x1000;

struct StaffGroupSpanMember
{
    std::uint16_t partId{};
    musx::dom::Cmper spanId{};
    musx::dom::StaffCmper staffId{};
    std::size_t blockOffset{};
    std::size_t decodedOffset{};
    records::LegacyTag identity{};
};

[[nodiscard]] bool hasStaffGroupSpans(const records::LegacyRecordIndex& index)
{
    return !index.getOthers().cmpersForTag(spanStaffGroupTag).empty();
}

[[nodiscard]] std::vector<StaffGroupSpanMember> collectStaffGroupSpanMembers(const ImportContext& context)
{
    const RecordFamilySource source{&context.index.getOthers(), spanMemberTag, false, false};
    std::vector<StaffGroupSpanMember> result;
    for (const auto& [partId, cmper] : recordKeys(source)) {
        if (cmper != musx::dom::BASE_SYSTEM_ID) {
            continue;
        }
        const auto rows = source.pool->getArray(source.identity, cmper, 0, partId);
        const auto payload = collectRecordPayload(source, rows);
        const auto completeSize = payload.size() - payload.size() % spanMemberBytes;
        for (std::size_t offset = 0; offset < completeSize; offset += spanMemberBytes) {
            const auto staffId = static_cast<musx::dom::StaffCmper>(payloadWord(payload, offset + spanMemberStaffOffset, context.profile.byteOrder));
            const auto spanId = static_cast<musx::dom::Cmper>(payloadWord(payload, offset + spanMemberIdOffset, context.profile.byteOrder));
            if (staffId == 0 || spanId == 0) {
                continue;
            }
            const auto& row = source.rowOfByte(rows, offset + spanMemberStaffOffset);
            result.push_back({partId, spanId, staffId, row.blockOffset, row.decodedOffset + source.byteOffsetInRow(offset + spanMemberStaffOffset),
                source.identity});
        }
    }
    return result;
}

[[nodiscard]] musx::dom::AlignJustify alignJustify(std::uint16_t value)
{
    using Result = musx::dom::AlignJustify;
    switch (value) {
    case 1: return Result::Right;
    case 2: return Result::Center;
    default: return Result::Left;
    }
}

[[nodiscard]] std::optional<BracketTarget::BracketStyle> spanBracketStyle(std::int16_t value)
{
    using Result = BracketTarget::BracketStyle;
    switch (value) {
    case 1: return Result::ThickLine;
    case 2: return Result::BracketStraightHooks;
    case 3: return Result::PianoBrace;
    // Finale 3.2 adds the curved-hook bracket.
    case 6: return Result::BracketCurvedHooks;
    default: return std::nullopt;
    }
}

[[nodiscard]] StaffGroupTarget::DrawBarlineStyle drawBarlineStyle(std::uint16_t flags)
{
    switch (flags & drawBarlinesMask) {
    case throughStavesValue: return StaffGroupTarget::DrawBarlineStyle::ThroughStaves;
    case mensurstricheValue: return StaffGroupTarget::DrawBarlineStyle::Mensurstriche;
    default: return StaffGroupTarget::DrawBarlineStyle::OnlyOnStaves;
    }
}

template <typename Reporting>
void reportStaffGroupField(Reporting& reporting, const typename Reporting::InstanceKey& key, const char* member, std::int64_t rawValue,
    const RecordFamilySource& source, std::span<const records::LegacyRow> rows, std::size_t slot, typename Reporting::Origin origin)
{
    const auto& row = source.rowOfWord(rows, slot);
    reporting.report().setField(
        key, member, {origin, row.blockOffset, row.decodedOffset + source.byteOffsetInRow(slot * sizeof(std::uint16_t)), rawValue, source.identity});
}

void reportStaffGroup(const ImportContext& context, const RecordFamilySource& source, std::span<const records::LegacyRow> rows,
    std::span<const std::int16_t> words, const StaffGroupTarget& target, bool hasMeasureRange, bool hasExtendedLayout, bool hasHideStaves,
    bool invalidHideStaves)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key =
            reporting.template instanceKey<StaffGroupTarget>(target.getSourcePartId(), target.getCmper1(), std::nullopt, target.getCmper2());
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        const auto legacy = [&](const char* member, std::size_t slot) {
            reportStaffGroupField(reporting, key, member, words[slot], source, rows, slot, Reporting::Origin::LegacyMus);
        };

        legacy("startInst", 0);
        legacy("endInst", 1);
        legacy("fullNameId", 2);
        legacy("fullNameXadj", 3);
        legacy("fullNameYadj", 4);
        legacy("bracket.style", 5);
        legacy("bracket.horzAdjLeft", 6);
        legacy("bracket.vertAdjTop", 7);
        legacy("bracket.vertAdjBot", 8);
        legacy("bracket.showOnSingleStaff", 9);
        legacy("barlineType", 10);
        legacy("fullNameJustify", 10);
        legacy("abbrvNameJustify", 10);
        legacy("drawBarlines", 10);
        legacy("ownBarline", 10);
        legacy("fullNameIndivPos", 10);
        legacy("abbrvNameIndivPos", 10);
        legacy("hideName", 10);
        legacy("abbrvNameId", 11);
        legacy("abbrvNameXadj", 12);
        legacy("abbrvNameYadj", 13);
        legacy("fullNameAlign", 14);
        legacy("abbrvNameAlign", 14);
        legacy("fullNameExpand", 14);
        legacy("abbrvNameExpand", 14);

        if (hasMeasureRange) {
            legacy("startMeas", 16);
            legacy("endMeas", 17);
        } else {
            reporting.report().setField(key, "startMeas", {Reporting::Origin::LegacyBehavior, 0, 0, target.startMeas});
            reporting.report().setField(key, "endMeas", {Reporting::Origin::LegacyBehavior, 0, 0, target.endMeas});
        }
        if (hasExtendedLayout) {
            legacy("customBarShape", 15);
        } else {
            reporting.unmappedField(key, "customBarShape", target.customBarShape);
        }
        if (hasHideStaves) {
            reportStaffGroupField(reporting, key, "hideStaves", words[14], source, rows, 14,
                invalidHideStaves ? Reporting::Origin::LegacyMusAdjusted : Reporting::Origin::LegacyMus);
        } else {
            reporting.report().setField(key, "hideStaves", {Reporting::Origin::Finale27Default, 0, 0, static_cast<std::int64_t>(target.hideStaves)});
        }
    });
}

void reportSpanStaffGroup(const ImportContext& context, const RecordFamilySource& source, std::span<const records::LegacyRow> rows,
    std::span<const std::int16_t> words, const StaffGroupSpanMember& firstMember, const StaffGroupSpanMember& lastMember,
    const StaffGroupTarget& target)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key =
            reporting.template instanceKey<StaffGroupTarget>(target.getSourcePartId(), target.getCmper1(), std::nullopt, target.getCmper2());
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        const auto legacy = [&](const char* member, std::size_t slot) {
            reportStaffGroupField(reporting, key, member, words[slot], source, rows, slot, Reporting::Origin::LegacyMus);
        };
        const auto behavior = [&](const char* member, std::int64_t value) {
            reporting.report().setField(key, member, {Reporting::Origin::LegacyBehavior, 0, 0, value});
        };

        const auto member = [&](const char* name, const StaffGroupSpanMember& value) {
            reporting.report().setField(
                key, name, {Reporting::Origin::LegacyMus, value.blockOffset, value.decodedOffset, value.staffId, value.identity});
        };

        member("startInst", firstMember);
        member("endInst", lastMember);
        behavior("startMeas", target.startMeas);
        behavior("endMeas", target.endMeas);
        behavior("fullNameId", target.fullNameId);
        behavior("fullNameXadj", target.fullNameXadj);
        behavior("fullNameYadj", target.fullNameYadj);
        legacy("bracket.style", 0);
        legacy("bracket.horzAdjLeft", 1);
        legacy("bracket.vertAdjTop", 2);
        legacy("bracket.vertAdjBot", 3);
        behavior("bracket.showOnSingleStaff", target.bracket->showOnSingleStaff);
        behavior("barlineType", static_cast<std::int64_t>(target.barlineType));
        behavior("fullNameJustify", static_cast<std::int64_t>(target.fullNameJustify));
        behavior("abbrvNameJustify", static_cast<std::int64_t>(target.abbrvNameJustify));
        behavior("drawBarlines", static_cast<std::int64_t>(target.drawBarlines));
        behavior("ownBarline", target.ownBarline);
        behavior("fullNameIndivPos", target.fullNameIndivPos);
        behavior("abbrvNameIndivPos", target.abbrvNameIndivPos);
        behavior("hideName", target.hideName);
        behavior("abbrvNameId", target.abbrvNameId);
        behavior("abbrvNameXadj", target.abbrvNameXadj);
        behavior("abbrvNameYadj", target.abbrvNameYadj);
        behavior("fullNameAlign", static_cast<std::int64_t>(target.fullNameAlign));
        behavior("abbrvNameAlign", static_cast<std::int64_t>(target.abbrvNameAlign));
        behavior("fullNameExpand", target.fullNameExpand);
        behavior("abbrvNameExpand", target.abbrvNameExpand);
        reporting.report().setField(key, "hideStaves", {Reporting::Origin::Finale27Default, 0, 0, static_cast<std::int64_t>(target.hideStaves)});
        reporting.unmappedField(key, "customBarShape", target.customBarShape);
    });
}

void importSpanStaffGroups(const ImportContext& context)
{
    const RecordFamilySource source{&context.index.getOthers(), spanStaffGroupTag, false, false};
    const auto members = collectStaffGroupSpanMembers(context);
    musx::dom::Cmper targetGroupId = 1;
    std::uint16_t currentPartId = (std::numeric_limits<std::uint16_t>::max)();
    for (const auto& [partId, spanId] : recordKeys(source)) {
        if (partId != currentPartId) {
            currentPartId = partId;
            targetGroupId = 1;
        }
        if (spanId == 0) {
            continue;
        }
        const auto rows = source.pool->getArray(source.identity, spanId, 0, partId);
        const auto firstMember = std::find_if(
            members.begin(), members.end(), [partId, spanId](const auto& member) { return member.partId == partId && member.spanId == spanId; });
        if (firstMember == members.end()) {
            // A span outside Scroll View is either unreferenced or used only by system-specific
            // staff lists, and only Scroll View groups are imported before Finale 2011.
            targetGroupId = static_cast<musx::dom::Cmper>(targetGroupId + rows.size());
            continue;
        }
        const auto lastMember = std::find_if(
            members.rbegin(), members.rend(), [partId, spanId](const auto& member) { return member.partId == partId && member.spanId == spanId; });
        for (const auto& row : rows) {
            const auto rowSpan = std::span<const records::LegacyRow>(&row, 1);
            const auto words = collectRecordWords(source, rowSpan, context.profile.byteOrder);
            const auto groupId = targetGroupId++;
            if (words.size() < spanStaffGroupWordCount) {
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Info, "Span-based staff group " + std::to_string(groupId) + " has a truncated record."});
                continue;
            }
            const auto style = spanBracketStyle(words[0]);
            if (!style) {
                context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
                    "Span-based staff group " + std::to_string(groupId) + " has unrecognized bracket style " + std::to_string(words[0]) + "."});
            }
            auto target = createDetailsRecordTarget<StaffGroupTarget>(context.document, source, row, musx::dom::BASE_SYSTEM_ID, groupId);
            target->startInst = firstMember->staffId;
            target->endInst = lastMember->staffId;
            target->startMeas = 1;
            target->endMeas = (std::numeric_limits<musx::dom::MeasCmper>::max)();
            target->bracket = std::make_shared<BracketTarget>(target->getDocument());
            target->bracket->style = style.value_or(BracketTarget::BracketStyle::None);
            target->bracket->horzAdjLeft = words[1];
            target->bracket->vertAdjTop = words[2];
            target->bracket->vertAdjBot = words[3];
            target->bracket->showOnSingleStaff = true;
            target->barlineType = StaffGroupTarget::BarlineType::Normal;
            target->drawBarlines = StaffGroupTarget::DrawBarlineStyle::ThroughStaves;
            reportSpanStaffGroup(context, source, rowSpan, words, *firstMember, *lastMember, *target);
            context.document->getDetails()->add(StaffGroupTarget::XmlNodeName, std::move(target));
        }
    }
}

bool populateStaffGroup(StaffGroupTarget& target, std::span<const std::int16_t> words, bool hasMeasureRange, bool hasHideStaves)
{
    target.startInst = words[0];
    target.endInst = words[1];
    target.fullNameId = words[2];
    target.fullNameXadj = words[3];
    target.fullNameYadj = words[4];
    target.bracket = std::make_shared<BracketTarget>(target.getDocument());
    target.bracket->style = static_cast<BracketTarget::BracketStyle>(words[5]);
    target.bracket->horzAdjLeft = words[6];
    target.bracket->vertAdjTop = words[7];
    target.bracket->vertAdjBot = words[8];
    target.bracket->showOnSingleStaff = (static_cast<std::uint16_t>(words[9]) & bracketOnSingleMask) != 0;

    const auto flags = static_cast<std::uint16_t>(words[10]);
    target.barlineType = barlineTypeOf((flags & barlineTypeMask) >> barlineTypeShift);
    target.fullNameJustify = alignJustify(flags & fullNameJustifyMask);
    target.abbrvNameJustify = alignJustify((flags & abbrvNameJustifyMask) >> abbrvNameJustifyShift);
    target.drawBarlines = drawBarlineStyle(flags);
    target.ownBarline = (flags & ownBarlineMask) != 0;
    target.fullNameIndivPos = (flags & fullNameIndivPosMask) != 0;
    target.abbrvNameIndivPos = (flags & abbrvNameIndivPosMask) != 0;
    target.hideName = (flags & hideNameMask) != 0;

    target.abbrvNameId = words[11];
    target.abbrvNameXadj = words[12];
    target.abbrvNameYadj = words[13];
    const auto auxFlags = static_cast<std::uint16_t>(words[14]);
    target.fullNameAlign = alignJustify(auxFlags & fullNameAlignMask);
    target.abbrvNameAlign = alignJustify((auxFlags & abbrvNameAlignMask) >> abbrvNameAlignShift);
    target.fullNameExpand = (auxFlags & fullNameExpandMask) != 0;
    target.abbrvNameExpand = (auxFlags & abbrvNameExpandMask) != 0;
    bool invalidHideStaves = false;
    if (hasHideStaves) {
        switch (auxFlags & hideStavesMask) {
        case hideStavesAsGroupValue: target.hideStaves = StaffGroupTarget::HideStaves::AsGroup; break;
        case hideStavesNeverValue: target.hideStaves = StaffGroupTarget::HideStaves::None; break;
        case hideStavesMask: invalidHideStaves = true; break;
        default: break;
        }
    }

    if (words.size() > staffGroupBaseWordCount) {
        target.customBarShape = words[15];
    }
    if (hasMeasureRange) {
        target.startMeas = words[16];
        target.endMeas = words[17];
    } else {
        target.startMeas = 1;
        target.endMeas = (std::numeric_limits<musx::dom::MeasCmper>::max)();
    }
    return invalidHideStaves;
}

void importStaffGroupFamily(const ImportContext& context, const RecordFamilySource& source)
{
    const bool hasMeasureRange = sourceAtOrAfter(context.profile, FormatEpoch::ZlibLegacy, versions::finale2011);
    const bool hasHideStaves = sourceAtOrAfter(context.profile, FormatEpoch::DclLegacy, versions::finale2003);
    for (const auto& [partId, sourceCmper1] : recordKeys(source)) {
        if (!hasMeasureRange && sourceCmper1 != musx::dom::BASE_SYSTEM_ID) {
            continue;
        }
        for (const auto groupId : source.pool->secondCmpersForTag(source.identity, sourceCmper1, partId)) {
            if (groupId == 0) {
                continue;
            }
            const auto rows = source.pool->getArray(source.identity, sourceCmper1, groupId, partId);
            const auto words = collectRecordWords(source, rows, context.profile.byteOrder);
            const auto requiredWords = hasMeasureRange ? staffGroupExtendedWordCount : staffGroupBaseWordCount;
            if (words.size() < requiredWords) {
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Info, "Staff group " + std::to_string(groupId) + " has a truncated record."});
                continue;
            }
            auto target = createDetailsRecordTarget<StaffGroupTarget>(context.document, source, rows.front(), sourceCmper1, groupId);
            const bool invalidHideStaves = populateStaffGroup(*target, words, hasMeasureRange, hasHideStaves);
            if (invalidHideStaves) {
                context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
                    "Staff group " + std::to_string(groupId) + " has both hide-staves flags set; using Normally."});
            }
            reportStaffGroup(
                context, source, rows, words, *target, hasMeasureRange, words.size() > staffGroupBaseWordCount, hasHideStaves, invalidHideStaves);
            context.document->getDetails()->add(StaffGroupTarget::XmlNodeName, std::move(target));
        }
    }
}

} // namespace

void importStaffGroups(const ImportContext& context)
{
    if (hasStaffGroupSpans(context.index)) {
        importSpanStaffGroups(context);
        return;
    }
    const auto source =
        selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), staffGroupTag, staffGroupClass, true);
    if (source) {
        importStaffGroupFamily(context, *source);
    }
}

} // namespace details
} // namespace finale_mus_reader
