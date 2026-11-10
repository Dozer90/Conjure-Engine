namespace Conjure.Bootstrap;

/// <summary>
/// Never executed: the engine hosts the runtime through hostfxr and resolves
/// Conjure.Interop.NativeBridge.Initialize itself. The entry point exists so the project is an
/// application with a runtimeconfig.json that can be published framework-dependent or self-contained.
/// </summary>
public static class Program
{
    public static int Main()
    {
        return 0;
    }
}
