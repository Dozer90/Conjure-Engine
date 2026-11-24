using System;

namespace Conjure;

/// <summary>
/// Serialises a private field of a ScriptBehaviour (public fields are serialised by default).
/// </summary>
[AttributeUsage(AttributeTargets.Field, AllowMultiple = false)]
public sealed class SerializeFieldAttribute : Attribute
{
}

/// <summary>
/// Inspector slider range of a numeric field.
/// </summary>
[AttributeUsage(AttributeTargets.Field, AllowMultiple = false)]
public sealed class RangeAttribute : Attribute
{
    public RangeAttribute(double min, double max)
    {
        Min = min;
        Max = max;
    }

    public double Min { get; }

    public double Max { get; }
}

/// <summary>
/// Inspector tooltip of a field.
/// </summary>
[AttributeUsage(AttributeTargets.Field, AllowMultiple = false)]
public sealed class TooltipAttribute : Attribute
{
    public TooltipAttribute(string text)
    {
        Text = text;
    }

    public string Text { get; }
}

/// <summary>
/// Serialised but not shown by the inspector.
/// </summary>
[AttributeUsage(AttributeTargets.Field, AllowMultiple = false)]
public sealed class HideInInspectorAttribute : Attribute
{
}

/// <summary>
/// Marks a blittable struct as the mirror of a registered native component (R61: the registered name,
/// e.g. "conjure.scene.local_transform"). Its layout hash must match type_info::layout_hash().
/// </summary>
[AttributeUsage(AttributeTargets.Struct, AllowMultiple = false)]
public sealed class NativeComponentAttribute : Attribute
{
    public NativeComponentAttribute(string registeredName)
    {
        RegisteredName = registeredName;
    }

    public string RegisteredName { get; }
}

/// <summary>
/// Marks a blittable struct as the mirror of a reflected native struct nested inside a component (the
/// registered name, e.g. "conjure.renderer.camera_data"). Like a [NativeComponent] mirror its Pad&lt;digits&gt;
/// fields are padding and its layout hash (LayoutHash.ComputeFor) must equal type_info::layout_hash() of the
/// registered struct. The runtime does not check it: a component lookup hashes only the component's top-level
/// fields, so a same-size reorder inside a nested struct is caught by the [dotnet] hash test of the shipped
/// mirrors, not at run time.
/// </summary>
[AttributeUsage(AttributeTargets.Struct, AllowMultiple = false)]
public sealed class NativeStructAttribute : Attribute
{
    public NativeStructAttribute(string registeredName)
    {
        RegisteredName = registeredName;
    }

    public string RegisteredName { get; }
}

/// <summary>
/// Marks an unmanaged struct as a user-defined ECS component registered at assembly load (R4).
/// </summary>
[AttributeUsage(AttributeTargets.Struct, AllowMultiple = false)]
public sealed class EcsComponentAttribute : Attribute
{
}
