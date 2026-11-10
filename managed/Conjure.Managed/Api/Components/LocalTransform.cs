using System.Numerics;
using System.Runtime.InteropServices;

namespace Conjure.Components;

/// <summary>
/// Mirror of scene::local_transform (13_scene.md 2.1): position, rotation (x, y, z, w), scale; 40 bytes.
/// The bindings add the other mirrors (15_bindings.md 2.6).
/// </summary>
[NativeComponent("conjure.scene.local_transform")]
[StructLayout(LayoutKind.Sequential)]
public struct LocalTransform : IComponent
{
    public Vector3 Position;
    public Quaternion Rotation;
    public Vector3 Scale;

    public static LocalTransform Identity => new LocalTransform
    {
        Position = Vector3.Zero,
        Rotation = Quaternion.Identity,
        Scale = Vector3.One,
    };
}
