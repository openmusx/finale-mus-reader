// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "import/support/text_encoding.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using CategoryNameTarget = musx::dom::others::StaffListCategoryName;
using CategoryPartsTarget = musx::dom::others::StaffListCategoryParts;
using CategoryScoreTarget = musx::dom::others::StaffListCategoryScore;
using RepeatNameTarget = musx::dom::others::StaffListRepeatName;
using RepeatPartsTarget = musx::dom::others::StaffListRepeatParts;
using RepeatPartsForcedTarget = musx::dom::others::StaffListRepeatPartsForced;
using RepeatScoreTarget = musx::dom::others::StaffListRepeatScore;
using RepeatScoreForcedTarget = musx::dom::others::StaffListRepeatScoreForced;

struct StaffListSelector
{
    records::LegacyTag fixedTag{};
    records::LegacyTag classId{};
};

constexpr records::LegacyTag categoryNameClass = 0x012f;
constexpr records::LegacyTag categoryScoreClass = 0x0130;
constexpr records::LegacyTag categoryScoreOverrideClass = 0x0131;
constexpr records::LegacyTag categoryPartsClass = 0x0132;
constexpr records::LegacyTag categoryPartsOverrideClass = 0x0133;
constexpr musx::dom::Cmper categoryListCount = 8;

constexpr StaffListSelector repeatNameSelector{records::packTag("Dc"), 0x00e1};
constexpr StaffListSelector repeatScoreSelector{records::packTag("DC"), 0x00e4};
constexpr StaffListSelector repeatPartsSelector{records::packTag("dc"), 0x00e2};
constexpr StaffListSelector repeatScoreForcedSelector{records::packTag("IO"), 0x00e5};
constexpr StaffListSelector repeatPartsForcedSelector{records::packTag("io"), 0x00e3};

std::optional<RecordFamilySource> selectStaffListSource(const ImportContext& context, StaffListSelector selector)
{
    return selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), selector.fixedTag, selector.classId);
}

bool hasCategoryRecords(const ImportContext& context)
{
    const auto& pool = context.index.getClassOthers();
    return !pool.cmpersForTag(categoryNameClass).empty() || !pool.cmpersForTag(categoryScoreClass).empty()
           || !pool.cmpersForTag(categoryScoreOverrideClass).empty() || !pool.cmpersForTag(categoryPartsClass).empty()
           || !pool.cmpersForTag(categoryPartsOverrideClass).empty();
}

bool sourceStoresCategoryLists(const ImportContext& context)
{
    if (context.profile.epoch != FormatEpoch::ZlibLegacy) {
        return false;
    }
    if (hasCategoryRecords(context)) {
        return true;
    }
    return sourceAtOrAfter(context.profile, FormatEpoch::ZlibLegacy, versions::finale2009);
}

void reportUnsupportedOverrides(const ImportContext& context, records::LegacyTag identity, std::string_view component)
{
    const RecordFamilySource source{.pool = &context.index.getClassOthers(), .identity = identity, .classRecords = true};
    for (const auto& [partId, cmper] : recordKeys(source)) {
        context.report.diagnostics.push_back(
            {musx::util::Logger::LogLevel::Info, "Category " + std::string(component) + " staff-list override " + std::to_string(cmper) + " for part "
                                                     + std::to_string(partId) + " is unsupported and was ignored."});
    }
}

template <typename Target, typename OnImported>
void importStaffListArrays(const ImportContext& context, const RecordFamilySource& source, OnImported&& onImported)
{
    for (const auto& [partId, cmper] : recordKeys(source)) {
        const auto rows = source.pool->getArray(source.identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        auto target = createOthersRecordTarget<Target>(context.document, source, rows.front(), cmper);
        for (const auto& row : rows) {
            const auto payload = source.pool->effectivePayloadOf(row);
            if (payload.size() % 2 != 0) {
                context.report.diagnostics.push_back(
                    {musx::util::Logger::LogLevel::Info, "Staff list " + std::to_string(cmper) + " has an incomplete trailing staff value."});
            }
            constexpr auto chunkBytes = records::otherWordCount * 2;
            for (std::size_t chunk = 0; chunk < payload.size(); chunk += chunkBytes) {
                const auto chunkEnd = (std::min)(chunk + chunkBytes, payload.size());
                for (std::size_t offset = chunk; offset + 2 <= chunkEnd; offset += 2) {
                    const auto value = static_cast<std::int16_t>(payloadWord(payload, offset, context.profile.byteOrder));
                    if (value == 0) {
                        break;
                    }
                    target->values.push_back(value);
                    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                        const auto key = reporting.template instanceKey<Target>(partId, cmper);
                        reporting.report().setField(key, "values[" + std::to_string(target->values.size() - 1) + "]",
                            {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset + offset, value, source.identity});
                    });
                }
            }
        }
        if (target->values.empty()) {
            continue;
        }
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            reporting.report().setInstanceOrigin(reporting.template instanceKey<Target>(partId, cmper), Reporting::Origin::LegacyMus);
        });
        context.document->getOthers()->add(Target::XmlNodeName, std::move(target));
        onImported(cmper);
    }
}

std::string staffListNameBytes(std::span<const std::uint8_t> payload)
{
    std::string result(reinterpret_cast<const char*>(payload.data()), payload.size());
    if (const auto end = result.find('\0'); end != std::string::npos) {
        result.resize(end);
    }
    return result;
}

template <typename Target, typename OnImported>
void importStaffListNames(const ImportContext& context, const RecordFamilySource& source, OnImported&& onImported)
{
    for (const auto& [partId, cmper] : recordKeys(source)) {
        const auto rows = source.pool->getArray(source.identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto payload = collectRecordPayload(source, rows);
        auto target = createOthersRecordTarget<Target>(context.document, source, rows.front(), cmper);
        auto stored = staffListNameBytes(payload);
        target->name = text::toUtf8(stored, context.profile.platform);
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            reporting.report().setInstanceOrigin(reporting.template instanceKey<Target>(partId, cmper), Reporting::Origin::LegacyMus);
        });
        context.document->getOthers()->add(Target::XmlNodeName, std::move(target));
        onImported(cmper);
    }
}

void importRepeatStaffLists(const ImportContext& context)
{
    const auto ignoreCmper = [](musx::dom::Cmper) {};
    if (const auto source = selectStaffListSource(context, repeatNameSelector)) {
        importStaffListNames<RepeatNameTarget>(context, *source, ignoreCmper);
    }
    if (const auto source = selectStaffListSource(context, repeatPartsSelector)) {
        importStaffListArrays<RepeatPartsTarget>(context, *source, ignoreCmper);
    }
    if (const auto source = selectStaffListSource(context, repeatPartsForcedSelector)) {
        importStaffListArrays<RepeatPartsForcedTarget>(context, *source, ignoreCmper);
    }
    if (const auto source = selectStaffListSource(context, repeatScoreSelector)) {
        importStaffListArrays<RepeatScoreTarget>(context, *source, ignoreCmper);
    }
    if (const auto source = selectStaffListSource(context, repeatScoreForcedSelector)) {
        importStaffListArrays<RepeatScoreForcedTarget>(context, *source, ignoreCmper);
    }
}

void importCategoryStaffLists(const ImportContext& context)
{
    std::set<musx::dom::Cmper> importedCmpers;
    const auto rememberCmper = [&importedCmpers](musx::dom::Cmper cmper) { importedCmpers.insert(cmper); };
    if (sourceStoresCategoryLists(context)) {
        reportUnsupportedOverrides(context, categoryScoreOverrideClass, "score");
        reportUnsupportedOverrides(context, categoryPartsOverrideClass, "parts");
        const RecordFamilySource nameSource{.pool = &context.index.getClassOthers(), .identity = categoryNameClass, .classRecords = true};
        const RecordFamilySource partsSource{.pool = &context.index.getClassOthers(), .identity = categoryScoreClass, .classRecords = true};
        const RecordFamilySource scoreSource{.pool = &context.index.getClassOthers(), .identity = categoryPartsClass, .classRecords = true};
        importStaffListNames<CategoryNameTarget>(context, nameSource, rememberCmper);
        importStaffListArrays<CategoryPartsTarget>(context, partsSource, rememberCmper);
        importStaffListArrays<CategoryScoreTarget>(context, scoreSource, rememberCmper);
    }
    const auto reportBaselineObject = baselineObjectReporter(context.report);
    for (musx::dom::Cmper cmper = 1; cmper <= categoryListCount; ++cmper) {
        if (importedCmpers.contains(cmper)) {
            continue;
        }
        static_cast<void>(musx::dom::others::importCategoryStaffListInto(context.document, context.referenceDocument, cmper, reportBaselineObject));
    }
}

} // namespace

void importStaffLists(const ImportContext& context)
{
    importCategoryStaffLists(context);
    importRepeatStaffLists(context);
}

} // namespace others
} // namespace finale_mus_reader
