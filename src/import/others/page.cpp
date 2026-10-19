// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "import/support/legacy_mapping.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using PageImportTarget = musx::dom::others::Page;

constexpr records::LegacyTag pageImportTag = records::packTag("PS");
constexpr records::LegacyTag pageImportClass = 0x00bb;
constexpr records::LegacyTag pageImportMarginTag = records::packTag("PO");
constexpr records::LegacyTag pageImportPercentTag = records::packTag("PP");

constexpr std::size_t pageImportCompactPayloadSize = 12;
constexpr std::size_t pageImportCompletePayloadSize = 24;
constexpr std::size_t pageImportHeightOffset = 0;
constexpr std::size_t pageImportWidthOffset = 4;
constexpr std::size_t pageImportFirstSystemOffset = 8;
constexpr std::size_t pageImportFlagsOffset = 10;
constexpr std::size_t pageImportMarginTopOffset = 12;
constexpr std::size_t pageImportMarginLeftOffset = 14;
constexpr std::size_t pageImportMarginBottomOffset = 16;
constexpr std::size_t pageImportMarginRightOffset = 18;
constexpr std::size_t pageImportPercentOffset = 20;
constexpr std::uint16_t pageImportHoldMarginsMask = 0x0002;
constexpr std::uint16_t pageImportCompactHoldMarginsMask = 0x4000;
constexpr int pageImportUnscaledPercent = 100;

template <typename Reporting>
void reportPageField(Reporting& reporting, const typename Reporting::InstanceKey& key, const RecordFamilySource& source,
    std::span<const records::LegacyRow> rows, const char* member, std::size_t offset, std::int64_t value)
{
    const auto& row = source.rowOfByte(rows, offset);
    reporting.report().setField(key, member,
        typename Reporting::FieldInfo{
            Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset + source.byteOffsetInRow(offset), value, source.identity});
}

void reportStoredPage(const ImportContext& context, const PageImportTarget& target, const RecordFamilySource& source,
    std::span<const records::LegacyRow> rows, bool completeLayout)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<PageImportTarget>(target.getSourcePartId(), target.getCmper());
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        reportPageField(reporting, key, source, rows, "height", pageImportHeightOffset, target.height);
        reportPageField(reporting, key, source, rows, "width", pageImportWidthOffset, target.width);
        reportPageField(reporting, key, source, rows, "firstSystemId", pageImportFirstSystemOffset, target.firstSystemId);
        if (!completeLayout) {
            return;
        }
        reportPageField(reporting, key, source, rows, "holdMargins", pageImportFlagsOffset, target.holdMargins);
        reportPageField(reporting, key, source, rows, "margTop", pageImportMarginTopOffset, target.margTop);
        reportPageField(reporting, key, source, rows, "margLeft", pageImportMarginLeftOffset, target.margLeft);
        reportPageField(reporting, key, source, rows, "margBottom", pageImportMarginBottomOffset, target.margBottom);
        reportPageField(reporting, key, source, rows, "margRight", pageImportMarginRightOffset, target.margRight);
        reportPageField(reporting, key, source, rows, "percent", pageImportPercentOffset, target.percent);
    });
}

template <typename Reporting>
void reportCompactPageField(Reporting& reporting, const typename Reporting::InstanceKey& key, const records::LegacyRow& row,
    records::LegacyTag identity, const char* member, std::size_t wordOffset, std::int64_t value)
{
    reporting.report().setField(key, member,
        typename Reporting::FieldInfo{
            Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset + wordOffset * sizeof(std::uint16_t), value, identity});
}

void importCompactPageFields(const ImportContext& context, PageImportTarget& target)
{
    const auto& pool = context.index.getOthers();
    const auto partId = target.getSourcePartId();
    const auto cmper = target.getCmper();
    const auto* margins = pool.get(pageImportMarginTag, cmper, 0, 0, partId);
    const auto* percent = pool.get(pageImportPercentTag, cmper, 0, 0, partId);

    if (margins && margins->wordCount >= 4) {
        target.margTop = margins->words[0];
        target.margLeft = margins->words[1];
        target.margBottom = margins->words[2];
        target.margRight = margins->words[3];
    }
    if (percent && percent->wordCount >= 2) {
        target.percent = percent->words[0];
        if (percent->words[1] != percent->words[0]) {
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Info, "Page " + std::to_string(cmper) + " has unequal percentage copies."});
        }
    } else {
        target.percent = pageImportUnscaledPercent;
    }
    target.holdMargins = !percent || percent->wordCount < 6 || (percent->words[5] & pageImportCompactHoldMarginsMask) != 0;

    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<PageImportTarget>(target.getSourcePartId(), target.getCmper());
        if (margins && margins->wordCount >= 4) {
            reportCompactPageField(reporting, key, *margins, pageImportMarginTag, "margTop", 0, target.margTop);
            reportCompactPageField(reporting, key, *margins, pageImportMarginTag, "margLeft", 1, target.margLeft);
            reportCompactPageField(reporting, key, *margins, pageImportMarginTag, "margBottom", 2, target.margBottom);
            reportCompactPageField(reporting, key, *margins, pageImportMarginTag, "margRight", 3, target.margRight);
        } else {
            reporting.unmappedField(key, "margTop", target.margTop);
            reporting.unmappedField(key, "margLeft", target.margLeft);
            reporting.unmappedField(key, "margBottom", target.margBottom);
            reporting.unmappedField(key, "margRight", target.margRight);
        }
        if (percent && percent->wordCount >= 2) {
            reportCompactPageField(reporting, key, *percent, pageImportPercentTag, "percent", 0, target.percent);
        } else {
            reporting.report().setField(key, "percent", typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, target.percent});
        }
        if (percent && percent->wordCount >= 6) {
            reportCompactPageField(reporting, key, *percent, pageImportPercentTag, "holdMargins", 5, target.holdMargins);
        } else {
            reporting.report().setField(
                key, "holdMargins", typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, target.holdMargins});
        }
    });
}

void importOnePage(const ImportContext& context, const RecordFamilySource& source, const records::LegacyRow& row, std::uint16_t cmper)
{
    const auto rows = source.pool->getArray(source.identity, cmper, 0, row.partId);
    const auto payload = collectRecordPayload(source, rows);
    if (payload.size() < pageImportCompactPayloadSize) {
        context.report.diagnostics.push_back(
            {musx::util::Logger::LogLevel::Info, "Page " + std::to_string(cmper) + " is shorter than its compact layout."});
        return;
    }
    if (payload.size() > pageImportCompactPayloadSize && payload.size() < pageImportCompletePayloadSize) {
        context.report.diagnostics.push_back(
            {musx::util::Logger::LogLevel::Info, "Page " + std::to_string(cmper) + " has an unrecognized intermediate layout."});
        return;
    }

    auto target = createOthersRecordTarget<PageImportTarget>(context.document, source, row, cmper);
    const auto byteOrder = context.profile.byteOrder;
    const auto longOrder = nativeLongWordOrder(byteOrder);
    target->height = payloadLong(payload, pageImportHeightOffset, byteOrder, longOrder);
    target->width = payloadLong(payload, pageImportWidthOffset, byteOrder, longOrder);
    target->firstSystemId = static_cast<musx::dom::SystemCmper>(payloadWord(payload, pageImportFirstSystemOffset, byteOrder));

    const bool completeLayout = payload.size() >= pageImportCompletePayloadSize;
    if (completeLayout) {
        const auto flags = payloadWord(payload, pageImportFlagsOffset, byteOrder);
        // Page ossias are a separate legacy feature and are deliberately outside this class.
        target->holdMargins = (flags & pageImportHoldMarginsMask) != 0;
        target->margTop = static_cast<std::int16_t>(payloadWord(payload, pageImportMarginTopOffset, byteOrder));
        target->margLeft = static_cast<std::int16_t>(payloadWord(payload, pageImportMarginLeftOffset, byteOrder));
        target->margBottom = static_cast<std::int16_t>(payloadWord(payload, pageImportMarginBottomOffset, byteOrder));
        target->margRight = static_cast<std::int16_t>(payloadWord(payload, pageImportMarginRightOffset, byteOrder));
        target->percent = static_cast<std::int16_t>(payloadWord(payload, pageImportPercentOffset, byteOrder));
    } else {
        importCompactPageFields(context, *target);
    }

    reportStoredPage(context, *target, source, rows, completeLayout);
    context.document->getOthers()->add(PageImportTarget::XmlNodeName, std::move(target));
}

} // namespace

void importPages(const ImportContext& context)
{
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), pageImportTag, pageImportClass);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        if (!rows.empty()) {
            importOnePage(context, *source, rows.front(), cmper);
        }
    }
}

} // namespace others
} // namespace finale_mus_reader
