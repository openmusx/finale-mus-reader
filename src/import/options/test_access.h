// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include "import/support/legacy_mapping.h"

// Stages of an options importer, exposed only so that a test can drive one of them alone.
//
// **No library code may include this header.** `import/options.h` states what the registry
// calls -- one importer per musxdom class, and nothing else -- and that is the whole contract
// between an options class and the rest of the reader. Everything here is reachable from an
// importer already; a declaration exists below only because driving the stage through its
// importer would make some test vaguer than it needs to be. Recovering a clef table against a
// purpose-built reference document is the motivating case.
//
// A stage that no test drives does not belong here either. Make it file-local in the class's
// own translation unit, where an anonymous namespace keeps it out of every other file's way.
// Two multimeasure-rest stages were briefly declared here out of symmetry with their
// neighbors, which is the mistake this comment exists to prevent.

namespace finale_mus_reader {
namespace options {

/// @brief Recovers source clef definitions and completes the modern 18-definition collection.
/// @details The collection is rebuilt rather than seeded: its shape and font comparators
/// belong to the baseline's own tables. Definitions the source does not store are copied
/// from the reference document, which is what Finale's own upgrade does. Runs before the
/// clef scalar tables so that they have an object to overlay.
void captureClefOptions(const records::LegacyRecordIndex& index, const SourceProfile& profile, const musx::dom::DocumentPtr& document,
    const musx::dom::DocumentPtr& referenceDocument, ImportReport& report, PendingReferences& pending,
    musx::factory::ConstructionContext& construction);

/// @brief Checks recovered ClefOptions values that only make sense once the tables have run.
/// @details Runs after the clef scalar tables, because the default clef index is a mapped
/// scalar and the collection it indexes is built before them.
void validateClefOptions(const musx::dom::DocumentPtr& document, ImportReport& report);

/// @brief The Finale 2007 and later clef scalars, as class records addressed by byte offset.
[[nodiscard]] const MappingTable& classClefOptionsTable();

/// @brief Recovers source FontOptions and completes the modern 45-type collection.
/// @details Physical tuples are interpreted through versioned semantic mappings. Types absent
/// from the source are copied from the separate reference document with safe font-id remapping.
void captureFontOptions(const records::LegacyRecordIndex& index, const SourceProfile& profile, const musx::dom::DocumentPtr& document,
    const musx::dom::DocumentPtr& referenceDocument, ImportReport& report, musx::factory::ConstructionContext& construction);

/// @brief Reconciles recovered nonzero FontOptions ids whose MUS FontDefinition is absent.
/// @details Selects the reference face by semantic type, reuses an equal nonzero target
/// face by normalized name, or clones it at the next target comparator when necessary.
void repairMissingRecoveredFontDefinitions(const musx::dom::DocumentPtr& document, const musx::dom::DocumentPtr& referenceDocument,
    const std::shared_ptr<musx::dom::options::FontOptions>& target, ImportReport& report, musx::factory::ConstructionContext& construction);

/// @brief Recovers the source's stem connections into the seeded StemOptions object.
/// @details The collection is dropped and rebuilt rather than overlaid: stem connections
/// are a document's own table, naming that document's fonts, so the baseline's connections
/// are never a default for another document. A source that stores none leaves the
/// collection empty. The scalars around it stay seeded from the pinned baseline.
void captureStemOptions(const records::LegacyRecordIndex& index, const SourceProfile& profile, const musx::dom::DocumentPtr& document,
    ImportReport& report, musx::factory::ConstructionContext& construction);

} // namespace options
} // namespace finale_mus_reader
