# 16 - Documentation system

Status: **binding**. The page shapes and author workflow are specified by
`.claude/skills/write-docs/SKILL.md`. This record describes the tooling supporting those requirements.

## 1. Overview and decisions

| Id | Decision |
|---|---|
| D1 | Two Markdown sets: the Public API wiki in `docs/api/` for engine users, and the Engineering handbook in `docs/handbook/` for contributors. Wiki links into design notes, engine source, or the agent guide must go through the handbook. |
| D2 | One searchable MkDocs Material site, with one navigation tab per set and a landing page. Output uses flat `.html` paths so `docs/api/core/containers.md` becomes `site/api/core/containers.html`. |
| D3 | Callouts use GitHub alerts: NOTE for context, TIP for a better practice, IMPORTANT for required actions, WARNING for gotchas, and CAUTION for undefined behaviour or data loss. Tools validate the syntax and five kinds; review validates the meaning. |
| D4 | Pages use the title, breadcrumbs, metadata, and sections in the skill's five templates. Badges, numbered headings, explicit contents lists, and a fixed page-length limit are not mandatory. Existing pages can retain helpful navigation elements. |
| D5 | Every `.h` under `engine/include/conjure/<module>/` must be named by its exact include path in an inline code span in `docs/api/<module>/`. This includes umbrella, error, logging, plumbing, and internal headers. One topic page can cover several headers. |
| D6 | The handbook has Engine and Editor scope pages, module pages under `<scope>/<module>.md`, and header indexes under `<scope>/<module>/<header>.md`, and complete structure/class pages beneath each header. The editor libraries belong to the Editor scope even though their source lives in `engine/src/editor/`. |
| D7 | Code is the source of truth. Quote short signatures by hand and update documentation with the code it describes; never paste a whole header. |
| D8 | Examples are complete, under 30 lines, follow the code style, and are drawn from real tests. Review checks compilation and semantic completeness; the Markdown tools do not compile snippets. |
| D9 | `tools/check_docs.py` checks local links and anchors, wiki boundaries, alert syntax, unfinished markers, exact header code spans, and scoped module-page coverage. It has no badge or page-length requirement. |
| D10 | Documentation is reviewed with its owning code. API prose introduces the simple facade before lower-level details; handbook prose explains internals precisely. British spelling and the skill's review rules apply. |

## 2. Layout

```text
mkdocs.yml                               site configuration and navigation
 docs/api/<module>/<topic>.md             user-facing topics
 docs/handbook/<chapter>.md               contributor chapters
 docs/handbook/engine.md                  Engine scope
 docs/handbook/editor.md                  Editor scope
 docs/handbook/<scope>/<module>.md        module overview
 docs/handbook/<scope>/<module>/<name>.md detailed header/area page
 docs/design/                            design record, linked from the handbook
 docs/assets/                            styles, images, and diagrams
 tools/docs_templates/                   five reusable page templates
 tools/create_doc.py                      safe draft creation
 tools/regenerate_docs.py                 content-preserving regeneration and public-header detail pages
 tools/check_docs.py                      dependency-free validation
 tools/docs_site.py                       site rendering and page validation
 tools/docs_requirements.txt              pinned documentation dependencies
```

Source and include paths in metadata name actual repository files. The handbook scope is an audience grouping,
so it does not imply an extra `conjure/` component in implementation paths. Engine implementations live under
`engine/src/<module>/`. Tests may be nested below `tests/<module>/`.

## 3. Page templates

| Page type | Reusable template | Shape |
|---|---|---|
| Wiki topic | `tools/docs_templates/wiki.md` | Title, API breadcrumb, purpose, metadata, Syntax, At a glance, Variables and Properties, Functions, See also. |
| Handbook scope | `tools/docs_templates/handbook-scope.md` | Title, purpose, module table. |
| Handbook module | `tools/docs_templates/handbook-module.md` | Title, scope breadcrumb, purpose/position, metadata, header table; applicable internals, threading, extension points, and tests follow. |
| Handbook header | `tools/docs_templates/handbook-header.md` | Breadcrumbs, file/API metadata, purpose, and separate Structs and Classes indexes. |
| Handbook type | `tools/docs_templates/handbook-type.md` | Type/source/API metadata, Syntax, applicable inheritance, Constructors, Destructors, Variables, Functions, and supported Operator Overloads. |

The site supports metadata tables beginning with a separator row, without a visible column-heading row. Inside
table code spans, embedded Markdown links render as clickable code signatures. Ordinary inline code and fenced
examples remain literal. These two rendering extensions belong to the site; GitHub's native Markdown rendering
can display the embedded syntax literally. Standard Markdown tables and links remain supported in both places.

## 4. Tooling

`python tools/docs.py install` creates `.venv-docs/` and installs the pinned requirements. `check` runs with the
calling interpreter in isolated mode and uses only the standard library. `build` runs `mkdocs build --strict`
inside the documentation environment; `serve` runs the live preview. All subprocesses run at the repository root.

`python tools/docs.py new --help` describes draft creation. Drafts use the five reusable templates, calculate
relative breadcrumbs and sibling links, require real public headers for header/wiki drafts, and never overwrite
existing files. Authors supply the content and register navigation/index entries; the tool does not invent API
behaviour or claim a generated draft is finished documentation.

`python tools/docs.py regenerate` applies the five templates to existing API/handbook content and creates one
detail page per public header. It preserves authored prose/examples, refreshes source-derived metadata and
summaries, and registers header detail navigation. `--check` detects drift without writing. Authored additions
belong in the marked `doc-content` regions; generated metadata and summaries are recomputed from source.
The regeneration command does not modify design records or skill files.

Internal links must be relative. The checker resolves URL-escaped paths and query/fragment components,
requires targets to exist, verifies Markdown heading anchors, and rejects forbidden wiki destinations. HTTP(S)
and mail links are external references and are not crawled. Directory breadcrumbs require an `index.md` target;
the site turns them into explicit page links before rendering. Missing anchors and unrecognised internal site
links fail strict builds. Design records remain outside navigation and are listed informationally.

GitHub alerts render with the configured admonitions plugin. Mermaid fences render as diagrams through the
Material/SuperFences integration. Images and ordinary blockquotes use standard Markdown.

## 5. Integration with the development process

A change to a public header updates the wiki topic covering it. An implementation change updates its scoped
handbook module/header page. A new module adds its wiki content, handbook overview, scope/index rows, and
navigation entries. Documentation remains part of `AGENTS.md` section 6 and `08_testing.md` section 7.4 item 7.

Run `python tools/docs.py check`, then `python tools/docs.py build`, and read the generated page or live preview.
Report actual command results and any outstanding findings; another owner's finding still makes the check fail.

## 6. Tests

`tools/tests/test_check_docs.py` exercises validation with temporary repositories, including removed badge/length
requirements, every-header coverage, scoped pages, local links and anchors, code examples, and alerts.
`tools/tests/test_docs.py` covers all four draft types, safe paths, no overwrite, command dispatch, and failures.
Both suites use only the standard library and run under CTest with label `docs`.

`tools/tests/test_regenerate_docs.py` checks content preservation, adoption of handwritten header pages,
repeatability, source updates, safe planning, and read-only drift detection. CTest entries `docs.regeneration_tool`
and `docs.generated_up_to_date` run the suite and freshness check. CI also checks generation before building.

`tools/tests/test_docs_site.py` runs with the installed documentation toolchain. It tests metadata tables,
clickable code signatures, all alert kinds, Mermaid, flat output paths, link rewriting, and a real strict build.
The documentation CI step runs these rendering tests after installing requirements, then builds the full site.
