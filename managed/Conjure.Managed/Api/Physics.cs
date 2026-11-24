using System;
using System.Numerics;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// What a physics query hits besides the default (conjure_query_flags): without a flag a query skips trigger
/// volumes and includes static bodies.
/// </summary>
[Flags]
public enum QueryFlags : uint
{
    /// <summary>
    /// Triggers skipped, static bodies hit.
    /// </summary>
    None = 0,

    /// <summary>
    /// Trigger volumes are hit too (RaycastHit.IsTrigger tells them apart).
    /// </summary>
    IncludeTriggers = 1,

    /// <summary>
    /// Static bodies are skipped.
    /// </summary>
    ExcludeStatic = 2,
}

/// <summary>
/// Scene queries, gravity and the layer table through the physics_* entries (docs/design/15_bindings.md
/// 2.5). NotSupportedException when the build or the configuration has no physics. Queries return what
/// they hit and never throw for "nothing hit"; a short span truncates and the full count comes back. A query
/// without a mask tests every layer; an explicit mask is used as given, so LayerMask.None hits nothing.
/// Each query has an overload with a required QueryFlags (right after the hit or result parameter) that can
/// include trigger volumes or exclude static bodies; the overloads without it skip trigger volumes and include
/// static bodies (QueryFlags.None). A QueryFlags value outside the defined bits throws ArgumentException.
/// </summary>
public static unsafe class Physics
{
    /// <summary>
    /// The closest hit along a ray; false when nothing is hit within 'maxDistance'. Skips trigger volumes.
    /// </summary>
    public static bool Raycast(
        Vector3 origin,
        Vector3 direction,
        float maxDistance,
        out RaycastHit hit,
        LayerMask? mask = null,
        Entity ignore = default)
    {
        return Raycast(origin, direction, maxDistance, out hit, QueryFlags.None, mask, ignore);
    }

    /// <summary>
    /// The closest hit along a ray, with 'flags' choosing whether trigger volumes and static bodies count; false
    /// when nothing is hit within 'maxDistance'.
    /// </summary>
    public static bool Raycast(
        Vector3 origin,
        Vector3 direction,
        float maxDistance,
        out RaycastHit hit,
        QueryFlags flags,
        LayerMask? mask = null,
        Entity ignore = default)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PhysicsRaycast, NativeFunction.PhysicsRaycast);
        RayQuery query = MakeRay(origin, direction, maxDistance, flags, mask, ignore);
        RaycastHit result = default;
        int status = api->PhysicsRaycast(&query, (Interop.RaycastHit*)&result);
        NativeCheck.ThrowIfFailed(status, NativeFunction.PhysicsRaycast);
        hit = result;
        return status > 0;
    }

    /// <summary>
    /// Every hit along a ray, closest first, up to 'hits.Length'; returns the total number of hits. The
    /// total is exact up to 1024 for a count query (an empty span); a longer span is always filled completely.
    /// Skips trigger volumes.
    /// </summary>
    public static int RaycastAll(
        Vector3 origin,
        Vector3 direction,
        float maxDistance,
        Span<RaycastHit> hits,
        LayerMask? mask = null,
        Entity ignore = default)
    {
        return RaycastAll(origin, direction, maxDistance, hits, QueryFlags.None, mask, ignore);
    }

    /// <summary>
    /// RaycastAll with 'flags' choosing whether trigger volumes and static bodies count.
    /// </summary>
    public static int RaycastAll(
        Vector3 origin,
        Vector3 direction,
        float maxDistance,
        Span<RaycastHit> hits,
        QueryFlags flags,
        LayerMask? mask = null,
        Entity ignore = default)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PhysicsRaycastAll, NativeFunction.PhysicsRaycastAll);
        RayQuery query = MakeRay(origin, direction, maxDistance, flags, mask, ignore);
        fixed (RaycastHit* pinned = hits)
        {
            int status = api->PhysicsRaycastAll(&query, (Interop.RaycastHit*)pinned, hits.Length);
            NativeCheck.ThrowIfFailed(status, NativeFunction.PhysicsRaycastAll);
            return status;
        }
    }

    /// <summary>
    /// Sweeps a sphere along a ray; false when nothing is hit. Skips trigger volumes.
    /// </summary>
    public static bool SphereCast(
        Vector3 origin,
        float radius,
        Vector3 direction,
        float maxDistance,
        out RaycastHit hit,
        LayerMask? mask = null,
        Entity ignore = default)
    {
        return SphereCast(origin, radius, direction, maxDistance, out hit, QueryFlags.None, mask, ignore);
    }

    /// <summary>
    /// SphereCast with 'flags' choosing whether trigger volumes and static bodies count.
    /// </summary>
    public static bool SphereCast(
        Vector3 origin,
        float radius,
        Vector3 direction,
        float maxDistance,
        out RaycastHit hit,
        QueryFlags flags,
        LayerMask? mask = null,
        Entity ignore = default)
    {
        ShapeQuery query = default;
        query.Ray = MakeRay(origin, direction, maxDistance, flags, mask, ignore);
        query.Shape = (byte)CastShape.Sphere;
        query.Radius = radius;
        query.Rotation = Quaternion.Identity;
        return ShapeCast(in query, out hit);
    }

    /// <summary>
    /// Sweeps an oriented box along a ray; false when nothing is hit. Skips trigger volumes.
    /// </summary>
    public static bool BoxCast(
        Vector3 origin,
        Vector3 halfExtents,
        Quaternion rotation,
        Vector3 direction,
        float maxDistance,
        out RaycastHit hit,
        LayerMask? mask = null,
        Entity ignore = default)
    {
        return BoxCast(origin, halfExtents, rotation, direction, maxDistance, out hit, QueryFlags.None, mask, ignore);
    }

    /// <summary>
    /// BoxCast with 'flags' choosing whether trigger volumes and static bodies count.
    /// </summary>
    public static bool BoxCast(
        Vector3 origin,
        Vector3 halfExtents,
        Quaternion rotation,
        Vector3 direction,
        float maxDistance,
        out RaycastHit hit,
        QueryFlags flags,
        LayerMask? mask = null,
        Entity ignore = default)
    {
        ShapeQuery query = default;
        query.Ray = MakeRay(origin, direction, maxDistance, flags, mask, ignore);
        query.Shape = (byte)CastShape.Box;
        query.HalfExtents = halfExtents;
        query.Rotation = rotation;
        return ShapeCast(in query, out hit);
    }

    /// <summary>
    /// Sweeps a Y-axis capsule (total height 2 * (halfHeight + radius)) along a ray; false when nothing is hit.
    /// Skips trigger volumes.
    /// </summary>
    public static bool CapsuleCast(
        Vector3 origin,
        float radius,
        float halfHeight,
        Quaternion rotation,
        Vector3 direction,
        float maxDistance,
        out RaycastHit hit,
        LayerMask? mask = null,
        Entity ignore = default)
    {
        return CapsuleCast(
            origin, radius, halfHeight, rotation, direction, maxDistance, out hit, QueryFlags.None, mask, ignore);
    }

    /// <summary>
    /// CapsuleCast with 'flags' choosing whether trigger volumes and static bodies count.
    /// </summary>
    public static bool CapsuleCast(
        Vector3 origin,
        float radius,
        float halfHeight,
        Quaternion rotation,
        Vector3 direction,
        float maxDistance,
        out RaycastHit hit,
        QueryFlags flags,
        LayerMask? mask = null,
        Entity ignore = default)
    {
        ShapeQuery query = default;
        query.Ray = MakeRay(origin, direction, maxDistance, flags, mask, ignore);
        query.Shape = (byte)CastShape.Capsule;
        query.Radius = radius;
        query.HalfExtents = new Vector3(0.0f, halfHeight, 0.0f);
        query.Rotation = rotation;
        return ShapeCast(in query, out hit);
    }

    /// <summary>
    /// The bodies overlapping a sphere, up to 'entities.Length'; returns the total count (exact up to 1024 for
    /// a count query with an empty span; a longer span is always filled completely). Skips trigger volumes.
    /// </summary>
    public static int OverlapSphere(Vector3 center, float radius, Span<Entity> entities, LayerMask? mask = null)
    {
        return OverlapSphere(center, radius, entities, QueryFlags.None, mask, default);
    }

    /// <summary>
    /// OverlapSphere with 'flags' choosing whether trigger volumes and static bodies count, and an entity left
    /// out of the result ('ignore', typically the caller's own).
    /// </summary>
    public static int OverlapSphere(
        Vector3 center,
        float radius,
        Span<Entity> entities,
        QueryFlags flags,
        LayerMask? mask = null,
        Entity ignore = default)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PhysicsOverlapSphere, NativeFunction.PhysicsOverlapSphere);
        uint flagBits = FlagBits(flags);
        fixed (Entity* pinned = entities)
        {
            int status = api->PhysicsOverlapSphere(
                &center, radius, MaskBits(mask), flagBits, ignore.Id, (ulong*)pinned, entities.Length);
            NativeCheck.ThrowIfFailed(status, NativeFunction.PhysicsOverlapSphere);
            return status;
        }
    }

    /// <summary>
    /// The bodies overlapping an oriented box, up to 'entities.Length'; returns the total count (exact up to
    /// 1024 for a count query with an empty span; a longer span is always filled completely). Skips trigger
    /// volumes.
    /// </summary>
    public static int OverlapBox(
        Vector3 center,
        Vector3 halfExtents,
        Quaternion rotation,
        Span<Entity> entities,
        LayerMask? mask = null)
    {
        return OverlapBox(center, halfExtents, rotation, entities, QueryFlags.None, mask, default);
    }

    /// <summary>
    /// OverlapBox with 'flags' choosing whether trigger volumes and static bodies count, and an entity left out
    /// of the result ('ignore', typically the caller's own).
    /// </summary>
    public static int OverlapBox(
        Vector3 center,
        Vector3 halfExtents,
        Quaternion rotation,
        Span<Entity> entities,
        QueryFlags flags,
        LayerMask? mask = null,
        Entity ignore = default)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PhysicsOverlapBox, NativeFunction.PhysicsOverlapBox);
        uint flagBits = FlagBits(flags);
        fixed (Entity* pinned = entities)
        {
            int status = api->PhysicsOverlapBox(
                &center, &halfExtents, &rotation, MaskBits(mask), flagBits, ignore.Id, (ulong*)pinned,
                entities.Length);
            NativeCheck.ThrowIfFailed(status, NativeFunction.PhysicsOverlapBox);
            return status;
        }
    }

    /// <summary>
    /// The world gravity (m/s^2); setting it takes effect at the next fixed step.
    /// </summary>
    public static Vector3 Gravity
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->PhysicsGetGravity, NativeFunction.PhysicsGetGravity);
            Vector3 value = Vector3.Zero;
            NativeCheck.ThrowIfFailed(api->PhysicsGetGravity(&value), NativeFunction.PhysicsGetGravity);
            return value;
        }
        set
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->PhysicsSetGravity, NativeFunction.PhysicsSetGravity);
            NativeCheck.ThrowIfFailed(api->PhysicsSetGravity(&value), NativeFunction.PhysicsSetGravity);
        }
    }

    /// <summary>
    /// The mask of the named layer; KeyNotFoundException for a name the layer table lacks.
    /// </summary>
    public static LayerMask Layer(string name) => LayerMask.FromIndex(LayerIndex(name));

    /// <summary>
    /// The index of the named layer; KeyNotFoundException for a name the layer table lacks.
    /// </summary>
    public static int LayerIndex(string name)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PhysicsLayerIndex, NativeFunction.PhysicsLayerIndex);
        Span<byte> scratch = stackalloc byte[Utf8Marshal.ScratchSize];
        Utf8Encoded encoded = Utf8Marshal.Encode(name, scratch);
        fixed (byte* pinned = encoded.Bytes)
        {
            int status = api->PhysicsLayerIndex(pinned, encoded.Length);
            NativeCheck.ThrowIfFailed(status, NativeFunction.PhysicsLayerIndex);
            return status;
        }
    }

    /// <summary>
    /// The name of a layer index; KeyNotFoundException beyond the table.
    /// </summary>
    public static string LayerName(int layer)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PhysicsLayerName, NativeFunction.PhysicsLayerName);
        string name = Utf8Marshal.ReadNeeded(layer, static (int state, byte* buffer, int capacity) =>
        {
            return NativeBridge.Api->PhysicsLayerName(state, buffer, capacity);
        }, out int status);
        NativeCheck.ThrowIfFailed(status, NativeFunction.PhysicsLayerName);
        return name;
    }

    /// <summary>
    /// True when bodies on the two layers collide.
    /// </summary>
    public static bool LayersCollide(int layerA, int layerB)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PhysicsLayersCollide, NativeFunction.PhysicsLayersCollide);
        int status = api->PhysicsLayersCollide(layerA, layerB);
        NativeCheck.ThrowIfFailed(status, NativeFunction.PhysicsLayersCollide);
        return status > 0;
    }

    private static bool ShapeCast(in ShapeQuery query, out RaycastHit hit)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PhysicsShapeCast, NativeFunction.PhysicsShapeCast);
        RaycastHit result = default;
        int status;
        fixed (ShapeQuery* pinned = &query)
        {
            status = api->PhysicsShapeCast(pinned, (Interop.RaycastHit*)&result);
        }
        NativeCheck.ThrowIfFailed(status, NativeFunction.PhysicsShapeCast);
        hit = result;
        return status > 0;
    }

    private static RayQuery MakeRay(
        Vector3 origin,
        Vector3 direction,
        float maxDistance,
        QueryFlags flags,
        LayerMask? mask,
        Entity ignore)
    {
        RayQuery query = default;
        query.Origin = origin;
        query.Direction = direction;
        query.MaxDistance = maxDistance;
        query.LayerMask = MaskBits(mask);
        query.IgnoreEntity = ignore.Id;
        query.Flags = FlagBits(flags);
        return query;
    }

    // Native refuses an undefined bit with INVALID_ARGUMENT "flags"; checking here first names the value.
    private static uint FlagBits(QueryFlags flags)
    {
        const QueryFlags known = QueryFlags.IncludeTriggers | QueryFlags.ExcludeStatic;
        if ((flags & ~known) != 0)
        {
            throw new ArgumentException("QueryFlags value " + (uint)flags + " holds undefined bits", nameof(flags));
        }
        return (uint)flags;
    }

    // No mask (null, the default argument) means every layer; an explicit mask is passed through, so
    // LayerMask.None matches nothing.
    private static uint MaskBits(LayerMask? mask) => mask.HasValue ? mask.Value.Bits : LayerMask.All.Bits;
}
