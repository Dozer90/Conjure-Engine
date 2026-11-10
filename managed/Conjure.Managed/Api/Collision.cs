using System.Numerics;

namespace Conjure;

/// <summary>
/// One contact as delivered to OnCollisionEnter / OnCollisionExit: the other entity, the contact point,
/// the normal pointing away from this entity and the impulse.
/// </summary>
public readonly struct Collision
{
    public Collision(Entity other, Vector3 point, Vector3 normal, float impulse)
    {
        Other = other;
        Point = point;
        Normal = normal;
        Impulse = impulse;
    }

    public Entity Other { get; }

    public Vector3 Point { get; }

    public Vector3 Normal { get; }

    public float Impulse { get; }
}
