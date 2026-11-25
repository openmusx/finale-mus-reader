// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "musx/dom/PlaybackResources.h"

namespace finale_mus_reader {
namespace percussion {

/// @brief Finale's legacy percussion-map conversion table: for each legacy layout name, the
/// device files and note-name lists it converts to, in table order.
class ConversionTable
{
public:
    /// @brief Returns the note-name list a legacy layout name converts to: the first table
    /// entry for that name whose device file and list are in @p resources.
    /// @param resources The caller's playback resources. May be null, which finds no list.
    [[nodiscard]] const musx::dom::DeviceNoteList* conversionTarget(std::string_view layoutName, const musx::dom::PlaybackResources* resources) const;

private:
    std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> m_conversions;

    friend ConversionTable parseConversionTable(std::span<const std::uint8_t>);
};

/// @brief Parses the contents of `PercMapConversionTable.txt`.
/// @details Each nonblank line other than a `//` comment has three comma-separated columns:
/// legacy layout name, device file name, and note-name list name. A backslash escapes a comma
/// or a backslash. A leading UTF-8 byte-order mark is ignored.
/// @throws std::invalid_argument if a row does not have three nonempty columns.
[[nodiscard]] ConversionTable parseConversionTable(std::span<const std::uint8_t> contents);

} // namespace percussion
} // namespace finale_mus_reader
