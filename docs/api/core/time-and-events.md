# Time, events and random

📘 **Public API wiki** · 🛠 Handbook: [Core module](../../handbook/modules/core.md)

The small primitives gameplay code uses every frame: clocks and stopwatches (`conjure/core/clock.h`), seeded random
numbers (`conjure/core/random.h`), multicast events (`conjure/core/event.h`), single-target delegates
(`conjure/core/delegate.h`) and the engine-wide typed `event_bus` (`conjure/core/event_bus.h`). Everything is in
`namespace conjure::core`; the random helpers sit one level deeper in `conjure::core::random`.

## At a glance

| You want to... | Use |
|---|---|
| Measure how long something took | `stopwatch` (`elapsed_seconds()`, `restart()`) |
| Read "now" in seconds | `default_clock().now_seconds()` or an injected `core::clock &` |
| Drive a simulation step by step (tests, tools) | `manual_clock` with `advance()` |
| Profile a hot path with minimal overhead | `ticks_now()` and `ticks_to_seconds()` |
| Roll dice, pick a random item, jitter a value | `random::uniform(random::default_engine(), min, max)` |
| Replay the same random sequence | `random::seed(value)` or an owned `random::random_engine(seed)` |
| Let several listeners react when something happens | `event<Args...>` and `subscribe()` |
| Store one callback without allocating | `delegate<Return(Args...)>` |
| Broadcast a typed message across modules | `event_bus::publish(my_event{...})` |
| Report something from a worker thread to the main thread | `event_bus::enqueue(my_event{...})` |

## Quick example

```cpp
#include "conjure/core/event.h"
#include "conjure/core/random.h"

using namespace conjure;
using namespace conjure::core;

struct spawner {
    event<int> onSpawned = {};

    void spawn_wave(random::engine &generator) {
        const int count = random::uniform(generator, 3, 6);   // inclusive: 3, 4, 5 or 6
        for (int index = 0; index < count; ++index) {
            onSpawned.emit(index);
        }
    }
};

int spawn_and_count() {
    spawner enemies = {};
    int spawned = 0;
    const event_subscription token = enemies.onSpawned.subscribe([&spawned](int enemyIndex) {
        spawned = enemyIndex + 1;   // indices run 0..count-1, so the last one is the total
    });

    enemies.spawn_wave(random::default_engine());   // this thread's seeded engine: replayable
    return spawned;                                 // 3 to 6; the same value on every run with the same seed
}
```

## Clocks and stopwatches

A `core::clock` is a monotonic time source in seconds (`f64`). Only differences between two readings of the same
clock mean anything; the epoch is unspecified. Two implementations ship:

| Type | Behaviour |
|---|---|
| `steady_clock` | Real time. Counts from the first use in the process, so readings start near zero and keep full precision. Every instance reads the same time. |
| `manual_clock` | Moves only through `advance(seconds)`, `set(seconds)` or `sleep_until(seconds)`. Starts at 0 (or the value you pass). Tests and deterministic tools use it. |

`default_clock()` returns the process-wide `steady_clock`; use it when nobody injected a clock. Both clocks implement
`sleep_until(seconds)`: the steady clock really sleeps (then spins for the last 2 ms so it wakes on time), the
manual clock simply jumps forward and never moves backwards through it.

`stopwatch` measures elapsed time on a clock. It starts at construction, `start()` resets the origin,
`elapsed_seconds()` reads without resetting, and `restart()` returns the interval and reopens it in one reading so
consecutive intervals add up without gaps.

```cpp
void load_level();   // your own code

const stopwatch loadTimer = {};                  // measures the process-wide steady clock
load_level();
const f64 loadSeconds = loadTimer.elapsed_seconds();

manual_clock simulated(10.0);
stopwatch frameTimer(simulated);                 // measures the injected clock

simulated.advance(1.0 / 60.0);
const f64 deltaSeconds = frameTimer.restart();   // 0.0166..., origin is now 10.0166...
simulated.advance(0.5);
const f64 sinceRestart = frameTimer.elapsed_seconds();   // 0.5
```

> [!WARNING]
> A `stopwatch` references its clock; it does not own it. The clock must outlive every stopwatch built on it. The
> default-constructed stopwatch measures `default_clock()`, which lives for the whole process.

> [!WARNING]
> With `using namespace conjure::core;` at global scope the unqualified name `clock` is ambiguous with the C library
> function `::clock()`. Write `core::clock` for the base class (the derived types `steady_clock`, `manual_clock` and
> `stopwatch` are unambiguous).

> [!TIP]
> Take the clock as a `const core::clock &` parameter instead of calling `default_clock()` inside your code. The
> engine passes its clock in, and your logic becomes testable with a `manual_clock`.

> [!NOTE]
> `manual_clock::advance` asserts and ignores negative or NaN amounts; `set()` is the way to move backwards. Raw
> ticks (`ticks_now()`, `ticks_to_seconds()`, `ticks_per_second()`) read the steady clock without a virtual call or
> double conversion and exist for profiling; convert differences, not absolute values.

## Random numbers

Every random number in Conjure comes from `random::engine` (`std::mt19937_64`), so a seeded run replays identically.
The helpers avoid the standard `<random>` distributions on purpose: their algorithms differ between standard
libraries, while `random::uniform` and `random::chance` give the same values on Windows, Linux and macOS.

| Function | Result |
|---|---|
| `random::default_engine()` | This thread's engine, created on first use from the process seed. |
| `random::seed(value)`, `random::current_seed()` | Set the seed new engines start from and reseed the calling thread now. Thread-safe. |
| `random::uniform(generator, min, max)` | Integers: inclusive `[min, max]`, any integral type except `bool`. Floats: half-open `[min, max)`, never returns `max`. Asserts and returns `min` when `min > max`. |
| `random::chance(generator, probability)` | `true` with that probability; 0 is never, 1 is always. |
| `random::unit_vector3<Vector3>(generator)` | Uniform point on the unit sphere; `Vector3` defaults to `std::array<f32, 3>`. Pass `math::vec3` (include `conjure/core/math.h`; `random.h` does not) to get one. It takes the same two engine draws on every platform, but x and y use `std::cos`/`std::sin`, so they may differ in the last bit between platforms (z is exact). |
| `random::random_engine` | An owned engine with its own seed and the helpers as members. Satisfies `std::uniform_random_bit_generator`, so `std::shuffle` accepts it. |

```cpp
#include "conjure/core/math.h"   // for math::vec3 below; random.h alone gives std::array<f32, 3>

random::random_engine dice(1234);                 // owned, replayable
const i32 roll = dice.uniform(1, 6);
const f32 pitch = dice.uniform(0.9f, 1.1f);       // [0.9, 1.1)
const bool critical = dice.chance(0.05);
const math::vec3 direction = dice.unit_vector3<math::vec3>();

std::vector<int> deck = {1, 2, 3, 4};
std::shuffle(deck.begin(), deck.end(), dice);      // standard algorithms accept it
```

> [!TIP]
> Reach for `random::default_engine()` for one-off gameplay randomness. Give a system its own `random_engine` when
> its results must not change because some other system drew numbers first (procedural generation, replays).

> [!WARNING]
> `default_engine()` is per thread and every thread starts from the same seed, so two jobs doing identical work draw
> identical numbers. If parallel work must differ, seed an owned `random_engine` per task (for example from the task
> index).

## Events

`event<Args...>` is a list of callbacks invoked in subscription order. `subscribe()` returns an `event_subscription`
that unsubscribes when it is released or destroyed; keep it alive for as long as you want to listen.

```cpp
event<int, std::string_view> onScore = {};
int total = 0;
event_subscription token = onScore.subscribe([&total](int points, std::string_view who) {
    total += who == "player" ? points : 0;
});

onScore.emit(10, "player");   // every live subscriber, in subscription order
token.release();              // or let it go out of scope
onScore.emit(5, "player");    // no subscribers, nothing happens: total stays 10
```

Emission is reentrancy-safe: a handler may subscribe (first invoked by the next emit), unsubscribe anything including
itself (never invoked again, not even later in the current emit), emit the same event again or even destroy the
event. Events are movable (subscriptions follow) but not copyable. `subscriber_count()` reports live subscriptions.

> [!WARNING]
> `subscribe()` is `[[nodiscard]]` for a reason: a subscription you do not store is destroyed at the end of the
> statement and the handler never runs. Store the `event_subscription` as a member next to the object whose
> lifetime the handler depends on; when that object dies, the subscription dies with it and no dangling callback
> remains.

> [!WARNING]
> `event`, `event_subscription` and `delegate` are not synchronised: subscribe, emit and release on one thread (the
> main thread for engine events). To raise an event from a worker, use `event_bus::enqueue` instead.

> [!NOTE]
> A subscription that outlives its event becomes inert: `is_active()` turns false and `release()` is a no-op.

## Delegates

`delegate<Return(Args...)>` holds exactly one callback in two pointers of inline storage (`STORAGE_BYTES`, 16 bytes
on 64-bit) with no heap allocation, which
makes it the right type for hot per-object callbacks (collision responses, timers) and for component fields. It never
owns anything: member bindings store the instance pointer, and lambdas must be trivially copyable and no larger than
two pointers (`CAN_BIND<Callable>` tells you; bigger ones fail to compile with a clear message).

```cpp
struct player {
    int health = 100;
    void take_damage(int amount) { health -= amount; }
};
void log_damage(int amount);   // a free function somewhere in your code

player hero = {};
int total = 0;
delegate<void(int)> onHit = delegate<void(int)>::bind<&player::take_damage>(&hero);   // member
onHit(10);
onHit = delegate<void(int)>::bind([&total](int amount) { total += amount; });          // small lambda
onHit(5);
onHit = delegate<void(int)>::bind<&log_damage>();                                       // free function
if (onHit) {
    onHit(1);
}
```

Delegates are comparable (same target on the same instance), `reset()` unbinds, and `is_bound()` / `operator bool`
tell you whether a target is set.

> [!NOTE]
> Equality compares the bound target and its stored bytes. It is exact for free functions, member functions,
> function pointers and lambdas that capture nothing. A capturing lambda is stored whole, padding included, so two
> bindings with equal captures compare equal only when the closure has no padding
> (`std::has_unique_object_representations_v`); a lambda capturing a `bool` next to a pointer may compare unequal
> to an identical one. Do not rely on `==` to find such a binding again.

> [!TIP]
> Several listeners, or a lambda that owns state? Use `event<>` (multicast, owning handlers) or `std::function`. A
> `delegate` is for exactly one, non-owning callback.

> [!WARNING]
> Invoking an unbound delegate asserts and returns a default-constructed value. The bound instance is referenced,
> not owned: reset or rebind the delegate before the instance is destroyed.

## The event bus

`event_bus` is typed publish/subscribe for messages that cross module boundaries (`asset_reloaded`,
`window_resized`, your own `level_completed`). An event is a plain copyable struct; its type is the channel.

```cpp
struct level_completed {
    int levelIndex = 0;
    f32 seconds = 0.0f;
};

event_bus bus = {};
int highestLevel = 0;
const event_subscription token = bus.subscribe<level_completed>([&highestLevel](const level_completed &done) {
    highestLevel = std::max(highestLevel, done.levelIndex);
});

bus.publish(level_completed{3, 42.5f});   // synchronous, delivered before publish returns
bus.enqueue(level_completed{4, 10.0f});   // from any thread: stored for the next dispatch_queued()
const usize delivered = bus.dispatch_queued();   // main thread; the engine calls this once per frame
```

- `publish(value)` delivers to every subscriber of that exact type now, on the calling thread. Publishing a type
  nobody listens to does nothing.
- `enqueue(value)` copies the event into a queue; `dispatch_queued()` publishes everything queued before the call,
  in order, and returns how many events it published (types without a subscriber count too). Events enqueued during
  a dispatch, by handlers or other threads, wait for the next one unless a handler re-enters `dispatch_queued()`.
- `queued_count()` and `clear_queue()` inspect and drop the queue. `subscriber_count<Event>()` is per type.

> [!WARNING]
> Only `enqueue`, `queued_count` and `clear_queue` are thread-safe. `subscribe`, `publish` and `dispatch_queued` are
> main-thread calls.

> [!IMPORTANT]
> The engine drains its bus once per frame. If you own a bus, call `dispatch_queued()` yourself or queued events are
> never delivered.

> [!TIP]
> Prefer `enqueue` for anything that originates off the main thread or inside a deep call stack (asset loading,
> physics callbacks): the handler then runs at a predictable point in the frame instead of in the middle of
> someone else's update. Use `publish` when the listener must react before the publisher continues.

> [!NOTE]
> Event types are identified by their plain type: `subscribe<const my_event>` or `subscribe<my_event &>` fails to
> compile. The subscription has the same semantics as for `event<>`: RAII, move-only, inert once the bus is gone.

## API summary

| Signature (short) | Purpose |
|---|---|
| `f64 clock::now_seconds() const`, `void clock::sleep_until(f64 seconds)` | The clock interface |
| `steady_clock`, `manual_clock(f64 startSeconds = 0)` | Real and simulated clocks |
| `void manual_clock::advance(f64)`, `set(f64)` | Step or jump the simulated time |
| `clock &default_clock()` | The process-wide steady clock |
| `u64 ticks_now()`, `f64 ticks_to_seconds(u64)`, `u64 ticks_per_second()` | Raw profiling ticks |
| `stopwatch()`, `stopwatch(const clock &)` | Start measuring |
| `void stopwatch::start()`, `f64 elapsed_seconds() const`, `f64 restart()`, `const clock &source() const` | Stopwatch operations |
| `random::engine`, `random::DEFAULT_SEED` | `std::mt19937_64` and the default seed `0x5EED` |
| `random::engine &random::default_engine()` | This thread's engine |
| `void random::seed(u64)`, `u64 random::current_seed()` | Process seed |
| `T random::uniform(Generator &, T min, T max)` | Inclusive integer or half-open float range |
| `bool random::chance(Generator &, f64 probability)` | Weighted coin flip |
| `Vector3 random::unit_vector3<Vector3>(Generator &)` | Random direction |
| `random::random_engine(u64 seed)`: `seed()`, `current_seed()`, `discard(n)`, `raw()`, `uniform()`, `chance()`, `unit_vector3()` | Owned engine |
| `event<Args...>`: `event_subscription subscribe(handler)`, `void emit(Args...) const`, `usize subscriber_count() const` | Multicast event |
| `event_subscription`: `void release()`, `bool is_active() const` | Subscription token (move-only) |
| `delegate<R(Args...)>::bind<&fn>()`, `bind<&type::method>(instance)`, `bind(functionPointer)`, `bind(callable)` | Bind a target |
| `R delegate::operator()(Args...) const`, `bool is_bound() const`, `void reset()`, `operator==` | Invoke, query, unbind |
| `delegate::STORAGE_BYTES`, `delegate::CAN_BIND<Callable>` | Capacity and the bindability test |
| `event_subscription event_bus::subscribe<Event>(handler)` | Listen to one event type |
| `void event_bus::publish(const Event &)` | Deliver now (main thread) |
| `void event_bus::enqueue(Event)` | Defer to the next dispatch (any thread) |
| `usize event_bus::dispatch_queued()` | Publish queued events (main thread) |
| `usize event_bus::queued_count() const`, `void clear_queue()`, `usize subscriber_count<Event>() const` | Queue and subscriber queries |

## See also

- 📘 API: [Jobs and threading](jobs.md) for what may run off the main thread
- 📘 API: [Math](math.md) for `math::vec3` with `random::unit_vector3`
- 🛠 Handbook: [Core module](../../handbook/modules/core.md)
