using Conjure.Interop;

namespace Conjure;

/// <summary>
/// Audio playback through the bindings' audio_* entries (NotSupportedException until the bindings fill
/// them, D11).
/// </summary>
public static unsafe class Audio
{
    public static bool Play(Entity entity)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->AudioPlay, NativeFunction.AudioPlay);
        return api->AudioPlay(entity.Id) >= 0;
    }

    public static bool Stop(Entity entity)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->AudioStop, NativeFunction.AudioStop);
        return api->AudioStop(entity.Id) >= 0;
    }

    public static bool IsPlaying(Entity entity)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->AudioIsPlaying, NativeFunction.AudioIsPlaying);
        return api->AudioIsPlaying(entity.Id) > 0;
    }

    public static float MasterGain
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->AudioGetMasterGain, NativeFunction.AudioGetMasterGain);
            float value = 1.0f;
            api->AudioGetMasterGain(&value);
            return value;
        }
        set
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->AudioSetMasterGain, NativeFunction.AudioSetMasterGain);
            api->AudioSetMasterGain(value);
        }
    }
}
