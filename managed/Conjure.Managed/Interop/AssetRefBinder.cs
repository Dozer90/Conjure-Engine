using System;
using System.Collections.Generic;
using System.Reflection;
using Conjure.Runtime;

namespace Conjure.Interop;

/// <summary>
/// The R25 bind pass for component writes from scripts (docs/design/15_bindings.md 2.6): before a mirror
/// with AssetRef&lt;T&gt; fields lands through component_add / component_set, every field's slot pointer is
/// filled by asset_ref_bind so the native side reads a bound reference. Each field is bound with its own
/// AssetRef type argument's asset type id, so an id of another asset type is refused natively
/// (ASSET_TYPE_MISMATCH), left unbound and logged once per field. Offsets and type arguments are reflected
/// once per type; a type without such fields costs one static check.
/// </summary>
public static unsafe class AssetRefBinder<T> where T : unmanaged
{
    private const byte KindAssetRef = 24;

    /// <summary>
    /// One AssetRef field of T: where it sits and how to resolve the asset type id of its type argument.
    /// </summary>
    private sealed class AssetRefField
    {
        public AssetRefField(int offset, string name, PropertyInfo? typeIdProperty)
        {
            Offset = offset;
            Name = name;
            TypeIdProperty = typeIdProperty;
        }

        public int Offset { get; }

        public string Name { get; }

        /// <summary>
        /// AssetTypeId&lt;TArg&gt;.Value of the field's type argument; null for an untyped reference.
        /// </summary>
        public PropertyInfo? TypeIdProperty { get; }

        public ulong TypeId;

        public int Generation = -1;

        public bool MismatchLogged;
    }

    private static readonly AssetRefField[] Fields = Reflect();

    /// <summary>
    /// True when T carries at least one AssetRef&lt;&gt; field.
    /// </summary>
    public static bool HasAssetRefs => Fields.Length > 0;

    /// <summary>
    /// Binds every AssetRef field of 'value' in place (null ids stay unbound; a failed bind leaves the slot
    /// null and the id intact, which the scene's own bind pass may still resolve later). A type mismatch
    /// leaves the slot null and logs once per field.
    /// </summary>
    public static void Bind(ref T value)
    {
        if (Fields.Length == 0)
        {
            return;
        }
        NativeApi* api = NativeBridge.Api;
        if (api == null || api->AssetRefBind == null)
        {
            return;
        }
        fixed (T* pinned = &value)
        {
            byte* bytes = (byte*)pinned;
            foreach (AssetRefField field in Fields)
            {
                UuidData* id = (UuidData*)(bytes + field.Offset);
                void** slot = (void**)(bytes + field.Offset + sizeof(UuidData));
                if (id->IsNull)
                {
                    *slot = null;
                    continue;
                }
                void* bound = null;
                int status = api->AssetRefBind(id, TypeIdOf(field), &bound);
                if (status >= 0)
                {
                    *slot = bound;
                    continue;
                }
                // A stale pointer the value carried (one read back with GetComponent) must not land bound.
                *slot = null;
                if (NativeCheck.Is(status, BindingStatus.AssetTypeMismatch) && !field.MismatchLogged)
                {
                    field.MismatchLogged = true;
                    NativeBridge.Log(
                        LogLevel.Warn,
                        typeof(T).Name + "." + field.Name + ": the asset " + id->ToString()
                            + " is not of the field's asset type; the reference stays unbound");
                }
            }
        }
    }

    /// <summary>
    /// The field's asset type id, resolved through AssetTypeId&lt;TArg&gt; once per native handshake (0 for an
    /// untyped reference, which skips the native check).
    /// </summary>
    private static ulong TypeIdOf(AssetRefField field)
    {
        if (field.TypeIdProperty == null)
        {
            return 0;
        }
        if (field.Generation != NativeBridge.Generation)
        {
            field.Generation = NativeBridge.Generation;
            field.TypeId = field.TypeIdProperty.GetValue(null) is ulong value ? value : 0;
        }
        return field.TypeId;
    }

    private static AssetRefField[] Reflect()
    {
        List<ComponentFieldLayout> layout;
        try
        {
            layout = LayoutHash.Describe(typeof(T));
        }
        catch (InvalidOperationException)
        {
            return Array.Empty<AssetRefField>();
        }
        // LayoutHash.Describe lists the fields in metadata order, one entry per field (a [NativeComponent]
        // mirror's padding excluded); the same walk here pairs every AssetRef entry with its FieldInfo to read
        // the type argument.
        FieldInfo[] fields = typeof(T).GetFields(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
        Array.Sort(fields, (left, right) => left.MetadataToken.CompareTo(right.MetadataToken));
        Dictionary<string, FieldInfo> byNativeName = new Dictionary<string, FieldInfo>(fields.Length);
        bool skipPadding = LayoutHash.SkipsPadding(typeof(T));
        foreach (FieldInfo field in fields)
        {
            if (!skipPadding || !LayoutHash.IsPaddingName(field.Name))
            {
                byNativeName[LayoutHash.NativeName(field.Name)] = field;
            }
        }
        List<AssetRefField> result = new List<AssetRefField>();
        foreach (ComponentFieldLayout entry in layout)
        {
            if (entry.Kind != KindAssetRef)
            {
                continue;
            }
            PropertyInfo? typeIdProperty = null;
            if (byNativeName.TryGetValue(entry.Name, out FieldInfo? info) && info.FieldType.IsGenericType)
            {
                Type argument = info.FieldType.GetGenericArguments()[0];
                typeIdProperty = typeof(AssetTypeId<>).MakeGenericType(argument).GetProperty(nameof(AssetTypeId<T>.Value));
            }
            result.Add(new AssetRefField(entry.Offset, entry.Name, typeIdProperty));
        }
        return result.ToArray();
    }
}
