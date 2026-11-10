using Conjure;

namespace Conjure.TestScripts;

/// <summary>
/// Attributes the inspector reads from the field metadata.
/// </summary>
public sealed class InspectorProbe : ScriptBehaviour
{
    [Range(0.0, 10.0)]
    [Tooltip("Speed in metres per second")]
    public float Speed = 3.0f;

    public Mode Current = Mode.Run;

    [HideInInspector]
    public int Internal = 4;

    public int Updates;

    protected override void OnUpdate(float deltaTime)
    {
        ++Updates;
    }
}
