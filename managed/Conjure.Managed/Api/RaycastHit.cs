using System.Numerics;
using System.Runtime.InteropServices;

namespace Conjure;

/// <summary>
/// One hit of a physics query: conjure_raycast_hit with the entity as an Entity (same 40-byte layout, so
/// native fills spans of these directly).
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public readonly struct RaycastHit
{
    public readonly Entity Entity;
    public readonly Vector3 Point;
    public readonly Vector3 Normal;
    public readonly float Distance;
    public readonly int Layer;

    public RaycastHit(Entity entity, Vector3 point, Vector3 normal, float distance, int layer)
    {
        Entity = entity;
        Point = point;
        Normal = normal;
        Distance = distance;
        Layer = layer;
    }

    public override string ToString() => Entity + " at " + Point + " (" + Distance + ")";
}
