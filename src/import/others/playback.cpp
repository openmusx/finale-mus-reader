// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"
#include "import/support/playback_plugin_names.h"
#include "musx/musx.h"

#include <array>
#include <cctype>
#include <iomanip>
#include <map>
#include <sstream>

namespace finale_mus_reader {
namespace others {
namespace {
using PlaybackRouteTarget = musx::dom::others::PlaybackRoute;
using PlaybackNameTarget = musx::dom::others::PlaybackRouteName;
using StaffPlaybackTarget = musx::dom::others::StaffPlayData;
constexpr auto playbackRouteTag = records::packTag("OC");
constexpr auto playbackNameTag = records::packTag("oc");
constexpr auto staffPlaybackTag = records::packTag("PL");
constexpr records::LegacyTag playbackRouteClass = 0x00b9;
constexpr records::LegacyTag playbackNameClass = 0x00ba;
constexpr records::LegacyTag staffPlaybackClass = 0x00e6;

enum class PlaybackFieldSource {
    Stored,
    Adjusted,
    Behavior,
    Unmapped
};

template <typename Target>
void reportPlaybackField(const ImportContext& context, const Target& target, const RecordFamilySource& source,
    std::span<const records::LegacyRow> rows, const std::string& member, std::size_t offset, std::int64_t raw,
    PlaybackFieldSource kind = PlaybackFieldSource::Stored)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<Target>(target.getSourcePartId(), target.getCmper());
        const auto origin = kind == PlaybackFieldSource::Behavior   ? Reporting::Origin::LegacyBehavior
                            : kind == PlaybackFieldSource::Adjusted ? Reporting::Origin::LegacyMusAdjusted
                            : kind == PlaybackFieldSource::Unmapped ? Reporting::Origin::Unmapped
                                                                    : Reporting::Origin::LegacyMus;
        if (kind == PlaybackFieldSource::Behavior || kind == PlaybackFieldSource::Unmapped) {
            reportFallbackField(reporting, key, member.c_str(), origin, raw);
        } else {
            const auto& row = source.rowOfByte(rows, offset);
            reportLegacyField(reporting, key, source, row, member.c_str(), source.classRecords ? offset : offset % 12, raw, origin);
        }
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
    });
}

void playbackWarning(const ImportContext& context, std::string message)
{
    context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning, std::move(message)});
}

bool playbackHasInlinePatch(const SourceProfile& profile)
{
    return sourceAtOrAfter(profile, FormatEpoch::DclLegacy, VersionBound{11, 0, 0, 0, 7});
}

void importPlaybackRouteRecord(const ImportContext& context, const RecordFamilySource& source, std::span<const records::LegacyRow> rows,
    std::span<const std::uint8_t> payload, musx::dom::Cmper cmper, std::size_t channelOffset, bool packed)
{
    auto target = createOthersRecordTarget<PlaybackRouteTarget>(context.document, source, rows.front(), cmper);
    if (!target) {
        return;
    }
    const auto word = [&](std::size_t offset) { return payloadWord(payload, offset, context.profile.byteOrder); };
    const auto rawChannel = word(channelOffset);
    const bool inlinePatch = !packed && playbackHasInlinePatch(context.profile);
    auto channel = inlinePatch ? rawChannel : rawChannel & 0x3fff;
    if (!inlinePatch && channel == 0x3fff) {
        channel = 0xffff;
    }
    target->virtualChannel = static_cast<std::int16_t>(channel);
    reportPlaybackField(context, *target, source, rows, "virtualChannel", channelOffset, rawChannel,
        inlinePatch ? PlaybackFieldSource::Stored : PlaybackFieldSource::Adjusted);

    std::uint16_t patch = 0, bank = 0;
    const auto patchSource =
        selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), records::packTag("MP"), 0x008e);
    auto patchRows = std::span<const records::LegacyRow>{};
    std::size_t patchOffset = 2;
    bool foundPatch = inlinePatch;
    bool simplePatch = false;
    if (inlinePatch) {
        patch = word(2);
        bank = word(4);
    } else if (patchSource && target->virtualChannel >= 0) {
        patchRows = patchSource->pool->getArray(patchSource->identity, 0, 0, rows.front().partId);
        const auto patchPayload = collectRecordPayload(*patchSource, patchRows);
        // Before Finale 3.7 alpha 3, each channel stores only a program word.
        simplePatch = sourcePredatesVersion(context.profile, FormatEpoch::UncompressedLegacy, VersionBound{3, 6, 0, 1, 3});
        patchOffset = static_cast<std::size_t>(target->virtualChannel) * (simplePatch ? 2 : 4);
        if (patchOffset + (simplePatch ? 2 : 4) <= patchPayload.size()) {
            patch = payloadWord(patchPayload, patchOffset, context.profile.byteOrder);
            if (simplePatch) {
                patch &= 0xff;
            } else {
                bank = payloadWord(patchPayload, patchOffset + 2, context.profile.byteOrder);
            }
            foundPatch = true;
        }
    }
    target->patch = patch & 0xff;
    target->midiBankSelectLeastSignificantByte = bank & 0xff;
    target->midiBankSelectMostSignificantByte = bank >> 8;
    constexpr std::array patchTypes{"", "C0_C32_PC", "C0_PC", "C32_PC", "PC_PC"};
    const auto type = static_cast<std::size_t>(patch >> 8);
    if (type < patchTypes.size()) {
        target->patchType = patchTypes[type];
    } else {
        playbackWarning(context, "Playback route has an unknown patch type.");
    }
    const auto patchReport = [&](const char* member, std::size_t offset, std::int64_t raw, bool valid = true) {
        if (!foundPatch || !valid) {
            reportPlaybackField(context, *target, source, rows, member, 0, raw, PlaybackFieldSource::Unmapped);
        } else if (inlinePatch) {
            reportPlaybackField(context, *target, source, rows, member, offset, raw);
        } else if (simplePatch && std::string_view(member) != "patch") {
            reportPlaybackField(context, *target, source, rows, member, 0, raw, PlaybackFieldSource::Behavior);
        } else {
            reportPlaybackField(context, *target, *patchSource, patchRows, member, patchOffset + offset - 2, raw);
        }
    };
    patchReport("patch", 2, patch);
    patchReport("patchType", 2, patch, type < patchTypes.size());
    patchReport("midiBankSelectLeastSignificantByte", 4, bank);
    patchReport("midiBankSelectMostSignificantByte", 4, bank);
    target->percussionMapId = packed ? 0 : word(6);
    reportPlaybackField(context, *target, source, rows, "percussionMapId", packed ? channelOffset : 6, target->percussionMapId,
        packed ? PlaybackFieldSource::Behavior : PlaybackFieldSource::Stored);
    const bool uuid = !packed && payload.size() >= 24;
    if (uuid && std::any_of(payload.begin() + 8, payload.begin() + 24, [](auto b) { return b != 0; })) {
        std::ostringstream value;
        value << std::hex << std::setfill('0');
        for (std::size_t i = 0; i < 16; ++i) {
            if (i == 4 || i == 6 || i == 8 || i == 10) {
                value << '-';
            }
            value << std::setw(2) << unsigned(payload[8 + i]);
        }
        target->soundUuid = value.str();
    }
    reportPlaybackField(
        context, *target, source, rows, "soundUuid", uuid ? 8 : channelOffset, 0, uuid ? PlaybackFieldSource::Stored : PlaybackFieldSource::Behavior);
    context.document->getOthers()->add(PlaybackRouteTarget::XmlNodeName, std::move(target));
}
void synthesizeCodaPlaybackNames(const ImportContext& context)
{
    for (const auto& staff : context.document->getOthers()->getArray<StaffPlaybackTarget>(musx::dom::SCORE_PARTID)) {
        const auto addName = [&](const auto& settings) {
            if (!settings || settings->playbackRouteIndex < 0) {
                return;
            }
            const auto cmper = settings->getPlaybackRouteId();
            if (context.document->getOthers()->get<PlaybackNameTarget>(musx::dom::SCORE_PARTID, cmper)) {
                return;
            }
            auto target =
                std::make_shared<PlaybackNameTarget>(context.document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, cmper);
            // Use a deterministic English label when no stored name supplies one.
            target->name = "Instrument " + std::to_string(cmper);
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<PlaybackNameTarget>(musx::dom::SCORE_PARTID, cmper);
                reportFallbackField(reporting, key, "name", Reporting::Origin::LegacyBehavior, 0);
                reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyBehavior);
            });
            context.document->getOthers()->add(PlaybackNameTarget::XmlNodeName, std::move(target));
        };
        addName(staff->chords);
        addName(staff->midiExpressions);
        for (const auto& layer : staff->layers) {
            addName(layer);
        }
    }
}

std::string playbackMapText(const musx::xml::XmlElementPtr& parent, const char* tag)
{
    const auto child = parent ? parent->getFirstChildElement(tag) : nullptr;
    return child ? child->getTextTrimmed() : std::string{};
}

std::string playbackBankPlugin(const ImportContext& context, std::uint16_t bank)
{
    if (bank >= 8) {
        return {};
    }
    const auto load = [&](records::LegacyTag tag, records::LegacyTag classId) -> std::string {
        const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), tag, classId);
        if (!source) {
            return {};
        }
        const auto rows = source->pool->getArray(source->identity, musx::dom::Cmper(bank + 1));
        const auto payload = collectRecordPayload(*source, rows);
        if (payload.size() < 0x214) {
            return {};
        }
        const auto order = context.profile.byteOrder == ByteOrder::BigEndian ? LongWordOrder::HighFirst : LongWordOrder::LowFirst;
        if (payloadLong(payload, 0, context.profile.byteOrder, order) != 0x210) {
            return {};
        }
        const auto id = static_cast<std::uint32_t>(payloadLong(payload, 4, context.profile.byteOrder, order));
        if (!id) {
            return {};
        }
        std::string result;
        for (int shift = 24; shift >= 0; shift -= 8) {
            result.push_back(static_cast<char>((id >> shift) & 0xff));
        }
        return result;
    };
    // The Mac conversion uses its bank first and accepts only SoftSynth from the alternate platform.
    auto plugin = load(records::packTag("Ai"), 0x0115);
    if (plugin.empty()) {
        plugin = load(records::packTag("Vi"), 0x0116);
        if (plugin != "SMSS") {
            plugin.clear();
        }
    }
    return plugin;
}

std::string lowerAscii(std::string value)
{
    for (auto& character : value) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

std::string normalizedStaffName(std::string value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    value.erase(0, first);
    value.erase(value.find_last_not_of("1234567890IVX ") + 1);
    return lowerAscii(value);
}

int staffSemitones(const musx::dom::others::Staff& staff)
{
    const auto [diatonic, alteration] = staff.calcTranspositionInterval();
    constexpr std::array majorScale{0, 2, 4, 5, 7, 9, 11};
    const auto absolute = diatonic < 0 ? -diatonic : diatonic;
    const auto semitones = 12 * (absolute / 7) + majorScale[static_cast<std::size_t>(absolute % 7)];
    return (diatonic < 0 ? -semitones : semitones) + alteration;
}

std::string staffInstrumentUuid(const ImportContext& context, const musx::dom::others::Staff& staff)
{
    if (!context.profile.instrumentCatalog || !staff.fullNameTextId || staff.defaultClef != 0) {
        return {};
    }
    const auto name = normalizedStaffName(musx::dom::others::TextBlock::getText(context.document, staff.fullNameTextId, 0, true));
    if (name.size() < 2) {
        return {};
    }
    const auto notation = staff.notationStyle == musx::dom::others::Staff::NotationStyle::Standard ? "standard" : "";
    // Catalog transposition is sounding pitch relative to written pitch; Staff reports the inverse.
    const auto transposition = -staffSemitones(staff);
    const playback::Instrument* best = nullptr;
    std::size_t bestLength = 0;
    for (const auto& instrument : *context.profile.instrumentCatalog) {
        if (instrument.clef != "treble" || instrument.notation != notation || instrument.transposition != transposition) {
            continue;
        }
        const auto full = lowerAscii(instrument.fullName);
        const auto internal = lowerAscii(instrument.name);
        if (name == full || name == internal) {
            return instrument.uuid;
        }
        if (full.size() >= 3 && full.starts_with(name) && full.size() > bestLength) {
            best = &instrument;
            bestLength = full.size();
        }
    }
    return best ? best->uuid : std::string{};
}

std::map<musx::dom::Cmper, std::string> playbackRouteInstruments(const ImportContext& context)
{
    std::map<musx::dom::Cmper, std::string> routes;
    if (!context.profile.instrumentCatalog) {
        return routes;
    }
    for (const auto& staff : context.document->getOthers()->getArray<musx::dom::others::Staff>(musx::dom::SCORE_PARTID)) {
        const auto uuid = staffInstrumentUuid(context, *staff);
        if (uuid.empty()) {
            continue;
        }
        const auto play = context.document->getOthers()->get<StaffPlaybackTarget>(musx::dom::SCORE_PARTID, staff->getCmper());
        if (!play) {
            continue;
        }
        const auto add = [&](const auto& settings) {
            if (!settings || settings->playbackRouteIndex < 0) {
                return;
            }
            const auto id = settings->getPlaybackRouteId();
            const auto [it, inserted] = routes.emplace(id, uuid);
            if (!inserted && it->second != uuid) {
                it->second.clear();
            }
        };
        add(play->chords);
        add(play->midiExpressions);
        for (const auto& layer : play->layers) {
            add(layer);
        }
    }
    return routes;
}

musx::xml::XmlElementPtr mappedSound(const musx::xml::XmlElementPtr& root, const std::string& instrumentUuid)
{
    for (auto mapping = root->getFirstChildElement("MapSounds"); mapping; mapping = mapping->getNextSibling("MapSounds")) {
        const auto instrument = mapping->getFirstChildElement("FinInstrument");
        const auto uuid = instrument ? instrument->findAttribute("uuid") : nullptr;
        if (!uuid || lowerAscii(uuid->getValueTrimmed()) != instrumentUuid) {
            continue;
        }
        const auto reference = mapping->getFirstChildElement("Sound");
        const auto id = reference ? reference->findAttribute("ref") : nullptr;
        if (!id) {
            continue;
        }
        for (auto sound = root->getFirstChildElement("Sound"); sound; sound = sound->getNextSibling("Sound")) {
            const auto soundId = sound->findAttribute("id");
            if (soundId && soundId->getValueTrimmed() == id->getValueTrimmed()) {
                return sound;
            }
        }
    }
    return {};
}

void upgradePlaybackNames(const ImportContext& context)
{
    if (!sourcePredatesVersion(context.profile, FormatEpoch::ZlibLegacy, VersionBound{17, 0, 0, 2, 112})) {
        return;
    }
    const soundmap::Documents emptyMaps;
    const auto& maps = context.profile.soundmaps ? *context.profile.soundmaps : emptyMaps;
    const auto instruments = playbackRouteInstruments(context);
    for (const auto& name : context.document->getOthers()->getArray<PlaybackNameTarget>(musx::dom::SCORE_PARTID)) {
        const auto route = context.document->getOthers()->get<PlaybackRouteTarget>(musx::dom::SCORE_PARTID, name->getCmper());
        if (!route) {
            continue;
        }
        auto plugin = playbackBankPlugin(context, static_cast<std::uint16_t>(route->virtualChannel) >> 4);
        if (!plugin.empty() && plugin != "SMSS" && route->soundUuid.empty()) {
            const auto instrument = instruments.find(route->getCmper());
            if (instrument != instruments.end() && !instrument->second.empty()) {
                for (const auto& document : maps) {
                    const auto root = document->getRootElement();
                    if (playbackMapText(root->getFirstChildElement("Description"), "PluginID") != plugin) {
                        continue;
                    }
                    const auto sound = mappedSound(root, instrument->second);
                    const auto uuid = sound ? sound->findAttribute("uuid") : nullptr;
                    if (uuid && !uuid->getValueTrimmed().empty()) {
                        std::const_pointer_cast<PlaybackRouteTarget>(route)->soundUuid = uuid->getValueTrimmed();
                        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                            const auto key = reporting.template instanceKey<PlaybackRouteTarget>(musx::dom::SCORE_PARTID, route->getCmper());
                            reportFallbackField(reporting, key, "soundUuid", Reporting::Origin::LegacyMusAdjusted, 0);
                        });
                        break;
                    }
                }
            }
        }
        const bool requestedSoftSynth = plugin == "SMSS";
        musx::xml::XmlElementPtr selected;
        for (const auto& document : maps) {
            const auto root = document->getRootElement();
            const auto mapPlugin = playbackMapText(root->getFirstChildElement("Description"), "PluginID");
            if (!plugin.empty()) {
                if (mapPlugin == plugin) {
                    selected = root;
                    break;
                }
            } else if (!route->soundUuid.empty()) {
                for (auto sound = root->getFirstChildElement("Sound"); sound; sound = sound->getNextSibling("Sound")) {
                    const auto uuid = sound->findAttribute("uuid");
                    if (uuid && uuid->getValueTrimmed() == route->soundUuid) {
                        selected = root;
                        plugin = mapPlugin;
                        break;
                    }
                }
                if (selected) {
                    break;
                }
            }
        }
        const auto label =
            playback::resolvePluginName(plugin, playbackMapText(selected ? selected->getFirstChildElement("Description") : nullptr, "Name"));
        if (!label.empty()) {
            std::const_pointer_cast<PlaybackNameTarget>(name)->name = label;
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<PlaybackNameTarget>(musx::dom::SCORE_PARTID, name->getCmper());
                if (auto* field = reporting.report().findField(key, "name")) {
                    field->origin = Reporting::Origin::LegacyMusAdjusted;
                }
            });
        }
        if (requestedSoftSynth && selected) {
            // Match the program only; an unmatched program uses the first sound in map order.
            auto selectedSound = selected->getFirstChildElement("Sound");
            for (auto sound = selectedSound; sound; sound = sound->getNextSibling("Sound")) {
                const auto commands = sound->getFirstChildElement("PatchMIDICommands");
                const auto program = commands ? commands->getFirstChildElement("ProgramChange") : nullptr;
                const auto number = program ? program->findAttribute("Number") : nullptr;
                if (number && number->getValueAs<int>() == route->patch) {
                    selectedSound = sound;
                    break;
                }
            }
            const auto uuid = selectedSound ? selectedSound->findAttribute("uuid") : nullptr;
            std::const_pointer_cast<PlaybackRouteTarget>(route)->soundUuid = uuid ? uuid->getValueTrimmed() : std::string{};
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<PlaybackRouteTarget>(musx::dom::SCORE_PARTID, route->getCmper());
                reportFallbackField(reporting, key, "soundUuid", Reporting::Origin::LegacyMusAdjusted, route->patch);
            });
        }
    }
}
} // namespace

void importPlaybackRoutes(const ImportContext& context)
{
    const auto source =
        selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), playbackRouteTag, playbackRouteClass);
    if (!source) {
        return;
    }
    // Preserve stored routes even when their staves are absent from Studio View; deleting them can leave dangling playback references.
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        const auto payload = collectRecordPayload(*source, rows);
        // Comparator zero structurally identifies the packed, six-channel route rows.
        if (cmper == 0) {
            for (std::size_t at = 0; at + 2 <= payload.size(); at += 2) {
                importPlaybackRouteRecord(context, *source, rows, payload, static_cast<musx::dom::Cmper>(at / 2 + 1), at, true);
            }
        } else if (payload.size() >= 12) {
            importPlaybackRouteRecord(context, *source, rows, payload, cmper, 0, false);
        } else {
            playbackWarning(context, "Playback route has an incomplete payload.");
        }
    }
}

void importPlaybackRouteNames(const ImportContext& context)
{
    context.pending.defer(DeferredStage::ClaimStoredIds, [&context] {
        if (context.profile.epoch == FormatEpoch::CodaBanner) {
            synthesizeCodaPlaybackNames(context);
        }
    });
    context.pending.defer(DeferredStage::Synthesize, [&context] { upgradePlaybackNames(context); });
    const auto source =
        selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), playbackNameTag, playbackNameClass);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        const auto payload = collectRecordPayload(*source, rows);
        auto target = createOthersRecordTarget<PlaybackNameTarget>(context.document, *source, rows.front(), cmper);
        if (!target) {
            continue;
        }
        if (sourceAtOrAfter(context.profile, FormatEpoch::ZlibLegacy, VersionBound{17, 0, 0, 2, 60})) {
            std::vector<std::int16_t> units;
            for (std::size_t at = 0; at + 2 <= payload.size(); at += 2) {
                const auto unit = payloadWord(payload, at, context.profile.byteOrder);
                if (!unit) {
                    break;
                }
                units.push_back(static_cast<std::int16_t>(unit));
            }
            target->name = text::utf16ToUtf8(units);
        } else {
            target->name = text::toUtf8(payloadString(payload, 0, payload.size()), context.profile.platform);
        }
        reportPlaybackField(context, *target, *source, rows, "name", 0, 0);
        context.document->getOthers()->add(PlaybackNameTarget::XmlNodeName, std::move(target));
    }
}

void importStaffPlayData(const ImportContext& context)
{
    const auto source =
        selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), staffPlaybackTag, staffPlaybackClass);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        auto payload = collectRecordPayload(*source, rows);
        // Older records may include extra incidences; payload length does not select the expanded layout.
        const bool modern = playbackHasInlinePatch(context.profile);
        const bool early = !modern && context.profile.epoch == FormatEpoch::CodaBanner;
        const auto required = modern ? 48U : early ? 8U : 14U;
        const auto storedSize = payload.size();
        if (payload.size() < required) {
            if (source->classRecords) {
                playbackWarning(context, "Staff playback record has an incomplete payload.");
                continue;
            }
            // Finale loads classic rows into a zeroed buffer, so words past a short record are zero.
            payload.resize(required, 0);
        }
        auto target = createOthersRecordTarget<StaffPlaybackTarget>(context.document, *source, rows.front(), cmper);
        if (!target) {
            continue;
        }
        const auto word = [&](std::size_t offset) { return payloadWord(payload, offset, context.profile.byteOrder); };
        for (std::size_t stream = 0; stream < 2 + target->layers.size(); ++stream) {
            const std::string prefix = stream == 0 ? "chords" : stream == 1 ? "midiExpressions" : "layers[" + std::to_string(stream - 2) + "]";
            auto settings = std::make_shared<StaffPlaybackTarget::PlaybackSettings>(target);
            settings->velocityControl = std::make_shared<StaffPlaybackTarget::VelocityControl>(settings);
            const std::array<std::size_t, 6> oldSlots{4, 5, 0, 1, 2, 3};
            const std::array<std::size_t, 6> earlySlots{2, 3, 0, 1, 0, 0};
            const auto routeOffset = modern ? stream * 8 : (early ? earlySlots[stream] : oldSlots[stream]) * 2;
            settings->playbackRouteIndex = static_cast<std::int16_t>(word(routeOffset));
            const auto playbackFlagsOffset = modern ? stream * 8 + 6 : 12;
            const auto playFlags = modern ? word(playbackFlagsOffset) : early ? 0x5055 : word(playbackFlagsOffset);
            const auto shift = modern ? 0 : stream < 2 ? 12 + stream * 2 : (stream - 2) * 2;
            settings->play = (playFlags & (1U << shift)) != 0;
            settings->solo = (playFlags & (2U << shift)) != 0;
            settings->velocityControlLevel = modern ? static_cast<std::int16_t>(word(stream * 8 + 2)) : 101;
            const auto velocityFlags = modern ? word(stream * 8 + 4) : 0x21;
            settings->velocityControl->controlOn = (velocityFlags & 1) != 0;
            settings->velocityControl->function = (velocityFlags & 0xf0) == 0x20 ? StaffPlaybackTarget::VelocityControlFunction::Relative
                                                                                 : StaffPlaybackTarget::VelocityControlFunction::Absolute;
            const auto report = [&](const char* member, std::size_t at, std::int64_t raw, PlaybackFieldSource kind = PlaybackFieldSource::Stored) {
                const bool padded = kind == PlaybackFieldSource::Stored && at + 2 > storedSize;
                reportPlaybackField(context, *target, *source, rows, prefix + "." + member, at, raw, padded ? PlaybackFieldSource::Behavior : kind);
            };
            report("playbackRouteIndex", routeOffset, settings->playbackRouteIndex);
            report("play", early ? 0 : playbackFlagsOffset, playFlags, early ? PlaybackFieldSource::Behavior : PlaybackFieldSource::Stored);
            report("solo", early ? 0 : playbackFlagsOffset, playFlags, early ? PlaybackFieldSource::Behavior : PlaybackFieldSource::Stored);
            const auto velocityOrigin = modern ? PlaybackFieldSource::Stored : PlaybackFieldSource::Behavior;
            report("velocityControlLevel", modern ? stream * 8 + 2 : 0, settings->velocityControlLevel, velocityOrigin);
            report("velocityControl.controlOn", modern ? stream * 8 + 4 : 0, velocityFlags, velocityOrigin);
            const auto function = velocityFlags & 0xf0;
            report("velocityControl.function", modern ? stream * 8 + 4 : 0, velocityFlags,
                function == 0 || function == 0x10 || function == 0x20 ? velocityOrigin : PlaybackFieldSource::Unmapped);
            if (function != 0 && function != 0x10 && function != 0x20) {
                playbackWarning(context, "Staff playback has an unknown velocity function.");
            }
            if (stream == 0) {
                target->chords = std::move(settings);
            } else if (stream == 1) {
                target->midiExpressions = std::move(settings);
            } else {
                target->layers[stream - 2] = std::move(settings);
            }
        }
        context.document->getOthers()->add(StaffPlaybackTarget::XmlNodeName, std::move(target));
    }
}
} // namespace others
} // namespace finale_mus_reader
