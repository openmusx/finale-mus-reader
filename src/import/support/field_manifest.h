// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "musx/musx.h"

// Field manifests shared by an importer and the instrumentation that exhaustively
// observes its musxdom target. This is a private reader interface, not public API.

namespace finale_mus_reader {
namespace options {

using MusicSymbolOptionsTarget = musx::dom::options::MusicSymbolOptions;
using MusicSymbolFontType = musx::dom::options::FontOptions::FontType;

struct NarrowMusicSymbolSource
{
    std::uint16_t selector;
    std::size_t word;
};

enum class NarrowMusicSymbolEra {
    Any,
    AfterCoda,
    Finale35AndLater,
    Finale351AndLater,
    Finale97AndLater,
    ZlibOnly,
};

enum class SharedMusicSymbolEra {
    None,
    CodaOnly,
    PreZlib,
};

struct MusicSymbolOptionsField
{
    std::string_view memberName;
    std::string_view leafName;
    char32_t MusicSymbolOptionsTarget::* member;
    MusicSymbolFontType fontType;
    std::optional<char32_t MusicSymbolOptionsTarget::*> sharedSource;
    SharedMusicSymbolEra sharedEra = SharedMusicSymbolEra::None;
    NarrowMusicSymbolSource narrowSource;
    NarrowMusicSymbolEra narrowEra = NarrowMusicSymbolEra::Any;
};

std::span<const MusicSymbolOptionsField> musicSymbolOptionsFields();

} // namespace options

namespace others {

template <typename Stored, typename Behavior>
void reportFretInstrumentFields(const musx::dom::others::FretInstrument& target, bool storedStructure, Stored&& stored, Behavior&& behavior)
{
    const auto structure = [&](std::string member, auto value) {
        if (storedStructure) {
            stored(std::move(member), value);
        } else {
            behavior(std::move(member), value);
        }
    };
    structure("numFrets", target.numFrets);
    structure("numStrings", target.numStrings);
    structure("speedyClef", target.speedyClef);
    for (std::size_t index = 0; index < target.strings.size(); ++index) {
        stored("strings[" + std::to_string(index) + "].pitch", target.strings[index]->pitch);
        behavior("strings[" + std::to_string(index) + "].nutOffset", target.strings[index]->nutOffset);
    }
    for (std::size_t index = 0; index < target.fretSteps.size(); ++index) {
        stored("fretSteps[" + std::to_string(index) + "]", target.fretSteps[index]);
    }
}

} // namespace others
} // namespace finale_mus_reader
