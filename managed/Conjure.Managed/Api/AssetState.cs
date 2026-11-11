namespace Conjure;

/// <summary>
/// Lifecycle state of an asset slot (assets::asset_state, 05_assets.md): what asset_state_of and
/// AssetRef&lt;T&gt;.State report.
/// </summary>
public enum AssetState : byte
{
    Unloaded = 0,
    Queued,
    Loading,
    Finalising,
    Loaded,
    Reloading,
    Failed,
}
