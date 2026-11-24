using System.Numerics;
using System.Runtime.InteropServices;

namespace Conjure;

/// <summary>
/// One hit of a physics query: conjure_raycast_hit with the entity as an Entity (same 48-byte layout, so
/// native fills spans of these directly). IsTrigger is true for a trigger volume, which a query hits only with
/// QueryFlags.IncludeTriggers.
/// </summary>
[StructLayout(LayoutKind.Sequential, Size = 48)]
public readonly struct RaycastHit
{
    public readonly Entity Entity;
    public readonly Vector3 Point;
    public readonly Vector3 Normal;
    public readonly float Distance;
    public readonly int Layer;

    // conjure_raycast_hit::is_trigger; private so the public fields keep their order and the struct grows only in
    // its tail (code compiled against the 40-byte struct keeps working).
    private readonly byte _isTrigger;

    public RaycastHit(Entity entity, Vector3 point, Vector3 normal, float distance, int layer)
    {
        Entity = entity;
        Point = point;
        Normal = normal;
        Distance = distance;
        Layer = layer;
        _isTrigger = 0;
    }

    /// <summary>
    /// True when the body hit is a trigger volume.
    /// </summary>
    public bool IsTrigger => _isTrigger != 0;

    public override string ToString() => Entity + " at " + Point + " (" + Distance + ")";
}
