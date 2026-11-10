using System.Numerics;
using Conjure;

namespace Conjure.TestScripts;

/// <summary>
/// The 10k-instance workload: a serialised private field, a public vector and a private counter that
/// hot reload resets.
/// </summary>
public sealed class Mover : ScriptBehaviour
{
    [SerializeField]
    private float _speed = 1.5f;

    public Vector3 Velocity = new Vector3(1.0f, 0.0f, 0.0f);

    public int Updates;

    public int PrivateCalls;

    private int _privateCounter;

    protected override void OnUpdate(float deltaTime)
    {
        ++Updates;
        ++_privateCounter;
        PrivateCalls = _privateCounter;
        Velocity += new Vector3(_speed * deltaTime, 0.0f, 0.0f);
    }
}
