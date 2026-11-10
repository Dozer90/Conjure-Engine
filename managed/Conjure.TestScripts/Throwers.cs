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
