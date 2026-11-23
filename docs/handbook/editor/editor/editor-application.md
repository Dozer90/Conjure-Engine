---
kind: header
---

[Editor](../../editor.md) → [Applications](../applications.md)

# editor_application.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/editor_application.h` |
| API ↗ | [The AI control protocol](../../../api/editor/ai-protocol.md), [Headless mode and the command model](../../../api/editor/headless-and-commands.md), [The editor](../../../api/editor/index.md) |

<p>The editor process object: owns the <code><a href="../../engine/engine/application/application.html">engine::application</a></code>, the <code><a href="editor-context/editor-context.html">editor_context</a></code> and the <code><a href="protocol/protocol-server/protocol-server.html">protocol_server</a></code>, drives the frame integration headless (through a core layer) or with the UI layer the executable injects, restarts the engine when <code>project.open</code> names another project and runs --export / --exec / the interactive loop. The command file of --exec is read and parsed before the engine starts, so a missing or malformed file fails at once (decision).</p>

## Structs

| File | Description |
|---|---|
| [`startup_plan`](editor-application/startup-plan.md) | What <code>initialise()</code> starts from (decision), as <code>editor_application::plan_startup</code> decides it: the one engine description, made over the project the trust check let through and no other, and the project that was left out for the window to ask about (empty when none was). |
| [`startup_project`](editor-application/startup-project.md) | What a session does about the project its options name (decision), as <code>editor_application::resolve_startup_project</code> decides it. At most one of the two paths is set. |

## Classes

| File | Description |
|---|---|
| [`editor_application`](editor-application/editor-application.md) |  |

## Type Aliases

### `editor_application::layer_factory` {#symbol-editor-application-layer-factory}

```cpp
using layer_factory = std::function<std::unique_ptr<engine::layer>(editor_context &context)>;
```

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="editor_application"></a>
<a id="editor_applicationapply_pending_project_open"></a>
<a id="editor_applicationboot_engine"></a>
<a id="editor_applicationcan_ask_about_trust"></a>
<a id="editor_applicationcontext"></a>
<a id="editor_applicationeditor_application"></a>
<a id="editor_applicationeditor_application_1"></a>
<a id="editor_applicationengine"></a>
<a id="editor_applicationexit_code"></a>
<a id="editor_applicationhas_pending_project_open"></a>
<a id="editor_applicationheadless_tick"></a>
<a id="editor_applicationinitialise"></a>
<a id="editor_applicationis_initialised"></a>
<a id="editor_applicationlayer_factory"></a>
<a id="editor_applicationmake_engine_desc"></a>
<a id="editor_applicationmcontext"></a>
<a id="editor_applicationmengine"></a>
<a id="editor_applicationmexecrequests"></a>
<a id="editor_applicationmexitcode"></a>
<a id="editor_applicationminitialised"></a>
<a id="editor_applicationmoptions"></a>
<a id="editor_applicationmpendingproject"></a>
<a id="editor_applicationmquitrequested"></a>
<a id="editor_applicationmserver"></a>
<a id="editor_applicationmticks"></a>
<a id="editor_applicationmtrust"></a>
<a id="editor_applicationmuifactory"></a>
<a id="editor_applicationoptions"></a>
<a id="editor_applicationplan_startup"></a>
<a id="editor_applicationpump_frame"></a>
<a id="editor_applicationquit_requested"></a>
<a id="editor_applicationrequest_open_project"></a>
<a id="editor_applicationrequest_quit"></a>
<a id="editor_applicationrequire_trusted"></a>
<a id="editor_applicationresolve_startup_project"></a>
<a id="editor_applicationrun"></a>
<a id="editor_applicationrun_exec"></a>
<a id="editor_applicationrun_export"></a>
<a id="editor_applicationserver"></a>
<a id="editor_applicationshutdown"></a>
<a id="editor_applicationstart_engine"></a>
<a id="editor_applicationstartup_scene_warning"></a>
<a id="editor_applicationticks"></a>
<a id="editor_applicationtrust"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="startup_plan"></a>
<a id="startup_plandesc"></a>
<a id="startup_planunanswered"></a>
<a id="startup_project"></a>
<a id="startup_projectopen"></a>
<a id="startup_projectunanswered"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
