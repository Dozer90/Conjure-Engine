using System.Numerics;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// The transform facade of an entity (local position, rotation and scale through the bindings'
/// transform_* entries; NotSupportedException until the bindings fill them).
/// </summary>
public readonly struct Transform
{
    public readonly Entity Entity;

    public Transform(Entity entity)
    {
        Entity = entity;
    }

    public unsafe TransformData Local
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->TransformGetLocal, NativeFunction.TransformGetLocal);
            TransformData value = TransformData.Identity;
            api->TransformGetLocal(Entity.Id, &value);
            return value;
        }
        set
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->TransformSetLocal, NativeFunction.TransformSetLocal);
            api->TransformSetLocal(Entity.Id, &value);
        }
    }

    public Vector3 Position
    {
        get => Local.Position;
        set
        {
            TransformData local = Local;
            local.Position = value;
            Local = local;
        }
    }

    public Quaternion Rotation
    {
        get => Local.Rotation;
        set
        {
            TransformData local = Local;
            local.Rotation = value;
            Local = local;
        }
    }

    public Vector3 Scale
    {
        get => Local.Scale;
        set
        {
            TransformData local = Local;
            local.Scale = value;
            Local = local;
        }
    }

    public unsafe Matrix4x4 WorldMatrix
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->TransformGetWorldMatrix, NativeFunction.TransformGetWorldMatrix);
            Matrix4x4 matrix = Matrix4x4.Identity;
            api->TransformGetWorldMatrix(Entity.Id, &matrix);
            return matrix;
        }
    }

    /// <summary>
    /// Reparents the entity keeping its world pose (Null detaches).
    /// </summary>
    public unsafe bool SetParent(Entity parent)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->TransformSetParent, NativeFunction.TransformSetParent);
        return api->TransformSetParent(Entity.Id, parent.Id) >= 0;
    }
}
