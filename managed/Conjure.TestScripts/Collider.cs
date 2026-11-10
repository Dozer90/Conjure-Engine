using Conjure;

namespace Conjure.TestScripts;

/// <summary>
/// Records collision and trigger callbacks.
/// </summary>
public sealed class Collider : ScriptBehaviour
{
    public int Enters;
    public int Exits;
    public int TriggerEnters;
    public int TriggerExits;
    public Entity LastOther;
    public float LastImpulse;
    public float LastNormalX;

    protected override void OnCollisionEnter(in Collision collision)
    {
        ++Enters;
        LastOther = collision.Other;
        LastImpulse = collision.Impulse;
        LastNormalX = collision.Normal.X;
    }

    protected override void OnCollisionExit(in Collision collision)
    {
        ++Exits;
        LastOther = collision.Other;
    }

    protected override void OnTriggerEnter(Entity other)
    {
        ++TriggerEnters;
        LastOther = other;
    }

    protected override void OnTriggerExit(Entity other)
    {
        ++TriggerExits;
        LastOther = other;
    }
}

/// <summary>
/// Overrides only the enter callback: exit records must not reach it.
/// </summary>
public sealed class EnterOnlyCollider : ScriptBehaviour
{
    public int Enters;

    protected override void OnCollisionEnter(in Collision collision)
    {
        ++Enters;
    }
}
