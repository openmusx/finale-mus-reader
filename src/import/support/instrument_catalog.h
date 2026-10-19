// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <vector>

namespace finale_mus_reader {
namespace playback {

struct Instrument
{
    std::string uuid;
    std::string name;
    std::string fullName;
    std::string clef;
    std::string notation;
    int transposition{};
};

using InstrumentCatalog = std::vector<Instrument>;

} // namespace playback
} // namespace finale_mus_reader
