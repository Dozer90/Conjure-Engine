---
name: write-docs
description: Write or update a Conjure documentation page, either a Public API wiki page (docs/api) or an Engineering handbook page (docs/handbook), and check it. Use when a public header, a module or a workflow changes, when adding a module, or when asked to document anything for engine users or contributors.
---

# Write documentation

Conjure has two Markdown document sets, published as one MkDocs site and readable on GitHub. The binding
design is `docs/design/16_documentation.md`; the contributor chapter is `docs/handbook/documentation.md`.
Read both once. This skill is the procedure.

| Set | Directory | Reader | Badge |
|---|---|---|---|
| 📘 Public API wiki | `docs/api/<module>/<topic>.md` | people making games with the engine | `📘 **Public API wiki**` |
| 🛠 Engineering handbook | `docs/handbook/<chapter>.md`, module internals in `docs/handbook/modules/<module>.md` | people working in the engine source | `🛠 **Engineering handbook**` |

## Rules that the tools enforce

- The first content line after the `# Title` is the badge string above, exactly. Append ` · ` and the
  sibling pointer when the other set has a page on the topic: `🛠 Handbook: [Core module](../../handbook/modules/core.md)`
  on a wiki page, `📘 API: [Containers](../../api/core/containers.md)` on a handbook page.
- Every link into the other set carries the prefix `📘 API:` or `🛠 Handbook:`. Links are relative and
  every target must exist (`mkdocs build --strict` and `check_docs.py` both fail otherwise).
- The wiki never links into `docs/design/`, `AGENTS.md` or `engine/`; it links to the handbook page instead.
- Every public header a wiki page covers is named by include path in a code span, e.g. `conjure/core/result.h`.
  `tools/check_docs.py` requires every header under `engine/include/conjure/<module>/` to appear that way in
  some page under `docs/api/<module>/`, and every module under `engine/src/` to have
  `docs/handbook/modules/<module>.md`.
- No page over 400 lines (split into sub-pages with an index page, as `docs/handbook/modules/core.md` does).
  No `TODO`, `FIXME` or `XXX`.
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
  Quote signatures by hand in short form; never paste a header; never document a function or parameter that
  the header does not have.
- Examples are complete, under 30 lines, compile against the real headers, follow `docs/CODE_STYLE.cpp` and
  copy their patterns from the tests. An example is mandatory where a function is only useful together with
  others (setup/teardown pairs, handles that must be checked, frame-ordering constraints).
- Wiki tone: short sentences, no engine jargon without a link to `docs/api/glossary.md`, the simple facade
  first and the data-oriented escape hatch second. Handbook tone: precise and complete.
- Say it once: an explanation longer than a paragraph lives on one page and is linked from the others.
- British spelling, no emoji beyond the badges and prefixes.

## Template: 📘 wiki page

````markdown
# <Topic title>

📘 **Public API wiki** · 🛠 Handbook: [<Module> module](../../handbook/modules/<module>.md)

<One or two sentences: what this is for and the header(s) to include, e.g. `conjure/<module>/<name>.h`.>

## At a glance

| You want to... | Use |
|---|---|
| ... | `function_name()` |

## Quick example

```cpp
#include "conjure/<module>/<name>.h"
// complete, short, compiles; copied from tests/<module>/test_<name>.cpp
```

## <Concept section>

> [!TIP]
> The better function when this one is commonly misused.

> [!WARNING]
> Lifetime, threading or frame-ordering gotcha.

## API summary

| Signature (short) | Purpose |
|---|---|
| `result<T> f(args)` | ... |

## See also

- 📘 API: [Other page](other-page.md)
- 🛠 Handbook: [<Module> module](../../handbook/modules/<module>.md)
````

## Template: 🛠 handbook page

```markdown
# <Chapter title>

🛠 **Engineering handbook** · 📘 API: [<User-facing page>](../api/<module>/<topic>.md)

<Purpose paragraph: what the chapter covers and who needs it.>

## Contents

- [Section](#1-section)
- [Related](#related)

## 1. Section

### 1.1 Subsection

| Reference data | in tables |
|---|---|

> [!NOTE]
> Context the reader may skip.

## Related

- design record: [`NN_area.md`](../design/NN_area.md)
- 📘 API: [<User-facing page>](../api/<module>/<topic>.md)
```

A module page (`docs/handbook/modules/<module>.md`) covers, in this order: purpose and position in the
dependency graph, public headers (table), internals and algorithms, threading model, extension points,
tests and how to run them, `## Related` with the module's design note and README.

## Procedure

1. Pick the set and the topic; one wiki page may cover several headers, one handbook page covers one subject.
2. Create the file from the template. Lower-case file names with hyphens.
3. Write the badge line and the sibling pointer; add the reverse pointer on the sibling page.
4. Write the body with the header and its tests open. Name every covered header by include path.
5. Register the page: `nav` in `mkdocs.yml` under the right tab (keep the two-tab structure), the set's index
   table (`docs/api/index.md` or `docs/handbook/index.md`), and new terms in `docs/api/glossary.md`.
6. Check, then build:

   ```bash
   python tools/docs.py check                      # tools/check_docs.py: badge, links, length, markers, coverage
   python tools/docs.py build                      # mkdocs build --strict into site/ (install once: python tools/docs.py install)
   ```

   Fix every line `check` prints and every warning `build` prints. In the build output, a wiki or handbook
   page listed under "not included in the nav" is a mistake; design notes listed there are expected.
7. Open `site/<path>/index.html` (or `python tools/docs.py serve`) and read the page as its reader would;
   confirm the callouts rendered as boxes.
8. Commit the page with the code it documents (`<module>: ...` or `docs: ...` subject). Documentation is part
   of the definition of done (`AGENTS.md` 6, `docs/design/08_testing.md` 7.4 item 7).

## Report

State which pages were written or updated, that `check` and `build` are clean, and any header or module the
checker still reports that belongs to another wave or owner.
