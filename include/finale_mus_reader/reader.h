// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "musx/dom/Document.h"
#include "musx/factory/DocumentFactory.h"
#include "musx/util/Logger.h"
#include "musx/xml/XmlInterface.h"

namespace finale_mus_reader {

namespace detail {
struct ReaderResources;
} // namespace detail

/// @brief Parses XML with the backend selected by the caller.
/// @details Used for embedded EnigmaXML defaults and caller-supplied XML resources.
using XmlParser = std::unique_ptr<musx::xml::IXmlDocument> (*)(const char* data, std::size_t size);

/// @brief Parses a complete EnigmaXML document with the caller's selected XML backend.
/// @details The pinned baseline uses this to retain a fully formed reference document
/// alongside the filtered pools copied into the import target.
using DocumentParser = musx::dom::DocumentPtr (*)(const char* data, std::size_t size);

enum class FormatEpoch {
    /// @brief The era before the `ENIGMA BINARY FILE` signature existed.
    /// @details These files are not headerless. They open with a plain-text product
    /// banner of the form `Finale(TM) 2.6 Copyright 1987 by Coda.`, which is where their
    /// version lives, and they still reserve a 0x200 header. Later eras use `Finale(R)`
    /// and name Coda too, so the distinguishing marks are the `(TM)` spelling and the
    /// absent signature.
    CodaBanner,
    UncompressedLegacy,
    DclLegacy,
    ZlibLegacy
};

enum class ByteOrder {
    Unknown,
    LittleEndian,
    BigEndian
};

enum class SourcePlatform {
    Unknown,
    MacOS,
    Windows
};

#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
enum class ValueOrigin {
    /// @brief No legacy source for this field has been located.
    /// @details The value remains default-initialized for a source-owned object or retains its
    /// seeded value for an options object, so what accompanies this origin is a placeholder that
    /// no evidence supports rather than a value anything determined.
    ///
    /// It covers both a field with no known source and a field whose known records demonstrably do
    /// not carry it: both leave a value with no evidence behind it, and either may yet be recovered
    /// from a record this library does not decode. The two are not distinguished here.
    ///
    /// It differs from @ref Finale27Default, which is the default provenance of a retained seeded
    /// value, and from @ref MusxOnly, which identifies a field known to postdate every supported
    /// legacy layout and therefore recoverable from nowhere.
    Unmapped,
    /// @brief The field postdates every supported legacy MUS layout.
    /// @details No source value can be recovered. The value remains default-initialized for a
    /// source-owned object or retains its seeded value for an options object.
    MusxOnly,
    /// @brief The source carries the field, but its recovery is deliberately excluded.
    /// @details The value remains default-initialized for a source-owned object or retains its
    /// seeded value for an options object. Unlike @ref Unmapped, the source of the value is known.
    OutOfScope,
    /// @brief Read from the source file's own bytes.
    LegacyMus,
    /// @brief Read from the source file and adjusted for source-era behavior.
    /// @details The raw stored value and its offsets remain available in @ref FieldInfo, but
    /// the document receives a semantically equivalent value in the modern representation.
    LegacyMusAdjusted,
    /// @brief Supplied from how the source version behaved, because it had no option to
    /// store.
    /// @details A field that later Finale versions expose as a setting is often fixed
    /// behavior in an earlier one. Such a value is neither read from the file nor guessed:
    /// the era's behavior determines it, and the version or epoch that introduced the
    /// setting is the boundary. Reporting it as a synthesized default would understate it,
    /// and reporting it as recovered would claim bytes that do not exist.
    ///
    /// Coda-banner clef courtesies are the motivating case. No version before 3.6.2 offers
    /// the option and every such document shows a courtesy clef, so the value is known
    /// exactly while nothing in the file records it.
    LegacyBehavior,
    /// @brief Retained from the pinned Finale 27 baseline because no explicit origin replaced it.
    Finale27Default
};
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)

/// @brief A Finale version recorded in the file.
/// @details Signature-era files store each version as a 32-bit value in the file's own
/// byte order, packed as major in bits 31-24, minor in bits 23-20, maintenance in bits
/// 19-16, a development-status code in bits 15-8, and build in bits 7-0. Finale 97
/// records an application version of `0x03820401`, which is 3.8.2 build 1, matching what
/// Finale 27 reports as the creator version when it converts such a file.
///
/// This is the same packing the running application reports to plug-ins, so the two are
/// directly comparable.
///
/// Major versions run 0-27 across Finale's history and do not order it by themselves:
/// Finale 3.2 through 3.7 and Finale 97 all carry major 3, separated only by minor.
struct SourceVersion
{
    /// @brief The 32-bit value in logical order, or zero when the version came from a
    /// @ref FormatEpoch::CodaBanner product string rather than a header tuple.
    std::uint32_t raw{};
    std::uint8_t major{};
    std::uint8_t minor{};
    std::uint8_t maint{};
    /// @brief Development-status code: 2 is musxdom's `beta` and 0 its `dev`. The remaining
    /// values, `4` among them, are not yet mapped.
    std::uint8_t devStatus{};
    std::uint8_t build{};
};

struct BlockInfo
{
    std::uint16_t type{};
    std::size_t sourceOffset{};
    std::size_t storedSize{};
    std::size_t decodedSize{};
    bool checksumPresent{};
    bool checksumValid{};
    /// @brief The block's payload is held verbatim rather than decompressed.
    /// @details The two terminal block types are usually empty markers, but when a document
    /// embeds a graphic they carry its bytes, uncompressed and with no checksum word. Such a
    /// block is reported so a caller can see that the document contains data this reader
    /// preserves but does not yet interpret.
    bool stored{};
};

#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
struct InstanceKey
{
    std::type_index classType{typeid(void)};
    musx::dom::Cmper partId = musx::dom::SCORE_PARTID;
    std::optional<musx::dom::Cmper> cmper1;
    std::optional<musx::dom::Inci> inci;
    std::optional<musx::dom::Cmper> cmper2;

    bool operator==(const InstanceKey&) const = default;
};

struct InstanceKeyHash
{
    std::size_t operator()(const InstanceKey& key) const noexcept
    {
        auto result = key.classType.hash_code();
        const auto combine = [&](std::size_t value) { result ^= value + 0x9e3779b9U + (result << 6U) + (result >> 2U); };
        combine(static_cast<std::size_t>(key.partId));
        combine(key.cmper1 ? static_cast<std::size_t>(*key.cmper1) + 1U : 0U);
        combine(key.inci ? static_cast<std::size_t>(*key.inci) + 1U : 0U);
        combine(key.cmper2 ? static_cast<std::size_t>(*key.cmper2) + 1U : 0U);
        return result;
    }
};

template <typename T>
[[nodiscard]] InstanceKey instanceKey(musx::dom::Cmper partId = musx::dom::SCORE_PARTID, std::optional<musx::dom::Cmper> cmper1 = std::nullopt,
    std::optional<musx::dom::Inci> inci = std::nullopt, std::optional<musx::dom::Cmper> cmper2 = std::nullopt)
{
    return {typeid(T), partId, cmper1, inci, cmper2};
}

struct FieldInfo
{
    FieldInfo() = default;
    FieldInfo(ValueOrigin fieldOrigin, std::size_t fieldBlockOffset, std::size_t fieldDecodedOffset, std::int64_t fieldRawValue,
        std::optional<std::uint16_t> fieldSourceIdentity = std::nullopt)
        : origin(fieldOrigin),
          blockOffset(fieldBlockOffset),
          decodedOffset(fieldDecodedOffset),
          rawValue(fieldRawValue),
          sourceIdentity(fieldSourceIdentity)
    {}

    ValueOrigin origin = ValueOrigin::Finale27Default;
    std::size_t blockOffset{};
    std::size_t decodedOffset{};
    std::int64_t rawValue{};
    /// @brief The normalized tag or class id of the record that supplied the value.
    std::optional<std::uint16_t> sourceIdentity;
#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
    /// @brief A source-derived value that identifies a known lossy Finale upgrade substitution.
    std::optional<std::int64_t> finaleUpgradeLossValue;
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
};

/// @brief Provenance for formatting commands completed on one imported text field.
/// @details Each flag is independent: a legacy string can omit any subset of the font, size,
/// and effects commands from its initial formatting run.
struct TextFieldInfo
{
    /// @brief The reader supplied the initial font command from the text class default.
    bool fontWasSynthesized{};
    /// @brief The reader supplied the initial size command from the text class default.
    bool sizeWasSynthesized{};
    /// @brief The reader supplied the initial effects command from the text class default.
    bool effectsWereSynthesized{};
};

/// @brief Instrumentation audit for every source family that produces staff-style assignments.
/// @details Producers add the number of assignments their source structure requires. After all
/// importers run, the audit records the number actually constructed and is marked complete only
/// when every staff agrees. A present entry with zero expected assignments identifies malformed
/// source material rather than proving that the staff had no assignment source.
struct StaffStyleAssignmentAudit
{
    musx::dom::Cmper partId = musx::dom::SCORE_PARTID;
    musx::dom::Cmper staffId{};
    std::size_t expectedAssignments{};
    std::size_t constructedAssignments{};
    bool malformedSource{};
};
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)

/// @brief One message about the import, with the level that decides where it surfaces.
/// @details The level is musxdom's own @c Logger::LogLevel rather than a parallel enum, so a
/// host can forward a diagnostic straight to its logging callback without translating.
///
/// Choosing it is a statement about the state the document is left in, not about how
/// interesting the cause was:
///
/// - @c Verbose: a designed-in fallback took effect and the document is complete.
///   Substituting a reference value the source could not supply is routine, and surfacing it
///   where a user sees it presents normal operation as a problem. The per-field
///   @ref ValueOrigin already carries that fact in a form a caller can act on.
/// - @c Info: the document is usable, but something in the source did not come across.
/// - @c Warning: the document is likely unusable, or content the user had is gone. A
///   considered choice that leaves a usable document is never a warning, however unusual
///   the input was.
struct Diagnostic
{
    musx::util::Logger::LogLevel level = musx::util::Logger::LogLevel::Warning;
    std::string message;
};

struct ImportReport
{
    explicit ImportReport(FormatEpoch epoch)
        : formatEpoch(epoch)
    {}

    FormatEpoch formatEpoch;
    ByteOrder byteOrder = ByteOrder::Unknown;
    SourcePlatform sourcePlatform = SourcePlatform::Unknown;
    /// @brief The pinned Finale 27 baseline that supplied the synthesized defaults.
    /// @details Never `Unknown`: a baseline is always selected, matching
    /// #sourcePlatform when that is known.
    SourcePlatform defaultsPlatform = SourcePlatform::MacOS;
    /// @brief The Enigma version recorded by the last application to save the file.
    /// @details Absent when the file has no banner, or when the recovered major version
    /// falls outside Finale's 0-27 range, which means the header layout was not what was
    /// expected. A source gate that needs a version fails when this is absent.
    std::optional<SourceVersion> sourceVersion;
    /// @brief The Enigma version recorded by the application that originally authored the file.
    /// @details Distinct from #sourceVersion, which names the release that last wrote the file and
    /// therefore the layout on disk. The two differ on a back-save: a later Finale can write a
    /// document out in an earlier format, and only this field says which release actually made it.
    /// Absent under the same conditions as #sourceVersion.
    std::optional<SourceVersion> creatorVersion;
    std::size_t sourceSize{};
    std::string banner;
    std::string savingProduct;
    std::vector<BlockInfo> blocks;
#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
    using InstanceFields = std::unordered_map<std::string, FieldInfo>;
    std::unordered_map<InstanceKey, InstanceFields, InstanceKeyHash> fields;
    /// @brief Provenance shared by every field of a document-pool instance.
    std::unordered_map<InstanceKey, ValueOrigin, InstanceKeyHash> instanceOrigins;
    /// @brief Text conversion provenance keyed first by class instance, then member.
    std::unordered_map<InstanceKey, std::unordered_map<std::string, TextFieldInfo>, InstanceKeyHash> textFields;
    /// @brief Selector-neutral completeness accounting for staff-style assignment importers.
    std::vector<StaffStyleAssignmentAudit> staffStyleAssignmentAudits;
    /// @brief True only after assignment counts were reconciled against the constructed document.
    bool staffStyleAssignmentAuditComplete{};
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
    std::vector<Diagnostic> diagnostics;

#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
    FieldInfo& setField(const InstanceKey& instance, std::string member, FieldInfo info)
    {
        return fields[instance].insert_or_assign(std::move(member), std::move(info)).first->second;
    }

    /// @brief Returns a field's provenance, creating its retained-default record if necessary.
    /// @details An explicit field report replaces this record. Otherwise an instance origin is
    /// inherited when one exists, and a seeded field retains @ref ValueOrigin::Finale27Default.
    FieldInfo& fieldProvenance(const InstanceKey& instance, std::string_view member)
    {
        auto [found, inserted] = fields[instance].try_emplace(std::string(member));
        if (inserted) {
            if (const auto* origin = findInstanceOrigin(instance)) {
                found->second.origin = *origin;
            }
        }
        return found->second;
    }

    void setInstanceOrigin(const InstanceKey& instance, ValueOrigin origin) { instanceOrigins.insert_or_assign(instance, origin); }

    [[nodiscard]] const ValueOrigin* findInstanceOrigin(const InstanceKey& instance) const
    {
        const auto found = instanceOrigins.find(instance);
        return found == instanceOrigins.end() ? nullptr : &found->second;
    }

    void setTextField(const InstanceKey& instance, std::string member, TextFieldInfo info)
    {
        textFields[instance].insert_or_assign(std::move(member), std::move(info));
    }

    StaffStyleAssignmentAudit& expectStaffStyleAssignments(musx::dom::Cmper partId, musx::dom::Cmper staffId, std::size_t count, bool malformedSource)
    {
        auto found =
            std::ranges::find_if(staffStyleAssignmentAudits, [&](const auto& audit) { return audit.partId == partId && audit.staffId == staffId; });
        if (found == staffStyleAssignmentAudits.end()) {
            found = staffStyleAssignmentAudits.insert(staffStyleAssignmentAudits.end(), StaffStyleAssignmentAudit{partId, staffId});
        }
        found->expectedAssignments += count;
        found->malformedSource = found->malformedSource || malformedSource;
        return *found;
    }

    [[nodiscard]] const StaffStyleAssignmentAudit* findStaffStyleAssignmentAudit(musx::dom::Cmper partId, musx::dom::Cmper staffId) const
    {
        const auto found =
            std::ranges::find_if(staffStyleAssignmentAudits, [&](const auto& audit) { return audit.partId == partId && audit.staffId == staffId; });
        return found == staffStyleAssignmentAudits.end() ? nullptr : &*found;
    }

    [[nodiscard]] const FieldInfo* findField(const InstanceKey& instance, std::string_view member) const
    {
        const auto foundInstance = fields.find(instance);
        if (foundInstance == fields.end()) {
            return nullptr;
        }
        const auto foundField = foundInstance->second.find(std::string(member));
        return foundField == foundInstance->second.end() ? nullptr : &foundField->second;
    }

    [[nodiscard]] FieldInfo* findField(const InstanceKey& instance, std::string_view member)
    {
        const auto foundInstance = fields.find(instance);
        if (foundInstance == fields.end()) {
            return nullptr;
        }
        const auto foundField = foundInstance->second.find(std::string(member));
        return foundField == foundInstance->second.end() ? nullptr : &foundField->second;
    }

    template <typename T>
    [[nodiscard]] const FieldInfo* findField(std::string_view member, musx::dom::Cmper partId = musx::dom::SCORE_PARTID,
        std::optional<musx::dom::Cmper> cmper1 = std::nullopt, std::optional<musx::dom::Inci> inci = std::nullopt,
        std::optional<musx::dom::Cmper> cmper2 = std::nullopt) const
    {
        return findField(InstanceKey{typeid(T), partId, cmper1, inci, cmper2}, member);
    }
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
};

/// @brief What @ref recognize concluded about a buffer.
enum class MusRecognition {
    /// @brief A legacy Finale score this reader can attempt. Reading may still fail on damage
    /// the recognition walk does not inspect, such as a corrupt compressed block.
    Candidate,
    /// @brief Not a Finale file.
    NotFinale,
    /// @brief A Finale file that is not a legacy score: a library, an ETF or MUSX document, a
    /// lesson, or a SmartMusic assessment.
    FinaleNonScore,
    /// @brief A legacy Finale binary whose block structure is inconsistent.
    Damaged
};

/// @brief The result of @ref recognize.
struct MusRecognitionResult
{
    MusRecognition verdict{MusRecognition::NotFinale};
    /// @brief The container epoch, when the buffer is a legacy Finale binary.
    std::optional<FormatEpoch> epoch;
    /// @brief A short description suitable for a non-error diagnostic; empty for a candidate.
    std::string_view detail;
};

/// @brief Decides, without importing, whether a buffer is a legacy Finale score.
/// @details Reads the header and walks the block headers of a signature-era file, or the
/// first pool prologue of a Coda-banner file; nothing is decompressed or decoded. A score
/// holds the others, details, entries, and text pools. A Finale binary missing any of them
/// is a library or another non-score file. @ref Reader rejects any buffer this does not
/// accept as a candidate, so the two never disagree.
/// @param data The whole file.
[[nodiscard]] MusRecognitionResult recognize(std::span<const std::uint8_t> data) noexcept;

/// @brief The outcome of an import, successful or not.
/// @details Input that cannot be read or classified as a supported MUS container is rejected
/// by exception before an ImportResult exists. Once classification establishes a source
/// epoch, a later recovery failure returns a null @ref document and records one
/// @c LogLevel::Error entry in @ref ImportReport::diagnostics. Every other diagnostic level
/// accompanies a document that exists.
struct ImportResult
{
    explicit ImportResult(FormatEpoch epoch)
        : report(epoch)
    {}

    /// @brief The imported document, or null when the import failed.
    std::shared_ptr<musx::dom::Document> document;
    ImportReport report;
};

/// @brief A Finale MIDI Device Annotation XML document and its filename.
struct PercussionMappingXml
{
    /// @brief Filename used by Finale's percussion-map conversion table.
    /// @details Paths are accepted; only the final filename component is used. A document
    /// without a name cannot be selected by the conversion table.
    std::string fileName{};

    /// @brief XML contents, parsed during reader creation without retaining the span.
    std::span<const std::uint8_t> bytes{};
};

/// @brief Optional resources supplied by the application when a reader is created.
struct ReaderOptions
{
    /// @brief Contents of Finale's `MacSymbolFonts.txt`, if available.
    /// @details The reader parses the bytes during creation and does not retain the span.
    /// Each nonblank line names a font whose stored character values are glyph numbers.
    /// A matching font definition retains its charset bank and receives that bank's symbol
    /// charset in the returned document.
    std::span<const std::uint8_t> macSymbolFonts{};

    /// @brief Finale MIDI Device Annotation XML documents used for legacy percussion maps.
    /// @details The reader parses every buffer during creation and does not retain the spans.
    /// Documents may contain one or more named `NoteNameList` tables. Within one filename, the
    /// first table with each name is retained.
    std::vector<PercussionMappingXml> percussionMappingXml{};

    /// @brief Contents of Finale's `PercMapConversionTable.txt`, if available.
    /// @details The reader parses these bytes during creation and does not retain the span.
    /// A matching legacy map name selects a supplied XML file and note-name list, which
    /// converts that map's note types. A map without an entry naming a supplied list keeps its
    /// General MIDI note types.
    std::span<const std::uint8_t> percussionMapConversionTable{};

    /// @brief Contents of optional Finale `.soundmap` XML files, in caller-specified order.
    /// @details Each nonempty buffer must have a `Soundmap` root. Documents are parsed once
    /// during creation using the caller's XML backend; the input spans are not retained.
    /// All supplied maps, including duplicate maps, retain their order. No files are located
    /// automatically. Invalid XML or an incorrect root causes reader creation to throw.
    std::vector<std::span<const std::uint8_t>> soundmapXml{};

    /// @brief XML instrument catalog exported from Finale's instrument database.
    /// @details Optional `InstrumentCatalog` XML is parsed once during reader creation.
    /// The input span is not retained. Invalid XML or an incorrect root causes creation to throw.
    std::span<const std::uint8_t> instrumentCatalogXml{};
};

class Reader
{
public:
    /// @brief Creates a reusable reader with the caller's XML backend and optional resources.
    /// @details Optional resources are parsed once during this call and reused by every
    /// subsequent import through the returned instance.
    template <typename XmlDocumentType>
    [[nodiscard]] static Reader create(const ReaderOptions& options = {})
    {
        return createWithParser(options, &parseXml<XmlDocumentType>, &parseDocument<XmlDocumentType>);
    }

    [[nodiscard]] musx::dom::DocumentPtr read(const std::filesystem::path& path) const;
    [[nodiscard]] musx::dom::DocumentPtr read(std::span<const std::uint8_t> data) const;

    template <typename XmlDocumentType>
    [[nodiscard]] static musx::dom::DocumentPtr read(const std::filesystem::path& path, const ReaderOptions& options = {})
    {
        return create<XmlDocumentType>(options).read(path);
    }

    template <typename XmlDocumentType>
    [[nodiscard]] static musx::dom::DocumentPtr read(std::span<const std::uint8_t> data, const ReaderOptions& options = {})
    {
        return create<XmlDocumentType>(options).read(data);
    }

#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
    [[nodiscard]] ImportResult readWithReport(const std::filesystem::path& path) const;
    [[nodiscard]] ImportResult readWithReport(std::span<const std::uint8_t> data) const;

    template <typename XmlDocumentType>
    [[nodiscard]] static ImportResult readWithReport(const std::filesystem::path& path, const ReaderOptions& options = {})
    {
        return create<XmlDocumentType>(options).readWithReport(path);
    }

    template <typename XmlDocumentType>
    [[nodiscard]] static ImportResult readWithReport(std::span<const std::uint8_t> data, const ReaderOptions& options = {})
    {
        return create<XmlDocumentType>(options).readWithReport(data);
    }
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)

private:
    Reader(std::shared_ptr<const detail::ReaderResources> resources, XmlParser parseXml, DocumentParser parseDocument)
        : m_resources(std::move(resources)), m_parseXml(parseXml), m_parseDocument(parseDocument)
    {}

    template <typename XmlDocumentType>
    static std::unique_ptr<musx::xml::IXmlDocument> parseXml(const char* data, std::size_t size)
    {
        static_assert(std::is_base_of_v<musx::xml::IXmlDocument, XmlDocumentType>, "XmlDocumentType must derive from musx::xml::IXmlDocument");

        auto xmlDocument = std::make_unique<XmlDocumentType>();
        xmlDocument->loadFromBuffer(data, size);
        return xmlDocument;
    }

    template <typename XmlDocumentType>
    static musx::dom::DocumentPtr parseDocument(const char* data, std::size_t size)
    {
        return musx::factory::DocumentFactory::create<XmlDocumentType>(data, size);
    }

    static Reader createWithParser(const ReaderOptions& options, XmlParser parseXml, DocumentParser parseDocument);
    ImportResult readPrepared(const std::filesystem::path& path) const;
    ImportResult readPrepared(std::span<const std::uint8_t> data) const;

    std::shared_ptr<const detail::ReaderResources> m_resources;
    XmlParser m_parseXml;
    DocumentParser m_parseDocument;
};

} // namespace finale_mus_reader
