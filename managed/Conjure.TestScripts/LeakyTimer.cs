using System.Threading;
using Conjure;

namespace Conjure.TestScripts;

/// <summary>
/// Keeps its load context alive on purpose when Arm is set: a static Timer rooted by the timer queue
/// references the instance and therefore the assembly (the documented .NET unload limitation).
/// </summary>
public sealed class LeakyTimer : ScriptBehaviour
{
    public bool Arm;
    public int Armed;

    private static Timer? _timer;

    protected override void OnCreate()
    {
        if (!Arm)
        {
            return;
        }
        _timer = new Timer(_ => { }, this, 1000, 1000);
        Armed = 1;
    }
}
