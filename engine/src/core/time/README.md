# core/time

Clocks, random numbers, events, delegates and the engine-wide event bus: the small, allocation-light
primitives the main loop and every module use to measure time and talk to each other. Static library
`conjure::core_time`; depends only on `conjure::core_foundation`. Design: `docs/design/10_core.md` 2.6.

## Types

| Header | Types | Purpose |
|---|---|---|
| `clock.h` | `clock`, `steady_clock`, `manual_clock`, `stopwatch`, `default_clock()`, `ticks_now()`, `ticks_to_seconds()`, `ticks_per_second()` | Time sources in seconds (`f64`). `steady_clock` wraps `std::chrono::steady_clock`, counts from the first use in the process and `sleep_until` sleeps until 2 ms (`SPIN_WINDOW_SECONDS`) before the deadline, then spins. `manual_clock` only moves through `advance`/`set`/`sleep_until`; negative or NaN `advance` asserts and is ignored. `stopwatch` measures any injected clock (default: `default_clock()`, the process-wide steady clock); `restart()` returns the interval and reopens it without a gap. Raw ticks are for profiling. |
| `random.h` | `random::engine` (= `std::mt19937_64`), `random::default_engine()`, `random::seed()`, `random::current_seed()`, `random::uniform`, `random::chance`, `random::unit_vector3`, `random::random_engine` | Seeded, replayable randomness. The helpers implement their own bounded-integer and unit-float draws so the same seed gives the same values on every platform (the standard distributions do not guarantee that). Integers are inclusive `[min, max]`, floats half-open `[min, max)` (ranges whose span overflows, such as `[-1e308, 1e308)`, are handled), `min > max` asserts and returns `min`. `test_random.cpp` pins the first outputs of every helper for `DEFAULT_SEED` as golden values (`unit_vector3`'s x and y excepted: they use libm `cos`/`sin`, and the golden test compares them with a tolerance; its two engine draws and its z are exact everywhere): changing them is a deliberate break of seeded replays. `unit_vector3<V>` builds any three-float type (`std::array<f32, 3>` by default, `math::vec3` when available). `random_engine` owns an engine and satisfies `std::uniform_random_bit_generator`. |
| `event.h` | `event<Args...>`, `event_subscription` | Multicast callbacks in subscription order with RAII, move-only subscriptions. Reentrancy-safe: handlers may subscribe (first invoked by the next emit), unsubscribe (never invoked again, including later in the running emit), emit recursively or destroy the event. Unsubscribed handlers are destroyed only after the subscriber list is consistent again, so a handler's captured state may own other subscriptions to the same event. A subscription that outlives its event is inert. Events are movable; subscriptions follow the moved state. |
| `delegate.h` | `delegate<Return(Args...)>` | Single-target, non-owning, allocation-free callback: 16 bytes of inline storage plus the invoker. Binds free functions (`bind<&fn>()` or a runtime pointer), member functions (`bind<&type::method>(&instance)`, const or not) and trivially copyable callables of at most two pointers (`CAN_BIND<Callable>` tells; larger ones fail a `static_assert`). Comparable: `operator==` compares the invoker and the raw inline storage, which is exact for free-function, member, function-pointer and stateless-lambda bindings; a capturing lambda is copied whole, so its equality is exact only when the closure has no padding (`std::has_unique_object_representations_v`), and equal captures with padding (a `bool` next to a pointer) may compare unequal. Invoking an unbound delegate asserts and returns a default value. |
| `event_bus.h` | `event_bus` | Typed publish/subscribe keyed by `std::type_index`. `publish` is synchronous; `enqueue` stores a copy for `dispatch_queued()`, which the engine drains on the main thread each frame and which returns the number of events published (listened to or not). Events enqueued while dispatching wait for the next dispatch unless a handler re-enters `dispatch_queued()`. Event types must be plain (no cv/ref qualifiers); every public template enforces that at compile time. |

## Threading model

- `clock`, `stopwatch`, `event<>`, `event_subscription`, `delegate` and the subscribe/publish/dispatch
  half of `event_bus` are main-thread objects: no synchronisation, documented as such in the headers.
  `steady_clock::now_seconds()` and the tick functions are safe from any thread (they hold no state).
- `event_bus::enqueue`, `queued_count` and `clear_queue` are thread-safe (mutex-guarded queue, swapped
  out wholesale before dispatch so handlers never run under the lock; the emptied buffer is handed back
  afterwards when nothing was enqueued meanwhile, so enqueueing after a dispatch does not reallocate).
- `random::default_engine()` is `thread_local`; `random::seed()` is thread-safe and reseeds the
  calling thread immediately while recording the seed future threads start from.

## Testing

`tests/core/time/` builds `conjure_core_time_tests` (one file per header). Everything is driven by
`manual_clock` and fixed seeds; the only wall-clock dependent checks are the `steady_clock` monotonicity
case and the default `stopwatch` case (a 2 ms wait, checked against generous bounds). Thread-safety
of `enqueue` and of the per-thread random engines is covered with `std::thread` in the tests only.

```
python tools/build.py --build-dir build/core_time --target conjure_core_time_tests --test -R core_time
build/core_time/bin/conjure_core_time_tests
```
