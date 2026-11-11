using System;
using System.Runtime.InteropServices;

namespace Conjure.Components;

/// <summary>
/// A one-byte bool for component mirrors (docs/design/15_bindings.md 2.6): the BOOL reflection kind with
/// the exact size and alignment of a C++ bool, so a mirror's Marshal.SizeOf agrees with its managed size
/// whatever the interop defaults do to System.Boolean.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public readonly struct NativeBool : IEquatable<NativeBool>
{
    public static readonly NativeBool True = new NativeBool(true);
    public static readonly NativeBool False = default;

    public readonly byte Value;

    public NativeBool(bool value)
    {
        Value = value ? (byte)1 : (byte)0;
    }

    public bool IsTrue => Value != 0;

    public bool Equals(NativeBool other) => IsTrue == other.IsTrue;

    public override bool Equals(object? obj) => obj is NativeBool other && Equals(other);

    public override int GetHashCode() => IsTrue ? 1 : 0;

    public override string ToString() => IsTrue ? "true" : "false";

    public static implicit operator bool(NativeBool value) => value.IsTrue;

    public static implicit operator NativeBool(bool value) => new NativeBool(value);

    public static bool operator ==(NativeBool left, NativeBool right) => left.Equals(right);

    public static bool operator !=(NativeBool left, NativeBool right) => !left.Equals(right);
}
