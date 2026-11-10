using Conjure;

namespace Conjure.TestScripts;

/// <summary>
/// Hot reload subject. Serialised fields survive a reload, private state resets, OnCreate runs again,
/// OnStart does not. V2 adds a field; Vanishing exists in V1 and V3 only.
/// </summary>
public sealed class ReloadProbe : ScriptBehaviour
{
    public int Health = 10;
    public string Label = "v1";
    public int CreateCalls;
    public int StartCalls;
    public int Updates;
    public int SecretView;

#if V2
    public int OnlyInV2 = 7;
#endif

    private int _secret = 42;

    protected override void OnCreate()
    {
        ++CreateCalls;
    }

    protected override void OnStart()
    {
        ++StartCalls;
        _secret = 99;
    }

    protected override void OnUpdate(float deltaTime)
    {
        ++Updates;
        SecretView = _secret;
    }
}

#if V1 || V3
public sealed class Vanishing : ScriptBehaviour
{
    public int Updates;

    protected override void OnUpdate(float deltaTime)
    {
        ++Updates;
    }
}
#endif

/// <summary>
/// Reports which variant was compiled so a test can prove the reload swapped assemblies.
/// </summary>
public sealed class VariantProbe : ScriptBehaviour
{
    public int Variant;

    protected override void OnCreate()
    {
#if V3
        Variant = 3;
#elif V2
        Variant = 2;
#else
        Variant = 1;
#endif
    }
}
