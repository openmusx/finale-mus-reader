// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

template <typename Target, typename Populate>
void importFileRecords(const ImportContext& context, records::LegacyTag tag, records::LegacyTag classId, Populate populate)
{
    // The Coda-banner epoch predates the graphic tool and has no graphic locators to import.
    if (context.profile.epoch == FormatEpoch::CodaBanner) {
        return;
    }
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), tag, classId);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        const auto payload = collectRecordPayload(*source, rows);
        auto target = createOthersRecordTarget<Target>(context.document, *source, rows.front(), cmper);
        if (!target) {
            continue;
        }
        const auto report = [&](const char* member, std::int64_t raw, std::size_t offset, bool adjusted = false, bool mapped = true) {
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto origin = !mapped    ? Reporting::Origin::Unmapped
                                    : adjusted ? Reporting::Origin::LegacyMusAdjusted
                                               : Reporting::Origin::LegacyMus;
                const auto& row = source->rowOfByte(rows, offset);
                reporting.report().setField(reporting.template instanceKey<Target>(partId, cmper), member,
                    {origin, row.blockOffset, row.decodedOffset, raw, source->identity});
            });
        };
        if (populate(*target, std::span<const std::uint8_t>(payload), report)) {
            context.document->getOthers()->add(Target::XmlNodeName, std::move(target));
        } else {
            context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning,
                std::string(Target::XmlNodeName) + " " + std::to_string(cmper) + " has an incomplete or invalid payload."});
        }
    }
}

void importFileAliases(const ImportContext& context)
{
    importFileRecords<musx::dom::others::FileAlias>(context, records::packTag("Fa"), 0x0089, [&](auto& target, auto payload, auto report) {
        if (payload.size() < 4) {
            return false;
        }
            // The length occupies a low-word-first long even in big-endian fixed rows.
        const auto length = static_cast<std::uint32_t>(payloadLong(payload, 0, context.profile.byteOrder, LongWordOrder::LowFirst));
        if (length > payload.size() - 4) {
            return false;
        }
        target.length = length;
        target.aliasHandle.assign(payload.begin() + 4, payload.begin() + 4 + length);
            // Believed: opaque alias words use the opposite byte order to the DOM blob,
            // independently of container endianness. Leave an unpaired final byte intact.
        for (std::size_t i = 0; i + 1 < target.aliasHandle.size(); i += 2) {
            std::swap(target.aliasHandle[i], target.aliasHandle[i + 1]);
        }
        report("length", length, 0);
        report("aliasHandle", length, 4, true);
        return true;
    });
}

void importFileDescriptions(const ImportContext& context)
{
    using Target = musx::dom::others::FileDescription;
    importFileRecords<Target>(context, records::packTag("Fd"), 0x008a, [&](auto& target, auto payload, auto report) {
        if (payload.size() < 12) {
            return false;
        }
        const auto word = [&](std::size_t offset) { return payloadWord(payload, offset, context.profile.byteOrder); };
        target.version = static_cast<std::uint16_t>(word(0));
        target.pathId = static_cast<musx::dom::Cmper>(word(4));
        target.volRefNum = static_cast<std::int16_t>(word(6));
        target.dirId = static_cast<std::int32_t>(payloadLong(payload, 8, context.profile.byteOrder, nativeLongWordOrder(context.profile.byteOrder)));
            // POSIX and URL-bookmark paths are treated as MUSX-only; unknown wire values
            // still produce diagnostics rather than silently selecting a modern type.
        const auto type = word(2);
        if (type == 1) {
            target.pathType = Target::PathType::DosPath;
        } else if (type == 2) {
            target.pathType = Target::PathType::MacFsSpec;
        } else if (type == 3) {
            target.pathType = Target::PathType::MacAlias;
        } else {
            context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning, "Unmapped file locator type " + std::to_string(type) + "."});
        }
        report("version", target.version, 0);
        report("pathType", type, 2, false, type >= 1 && type <= 3);
        report("pathId", target.pathId, 4);
        report("volRefNum", target.volRefNum, 6);
        report("dirId", target.dirId, 8);
        return true;
    });
}

void importFilePaths(const ImportContext& context)
{
    importFileRecords<musx::dom::others::FilePath>(context, records::packTag("Fp"), 0x008b, [&](auto& target, auto payload, auto report) {
        if (payload.empty()) {
            return false;
        }
        const auto raw = payloadString(payload, 0, payload.size());
        target.path = versions::storesUnicodeCodepoints(context.profile.version) ? raw : text::toUtf8(raw, context.profile.platform);
        report("path", 0, 0);
        return true;
    });
}

} // namespace

void importFilePath(const ImportContext& context)
{
    importFileAliases(context);
    importFileDescriptions(context);
    importFilePaths(context);
    // No legacy selector is established in any epoch. A later conversion may synthesize
    // bookmarks; their presence there does not justify creating a source-owned object.
}

} // namespace others
} // namespace finale_mus_reader
