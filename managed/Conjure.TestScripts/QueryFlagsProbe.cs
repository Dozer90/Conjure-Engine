using System;
using System.Numerics;
using Conjure;

namespace Conjure.TestScripts;

/// <summary>
/// The Physics query overloads with and without QueryFlags over a ground plane and a trigger volume centred at
/// (0, 3, 0): what a ray straight down from (0, 10, 0) and a small sphere at the volume's centre find.
/// </summary>
public sealed class QueryFlagsProbe : ScriptBehaviour
{
    public long DefaultHit;
    public int DefaultHitIsTrigger = -1;
    public long TriggerHit;
    public int TriggerHitIsTrigger = -1;
    public int DefaultCastHitIsTrigger = -1;
    public int TriggerCastHitIsTrigger = -1;
    public int DefaultOverlaps = -1;
    public int TriggerOverlaps = -1;
    public int IgnoredOverlaps = -1;
    public int DefaultBoxOverlaps = -1;
    public int TriggerBoxOverlaps = -1;
    public int IgnoredBoxOverlaps = -1;
    public int StaticExcludedOverlaps = -1;
    public string UnknownFlagThrows = "";
    public int Ran;

    protected override void OnUpdate(float deltaTime)
    {
        if (Ran > 0)
        {
            return;
        }
        Ran = 1;
        Vector3 origin = new Vector3(0.0f, 10.0f, 0.0f);
        Vector3 down = new Vector3(0.0f, -1.0f, 0.0f);
        Vector3 centre = new Vector3(0.0f, 3.0f, 0.0f);

        if (Physics.Raycast(origin, down, 100.0f, out RaycastHit hit))
        {
            DefaultHit = (long)hit.Entity.Id;
            DefaultHitIsTrigger = hit.IsTrigger ? 1 : 0;
        }
        if (Physics.Raycast(origin, down, 100.0f, out RaycastHit triggerHit, QueryFlags.IncludeTriggers))
        {
            TriggerHit = (long)triggerHit.Entity.Id;
            TriggerHitIsTrigger = triggerHit.IsTrigger ? 1 : 0;
        }
        if (Physics.SphereCast(origin, 0.25f, down, 100.0f, out RaycastHit castHit))
        {
            DefaultCastHitIsTrigger = castHit.IsTrigger ? 1 : 0;
        }
        if (Physics.SphereCast(origin, 0.25f, down, 100.0f, out RaycastHit triggerCastHit, QueryFlags.IncludeTriggers))
        {
            TriggerCastHitIsTrigger = triggerCastHit.IsTrigger ? 1 : 0;
        }

        Span<Entity> found = stackalloc Entity[4];
        DefaultOverlaps = Physics.OverlapSphere(centre, 0.2f, found);
        TriggerOverlaps = Physics.OverlapSphere(centre, 0.2f, found, QueryFlags.IncludeTriggers);
        Entity trigger = TriggerOverlaps > 0 ? found[0] : default;
        IgnoredOverlaps = Physics.OverlapSphere(centre, 0.2f, found, QueryFlags.IncludeTriggers, null, trigger);
        Vector3 boxHalfExtents = new Vector3(0.2f);
        DefaultBoxOverlaps = Physics.OverlapBox(centre, boxHalfExtents, Quaternion.Identity, found);
        TriggerBoxOverlaps = Physics.OverlapBox(
            centre, boxHalfExtents, Quaternion.Identity, found, QueryFlags.IncludeTriggers);
        IgnoredBoxOverlaps = Physics.OverlapBox(
            centre, boxHalfExtents, Quaternion.Identity, found, QueryFlags.IncludeTriggers, null, trigger);
        StaticExcludedOverlaps = Physics.OverlapBox(
            centre, boxHalfExtents, Quaternion.Identity, found,
            QueryFlags.IncludeTriggers | QueryFlags.ExcludeStatic);

        try
        {
            Physics.Raycast(origin, down, 100.0f, out _, (QueryFlags)8);
            UnknownFlagThrows = "none";
        }
        catch (Exception exception)
        {
            UnknownFlagThrows = exception.GetType().Name;
        }
    }
}
