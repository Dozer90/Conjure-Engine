---
name: write-docs
description: Write or update a Conjure documentation page, either a Public API wiki page (docs/api) or an Engineering handbook page (docs/handbook), and check it. Use when a public header, a module or a workflow changes, when adding a module, or when asked to document anything for engine users or contributors.
metadata:
  last_updated: 22/11/2026
---

# Write documentation

Conjure has two Markdown document sets, published as one MkDocs site and readable on GitHub. The binding
design is `docs/design/16_documentation.md`; the contributor chapter is `docs/handbook/contributing/writing-documentation.md`.
Read both once. This skill is the procedure.

| Set | Directory | Reader |
|---|---|---|
| [Conjure icon] Public API wiki | `docs/api/<module>/<topic>.md` | people making games with the engine |
| 🛠 Engineering handbook | `docs/handbook/<chapter>.md`, scope → component → header → structure/class pages under `docs/handbook/<scope>/` | people working in the engine source |

## Rules that the tools enforce

Contributor chapters are grouped under Getting Started, Architecture and Design, Development, Extensions,
Contributing, and Project Reference. Their overview pages live in each matching directory's `index.md` beneath
`docs/handbook/`: `getting-started/`, `architecture-and-design/`, `development/`, `extensions/`, `contributing/`,
and `project-reference/`. Dependency guides are beneath `project-reference/third-party-dependencies/`.
Keep first-run instructions short; detailed presets, configurations, build options, and private build directories
belong in Development. Give every group an overview and every chapter descriptive heading links.

Preserve moved URLs using a small Markdown redirect page with `redirect_to: relative/path.md` in its front matter.
For a split chapter, `redirect_fragments` is a JSON object mapping old section anchors to their new relative links.
Targets must exist, remain within the documentation, and lead directly to a canonical page, without redirect chains.
Retain old section anchors in the destination content. Redirects stay out of navigation and search; regeneration
must preserve their metadata and fallback links. Update authored links to canonical locations.

- Links are relative and every target must exist (`mkdocs build --strict` and `check_docs.py` both fail otherwise).
- The wiki never links into `docs/design/`, `AGENTS.md` or `engine/`; it links to the handbook page instead.
- Every public header a wiki page covers is named by include path in a code span, e.g. `conjure/core/result.h`.
  `tools/check_docs.py` requires every header under `engine/include/conjure/<module>/` to appear that way in
  some page under `docs/api/<module>/`. Engine modules require `docs/handbook/engine/<module>.md`.
  The editor requires its application architecture page and every populated component overview.
- No `TODO`, `FIXME` or `XXX`.
- Callouts use GitHub alert syntax with these meanings and no other:

  | Syntax | Meaning |
  |---|---|
  | `> [!NOTE]` | background and context |
  | `> [!TIP]` | the better function or practice when this one is commonly misused |
  | `> [!IMPORTANT]` | the reader must do this for the feature to work |
  | `> [!WARNING]` | a gotcha: lifetimes, threading, frame ordering |
  | `> [!CAUTION]` | undefined behaviour or data loss if ignored |

## Rules that review enforces

- The code is the source of truth. Open the header and `tests/<module>/test_<header>.cpp` while writing.
  Generate short declarations from the actual source; never paste a whole header or invent a function
  or parameter. Use nested test files when the header belongs to an area within its module.
- Examples are complete, under 30 lines, compile against the real headers, follow `docs/CODE_STYLE.cpp` and
  copy their patterns from the tests. An example is mandatory where a function is only useful together with
  others (setup/teardown pairs, handles that must be checked, frame-ordering constraints).
- API tone: short sentences, easy to digest, no engine jargon without a link to `docs/api/glossary.md`,
  the simple facade first and the data-oriented escape hatch second. Handbook tone: precise and complete,
  detailed.
- Say it once: an explanation longer than a paragraph lives on one page and is linked from the others.
- British spelling, no emoji beyond prefixes; Oxford comma.
- All text should be formatted with appropriate line breaks if text block becomes too dense. Add graphs,
  diagrams, and images to assist in explanations where needed. Use quote markup when quoting something
  from a related page and a link to the relevant page + section it came from.

The engineering hierarchy is Scope → Component → Header → Structure/Class. Engine components are modules.
Editor components are Applications, Commands and Undo, Protocol, and User Interface; the editor application
is not an Engine module in this documentation taxonomy. Ownership comes from `tools/docs_components.py`,
independently of build libraries and namespaces. Classify new editor root headers there explicitly.
Editor component overviews use `docs/handbook/editor/<component>.md`. Existing editor header/type URLs under
`editor/editor/` remain stable; their breadcrumbs and navigation use the owning functional component.
Use lower-case, hyphenated URLs for engine modules:
`docs/handbook/<scope>.md`, `<scope>/<module>.md`, `<scope>/<module>/<header>.md`, and
`<scope>/<module>/<header>/<type>.md`. A type page represents a fully defined struct/class, including nested
definitions. Forward declarations do not get pages. Editor-library headers belong to the Editor scope.

- Module pages contain metadata, purpose, an actual CMake dependency graph, a Headers table with File,
  Description, and Provides columns, Breakdown, Threading Model, Extension Points, and Testing.
  Rewrite legacy chapters against the current implementation instead of placing the old chapter inside
  Breakdown. Keep one header index and the site's generated contents hierarchy; omit embedded contents lists.
  Use descriptive section links instead of detached section numbers. Preserve approved authored regions on refresh.
- Editor component overviews use the module layout with Type set to Component and an Implementation row
  naming the actual libraries/application. Include the component's own header index and authored explanations.
  Do not fabricate CMake dependency graphs for functional groups. Preserve the combined editor architecture
  overview at `docs/handbook/editor/editor.md`, titled Editor Architecture with Type Application; its content
  describes `conjure_editor`, `conjure::editor_core`, and `conjure::editor_ui`, without a duplicate header index.
- The sidebar has sibling Engineering Handbook, Engine, and Editor sections. Contributor chapters and general
  workflows belong under Engineering Handbook. Engine lists modules; Editor lists components and Architecture.
  Scope landing pages list Modules for Engine and Components for Editor.
  Each primary heading is a collapsible parent. Its name opens its overview; its arrow toggles every descendant.
  Keep all three parent headings visible, with inactive groups collapsed by default. Do not use permanently
  expanded theme section styling for these parents. On desktop, place disclosure arrows before the page
  names, indent every nested level, and align leaf names with sibling names using the same arrow gutter.
- Header pages contain breadcrumbs, file/API metadata, purpose, and separate Structs and Classes indexes.
  Include only populated indexes. Put enums under Enumerations with a C++ block containing each complete
  definition, underlying type, enumerator, and explicit value. Keep enumerators in source order, including
  conditional branches; never sort them or infer values. Prefer a definition over a duplicate forward declaration.
  Group other declarations logically: Type Aliases, Concepts, Unions, Forward Declarations, Constants,
  Variables, Macros, and Functions. Use code blocks for declarations and Name | Type | Value | Description
  tables for namespace constants/variables. Preserve useful authored notes and stable symbol anchors.
- Type pages contain breadcrumbs, type/namespace/include/source/API metadata, Syntax, applicable inheritance
  and derived classes, Constructors, Destructors, Variables, Functions, and supported Operator Overloads.
  Use `N/A` when no source file exists. Omit inheritance/derived sections when inapplicable, and operator
  sections when there are no supported overloads. Omit every empty section and header-only table, including
  empty member sections and Implementation Notes. Retain meaningful restrictions and callouts even when no
  callable constructor remains; do not add filler text saying that nothing is declared.
- Variable tables use Access | Name | Type | Default | Description. Names, types, defaults, and code mentioned
  in prose use code formatting. Link Conjure types, functions, and overload references. An em dash means no
  explicit initializer; do not infer one. Table columns size automatically to their contents. Header listings
  keep filenames and provided type names intact; the Provides column must accommodate complete identifiers.
- Table headers share bold italic styling. Metadata labels are bold and non-italic, with visible row dividers.
- Group function overloads beneath one linkable H4 per name, nested under the access heading. The H4 itself
  toggles all signatures/descriptions; there is no separate Details line. Groups start collapsed, with compact
  spacing that expands when opened. Regular section headings have 50% more top spacing than the original layout.
- Keep every explicit access level, qualifier, default argument, and non-deleted member. Arguments appear on
  separate lines with four-space indentation. Constructor names/types do not gain artificial word breaks.
  Function code stays inside its code box. Do not repeat return-type rows in descriptions.
- Operators have a separate section after Functions and the same H4 grouping. Omit deleted operators and
  deleted constructor rows. Put their copy/move restrictions in an Important callout below Constructors.
  When both copy operations are deleted, use exactly: "Copy constructor and assignment has been deleted."
- Sort modules, headers, types, variables, functions, and derived classes alphabetically, ignoring case.
  Preserve the hierarchy, access groups, and source order among overloads of the same name.
- Use all five callout types with their documented meanings. Keep the coloured outline, tinted heading band,
  and matching icon, with readable dark-theme fonts and colours.
- Graphs show the selected module's direct dependencies in alphabetical Public (left) and Private (right)
  columns, aligned at the bottom. Omit relationships between those dependencies; their own pages show them.
  Merge each access group into one incoming trunk and small filled arrowhead. Put consumers in separate
  access groups directly below the selected module, without a Consumers heading or repeated Public/Private
  headings. Keep public routes solid and private routes dashed,
  with no routes through unrelated nodes or shared segments between access groups. Use inward-facing lower
  corner dots for grouped dependencies, short 45-degree branches, and transparent cut-outs in node fills and
  outlines around connection dots. Never paint gaps with a background colour. Retain the 2.5-unit connector
  width, radius-4 dots, radius-8 cut-outs, and `7 5` private-edge dash spacing. Lines start at the filled dot's
  centre with no separate solid stem; private lines use their dash pattern from that point onward.
  End connector strokes at the arrowhead base so no line extends past the arrow tip. Do not add a legend or
  explanatory caption below the graph, either inside the SVG or in the page. Layout references are not to scale.
- Published pages never cite internal planning/decision files, agent directives, rollout labels, or design
  decision identifiers. Explain the implementation directly, preserving meaningful technical constraints.
- Use Futura Bk BT, white body text, Fira Code with `#C8C8C8` code, Ebrima bold page titles, and `#FFA800` links.
  Apply the same fonts and orange accent to navigation, search, header, and footer. Use the Conjure logo and
  favicon, and keep sticky sidebars within the content area so the footer cannot cover their links.
  Keep CSS and JavaScript sources readable, meaningfully named, and formatted with four spaces.

## Template: wiki page

````markdown
# <Topic title>
[API](../../..) -> [branch1](../..) -> [branch2](..)

<One or two sentences: what this is for and general use purpose.>

|---|---|
| Name | <name> |
| Module | <module> |
| `#include` | `conjure/<module>/<name>.h` |
| Engineering Handbook | [<Chapter title>](../../handbook/modules/<module>/<name>.md) |

## Syntax

```cpp
#include "conjure/<module>/<name>.h"
// complete, short, compiles; copied from tests/<module>/test_<name>.cpp
```

## At a glance

| If you want to... | Then use... |
|---|---|
| ... | `[function_name()](link to function in table below, eg '#function-name')` |

> [!TIP]
> The better function when this one is commonly misused.

> [!WARNING]
> Lifetime, threading or frame-ordering gotcha.

## Variables and Properties

| Name | Access | Purpose |
|---|---|---|
| `[matrix_id](link to 'matrix_id.md' or wherever the type documentation lies, eg: 'math#matrix_id') mSomeVariable` | public | ... |

## Functions

| Signature | Purpose |
|---|---|
| `[result](link to 'result-type.md')<T> [f](link to function doc page)([args](link for arg types where required))` | ... |

------------------------------------------

**See also:**
<only API links>
- [Related API link 1](link)
- [Related API link 2](link)
- ...
````

## Template: handbook scope page

````markdown
# <Chapter title>

<Purpose: what is the scope; when to use it.>

## Modules

| Module | Description |
|---|---|
| ... <include link to page> | ... |
````

## Template: handbook scope > module page

````markdown
<breadcrumbs>

# <title>

|---|---|
| Type | Module |
| Namespace | `<namespace>` |
| Include Folder Path | `<include path>` |
| Source Folder Path | `<source path>` |

<purpose>

## Dependency Graph

![<title> module dependencies](<dependency graph>)

## Headers

| File | Description | Provides |
|---|---|---|
<headers>

## Breakdown

<breakdown>

## Threading Model

<threading>

## Extension Points

<extensions>

## Testing

<testing>
````

## Template: handbook scope > module > header page

````markdown
<breadcrumbs>

# <title>

|---|---|
| File Path | `<include path>` |
| API ↗ | <api> |

<purpose>

## Structs

| File | Description |
|---|---|
<structs>

## Classes

| File | Description |
|---|---|
<classes>

## Enumerations

### `enumeration_name`

```cpp
enum class enumeration_name : type {
    NONE = 0,
    VALUE_01,
    VALUE_02,
    COUNT
};
```

<other populated declaration groups and authored notes>
````

## Template: handbook scope > module > header > structure/class page

````markdown
<breadcrumbs>

# <title>

|---|---|
| Type | <type> |
| Namespace | `<namespace>` |
| Include File Path | `<include path>` |
| Source File Path | <source path> |
| API ↗ | <api> |

## Syntax

```cpp
<syntax>
```

<inheritance>

## Constructors

<constructors>

## Destructors

<destructors>

## Variables

<variables>

## Functions

<functions>

<operators>
````

## Procedure

1. Pick the set, scope, module, and topic. A wiki topic can cover several headers; each engineering type page
   describes one complete definition beneath its owning header.
2. Create a missing page from the six supported page kinds. Commands never overwrite an existing file:

   ```bash
   python tools/docs.py new scope docs/handbook/engine.md --title "Engine"
   python tools/docs.py new module docs/handbook/engine/audio.md --module audio --title "Audio"
   python tools/docs.py new component docs/handbook/editor/protocol.md --scope editor --component protocol
   python tools/docs.py new header docs/handbook/engine/audio/audio-engine.md --module audio --header audio_engine.h
   python tools/docs.py new type docs/handbook/engine/audio/audio-engine/audio-engine.md --module audio --header audio_engine.h --symbol audio_engine
   python tools/docs.py new wiki docs/api/audio/playing-sounds.md --module audio --header audio_engine.h
   ```

   For Editor header/type drafts, `--module editor` identifies the existing include directory; the component
   is inferred from the header, e.g. `--scope editor --module editor --header protocol/access_token.h`.

3. Write with the headers, implementations, CMake dependencies, and matching tests open. Keep explanations
   in the marked `doc-content` regions. Maintained text in `tools/docs_content/` supplies reviewed descriptions
   and module explanations; these are authoring inputs, not generated output. Never claim an example compiles
   unless it was checked against the real headers.
4. Register new modules and topics in `mkdocs.yml`, the set index, and the glossary where relevant. Keep the
   Public API and Engineering handbook tabs. Regeneration registers header/type navigation under each module.
   Use scope, module, and header overview pages as clickable parent entries with separate expand/collapse
   arrows. Keep each overview first in its section's navigation data, but do not render it again as a child.
   List headers directly beneath their component, without a Headers dropdown. Type pages stay directly beneath
   their owning header.
5. Refresh computed pages, then check and build:

   ```bash
   python tools/docs.py regenerate                 # preserve authored text; refresh source metadata and indexes
   python tools/docs.py regenerate --check         # report drift without writing
   python tools/docs.py check                      # links, anchors, alerts, publication policy, header coverage
   python tools/docs.py build                      # strict MkDocs build; install once with docs.py install
   python tools/docs.py assets --format            # format editable source CSS and JavaScript when changed
   ```

   Fix every finding and build warning. Generated pages do not modify this skill or internal design records.
   Regeneration must be repeatable; authored edits survive it. Run the documentation tests when changing tools.
6. Open `site/<path>.html` or run `python tools/docs.py serve`. Review typography, table widths, callouts,
   diagrams, heading disclosures, contents entries, and relative links. Search and site navigation must work.
7. Commit documentation with the code it describes using a `<module>: ...` or `docs: ...` subject, after the
   repository's required review and validation. Documentation remains part of the definition of done.

## Report

State which pages and tools changed, which checks/builds/examples ran, and any remaining findings or limits.
