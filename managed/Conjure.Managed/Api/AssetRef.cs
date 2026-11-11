using System;
using System.Runtime.InteropServices;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// Untyped view of an asset reference (the field codec reads the id through it).
/// </summary>
public interface IAssetRef
{
    UuidData Id { get; }
}

/// <summary>
/// A reference to an asset of type T by id (docs/design/15_bindings.md 2.5): the ASSET field kind of
/// scripts and the 24-byte ASSET_REF mirror of assets::asset_ref (the id followed by the bound slot
/// pointer, which native fills when a component carrying the reference lands). T is an IAssetType tag;
/// any other T leaves the reference untyped. Residency goes through the asset_* entries (B9): Load holds
/// the asset for the scripts until Release or the next scene unload.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public readonly struct AssetRef<T> : IAssetRef, IEquatable<AssetRef<T>>
{
    public readonly AssetId Id;
    private readonly nint _slot;

    public AssetRef(AssetId id)
    {
        Id = id;
        _slot = 0;
    }

    public AssetRef(UuidData id)
    {
        Id = id;
        _slot = 0;
    }

    UuidData IAssetRef.Id => Id;

    /// <summary>
    /// True for a non-null id.
    /// </summary>
    public bool IsValid => !Id.IsNull;

    public bool IsNull => Id.IsNull;

    /// <summary>
    /// True once native bound the slot (the reference came out of a component).
    /// </summary>
    public bool IsBound => _slot != 0;

    /// <summary>
    /// The native asset type id of T (0 for an untyped reference).
    /// </summary>
    public static ulong TypeId => AssetTypeId<T>.Value;

    /// <summary>
    /// The slot state; Unloaded for a null id or an unknown asset.
    /// </summary>
    public unsafe AssetState State
    {
        get
        {
            AssetInfo info = default;
            if (!TryGetInfo(&info))
            {
                return AssetState.Unloaded;
            }
            return (AssetState)info.State;
        }
    }

    /// <summary>
    /// True when the asset is resident (never throws, B11).
    /// </summary>
    public bool IsLoaded => State == AssetState.Loaded;

    /// <summary>
    /// The slot's reload counter (0 for a null id).
    /// </summary>
    public unsafe uint Version
    {
        get
        {
            AssetInfo info = default;
            return TryGetInfo(&info) ? info.Version : 0u;
        }
    }

    /// <summary>
    /// Requests an asynchronous load and holds the asset for the scripts (asset_load).
    /// </summary>
    public unsafe void Load(AssetLoadPriority priority = AssetLoadPriority.Normal)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->AssetLoad, NativeFunction.AssetLoad);
        UuidData id = Id;
        NativeCheck.ThrowIfFailed(api->AssetLoad(&id, AssetTypeId<T>.Value, (byte)priority), NativeFunction.AssetLoad);
    }

    /// <summary>
    /// Loads now, on the calling thread, and holds the asset for the scripts (asset_load_blocking).
    /// </summary>
    public unsafe void LoadBlocking()
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->AssetLoadBlocking, NativeFunction.AssetLoadBlocking);
        UuidData id = Id;
        NativeCheck.ThrowIfFailed(api->AssetLoadBlocking(&id, AssetTypeId<T>.Value), NativeFunction.AssetLoadBlocking);
    }

    /// <summary>
    /// Drops the scripts' hold; true when one was held.
    /// </summary>
    public unsafe bool Release()
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->AssetRelease, NativeFunction.AssetRelease);
        UuidData id = Id;
        int status = api->AssetRelease(&id);
        NativeCheck.ThrowIfFailed(status, NativeFunction.AssetRelease);
        return status > 0;
    }

    /// <summary>
    /// The asset path ("" when unknown to every source or without a database).
    /// </summary>
    public unsafe string Path
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->AssetPathOf, NativeFunction.AssetPathOf);
            UuidData id = Id;
            string path = Utf8Marshal.ReadNeeded(id, static (UuidData state, byte* buffer, int capacity) =>
            {
                return NativeBridge.Api->AssetPathOf(&state, buffer, capacity);
            }, out int status);
            if (status < 0 && !NativeCheck.Is(status, BindingStatus.NotFound))
            {
                NativeCheck.ThrowIfFailed(status, NativeFunction.AssetPathOf);
            }
            return path;
        }
    }

    /// <summary>
    /// The reference of the asset at 'path', or an invalid reference when no source knows it.
    /// </summary>
    public static unsafe AssetRef<T> Find(string path)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->AssetFind, NativeFunction.AssetFind);
        Span<byte> scratch = stackalloc byte[Utf8Marshal.ScratchSize];
        Utf8Encoded encoded = Utf8Marshal.Encode(path, scratch);
        UuidData found = default;
        int status;
        fixed (byte* pinned = encoded.Bytes)
        {
            status = api->AssetFind(pinned, encoded.Length, &found);
        }
        if (NativeCheck.Is(status, BindingStatus.NotFound))
        {
            return default;
        }
        NativeCheck.ThrowIfFailed(status, NativeFunction.AssetFind);
        return new AssetRef<T>(found);
    }

    private unsafe bool TryGetInfo(AssetInfo* info)
    {
        if (Id.IsNull)
        {
            return false;
        }
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->AssetRefGet, NativeFunction.AssetRefGet);
        UuidData id = Id;
        int status = api->AssetRefGet(&id, AssetTypeId<T>.Value, info);
        if (status >= 0)
        {
            return true;
        }
        if (NativeCheck.Is(status, BindingStatus.InvalidArgument) || NativeCheck.Is(status, BindingStatus.NotFound))
        {
            return false;
        }
        NativeCheck.ThrowIfFailed(status, NativeFunction.AssetRefGet);
        return false;
    }

    public bool Equals(AssetRef<T> other) => Id == other.Id;

    public override bool Equals(object? obj) => obj is AssetRef<T> other && Equals(other);

    public override int GetHashCode() => Id.GetHashCode();

    public override string ToString() => typeof(T).Name + "(" + Id + ")";

    public static bool operator ==(AssetRef<T> left, AssetRef<T> right) => left.Equals(right);

    public static bool operator !=(AssetRef<T> left, AssetRef<T> right) => !left.Equals(right);
}

/// <summary>
/// Builds AssetRef&lt;T&gt; values from a runtime type (field codec).
/// </summary>
internal static class AssetRefFactory
{
    public static object? Create(Type assetRefType, UuidData id)
    {
        if (!assetRefType.IsGenericType || assetRefType.GetGenericTypeDefinition() != typeof(AssetRef<>))
        {
            return null;
        }
        return Activator.CreateInstance(assetRefType, id);
    }
}
