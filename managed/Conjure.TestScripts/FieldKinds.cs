using System.Collections.Generic;
using System.Numerics;
using Conjure;

namespace Conjure.TestScripts;

public enum Mode
{
    Idle,
    Run,
    Jump,
}

public sealed class TextureAsset
{
}

/// <summary>
/// Every serialised field kind with a non-default initial value, so a blob round trip through
/// reflection can be checked kind by kind.
/// </summary>
public sealed class FieldKinds : ScriptBehaviour
{
    public bool Flag = true;
    public int Count = 7;
    public long Big = 1L << 40;
    public float Ratio = 1.5f;
    public double Precise = 2.25;
    public string Label = "hello";
    public Vector2 Planar = new Vector2(1.0f, 2.0f);
    public Vector3 Spatial = new Vector3(1.0f, 2.0f, 3.0f);
    public Vector4 Homogeneous = new Vector4(1.0f, 2.0f, 3.0f, 4.0f);
    public Quaternion Orientation = new Quaternion(0.0f, 0.70710677f, 0.0f, 0.70710677f);
    public Color Tint = new Color(1.0f, 0.5f, 0.25f, 1.0f);
    public Entity Link;
    public AssetRef<TextureAsset> Texture;
    public Mode Current = Mode.Run;
    public List<int> Ints = new List<int> { 1, 2, 3 };
    public float[] Floats = { 0.5f, 1.5f };
    public List<string> Names = new List<string> { "a", "bc" };
    public List<Entity> Links = new List<Entity>();

    [HideInInspector]
    public int Hidden = 9;

    public int Touched;

    protected override void OnUpdate(float deltaTime)
    {
        ++Touched;
    }
}
