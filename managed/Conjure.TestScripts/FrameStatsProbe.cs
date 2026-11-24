using Conjure;

namespace Conjure.TestScripts;

/// <summary>
/// Allocates AllocateBytes on the managed heap in every update and records what Diagnostics.Stats reports for
/// the frame before: the managed figures of FrameStats end to end.
/// </summary>
public sealed class FrameStatsProbe : ScriptBehaviour
{
    public int AllocateBytes;
    public long LastAllocatedBytes = -1;
    public long LastHeapBytes = -1;
    public long LastCollections = -1;
    public int Updates;
    public int KeptLength;

    // Holds the latest block, so the allocation escapes to the heap and survives the update.
    private byte[]? _kept;

    protected override void OnUpdate(float deltaTime)
    {
        KeptLength = _kept?.Length ?? 0;
        FrameStats stats = Diagnostics.Stats;
        LastAllocatedBytes = (long)stats.ManagedAllocatedBytes;
        LastHeapBytes = (long)stats.ManagedHeapBytes;
        LastCollections = stats.ManagedCollections;
        ++Updates;
        if (AllocateBytes > 0)
        {
            _kept = new byte[AllocateBytes];
        }
    }
}
