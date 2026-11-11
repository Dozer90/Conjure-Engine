using System.Numerics;
using Conjure.Components;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// Mixer-level audio through the audio_* entries (docs/design/15_bindings.md 2.5): one-shots, the master and
/// group gains and clip durations. NotSupportedException when the build or the configuration has no audio.
/// Per-entity playback is AudioSource.
/// </summary>
public static unsafe class Audio
{
    /// <summary>
    /// Plays a resident clip once on a fire-and-forget voice (AssetNotLoadedException otherwise), spatial
    /// at 'position' when given, on the named mix group (null = master; KeyNotFoundException for an unknown
    /// group).
    /// </summary>
    public static void PlayOneShot(
        AssetRef<AudioClipAsset> clip,
        Vector3? position = null,
        float volume = 1.0f,
        float pitch = 1.0f,
        string? group = null)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->AudioPlayOneShot, NativeFunction.AudioPlayOneShot);
        UuidData id = clip.Id;
        ulong groupHash = StringId.Of(group).Hash;
        int status;
        if (position.HasValue)
        {
            Vector3 at = position.Value;
            status = api->AudioPlayOneShot(&id, &at, volume, pitch, groupHash);
        }
        else
        {
            status = api->AudioPlayOneShot(&id, null, volume, pitch, groupHash);
        }
        NativeCheck.ThrowIfFailed(status, NativeFunction.AudioPlayOneShot);
    }

    /// <summary>
    /// The master gain (0..2).
    /// </summary>
    public static float MasterGain
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->AudioGetMasterGain, NativeFunction.AudioGetMasterGain);
            float value = 1.0f;
            NativeCheck.ThrowIfFailed(api->AudioGetMasterGain(&value), NativeFunction.AudioGetMasterGain);
            return value;
        }
        set
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->AudioSetMasterGain, NativeFunction.AudioSetMasterGain);
            NativeCheck.ThrowIfFailed(api->AudioSetMasterGain(value), NativeFunction.AudioSetMasterGain);
        }
    }

    /// <summary>
    /// The gain of a named mix group (KeyNotFoundException for an unknown one).
    /// </summary>
    public static float GetGroupGain(string group)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->AudioGetGroupGain, NativeFunction.AudioGetGroupGain);
        float value = 1.0f;
        NativeCheck.ThrowIfFailed(api->AudioGetGroupGain(StringId.Of(group).Hash, &value), NativeFunction.AudioGetGroupGain);
        return value;
    }

    /// <summary>
    /// Sets the gain of a named mix group (KeyNotFoundException for an unknown one).
    /// </summary>
    public static void SetGroupGain(string group, float gain)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->AudioSetGroupGain, NativeFunction.AudioSetGroupGain);
        NativeCheck.ThrowIfFailed(api->AudioSetGroupGain(StringId.Of(group).Hash, gain), NativeFunction.AudioSetGroupGain);
    }

    /// <summary>
    /// The duration of a resident clip in seconds (AssetNotLoadedException otherwise).
    /// </summary>
    public static float ClipDuration(AssetRef<AudioClipAsset> clip)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->AudioClipDuration, NativeFunction.AudioClipDuration);
        UuidData id = clip.Id;
        float seconds = 0.0f;
        NativeCheck.ThrowIfFailed(api->AudioClipDuration(&id, &seconds), NativeFunction.AudioClipDuration);
        return seconds;
    }
}
