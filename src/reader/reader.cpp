// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "finale_mus_reader/reader.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "container/mus_container.h"
#include "musx/util/Logger.h"
#include "reader/document_factory.h"
#include "reader/resources.h"
#include "reader/timing.h"

namespace finale_mus_reader {
namespace {

ImportResult readImpl(std::span<const std::uint8_t> data, const container::ParsedContainer& parsed,
    const std::optional<std::filesystem::path>& sourcePath, const detail::ReaderResources& resources, XmlParser parseXml,
    DocumentParser parseDocument, musx::dom::PartVoicingPolicy partVoicingPolicy)
{
    ImportResult result(parsed.formatEpoch);
    {
        FINALE_MUS_READER_TIMED_SCOPE(timing::Phase::SourceReport);
        result.report.formatEpoch = parsed.formatEpoch;
        result.report.byteOrder = parsed.byteOrder;
        result.report.sourceSize = data.size();
        describeSourceIdentity(data.data(), data.size(), result.report);
        for (const auto& block : parsed.blocks) {
            result.report.blocks.push_back(block.info);
        }
        if (parsed.trailingByteCount != 0) {
            result.report.diagnostics.push_back({musx::util::Logger::LogLevel::Verbose,
                "Preserved classification after a terminal block with " + std::to_string(parsed.trailingByteCount) + " trailing bytes."});
        }
    }

    result.document = createDocument(parsed, data.data(), data.size(), sourcePath, resources, parseXml, parseDocument, result.report,
        DeferredOrder::AsRegistered, partVoicingPolicy);

    // Each diagnostic goes out at its own level. Forwarding them all as warnings was what
    // made a routine fallback indistinguishable from an unreadable document.
    {
        FINALE_MUS_READER_TIMED_SCOPE(timing::Phase::DiagnosticDelivery);
        for (const auto& diagnostic : result.report.diagnostics) {
            musx::util::Logger::log(diagnostic.level, diagnostic.message);
        }
    }
    return result;
}

// Runs an import after container classification and converts a thrown recovery failure into
// a returned one. Classification and file-access failures occur before this boundary and
// propagate to the caller because no valid source epoch exists for their report.
//
// Error is the only level that can be asserted rather than judged: inside this catch there
// is provably no document, because the sole path that produces one has already unwound. The
// caller sees that as a null `document`, so a failed import is checked the same way a
// successful one is used, and the reason is carried in the report alongside every other
// diagnostic instead of arriving through a separate channel.
//
// Failure is returned rather than propagated because this reader's whole purpose is
// rescuing damaged and obsolete documents: a caller sweeping thousands of files should not
// have to wrap each one to keep going, and a batch run stopping on its first bad file is
// the wrong default for that job.
template <typename Body>
ImportResult runGuarded(FormatEpoch epoch, Body&& body)
{
    try {
        return body();
    } catch (const std::exception& error) {
        musx::util::Logger::log(musx::util::Logger::LogLevel::Error, error.what());
        ImportResult failed(epoch);
        failed.report.diagnostics.push_back({musx::util::Logger::LogLevel::Error, error.what()});
        return failed;
    } catch (...) {
        constexpr auto unknown = "MUS import failed with an unrecognized exception.";
        musx::util::Logger::log(musx::util::Logger::LogLevel::Error, unknown);
        ImportResult failed(epoch);
        failed.report.diagnostics.push_back({musx::util::Logger::LogLevel::Error, unknown});
        return failed;
    }
}

} // namespace

Reader Reader::createWithParser(const ReaderOptions& options, XmlParser parseXml, DocumentParser parseDocument)
{
    return Reader(std::make_shared<detail::ReaderResources>(detail::prepareReaderResources(options, parseXml)), parseXml, parseDocument);
}

musx::dom::DocumentPtr Reader::read(const std::filesystem::path& path, const ImportOptions& importOptions) const
{
    return readPrepared(path, importOptions).document;
}

musx::dom::DocumentPtr Reader::read(std::span<const std::uint8_t> data, const ImportOptions& importOptions) const
{
    return readPrepared(data, importOptions).document;
}

MusRecognitionResult recognize(std::span<const std::uint8_t> data) noexcept
{
    return container::recognize(data.data(), data.size());
}

#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
ImportResult Reader::readWithReport(const std::filesystem::path& path, const ImportOptions& importOptions) const
{
    return readPrepared(path, importOptions);
}

ImportResult Reader::readWithReport(std::span<const std::uint8_t> data, const ImportOptions& importOptions) const
{
    return readPrepared(data, importOptions);
}
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)

ImportResult Reader::readPrepared(const std::filesystem::path& path, const ImportOptions& importOptions) const
{
    auto data = [&] {
        FINALE_MUS_READER_TIMED_SCOPE(timing::Phase::FileIo);
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input) {
            throw std::runtime_error("Unable to open MUS input: " + path.string());
        }
        const auto end = input.tellg();
        if (end <= 0) {
            throw std::invalid_argument("MUS input is empty: " + path.string());
        }
        const auto unsignedSize = static_cast<std::uintmax_t>(end);
        if (unsignedSize > (std::numeric_limits<std::size_t>::max)()
            || unsignedSize > static_cast<std::uintmax_t>((std::numeric_limits<std::streamsize>::max)())) {
            throw std::length_error("MUS input is too large for this platform");
        }
        std::vector<std::uint8_t> result(static_cast<std::size_t>(unsignedSize));
        input.seekg(0);
        input.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(result.size()));
        if (!input) {
            throw std::runtime_error("Unable to read complete MUS input: " + path.string());
        }
        return result;
    }();
    const auto parsed = [&] {
        FINALE_MUS_READER_TIMED_SCOPE(timing::Phase::ContainerParse);
        return container::parse(data.data(), data.size());
    }();
    return runGuarded(parsed.formatEpoch, [&] {
        return readImpl(
            data, parsed, importOptions.sourcePath.value_or(path), *m_resources, m_parseXml, m_parseDocument, importOptions.partVoicingPolicy);
    });
}

ImportResult Reader::readPrepared(std::span<const std::uint8_t> data, const ImportOptions& importOptions) const
{
    if (data.empty()) {
        throw std::invalid_argument("MUS input is empty");
    }
    const auto parsed = [&] {
        FINALE_MUS_READER_TIMED_SCOPE(timing::Phase::ContainerParse);
        return container::parse(data.data(), data.size());
    }();
    return runGuarded(parsed.formatEpoch,
        [&] { return readImpl(data, parsed, importOptions.sourcePath, *m_resources, m_parseXml, m_parseDocument, importOptions.partVoicingPolicy); });
}

} // namespace finale_mus_reader
