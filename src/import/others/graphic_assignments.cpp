// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "import/shared/graphic_assignment.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using PageTarget = musx::dom::others::PageGraphicAssign;
using ShapeTarget = musx::dom::others::ShapeGraphicAssign;

constexpr auto pageGraphicTag = records::packTag("pg");
constexpr auto shapeGraphicTag = records::packTag("sg");
constexpr records::LegacyTag pageGraphicClass = 0x00bc;
constexpr records::LegacyTag shapeGraphicClass = 0x00d8;

template <typename Target, typename Reporting>
void reportAssignmentValue(Reporting& reporting, musx::dom::Cmper cmper, musx::dom::Inci inci, std::uint16_t partId, std::string member,
    std::int64_t value, const records::LegacyRow& row)
{
    reporting.report().setField(reporting.template instanceKey<Target>(partId, cmper, inci), std::move(member),
        {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset, value});
}

template <typename Target, typename Reporting>
void reportPositionValues(Reporting& reporting, musx::dom::Cmper cmper, musx::dom::Inci inci, std::uint16_t partId, std::string_view prefix,
    std::int64_t value, const records::LegacyRow& row, bool hasPositionFrom)
{
    const auto memberName = [prefix](std::string_view suffix) {
        if (!prefix.empty()) {
            return std::string(prefix) + std::string(suffix);
        }
        auto result = std::string(suffix);
        result.front() = static_cast<char>(result.front() - 'A' + 'a');
        return result;
    };
    reportAssignmentValue<Target>(reporting, cmper, inci, partId, memberName("HAlign"), value, row);
    reportAssignmentValue<Target>(reporting, cmper, inci, partId, memberName("VAlign"), value, row);
    if (hasPositionFrom) {
        reportAssignmentValue<Target>(reporting, cmper, inci, partId, memberName("PosFrom"), value, row);
    }
    reportAssignmentValue<Target>(reporting, cmper, inci, partId, memberName("FixedPerc"), value, row);
}

void populatePagePosition(PageTarget& target, std::uint16_t packed, bool right)
{
    if (!right) {
        populateGraphicAssignmentPosition<true>(target, packed);
        return;
    }
    using H = PageTarget::HorizontalAlignment;
    using V = PageTarget::VerticalAlignment;
    switch (packed & 0x0007U) {
    case 0x0001: target.rightPgHAlign = H::Left; break;
    case 0x0002: target.rightPgHAlign = H::Right; break;
    case 0x0004: target.rightPgHAlign = H::Center; break;
    default: break;
    }
    switch (packed & 0x0038U) {
    case 0x0008: target.rightPgVAlign = V::Top; break;
    case 0x0010: target.rightPgVAlign = V::Bottom; break;
    case 0x0020: target.rightPgVAlign = V::Center; break;
    default: break;
    }
    target.rightPgFixedPerc = (packed & 0x0100U) != 0;
    if ((packed & 0x0080U) != 0) {
        target.rightPgPosFrom = PageTarget::PositionFrom::PageEdge;
    } else if ((packed & 0x0040U) != 0) {
        target.rightPgPosFrom = PageTarget::PositionFrom::Margins;
    }
}

PageTarget::PageAssignType pageAssignType(std::uint16_t raw)
{
    // Page selection occupies a one-hot low nibble; visibility is an independent bit in the
    // same display-flags word.
    switch (raw & 0x000fU) {
    case 1: return PageTarget::PageAssignType::One;
    case 2: return PageTarget::PageAssignType::AllPages;
    case 4: return PageTarget::PageAssignType::Odd;
    case 8: return PageTarget::PageAssignType::Even;
    default: return PageTarget::PageAssignType::AllPages;
    }
}

void importPageFamily(const ImportContext& context)
{
    const auto source =
        selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), pageGraphicTag, pageGraphicClass);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        const auto words = collectRecordWords(*source, rows, context.profile.byteOrder);
        if (words.size() % graphicAssignmentWordCount != 0) {
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Info, "Page graphic assignment " + std::to_string(cmper) + " has an incomplete trailing tuple."});
        }
        for (std::size_t at = 0; at + graphicAssignmentWordCount <= words.size(); at += graphicAssignmentWordCount) {
            const auto inci = static_cast<musx::dom::Inci>(at / graphicAssignmentWordCount);
            auto target = createOthersRecordTarget<PageTarget>(context.document, *source, rows.front(), cmper, inci);
            if (!target) {
                continue;
            }
            const std::span<const std::int16_t> tuple(words.data() + at, graphicAssignmentWordCount);
            populateGraphicAssignmentCommon(*target, tuple);
            target->displayType = pageAssignType(static_cast<std::uint16_t>(tuple[7]));
            populatePagePosition(*target, static_cast<std::uint16_t>(tuple[8]), false);
            target->startPage = static_cast<musx::dom::PageCmper>(tuple[9]);
            target->endPage = static_cast<musx::dom::PageCmper>(tuple[10]);
            target->rightPgLeft = tuple[14];
            target->rightPgBottom = tuple[15];
            populatePagePosition(*target, static_cast<std::uint16_t>(tuple[16]), true);
            constexpr const char* names[] = {"version", "left", "bottom", "width", "height", "fDescId", nullptr, "displayType", nullptr, "startPage",
                "endPage", "savedRecord", "origWidth", "origHeight", "rightPgLeft", "rightPgBottom", nullptr, "graphicCmper"};
            for (std::size_t slot = 0; slot < graphicAssignmentWordCount; ++slot) {
                if (!names[slot]) {
                    continue;
                }
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                    reportAssignmentValue<PageTarget>(reporting, cmper, inci, partId, names[slot], tuple[slot], source->rowOfWord(rows, at + slot));
                });
            }
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                reportAssignmentValue<PageTarget>(reporting, cmper, inci, partId, "hidden", tuple[7], source->rowOfWord(rows, at + 7));
                reportPositionValues<PageTarget>(reporting, cmper, inci, partId, "", tuple[8], source->rowOfWord(rows, at + 8), true);
                reportPositionValues<PageTarget>(reporting, cmper, inci, partId, "rightPg", tuple[16], source->rowOfWord(rows, at + 16), true);
                reporting.report().setInstanceOrigin(reporting.template instanceKey<PageTarget>(partId, cmper, inci), Reporting::Origin::LegacyMus);
            });
            context.document->getOthers()->add(PageTarget::XmlNodeName, std::move(target));
        }
    }
}

void importShapeFamily(const ImportContext& context)
{
    const auto source =
        selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), shapeGraphicTag, shapeGraphicClass);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        const auto words = collectRecordWords(*source, rows, context.profile.byteOrder);
        if (words.size() % graphicAssignmentWordCount != 0) {
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Info, "Shape graphic assignment " + std::to_string(cmper) + " has an incomplete trailing tuple."});
        }
        for (std::size_t at = 0; at + graphicAssignmentWordCount <= words.size(); at += graphicAssignmentWordCount) {
            const auto inci = static_cast<musx::dom::Inci>(at / graphicAssignmentWordCount);
            auto target = createOthersRecordTarget<ShapeTarget>(context.document, *source, rows.front(), cmper, inci);
            if (!target) {
                continue;
            }
            const std::span<const std::int16_t> tuple(words.data() + at, graphicAssignmentWordCount);
            populateGraphicAssignmentCommon(*target, tuple);
            populateGraphicAssignmentPosition<false>(*target, static_cast<std::uint16_t>(tuple[8]));
            constexpr std::size_t importedSlots[] = {0, 1, 2, 3, 4, 5, 7, 11, 12, 13, 17};
            constexpr const char* names[] = {
                "version", "left", "bottom", "width", "height", "fDescId", "hidden", "savedRecord", "origWidth", "origHeight", "graphicCmper"};
            for (std::size_t index = 0; index < std::size(importedSlots); ++index) {
                const auto slot = importedSlots[index];
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                    reportAssignmentValue<ShapeTarget>(reporting, cmper, inci, partId, names[index], tuple[slot], source->rowOfWord(rows, at + slot));
                });
            }
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                reportPositionValues<ShapeTarget>(reporting, cmper, inci, partId, "", tuple[8], source->rowOfWord(rows, at + 8), false);
                reporting.report().setInstanceOrigin(reporting.template instanceKey<ShapeTarget>(partId, cmper, inci), Reporting::Origin::LegacyMus);
            });
            context.document->getOthers()->add(ShapeTarget::XmlNodeName, std::move(target));
        }
    }
}

} // namespace

void importPageGraphicAssignments(const ImportContext& context)
{
    importPageFamily(context);
}

void importShapeGraphicAssignments(const ImportContext& context)
{
    importShapeFamily(context);
}

} // namespace others
} // namespace finale_mus_reader
