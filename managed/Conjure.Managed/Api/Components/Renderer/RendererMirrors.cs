using System.Numerics;
using System.Runtime.InteropServices;

namespace Conjure.Components;

// The renderer component mirrors (docs/design/15_bindings.md 2.6, 02_renderer.md 4.2). Each struct restates the
// native one byte for byte: the [NativeComponent] mirrors are verified by their layout hash at first use, the
// nested [NativeStruct] ones by the [dotnet] hash test only. Units: metres, radians; light intensity is lux for a
// directional light and candela for point and spot lights. The engine registers the native types even without
// rendering; in a build without the renderer they resolve to id 0.

/// <summary>
/// renderer::sprite_space: where a sprite is drawn.
/// </summary>
public enum SpriteSpace : byte
{
    /// <summary>
    /// A world-space quad, drawn into the HDR target after the blended meshes.
    /// </summary>
    World,

    /// <summary>
    /// A pixel-space quad (origin top-left, y down), drawn after tonemapping.
    /// </summary>
    Screen,
}

/// <summary>
/// renderer::projection_type.
/// </summary>
public enum ProjectionType : byte
{
    Perspective,
    Orthographic,
}

/// <summary>
/// renderer::light_type.
/// </summary>
public enum LightType : byte
{
    Directional,
    Point,
    Spot,
}

/// <summary>
/// renderer::shadow_filter, the per-light shadow filter. Default resolves to the project's default filter at
/// render time. The one definition of the enum in Conjure.Managed: every mirror holding a filter uses it.
/// </summary>
public enum ShadowFilter : byte
{
    Default,
    Hard,
    Pcf,
    Pcss,
}

/// <summary>
/// Mirror of renderer.sprite (72 bytes): a textured quad of Size metres (World) or pixels (Screen), tinted by
/// Colour (linear RGBA) over the UvRect (min u, min v, max u, max v in texture space; the default 0, 0, 1, 1 is the
/// whole texture). Layer, then Order, sort sprites.
/// </summary>
[NativeComponent("conjure.renderer.sprite")]
[StructLayout(LayoutKind.Sequential)]
public struct Sprite : IComponent
{
    public AssetRef<TextureAsset> Texture;
    public Vector4 UvRect;
    public Vector4 Colour;
    public Vector2 Size;
    public SpriteSpace Space;
    public short Layer;
    public short Order;

    /// <summary>
    /// The native defaults: no texture, the whole texture, white, 1 x 1, World.
    /// </summary>
    public static Sprite Default => new Sprite
    {
        UvRect = new Vector4(0.0f, 0.0f, 1.0f, 1.0f),
        Colour = Vector4.One,
        Size = Vector2.One,
        Space = SpriteSpace.World,
    };
}

/// <summary>
/// Mirror of the nested renderer::camera (conjure.renderer.camera_data, 20 bytes): the projection. The vertical
/// field of view (radians) applies to a perspective camera, OrthographicHeight (metres visible vertically) to an
/// orthographic one; the planes are metres along the forward axis.
/// </summary>
[NativeStruct("conjure.renderer.camera_data")]
[StructLayout(LayoutKind.Sequential)]
public struct CameraData
{
    public ProjectionType Projection;
    public float VerticalFovRadians;
    public float OrthographicHeight;
    public float NearPlane;
    public float FarPlane;

    /// <summary>
    /// The native defaults: perspective, 1.0472 rad (60 degrees), 10 m, near 0.1 m, far 1000 m.
    /// </summary>
    public static CameraData Default => new CameraData
    {
        Projection = ProjectionType.Perspective,
        VerticalFovRadians = 1.0472f,
        OrthographicHeight = 10.0f,
        NearPlane = 0.1f,
        FarPlane = 1000.0f,
    };
}

/// <summary>
/// Mirror of renderer.camera (32 bytes): the camera component. LayerMask selects the mesh layers it draws.
/// Conjure.Camera.Main picks, among the cameras that are enabled, not under a disabled ancestor and have a render
/// world transform: IsMain cameras before the others, then the highest Priority, then the first in world order.
/// PostProcess false renders the view without post-processing. The Conjure.Camera facade owns the short name.
/// </summary>
[NativeComponent("conjure.renderer.camera")]
[StructLayout(LayoutKind.Sequential)]
public struct CameraSettings : IComponent
{
    public CameraData CameraData;
    public uint LayerMask;
    public int Priority;
    public NativeBool IsMain;
    public NativeBool PostProcess;

    /// <summary>
    /// The native defaults: CameraData.Default, every layer, priority 0, main, post-processing on.
    /// </summary>
    public static CameraSettings Default => new CameraSettings
    {
        CameraData = CameraData.Default,
        LayerMask = 0xFFFFFFFFu,
        Priority = 0,
        IsMain = true,
        PostProcess = true,
    };
}

/// <summary>
/// Mirror of the nested renderer::light (conjure.renderer.light_data, 68 bytes). Position is ignored: the
/// extraction places the light at its entity's world position. Direction is in the entity's local space; the
/// extraction rotates it by the entity's world transform. Intensity is lux for Directional and candela for Point
/// and Spot; Range is metres; the cone angles are radians; SourceRadius (metres, an angular radius for
/// Directional) widens PCSS penumbrae; ShadowResolution requests an atlas tile of 256, 512 or 1024 texels. Pad0 is
/// the native runtime entityId: unreflected, set only in the extraction's copy and never saved; leave it 0.
/// </summary>
[NativeStruct("conjure.renderer.light_data")]
[StructLayout(LayoutKind.Sequential)]
public struct LightData
{
    public LightType Type;
    public Vector3 Position;
    public Vector3 Direction;
    public Vector3 Colour;
    public float Intensity;
    public float Range;
    public float InnerConeRadians;
    public float OuterConeRadians;
    public float SourceRadius;
    public NativeBool CastsShadows;
    public ShadowFilter Filter;
    public ushort ShadowResolution;
    public uint Pad0;

    /// <summary>
    /// The native defaults: a white 10 cd point light pointing down, range 10 m, cones 0.4 / 0.6 rad, source
    /// 0.05 m, no shadows, the default filter, a 512 tile.
    /// </summary>
    public static LightData Default => new LightData
    {
        Type = LightType.Point,
        Direction = new Vector3(0.0f, -1.0f, 0.0f),
        Colour = Vector3.One,
        Intensity = 10.0f,
        Range = 10.0f,
        InnerConeRadians = 0.4f,
        OuterConeRadians = 0.6f,
        SourceRadius = 0.05f,
        CastsShadows = false,
        Filter = ShadowFilter.Default,
        ShadowResolution = 512,
    };
}

/// <summary>
/// Mirror of renderer.light (68 bytes): the light component.
/// </summary>
[NativeComponent("conjure.renderer.light")]
[StructLayout(LayoutKind.Sequential)]
public struct Light : IComponent
{
    public LightData LightData;

    /// <summary>
    /// The native defaults (LightData.Default).
    /// </summary>
    public static Light Default => new Light { LightData = LightData.Default };
}
