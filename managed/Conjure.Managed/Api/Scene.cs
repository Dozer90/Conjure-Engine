using System;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// Scene loading and the loaded set through the scene_* entries (docs/design/15_bindings.md 2.5). Loads and
/// unloads are requests the scene manager applies at the next FRAME_BEGIN, never immediately; a path the
/// asset sources do not know is KeyNotFoundException. IsLoaded and IsLoadPending never throw for an
/// unknown path.
/// </summary>
public static unsafe class Scene
{
    /// <summary>
    /// Requests a scene by asset path; replaces the loaded scenes unless 'additive'.
    /// </summary>
    public static void Load(string path, bool additive = false)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->SceneRequestLoad, NativeFunction.SceneRequestLoad);
        Span<byte> scratch = stackalloc byte[Utf8Marshal.ScratchSize];
        Utf8Encoded encoded = Utf8Marshal.Encode(path, scratch);
        fixed (byte* pinned = encoded.Bytes)
        {
            NativeCheck.ThrowIfFailed(api->SceneRequestLoad(pinned, encoded.Length, additive ? 1 : 0), NativeFunction.SceneRequestLoad);
        }
    }

    /// <summary>
    /// Requests a scene by asset reference; replaces the loaded scenes unless 'additive'.
    /// </summary>
    public static void Load(AssetRef<SceneAsset> scene, bool additive = false)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->SceneRequestLoadAsset, NativeFunction.SceneRequestLoadAsset);
        UuidData id = scene.Id;
        NativeCheck.ThrowIfFailed(api->SceneRequestLoadAsset(&id, additive ? 1 : 0), NativeFunction.SceneRequestLoadAsset);
    }

    /// <summary>
    /// Requests the unload of a loaded scene (KeyNotFoundException for an unknown path).
    /// </summary>
    public static void Unload(string path)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->SceneRequestUnload, NativeFunction.SceneRequestUnload);
        Span<byte> scratch = stackalloc byte[Utf8Marshal.ScratchSize];
        Utf8Encoded encoded = Utf8Marshal.Encode(path, scratch);
        fixed (byte* pinned = encoded.Bytes)
        {
            NativeCheck.ThrowIfFailed(api->SceneRequestUnload(pinned, encoded.Length), NativeFunction.SceneRequestUnload);
        }
    }

    /// <summary>
    /// True when the scene at 'path' is loaded.
    /// </summary>
    public static bool IsLoaded(string path)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->SceneIsLoaded, NativeFunction.SceneIsLoaded);
        Span<byte> scratch = stackalloc byte[Utf8Marshal.ScratchSize];
        Utf8Encoded encoded = Utf8Marshal.Encode(path, scratch);
        fixed (byte* pinned = encoded.Bytes)
        {
            return api->SceneIsLoaded(pinned, encoded.Length) > 0;
        }
    }

    /// <summary>
    /// True while a load of the scene at 'path' is requested but not applied.
    /// </summary>
    public static bool IsLoadPending(string path)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->SceneIsLoadPending, NativeFunction.SceneIsLoadPending);
        Span<byte> scratch = stackalloc byte[Utf8Marshal.ScratchSize];
        Utf8Encoded encoded = Utf8Marshal.Encode(path, scratch);
        fixed (byte* pinned = encoded.Bytes)
        {
            return api->SceneIsLoadPending(pinned, encoded.Length) > 0;
        }
    }

    /// <summary>
    /// The path of the active scene, Unity's meaning: the first loaded scene. That is the scene of the last
    /// non-additive load (it unloads everything before it), or the oldest one when every load was additive.
    /// Additive loads never change it; when it is unloaded the oldest remaining scene takes over. "" when no
    /// scene is loaded; the uuid text for a scene no asset database names.
    /// </summary>
    public static string ActivePath
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->SceneActivePath, NativeFunction.SceneActivePath);
            string path = Utf8Marshal.ReadNeeded(0, static (int state, byte* buffer, int capacity) =>
            {
                return NativeBridge.Api->SceneActivePath(buffer, capacity);
            }, out int status);
            NativeCheck.ThrowIfFailed(status, NativeFunction.SceneActivePath);
            return path;
        }
    }

    /// <summary>
    /// The number of loaded scenes.
    /// </summary>
    public static int LoadedCount
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->SceneLoadedCount, NativeFunction.SceneLoadedCount);
            int status = api->SceneLoadedCount();
            NativeCheck.ThrowIfFailed(status, NativeFunction.SceneLoadedCount);
            return status;
        }
    }

    /// <summary>
    /// The asset path of the loaded scene at 'index' (ArgumentException beyond LoadedCount).
    /// </summary>
    public static string LoadedPath(int index)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->SceneLoadedPath, NativeFunction.SceneLoadedPath);
        string path = Utf8Marshal.ReadNeeded(index, static (int state, byte* buffer, int capacity) =>
        {
            return NativeBridge.Api->SceneLoadedPath(state, buffer, capacity);
        }, out int status);
        NativeCheck.ThrowIfFailed(status, NativeFunction.SceneLoadedPath);
        return path;
    }

    /// <summary>
    /// The number of live entities in the world every loaded scene shares (not only the active scene's).
    /// </summary>
    public static int EntityCount
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->SceneEntityCount, NativeFunction.SceneEntityCount);
            int status = api->SceneEntityCount();
            NativeCheck.ThrowIfFailed(status, NativeFunction.SceneEntityCount);
            return status;
        }
    }

    /// <summary>
    /// A counter that changes with every hierarchy edit; equal values mean an unchanged structure.
    /// </summary>
    public static ulong StructureVersion
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->SceneStructureVersion, NativeFunction.SceneStructureVersion);
            return api->SceneStructureVersion();
        }
    }

    /// <summary>
    /// Copies up to 'roots.Length' root entities in order and returns the total number of roots.
    /// </summary>
    public static int GetRoots(Span<Entity> roots)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->SceneRoots, NativeFunction.SceneRoots);
        fixed (Entity* pinned = roots)
        {
            int status = api->SceneRoots((ulong*)pinned, roots.Length);
            NativeCheck.ThrowIfFailed(status, NativeFunction.SceneRoots);
            return status;
        }
    }
}
