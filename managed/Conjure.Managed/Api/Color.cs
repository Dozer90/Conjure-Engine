using System;
using System.Runtime.InteropServices;

namespace Conjure;

/// <summary>
/// An RGBA colour with float channels (the COLOR field kind; 16 bytes).
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct Color : IEquatable<Color>
{
    public float R;
    public float G;
    public float B;
    public float A;

    public Color(float r, float g, float b, float a = 1.0f)
    {
        R = r;
        G = g;
        B = b;
        A = a;
    }

    public static Color White => new Color(1.0f, 1.0f, 1.0f, 1.0f);

    public static Color Black => new Color(0.0f, 0.0f, 0.0f, 1.0f);

    public static Color Transparent => new Color(0.0f, 0.0f, 0.0f, 0.0f);

    public bool Equals(Color other) => R == other.R && G == other.G && B == other.B && A == other.A;

    public override bool Equals(object? obj) => obj is Color other && Equals(other);

    public override int GetHashCode() => HashCode.Combine(R, G, B, A);

    public override string ToString() => "(" + R + ", " + G + ", " + B + ", " + A + ")";
}
