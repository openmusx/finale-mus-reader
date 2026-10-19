// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

namespace finale_mus_reader {

/// @brief The order in which the steps within one deferred stage run.
/// @details Stages always run in order. Reversing the steps within each stage exposes a step that
/// depends on another step of its own stage without declaring it; a correct import produces the
/// same document either way.
enum class DeferredOrder : std::uint8_t {
    AsRegistered,
    ReversedWithinStage,
};

} // namespace finale_mus_reader
