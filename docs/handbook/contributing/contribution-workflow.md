# Contribution Workflow
<!-- doc-template:chapter -->
[Handbook](../index.md) → [Contributing](index.md)

<!-- doc-content:purpose:start -->
<a id="contributing"></a>

This chapter describes how to prepare, implement, test, review, document, and commit a change to Conjure.
<!-- doc-content:purpose:end -->

<!-- doc-content:body:start -->
<a id="1-the-workflow"></a>

## The workflow

```text
inspect code -> implement -> tests -> review -> documentation -> commit
```

| Step | What it involves |
|---|---|
| Inspect | Read the affected headers, implementation, tests, and module handbook page. |
| Implement | Put public headers in `engine/include/conjure/<module>/` and implementation in `engine/src/<module>/`; register files explicitly in CMake. |
| Test | Add behaviour tests and run `python tools/build.py --test` in Debug with warnings as errors. |
| Review | Read the complete diff, check ownership and failure paths, and fix findings before committing. |
| Document | Update the module README, wiki topics, and scoped handbook pages; run the documentation checks and build. |
| Commit | Keep one logical change per commit, with a clear subject and a buildable result. |

<a id="2-before-you-start"></a>

## Before you start

Read Architecture, the affected module's handbook page, and its `README.md`. Locate the
matching tests, including nested files below `tests/<module>/`. Headers and implementation define the actual
behaviour when prose disagrees with code.

Verify the toolchain with `python tools/build.py --configure-only`. Coordinate cross-module API changes with
the contributor responsible for that module before changing its interface.

<a id="3-non-negotiable-rules"></a>

<a id="3-engineering-rules"></a>

## Engineering rules

- Write production code with complete failure paths and tests. Report unfinished work explicitly.
- Follow [Coding standards](../development/coding-standards.md): naming, four-space indentation, braces, initialisation,
  header hygiene, class layout, and the 120-column limit.
- Engine code uses `conjure::core::result<T>` or `status` for recoverable errors and `CONJURE_ASSERT` for
  programmer errors. Check a result before reading its value.
- Every module has headless tests. Keep warnings enabled and fix them; use `SYSTEM` includes for third-party headers.
- Register source files explicitly in CMake. Pin third-party dependencies, provide a `conjure::thirdparty::`
  alias, register their licence notices, and regenerate `THIRD_PARTY_NOTICES.md` when dependencies change.
- Keep platform-specific code in backends and supported platform headers. Build on Windows, Linux, and macOS.
- Public headers compile on their own and expose no third-party headers, except the supported glm interface.
- Build, test, review, and update documentation before reporting completion.

<a id="4-working-alongside-other-contributors"></a>

## Working alongside other contributors

Agree on module ownership and coordinate shared API changes. Parallel builds require separate build directories:

```bash
python tools/build.py --build-dir build/<your-name> --target conjure_<module>_tests --test -R <module>
python tools/build.py --build-dir build/<your-name> --no-configure --target conjure_<module>_tests --test -R <module>
```

Reuse private directory names across related work. Once a directory is no longer needed, remove it with
`python tools/prune_builds.py <your-name> --delete`. See [Private build directories](../development/building-and-configuration.md#6-private-build-directories).

<a id="5-the-review-checklist"></a>

<a id="5-review"></a>

## Review

Read every changed file and the full diff. Record findings with file and line references, fix them, then
rebuild and rerun the relevant tests.

| Area | Checks |
|---|---|
| Correctness | Happy paths, failures, edge cases, initialisation, lifetimes, invalidation, checked results, and synchronisation. |
| Ownership | Explicit resource ownership, non-owning views used within their lifetimes, and safe shutdown order. |
| Style | Naming, braces, early returns, initialised declarations, class layout, enum types, and public method summaries. |
| Boundaries | Explicit includes and CMake file lists, backend isolation, module aliases, and no third-party header leaks. |
| Hygiene | No debug output, commented-out code, dead code, build outputs, downloaded assets, or IDE files in the commit. |
| Documentation | Accurate signatures and examples, updated README/wiki/handbook content, and clean documentation checks. |

Run `python tools/check_style.py --tests-mirror-headers` for the mechanical style checks. A managed-source change
also passes `dotnet format --verify-no-changes` for `Conjure.Managed`, `Conjure.Bootstrap`, and `Conjure.TestScripts`.

<a id="6-commit-messages"></a>

## Commit messages

Use `<module>: <imperative summary>` with a lower-case module and no trailing period. For example,
`audio: handle device reconnection` describes an audio change. Explain the reason in the body when it is not
clear from the diff. Keep the scope coherent so every commit builds and passes tests on its own.

Do not commit build outputs, IDE files, the documentation virtual environment, or downloaded assets. Prefer a
new commit over amending one that other contributors have already fetched.

<a id="8-definition-of-done"></a>

<a id="7-definition-of-done"></a>

## Definition of done

- [ ] Debug builds with warnings as errors; Distribution builds when assertions or logging change.
- [ ] Behaviour tests cover success, failures, and edge cases; relevant tests pass without unexpected skips.
- [ ] Every public header has its matching test file or an explicit, reviewed exception.
- [ ] Style checks pass, public headers compile alone, and managed formatting checks pass where applicable.
- [ ] Module README, public API topics, and handbook pages are updated.
- [ ] `python tools/docs.py regenerate --check`, `python tools/docs.py check`, and `python tools/docs.py build` pass.
- [ ] Integration scenarios, determinism checks, and benchmark thresholds pass when the change affects them.
- [ ] The completion report identifies what was built, what was tested, and any remaining work.

## Related

- [Setup and First Run](../getting-started/setup-and-first-run.md)
- [Testing and Benchmarks](../development/testing-and-benchmarks.md)
- [Continuous Integration](../development/continuous-integration.md)
- [Writing Documentation](writing-documentation.md)

<a id="contents"></a>
<!-- doc-content:body:end -->
