using System;
using System.Diagnostics;
using System.Runtime;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

namespace Conjure.Interop;

/// <summary>
/// Log levels of the native log_write entry (core::log_level order).
/// </summary>
public enum LogLevel : byte
{
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
}

/// <summary>
/// The native table, the shared frame block and the handshake (docs/design/03_scripting.md 3.1 step 10).
/// Initialize is idempotent: every script_host::initialise re-validates and refills.
/// </summary>
public static unsafe class NativeBridge
{
    private const int HandshakeOk = 0;
    private const int HandshakeNullTable = 1;
    private const int HandshakeSizeMismatch = 2;
    private const int HandshakeVersionMismatch = 3;
    private const int HandshakeHashMismatch = 4;
    private const int HandshakeRecordMismatch = 5;
    private const int HandshakeFailure = 99;

    private static bool _unhandledHooked;

    /// <summary>
    /// The native table; null before the handshake.
    /// </summary>
    public static NativeApi* Api { get; private set; }

    /// <summary>
    /// The per-frame block native writes before FRAME_BEGIN.
    /// </summary>
    public static FrameBlock* Frame => Api == null ? null : Api->Frame;

    /// <summary>
    /// The thread the handshake ran on: the only thread that may call native (D10).
    /// </summary>
    public static int MainThreadId { get; private set; } = -1;

    /// <summary>
    /// Entries native left null at the last handshake (their facades throw NotSupportedException).
    /// </summary>
    public static int MissingEntryCount { get; private set; }

    /// <summary>
    /// Bumped by every successful handshake and every Reset: caches keyed on the native table or its
    /// registry (ComponentType&lt;T&gt;) re-resolve when it changed.
    /// </summary>
    public static int Generation { get; private set; }

    public static bool IsInitialized => Api != null;

    /// <summary>
    /// The handshake: validates the native header and record layouts, records the main thread and fills
    /// the managed table. Returns 0, or a code native reports as ABI_MISMATCH.
    /// </summary>
    [UnmanagedCallersOnly]
    public static int Initialize(NativeApi* native, ManagedApi* managed)
    {
        try
        {
            if (managed != null)
            {
                ManagedEntryPoints.Fill(managed);
            }
            if (native == null || managed == null)
            {
                return HandshakeNullTable;
            }
            if (native->Size != sizeof(NativeApi))
            {
                return HandshakeSizeMismatch;
            }
            if (native->Version != NativeApi.ExpectedVersion)
            {
                return HandshakeVersionMismatch;
            }
            if (native->LayoutHash != NativeApi.ExpectedLayoutHash)
            {
                return HandshakeHashMismatch;
            }
            if (!RecordSizes.AllMatch(out string mismatch))
            {
                Api = native;
                Log(LogLevel.Error, "ABI record '" + mismatch + "' has an unexpected managed layout");
                Api = null;
                return HandshakeRecordMismatch;
            }

            Api = native;
            MainThreadId = Environment.CurrentManagedThreadId;
            ++Generation;
            int missing = 0;
            for (int index = 0; index < NativeApi.EntryCount; ++index)
            {
                if (native->Pointer((NativeFunction)index) == null)
                {
                    ++missing;
                }
            }
            MissingEntryCount = missing;

            GCSettings.LatencyMode = GCLatencyMode.SustainedLowLatency;
            if (!_unhandledHooked)
            {
                _unhandledHooked = true;
                AppDomain.CurrentDomain.UnhandledException += OnUnhandledException;
            }
            WaitForDebuggerIfRequested();
            Log(LogLevel.Debug, "Conjure.Managed handshake complete (" + NativeApi.EntryCount + " entries, " + missing + " unfilled)");
            return HandshakeOk;
        }
        catch (Exception exception)
        {
            try
            {
                Log(LogLevel.Error, "Initialize failed: " + exception);
            }
            catch
            {
                // Nothing else can report it; native maps the status to ABI_MISMATCH.
            }
            return HandshakeFailure;
        }
    }

    /// <summary>
    /// Forgets the native table (ScriptRuntime.Shutdown): the memory behind it belongs to a host that may
    /// be gone, so every facade throws "not initialised" and Log drops messages until the next handshake.
    /// </summary>
    internal static void Reset()
    {
        Api = null;
        MainThreadId = -1;
        MissingEntryCount = 0;
        ++Generation;
    }

    /// <summary>
    /// Throws when called off the main thread (D10).
    /// </summary>
    public static void EnsureMainThread()
    {
        if (Environment.CurrentManagedThreadId != MainThreadId)
        {
            throw new InvalidOperationException("Conjure scripting APIs may only be used from the main thread");
        }
    }

    /// <summary>
    /// Throws when the runtime was never initialised.
    /// </summary>
    public static NativeApi* RequireApi()
    {
        if (Api == null)
        {
            throw new InvalidOperationException("the native scripting table is not initialised");
        }
        EnsureMainThread();
        return Api;
    }

    /// <summary>
    /// The NotSupportedException raised for an entry native did not provide (D11).
    /// </summary>
    public static NotSupportedException NotProvided(NativeFunction function)
    {
        return new NotSupportedException("entry " + NativeFunctionNames.Of(function) + " not provided");
    }

    /// <summary>
    /// Throws NotProvided when 'pointer' (the entry's function pointer) is null.
    /// </summary>
    public static void Require(void* pointer, NativeFunction function)
    {
        if (pointer == null)
        {
            throw NotProvided(function);
        }
    }

    /// <summary>
    /// Sends a message to the engine log (category "scripting"). Safe on any thread when native provides
    /// log_write; otherwise the message is dropped.
    /// </summary>
    public static void Log(LogLevel level, string text)
    {
        NativeApi* api = Api;
        if (api == null || api->LogWrite == null || text == null)
        {
            return;
        }
        int byteCount = Encoding.UTF8.GetByteCount(text);
        if (byteCount <= 512)
        {
            byte* buffer = stackalloc byte[512];
            int written = Encoding.UTF8.GetBytes(text, new Span<byte>(buffer, 512));
            api->LogWrite((byte)level, buffer, written);
            return;
        }
        byte[] bytes = Encoding.UTF8.GetBytes(text);
        fixed (byte* pinned = bytes)
        {
            api->LogWrite((byte)level, pinned, bytes.Length);
        }
    }

    /// <summary>
    /// A UTF-8 (pointer, length) pair as a string; "" for a null pointer.
    /// </summary>
    public static string ReadUtf8(byte* utf8, int length)
    {
        if (utf8 == null || length <= 0)
        {
            return "";
        }
        return Encoding.UTF8.GetString(utf8, length);
    }

    private static void OnUnhandledException(object sender, UnhandledExceptionEventArgs arguments)
    {
        Log(LogLevel.Fatal, "unhandled managed exception: " + arguments.ExceptionObject);
    }

    private static void WaitForDebuggerIfRequested()
    {
        object? requested = AppContext.GetData("CONJURE_WAIT_FOR_DEBUGGER");
        if (requested is not string text || text != "1")
        {
            return;
        }
        Log(LogLevel.Info, "waiting up to 60 s for a managed debugger to attach");
        Stopwatch watch = Stopwatch.StartNew();
        while (!Debugger.IsAttached && watch.Elapsed.TotalSeconds < 60.0)
        {
            Thread.Sleep(100);
        }
    }
}
