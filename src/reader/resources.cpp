// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "reader/resources.h"

#include <stdexcept>

namespace finale_mus_reader {
namespace detail {

ReaderResources prepareReaderResources(const ReaderOptions& options, XmlParser parseXml)
{
    ReaderResources result;
    result.symbolFontNames = text::parseMacSymbolFonts(options.macSymbolFonts);
    result.percussionMappings = percussion::parseMappingTables(options.percussionMappingXml, options.percussionMapConversionTable, parseXml);
    for (const auto bytes : options.soundmapXml) {
        if (bytes.empty()) {
            continue;
        }
        auto xml = parseXml(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        if (!xml || !xml->getRootElement() || xml->getRootElement()->getTagName() != "Soundmap") {
            throw std::invalid_argument("Sound-map XML must have a Soundmap root");
        }
        result.soundmaps.push_back(std::move(xml));
    }
    if (!options.instrumentCatalogXml.empty()) {
        const auto bytes = options.instrumentCatalogXml;
        auto xml = parseXml(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        if (!xml || !xml->getRootElement() || xml->getRootElement()->getTagName() != "InstrumentCatalog") {
            throw std::invalid_argument("Instrument catalog XML must have an InstrumentCatalog root");
        }
        for (auto item = xml->getRootElement()->getFirstChildElement("Instrument"); item; item = item->getNextSibling("Instrument")) {
            const auto attribute = [&](const char* name) {
                const auto value = item->findAttribute(name);
                if (!value || value->getValueTrimmed().empty()) {
                    throw std::invalid_argument(std::string("Instrument catalog entry is missing ") + name);
                }
                return value->getValueTrimmed();
            };
            playback::Instrument instrument;
            instrument.uuid = attribute("uuid");
            instrument.name = attribute("name");
            instrument.fullName = attribute("fullName");
            instrument.clef = attribute("clef");
            instrument.notation = attribute("notation");
            instrument.transposition = std::stoi(attribute("transposition"));
            result.instrumentCatalog.push_back(std::move(instrument));
        }
    }
    return result;
}

} // namespace detail
} // namespace finale_mus_reader
