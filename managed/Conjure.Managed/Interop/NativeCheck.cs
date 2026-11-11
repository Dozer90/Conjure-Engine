using System;
using System.Collections.Generic;
using System.Text;

namespace Conjure.Interop;

/// <summary>
/// The status protocol of the bindings entries (docs/design/15_bindings.md B4, 3.4): an int32 result
/// &gt;= 0 is a success (0, a count, a bool or a needed length), &lt; 0 is -(int)BindingStatus. ThrowIfFailed
/// turns a failure into the matching exception with the native last-error message.
/// </summary>
public static unsafe class NativeCheck
{
    /// <summary>
    /// Size of the native last-error buffer (conjure::bindings::LAST_ERROR_CAPACITY).
    /// </summary>
    public const int LastErrorCapacity = 512;

    /// <summary>
    /// True for a failure status.
    /// </summary>
    public static bool IsFailure(int status) => status < 0;

    /// <summary>
    /// The BindingStatus of a failure status (None for a success).
    /// </summary>
    public static BindingStatus StatusOf(int status) => status < 0 ? (BindingStatus)(-status) : BindingStatus.None;

    /// <summary>
    /// True when the status is exactly -(int)code.
    /// </summary>
    public static bool Is(int status, BindingStatus code) => status < 0 && -status == (int)code;

    /// <summary>
    /// Returns when the status is a success; otherwise reads the last-error message and throws the mapped
    /// exception (3.4).
    /// </summary>
    public static void ThrowIfFailed(int status)
    {
        if (status >= 0)
        {
            return;
        }
        throw BindingStatusMap.Create((BindingStatus)(-status), ReadLastError(out _));
    }

    /// <summary>
    /// ThrowIfFailed naming the entry when native kept no message (an unfilled slot cannot write one).
    /// </summary>
    public static void ThrowIfFailed(int status, NativeFunction function)
    {
        if (status >= 0)
        {
            return;
        }
        string message = ReadLastError(out _);
        if (message.Length == 0)
        {
            message = NativeFunctionNames.Of(function);
        }
        throw BindingStatusMap.Create((BindingStatus)(-status), message);
    }

    /// <summary>
    /// Throws the exception of the last recorded native error: for uint64 entries that return 0 on
    /// failure. Throws ConjureNativeException(Internal) when nothing was recorded.
    /// </summary>
    public static Exception LastErrorException(NativeFunction function)
    {
        string message = ReadLastError(out int status);
        if (status >= 0)
        {
            return new ConjureNativeException(BindingStatus.Internal, NativeFunctionNames.Of(function) + " failed");
        }
        if (message.Length == 0)
        {
            message = NativeFunctionNames.Of(function);
        }
        return BindingStatusMap.Create((BindingStatus)(-status), message);
    }

    /// <summary>
    /// The main thread's last native error: the status (0 when none) and the "entry: detail" message ("" when
    /// none or when the diagnostics entry is unavailable). Reading clears nothing.
    /// </summary>
    public static string ReadLastError(out int status)
    {
        status = 0;
        NativeApi* api = NativeBridge.Api;
        if (api == null || api->DiagnosticsLastError == null)
        {
            return "";
        }
        byte* buffer = stackalloc byte[LastErrorCapacity];
        int recorded = 0;
        int needed = api->DiagnosticsLastError(&recorded, buffer, LastErrorCapacity);
        status = recorded;
        if (needed <= 0)
        {
            return "";
        }
        int length = needed < LastErrorCapacity ? needed : LastErrorCapacity;
        return Encoding.UTF8.GetString(buffer, length);
    }
}

/// <summary>
/// BindingStatus to exception type (docs/design/15_bindings.md 3.4).
/// </summary>
public static class BindingStatusMap
{
    public static Exception Create(BindingStatus status, string message)
    {
        string text = message.Length == 0 ? BindingStatusNames.Of(status) : message;
        switch (status)
        {
            case BindingStatus.EntityNotAlive:
                return new EntityNotAliveException(text);
            case BindingStatus.AssetNotLoaded:
                return new AssetNotLoadedException(text);
            case BindingStatus.NotSupported:
                return new NotSupportedException(text);
            case BindingStatus.WrongThread:
            case BindingStatus.NotInPhase:
            case BindingStatus.StructureLocked:
                return new InvalidOperationException(text);
            case BindingStatus.InvalidArgument:
                return new ArgumentException(text);
            case BindingStatus.NotFound:
            case BindingStatus.SceneNotFound:
                return new KeyNotFoundException(text);
            default:
                return new ConjureNativeException(status, text);
        }
    }
}
