using System;
using Conjure;

namespace Game;

/// <summary>
/// The one script of the scripted export project: it logs a line when the runtime creates it, which the export cases
/// and the nightly export look for in the exported runtime's log to tell that the shipped scripts ran. It also uses
/// features of C# 14, the language version scripts/Game.csproj pins (an older one fails the export's build of the
/// scripts), and logs what they computed, which the editor's export case checks.
/// </summary>
public sealed class ExportProbe : ScriptBehaviour
{
    private int Lives
    {
        get;
        set => field = Math.Max(value, 0);
    }

    protected override void OnCreate()
    {
        Log.Info("ExportProbe ran in the exported game");
        Lives = -3;
        Log.Info("ExportProbe C# 14: lives " + Lives + ", seven squared " + 7.Squared);
    }
}

/// <summary>
/// The extension block ExportProbe uses.
/// </summary>
public static class ExportProbeExtensions
{
    extension(int value)
    {
        /// <summary>The value times itself.</summary>
        public int Squared => value * value;
    }
}
