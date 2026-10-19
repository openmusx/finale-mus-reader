---
name: comment-production-code
description: Write or revise comments in this repository's C++ (src/, include/, tests/). Use when adding a decoder, reviewing a diff for comment quality, or cleaning up comments that have drifted into narrating how a finding was made. Covers what a comment may state, how to label a claim this repository is not certain of, and where derivation history belongs instead.
---

# Comment production code

A production comment states **how the code works**, and **how we believe it ought
to work** where belief is all we have. Nothing else.

This applies to every C++ file in this repository: `src/`, `include/`, and
`tests/`. A test comment says what the test asserts and why the assertion is the
right one; it does not narrate which save located a field or what the field was
thought to be before.

## The rule

A comment in production code may contain:

- **Behavior.** What this code does, in terms a reader of the code can check
  against the code.
- **Mechanism.** How the format works, where that is what makes the code make
  sense — a layout, an encoding, an ordering constraint, a lifetime.
- **Belief, labeled as belief.** A theory about the format that is untested or
  only partly established, stated as a theory and marked with how far it is
  trusted. This repository decodes an undocumented format; a comment that admits
  uncertainty is worth more than one that hides it.
- **Consequence.** What breaks if the code is changed in an obvious-looking way,
  when the reason is not visible locally.

A comment in production code may **not** contain:

- **How the behavior was derived.** Which fixture established it, which
  companion corroborated it, how many documents agreed. This is the single most
  common drift.
- **What was believed before.** A refuted prediction, a hypothesis that did not
  hold, "this turned out not to be", "the first attempt did". A reader needs the
  conclusion, not the path.
- **Project narrative.** When something was added, what it replaced, which
  release or conversation prompted it, what is planned next.
- **Self-reference.** "this reader", "we", "I", and the like, narrating the
  author's process rather than describing the code.

## Confidence labels

Where a statement about the format is not certain, say so in the comment and say
how uncertain. Use one of these, consistently:

| Label | Meaning |
|---|---|
| *(unlabelled)* | Established. The code depends on it and nothing known contradicts it. |
| **Believed** | Consistent with everything seen, with no case that would distinguish it from a near alternative. |
| **Unverified** | A theory that fits, with nothing yet confirming it. Recorded so that a document which contradicts it is recognized as contradicting something. |

An unverified theory earns its place only when it would change what a reader
does — most often as a tripwire, where the code deliberately fails loudly rather
than guessing, and the comment says what a future observation would settle.
Never let an unverified theory read as fact, and never act on one in code that
could instead report the gap.

## Where retained information belongs

| Material | Home |
|---|---|
| What a specific fixture is and what it changes | `tests/evidence/<era>/provenance.txt` |
| A binding reader rule | the relevant production `docs/` file |
| Cross-cutting container, epoch, framing, or shared-record fact | `docs/format_overview.md` |
| A class-specific selector, class id, payload layout, flag, version behavior, sharing behavior, or recovery status | the class's importer and tests, never `docs/format_overview.md` |

Delete derivation history, refuted predictions, investigative narratives, and references to
materials that are not part of this repository.

## Doxygen

`AGENTS.md` already governs Doxygen: it documents the contract, not its history.
Everything above applies to Doxygen as well, and Doxygen additionally excludes
mechanism a caller does not need. A caller arriving at the published API has none
of this repository's context.

## Revising an existing comment

Work sentence by sentence, not comment by comment. Most comments that violate
this rule are half compliant: a correct statement of mechanism followed by a
sentence naming the fixture that established it. Keep the first, move the second.

Two checks before finishing:

1. **Would this sentence still be true and useful to someone who has only this
   repository?** If it only makes sense as a reference to work that happened
   elsewhere, it belongs with that work.
2. **Does the comment claim more confidence than the evidence supports?**
   Stripping the derivation makes a hedged claim read as fact. Where that
   happens, add the label rather than restoring the history.
