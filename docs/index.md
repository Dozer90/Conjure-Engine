# Conjure Engine documentation

Conjure is a data-oriented 2D/3D game engine with C# scripting, an AI-controllable editor and a standalone
runtime. Its documentation comes in two sets written for two different readers. Pick yours:

| | 📘 Public API wiki | 🛠 Engineering handbook |
|---|---|---|
| **For** | people making games with Conjure: designers, gameplay programmers, C# scripters, AI agents driving the editor | people working in the engine source: engine programmers, module authors, contributors |
| **Style** | task-oriented pages, short examples, tips and gotchas in coloured boxes | Confluence-style chapters with a table of contents, covering the whole codebase |
| **Start here** | [Public API wiki](api/index.md) | [Engineering handbook](handbook/index.md) |

Every page opens with its badge (📘 or 🛠) so you always know which set you are reading, and links that jump
to the other set are labelled `📘 API:` or `🛠 Handbook:`.

> [!TIP]
> Use the search box at the top of this site; it indexes both sets. On GitHub, the same pages live under
> [`docs/api`](api/index.md) and [`docs/handbook`](handbook/index.md).

## Callout boxes used throughout

> [!NOTE]
> Background and context.

> [!TIP]
> The better function or practice when there is more than one way to do something.

> [!IMPORTANT]
> Something you must do for the feature to work (for example, an initialisation order).

> [!WARNING]
> A gotcha that bites in practice: lifetimes, threading, frame ordering.

> [!CAUTION]
> Undefined behaviour or data loss if ignored.
