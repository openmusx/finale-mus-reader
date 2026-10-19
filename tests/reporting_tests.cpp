// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/support/reporting.h"

#include <stdexcept>
#include <type_traits>

#include "musx/dom/Others.h"

namespace reporting_test_internal {

// Named rather than anonymous: ReportState<TestReportData<Reporting>> holds this by value and
// has external linkage, and under the unity build GCC rejects an anonymous-namespace member
// there (-Wsubobject-linkage).

template <typename Reporting>
struct TestReportData
{
    typename Reporting::FieldInfo info;
};

} // namespace reporting_test_internal

namespace {

using namespace finale_mus_reader;
using Target = musx::dom::others::Measure;

#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
constexpr bool enabled = true;
#else
constexpr bool enabled = false;
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)

template <typename T>
concept HasFieldReports = requires(T report) { report.fields; };

using reporting_test_internal::TestReportData;

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testLazyReporting()
{
    static_assert(HasFieldReports<ImportReport> == enabled);
    static_assert(std::is_empty_v<ReportState<TestReportData>> != enabled);
    static_assert(std::is_empty_v<ReportClass> != enabled);
    static_assert(std::is_empty_v<ReportInstance> != enabled);
    static_assert(std::is_empty_v<DeferredFieldReport> != enabled);

    ImportReport report(FormatEpoch::DclLegacy);
    ReportState<TestReportData> state;
    DeferredFieldReport deferred;
    const auto identity = ReportInstance::of<Target>(2, 3, 4, 5);
    const auto type = ReportClass::of<Target>();
    int calls = 0;
    int preparations = 0;
    withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
        ++calls;
        const auto prepareMember = [&] {
            ++preparations;
            return std::string("nested->field");
        };
        const auto member = prepareMember();
        const auto key = reporting.instanceKey(identity);
        require(key == reporting.instanceKey(type, 2, 3, 4, 5), "Opaque identity lost a key");
        require(key == reporting.template instanceKey<Target>(2, 3, 4, 5), "Typed identity disagrees with opaque identity");
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyBehavior);
        auto& info = reporting.report().fieldProvenance(key, member);
        require(info.origin == Reporting::Origin::LegacyBehavior, "A new field did not inherit its instance origin");
        info = {Reporting::Origin::LegacyMusAdjusted, 123, 456, -789, 42};
        reporting.unmappedField(key, member, 999);
        require(info.origin == Reporting::Origin::LegacyMusAdjusted && info.rawValue == -789, "Unmapped reporting replaced an established field");
        require(info.blockOffset == 123 && info.decodedOffset == 456 && info.sourceIdentity == 42, "Adjusted reporting lost source provenance");
        reporting.unmappedField(key, "unknown", 7);
        require(reporting.report().findField(key, "unknown")->origin == Reporting::Origin::Unmapped, "Unknown field origin changed");
        reporting.template defaultField<Target>("seeded", 8);
        reporting.template behaviorField<Target>("behavior", 9);
        reporting.textField(key, "text", true, false, true);
        const auto& text = reporting.report().textFields.at(key).at("text");
        require(text.fontWasSynthesized && !text.sizeWasSynthesized && text.effectsWereSynthesized, "Text formatting provenance changed");
        require(reporting.memberName(member.c_str()) == "nested.field", "Report member normalization changed");
        reporting.state(state).info = info;
        reporting.state(deferred) = {key, member};
    });
    require(calls == (enabled ? 1 : 0), "Reporting callback invocation count changed");
    require(preparations == calls, "Disabled reporting evaluated report preparation");

    withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
        const auto& saved = reporting.state(std::as_const(state)).info;
        const auto& field = reporting.state(std::as_const(deferred));
        auto* info = reporting.report().findField(field.instance, field.member);
        require(info && info->rawValue == saved.rawValue, "Deferred field identity was lost");
        info->rawValue = 1001;
        require(info->origin == saved.origin && info->blockOffset == saved.blockOffset, "Deferred update changed unrelated provenance");
        const auto singleton = reporting.template instanceKey<Target>();
        require(reporting.report().findField(singleton, "seeded")->origin == Reporting::Origin::Finale27Default,
            "Default convenience method changed origin");
        require(reporting.report().findField(singleton, "behavior")->origin == Reporting::Origin::LegacyBehavior,
            "Behavior convenience method changed origin");
    });
}

} // namespace

int main()
{
    testLazyReporting();
}
