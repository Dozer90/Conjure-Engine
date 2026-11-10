using System;
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
/// A reference to an asset of type T by uuid (the ASSET field kind). Loading and residency are the
/// bindings' asset_* entries; this struct only carries the id.
/// </summary>
public readonly struct AssetRef<T> : IAssetRef, IEquatable<AssetRef<T>>
{
    public AssetRef(UuidData id)
    {
        Id = id;
    }

    public UuidData Id { get; }

    public bool IsNull => Id.IsNull;

    public bool Equals(AssetRef<T> other) => Id == other.Id;

    public override bool Equals(object? obj) => obj is AssetRef<T> other && Equals(other);

    public override int GetHashCode() => Id.GetHashCode();

    public override string ToString() => typeof(T).Name + "(" + Id + ")";
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
