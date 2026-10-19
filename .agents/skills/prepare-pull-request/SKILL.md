---
name: prepare-pull-request
description: Validate and deliver a feature branch through a pull request in this repository. Use whenever the user asks to open or create a PR, or asks to commit and push changes that will immediately become a PR. Requires final local validation before opening a PR and prompt delivery of trivial CI fixes afterward.
---

# Prepare a pull request

Use this workflow only after the user has authorized the requested commit, push, and pull-request
operations. That authorization does not cover unrelated changes or cleanup.

## Final local validation

After the implementation, tests, evidence, and documentation have stabilized, and immediately
before delivery:

1. Build the test-enabled tree from the final branch state and run the complete CTest suite with
   `ctest --test-dir <build-dir> --output-on-failure`. Do not rely on CI or an earlier test run.
   If any test fails, fix the failure and rerun the complete suite after the final change.
2. Read `.github/workflows/build_and_test.yml` and locate its `non-instrumented-build` job. That job
   is the source of truth for the required CMake options and build target.
3. Configure a separate build directory with the same library-only, tests-off, and
   instrumentation-off settings. Adapt only generator and compiler selection when the local host
   cannot reproduce the Linux runner; on macOS, use the available local toolchain.
4. Build the same `finale_mus_reader` target. An existing instrumented build directory does not
   satisfy this check.
5. If the build fails, stop delivery, fix it, and rerun both the full test suite and this build
   after the changes stabilize.

Open the pull request only after both local validations pass.

## CI follow-ups on an open pull request

For a trivial fix to a specific CI failure, make the narrow change, commit it, and push the
feature branch immediately. Do not delay the push for a local rebuild or full test run; let the
new CI run verify the fix. If the failure calls for a broader code change, use the final local
validation workflow above before pushing.

## Deliver the branch

1. Inspect `git status --short`, the complete intended diff, and `git diff --check`, and run
   `python3 scripts/check_format.py`. Preserve unrelated worktree changes.
2. Stage only the intended files and review the staged status and diff check.
3. Review the diff, every commit message, and the pull-request title and body as public text.
   They state what the change does and how it was tested in this repository. They never name
   materials outside it: private documents or collections, their counts, machine-local paths,
   or the internals of proprietary software and toolkits.
4. Commit and push only when authorized by the user. Never deliver directly from `main`.
5. Open the pull request against the intended base branch and verify its URL, title, head, base,
   and open state.
6. Report the branch, commit, validation result, and pull-request link.
