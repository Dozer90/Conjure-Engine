# Documentation

🛠 **Engineering handbook** · 📘 API: [Public API wiki](../api/index.md)

Conjure ships two documentation sets written from the same repository: the Public API wiki for people making
games with the engine and this Engineering handbook for people working in the source. This chapter is for
anyone who writes or reviews a page: what the sets are, how a page is laid out, which tools check it and how
documentation fits into the definition of done. The binding record is
[`16_documentation.md`](../design/16_documentation.md); this chapter is its working guide.

## Contents

- [The two sets](#1-the-two-sets)
- [Anatomy of a page](#2-anatomy-of-a-page)
- [Writing rules](#3-writing-rules)
- [Tooling](#4-tooling)
- [The site](#5-the-site)
- [Documentation in the development process](#6-documentation-in-the-development-process)
- [Adding a page, step by step](#7-adding-a-page-step-by-step)
- [Related](#related)

## 1. The two sets

| | 📘 Public API wiki (`docs/api/`) | 🛠 Engineering handbook (`docs/handbook/`) |
|---|---|---|
| Reader | designers, gameplay programmers, C# scripters, agents driving the editor | engine programmers, module authors, contributors |
| Organised by | what the reader wants to do; one page per user-facing topic, small things grouped (D5) | the codebase: one chapter per subject, one page per engine module under `modules/` (D6) |
| Shape | purpose, *At a glance*, *Quick example*, concepts with callouts, *API summary*, *See also* | purpose, explicit `## Contents`, numbered sections, tables for reference data, `## Related` |
| Links to `docs/design/` | never (D1) | wherever a decision is explained there rather than here |
| Tone | short sentences, no engine jargon without a link to the [glossary](../api/glossary.md), OOP facade first and data-oriented escape hatch second (D10) | precise and complete; assumes the reader can open the source |

The landing page [`docs/index.md`](../index.md) lets a reader pick a set; `docs/api/index.md` and
`docs/handbook/index.md` are the entry pages of each set. `docs/design/` is the design record, `docs/ARCHITECTURE.md`
the binding module map and `docs/CONVENTIONS.md` the engineering conventions; they belong to neither set and are
linked from the handbook only.

## 2. Anatomy of a page

### 2.1 Badge and sibling pointer

The first content line after the H1 is the badge, exactly `📘 **Public API wiki**` or `🛠 **Engineering handbook**`
(`tools/check_docs.py` compares the string). When the other set has a sibling page, the badge line continues with
` · ` and the pointer to it, written with the cross-set prefix. The two badge lines below are those of
`docs/api/core/containers.md` and `docs/handbook/modules/core.md`, with their link targets rewritten relative to
this chapter (from `docs/api/core/` the first target is `../../handbook/modules/core.md`):

```markdown
# Containers

📘 **Public API wiki** · 🛠 Handbook: [Core module](modules/core.md)
```

```markdown
# Core module

🛠 **Engineering handbook** · 📘 API: [Errors and results](../api/core/errors-and-results.md)
```

A handbook page may point at several wiki pages (the core area pages do); a wiki page points at the handbook page
of its module. A page without a sibling carries the badge alone.

### 2.2 Link conventions

| Link | Written as |
|---|---|
| within the same set | plain relative link: `[Testing](testing.md)` |
| into the other set | prefixed with the target set's badge: `📘 API: [Jobs and threading](../api/core/jobs.md)` from a handbook page, `🛠 Handbook:` before the link from a wiki page |
| into the design record (handbook only) | `[10_core.md](../design/10_core.md)` or the bare path in a code span under `## Related` |
| to a header | its include path in a code span, `conjure/core/result.h`, never a link to a file in `engine/` |
| to an anchor | `[Section](#2-anatomy-of-a-page)`; the slug is the lower-cased heading with punctuation removed and spaces as hyphens |

Every link is relative and its target must exist: `mkdocs build --strict` and `check_docs.py` both fail on a
dangling path. The wiki never links into `docs/design/`, `AGENTS.md` or `engine/`; it links to the handbook
instead, which links on.

### 2.3 Alert boxes

Callouts use the GitHub alert syntax so they render on GitHub as well as on the site, where the
`gh-admonitions` plugin turns them into Material admonitions. The five kinds have fixed meanings (D3) and
every page uses them the same way; the reader-facing legend is on the
[landing page](../index.md#callout-boxes-used-throughout).

| Syntax | Use it for | Rendered class |
|---|---|---|
| `> [!NOTE]` | background and context the reader may skip | `note` |
| `> [!TIP]` | the better function or practice when the one being read is commonly misused (the "nudge") | `tip` |
| `> [!IMPORTANT]` | something the reader must do for the feature to work: an initialisation order, a required call | `important` |
| `> [!WARNING]` | a gotcha that bites in practice: lifetimes, threading, ordering within the frame | `warning` |
| `> [!CAUTION]` | undefined behaviour or data loss if ignored | `danger` |

The marker is the first line of the quote on its own; the text follows on the next quoted lines. A blank line
separates two consecutive boxes, otherwise Markdown merges them into one.

> [!WARNING]
> `admonition`-style syntax (`!!! note`) renders on the site but not on GitHub. Use the GitHub alert syntax only;
> a page that mixes the two reads differently in the two places.

### 2.4 Templates

The two page templates are in [`16_documentation.md` section 3](../design/16_documentation.md#3-page-templates)
and reproduced ready to copy in `.claude/skills/write-docs/SKILL.md`. Their required parts:

| Set | Required sections, in order |
|---|---|
| 📘 wiki page | H1, badge line, one or two sentences naming the header(s) to include, `## At a glance` (*You want to... / Use* table), `## Quick example`, concept sections with callouts, `## API summary` (short signatures / purpose), `## See also` |
| 🛠 handbook page | H1, badge line, purpose paragraph, `## Contents` (GitHub shows no automatic table of contents), numbered `## N. Section` / `### N.M Subsection`, tables for reference data, `## Related` ending with the design record |

## 3. Writing rules

- **The code is the source of truth** (D7). Quote signatures by hand in short form, never paste a header. When a
  header and a page disagree, the header wins and the page is fixed in the same pull request.
- **Examples are complete and short** (D8): under 30 lines, compile against the real headers, copy their patterns
  from `tests/<module>/`, use the engine's code style (`docs/CODE_STYLE.cpp`). An example is mandatory wherever a
  function only makes sense together with others: setup/teardown pairs, handles that must be checked, calls with a
  frame-ordering constraint.
- **Name every header you cover by include path** in a code span (`conjure/core/result.h`). The coverage check
  (section 4.2) looks for that exact string in the wiki pages of the header's module.
- **One page, one topic, under 400 lines.** Split a long page into sub-pages with an index (the core module does:
  `modules/core.md` plus `modules/core/foundation-to-memory.md` and `modules/core/time-to-reflect.md`).
- **Say it once.** An explanation longer than a paragraph lives on one page; other pages link to it. Terms the
  wiki uses without explaining go into the [glossary](../api/glossary.md) with the page that explains them.
- **No to-do, fix-me or triple-X markers** in a page (the exact words are the tuple in `check_page()` of
  `tools/check_docs.py`); unfinished parts are left out and listed in the pull request instead.
- **Spelling and tone** follow the code base: British spelling (`initialise`, `behaviour`), no emoji beyond the two
  badges and the two link prefixes, no exclamation marks.

## 4. Tooling

### 4.1 `tools/docs.py`

`tools/docs.py` wraps the toolchain; [Getting started](getting-started.md#9-documentation-toolchain) covers the
one-off `install`. The pinned versions are in `tools/docs_requirements.txt` and the virtual environment is
`.venv-docs/` (ignored by git, like `site/`).

| Command | Runs | Needs the virtual environment |
|---|---|---|
| `python tools/docs.py install` | creates `.venv-docs/` and installs `tools/docs_requirements.txt` | creates it |
| `python tools/docs.py serve` | `mkdocs serve`, live preview at `http://127.0.0.1:8000` | yes |
| `python tools/docs.py build` | `mkdocs build --strict` into `site/`; any warning fails | yes |
| `python tools/docs.py check` | `python -I tools/check_docs.py` | no |

### 4.2 `tools/check_docs.py`

The structural checker implements D9. It walks every `*.md` under `docs/api/` and `docs/handbook/`, prints one
line per problem and exits 1 when any exists:

| Check | Fails when |
|---|---|
| badge | the first content line after the H1 (front matter skipped) does not contain the set's badge string |
| links | a relative Markdown link points at a file that does not exist, anywhere in the page including code blocks; `http(s):`, `mailto:` and `#anchor` links are not checked and anchors are not verified |
| length | a page exceeds 400 lines |
| markers | the page contains a to-do, fix-me or triple-X marker word, in any context |
| header coverage | a header under `engine/include/conjure/<module>/` is not mentioned by include path (`conjure/<module>/<name>.h`) in any page under `docs/api/<module>/` |
| module pages | a directory under `engine/src/` with a `CMakeLists.txt` has no `docs/handbook/modules/<module>.md` |

`needs_wiki_page()` decides which headers the coverage rule applies to. It currently returns `True` for every
header, so umbrella headers (`conjure/core/math.h`), `log_category.h` and `*_error.h` files must be mentioned
too; the intended exclusion policy (umbrella includes, generated files, `detail/` directories) is documented in
the function and not implemented yet. The core wiki pages mention every core header, so core is clean; the
checker reports the headers and module pages of every later-wave module whose code is already in the tree.

> [!TIP]
> Run `python tools/docs.py check` first and `python tools/docs.py build` second. The checker's messages name
> the page and the missing header or target; the strict build then catches what the checker does not see, such as
> a page missing from the `nav` or a link into a file the site excludes.

### 4.3 Planned test entries and CI

D9 registers `check_docs.py` as the CTest entry `docs.check` (label `docs`) and section 6 of the record adds a
self-test `docs.check_tool` with fixtures under `tools/tests/check_docs/`. Neither exists yet: `tests/CMakeLists.txt`
registers only `style.check`, the fixture directory is empty and `.github/workflows/ci.yml` has no `docs` job
([Continuous integration](ci.md#6-planned-jobs), [Testing](testing.md#10-labels)). Until they land,
run both commands by hand before every documentation change is reported done.

## 5. The site

`mkdocs.yml` at the repository root builds both sets into one MkDocs Material site (D2):

| Setting | Value | Why |
|---|---|---|
| `theme.features` | `navigation.tabs`, `navigation.tabs.sticky`, `navigation.sections`, `navigation.indexes`, `search.suggest`, `search.highlight`, `content.code.copy`, `content.action.edit` | one tab per set, section index pages (`api/index.md`, `handbook/index.md`, `modules/core.md`), search across both sets, copy buttons on code, an edit link per page (`edit_uri`) |
| `plugins` | `search`, `gh-admonitions` | full-text search; GitHub alerts rendered as admonitions |
| `markdown_extensions` | `tables`, `fenced_code`, `attr_list`, `admonition`, `toc` (permalinks, depth 3), `pymdownx.superfences`, `pymdownx.highlight`, `pymdownx.inlinehilite`, `pymdownx.tabbed` | tables and fenced code as on GitHub, heading anchors, syntax highlighting |
| `strict: true` | always | the build fails on any warning, including a broken link or a page missing from `nav` |
| `exclude_docs` | `CODE_STYLE.cpp` | a C++ file is not a page |
| `nav` | every page of both sets, listed explicitly | pages not in `nav` are still built but the strict build warns about them, so every new page gets a `nav` entry |

`docs/design/`, `docs/ARCHITECTURE.md`, `docs/CONVENTIONS.md` and `docs/DESIGN_DOC.md` are built so the handbook's
links into them resolve, but they are deliberately absent from `nav`: they are reached from the handbook's
chapters ([Decision records](decision-records.md) in particular) and appear in search, never in the tabs.

> [!NOTE]
> `strict: true` makes "page exists but is not in nav" an informational message, not a failure; the build output
> lists those pages under "The following pages exist in the docs directory, but are not included in the nav". Read
> that list after adding a page: a wiki or handbook page in it is a mistake, a design note in it is expected.

## 6. Documentation in the development process

Documentation is written per wave by the module owner and reviewed like code (D10). It is part of the definition
of done in three places that say the same thing: `AGENTS.md` section 6, [`08_testing.md` 7.4 item 7](../design/08_testing.md#74-definition-of-done-for-a-module)
and the checklist in [Contributing](contributing.md#8-definition-of-done): *the public API wiki and handbook pages
for the change are written or updated and `docs.check` passes*. Concretely, a change that touches a public header
updates the wiki page that covers it, a change that touches a module's internals updates
`docs/handbook/modules/<module>.md`, and a new module adds both plus a row in the two index pages and in `nav`.

Wave 1 documented core; waves 2 to 5 document their modules in the same pull request as the code, and the final
acceptance review audits both sets for completeness and readability.

## 7. Adding a page, step by step

1. Decide the set and the topic. A wiki page answers "how do I ...?" for a game developer and may cover several
   headers; a handbook page describes one subject of the codebase for a contributor.
2. Copy the template from `.claude/skills/write-docs/SKILL.md` into `docs/api/<module>/<topic>.md` or
   `docs/handbook/<chapter>.md` (module internals: `docs/handbook/modules/<module>.md`). File names are
   lower-case with hyphens.
3. Write the badge line with the sibling pointer, then the body. Open the headers and the tests while writing;
   take examples from `tests/<module>/`.
4. Add the page to `nav` in `mkdocs.yml` under its tab, to the set's index page table and, for a wiki page, add
   any new term to `docs/api/glossary.md`. Link the sibling page back.
5. `python tools/docs.py check`, then `python tools/docs.py build`; fix everything either reports.
6. Open `site/<path>/index.html` or `python tools/docs.py serve` and read the page once as its reader would.
7. Review the page with the code review, commit it with the code it documents.

## Related

- design record: [`16_documentation.md`](../design/16_documentation.md) (decisions D1 to D10, layout, templates, tooling, integration, tests)
- design record: [`08_testing.md`](../design/08_testing.md) section 7.4 (definition of done for a module, item 7)
- `.claude/skills/write-docs/SKILL.md` (the same rules as an agent skill, with the templates ready to copy)
- [Getting started](getting-started.md#9-documentation-toolchain), [Testing](testing.md), [Continuous integration](ci.md), [Contributing](contributing.md), [Repository layout](repository-layout.md#5-documentation)
- 📘 API: [Public API wiki](../api/index.md), [Glossary](../api/glossary.md)
