using System.Text;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// Scene loading through the bindings' scene_* entries (NotSupportedException until the bindings fill
/// them, D11). Loads are requests applied at FRAME_BEGIN, never immediate.
/// </summary>
public static unsafe class Scene
{
    public static bool RequestLoad(string path, bool additive = false)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->SceneRequestLoad, NativeFunction.SceneRequestLoad);
        byte[] bytes = Encoding.UTF8.GetBytes(path ?? "");
        fixed (byte* pinned = bytes)
        {
            return api->SceneRequestLoad(pinned, bytes.Length, additive ? 1 : 0) >= 0;
        }
    }

    public static bool RequestUnload(string path)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->SceneRequestUnload, NativeFunction.SceneRequestUnload);
        byte[] bytes = Encoding.UTF8.GetBytes(path ?? "");
        fixed (byte* pinned = bytes)
        {
            return api->SceneRequestUnload(pinned, bytes.Length) >= 0;
        }
    }

    public static bool IsLoaded(string path)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->SceneIsLoaded, NativeFunction.SceneIsLoaded);
        byte[] bytes = Encoding.UTF8.GetBytes(path ?? "");
        fixed (byte* pinned = bytes)
        {
            return api->SceneIsLoaded(pinned, bytes.Length) > 0;
        }
    }

    public static int EntityCount
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->SceneEntityCount, NativeFunction.SceneEntityCount);
            return api->SceneEntityCount();
        }
    }
}
