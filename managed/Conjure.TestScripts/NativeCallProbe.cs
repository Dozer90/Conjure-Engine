using Conjure;

namespace Conjure.TestScripts;

/// <summary>
/// The native-call overhead workload of docs/design/08_testing.md 7.1 (scripting.native_call_100k):
/// 100,000 Entity.IsAlive calls per update on the probe's own entity. IsAlive is the API's spelling of
/// the design row's Entity.IsValid, the cheapest native-backed member of Entity (one bridge call, one
/// generation check). The result is kept so the loop cannot be dropped.
/// </summary>
public sealed class NativeCallProbe : ScriptBehaviour
{
    public const int CallsPerUpdate = 100_000;

    public int Updates;

    public int AliveCount;

    protected override void OnUpdate(float deltaTime)
    {
        Entity entity = Entity;
        int alive = 0;
        for (int index = 0; index < CallsPerUpdate; ++index)
        {
            if (entity.IsAlive)
            {
                ++alive;
            }
        }
        AliveCount = alive;
        ++Updates;
    }
}
