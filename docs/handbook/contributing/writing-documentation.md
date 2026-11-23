# Writing Documentation
<!-- doc-template:chapter -->
[Handbook](../index.md) → [Contributing](index.md)

<!-- doc-content:purpose:start -->
<a id="documentation"></a>

Conjure publishes the Public API wiki for engine users and the Engineering handbook for contributors. This
chapter explains how to create pages, update their content, and verify the generated site.
<!-- doc-content:purpose:end -->

<!-- doc-content:body:start -->
<a id="1-the-two-sets"></a>

## The two sets

| Set | Reader | Organisation |
|---|---|---|
| [Public API wiki](../../api/index.md) | People making games with the engine. | Topics under `api/<module>/`; one topic may cover several headers. |
| [Engineering handbook](../index.md) | People changing engine source. | Contributor chapters, [Engine](../engine.md) and [Editor](../editor.md) scopes, module overviews, header indexes, and structure/class details. |

Wiki pages describe public usage and link to handbook pages for implementation details.

<a id="2-anatomy-of-a-page"></a>

## Anatomy of a page

<a id="21-badge-and-sibling-pointer"></a>

<a id="21-page-types-and-sibling-pointers"></a>

### Page types and sibling pointers

Use the six supported page kinds from `tools/docs_templates/`:

| Kind | Output location | Contents |
|---|---|---|
| `wiki` | `docs/api/<module>/<topic>.md` | Title, API breadcrumb, purpose, metadata, Syntax, At a glance, Variables and Properties, Functions, and See also. |
| `scope` | `docs/handbook/<scope>.md` | Title, purpose, and Modules (Engine) or Components (Editor). |
| `module` | `docs/handbook/<scope>/<module>.md` | Title, scope breadcrumb, purpose and dependency position, metadata, header table, and applicable implementation sections. |
| `component` | `docs/handbook/editor/<component>.md` | Functional editor component, implementation ownership, header index, and authored implementation sections. |
| `header` | `docs/handbook/<scope>/<module>/<header>.md` | Breadcrumbs, file/API metadata, purpose, and Structs and Classes indexes. |
| `type` | `docs/handbook/<scope>/<module>/<header>/<type>.md` | Type/source/API metadata, Syntax, applicable inheritance, Constructors, Destructors, Variables, Functions, and supported Operator Overloads. |

Scopes are `engine` and `editor`. The sidebar presents Engineering Handbook, Engine, and Editor as sibling
sections. Contributor chapters and general workflows sit under Engineering Handbook. Engine contains modules;
Editor contains Applications, Commands and Undo, Protocol, User Interface, and Editor Architecture.
The editor libraries remain in `engine/src/editor/`, but their documentation groups follow their functional
ownership in `tools/docs_components.py`. Existing editor header/type URLs remain stable; breadcrumbs and
navigation point to component overviews. Include and source metadata always describe real repository paths. Badges, numbered
headings, explicit contents lists, and a fixed page-line limit are optional rather than checker requirements.

Tables use automatic column sizing. Metadata labels are bold and non-italic; column headings share bold
italic styling. Variable tables separate Access, Name, Type, Default, and Description. Functions are alphabetical
within access groups, with one collapsed H4 disclosure for each name and its overloads. Supported operators
have their own section after Functions; empty operator sections and deleted operator declarations are omitted.
Deleted copy construction/assignment is described by an Important callout below Constructors.

<a id="22-link-conventions"></a>

### Link conventions

Use relative paths to Markdown source files and heading anchors. Use real page targets for breadcrumbs; when a
link names a directory, that directory must contain `index.md`. Include paths belong in inline code spans.

The checker follows inline links, images, reference definitions, and quoted HTML `href`/`src` values outside
literal code. It also follows embedded links inside table code signatures because those become links on the site.
URL-escaped filenames and query/fragment components are supported. Missing Markdown anchors fail validation.
HTTP(S) and mail links are accepted as external references and are not crawled.

<a id="23-alert-boxes"></a>

### Alert boxes

| Syntax | Meaning |
|---|---|
| `> [!NOTE]` | Background and context. |
| `> [!TIP]` | A better function or practice when this one is commonly misused. |
| `> [!IMPORTANT]` | An action required for the feature to work. |
| `> [!WARNING]` | Lifetime, threading, or frame-ordering gotchas. |
| `> [!CAUTION]` | Undefined behaviour or data loss. |

Write the marker alone on the first quoted line, with the message on following quoted lines. Separate boxes
with blank lines. The checker rejects other kinds and alternative admonition syntax; review checks meaning.

<a id="24-templates"></a>

### Templates

Create a draft with `python tools/docs.py new`; use `--help` for all options. For example:

```bash
python tools/docs.py new scope docs/handbook/engine.md --title "Engine"
python tools/docs.py new module docs/handbook/engine/core.md --module core --title "Core"
python tools/docs.py new component docs/handbook/editor/protocol.md --scope editor --component protocol
python tools/docs.py new header docs/handbook/engine/core/result.md --module core --header result.h
python tools/docs.py new type docs/handbook/engine/audio/audio-engine/audio-engine.md --module audio --header audio_engine.h --symbol audio_engine
python tools/docs.py new wiki docs/api/core/result.md --module core --header result.h
```

Choose an unused output path; these examples fail safely when the destination already exists. Add `--scope editor`
for Editor pages. `--header` is relative to the module include directory and must identify an existing header.
The command supplies structure and relative links, not API explanations or working examples. Registration in
navigation and indexes remains an author step.

Metadata tables can start with a separator row, without a visible header row. The site renders these without a header row.
Code signatures inside table cells can contain Markdown links; they remain code while their names are clickable.
This custom syntax is rendered by the site hook. Use conventional Markdown links/tables where identical GitHub
rendering is required. Ordinary code examples remain literal on both surfaces.

<a id="3-writing-rules"></a>

## Writing rules

- Open the real headers and their tests. Tests can be nested under `tests/<module>/`; locate the matching file.
- Name every covered public header by exact include path in an inline code span.
- Quote short signatures by hand; never paste an entire header or invent a function or parameter.
- Keep examples complete and under 30 lines. Follow `docs/CODE_STYLE.cpp` and patterns from actual tests.
- Use short API sentences, explain jargon or link the [glossary](../../api/glossary.md), and introduce the simple facade first.
- Explain handbook internals precisely. Keep shared explanations on one page and link to them elsewhere.
- Use British spelling, consistent punctuation, short paragraphs, and useful illustrations.

Semantic accuracy, callout meaning, prose quality, and example compilation require review; Markdown checks do
not establish them automatically.

<a id="4-tooling"></a>

## Tooling

<a id="41-toolsdocspy"></a>

### `tools/docs.py`

| Command | Behaviour |
|---|---|
| `python tools/docs.py install` | Creates `.venv-docs/` if needed, then installs `tools/docs_requirements.txt`. |
| `python tools/docs.py check` | Runs the dependency-free checker using the current interpreter in isolated mode. |
| `python tools/docs.py build` | Runs `mkdocs build --strict` in the documentation environment and writes `site/`. |
| `python tools/docs.py serve` | Starts the MkDocs live preview, normally at `http://127.0.0.1:8000`. |
| `python tools/docs.py new --help` | Describes the four draft types and their arguments. |
| `python tools/docs.py regenerate` | Refreshes all existing API/handbook pages and the detail page for every public header, preserving authored content. |
| `python tools/docs.py regenerate --check` | Reports generated-content drift without writing any files. |
| `python tools/docs.py assets` | Creates missing, readable CSS and JavaScript source assets without overwriting edits. |
| `python tools/docs.py assets --format` | Formats existing source CSS and JavaScript using four-space indentation. |

The four original commands take no additional arguments. Child exit codes are returned to the caller. Missing
toolchains and invalid command forms fail rather than silently installing packages or changing another environment.

Regeneration shares the same page layouts as draft creation. It updates metadata, module header tables, source
declaration summaries, implementation/test references, breadcrumbs, and navigation. Existing explanations and
examples are retained in `doc-content` regions; edit those regions to add handwritten detail. Generated header
summaries come from the real source comments, and shared algorithms/threading contracts link to module chapters.
Header pages index complete structures/classes and group enumerations, aliases, concepts, constants, variables,
and free functions by kind. Member details live on structure/class pages. Empty sections are omitted. Short
declarations retain access levels, static/constant qualifiers, and member signatures while omitting function
bodies. The lexical inventory includes conditional branches as written; it does not expand macros or infer
inherited members. Macro invocations remain visible as source declarations.
Legacy section anchors remain available after headings are rearranged. Run `regenerate` after changing public-header documentation or templates,
then run `check` and `build`.

<a id="42-toolscheck_docspy"></a>

### `tools/check_docs.py`

The checker scans the site landing page and every Markdown page beneath `docs/api/` and `docs/handbook/`. It checks relative local links,
Markdown anchors, forbidden wiki destinations, supported alert kinds/syntax, and unfinished marker words.
Published pages describe current behavior and technical contracts. Internal planning provenance, authoring
instructions, rollout labels, and reference identifiers are excluded. Regeneration cleans source-comment
citations, and the checker rejects remaining provenance before publication.
Every `.h` beneath `engine/include/conjure/<module>/` needs an exact include-path code span in that module's wiki:
umbrella headers, errors, logging, plumbing, and internal directories are included. A mention in plain text or
a fenced example does not satisfy the rule.

Engine modules need their scoped handbook overviews. The editor application retains the combined architecture
page at `handbook/editor/editor.md`; each populated editor component also needs its own overview. Header pages
and scope indexes supplement these overviews. Assign a new editor root header to a component explicitly.

The checker prints one line per finding and exits 1 when any remain; it prints `docs.check: clean` and exits 0
otherwise. Findings are not exempted by ownership; no badge or fixed page-length requirement applies.

<a id="43-test-entries-and-ci"></a>

### Test entries and CI

CTest entries `docs.check`, `docs.check_tool`, `docs.driver_tool`, `docs.regeneration_tool`, and
`docs.generated_up_to_date`, `docs.publication_policy_tool`, and `docs.declaration_inventory_tool` use label `docs`.
The tool suites and generated-content check run
with only the standard library. Rendering tests in `tools/tests/test_docs_site.py` run using the documentation
environment after its dependencies are installed, both locally and in the documentation CI step.

<a id="5-the-site"></a>

## The site

MkDocs Material supplies search, navigation, syntax highlighting, themes, and page tables of contents.
GitHub alerts use the admonitions plugin. Mermaid diagrams use fenced blocks labelled `mermaid`; images and
quotations use ordinary Markdown. The local site hook supplies metadata tables and linked code signatures.

`use_directory_urls: false` produces `site/api/core/containers.html` from `docs/api/core/containers.md`, and
`site/api/index.html` from `docs/api/index.md`. The live preview resolves the same pages over HTTP.

Strict builds fail on warnings and page-rule violations, including broken local links and anchors. Pages absent
from `nav` fail the strict build. Internal source-only material is excluded from publication. Register all new pages beneath the correct document-set tab and in the relevant scope/index tables.

<a id="6-documentation-in-the-development-process"></a>

## Documentation in the development process

### Editing stylesheets and JavaScript

Edit `docs/assets/stylesheets/conjure-docs.css` for Conjure-specific styling. The full theme and palette are
`material-theme.css` and `material-palette.css` in the same directory. Readable scripts are under
`docs/assets/javascripts/`, including `material-theme.js` and `workers/search-worker.js`; language helpers are in
`lunr/languages/`. File names are stable, without content hashes or misleading minified suffixes.

`build` and `serve` copy those source files into `site/assets/` and retain your changes. Files edited directly
under `site/` are replaced on the next build. Missing source assets are imported from the pinned theme package;
existing ones are never restored over your edits. Use `assets --format` when you want to reformat your edits.
Copyright comments are retained, and the theme licence is included in `docs/assets/licenses/`.

The dark theme uses Futura Bk BT for body text, Ebrima bold for the page title, and Fira Code for code.
Text is white (`#FFFFFF`), code is grey (`#C8C8C8`), and links are orange (`#FFA800`). These font families
use locally installed fonts, with fallback families declared in `conjure-docs.css`.

Update the wiki when public API changes and the relevant scope/module/header pages when implementation changes.
Documentation updates are part of the completion criteria for a change.
Report actual check/build results and outstanding findings; a finding owned elsewhere still means the check failed.

<a id="7-adding-a-page-step-by-step"></a>

## Adding a page, step by step

1. Choose the audience, topic, and page type. Create an unused path from its template.
2. Write the body with headers and tests open. Replace guidance comments with factual documentation.
3. Register `mkdocs.yml` navigation, the set/scope index, and any new glossary terms. Add sibling links where useful.
4. Run `python tools/docs.py check`, then `python tools/docs.py build`, and fix all findings and warnings.
5. Read `site/<path>.html` or the live preview. Inspect tables, code links, diagrams, and callout boxes.
6. Review and commit the page with the code it describes.

## Related

- [Writing Documentation](writing-documentation.md#9-documentation-toolchain)
- [Testing and Benchmarks](../development/testing-and-benchmarks.md#10-labels)
- [Continuous Integration](../development/continuous-integration.md#2-the-matrix)

<a id="9-documentation-toolchain"></a>

## Documentation toolchain

The documentation is Markdown under `docs/`, published as one MkDocs Material site (`mkdocs.yml`), and
checked structurally by `tools/check_docs.py`. `tools/docs.py` wraps everything:

```bash
python tools/docs.py install      # creates .venv-docs and installs tools/docs_requirements.txt
python tools/docs.py serve        # live-reloading preview at http://127.0.0.1:8000
python tools/docs.py build        # mkdocs build --strict into site/ (broken links fail)
python tools/docs.py check        # links, alerts, all-header code spans, scoped module pages
python tools/docs.py new --help   # create a draft from one of the four page templates
python tools/docs.py regenerate   # refresh authored pages and all public-header details
```

`tools/docs_requirements.txt` pins MkDocs, Markdown, PyMdown Extensions, Material, and the GitHub alerts plugin.
`.venv-docs/` and `site/` are ignored by git. `check` needs no virtual environment; it runs
`python -I tools/check_docs.py` and reports one line per problem: a broken relative target or Markdown anchor,
a forbidden wiki destination, unsupported alert syntax, an unfinished marker word, a public header without an
exact include-path code span in its module's wiki, or a missing scoped handbook module page. Badges and a fixed
page-length limit are not required. See [Writing Documentation](writing-documentation.md#4-tooling) for the full contract.

<a id="contents"></a>
## Handbook Organisation

The Engineering Handbook groups shared contributor guidance into Getting Started, Architecture and Design,
Development, Extensions, Contributing, and Project Reference. Each group has a directory and an `index.md`
overview. Engine and Editor remain separate implementation-reference scopes in the sidebar.

Put first-run instructions in Getting Started, and build presets, configurations, options, and private build
maintenance in Development. Keep procedural chapters focused and use links for shared reference information.
Use descriptive headings and the site's contents list instead of embedding a second contents list in a chapter.

When a page moves, update its authored links and retain its old URL as a redirect page. Set `redirect_to` to
its new relative Markdown path. If sections moved to different pages, use `redirect_fragments` to map the old
section anchors to their new relative links. The checker validates these destinations and their anchors.
Keep old anchors at the destinations so existing bookmarks still work; avoid redirect chains.
<!-- doc-content:body:end -->
