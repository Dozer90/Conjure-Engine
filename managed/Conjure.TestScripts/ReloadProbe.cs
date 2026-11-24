using System.Runtime.InteropServices;
using Conjure;

namespace Conjure.TestScripts;

/// <summary>
/// Hot reload subject. Serialised fields survive a reload, private state resets, OnCreate and OnStart do not run
/// again and OnReload runs instead. V2 adds a field; Vanishing exists in V1 and V3 only.
/// </summary>
public sealed class ReloadProbe : ScriptBehaviour
{
    public int Health = 10;
    public string Label = "v1";
    public int CreateCalls;
    public int ReloadCalls;
    public int StartCalls;
    public int Updates;
    public int SecretView;
    public int SecretAtReload;

#if V2
    public int OnlyInV2 = 7;
#endif

    private int _secret = 42;

    protected override void OnCreate()
    {
        ++CreateCalls;
    }

    protected override void OnReload()
    {
        ++ReloadCalls;
        SecretAtReload = _secret;
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
/// Reports which variant was compiled so a test can prove the reload swapped assemblies: OnCreate writes it on a
/// new instance, OnReload on one a reload recreated (whose restored field still names the previous variant).
/// </summary>
public sealed class VariantProbe : ScriptBehaviour
{
    public int Variant;

    protected override void OnCreate()
    {
        Variant = CompiledVariant();
    }

    protected override void OnReload()
    {
        Variant = CompiledVariant();
    }

    private static int CompiledVariant()
    {
#if V3
        return 3;
#elif V2
        return 2;
#else
        return 1;
#endif
    }
}

/// <summary>
/// The one-shot sound a CoinProbe plays when it is created.
/// </summary>
[EcsComponent]
[StructLayout(LayoutKind.Sequential)]
public struct CoinChime
{
    public float Pitch;
}

/// <summary>
/// The owner's blue coin (code sweep OWN-RELOAD): OnCreate adds a one-shot CoinChime without a HasComponent guard,
/// and the first update plays it and removes it. A hot reload recreates the instance without running OnCreate
/// again, so the chime does not come back and plays only once.
/// </summary>
public sealed class CoinProbe : ScriptBehaviour
{
    public int CreateCalls;
    public int ReloadCalls;
    public int ChimesPlayed;

    protected override void OnCreate()
    {
        ++CreateCalls;
        AddComponent(new CoinChime { Pitch = 1.5f });
    }

    protected override void OnReload()
    {
        ++ReloadCalls;
    }

    protected override void OnUpdate(float deltaTime)
    {
        if (RemoveComponent<CoinChime>())
        {
            ++ChimesPlayed;
        }
    }
}
