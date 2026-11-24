using System;
using System.Collections.Generic;
using Conjure;

namespace Conjure.TestScripts;

/// <summary>
/// Uses features of C# 14, the language version the managed projects and the game project files pin
/// (managed/Directory.Build.props, managed/Conjure.Templates/Game/Game.csproj and the Game.csproj the editor's
/// script.create writes): under an older version this file does not compile. OnCreate stores what each feature
/// computed in the public fields the [dotnet] case of tests/scripting reads back. The cases for the game template and
/// for script.create compile this file into a game project as well, so it uses nothing but Conjure.Managed and the
/// base class library.
/// </summary>
public sealed class LanguageVersionProbe : ScriptBehaviour
{
    /// <summary>What Health, whose setter writes through the field keyword, kept of 250.</summary>
    public int ClampedHealth;

    /// <summary>7.Squared, a property of an extension block.</summary>
    public int ExtensionSquare;

    /// <summary>The sum of an int[] through an extension of ReadOnlySpan&lt;int&gt; (first-class spans).</summary>
    public int SpanTotal;

    /// <summary>The value a null-conditional assignment wrote through the one target that is not null.</summary>
    public int AssignedValue;

    /// <summary>Runs of the null-conditional assignment's right-hand side: one per target that is not null.</summary>
    public int RightSideEvaluations;

    /// <summary>nameof of the unbound generic type Dictionary&lt;,&gt;.</summary>
    public string UnboundGenericName = "";

    /// <summary>What a lambda with an out parameter and no parameter types parsed from "42".</summary>
    public int ParsedByLambda;

    private delegate bool Parser(string text, out int value);

    private int Health
    {
        get;
        set => field = Math.Clamp(value, 0, 100);
    }

    protected override void OnCreate()
    {
        Health = 250;
        ClampedHealth = Health;

        ExtensionSquare = 7.Squared;

        int[] numbers = [1, 2, 3, 4];
        SpanTotal = numbers.Total();

        Holder present = new Holder();
        Holder?[] targets = [null, present, null];
        foreach (Holder? target in targets)
        {
            target?.Value = NextValue();
        }
        AssignedValue = present.Value;

        UnboundGenericName = nameof(Dictionary<,>);

        Parser parse = (text, out value) => int.TryParse(text, out value);
        ParsedByLambda = parse("42", out int parsed) ? parsed : -1;
    }

    private int NextValue()
    {
        ++RightSideEvaluations;
        return 42;
    }

    private sealed class Holder
    {
        public int Value;
    }
}

/// <summary>
/// The extension blocks LanguageVersionProbe uses.
/// </summary>
public static class LanguageVersionExtensions
{
    extension(int value)
    {
        /// <summary>The value times itself.</summary>
        public int Squared => value * value;
    }

    extension(ReadOnlySpan<int> values)
    {
        /// <summary>The sum of the values.</summary>
        public int Total()
        {
            int total = 0;
            foreach (int value in values)
            {
                total += value;
            }
            return total;
        }
    }
}
