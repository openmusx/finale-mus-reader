// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include "finale_mus_reader/reader.h"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace finale_mus_reader {

class ReportWriter;

/// @brief Report-only state, with no payload in a non-instrumented build.
/// @details State must be a template whose instrumentation types depend on its writer parameter.
template <template <typename> class State>
class ReportState
{
    friend class ReportWriter;
#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
    State<ReportWriter> m_value;
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
};

/// @brief A report's class identity, with no RTTI payload when instrumentation is disabled.
class ReportClass
{
    friend class ReportWriter;

public:
    template <typename T>
    static ReportClass of()
    {
        ReportClass result;
#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
        result.m_type = typeid(T);
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
        return result;
    }

private:
#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
    std::type_index m_type{typeid(void)};
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
};

/// @brief An instance identity retained across reporting callbacks, empty when disabled.
class ReportInstance
{
    friend class ReportWriter;

public:
    template <typename T, typename... Keys>
    static ReportInstance of([[maybe_unused]] Keys&&... keys)
    {
        ReportInstance result;
#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
        result.m_key = finale_mus_reader::instanceKey<T>(std::forward<Keys>(keys)...);
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
        return result;
    }

private:
#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
    InstanceKey m_key;
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
};

template <typename Reporting>
struct DeferredFieldData
{
    typename Reporting::InstanceKey instance;
    std::string member;
};
using DeferredFieldReport = ReportState<DeferredFieldData>;

#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
/// @brief Provides dependent access to the instrumentation API within generic callbacks.
class ReportWriter
{
public:
    using Origin = ValueOrigin;
    using FieldInfo = finale_mus_reader::FieldInfo;
    using InstanceKey = finale_mus_reader::InstanceKey;
    using TextFieldInfo = finale_mus_reader::TextFieldInfo;

    explicit ReportWriter(ImportReport& report)
        : m_report(report)
    {}
    ImportReport& report() const { return m_report; }

    template <typename T, typename... Keys>
    static InstanceKey instanceKey(Keys&&... keys)
    {
        return finale_mus_reader::instanceKey<T>(std::forward<Keys>(keys)...);
    }

    template <typename... Keys>
    static InstanceKey instanceKey(const ReportClass& type, Keys&&... keys)
    {
        auto instance = finale_mus_reader::instanceKey<void>(std::forward<Keys>(keys)...);
        instance.classType = type.m_type;
        return instance;
    }

    static const InstanceKey& instanceKey(const ReportInstance& instance) { return instance.m_key; }

    template <template <typename> class State>
    static auto& state(ReportState<State>& storage)
    {
        return storage.m_value;
    }
    template <template <typename> class State>
    static const auto& state(const ReportState<State>& storage)
    {
        return storage.m_value;
    }

    /// @brief Records a field whose legacy source has not been located in any supported layout.
    /// @details An existing entry always wins, so a recovered value or known fallback cannot be
    /// downgraded by the completeness pass.
    void unmappedField(const InstanceKey& key, std::string member, std::int64_t value)
    {
        if (!m_report.findField(key, member)) {
            m_report.setField(key, std::move(member), {Origin::Unmapped, 0, 0, value});
        }
    }

    template <typename T>
    void defaultField(std::string member, std::int64_t value)
    {
        m_report.setField(instanceKey<T>(), std::move(member), {Origin::Finale27Default, 0, 0, value});
    }

    template <typename T>
    void behaviorField(std::string member, std::int64_t value)
    {
        m_report.setField(instanceKey<T>(), std::move(member), {Origin::LegacyBehavior, 0, 0, value});
    }

    void textField(const InstanceKey& instance, std::string member, bool fontWasSynthesized, bool sizeWasSynthesized, bool effectsWereSynthesized)
    {
        m_report.setTextField(instance, std::move(member), {fontWasSynthesized, sizeWasSynthesized, effectsWereSynthesized});
    }

    template <typename ConvertedText>
    void textField(const InstanceKey& instance, std::string member, const ConvertedText& converted)
    {
        textField(instance, std::move(member), converted.fontWasSynthesized, converted.sizeWasSynthesized, converted.effectsWereSynthesized);
    }

    /// @brief Converts a C++ member path to a report path, replacing arrows with dots.
    [[nodiscard]] static std::string memberName(const char* memberPath);
    static std::optional<InstanceKey> importedInstance(const musx::dom::EnigmaBase& object);

private:
    ImportReport& m_report;
};
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)

/// @brief Invokes a generic reporting callback synchronously, only in instrumented builds.
/// @details Put report preparation inside the callback and capture existing objects by reference.
/// Instrumentation-only names must depend on the callback's writer parameter. The callback must
/// not mutate the document or perform validation needed by a non-instrumented import.
template <typename Callback>
void withReporting([[maybe_unused]] ImportReport& report, [[maybe_unused]] Callback&& callback)
{
#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
    ReportWriter writer(report);
    std::forward<Callback>(callback)(writer);
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
}

template <typename Reporting, typename Source, typename Row>
void reportLegacyField(Reporting& reporting, const typename Reporting::InstanceKey& key, const Source& source, const Row& row, const char* member,
    std::size_t offset, std::int64_t value, typename Reporting::Origin origin = Reporting::Origin::LegacyMus)
{
    reporting.report().setField(
        key, member, typename Reporting::FieldInfo{origin, row.blockOffset, row.decodedOffset + offset, value, source.identity});
}

template <typename Reporting>
void reportFallbackField(
    Reporting& reporting, const typename Reporting::InstanceKey& key, const char* member, typename Reporting::Origin origin, std::int64_t value)
{
    reporting.report().setField(key, member, typename Reporting::FieldInfo{origin, 0, 0, value});
}

} // namespace finale_mus_reader
