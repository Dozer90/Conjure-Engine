using System.Numerics;
using Conjure;

namespace Game;

/// <summary>
/// The template script: moves its entity with WASD, logs its lifecycle and shows the serialised field
/// kinds the inspector understands. Public fields and [SerializeField] fields are saved with the scene
/// and survive hot reload.
/// </summary>
public sealed class Player : ScriptBehaviour
{
    [Tooltip("Units per second")]
    [Range(0.0, 20.0)]
    public float Speed = 5.0f;

    public Vector3 Spawn = Vector3.Zero;

    public Color Tint = Color.White;

    public Entity Target;

    [SerializeField]
    private int _livesLeft = 3;

    private float _distance;

    protected override void OnCreate()
    {
        Log.Info("Player created on entity " + Entity);
    }

    protected override void OnStart()
    {
        // Transform is a readonly struct returned by value: copy it to a local before writing a property.
        Transform transform = Transform;
        transform.Position = Spawn;
    }

    protected override void OnUpdate(float deltaTime)
    {
        Vector3 direction = Vector3.Zero;
        if (Input.IsKeyDown(Key.W))
        {
            direction.Z -= 1.0f;
        }
        if (Input.IsKeyDown(Key.S))
        {
            direction.Z += 1.0f;
        }
        if (Input.IsKeyDown(Key.A))
        {
            direction.X -= 1.0f;
        }
        if (Input.IsKeyDown(Key.D))
        {
            direction.X += 1.0f;
        }
        if (direction == Vector3.Zero)
        {
            return;
        }
        Vector3 step = Vector3.Normalize(direction) * Speed * deltaTime;
        Transform transform = Transform;
        transform.Position += step;
        _distance += step.Length();
    }

    protected override void OnCollisionEnter(in Collision collision)
    {
        --_livesLeft;
        Log.Info("Player hit " + collision.Other + " (" + _livesLeft + " lives left)");
        if (_livesLeft <= 0)
        {
            Destroy();
        }
    }

    protected override void OnDestroy()
    {
        Log.Info("Player travelled " + _distance + " units");
    }
}
