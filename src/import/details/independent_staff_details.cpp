// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/details.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace details {
namespace {

using IndependentStaffTarget = musx::dom::details::IndependentStaffDetails;

constexpr auto independentStaffTag = records::packTag("FL");
constexpr records::LegacyTag independentStaffClass = 0x0411;

// The record's word stream, keyed by staff and measure. The first incidence is unchanged from
// Finale 1.0.0 through Finale 2012; the fifth word is its only flag word and the fourth is unused.
constexpr std::size_t keySlot = 0;
constexpr std::size_t beatsSlot = 1;
constexpr std::size_t divBeatSlot = 2;
constexpr std::size_t flagsSlot = 4;
constexpr std::size_t firstIncidenceWords = 5;

// A second incidence carries the display time signature in its first two words. **The record
// states its own shape**: one incidence has no display time signature, two do. Its three
// remaining words are not interpreted. The two words are taken as stored whether or not the flag
// word enables the display time signature; when it does not, they need not hold a meaningful value.
constexpr std::size_t dispBeatsSlot = 5;
constexpr std::size_t dispDivBeatSlot = 6;
constexpr std::size_t withDisplayTimeSigWords = 10;

constexpr std::uint16_t hasKeyMask = 0x0800;
constexpr std::uint16_t hasTimeMask = 0x0400;
constexpr std::uint16_t hasDispTimeMask = 0x0200;
constexpr std::uint16_t altNumTsigMask = 0x0080;
constexpr std::uint16_t altDenTsigMask = 0x0040;
constexpr std::uint16_t displayAbbrvTimeMask = 0x0004;
constexpr std::uint16_t displayAltDenTsigMask = 0x0002;
constexpr std::uint16_t displayAltNumTsigMask = 0x0001;

/// @brief Decodes one record into its target and reports every persisted member.
class IndependentStaffDecoder
{
public:
    IndependentStaffDecoder(const ImportContext& context, const RecordFamilySource& source, std::span<const records::LegacyRow> rows,
        const std::vector<std::int16_t>& words, IndependentStaffTarget& target)
        : m_context(context), m_source(source), m_rows(rows), m_words(words), m_target(target)
    {}

    void decode()
    {
        const auto flags = word(flagsSlot);
        m_target.keySig->key = word(keySlot);
        m_target.beats = word(beatsSlot);
        m_target.divBeat = word(divBeatSlot);
        m_target.hasKey = (flags & hasKeyMask) != 0;
        m_target.hasTime = (flags & hasTimeMask) != 0;
        m_target.altNumTsig = (flags & altNumTsigMask) != 0;
        m_target.altDenTsig = (flags & altDenTsigMask) != 0;
        // Neither key-signature switch exists before the musx format, so no supported layout
        // carries either and both take the era's behavior.
        m_target.keySig->keyless = false;
        m_target.keySig->hideKeySigShowAccis = false;
        if (hasDisplayTimeSig()) {
            m_target.dispBeats = word(dispBeatsSlot);
            m_target.dispDivBeat = word(dispDivBeatSlot);
            m_target.hasDispTime = (flags & hasDispTimeMask) != 0;
            m_target.displayAltNumTsig = (flags & displayAltNumTsigMask) != 0;
            m_target.displayAltDenTsig = (flags & displayAltDenTsigMask) != 0;
            m_target.displayAbbrvTime = (flags & displayAbbrvTimeMask) != 0;
        }
        report(flags);
    }

private:
    [[nodiscard]] bool hasDisplayTimeSig() const { return m_words.size() >= withDisplayTimeSigWords; }

    [[nodiscard]] std::uint16_t word(std::size_t slot) const { return static_cast<std::uint16_t>(m_words[slot]); }

    void report(std::uint16_t flags) const
    {
        withReporting(m_context.report, [&]<typename Reporting>(Reporting& reporting) {
            const auto key = reporting.template instanceKey<IndependentStaffTarget>(
                m_target.getSourcePartId(), m_target.getCmper1(), std::nullopt, m_target.getCmper2());
            reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
            const auto stored = [&](const char* member, std::size_t slot, std::int64_t raw) {
                const auto& row = m_source.rowOfWord(m_rows, slot);
                reportLegacyField(reporting, key, m_source, row, member, m_source.byteOffsetInRow(slot * sizeof(std::uint16_t)), raw);
            };
            const auto behavior = [&](const char* member, std::int64_t value) {
                reportFallbackField(reporting, key, member, Reporting::Origin::LegacyBehavior, value);
            };
            stored("keySig.key", keySlot, word(keySlot));
            stored("beats", beatsSlot, word(beatsSlot));
            stored("divBeat", divBeatSlot, word(divBeatSlot));
            for (const auto* member : {"hasKey", "hasTime", "altNumTsig", "altDenTsig"}) {
                stored(member, flagsSlot, flags);
            }
            behavior("keySig.keyless", false);
            behavior("keySig.hideKeySigShowAccis", false);
            if (hasDisplayTimeSig()) {
                stored("dispBeats", dispBeatsSlot, word(dispBeatsSlot));
                stored("dispDivBeat", dispDivBeatSlot, word(dispDivBeatSlot));
                for (const auto* member : {"hasDispTime", "displayAltNumTsig", "displayAltDenTsig", "displayAbbrvTime"}) {
                    stored(member, flagsSlot, flags);
                }
            } else {
                for (const auto* member : {"dispBeats", "dispDivBeat", "hasDispTime", "displayAltNumTsig", "displayAltDenTsig", "displayAbbrvTime"}) {
                    behavior(member, 0);
                }
            }
        });
    }

    const ImportContext& m_context;
    const RecordFamilySource& m_source;
    std::span<const records::LegacyRow> m_rows;
    const std::vector<std::int16_t>& m_words;
    IndependentStaffTarget& m_target;
};

void importIndependentStaffFamily(const ImportContext& context, const RecordFamilySource& source)
{
    for (const auto& [partId, staffId] : recordKeys(source)) {
        for (const auto meas : source.pool->secondCmpersForTag(source.identity, staffId, partId)) {
            const auto rows = source.pool->getArray(source.identity, staffId, meas, partId);
            if (rows.empty()) {
                continue;
            }
            const auto words = collectRecordWords(source, rows, context.profile.byteOrder);
            if (words.size() < firstIncidenceWords) {
                context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
                    "Independent staff detail for staff " + std::to_string(staffId) + ", measure " + std::to_string(meas) + " is truncated."});
                continue;
            }
            auto target = createDetailsRecordTarget<IndependentStaffTarget>(context.document, source, rows.front(), staffId, meas);
            // musxdom guarantees the contained key signature only from its own integrity check, which
            // runs later than this. The decoder writes into it, so it has to exist first.
            target->keySig = std::make_shared<musx::dom::KeySignature>(context.document);
            IndependentStaffDecoder(context, source, rows, words, *target).decode();
            context.document->getDetails()->add(IndependentStaffTarget::XmlNodeName, std::move(target));
        }
    }
}

} // namespace

void importIndependentStaffDetails(const ImportContext& context)
{
    const auto source = selectRecordFamilySource(
        context, context.index.getDetails(), context.index.getClassDetails(), independentStaffTag, independentStaffClass, true);
    if (source) {
        importIndependentStaffFamily(context, *source);
    }
}

} // namespace details
} // namespace finale_mus_reader
