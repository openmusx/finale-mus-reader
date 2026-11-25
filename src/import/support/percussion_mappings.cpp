// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/support/percussion_mappings.h"

#include <array>
#include <stdexcept>

#include "import/support/text_encoding.h"

namespace finale_mus_reader {
namespace percussion {
namespace {

std::string_view trimMappingField(std::string_view value)
{
    while (!value.empty() && text::isSpace(value.front())) {
        value.remove_prefix(1);
    }
    while (!value.empty() && text::isSpace(value.back())) {
        value.remove_suffix(1);
    }
    return value;
}

std::array<std::string, 3> parseConversionRow(std::string_view line)
{
    std::array<std::string, 3> fields;
    std::size_t column = 0;
    bool escaped = false;
    for (const char value : line) {
        if (escaped) {
            if (value != ',' && value != '\\') {
                fields[column].push_back('\\');
            }
            fields[column].push_back(value);
            escaped = false;
        } else if (value == '\\') {
            escaped = true;
        } else if (value == ',') {
            if (++column == fields.size()) {
                throw std::invalid_argument("Percussion map conversion row has too many columns");
            }
        } else {
            fields[column].push_back(value);
        }
    }
    if (escaped) {
        fields[column].push_back('\\');
    }
    if (column + 1 != fields.size()) {
        throw std::invalid_argument("Percussion map conversion row must have three columns");
    }
    for (auto& field : fields) {
        field = std::string(trimMappingField(field));
        if (field.empty()) {
            throw std::invalid_argument("Percussion map conversion row has an empty column");
        }
    }
    return fields;
}

} // namespace

const musx::dom::DeviceNoteList* ConversionTable::conversionTarget(std::string_view layoutName, const musx::dom::PlaybackResources* resources) const
{
    if (!resources) {
        return nullptr;
    }
    const auto conversions = m_conversions.find(std::string(trimMappingField(layoutName)));
    if (conversions == m_conversions.end()) {
        return nullptr;
    }
    for (const auto& [fileName, listName] : conversions->second) {
        if (const auto list = resources->findNoteList(fileName, listName)) {
            return list.value;
        }
    }
    return nullptr;
}

ConversionTable parseConversionTable(std::span<const std::uint8_t> conversionTable)
{
    ConversionTable result;
    if (!conversionTable.empty()) {
        std::string_view contents(reinterpret_cast<const char*>(conversionTable.data()), conversionTable.size());
        if (contents.starts_with("\xef\xbb\xbf")) {
            contents.remove_prefix(3);
        }
        while (!contents.empty()) {
            const auto end = contents.find('\n');
            const auto line = trimMappingField(contents.substr(0, end));
            if (!line.empty() && !line.starts_with("//")) {
                const auto [legacyName, filename, listName] = parseConversionRow(line);
                result.m_conversions[legacyName].emplace_back(filename, listName);
            }
            if (end == std::string_view::npos) {
                break;
            }
            contents.remove_prefix(end + 1);
        }
    }
    return result;
}

} // namespace percussion
} // namespace finale_mus_reader
