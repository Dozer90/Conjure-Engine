using System;
using System.Threading.Tasks;
using Conjure;

namespace Conjure.TestScripts;

/// <summary>
/// Calls a native-backed API from a thread pool thread and records the exception type.
/// </summary>
public sealed class WrongThreadProbe : ScriptBehaviour
{
    public string Caught = "";

    protected override void OnUpdate(float deltaTime)
    {
        if (Caught.Length > 0)
        {
            return;
        }
        Entity entity = Entity;
        Task<string> task = Task.Run(() =>
        {
            try
            {
                _ = entity.IsAlive;     // off the main thread this throws InvalidOperationException
                return "none";
            }
            catch (Exception exception)
            {
                return exception.GetType().Name;
            }
        });
        task.Wait();
        Caught = task.Result;
    }
}

/// <summary>
/// Calls an entry the bindings have not provided and records the exception type.
/// </summary>
public sealed class NullEntryProbe : ScriptBehaviour
{
    public string Caught = "";

    protected override void OnUpdate(float deltaTime)
    {
        if (Caught.Length > 0)
        {
            return;
        }
        try
        {
            _ = Physics.Gravity;
            Caught = "none";
        }
        catch (Exception exception)
        {
            Caught = exception.GetType().Name;
        }
    }
}

/// <summary>
/// Reads the frame block through Time and Input.
/// </summary>
public sealed class FrameProbe : ScriptBehaviour
{
    public float LastDelta;
    public long LastFrame;
    public int SpaceDown;
    public int CurrentPhase;

    protected override void OnUpdate(float deltaTime)
    {
        LastDelta = Time.DeltaTime;
        LastFrame = (long)Time.FrameIndex;
        SpaceDown = Input.IsKeyDown(Key.Space) ? 1 : 0;
        CurrentPhase = (int)Time.CurrentPhase;
    }
}
