using Conjure.Components;
using Conjure.Dod;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// The audio source facade of an entity (docs/design/15_bindings.md 2.5, B8): playback commands land in
/// audio.audio_source.playbackRequest and the engine's audio sync system applies them at the end of the
/// frame; IsPlaying is what the engine wrote back last frame and never throws (B11). Volume, Pitch and Loop
/// edit the component through component_get / component_set (InvalidOperationException without one): one
/// component_get and one component_set each, with no asset bind once the clip is bound; edit many sources
/// through a ComponentQuery span.
/// </summary>
public readonly struct AudioSource
{
    public readonly Entity Entity;

    public AudioSource(Entity entity)
    {
        Entity = entity;
    }

    /// <summary>
    /// True when the entity carries an audio source; false in a world without the audio components (a build
    /// without the audio subsystem), where Entity.HasComponent&lt;AudioSourceData&gt; throws.
    /// </summary>
    public bool Exists => ComponentType<AudioSourceData>.Id != 0 && Entity.HasComponent<AudioSourceData>();

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
            WriteFields(in data);
        }
    }

    public float Pitch
    {
        get => Data.Pitch;
        set
        {
            AudioSourceData data = Data;
            data.Pitch = value;
            WriteFields(in data);
        }
    }

    public bool Loop
    {
        get => Data.Loop;
        set
        {
            AudioSourceData data = Data;
            data.Loop = value;
            WriteFields(in data);
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

    /// <summary>
    /// Writes back a Volume, Pitch or Loop edit of 'data', which was just read through Data.
    /// </summary>
    private unsafe void WriteFields(in AudioSourceData data)
    {
        if (!data.Clip.IsNull && !data.Clip.IsBound)
        {
            // Native holds the clip id with an unbound slot (asset_ref(id) reads nullptr until bound): the R25
            // bind pass of SetComponent binds it, as every write did before.
            Entity.SetComponent(in data);
            return;
        }
        // Read back through component_get, a non-null slot is the one native bound: the bind pass
        // (asset_ref_bind) would have nothing to do.
        NativeApi* api = NativeBridge.RequireApi();
        AudioSourceData copy = data;
        api->ComponentSet(Entity.Id, ComponentType<AudioSourceData>.RequireId(), &copy);
    }
}
