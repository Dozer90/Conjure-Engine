using System;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// Tag of an asset type for AssetRef&lt;T&gt; (docs/design/15_bindings.md 2.5): an empty struct whose TypeName
/// is the registered asset type name (assets::asset_type_traits). A type parameter that does not implement
/// it (a plain class) makes the reference untyped: native skips the type check.
/// </summary>
public interface IAssetType
{
    string TypeName { get; }
}

public readonly struct TextureAsset : IAssetType
{
    public string TypeName => "conjure.renderer.texture";
}

public readonly struct MeshAsset : IAssetType
{
    public string TypeName => "conjure.renderer.mesh";
}

public readonly struct MaterialAsset : IAssetType
{
    public string TypeName => "conjure.renderer.material";
}

public readonly struct ModelAsset : IAssetType
{
    public string TypeName => "conjure.renderer.model";
}

public readonly struct EnvironmentAsset : IAssetType
{
    public string TypeName => "conjure.renderer.environment";
}

public readonly struct AudioClipAsset : IAssetType
{
    public string TypeName => "conjure.audio.clip";
}

public readonly struct PrefabAsset : IAssetType
{
    public string TypeName => "conjure.scene.prefab";
}

public readonly struct SceneAsset : IAssetType
{
    public string TypeName => "conjure.scene.scene";
}

/// <summary>
/// "conjure.physics.collision_mesh" (R44).
/// </summary>
public readonly struct CollisionMeshAsset : IAssetType
{
    public string TypeName => "conjure.physics.collision_mesh";
}

/// <summary>
/// The native asset type id of a tag, resolved once per class load (and per native handshake) through
/// asset_type_id; 0 for an untyped T or before the handshake.
/// </summary>
public static unsafe class AssetTypeId<T>
{
    private static ulong _value;
    private static int _generation = -1;

    /// <summary>
    /// The registered type name of T, or "" when T is not an IAssetType tag.
    /// </summary>
    public static readonly string TypeName = ResolveName();

    public static ulong Value
    {
        get
        {
            if (_value == 0 || _generation != NativeBridge.Generation)
            {
                _generation = NativeBridge.Generation;
                _value = Resolve();
            }
            return _value;
        }
    }

    private static string ResolveName()
    {
        Type type = typeof(T);
        if (!type.IsValueType || !typeof(IAssetType).IsAssignableFrom(type))
        {
            return "";
        }
        object? tag = Activator.CreateInstance(type);
        return tag is IAssetType assetType ? assetType.TypeName : "";
    }

    private static ulong Resolve()
    {
        if (TypeName.Length == 0)
        {
            return 0;
        }
        NativeApi* api = NativeBridge.Api;
        if (api == null || api->AssetTypeId == null)
        {
            return 0;
        }
        Span<byte> scratch = stackalloc byte[Utf8Marshal.ScratchSize];
        Utf8Encoded encoded = Utf8Marshal.Encode(TypeName, scratch);
        fixed (byte* pinned = encoded.Bytes)
        {
            return api->AssetTypeId(pinned, encoded.Length);
        }
    }
}
