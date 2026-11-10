using Conjure.Interop;

namespace Conjure;

/// <summary>
/// Frame timing, read from the shared frame block (D4).
/// </summary>
public static unsafe class Time
{
    private static FrameBlock* Frame => NativeBridge.Frame;

    /// <summary>
    /// Seconds since the engine started, scaled by TimeScale.
    /// </summary>
    public static double Seconds => Frame == null ? 0.0 : Frame->TimeSeconds;

    public static double UnscaledSeconds => Frame == null ? 0.0 : Frame->UnscaledTimeSeconds;

    /// <summary>
    /// Seconds of the current frame (or of the fixed step inside OnFixedUpdate).
    /// </summary>
    public static float DeltaTime => Frame == null ? 0.0f : Frame->DeltaTime;

    public static float FixedDeltaTime => Frame == null ? 0.0f : Frame->FixedDeltaTime;

    public static float TimeScale => Frame == null ? 1.0f : Frame->TimeScale;

    public static ulong FrameIndex => Frame == null ? 0 : Frame->FrameIndex;

    public static uint FixedStepIndex => Frame == null ? 0 : Frame->FixedStepIndex;

    /// <summary>
    /// The phase being run (ScriptPhase.Count outside a script phase).
    /// </summary>
    public static ScriptPhase CurrentPhase => Frame == null ? ScriptPhase.Count : (ScriptPhase)Frame->CurrentPhase;

    public static bool InPlayMode => Frame != null && Frame->InPlayMode != 0;
}
