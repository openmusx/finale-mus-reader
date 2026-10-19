// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "import/support/legacy_mapping.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using PartGlobalsTarget = musx::dom::others::PartGlobals;

constexpr records::LegacyTag partGlobalsClass = 0x0120;
constexpr std::size_t partGlobalsPayloadSize = 12;

constexpr std::size_t showTransposedOffset = 0;
constexpr std::size_t scrollViewIUlistOffset = 2;
constexpr std::size_t studioViewIUlistOffset = 4;
constexpr std::size_t specialPartExtractionIUListOffset = 6;

constexpr std::uint16_t showTransposedSelector = 12;
constexpr std::size_t showTransposedWord = 0;
constexpr std::uint16_t specialPartExtractionSelector = 23;
constexpr std::size_t specialPartExtractionWord = 4;

void reportFixedRowPartGlobals(const ImportContext& context, const PartGlobalsTarget& instance,
    const std::optional<records::RecordWord>& showTransposedSource, const std::optional<records::RecordWord>& specialPartExtractionSource)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<PartGlobalsTarget>(musx::dom::SCORE_PARTID, musx::dom::MUSX_GLOBALS_CMPER);
        if (showTransposedSource) {
            reporting.report().setField(key, "showTransposed",
                typename Reporting::FieldInfo{Reporting::Origin::LegacyMus, showTransposedSource->blockOffset, showTransposedSource->decodedOffset,
                    instance.showTransposed, numericGlobalTag(showTransposedSelector)});
        } else {
            reporting.unmappedField(key, "showTransposed", instance.showTransposed);
        }
        reporting.report().setField(
            key, "scrollViewIUlist", typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, instance.scrollViewIUlist});
        reporting.report().setField(
            key, "studioViewIUlist", typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, instance.studioViewIUlist});
        if (specialPartExtractionSource) {
            reporting.report().setField(key, "specialPartExtractionIUList",
                typename Reporting::FieldInfo{Reporting::Origin::LegacyMus, specialPartExtractionSource->blockOffset,
                    specialPartExtractionSource->decodedOffset, instance.specialPartExtractionIUList,
                    numericGlobalTag(specialPartExtractionSelector)});
        } else {
            reporting.unmappedField(key, "specialPartExtractionIUList", instance.specialPartExtractionIUList);
        }
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
    });
}

void reportClassPartGlobals(
    const ImportContext& context, const PartGlobalsTarget& instance, const RecordFamilySource& source, const records::LegacyRow& row)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<PartGlobalsTarget>(instance.getSourcePartId(), instance.getCmper());
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        const auto reportField = [&](const char* member, std::size_t offset, std::int64_t value) {
            reporting.report().setField(key, member,
                typename Reporting::FieldInfo{Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset + offset, value, source.identity});
        };
        reportField("showTransposed", showTransposedOffset, instance.showTransposed);
        reportField("scrollViewIUlist", scrollViewIUlistOffset, instance.scrollViewIUlist);
        reportField("studioViewIUlist", studioViewIUlistOffset, instance.studioViewIUlist);
        reportField("specialPartExtractionIUList", specialPartExtractionIUListOffset, instance.specialPartExtractionIUList);
    });
}

void importFixedRowPartGlobals(const ImportContext& context)
{
    auto instance = std::make_shared<PartGlobalsTarget>(
        context.document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, musx::dom::MUSX_GLOBALS_CMPER);

    const auto showTransposedSource = context.index.word(numericGlobalTag(showTransposedSelector), musx::dom::MUSX_GLOBALS_CMPER, showTransposedWord);
    if (showTransposedSource) {
        instance->showTransposed = showTransposedSource->value != 0;
    }

    instance->scrollViewIUlist = musx::dom::BASE_SYSTEM_ID;
    instance->studioViewIUlist = musx::dom::STUDIO_VIEW_SYSTEM_ID;

    const auto specialPartExtractionSource =
        context.index.word(numericGlobalTag(specialPartExtractionSelector), musx::dom::MUSX_GLOBALS_CMPER, specialPartExtractionWord);
    if (specialPartExtractionSource) {
        instance->specialPartExtractionIUList = static_cast<musx::dom::Cmper>(specialPartExtractionSource->value);
    }

    reportFixedRowPartGlobals(context, *instance, showTransposedSource, specialPartExtractionSource);
    context.document->getOthers()->add(PartGlobalsTarget::XmlNodeName, std::move(instance));
}

void importClassPartGlobals(const ImportContext& context)
{
    const RecordFamilySource source{.pool = &context.index.getClassOthers(), .identity = partGlobalsClass, .classRecords = true};
    for (const auto& [partId, cmper] : recordKeys(source)) {
        const auto rows = source.pool->getArray(source.identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto& row = rows.front();
        const auto payload = source.pool->effectivePayloadOf(row);
        if (payload.size() < partGlobalsPayloadSize) {
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Info, "Part globals record for part " + std::to_string(partId) + " is shorter than its layout."});
            continue;
        }
        auto instance = createOthersRecordTarget<PartGlobalsTarget>(context.document, source, row, cmper);
        const auto word = [&](std::size_t offset) { return payloadWord(payload, offset, context.profile.byteOrder); };
        instance->showTransposed = word(showTransposedOffset) != 0;
        instance->scrollViewIUlist = word(scrollViewIUlistOffset);
        instance->studioViewIUlist = word(studioViewIUlistOffset);
        instance->specialPartExtractionIUList = word(specialPartExtractionIUListOffset);

        reportClassPartGlobals(context, *instance, source, row);
        context.document->getOthers()->add(PartGlobalsTarget::XmlNodeName, std::move(instance));
    }
}

} // namespace

void importPartGlobals(const ImportContext& context)
{
    if (sourceMatches(context.profile, EpochMask::Zlib)) {
        importClassPartGlobals(context);
    } else {
        // Every pre-zlib epoch stores the two editable values among its numeric
        // globals; linked parts do not exist there, so one score instance supplies
        // the class.
        importFixedRowPartGlobals(context);
    }
}

} // namespace others
} // namespace finale_mus_reader
