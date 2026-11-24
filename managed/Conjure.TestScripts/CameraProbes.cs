using System;
using System.Numerics;
using Conjure;
using Conjure.Components;

namespace Conjure.TestScripts;

// Probes of the Camera, Screen and picking facades (wave 3 GAP-CSHARP 2.2), driven by
// tests/bindings/test_camera_managed.cpp inside a headless application with the 1280 x 720 null window. Numbers
// are reported in thousandths (ProbeSupport.Join) and exceptions by type name (ProbeSupport.Catch).

/// <summary>
/// Asks Camera.Main before any camera exists, then makes its own entity a Grid-Sweeper-style 2D camera
/// (orthographic, 10 m high, at z = 10 looking down -z) in its first update and picks through it in the same
/// update: the cell under a pixel, the round trip back to the screen, Camera.Main and every refusal of the camera
/// entries and of Ray.
/// </summary>
public sealed class CameraPickingProbe : ScriptBehaviour
{
    /// <summary>
    /// The pixel whose world point is (4.5, 2.5) on the 1280 x 720 screen: x = (4.5 / (5 * 1280 / 720) + 1) * 640.
    /// </summary>
    public static readonly Vector2 CellPixel = new Vector2(964.0f, 180.0f);

    public string Errors = "";
    public int Ran;
    public int NoMainExists = -1;
    public string NoMainRay = "";
    public int IsMain;
    public int Exists;
    public int PlainExists = -1;
    public string Cell = "";
    public string CellIndex = "";
    public string BackToScreen = "";
    public int BackVisible = -1;
    public string CentreRay = "";
    public int CentreRayDone = -1;
    public int ParallelPlane = -1;
    public int PlaneBehind = -1;
    public string BehindTheCamera2D = "";
    public string DeadCamera = "";
    public string NoCameraComponent = "";
    public string NoTransform = "";
    public string NonFinite = "";
    public string BadCameraData = "";
    public string ZeroDirectionRay = "";
    public string NonFiniteRayOrigin = "";
    public string NonFiniteRayDirection = "";
    public string ExtremeRayDirections = "";

    protected override void OnUpdate(float deltaTime)
    {
        if (Ran > 0)
        {
            return;
        }
        Ran = 1;
        try
        {
            NoMain();
            MakeCamera();
            Pick();
            Refusals();
        }
        catch (Exception exception)
        {
            Errors += exception.GetType().Name + ": " + exception.Message + "\n";
        }
    }

    // No entity carries CameraSettings yet: Main is the null entity.
    private void NoMain()
    {
        NoMainExists = Camera.Main.Exists ? 1 : 0;
        NoMainRay = ProbeSupport.Catch(() => Camera.Main.ScreenPointToRay(Vector2.Zero, out _));
    }

    private void MakeCamera()
    {
        CameraSettings settings = CameraSettings.Default;
        settings.CameraData.Projection = ProjectionType.Orthographic;
        settings.CameraData.OrthographicHeight = 10.0f;
        settings.CameraData.FarPlane = 100.0f;
        AddComponent(in settings);
        MoveTo(new Vector3(0.0f, 0.0f, 10.0f));
    }

    private void Pick()
    {
        Camera camera = new Camera(Entity);
        IsMain = Camera.Main == camera ? 1 : 0;
        Exists = camera.Exists ? 1 : 0;

        Vector2 cell = camera.ScreenToWorld2D(CellPixel);
        Cell = ProbeSupport.Join(cell.X, cell.Y);
        CellIndex = (int)MathF.Floor(cell.X) + "," + (int)MathF.Floor(cell.Y);
        BackVisible = camera.WorldToScreen(new Vector3(cell, 0.0f), out Vector3 back) ? 1 : 0;
        BackToScreen = ProbeSupport.Axis(back);

        CentreRayDone = camera.ScreenPointToRay(Screen.Size * 0.5f, out Ray ray) ? 1 : 0;
        CentreRay = ProbeSupport.Axis(ray.Origin) + " " + ProbeSupport.Axis(ray.Direction);
        ParallelPlane = camera.ScreenToPlane(CellPixel, new Plane(Vector3.UnitX, 0.0f), out _) ? 1 : 0;
        PlaneBehind = camera.ScreenToPlane(CellPixel, new Plane(Vector3.UnitZ, -20.0f), out _) ? 1 : 0;

        // The same camera moved below the plane: z = 0 is now behind it.
        MoveTo(new Vector3(0.0f, 0.0f, -10.0f));
        BehindTheCamera2D = ProbeSupport.Catch(() => camera.ScreenToWorld2D(CellPixel));
        MoveTo(new Vector3(0.0f, 0.0f, 10.0f));
    }

    private void Refusals()
    {
        Camera camera = new Camera(Entity);
        DeadCamera = ProbeSupport.Catch(() => new Camera(ProbeSupport.Dead).ScreenPointToRay(Vector2.Zero, out _));
        Entity plain = Entity.Create("plain");
        PlainExists = new Camera(plain).Exists ? 1 : 0;
        NoCameraComponent =
            ProbeSupport.CatchWithMessage(() => new Camera(plain).WorldToScreen(Vector3.Zero, out _));
        plain.AddComponent(CameraSettings.Default);
        NoTransform = ProbeSupport.CatchWithMessage(() => new Camera(plain).ScreenToWorld(Vector2.Zero, 1.0f));
        NonFinite = ProbeSupport.Catch(() => camera.ScreenToWorld(new Vector2(float.NaN, 0.0f), 1.0f));

        CameraSettings saved = camera.Settings;
        CameraSettings broken = saved;
        broken.CameraData.NearPlane = 0.0f;
        camera.Settings = broken;
        BadCameraData = ProbeSupport.Catch(() => camera.ScreenPointToRay(Vector2.Zero, out _));
        camera.Settings = saved;

        ZeroDirectionRay = ProbeSupport.Catch(() => new Ray(Vector3.Zero, Vector3.Zero));
        NonFiniteRayOrigin = ProbeSupport.Catch(() => new Ray(new Vector3(float.NaN, 0.0f, 0.0f), Vector3.UnitZ));
        NonFiniteRayDirection =
            ProbeSupport.Catch(() => new Ray(Vector3.Zero, new Vector3(float.PositiveInfinity, 0.0f, 0.0f)));
        // Finite directions whose length underflows or overflows a float are still directions.
        Ray tiny = new Ray(Vector3.Zero, new Vector3(1.0e-30f, 0.0f, 0.0f));
        Ray huge = new Ray(Vector3.Zero, new Vector3(3.0e38f, 3.0e38f, 0.0f));
        ExtremeRayDirections = ProbeSupport.Axis(tiny.Direction) + " " + ProbeSupport.Axis(huge.Direction);
    }

    // Transform is a readonly facade: its setters run on a copy, which writes through to the entity.
    private void MoveTo(Vector3 position)
    {
        Transform pose = Transform;
        pose.Position = position;
    }
}

/// <summary>
/// A perspective camera (CameraSettings.Default) on its own entity, turned 90 degrees about +y in its first update
/// and asked for the centre ray in that same update: the ray follows the new pose before any transform system or
/// extraction ran. Also projects a point ahead of it and one behind it.
/// </summary>
public sealed class CameraTurnProbe : ScriptBehaviour
{
    public string Errors = "";
    public int Ran;
    public string Forward = "";
    public string AheadPixel = "";
    public int AheadVisible = -1;
    public int BehindVisible = -1;

    protected override void OnUpdate(float deltaTime)
    {
        if (Ran > 0)
        {
            return;
        }
        Ran = 1;
        try
        {
            AddComponent(CameraSettings.Default);
            Transform pose = Transform;
            pose.Rotation = Quaternion.CreateFromAxisAngle(Vector3.UnitY, MathF.PI * 0.5f);
            Camera camera = new Camera(Entity);
            camera.ScreenPointToRay(Screen.Size * 0.5f, out Ray ray);
            Forward = ProbeSupport.Axis(ray.Direction);
            AheadVisible = camera.WorldToScreen(new Vector3(-5.0f, 0.0f, 0.0f), out Vector3 ahead) ? 1 : 0;
            AheadPixel = ProbeSupport.Axis(ahead);
            BehindVisible = camera.WorldToScreen(new Vector3(5.0f, 0.0f, 0.0f), out _) ? 1 : 0;
        }
        catch (Exception exception)
        {
            Errors += exception.GetType().Name + ": " + exception.Message + "\n";
        }
    }
}

/// <summary>
/// Records the Screen facade and the mouse every update (the fields hold the latest frame). The first update moves
/// the mouse to (100, 50) in logical units through Input.Inject, seen from the next frame on. When the native test
/// gave the entity a camera, the zero-screen behaviour of the picking calls is recorded too.
/// </summary>
public sealed class ScreenProbe : ScriptBehaviour
{
    public string Errors = "";
    public int Updates;
    public int Width = -1;
    public int Height = -1;
    public string Size = "";
    public int AspectRatio = -1;
    public string ContentScale = "";
    public int UiScale = -1;
    public int Focused = -1;
    public int Minimised = -1;
    public int Fullscreen = -1;
    public string LogicalToPixels = "";
    public string Mouse = "";
    public string MouseScreen = "";
    public int RayDone = -1;
    public string RayToWorld = "";
    public int ProjectDone = -1;
    public string Projected = "";

    protected override void OnUpdate(float deltaTime)
    {
        ++Updates;
        try
        {
            if (Updates == 1)
            {
                Input.Inject.MouseMove(100.0f, 50.0f);
            }
            Width = Screen.Width;
            Height = Screen.Height;
            Size = ProbeSupport.Join(Screen.Size.X, Screen.Size.Y);
            AspectRatio = ProbeSupport.Thousandths(Screen.AspectRatio);
            ContentScale = ProbeSupport.Join(Screen.ContentScale.X, Screen.ContentScale.Y);
            UiScale = ProbeSupport.Thousandths(Screen.UiScale);
            Focused = Screen.IsFocused ? 1 : 0;
            Minimised = Screen.IsMinimised ? 1 : 0;
            Fullscreen = Screen.IsFullscreen ? 1 : 0;
            Vector2 pixels = Screen.LogicalToPixels(new Vector2(10.0f, 20.0f));
            LogicalToPixels = ProbeSupport.Join(pixels.X, pixels.Y);
            Mouse = ProbeSupport.Join(Input.MousePosition.X, Input.MousePosition.Y);
            MouseScreen = ProbeSupport.Join(Input.MouseScreenPosition.X, Input.MouseScreenPosition.Y);
            RecordPicking();
        }
        catch (Exception exception)
        {
            Errors += exception.GetType().Name + ": " + exception.Message + "\n";
        }
    }

    private void RecordPicking()
    {
        Camera camera = new Camera(Entity);
        if (!camera.Exists)
        {
            return;
        }
        RayDone = camera.ScreenPointToRay(Vector2.Zero, out _) ? 1 : 0;
        RayToWorld = ProbeSupport.Catch(() => camera.ScreenToWorld(Vector2.Zero, 1.0f));
        ProjectDone = camera.WorldToScreen(new Vector3(0.0f, 0.0f, -5.0f), out Vector3 projected) ? 1 : 0;
        Projected = ProbeSupport.Axis(projected);
    }
}

/// <summary>
/// Reads Entity.IsActiveInHierarchy every update for the two entities the native test passes in ParentEntity and
/// ChildEntity (the child under the parent), for its own entity and for a dead entity.
/// </summary>
public sealed class HierarchyActivityProbe : ScriptBehaviour
{
    public Entity ParentEntity;
    public Entity ChildEntity;
    public string Errors = "";
    public int Parent = -1;
    public int Child = -1;
    public int Self = -1;
    public int Dead = -1;

    protected override void OnUpdate(float deltaTime)
    {
        try
        {
            Parent = ParentEntity.IsActiveInHierarchy ? 1 : 0;
            Child = ChildEntity.IsActiveInHierarchy ? 1 : 0;
            Self = Entity.IsActiveInHierarchy ? 1 : 0;
            Dead = ProbeSupport.Dead.IsActiveInHierarchy ? 1 : 0;
        }
        catch (Exception exception)
        {
            Errors += exception.GetType().Name + ": " + exception.Message + "\n";
        }
    }
}
