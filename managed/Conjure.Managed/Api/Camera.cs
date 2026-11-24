using System;
using System.Numerics;
using Conjure.Components;
using Conjure.Dod;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// The facade of an entity's camera (CameraSettings) and screen picking through the camera_* entries. Screen
/// positions are game screen pixels (Screen, Input.MouseScreenPosition): origin top-left, y down. Depth is metres
/// along the camera's forward axis. Every call uses the camera's current pose, composed from the local transforms
/// at the call, so turning the camera and picking in the same Update gives this frame's ray; a camera driven by
/// physics interpolation is drawn up to one fixed step behind that pose. A camera entity without a
/// LocalTransform uses its render world transform.
///
/// Failures: EntityNotAliveException for a dead entity (Main when there is no camera);
/// InvalidOperationException when the entity has no CameraSettings, has no transform at all, or (ScreenToWorld,
/// ScreenToWorld2D) when the screen has zero area; ArgumentException for non-finite input or camera data the
/// projection refuses (a near plane at or below 0, a far plane at or before it, a field of view outside 0..pi, an
/// orthographic height at or below 0). In a build without the renderer Main is a camera that does not exist
/// (Exists false), Settings throws InvalidOperationException (CameraSettings is not registered) and the picking
/// methods throw NotSupportedException.
/// </summary>
public readonly struct Camera : IEquatable<Camera>
{
    /// <summary>
    /// The camera facade of 'entity' (nothing is checked until a call).
    /// </summary>
    public Camera(Entity entity)
    {
        Entity = entity;
    }

    /// <summary>
    /// The camera entity.
    /// </summary>
    public Entity Entity { get; }

    /// <summary>
    /// The camera the game view shows. Among the cameras that are enabled, not under a disabled ancestor and have
    /// a render world transform: IsMain cameras before the others, then the highest Priority, then the first in
    /// world order. Main.Exists is false when there is none, and its picking methods then throw
    /// EntityNotAliveException.
    /// </summary>
    public static unsafe Camera Main
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->CameraMain, NativeFunction.CameraMain);
            return new Camera(new Entity(api->CameraMain()));
        }
    }

    /// <summary>
    /// True when the entity is alive and carries CameraSettings (false in a build without the renderer).
    /// </summary>
    public bool Exists =>
        ComponentType<CameraSettings>.Id != 0 && Entity.IsAlive && Entity.HasComponent<CameraSettings>();

    /// <summary>
    /// The camera component. EntityNotAliveException for a dead entity, InvalidOperationException when it has
    /// none.
    /// </summary>
    public CameraSettings Settings
    {
        get => Entity.GetComponent<CameraSettings>();
        set => Entity.SetComponent(in value);
    }

    /// <summary>
    /// The world ray through a screen pixel: it starts on the near plane and, for a perspective camera, passes
    /// through the eye; an orthographic ray runs along the forward axis. False, with the ray from the camera
    /// position along its forward axis, when the screen has zero area.
    /// </summary>
    public unsafe bool ScreenPointToRay(Vector2 screenPixels, out Ray ray)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->CameraScreenRay, NativeFunction.CameraScreenRay);
        Vector3 origin = default;
        Vector3 direction = default;
        int status = api->CameraScreenRay(Entity.Id, &screenPixels, &origin, &direction);
        ThrowIfFailed(status, NativeFunction.CameraScreenRay);
        ray = new Ray(origin, direction);
        return status > 0;
    }

    /// <summary>
    /// The world point under a screen pixel at 'depth' metres along the forward axis. InvalidOperationException
    /// when the screen has zero area.
    /// </summary>
    public unsafe Vector3 ScreenToWorld(Vector2 screenPixels, float depth)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->CameraScreenToWorld, NativeFunction.CameraScreenToWorld);
        Vector3 input = new Vector3(screenPixels, depth);
        Vector3 world = default;
        int status = api->CameraScreenToWorld(Entity.Id, &input, &world);
        ThrowIfFailed(status, NativeFunction.CameraScreenToWorld);
        if (status == 0)
        {
            throw new InvalidOperationException("ScreenToWorld: the screen has zero area (minimised or no window)");
        }
        return world;
    }

    /// <summary>
    /// Where the ray through a screen pixel meets 'plane' (Normal . p + D = 0) ahead of the ray origin. False, with
    /// 'point' zero, when the screen has zero area or the ray is parallel to the plane or points away from it.
    /// </summary>
    public bool ScreenToPlane(Vector2 screenPixels, Plane plane, out Vector3 point)
    {
        point = Vector3.Zero;
        if (!ScreenPointToRay(screenPixels, out Ray ray))
        {
            return false;
        }
        float facing = Vector3.Dot(plane.Normal, ray.Direction);
        if (MathF.Abs(facing) <= ParallelEpsilon)
        {
            return false;
        }
        float distance = -(Vector3.Dot(plane.Normal, ray.Origin) + plane.D) / facing;
        if (!float.IsFinite(distance) || distance < 0.0f)
        {
            return false;
        }
        point = ray.GetPoint(distance);
        return true;
    }

    /// <summary>
    /// The x and y of the point where the ray through a screen pixel meets the z = 0 plane: the picking of a 2D game
    /// (a grid cell under the mouse is Camera.Main.ScreenToWorld2D(Input.MouseScreenPosition)).
    /// InvalidOperationException when ScreenToPlane finds no point (a zero screen, or a camera that does not look
    /// at the plane through that pixel).
    /// </summary>
    public Vector2 ScreenToWorld2D(Vector2 screenPixels)
    {
        if (!ScreenToPlane(screenPixels, new Plane(Vector3.UnitZ, 0.0f), out Vector3 point))
        {
            throw new InvalidOperationException(
                "ScreenToWorld2D: the ray through " + screenPixels + " does not meet the z = 0 plane ahead of the " +
                "camera, or the screen has zero area");
        }
        return new Vector2(point.X, point.Y);
    }

    /// <summary>
    /// The screen pixel (x, y) of a world point and its depth (z, metres along the forward axis). True when the
    /// point is in front of the camera (depth &gt; 0), even outside the screen; false when it is behind or on the
    /// camera plane (the pixel of a perspective camera is then mirrored, or the screen centre on the plane, which
    /// for a perspective camera is any depth within 1e-6 m of 0) or when the screen has zero area (pixel 0, 0).
    /// </summary>
    public unsafe bool WorldToScreen(Vector3 world, out Vector3 screenPixelsAndDepth)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->CameraWorldToScreen, NativeFunction.CameraWorldToScreen);
        Vector3 result = default;
        int status = api->CameraWorldToScreen(Entity.Id, &world, &result);
        ThrowIfFailed(status, NativeFunction.CameraWorldToScreen);
        screenPixelsAndDepth = result;
        return status > 0;
    }

    public bool Equals(Camera other) => Entity == other.Entity;

    public override bool Equals(object? obj) => obj is Camera other && Equals(other);

    public override int GetHashCode() => Entity.GetHashCode();

    public override string ToString() => "Camera(" + Entity + ")";

    public static bool operator ==(Camera left, Camera right) => left.Equals(right);

    public static bool operator !=(Camera left, Camera right) => !left.Equals(right);

    // Below this |cos| between the ray and the plane normal the ray is treated as parallel to the plane.
    private const float ParallelEpsilon = 1.0e-6f;

    // A missing camera or transform is a wrong use of this entity, as GetComponent reports a missing component.
    private static void ThrowIfFailed(int status, NativeFunction function)
    {
        if (NativeCheck.Is(status, BindingStatus.ComponentMissing))
        {
            throw new InvalidOperationException(NativeCheck.ReadLastError(out _));
        }
        NativeCheck.ThrowIfFailed(status, function);
    }
}
