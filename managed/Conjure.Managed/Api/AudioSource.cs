using System;
using Conjure.Components;
using Conjure.Dod;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// The audio source facade of an entity (docs/design/15_bindings.md 2.5, B8): playback commands land in
/// audio.audio_source.playbackRequest and the engine's audio sync system applies them at the end of the
/// frame; IsPlaying is what the engine wrote back last frame and never throws (B11). Volume, Pitch and Loop
/// edit the component through component_get / component_set (InvalidOperationException without one,
/// EntityNotAliveException for a dead entity): one component_get and one component_set each, with no asset bind
/// once the clip is bound; edit many sources through a ComponentQuery span. On an entity Entity.Create or
/// Prefab.Instantiate reserved in the current ComponentQuery scope the playback commands and the setters never
/// throw: they are kept and made in call order once the scope closed and the entity exists (a failure then, such
/// as no audio source, is logged at ERROR); the getters throw EntityNotAliveException until then (IsPlaying is
/// false).
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
        if (Defer(this, static facade => facade.Play()))
        {
            return;
        }
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->AudioPlay, NativeFunction.AudioPlay);
        NativeCheck.ThrowIfFailed(api->AudioPlay(Entity.Id), NativeFunction.AudioPlay);
    }

    public unsafe void Stop()
    {
        if (Defer(this, static facade => facade.Stop()))
        {
            return;
        }
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->AudioStop, NativeFunction.AudioStop);
        NativeCheck.ThrowIfFailed(api->AudioStop(Entity.Id), NativeFunction.AudioStop);
    }

    public unsafe void Pause()
    {
        if (Defer(this, static facade => facade.Pause()))
        {
            return;
        }
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->AudioPause, NativeFunction.AudioPause);
        NativeCheck.ThrowIfFailed(api->AudioPause(Entity.Id), NativeFunction.AudioPause);
    }

    public unsafe void Resume()
    {
        if (Defer(this, static facade => facade.Resume()))
        {
            return;
        }
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
            if (Defer((this, value), static state => state.Item1.Volume = state.Item2))
            {
                return;
            }
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
            if (Defer((this, value), static state => state.Item1.Pitch = state.Item2))
            {
                return;
            }
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
            if (Defer((this, value), static state => state.Item1.Loop = state.Item2))
            {
                return;
            }
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
            if (Defer((this, value), static state => state.Item1.Clip = state.Item2))
            {
                return;
            }
            AudioSourceData data = Data;
            data.Clip = value;
            Data = data;
        }
    }

    /// <summary>
    /// The audio_source component itself; the setter writes it back through component_set (binding the clip).
    /// Both throw EntityNotAliveException for a dead entity and InvalidOperationException when the entity has no
    /// audio source (Entity.GetComponent / SetComponent).
    /// </summary>
    public AudioSourceData Data
    {
        get => Entity.GetComponent<AudioSourceData>();
        set
        {
            if (Defer((this, value), static state => state.Item1.Data = state.Item2))
            {
                return;
            }
            Entity.SetComponent(in value);
        }
    }

    /// <summary>
    /// Keeps a command or write for the end of the ComponentQuery scope that reserved the entity (ReservedWrites);
    /// false when the entity is not reserved and the caller acts now.
    /// </summary>
    private bool Defer<TState>(TState state, Action<TState> write) => ReservedWrites.TryDefer(Entity, state, write);

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
        if (api->ComponentSet(Entity.Id, ComponentType<AudioSourceData>.RequireId(), &copy) != 0)
        {
            return;
        }
        throw Entity.ComponentFailure<AudioSourceData>(NativeFunction.ComponentSet);
    }
}
