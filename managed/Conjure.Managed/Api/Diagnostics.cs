using System;
using System.Numerics;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// One frame's timing sample (engine::frame_sample as diagnostics_frame_stats reports it).
/// </summary>
public readonly unsafe struct FrameStats
{
    private readonly FrameStatsBlock _block;

    internal FrameStats(in FrameStatsBlock block)
    {
        _block = block;
    }

    public const int PhaseCount = 10;

    public float FrameMs => _block.FrameMs;

    public float SubmitMs => _block.SubmitMs;

    public uint FixedSteps => _block.FixedSteps;

    public uint JobsExecuted => _block.JobsExecuted;

    public ulong FrameMemoryHighWater => _block.FrameMemoryHighWater;

    /// <summary>
    /// Milliseconds spent in engine phase 'phase' (0..9); ArgumentOutOfRangeException otherwise.
    /// </summary>
    public float PhaseMs(int phase)
    {
        if (phase < 0 || phase >= PhaseCount)
        {
            throw new ArgumentOutOfRangeException(nameof(phase));
        }
        FrameStatsBlock copy = _block;
        return copy.PhaseMs[phase];
    }
}

/// <summary>
/// Scripted diagnostics through the diagnostics_* entries (docs/design/15_bindings.md 2.5, B14): checks
/// that land in the engine's check registry (the feature tests read them), debug drawing (a silent no-op
/// without a drawer), frame statistics and the native call counters.
/// </summary>
public static unsafe class Diagnostics
{
    /// <summary>
    /// Records a named check with the current frame (ArgumentException for an empty name).
    /// </summary>
    public static void Check(string name, bool passed, string detail = "")
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->DiagnosticsCheck, NativeFunction.DiagnosticsCheck);
        Span<byte> nameScratch = stackalloc byte[Utf8Marshal.ScratchSize];
        Span<byte> detailScratch = stackalloc byte[Utf8Marshal.ScratchSize];
        Utf8Encoded encodedName = Utf8Marshal.Encode(name, nameScratch);
        Utf8Encoded encodedDetail = Utf8Marshal.Encode(detail, detailScratch);
        fixed (byte* pinnedName = encodedName.Bytes)
        fixed (byte* pinnedDetail = encodedDetail.Bytes)
        {
            int status = api->DiagnosticsCheck(
                pinnedName, encodedName.Length, passed ? 1 : 0, pinnedDetail, encodedDetail.Length);
            NativeCheck.ThrowIfFailed(status, NativeFunction.DiagnosticsCheck);
        }
    }

    /// <summary>
    /// The main thread's last native error: the message ("" when none) and its status (0 when none).
    /// </summary>
    public static string LastError(out int status) => NativeCheck.ReadLastError(out status);

    /// <summary>
    /// The latest frame sample (NotSupportedException when the engine keeps no statistics).
    /// </summary>
    public static FrameStats Stats
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->DiagnosticsFrameStats, NativeFunction.DiagnosticsFrameStats);
            FrameStatsBlock block = default;
            NativeCheck.ThrowIfFailed(api->DiagnosticsFrameStats(&block), NativeFunction.DiagnosticsFrameStats);
            return new FrameStats(in block);
        }
    }

    /// <summary>
    /// How many times the native entry has been called since the counters were last reset
    /// (NotSupportedException in a Distribution build, which keeps no counters).
    /// </summary>
    public static int CallCount(NativeFunction function)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->DiagnosticsCallCount, NativeFunction.DiagnosticsCallCount);
        int status = api->DiagnosticsCallCount((int)function);
        NativeCheck.ThrowIfFailed(status, NativeFunction.DiagnosticsCallCount);
        return status;
    }

    /// <summary>
    /// Debug drawing in world space; 'seconds' of 0 means this frame only. Colours are packed RGBA8.
    /// </summary>
    public static class Draw
    {
        public static void Line(Vector3 a, Vector3 b, Color color, float seconds = 0.0f)
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->DiagnosticsDrawLine, NativeFunction.DiagnosticsDrawLine);
            NativeCheck.ThrowIfFailed(api->DiagnosticsDrawLine(&a, &b, Pack(color), seconds), NativeFunction.DiagnosticsDrawLine);
        }

        public static void Box(Vector3 center, Vector3 halfExtents, Quaternion rotation, Color color, float seconds = 0.0f)
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->DiagnosticsDrawBox, NativeFunction.DiagnosticsDrawBox);
            NativeCheck.ThrowIfFailed(
                api->DiagnosticsDrawBox(&center, &halfExtents, &rotation, Pack(color), seconds),
                NativeFunction.DiagnosticsDrawBox);
        }

        public static void Sphere(Vector3 center, float radius, Color color, float seconds = 0.0f)
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->DiagnosticsDrawSphere, NativeFunction.DiagnosticsDrawSphere);
            NativeCheck.ThrowIfFailed(
                api->DiagnosticsDrawSphere(&center, radius, Pack(color), seconds),
                NativeFunction.DiagnosticsDrawSphere);
        }

        public static void Text(Vector3 position, string text, Color color, float seconds = 0.0f)
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->DiagnosticsDrawText, NativeFunction.DiagnosticsDrawText);
            Span<byte> scratch = stackalloc byte[Utf8Marshal.ScratchSize];
            Utf8Encoded encoded = Utf8Marshal.Encode(text, scratch);
            fixed (byte* pinned = encoded.Bytes)
            {
                NativeCheck.ThrowIfFailed(
                    api->DiagnosticsDrawText(&position, pinned, encoded.Length, Pack(color), seconds),
                    NativeFunction.DiagnosticsDrawText);
            }
        }

        /// <summary>
        /// RGBA8 with red in the top byte (marshal::rgba_to_color).
        /// </summary>
        public static uint Pack(Color color)
        {
            return ((uint)Channel(color.R) << 24) | ((uint)Channel(color.G) << 16) | ((uint)Channel(color.B) << 8) | Channel(color.A);
        }

        private static byte Channel(float value)
        {
            if (!(value > 0.0f))
            {
                return 0;
            }
            if (value >= 1.0f)
            {
                return 255;
            }
            return (byte)(value * 255.0f + 0.5f);
        }
    }
}
