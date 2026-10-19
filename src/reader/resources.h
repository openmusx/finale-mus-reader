// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include "import/support/instrument_catalog.h"
#include "import/support/percussion_mappings.h"
#include "import/support/soundmaps.h"
#include "import/support/text_encoding.h"

namespace finale_mus_reader {
namespace detail {

struct ReaderResources
{
    text::SymbolFontNames symbolFontNames;
    percussion::MappingTables percussionMappings;
    soundmap::Documents soundmaps;
    playback::InstrumentCatalog instrumentCatalog;
};

[[nodiscard]] ReaderResources prepareReaderResources(const ReaderOptions& options, XmlParser parseXml);

} // namespace detail
} // namespace finale_mus_reader
