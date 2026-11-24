namespace Conjure.Interop;

/// <summary>
/// The statuses the scripting side of the native table exchanges: 0 for success, otherwise the negated
/// conjure::scripting::scripting_error code (engine/include/conjure/scripting/scripting_error.h). One spelling
/// for the managed callbacks that return them and the API that reads them from the core entries.
/// </summary>
internal static class ScriptStatus
{
    /// <summary>
    /// Success.
    /// </summary>
    public const int Ok = 0;

    /// <summary>
    /// -scripting_error::ASSEMBLY_LOAD_FAILED: the game assembly did not load.
    /// </summary>
    public const int LoadFailed = -7;

    /// <summary>
    /// -scripting_error::INSTANCE_NOT_FOUND: no instance (or, from script_add, no entity) behind the id.
    /// </summary>
    public const int InstanceNotFound = -12;

    /// <summary>
    /// -scripting_error::NOT_INITIALISED: no script runtime with a world is running.
    /// </summary>
    public const int NotInitialised = -16;

    /// <summary>
    /// -scripting_error::NOT_SUPPORTED: the request is not available (from script_add: the entity refused it).
    /// </summary>
    public const int NotSupported = -17;
}
