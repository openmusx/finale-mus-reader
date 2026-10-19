// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "container/mus_container.h"
#include "records/legacy_record_index.h"

namespace finale_mus_reader {
namespace records {

struct LegacyOther
{
    std::uint16_t cmper{};
    std::string tag;
    std::uint32_t incident{};
    std::array<std::uint8_t, otherInciByteCount> payload{};
    std::size_t blockOffset{};
    std::size_t decodedOffset{};
};

[[nodiscard]] std::vector<LegacyOther> decodeLegacyOthers(const container::ParsedContainer& parsed);

[[nodiscard]] std::int16_t readPayloadWord(const LegacyOther& record, std::size_t wordIndex, ByteOrder byteOrder);

} // namespace records
} // namespace finale_mus_reader
