using System;

namespace Conjure.Interop;

/// <summary>
/// A bindings entry failed with a status that has no closer .NET exception (docs/design/15_bindings.md
/// 3.4). Status is the native code; Message is "&lt;entry&gt;: &lt;detail&gt;" from the last-error buffer.
/// </summary>
public class ConjureNativeException : Exception
{
    public ConjureNativeException(BindingStatus status, string message)
        : base(message)
    {
        Status = status;
    }

    /// <summary>
    /// The conjure::bindings::bindings_error behind the failure.
    /// </summary>
    public BindingStatus Status { get; }
}

/// <summary>
/// The entity of the call is not alive (BindingStatus.EntityNotAlive). Predicates never raise it; setters
/// and getters do.
/// </summary>
public sealed class EntityNotAliveException : ConjureNativeException
{
    public EntityNotAliveException(string message)
        : base(BindingStatus.EntityNotAlive, message)
    {
    }
}

/// <summary>
/// The asset the call needs is not resident (BindingStatus.AssetNotLoaded): load it first.
/// </summary>
public sealed class AssetNotLoadedException : ConjureNativeException
{
    public AssetNotLoadedException(string message)
        : base(BindingStatus.AssetNotLoaded, message)
    {
    }
}
