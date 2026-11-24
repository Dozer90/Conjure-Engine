using System;
using System.Runtime.CompilerServices;
using Conjure.Interop;
using Conjure.Runtime;

namespace Conjure;

/// <summary>
/// Per-callback consecutive fault counters (one byte per ScriptCallback).
/// </summary>
[InlineArray((int)ScriptCallback.Count)]
public struct FaultCounters
{
    private byte _element;
}

/// <summary>
/// Index of the instance in each phase list (-1 when absent).
/// </summary>
[InlineArray(3)]
public struct PhaseIndexSet
{
    private int _element;
}

/// <summary>
/// Base class of every script (docs/design/03_scripting.md 4.1). Override the callbacks you need; the
/// engine calls them on the main thread inside its exclusive scripting systems. Public fields and
/// [SerializeField] fields of supported types are saved with the scene and survive hot reload.
/// </summary>
public abstract class ScriptBehaviour
{
    internal Entity EntityValue;
    internal int Slot = -1;
    internal bool Started;
    internal bool Active = true;
    internal ScriptClassInfo? ClassInfo;
    internal FaultCounters ConsecutiveFaults;
    internal ushort DisabledMask;
    internal PhaseIndexSet PhaseIndices;

    protected ScriptBehaviour()
    {
        PhaseIndices[0] = -1;
        PhaseIndices[1] = -1;
        PhaseIndices[2] = -1;
    }

    /// <summary>
    /// The entity this script is attached to.
    /// </summary>
    public Entity Entity => EntityValue;

    /// <summary>
    /// The entity's transform facade.
    /// </summary>
    public Transform Transform => new Transform(EntityValue);

    /// <summary>
    /// True once OnStart ran (or the instance was created as already started after a reload).
    /// </summary>
    public bool HasStarted => Started;

    /// <summary>
    /// True while the script runs: its entity is alive and neither it nor an ancestor is disabled, as of the last
    /// point where activity changes take effect (every flush of the pending creates and destroys, which runs at the
    /// end of each phase and at the frame's begin and end, the start of each phase and collision dispatch, and
    /// before a hot reload or snapshot restore recreates the instance), so it stays true for the rest of the phase
    /// in which the entity was disabled. It reads false inside OnDisable and OnDestroy and after the instance was
    /// destroyed. While it is false the script gets no update or collision callbacks.
    /// </summary>
    public bool IsActive => Active;

    /// <summary>
    /// Callbacks disabled after three consecutive faults (bits of ScriptCallback).
    /// </summary>
    public ushort DisabledCallbacks => DisabledMask;

    /// <summary>
    /// Entity.GetComponent: a copy of the component; EntityNotAliveException for a dead entity,
    /// InvalidOperationException when the entity lacks it.
    /// </summary>
    public T GetComponent<T>() where T : unmanaged => EntityValue.GetComponent<T>();

    /// <summary>
    /// Entity.HasComponent: true when the entity carries a T, false for a dead entity.
    /// </summary>
    public bool HasComponent<T>() where T : unmanaged => EntityValue.HasComponent<T>();

    /// <summary>
    /// Entity.AddComponent: EntityNotAliveException for a dead entity, InvalidOperationException when the entity
    /// already has it (deferred inside a ComponentQuery scope, where it cannot fail at the call).
    /// </summary>
    public void AddComponent<T>(in T value) where T : unmanaged => EntityValue.AddComponent(in value);

    /// <summary>
    /// Entity.SetComponent: EntityNotAliveException for a dead entity, InvalidOperationException when the entity
    /// lacks it.
    /// </summary>
    public void SetComponent<T>(in T value) where T : unmanaged => EntityValue.SetComponent(in value);

    /// <summary>
    /// Entity.RemoveComponent: true when removed, false when there was nothing to remove.
    /// </summary>
    public bool RemoveComponent<T>() where T : unmanaged => EntityValue.RemoveComponent<T>();

    /// <summary>
    /// Another script on the same entity, or null.
    /// </summary>
    public T? GetScript<T>() where T : ScriptBehaviour => EntityValue.GetScript<T>();

    /// <summary>
    /// Destroys the entity (OnDestroy runs with the entity still alive, at the end of the phase); does nothing
    /// when the entity is already dead (Entity.Destroy returns false for that).
    /// </summary>
    public void Destroy() => EntityValue.Destroy();

    /// <summary>
    /// Runs once per entity lifetime, when the instance is created. A hot reload of the game assembly recreates
    /// the instance without running OnCreate again (OnReload runs instead; OnStart does not run again either),
    /// so a one-shot effect started here does not replay; an instance whose creation was still queued at the
    /// reload runs OnCreate on the new assembly. The entity can already carry the component when OnCreate runs:
    /// on a copy made by Entity.Duplicate, or on a prefab or scene instance authored with it. So an AddComponent
    /// here checks HasComponent first: adding one the entity already has throws, and an exception in OnCreate
    /// disables every callback of the instance until the next reload. An OnCreate that threw did not complete:
    /// that reload (once the code is fixed) runs OnCreate once more instead of OnReload, and then OnStart, which
    /// the disabled instance never ran.
    /// </summary>
    protected virtual void OnCreate()
    {
    }

    /// <summary>
    /// Runs on each instance a hot reload of the game assembly recreated, in place of OnCreate, when its OnCreate
    /// completed (one whose OnCreate threw runs OnCreate again instead): after the serialised fields were
    /// restored and before the next phase. A play-mode snapshot restore, or a replace of the entity's scripts
    /// from JSON saved after OnCreate returned, recreates the instance the same way. The reload runs no script
    /// phase, so a
    /// ComponentQuery begun here is refused. Private and [NonSerialized] state starts from its initialisers, so
    /// this is the place to rebuild caches; the entity kept its components. An exception here disables every
    /// callback of the instance until the next reload, like one in OnCreate.
    /// </summary>
    protected virtual void OnReload()
    {
    }

    /// <summary>
    /// Runs once before the first OnFixedUpdate or OnUpdate of the instance, after OnEnable. An instance created on
    /// an inactive entity runs it after its first activation instead.
    /// </summary>
    protected virtual void OnStart()
    {
    }

    /// <summary>
    /// Runs when the script becomes active: right after OnCreate when the entity is active then, and each time the
    /// entity becomes active again (it, or the ancestor that disabled it, is enabled), before OnStart when that never
    /// ran. The change takes effect at the end of the phase that made it; one made outside a phase takes effect at
    /// whichever comes first of the frame's begin or end flush, the start of the next phase or collision dispatch,
    /// and the point before a hot reload or snapshot restore recreates the instance, where it runs on the old
    /// instance (in those flushes no phase runs, so a ComponentQuery opened here is refused). Not after OnReload: a
    /// recreated instance keeps its state. Three consecutive exceptions disable only this callback.
    /// </summary>
    protected virtual void OnEnable()
    {
    }

    /// <summary>
    /// Runs when the script becomes inactive (its entity, or an ancestor, was disabled) at the end of the phase that
    /// disabled it, the point where a Destroy takes effect, not inside the setter: the rest of that phase still runs
    /// the entity's scripts. A change made outside a phase takes effect at the same points as for OnEnable. Until
    /// OnEnable the script gets no OnFixedUpdate, OnUpdate, OnLateUpdate or collision callback. It also runs before
    /// OnDestroy when an active script is destroyed (not on a hot reload, and not when a snapshot restore recreates
    /// the same script with OnReload). Three consecutive exceptions disable only this callback.
    /// </summary>
    protected virtual void OnDisable()
    {
    }

    protected virtual void OnFixedUpdate(float fixedDeltaTime)
    {
    }

    protected virtual void OnUpdate(float deltaTime)
    {
    }

    protected virtual void OnLateUpdate(float deltaTime)
    {
    }

    /// <summary>
    /// Runs once when the script is destroyed (its entity destroyed, the script removed, Play stopped, the scene
    /// unloaded), after OnDisable when the script was active. Entity.Destroy() keeps the entity alive for it; an
    /// entity native code destroyed directly is already gone. A hot reload recreates the instance without it, and
    /// so does a snapshot restore or script replacement that brings back the same script as already created.
    /// </summary>
    protected virtual void OnDestroy()
    {
    }

    protected virtual void OnCollisionEnter(in Collision collision)
    {
    }

    protected virtual void OnCollisionExit(in Collision collision)
    {
    }

    /// <summary>
    /// Runs every fixed step a solid contact persists after OnCollisionEnter, with the contact's current point and
    /// normal and an impulse of 0. A pair whose bodies fall asleep reports OnCollisionExit instead (and
    /// OnCollisionEnter when one wakes); triggers have no stay. The engine converts these contacts only while some
    /// loaded script overrides this method.
    /// </summary>
    protected virtual void OnCollisionStay(in Collision collision)
    {
    }

    protected virtual void OnTriggerEnter(Entity other)
    {
    }

    protected virtual void OnTriggerExit(Entity other)
    {
    }

    /// <summary>
    /// Invokes a lifecycle or phase callback under the fault guard (D11).
    /// </summary>
    internal void Run(ScriptCallback callback, float deltaTime)
    {
        if ((DisabledMask & Guard.Bit(callback)) != 0)
        {
            return;
        }
        try
        {
            switch (callback)
            {
                case ScriptCallback.Create:
                    OnCreate();
                    break;
                case ScriptCallback.Start:
                    OnStart();
                    break;
                case ScriptCallback.FixedUpdate:
                    OnFixedUpdate(deltaTime);
                    break;
                case ScriptCallback.Update:
                    OnUpdate(deltaTime);
                    break;
                case ScriptCallback.LateUpdate:
                    OnLateUpdate(deltaTime);
                    break;
                case ScriptCallback.Destroy:
                    OnDestroy();
                    break;
                case ScriptCallback.Reload:
                    OnReload();
                    break;
                case ScriptCallback.Enable:
                    OnEnable();
                    break;
                case ScriptCallback.Disable:
                    OnDisable();
                    break;
                default:
                    return;
            }
            Guard.Succeeded(this, callback);
        }
        catch (Exception exception)
        {
            Guard.Report(this, callback, exception);
        }
    }

    internal void RunCollision(ScriptCallback callback, in Collision collision)
    {
        if ((DisabledMask & Guard.Bit(callback)) != 0)
        {
            return;
        }
        try
        {
            switch (callback)
            {
                case ScriptCallback.CollisionEnter:
                    OnCollisionEnter(in collision);
                    break;
                case ScriptCallback.CollisionExit:
                    OnCollisionExit(in collision);
                    break;
                case ScriptCallback.CollisionStay:
                    OnCollisionStay(in collision);
                    break;
                case ScriptCallback.TriggerEnter:
                    OnTriggerEnter(collision.Other);
                    break;
                case ScriptCallback.TriggerExit:
                    OnTriggerExit(collision.Other);
                    break;
                default:
                    return;
            }
            Guard.Succeeded(this, callback);
        }
        catch (Exception exception)
        {
            Guard.Report(this, callback, exception);
        }
    }
}
