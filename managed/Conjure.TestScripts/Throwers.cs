using System;
using Conjure;

namespace Conjure.TestScripts;

/// <summary>
/// Throws from OnUpdate on the frames whose bit is set in Pattern (bit i = update number i); Updates
/// counts the attempts, so a disabled callback stops the count.
/// </summary>
public sealed class Thrower : ScriptBehaviour
{
    public int Pattern = 0b111;
    public int Updates;

    protected override void OnUpdate(float deltaTime)
    {
        int frame = Updates;
        ++Updates;
        if (frame < 31 && (Pattern & (1 << frame)) != 0)
        {
            throw new InvalidOperationException("Thrower faults on update " + frame);
        }
    }
}

/// <summary>
/// Throws from OnCreate: the instance is kept but every callback is disabled (D11).
/// </summary>
public sealed class CreateThrower : ScriptBehaviour
{
    public int Updates;
    public int Destroyed;

    protected override void OnCreate()
    {
        throw new InvalidOperationException("CreateThrower faults in OnCreate");
    }

    protected override void OnUpdate(float deltaTime)
    {
        ++Updates;
    }

    protected override void OnDestroy()
    {
        ++Destroyed;
    }
}

/// <summary>
/// Throws from OnReload: the instance a hot reload recreated is kept but every callback is disabled, as after an
/// OnCreate fault.
/// </summary>
public sealed class ReloadThrower : ScriptBehaviour
{
    public int Updates;

    protected override void OnReload()
    {
        throw new InvalidOperationException("ReloadThrower faults in OnReload");
    }

    protected override void OnUpdate(float deltaTime)
    {
        ++Updates;
    }

    protected override void OnDestroy()
    {
        Log.Info("ReloadThrower.OnDestroy ran");
    }
}

/// <summary>
/// Throws from OnCreate in variant v1 only (code sweep OWN-RELOAD): an OnCreate that threw did not complete, so a
/// hot reload to the fixed v2 runs OnCreate once more and then OnStart, and the reload after that runs OnReload.
/// </summary>
public sealed class FixedCreateThrower : ScriptBehaviour
{
    public int CreateCalls;
    public int StartCalls;
    public int ReloadCalls;
    public int Updates;

    protected override void OnCreate()
    {
        ++CreateCalls;
#if V1
        throw new InvalidOperationException("FixedCreateThrower faults in OnCreate until v2 fixes it");
#endif
    }

    protected override void OnStart()
    {
        ++StartCalls;
    }

    protected override void OnReload()
    {
        ++ReloadCalls;
    }

    protected override void OnUpdate(float deltaTime)
    {
        ++Updates;
    }
}
