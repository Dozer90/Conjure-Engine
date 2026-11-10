using System.Numerics;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// Physics queries and body control through the bindings' physics_* entries (NotSupportedException
/// until the bindings fill them, D11).
/// </summary>
public static unsafe class Physics
{
    public static bool Raycast(in RayQuery query, out RaycastHit hit)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PhysicsRaycast, NativeFunction.PhysicsRaycast);
        RaycastHit result = default;
        fixed (RayQuery* pinned = &query)
        {
            bool found = api->PhysicsRaycast(pinned, &result) > 0;
            hit = result;
            return found;
        }
    }

    public static Vector3 Gravity
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->PhysicsGetGravity, NativeFunction.PhysicsGetGravity);
            Vector3 value = Vector3.Zero;
            api->PhysicsGetGravity(&value);
            return value;
        }
        set
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->PhysicsSetGravity, NativeFunction.PhysicsSetGravity);
            api->PhysicsSetGravity(&value);
        }
    }

    public static Vector3 GetLinearVelocity(Entity entity)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PhysicsGetLinearVelocity, NativeFunction.PhysicsGetLinearVelocity);
        Vector3 value = Vector3.Zero;
        api->PhysicsGetLinearVelocity(entity.Id, &value);
        return value;
    }

    public static bool SetLinearVelocity(Entity entity, Vector3 value)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PhysicsSetLinearVelocity, NativeFunction.PhysicsSetLinearVelocity);
        return api->PhysicsSetLinearVelocity(entity.Id, &value) >= 0;
    }

    public static bool AddForce(Entity entity, Vector3 force, ForceMode mode = ForceMode.Force)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PhysicsAddForce, NativeFunction.PhysicsAddForce);
        return api->PhysicsAddForce(entity.Id, &force, (byte)mode) >= 0;
    }
}
