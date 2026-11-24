using System;
using System.Runtime.InteropServices;
using Conjure.Runtime;

namespace Conjure.Interop;

/// <summary>
/// Mirror of conjure_managed_api: the functions managed provides to native, filled by
/// NativeBridge.Initialize with the entry points below. The native members are part of
/// NativeApi.ExpectedLayoutHash (sweep F445), so a build of this assembly older than a change of
/// conjure_managed_api fails the handshake; of this hand-written mirror only its size is checked
/// (validate_managed_api), so a same-size reorder or retype goes unnoticed: change it together with
/// native_api.h.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct ManagedApi
{
    public uint Size;
    public uint Version;
    public ulong LayoutHash;
    public delegate* unmanaged<byte*, int, int> LoadGameAssembly;
    public delegate* unmanaged<int> UnloadGameAssembly;
    public delegate* unmanaged<CreateRequest*, int, int> CreateInstances;
    public delegate* unmanaged<int*, int, int, int> DestroyInstances;
    public delegate* unmanaged<byte, float, int> RunPhase;
    public delegate* unmanaged<CollisionRecord*, int, int> DispatchCollisions;
    public delegate* unmanaged<int, byte*, int, int> ApplyFields;
    public delegate* unmanaged<int, byte*, int, int> CollectFields;
    public delegate* unmanaged<int> CollectGarbage;
    public delegate* unmanaged<int> Shutdown;
}

/// <summary>
/// The [UnmanagedCallersOnly] entry points (D9: they exist only in Conjure.Managed). Every one catches
/// every exception (D11): a failure is logged through native and reported as a negative status, never
/// thrown across the boundary.
/// </summary>
internal static unsafe class ManagedEntryPoints
{
    private const int StatusFailure = -1;

    [UnmanagedCallersOnly]
    public static int LoadGameAssembly(byte* utf8Path, int length)
    {
        try
        {
            string path = NativeBridge.ReadUtf8(utf8Path, length);
            return ScriptRuntime.LoadGameAssembly(path);
        }
        catch (Exception exception)
        {
            NativeBridge.Log(LogLevel.Error, "LoadGameAssembly failed: " + exception);
            return StatusFailure;
        }
    }

    [UnmanagedCallersOnly]
    public static int UnloadGameAssembly()
    {
        try
        {
            return ScriptRuntime.UnloadGameAssembly();
        }
        catch (Exception exception)
        {
            NativeBridge.Log(LogLevel.Error, "UnloadGameAssembly failed: " + exception);
            return StatusFailure;
        }
    }

    [UnmanagedCallersOnly]
    public static int CreateInstances(CreateRequest* requests, int count)
    {
        try
        {
            return ScriptRuntime.CreateInstances(requests, count);
        }
        catch (Exception exception)
        {
            NativeBridge.Log(LogLevel.Error, "CreateInstances failed: " + exception);
            return StatusFailure;
        }
    }

    [UnmanagedCallersOnly]
    public static int DestroyInstances(int* slots, int count, int callOnDestroy)
    {
        try
        {
            return ScriptRuntime.DestroyInstances(slots, count, callOnDestroy != 0);
        }
        catch (Exception exception)
        {
            NativeBridge.Log(LogLevel.Error, "DestroyInstances failed: " + exception);
            return StatusFailure;
        }
    }

    [UnmanagedCallersOnly]
    public static int RunPhase(byte phase, float deltaTime)
    {
        try
        {
            return ScriptRuntime.RunPhase((ScriptPhase)phase, deltaTime);
        }
        catch (Exception exception)
        {
            NativeBridge.Log(LogLevel.Error, "RunPhase failed: " + exception);
            return StatusFailure;
        }
    }

    [UnmanagedCallersOnly]
    public static int DispatchCollisions(CollisionRecord* records, int count)
    {
        try
        {
            return ScriptRuntime.DispatchCollisions(records, count);
        }
        catch (Exception exception)
        {
            NativeBridge.Log(LogLevel.Error, "DispatchCollisions failed: " + exception);
            return StatusFailure;
        }
    }

    [UnmanagedCallersOnly]
    public static int ApplyFields(int slot, byte* blob, int size)
    {
        try
        {
            return ScriptRuntime.ApplyFields(slot, new ReadOnlySpan<byte>(blob, size));
        }
        catch (Exception exception)
        {
            NativeBridge.Log(LogLevel.Error, "ApplyFields failed: " + exception);
            return StatusFailure;
        }
    }

    [UnmanagedCallersOnly]
    public static int CollectFields(int slot, byte* buffer, int capacity)
    {
        try
        {
            return ScriptRuntime.CollectFields(slot, new Span<byte>(buffer, capacity));
        }
        catch (Exception exception)
        {
            NativeBridge.Log(LogLevel.Error, "CollectFields failed: " + exception);
            return StatusFailure;
        }
    }

    [UnmanagedCallersOnly]
    public static int CollectGarbage()
    {
        try
        {
            return ScriptRuntime.CollectGarbage();
        }
        catch (Exception exception)
        {
            NativeBridge.Log(LogLevel.Error, "CollectGarbage failed: " + exception);
            return StatusFailure;
        }
    }

    [UnmanagedCallersOnly]
    public static int Shutdown()
    {
        try
        {
            return ScriptRuntime.Shutdown();
        }
        catch (Exception exception)
        {
            NativeBridge.Log(LogLevel.Error, "Shutdown failed: " + exception);
            return StatusFailure;
        }
    }

    /// <summary>
    /// Fills the table native reads.
    /// </summary>
    public static void Fill(ManagedApi* api)
    {
        api->Size = (uint)sizeof(ManagedApi);
        api->Version = NativeApi.ExpectedVersion;
        api->LayoutHash = NativeApi.ExpectedLayoutHash;
        api->LoadGameAssembly = &LoadGameAssembly;
        api->UnloadGameAssembly = &UnloadGameAssembly;
        api->CreateInstances = &CreateInstances;
        api->DestroyInstances = &DestroyInstances;
        api->RunPhase = &RunPhase;
        api->DispatchCollisions = &DispatchCollisions;
        api->ApplyFields = &ApplyFields;
        api->CollectFields = &CollectFields;
        api->CollectGarbage = &CollectGarbage;
        api->Shutdown = &Shutdown;
    }
}
