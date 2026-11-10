using System;
using Conjure.Interop;

namespace Conjure.Runtime;

/// <summary>
/// Fault policy of the callbacks (D11): every exception is logged through native; the third consecutive
/// fault of one callback on one instance disables that callback until reload; a success resets the
/// counter; an exception inside OnCreate disables every callback of the instance.
/// </summary>
public static class Guard
{
    public const int ConsecutiveFaultsToDisable = 3;

    public static ushort AllCallbacksMask => (ushort)((1 << (int)ScriptCallback.Count) - 1);

    public static ushort Bit(ScriptCallback callback) => (ushort)(1 << (int)callback);

    /// <summary>
    /// Records a fault; returns true when the callback was just disabled.
    /// </summary>
    public static unsafe bool Report(ScriptBehaviour instance, ScriptCallback callback, Exception exception)
    {
        string className = instance.GetType().FullName ?? instance.GetType().Name;
        NativeBridge.Log(LogLevel.Error, className + "." + callback + " threw: " + exception);

        if (callback == ScriptCallback.Create)
        {
            instance.DisabledMask = AllCallbacksMask;
            MarkFaulted(instance, callback);
            return true;
        }

        int index = (int)callback;
        byte count = instance.ConsecutiveFaults[index];
        if (count < byte.MaxValue)
        {
            ++count;
        }
        instance.ConsecutiveFaults[index] = count;
        if (count < ConsecutiveFaultsToDisable)
        {
            return false;
        }
        instance.DisabledMask |= Bit(callback);
        MarkFaulted(instance, callback);
        NativeBridge.Log(LogLevel.Warn, className + "." + callback + " disabled after " + count + " consecutive faults");
        return true;
    }

    public static void Succeeded(ScriptBehaviour instance, ScriptCallback callback)
    {
        instance.ConsecutiveFaults[(int)callback] = 0;
    }

    private static unsafe void MarkFaulted(ScriptBehaviour instance, ScriptCallback callback)
    {
        NativeApi* api = NativeBridge.Api;
        if (api != null && api->ScriptMarkFaulted != null && instance.Slot >= 0)
        {
            api->ScriptMarkFaulted(instance.Slot, (byte)callback);
        }
    }
}
