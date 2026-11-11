using System.Runtime.InteropServices;

namespace Conjure.Components;

// The audio component mirrors (docs/design/15_bindings.md 2.6, 12_audio.md 2.6, R50). The engine's
// audio_subsystem registers the native types; the AudioSource facade drives playback through the hidden
// PlaybackRequest field (B8).

/// <summary>
/// audio::attenuation_model.
/// </summary>
public enum AttenuationModel : byte
{
    None,
    Inverse,
    Linear,
    Exponential,
}

/// <summary>
/// audio::audio_playback_request: the command engine.audio_sync consumes.
/// </summary>
public enum AudioPlaybackRequest : byte
{
    None = 0,
    Play,
    Stop,
    Pause,
    Resume,
}

/// <summary>
/// core::slot_key (8 bytes).
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct SlotKey
{
    public uint Index;
    public uint Generation;
}

/// <summary>
/// audio::voice_id (8 bytes).
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct VoiceId
{
    public SlotKey Key;
}

/// <summary>
/// audio::source_handle (16 bytes): the link between a source and its voice, owned by the engine.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct SourceHandle
{
    public VoiceId Voice;
    public uint ClipVersion;
    public byte Flags;
    public byte Pad0;
    public byte Pad1;
    public byte Pad2;
}

/// <summary>
/// Mirror of audio.audio_source (88 bytes). PlaybackRequest, IsPlaying and Handle are runtime fields: write
/// PlaybackRequest through AudioSource, read IsPlaying (one frame old), leave Handle alone.
/// </summary>
[NativeComponent("conjure.audio.audio_source")]
[StructLayout(LayoutKind.Sequential)]
public struct AudioSourceData : IComponent
{
    public AssetRef<AudioClipAsset> Clip;
    public float Volume;
    public float Pitch;
    public NativeBool Loop;
    public NativeBool Spatial;
    public NativeBool PlayOnStart;
    public NativeBool OneShot;
    public StringId Group;
    public AttenuationModel Attenuation;
    public byte Pad0;
    public byte Pad1;
    public byte Pad2;
    public float MinDistance;
    public float MaxDistance;
    public float Rolloff;
    public AudioPlaybackRequest PlaybackRequest;
    public NativeBool IsPlaying;
    public byte Pad3;
    public byte Pad4;
    public SourceHandle Handle;

    /// <summary>
    /// The native defaults: full volume, spatial, plays on start, inverse attenuation over 1..100 m.
    /// </summary>
    public static AudioSourceData Default => new AudioSourceData
    {
        Volume = 1.0f,
        Pitch = 1.0f,
        Spatial = true,
        PlayOnStart = true,
        Attenuation = AttenuationModel.Inverse,
        MinDistance = 1.0f,
        MaxDistance = 100.0f,
        Rolloff = 1.0f,
    };
}

/// <summary>
/// Mirror of audio.audio_listener (8 bytes).
/// </summary>
[NativeComponent("conjure.audio.audio_listener")]
[StructLayout(LayoutKind.Sequential)]
public struct AudioListenerData : IComponent
{
    public float Gain;
    public NativeBool Active;
    public byte Pad0;
    public byte Pad1;
    public byte Pad2;

    public static AudioListenerData Default => new AudioListenerData { Gain = 1.0f, Active = true };
}
