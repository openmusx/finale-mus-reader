// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "records/legacy_others.h"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <tuple>

namespace finale_mus_reader {
namespace records {
namespace {

std::uint16_t readUnsignedWord(const std::uint8_t* data, ByteOrder byteOrder)
{
    if (byteOrder == ByteOrder::BigEndian) {
        return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[0]) << 8U) | data[1]);
    }
    return static_cast<std::uint16_t>(data[0] | (static_cast<std::uint16_t>(data[1]) << 8U));
}

} // namespace

std::vector<LegacyOther> decodeLegacyOthers(const container::ParsedContainer& parsed)
{
    std::uint16_t otherBlockType{};
    if (parsed.formatEpoch == FormatEpoch::UncompressedLegacy) {
        otherBlockType = 0x0001;
    } else if (parsed.formatEpoch == FormatEpoch::DclLegacy) {
        otherBlockType = 0x000f;
    } else {
        return {};
    }

    std::vector<LegacyOther> result;
    std::map<std::pair<std::string, std::uint16_t>, std::uint32_t> nextIncident;
    for (const auto& block : parsed.blocks) {
        if (block.info.type != otherBlockType) {
            continue;
        }
        if (block.data.size() % 16 != 0) {
            throw std::runtime_error("Legacy other pool is not a multiple of 16 bytes");
        }
        for (std::size_t offset = 0; offset < block.data.size(); offset += 16) {
            LegacyOther record;
            record.cmper = readUnsignedWord(block.data.data() + offset, parsed.byteOrder);
            record.tag.assign(reinterpret_cast<const char*>(block.data.data() + offset + 2), 2);
            std::copy_n(block.data.begin() + static_cast<std::ptrdiff_t>(offset + 4), record.payload.size(), record.payload.begin());
            record.blockOffset = block.info.sourceOffset;
            record.decodedOffset = offset;
            auto& incident = nextIncident[{record.tag, record.cmper}];
            record.incident = incident++;
            result.push_back(std::move(record));
        }
    }
    return result;
}

std::int16_t readPayloadWord(const LegacyOther& record, std::size_t wordIndex, ByteOrder byteOrder)
{
    if (wordIndex >= record.payload.size() / 2) {
        throw std::out_of_range("Legacy payload word index is out of range");
    }
    return static_cast<std::int16_t>(readUnsignedWord(record.payload.data() + wordIndex * 2, byteOrder));
}

} // namespace records
} // namespace finale_mus_reader
