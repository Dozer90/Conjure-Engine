using Conjure.Components;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// The audio source facade of an entity (docs/design/15_bindings.md 2.5, B8): playback commands land in
/// audio.audio_source.playbackRequest and the engine's audio sync system applies them at the end of the
/// frame; IsPlaying is what the engine wrote back last frame and never throws (B11). Volume, Pitch and Loop
/// edit the component through component_get / component_set (InvalidOperationException without one).
/// </summary>
public readonly struct AudioSource
{
    public readonly Entity Entity;

    public AudioSource(Entity entity)
    {
        Entity = entity;
    }

    /// <summary>
    /// True when the entity carries an audio source.
    /// </summary>
    public bool Exists => Entity.HasComponent<AudioSourceData>();

    public unsafe void Play()
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->AudioPlay, NativeFunction.AudioPlay);
        NativeCheck.ThrowIfFailed(api->AudioPlay(Entity.Id), NativeFunction.AudioPlay);
    }

    public unsafe void Stop()
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->AudioStop, NativeFunction.AudioStop);
        NativeCheck.ThrowIfFailed(api->AudioStop(Entity.Id), NativeFunction.AudioStop);
    }

    public unsafe void Pause()
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->AudioPause, NativeFunction.AudioPause);
        NativeCheck.ThrowIfFailed(api->AudioPause(Entity.Id), NativeFunction.AudioPause);
    }

    public unsafe void Resume()
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->AudioResume, NativeFunction.AudioResume);
        NativeCheck.ThrowIfFailed(api->AudioResume(Entity.Id), NativeFunction.AudioResume);
    }

    /// <summary>
    /// True when the voice was playing at the end of the last frame; false for a dead entity or one
    /// without a source.
    /// </summary>
    public unsafe bool IsPlaying
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->AudioIsPlaying, NativeFunction.AudioIsPlaying);
            return api->AudioIsPlaying(Entity.Id) > 0;
        }
    }

    public float Volume
    {
        get => Data.Volume;
        set
        {
            AudioSourceData data = Data;
            data.Volume = value;
            Data = data;
        }
    }

    public float Pitch
    {
        get => Data.Pitch;
        set
        {
            AudioSourceData data = Data;
            data.Pitch = value;
            Data = data;
        }
    }

    public bool Loop
    {
        get => Data.Loop;
        set
        {
            AudioSourceData data = Data;
            data.Loop = value;
            Data = data;
        }
    }

    /// <summary>
    /// The clip the source plays.
    /// </summary>
    public AssetRef<AudioClipAsset> Clip
    {
        get => Data.Clip;
        set
        {
            AudioSourceData data = Data;
            data.Clip = value;
            Data = data;
        }
    }

    /// <summary>
    /// The audio_source component itself; the setter writes it back through component_set (binding the clip).
    /// </summary>
    public AudioSourceData Data
    {
        get => Entity.GetComponent<AudioSourceData>();
        set => Entity.SetComponent(in value);
    }
}
