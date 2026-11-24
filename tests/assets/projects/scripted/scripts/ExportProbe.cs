using Conjure;

namespace Game;

/// <summary>
/// The one script of the scripted export project: it logs a line when the runtime creates it, which the export cases
/// and the nightly export look for in the exported runtime's log to tell that the shipped scripts ran.
/// </summary>
public sealed class ExportProbe : ScriptBehaviour
{
    protected override void OnCreate()
    {
        Log.Info("ExportProbe ran in the exported game");
    }
}
