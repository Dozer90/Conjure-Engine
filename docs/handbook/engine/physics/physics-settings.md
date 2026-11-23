---
kind: header
---

[Engine](../../engine.md) → [Physics](../physics.md)

# physics_settings.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/physics/physics_settings.h` |
| API ↗ | [Physics](../../../api/physics/index.md), [The physics world and queries](../../../api/physics/physics-world-and-queries.md) |

<p>World construction parameters. Everything a <code><a href="physics-world/physics-world.html">physics_world</a></code> sizes at construction lives here; the engine&#x27;s <code>physics_subsystem</code> fills it with <code>from_json()</code> from the raw <code>project.json</code> &quot;physics&quot; section (<code>snake_case</code> keys) and &quot;<code>extensions.physics_layers</code>&quot;.</p>

## Structs

| File | Description |
|---|---|
| [`physics_settings`](physics-settings/physics-settings.md) |  |

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="physics_settings"></a>
<a id="physics_settingsallowsleeping"></a>
<a id="physics_settingscollisionsteps"></a>
<a id="physics_settingsdeterministic"></a>
<a id="physics_settingsestimatecontactimpulses"></a>
<a id="physics_settingsfrom_json"></a>
<a id="physics_settingsgravity"></a>
<a id="physics_settingslayers"></a>
<a id="physics_settingsmaxbarriers"></a>
<a id="physics_settingsmaxbodies"></a>
<a id="physics_settingsmaxbodypairs"></a>
<a id="physics_settingsmaxcharacters"></a>
<a id="physics_settingsmaxcontactconstraints"></a>
<a id="physics_settingsmaxcontacteventsperstep"></a>
<a id="physics_settingsmaxjobs"></a>
<a id="physics_settingsmaxqueuedcommands"></a>
<a id="physics_settingsnumbodymutexes"></a>
<a id="physics_settingsnumpositionsteps"></a>
<a id="physics_settingsnumvelocitysteps"></a>
<a id="physics_settingspenetrationslop"></a>
<a id="physics_settingspointvelocitysleepthreshold"></a>
<a id="physics_settingsspeculativecontactdistance"></a>
<a id="physics_settingstempallocatorbytes"></a>
<a id="physics_settingstimebeforesleepseconds"></a>
<a id="physics_settingsvalidate"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
