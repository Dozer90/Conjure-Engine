using System;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// modules::module_state.
/// </summary>
public enum ModuleState : byte
{
    Discovered,
    Skipped,
    Resolved,
    Loaded,
    Failed,
    Unloaded,
    Faulted,
}

/// <summary>
/// modules::module_kind.
/// </summary>
public enum ModuleKind : byte
{
    Engine,
    Plugin,
    Game,
}

/// <summary>
/// One module of the registry as the modules_* entries report it.
/// </summary>
public readonly struct ModuleInfo
{
    public ModuleInfo(string name, Version version, ModuleState state, ModuleKind kind, uint featureFlags)
    {
        Name = name;
        Version = version;
        State = state;
        Kind = kind;
        FeatureFlags = featureFlags;
    }

    public string Name { get; }

    public Version Version { get; }

    public ModuleState State { get; }

    public ModuleKind Kind { get; }

    /// <summary>
    /// The manifest's required feature flags (modules::module_feature_flags bits).
    /// </summary>
    public uint FeatureFlags { get; }

    public bool IsLoaded => State == ModuleState.Loaded;

    public override string ToString() => Name + " " + Version + " (" + State + ")";
}

/// <summary>
/// The read-only, index-based view of the module registry (docs/design/15_bindings.md 2.5, B12): the
/// engine's built-in libraries, the plugins and the game module. Get and Find allocate the record.
/// </summary>
public static class Modules
{
    public static unsafe class Registry
    {
        /// <summary>
        /// The number of registered modules.
        /// </summary>
        public static int Count
        {
            get
            {
                NativeApi* api = NativeBridge.RequireApi();
                NativeBridge.Require(api->ModulesCount, NativeFunction.ModulesCount);
                int status = api->ModulesCount();
                NativeCheck.ThrowIfFailed(status, NativeFunction.ModulesCount);
                return status;
            }
        }

        /// <summary>
        /// The module at 'index' (ArgumentException beyond Count).
        /// </summary>
        public static ModuleInfo Get(int index)
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->ModulesInfo, NativeFunction.ModulesInfo);
            NativeBridge.Require(api->ModulesName, NativeFunction.ModulesName);
            ModuleRecord record = default;
            NativeCheck.ThrowIfFailed(api->ModulesInfo(index, &record), NativeFunction.ModulesInfo);
            string name = Utf8Marshal.ReadNeeded(index, static (int state, byte* buffer, int capacity) =>
            {
                return NativeBridge.Api->ModulesName(state, buffer, capacity);
            }, out int status);
            NativeCheck.ThrowIfFailed(status, NativeFunction.ModulesName);
            Version version = new Version((int)record.VersionMajor, (int)record.VersionMinor, (int)record.VersionPatch);
            return new ModuleInfo(name, version, (ModuleState)record.State, (ModuleKind)record.Kind, record.FeatureFlags);
        }

        /// <summary>
        /// The index of the named module, or -1.
        /// </summary>
        public static int IndexOf(string name)
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->ModulesFind, NativeFunction.ModulesFind);
            Span<byte> scratch = stackalloc byte[Utf8Marshal.ScratchSize];
            Utf8Encoded encoded = Utf8Marshal.Encode(name, scratch);
            fixed (byte* pinned = encoded.Bytes)
            {
                int status = api->ModulesFind(pinned, encoded.Length);
                if (NativeCheck.Is(status, BindingStatus.NotFound))
                {
                    return -1;
                }
                NativeCheck.ThrowIfFailed(status, NativeFunction.ModulesFind);
                return status;
            }
        }

        /// <summary>
        /// The named module, or null when unknown.
        /// </summary>
        public static ModuleInfo? Find(string name)
        {
            int index = IndexOf(name);
            return index < 0 ? null : Get(index);
        }

        /// <summary>
        /// True when the named module is registered and LOADED.
        /// </summary>
        public static bool IsLoaded(string name)
        {
            ModuleInfo? found = Find(name);
            return found.HasValue && found.Value.IsLoaded;
        }
    }
}
