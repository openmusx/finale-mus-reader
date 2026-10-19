// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/support/playback_plugin_names.h"

#include <array>
#include <utility>

namespace finale_mus_reader {
namespace playback {
namespace {

constexpr std::array<std::pair<std::string_view, std::string_view>, 1> playbackPluginNames{{
    {"GARA", "ARIA Player"},
}};

} // namespace

std::string resolvePluginName(std::string_view pluginId, std::string_view soundmapDescription)
{
    for (const auto& [id, name] : playbackPluginNames) {
        if (pluginId == id) {
            return std::string(name);
        }
    }
    return std::string(soundmapDescription);
}

} // namespace playback
} // namespace finale_mus_reader
