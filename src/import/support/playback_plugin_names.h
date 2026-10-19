// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <string_view>

namespace finale_mus_reader {
namespace playback {

/// @brief Resolves a player name from its plugin ID, falling back to the selected
/// sound map's description name. Returns empty when neither supplies a name.
[[nodiscard]] std::string resolvePluginName(std::string_view pluginId, std::string_view soundmapDescription);

} // namespace playback
} // namespace finale_mus_reader
