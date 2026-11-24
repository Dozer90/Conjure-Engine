using System;
using System.Numerics;

namespace Conjure;

/// <summary>
/// A half-line in world space (metres): an origin and a unit direction. Camera.ScreenPointToRay makes one per
/// pixel; Physics.Raycast takes the two parts.
/// </summary>
public readonly struct Ray : IEquatable<Ray>
{
    /// <summary>
    /// Where the ray starts.
    /// </summary>
    public readonly Vector3 Origin;

    /// <summary>
    /// Where it points, unit length.
    /// </summary>
    public readonly Vector3 Direction;

    /// <summary>
    /// A ray from 'origin' along 'direction', normalised here. ArgumentException for a non-finite origin or a
    /// direction that is zero or not finite.
    /// </summary>
    public Ray(Vector3 origin, Vector3 direction)
    {
        if (!float.IsFinite(origin.X) || !float.IsFinite(origin.Y) || !float.IsFinite(origin.Z))
        {
            throw new ArgumentException("the origin of a ray must be finite", nameof(origin));
        }
        // Scaled by its largest component first, so a tiny or huge finite direction neither underflows to a zero
        // length nor overflows to an infinite one.
        float largest = MathF.Max(MathF.Abs(direction.X), MathF.Max(MathF.Abs(direction.Y), MathF.Abs(direction.Z)));
        if (!float.IsFinite(largest) || largest <= 0.0f)
        {
            throw new ArgumentException("the direction of a ray must be finite and not zero", nameof(direction));
        }
        Vector3 scaled = direction / largest;
        Origin = origin;
        Direction = scaled / scaled.Length();
    }

    /// <summary>
    /// The point 'distance' metres along the ray (behind the origin for a negative distance).
    /// </summary>
    public Vector3 GetPoint(float distance) => Origin + Direction * distance;

    public bool Equals(Ray other) => Origin == other.Origin && Direction == other.Direction;

    public override bool Equals(object? obj) => obj is Ray other && Equals(other);

    public override int GetHashCode() => HashCode.Combine(Origin, Direction);

    public override string ToString() => "Ray(" + Origin + ", " + Direction + ")";

    public static bool operator ==(Ray left, Ray right) => left.Equals(right);

    public static bool operator !=(Ray left, Ray right) => !left.Equals(right);
}
