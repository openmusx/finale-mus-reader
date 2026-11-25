// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include "import/support/percussion_mappings.h"
#include "import/support/text_encoding.h"

namespace finale_mus_reader {
namespace detail {

struct ReaderResources
{
    text::SymbolFontNames symbolFontNames;
    percussion::ConversionTable percussionConversions;
    /// @brief The caller's playback resources, shared with every created document. May be null.
    musx::dom::PlaybackResourcesPtr playbackResources;
};

[[nodiscard]] ReaderResources prepareReaderResources(const ReaderOptions& options);

} // namespace detail
} // namespace finale_mus_reader
