using System;
using System.Runtime.CompilerServices;
using Conjure.Interop;
using Conjure.Runtime;

namespace Conjure;

/// <summary>
/// Per-callback consecutive fault counters (one byte per ScriptCallback).
/// </summary>
[InlineArray(10)]
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
    /// Callbacks disabled after three consecutive faults (bits of ScriptCallback).
    /// </summary>
    public ushort DisabledCallbacks => DisabledMask;

    public T GetComponent<T>() where T : unmanaged => EntityValue.GetComponent<T>();

    public bool HasComponent<T>() where T : unmanaged => EntityValue.HasComponent<T>();

    public void AddComponent<T>(in T value) where T : unmanaged => EntityValue.AddComponent(in value);

    public void SetComponent<T>(in T value) where T : unmanaged => EntityValue.SetComponent(in value);

    public bool RemoveComponent<T>() where T : unmanaged => EntityValue.RemoveComponent<T>();

    /// <summary>
    /// Another script on the same entity, or null.
    /// </summary>
    public T? GetScript<T>() where T : ScriptBehaviour => EntityValue.GetScript<T>();

    /// <summary>
    /// Destroys the entity (OnDestroy runs with the entity still alive, at the end of the phase).
    /// </summary>
    public void Destroy() => EntityValue.Destroy();

    protected virtual void OnCreate()
    {
    }

    protected virtual void OnStart()
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

    protected virtual void OnDestroy()
    {
    }

    protected virtual void OnCollisionEnter(in Collision collision)
    {
    }

    protected virtual void OnCollisionExit(in Collision collision)
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
