using System;
using System.Collections.Generic;
using System.Reflection;
using System.Text;
using Conjure.Interop;

namespace Conjure.Runtime;

/// <summary>
/// The script classes and [EcsComponent] types of one game assembly (docs/design/03_scripting.md 3.7,
/// 3.9). Built once per load, reported to native through script_register_class/field and
/// component_register_managed, dropped at unload.
/// </summary>
public sealed class ScriptCatalog
{
    // FieldAttributes.NotSerialized (0x80) is obsolete as an API but still the flag [NonSerialized] sets.
    private const int NotSerializedFlag = 0x80;

    private static readonly string[] CallbackMethods =
    {
        "OnCreate", "OnStart", "OnFixedUpdate", "OnUpdate", "OnLateUpdate", "OnDestroy",
        "OnCollisionEnter", "OnCollisionExit", "OnTriggerEnter", "OnTriggerExit", "OnReload",
    };

    private readonly List<ScriptClassInfo> _classes;
    private readonly Dictionary<ulong, ScriptClassInfo> _byHash;
    private readonly Dictionary<Type, uint> _componentIds;

    private ScriptCatalog(List<ScriptClassInfo> classes, Dictionary<Type, uint> componentIds)
    {
        _classes = classes;
        _byHash = new Dictionary<ulong, ScriptClassInfo>(classes.Count);
        foreach (ScriptClassInfo info in classes)
        {
            _byHash[info.Hash] = info;
        }
        _componentIds = componentIds;
    }

    /// <summary>
    /// Classes in full-name order (deterministic phase iteration).
    /// </summary>
    public IReadOnlyList<ScriptClassInfo> Classes => _classes;

    public ScriptClassInfo? Find(ulong hash) => _byHash.TryGetValue(hash, out ScriptClassInfo? info) ? info : null;

    /// <summary>
    /// The native component id of a registered [EcsComponent] type, 0 when unknown.
    /// </summary>
    public uint ComponentIdOf(Type type) => _componentIds.TryGetValue(type, out uint id) ? id : 0;

    /// <summary>
    /// Discovers every concrete ScriptBehaviour subclass with a public parameterless constructor and
    /// every [EcsComponent] struct of the assembly.
    /// </summary>
    public static ScriptCatalog Build(Assembly assembly)
    {
        Type[] types;
        try
        {
            types = assembly.GetTypes();
        }
        catch (ReflectionTypeLoadException exception)
        {
            List<Type> loadable = new List<Type>();
            foreach (Type? type in exception.Types)
            {
                if (type != null)
                {
                    loadable.Add(type);
                }
            }
            types = loadable.ToArray();
            NativeBridge.Log(LogLevel.Warn, "some types of " + assembly.GetName().Name + " did not load: " + exception.Message);
        }

        List<ScriptClassInfo> classes = new List<ScriptClassInfo>();
        List<Type> components = new List<Type>();
        foreach (Type type in types)
        {
            if (type.IsValueType && type.GetCustomAttribute<EcsComponentAttribute>() != null)
            {
                components.Add(type);
                continue;
            }
            if (!typeof(ScriptBehaviour).IsAssignableFrom(type) || type.IsAbstract || type.IsGenericTypeDefinition)
            {
                continue;
            }
            ConstructorInfo? constructor = type.GetConstructor(Type.EmptyTypes);
            if (constructor == null)
            {
                NativeBridge.Log(LogLevel.Info, type.FullName + " has no public parameterless constructor; skipped");
                continue;
            }
            classes.Add(new ScriptClassInfo(type, constructor, DiscoverFields(type), DiscoverCallbacks(type)));
        }
        classes.Sort((left, right) => string.CompareOrdinal(left.FullName, right.FullName));
        components.Sort((left, right) => string.CompareOrdinal(left.FullName, right.FullName));

        Dictionary<Type, uint> componentIds = RegisterComponents(components);
        ScriptCatalog catalog = new ScriptCatalog(classes, componentIds);
        catalog.ReportToNative();
        return catalog;
    }

    /// <summary>
    /// Releases every reference to the game assembly's types.
    /// </summary>
    public void Drop()
    {
        foreach (ScriptClassInfo info in _classes)
        {
            foreach (PhaseList list in info.Lists)
            {
                list.Clear();
            }
        }
        _classes.Clear();
        _byHash.Clear();
        _componentIds.Clear();
    }

    private static FieldAccessor[] DiscoverFields(Type type)
    {
        List<FieldAccessor> accessors = new List<FieldAccessor>();
        List<FieldInfo> fields = new List<FieldInfo>();
        // Fields are hashed by name (3.7): a base-class field redeclared by a derived class would alias it
        // in every blob and scene file, so the most derived declaration keeps the bare name and a hidden
        // base field is serialised under "<DeclaringType>.<name>" instead.
        HashSet<string> names = new HashSet<string>(StringComparer.Ordinal);
        Dictionary<FieldInfo, string> qualifiedNames = new Dictionary<FieldInfo, string>();
        for (Type? current = type; current != null && current != typeof(ScriptBehaviour); current = current.BaseType)
        {
            FieldInfo[] declared = current.GetFields(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly);
            Array.Sort(declared, (left, right) => left.MetadataToken.CompareTo(right.MetadataToken));
            foreach (FieldInfo field in declared)
            {
                if (!IsSerialisedField(field))
                {
                    continue;
                }
                if (!names.Add(field.Name))
                {
                    string qualified = current.Name + "." + field.Name;
                    NativeBridge.Log(LogLevel.Info,
                        type.FullName + ": field '" + field.Name + "' of " + current.FullName +
                        " is hidden by a derived field of the same name; serialised as '" + qualified + "'");
                    qualifiedNames[field] = qualified;
                }
                fields.Add(field);
            }
        }
        fields.Sort((left, right) => left.MetadataToken.CompareTo(right.MetadataToken));
        foreach (FieldInfo field in fields)
        {
            if (!FieldAccessor.TryClassify(field.FieldType, out ScriptFieldKind kind, out ScriptFieldKind elementKind, out Type elementType))
            {
                NativeBridge.Log(LogLevel.Info, type.FullName + "." + field.Name + " has an unsupported field type " + field.FieldType.Name + "; not serialised");
                continue;
            }
            string name = qualifiedNames.TryGetValue(field, out string? qualified) ? qualified : field.Name;
            accessors.Add(new FieldAccessor(field, name, kind, elementKind, elementType));
        }
        accessors.Sort((left, right) => left.Hash.CompareTo(right.Hash));
        return accessors.ToArray();
    }

    /// <summary>
    /// The inclusion rules of 3.7: public or [SerializeField], not static, readonly, const, [NonSerialized]
    /// or compiler generated (auto-property backing fields).
    /// </summary>
    private static bool IsSerialisedField(FieldInfo field)
    {
        bool included = field.IsPublic || field.GetCustomAttribute<SerializeFieldAttribute>() != null;
        if (!included || field.IsStatic || field.IsInitOnly || field.IsLiteral ||
            ((int)field.Attributes & NotSerializedFlag) != 0)
        {
            return false;
        }
        return field.GetCustomAttribute<System.Runtime.CompilerServices.CompilerGeneratedAttribute>() == null;
    }

    private static ushort DiscoverCallbacks(Type type)
    {
        ushort mask = 0;
        const BindingFlags flags = BindingFlags.Instance | BindingFlags.NonPublic | BindingFlags.Public;
        for (int index = 0; index < CallbackMethods.Length; ++index)
        {
            MethodInfo? method = null;
            foreach (MethodInfo candidate in type.GetMethods(flags))
            {
                if (candidate.Name == CallbackMethods[index] && candidate.IsVirtual && candidate.DeclaringType != typeof(ScriptBehaviour))
                {
                    method = candidate;
                    break;
                }
            }
            if (method != null)
            {
                mask |= (ushort)(1 << index);
            }
        }
        return mask;
    }

    private static unsafe Dictionary<Type, uint> RegisterComponents(List<Type> components)
    {
        Dictionary<Type, uint> ids = new Dictionary<Type, uint>();
        NativeApi* api = NativeBridge.Api;
        if (api == null || api->ComponentRegisterManaged == null)
        {
            return ids;
        }
        foreach (Type type in components)
        {
            List<ComponentFieldLayout> layout;
            int size;
            try
            {
                layout = LayoutHash.Describe(type, out size);
            }
            catch (InvalidOperationException exception)
            {
                NativeBridge.Log(LogLevel.Warn, "[EcsComponent] " + type.FullName + " skipped: " + exception.Message);
                continue;
            }
            string? schemaJson = LayoutHash.DynamicSchemaJson(type, layout, out string refusal);
            if (schemaJson == null)
            {
                NativeBridge.Log(LogLevel.Warn, "[EcsComponent] " + type.FullName + " skipped: " + refusal);
                continue;
            }
            string fullName = type.FullName ?? type.Name;
            byte[] nameBytes = Encoding.UTF8.GetBytes(fullName);
            byte[] schemaBytes = Encoding.UTF8.GetBytes(schemaJson);
            fixed (byte* name = nameBytes)
            fixed (byte* schema = schemaBytes)
            {
                ManagedComponentDesc desc = new ManagedComponentDesc
                {
                    NameHash = NameHash.Fnv1a64(fullName),
                    FullName = name,
                    NameLength = nameBytes.Length,
                    Size = (uint)size,
                    Alignment = (uint)LayoutHash.Alignment(layout),
                    LayoutHash = LayoutHash.Compute(layout),
                    SchemaJson = schema,
                    SchemaLength = schemaBytes.Length,
                };
                uint id = api->ComponentRegisterManaged(&desc);
                if (id == 0)
                {
                    NativeBridge.Log(LogLevel.Warn, "[EcsComponent] " + fullName + " was not registered by native");
                    continue;
                }
                ids[type] = id;
            }
        }
        return ids;
    }

    private unsafe void ReportToNative()
    {
        NativeApi* api = NativeBridge.Api;
        if (api == null || api->ScriptRegisterClass == null)
        {
            return;
        }
        foreach (ScriptClassInfo info in _classes)
        {
            byte[] nameBytes = Encoding.UTF8.GetBytes(info.FullName);
            fixed (byte* name = nameBytes)
            {
                ScriptClassDesc desc = new ScriptClassDesc
                {
                    Hash = info.Hash,
                    FullName = name,
                    NameLength = nameBytes.Length,
                    CallbackMask = info.CallbackMask,
                    FieldCount = (ushort)info.Fields.Length,
                };
                api->ScriptRegisterClass(&desc);
            }
            if (api->ScriptRegisterField == null || info.Fields.Length == 0)
            {
                continue;
            }

            ScriptBehaviour? defaults = null;
            try
            {
                defaults = info.CreateInstance();
            }
            catch (Exception exception)
            {
                NativeBridge.Log(LogLevel.Warn, info.FullName + " constructor threw while reading defaults: " + exception.Message);
            }
            foreach (FieldAccessor accessor in info.Fields)
            {
                bool clamped = false;
                string defaultJson = defaults == null ? "" : accessor.DefaultJson(defaults, out clamped);
                if (clamped)
                {
                    // INFO, not WARN: a deliberate float.PositiveInfinity default is legitimate game code.
                    NativeBridge.Log(LogLevel.Info, info.FullName + "." + accessor.Name +
                        ": a NaN or infinite default cannot be stored in JSON; stored as 0 / the largest finite value");
                }
                byte[] fieldName = Encoding.UTF8.GetBytes(accessor.Name);
                byte[] defaultBytes = Encoding.UTF8.GetBytes(defaultJson);
                byte[] metaBytes = Encoding.UTF8.GetBytes(accessor.MetaJson());
                fixed (byte* namePointer = fieldName)
                fixed (byte* defaultPointer = defaultBytes)
                fixed (byte* metaPointer = metaBytes)
                {
                    ScriptFieldDesc desc = new ScriptFieldDesc
                    {
                        Hash = accessor.Hash,
                        Name = namePointer,
                        NameLength = fieldName.Length,
                        Kind = (byte)accessor.Kind,
                        ElementKind = (byte)accessor.ElementKind,
                        DefaultJson = defaultPointer,
                        DefaultLength = defaultBytes.Length,
                        MetaJson = metaPointer,
                        MetaLength = metaBytes.Length,
                    };
                    api->ScriptRegisterField(info.Hash, &desc);
                }
            }
        }
    }
}
