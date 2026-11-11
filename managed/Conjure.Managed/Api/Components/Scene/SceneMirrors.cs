using System.Numerics;
using System.Runtime.InteropServices;
using Conjure.Interop;

namespace Conjure.Components;

// The scene component mirrors (docs/design/15_bindings.md 2.6, 13_scene.md 2.1, R55). Field order, kinds and
// offsets restate the native structs byte for byte; ComponentType<T> verifies the layout hash at first use.
// LocalTransform stays in Api/Components/LocalTransform.cs (scripting's).

/// <summary>
/// Mirror of scene.world_transform: the propagated world matrix (read-only by convention; 64 bytes).
/// </summary>
[NativeComponent("conjure.scene.world_transform")]
[StructLayout(LayoutKind.Sequential)]
public struct WorldTransform : IComponent
{
    public Matrix4x4 Matrix;
}

/// <summary>
/// Mirror of scene.interpolated_transform: the poses of the two most recent fixed steps (80 bytes).
/// </summary>
[NativeComponent("conjure.scene.interpolated_transform")]
[StructLayout(LayoutKind.Sequential)]
public struct InterpolatedTransform : IComponent
{
    public TransformData Previous;
    public TransformData Current;
}

/// <summary>
/// Mirror of scene.parent: the parent entity and the sibling index (12 bytes; roots carry none).
/// </summary>
[NativeComponent("conjure.scene.parent")]
[StructLayout(LayoutKind.Sequential)]
public struct Parent : IComponent
{
    public EntityHandle Value;
    public uint SiblingIndex;
}

/// <summary>
/// Mirror of scene.name: the display name as a string id (8 bytes).
/// </summary>
[NativeComponent("conjure.scene.name")]
[StructLayout(LayoutKind.Sequential)]
public struct Name : IComponent
{
    public StringId Value;
}

/// <summary>
/// Mirror of scene.tag: the gameplay tag (8 bytes).
/// </summary>
[NativeComponent("conjure.scene.tag")]
[StructLayout(LayoutKind.Sequential)]
public struct Tag : IComponent
{
    public StringId Value;
}

/// <summary>
/// Mirror of scene.layer: the layer index 0..31 (1 byte).
/// </summary>
[NativeComponent("conjure.scene.layer")]
[StructLayout(LayoutKind.Sequential)]
public struct Layer : IComponent
{
    public byte Index;
}

/// <summary>
/// Mirror of scene.guid: the editor identity (16 bytes).
/// </summary>
[NativeComponent("conjure.scene.guid")]
[StructLayout(LayoutKind.Sequential)]
public struct EntityGuid : IComponent
{
    public AssetId Value;
}

/// <summary>
/// Mirror of the scene.disabled tag: the entity is switched off.
/// </summary>
[NativeComponent("conjure.scene.disabled")]
[StructLayout(LayoutKind.Sequential)]
public struct Disabled : IComponent
{
}

/// <summary>
/// Mirror of the scene.transform_dirty tag: set by structural changes, cleared by propagation.
/// </summary>
[NativeComponent("conjure.scene.transform_dirty")]
[StructLayout(LayoutKind.Sequential)]
public struct TransformDirty : IComponent
{
}

/// <summary>
/// Mirror of scene.prefab_instance: on an instance root, the prefab it came from (24 bytes).
/// </summary>
[NativeComponent("conjure.scene.prefab_instance")]
[StructLayout(LayoutKind.Sequential)]
public struct PrefabInstance : IComponent
{
    public AssetRef<PrefabAsset> Prefab;
}
