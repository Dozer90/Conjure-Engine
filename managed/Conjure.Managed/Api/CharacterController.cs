using System.Numerics;
using Conjure.Components;
using Conjure.Dod;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// The character controller facade of an entity (docs/design/15_bindings.md 2.5): the desired velocity and
/// the step's ground report through physics_character_*, which need a physics.character_controller on the
/// entity (ConjureNativeException(ComponentMissing) otherwise). IsGrounded never throws (B11).
/// </summary>
public readonly struct CharacterController
{
    public readonly Entity Entity;

    public CharacterController(Entity entity)
    {
        Entity = entity;
    }

    /// <summary>
    /// True when the entity carries a character controller; false in a world without the physics components (a
    /// build without physics), where Entity.HasComponent&lt;CharacterControllerData&gt; throws.
    /// </summary>
    public bool Exists =>
        ComponentType<CharacterControllerData>.Id != 0 && Entity.HasComponent<CharacterControllerData>();

    /// <summary>
    /// Sets the velocity the next fixed step tries to achieve.
    /// </summary>
    public unsafe void Move(Vector3 velocity)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PhysicsCharacterMove, NativeFunction.PhysicsCharacterMove);
        NativeCheck.ThrowIfFailed(api->PhysicsCharacterMove(Entity.Id, &velocity), NativeFunction.PhysicsCharacterMove);
    }

    /// <summary>
    /// True when the last step left the character on walkable ground; false otherwise, for a dead entity
    /// and for an entity without a controller.
    /// </summary>
    public unsafe bool IsGrounded
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->PhysicsCharacterIsGrounded, NativeFunction.PhysicsCharacterIsGrounded);
            return api->PhysicsCharacterIsGrounded(Entity.Id) > 0;
        }
    }

    /// <summary>
    /// The velocity the last step achieved.
    /// </summary>
    public unsafe Vector3 Velocity
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->PhysicsCharacterVelocity, NativeFunction.PhysicsCharacterVelocity);
            Vector3 value = Vector3.Zero;
            NativeCheck.ThrowIfFailed(api->PhysicsCharacterVelocity(Entity.Id, &value), NativeFunction.PhysicsCharacterVelocity);
            return value;
        }
    }

    /// <summary>
    /// The character_controller component itself; the setter writes it back through component_set.
    /// </summary>
    public CharacterControllerData Data
    {
        get => Entity.GetComponent<CharacterControllerData>();
        set => Entity.SetComponent(in value);
    }
}
