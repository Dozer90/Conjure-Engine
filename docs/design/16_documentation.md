# 16 - Documentation system

Status: **binding** (owner: lead architect). Added 2026-11-09 from the customer's additional requirement:
documentation must be easily searchable, link between relevant pages, and be succinct without being bare.
Two document sets exist with different audiences, and a reader must always know which one they are in.

## 1. Overview and decisions

| Id | Decision |
|---|---|
| D1 | Two document sets, both Markdown in the repository: the **Public API wiki** (`docs/api/`) for engine users (game developers, scripters, agents building games) and the **Engineering handbook** (`docs/handbook/`) for people working in the source. `docs/design/` stays the design record and is linked from the handbook, never from the wiki. |
| D2 | Published as one searchable site with **MkDocs + Material** (`mkdocs.yml` at the repository root): built-in full-text search, navigation tabs (one per document set), automatic page table of contents, dark/light themes. Everything remains readable on GitHub without the site. |
| D3 | Coloured boxes use **GitHub alert syntax** (`> [!NOTE]`, `> [!TIP]`, `> [!IMPORTANT]`, `> [!WARNING]`, `> [!CAUTION]`). GitHub renders them natively; the site converts them with `mkdocs-github-admonitions-plugin`. Meaning: NOTE = context, TIP = the better function or practice ("nudge"), IMPORTANT = must do, WARNING = gotcha, CAUTION = undefined behaviour / data loss. |
| D4 | Every page opens with a **type badge** on its first content line: `📘 **Public API wiki**` or `🛠 **Engineering handbook**`, followed by a one-line pointer to the sibling page of the other set when one exists. Cross-type links are always written as `🛠 Handbook: <title>` or `📘 API: <title>` so a jump between sets is obvious. |
| D5 | Wiki pages are organised by **what the reader wants to do**, not by header: one page per user-facing topic (e.g. *Entities and components*, *Transforms*, *Playing sounds*) with small things grouped (`Data types` covers `u8` ... `f64`, `usize`, `byte`). A page may cover several headers; `tools/check_docs.py` enforces that every public header is referenced from at least one wiki page of its module. |
| D6 | Handbook pages follow a **Confluence-style** structure: title, badge, a short purpose paragraph, an explicit `## Contents` list (GitHub shows no automatic TOC), primary headings with nested subheadings, tables for reference data, a `## Related` footer. It covers the entire codebase: getting started, architecture, repository layout, build pipeline, coding standards and data-oriented design practice, testing, each module's internals, the **Module system** (dedicated chapter: what a module is, create, import, distribute), editor and AI protocol, scripting pipeline, renderer pipeline, assets, export, CI, contributing. |
| D7 | Source of truth for signatures is the code (`/// <summary>` blocks). Wiki pages quote signatures by hand in short form and link the header path; they never paste whole headers. When a signature changes, the owning PR updates the page (definition of done, 08_testing 7.4 item 7 added below). |
| D8 | Examples are mandatory where a function depends on others to be useful (setup/teardown pairs, handles that must be checked, frame-ordering constraints). Examples are complete, compile-checked snippets drawn from tests where possible, kept under 30 lines. |
| D9 | `tools/check_docs.py` runs as the CTest entry `docs.check` (label `docs`): header coverage (D5), one handbook page per engine module, badge present on every page, relative links resolve, no page over 400 lines (split instead), no `TODO`. |
| D10 | Documentation is written per wave by the module owner and reviewed like code. The wiki is written for a broader audience: short sentences, no engine jargon without a link to its definition, complexity hidden behind the OOP facade first and the DOD escape hatch second. |

## 2. Layout

```
mkdocs.yml                         site: two navigation tabs, search, Material theme, GitHub alerts plugin
docs/index.md                      landing page: pick your document set
docs/api/                          📘 Public API wiki
  index.md                         what Conjure is for users, how to read the wiki, where to start
  getting-started/                 install, first project, first scene, first script, export
  concepts/                        entities & components, scenes & prefabs, assets, the frame, coordinate system
  core/                            data types, errors & results, logging, math, containers, events, time, jobs, json, files
  ecs/ platform/ rendering/ physics/ audio/ assets/ scene/ scripting/ editor/ modules/
  scripting/csharp/                the C# API as users see it (Entity, Transform, Input, ...)
  editor/ai-protocol.md            the command protocol for agents (generated catalogue + hand-written guide)
  glossary.md, faq.md, gotchas.md  cross-cutting
docs/handbook/                     🛠 Engineering handbook
  index.md                         contents of the handbook
  getting-started.md               clone, build, run tests, IDE setup, presets, build.py
  architecture.md                  module map, dependency rules, frame, threading (summary of ARCHITECTURE.md + links)
  repository-layout.md
  build-pipeline.md                CMake structure, configs, options, third-party pinning, shaders, managed build, export
  coding-standards.md              style, conventions, review checklist, error model, logging, asserts
  data-oriented-design.md          DOD practices in Conjure: archetypes, chunk iteration, SoA, change detection, jobs, frame memory, allocation-free steady state, what to avoid
  testing.md                       test support library, conventions, labels, preconditions, benchmarks, CI matrix
  modules/                         one page per engine module: purpose, internals, threading, extension points, tests
  module-system/                   dedicated section: what a module is, manifest, ABI, create a module, import, distribute, game modules, C# in modules
  editor-internals.md, ai-protocol-internals.md, scripting-pipeline.md, renderer-pipeline.md, asset-pipeline.md, export-and-distribution.md
  ci.md, contributing.md, decision-records.md (links into docs/design/)
docs/assets/                       images, diagrams (SVG), extra.css
tools/check_docs.py                D9
tools/docs_requirements.txt        mkdocs-material, mkdocs-github-admonitions-plugin (pinned)
```

## 3. Page templates

### 3.1 Public API wiki page

```markdown
# <Topic title>

📘 **Public API wiki** · 🛠 Handbook: [<internals page>](../../handbook/modules/<module>.md)

<One or two sentences: what this is for and the header to include.>

## At a glance
| You want to... | Use |
|---|---|
| ... | `function_name()` |

## Quick example
```cpp
... complete, short, compiles ...
```

## <Concept sections as needed>

> [!TIP]
> Nudges towards the more appropriate function when this one is commonly misused.

> [!WARNING]
> Gotchas: lifetime rules, threading, ordering within the frame.

## API summary
| Signature (short) | Purpose |
|---|---|

## See also
- 📘 API: ...
- 🛠 Handbook: ...
```

### 3.2 Engineering handbook page

```markdown
# <Chapter title>

🛠 **Engineering handbook** · 📘 API: [<user-facing page>](../api/...)

<Purpose paragraph.>

## Contents
- [Section](#section)
- ...

## 1. Section
### 1.1 Subsection
...

## Related
- design record: `docs/design/NN_*.md`
```

## 4. Tooling

- `mkdocs.yml`: `site_name: Conjure Engine`, theme `material` with `navigation.tabs`, `navigation.sections`,
  `search.suggest`, `content.code.copy`, `toc.integrate` off; `nav` lists both sets explicitly; plugins `search`,
  `gh-admonitions`; markdown extensions `tables`, `fenced_code`, `toc` with permalinks, `pymdownx.superfences`.
- `python tools/docs.py serve|build|check` wraps `mkdocs serve`, `mkdocs build --strict` (broken links fail) and
  `check_docs.py`. `pip install -r tools/docs_requirements.txt` once.
- CI: `docs.check` runs in the style job; `mkdocs build --strict` runs in a `docs` job on PRs.

## 5. Integration with the development process

- Definition of done (08_testing 7.4) gains item 7: "the public API wiki and handbook pages for the change are
  written or updated and `docs.check` passes".
- `AGENTS.md` section 1 points at both sets and at this note; `.claude/skills/write-docs/SKILL.md` describes how to
  write a wiki page and a handbook page (templates above, badge, alerts, link conventions, check tool).
- Per wave: wave 1 (core) documents core now; waves 2-5 document their modules in the same PR as the code; the final
  acceptance review (wave 5) audits both sets for completeness and readability.

## 6. Tests

`tools/check_docs.py` self-test fixtures live in `tools/tests/check_docs/` (a tiny fake docs tree with a missing badge,
a dangling link, an undocumented header) and run through `python -m pytest`-free plain asserts in
`tools/tests/test_check_docs.py`, registered as CTest `docs.check_tool` (label `docs`).
