// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <memory>
#include <string>

#include "import/support/legacy_mapping.h"
#include "import/support/text_encoding.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using FretboardGroupTarget = musx::dom::others::FretboardGroup;
constexpr records::LegacyTag fretboardGroupClass = 0x0094;
constexpr records::LegacyTag fretboardGroupTag = records::packTag("fg");
constexpr std::size_t narrowFretboardGroupTupleSize = 60;
constexpr std::size_t unicodeFretboardGroupTupleSize = 204;
constexpr std::size_t fretboardGroupNameOffset = 12;
constexpr std::size_t narrowFretboardGroupNameSize = 48;
constexpr std::size_t unicodeFretboardGroupNameSize = 192;

} // namespace

void importFretboardGroups(const ImportContext& context)
{
    const auto source =
        selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), fretboardGroupTag, fretboardGroupClass);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto payload = collectRecordPayload(*source, rows);
        // Finale 2012 widens the fixed-capacity group name from bytes to UTF-16LE code units.
        // The class record is correspondingly 204 bytes per logical group incidence.
        const bool unicodeLayout = source->classRecords && versions::storesUnicodeCodepoints(context.profile.version);
        const auto tupleSize = unicodeLayout ? unicodeFretboardGroupTupleSize : narrowFretboardGroupTupleSize;
        if (payload.size() % tupleSize != 0) {
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Info, "Fretboard group " + std::to_string(cmper) + " has an incomplete trailing tuple."});
        }
        for (std::size_t at = 0; at + tupleSize <= payload.size(); at += tupleSize) {
            const auto inci = static_cast<musx::dom::Inci>(at / tupleSize);
            auto target = createOthersRecordTarget<FretboardGroupTarget>(context.document, *source, rows.front(), cmper, inci);
            if (!target) {
                continue;
            }
            target->fretInstId = payloadWord(payload, at, context.profile.byteOrder);
            if (unicodeLayout) {
                target->name = text::utf16LeToUtf8(std::span(payload).subspan(at + fretboardGroupNameOffset, unicodeFretboardGroupNameSize));
            } else {
                target->name =
                    text::toUtf8(payloadString(payload, at + fretboardGroupNameOffset, narrowFretboardGroupNameSize), context.profile.platform);
            }
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<FretboardGroupTarget>(partId, cmper, inci);
                reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
                reporting.report().setField(
                    key, "fretInstId", {Reporting::Origin::LegacyMus, rows.front().blockOffset, rows.front().decodedOffset, target->fretInstId});
            });
            context.document->getOthers()->add(FretboardGroupTarget::XmlNodeName, std::move(target));
        }
    }
}

} // namespace others
} // namespace finale_mus_reader
