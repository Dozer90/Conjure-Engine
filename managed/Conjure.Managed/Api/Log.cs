using Conjure.Interop;

namespace Conjure;

/// <summary>
/// The engine log (category "scripting").
/// </summary>
public static class Log
{
    public static void Trace(string message) => NativeBridge.Log(LogLevel.Trace, message);

    public static void Debug(string message) => NativeBridge.Log(LogLevel.Debug, message);

    public static void Info(string message) => NativeBridge.Log(LogLevel.Info, message);

    public static void Warn(string message) => NativeBridge.Log(LogLevel.Warn, message);

    public static void Error(string message) => NativeBridge.Log(LogLevel.Error, message);
}
