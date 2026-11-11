#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/log_capture.h"
#include "engine_test_support.h"

#include "conjure/engine/engine_context.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/layer.h"
#include "conjure/engine/layer_stack.h"
#include "conjure/platform/event.h"

#include <memory>
#include <string>
#include <vector>

// layer_stack.h (docs/design/06_engine_loop.md 2.8): ordering of layers and overlays, top-down event
// dispatch, bottom-up updates, deferred push/pop and the attach unwind.

using namespace conjure;
using namespace conjure::engine;
using namespace conjure::testing;
using engine_test::probe_layer;

namespace {

    /// <summary>
    /// A layer that pops itself (or another) while an event is dispatched.
    /// </summary>
    class popping_layer final : public layer {
        private:
            layer_stack *mStack = nullptr;
            std::vector<std::string> *mLog = nullptr;

        public:
            popping_layer(layer_stack &stack, std::vector<std::string> &log) :
                layer("popper"),
                mStack(&stack),
                mLog(&log) {}
            ~popping_layer() override {
                mLog->push_back("popper.destroyed");
            }

        public:
            [[nodiscard]] bool on_event(engine_context &, const platform::event &) override {
                mLog->push_back("popper.event");
                mStack->pop(*this);
                return false;
            }
    };
}

TEST_SUITE("engine/layer_stack") {

    TEST_CASE("layer_stack - layers sit below overlays whatever the push order") {
        std::vector<std::string> log = {};
        layer_stack stack = {};
        stack.push_overlay(std::make_unique<probe_layer>("overlay1", log));
        stack.push_layer(std::make_unique<probe_layer>("layer1", log));
        stack.push_layer(std::make_unique<probe_layer>("layer2", log));
        stack.push_overlay(std::make_unique<probe_layer>("overlay2", log));
        REQUIRE(stack.layers().size() == 4);
        CHECK(stack.layers()[0]->name() == "layer1");
        CHECK(stack.layers()[1]->name() == "layer2");
        CHECK(stack.layers()[2]->name() == "overlay1");
        CHECK(stack.layers()[3]->name() == "overlay2");
        CHECK(stack.overlay_count() == 2);
    }

    TEST_CASE("layer_stack - events go top-down, stop at the consumer and reach overlays first") {
        std::vector<std::string> log = {};
        layer_stack stack = {};
        stack.push_layer(std::make_unique<probe_layer>("bottom", log));
        stack.push_layer(std::make_unique<probe_layer>("middle", log, true));
        stack.push_overlay(std::make_unique<probe_layer>("overlay", log));
        engine_context context = {};
        const platform::event closing = platform::close_event{};
        CHECK(stack.dispatch_event(context, closing));
        const std::vector<std::string> expected = {"overlay.event", "middle.event"};
        CHECK(log == expected);
    }

    TEST_CASE("layer_stack - an unconsumed event reaches every layer and reports false") {
        std::vector<std::string> log = {};
        layer_stack stack = {};
        stack.push_layer(std::make_unique<probe_layer>("a", log));
        stack.push_layer(std::make_unique<probe_layer>("b", log));
        engine_context context = {};
        CHECK_FALSE(stack.dispatch_event(context, platform::event(platform::close_event{})));
        CHECK(log.size() == 2);
    }

    TEST_CASE("layer_stack - updates and frame hooks run bottom-up") {
        std::vector<std::string> log = {};
        layer_stack stack = {};
        stack.push_overlay(std::make_unique<probe_layer>("overlay", log));
        stack.push_layer(std::make_unique<probe_layer>("layer", log));
        engine_context context = {};
        stack.frame_begin(context);
        stack.update(context);
        stack.imgui(context);
        stack.imgui_end(context);
        stack.frame_end(context);
        const std::vector<std::string> expected = {
            "layer.frame_begin", "overlay.frame_begin", "layer.update", "overlay.update", "layer.imgui",
            "overlay.imgui",
            "layer.imgui_end", "overlay.imgui_end", "layer.frame_end", "overlay.frame_end"
        };
        CHECK(log == expected);
    }

    TEST_CASE("layer_stack - a pop during dispatch is deferred exactly once") {
        std::vector<std::string> log = {};
        layer_stack stack = {};
        stack.push_layer(std::make_unique<probe_layer>("bottom", log));
        stack.push_layer(std::make_unique<popping_layer>(stack, log));
        engine_context context = {};
        CHECK_FALSE(stack.dispatch_event(context, platform::event(platform::close_event{})));
        CHECK(stack.layers().size() == 2);
        CHECK(stack.pending_count() == 1);
        stack.apply_pending(context);
        CHECK(stack.layers().size() == 1);
        CHECK(stack.pending_count() == 0);
        CHECK(stack.layers()[0]->name() == "bottom");
        const std::vector<std::string> expected = {"popper.event", "bottom.event", "popper.destroyed"};
        CHECK(log == expected);
    }

    TEST_CASE("layer_stack - a push while attached waits for apply_pending and attaches then") {
        std::vector<std::string> log = {};
        layer_stack stack = {};
        engine_context context = {};
        stack.push_layer(std::make_unique<probe_layer>("first", log));
        CONJURE_REQUIRE_OK(stack.attach_all(context));
        CHECK(stack.is_attached());
        stack.push_layer(std::make_unique<probe_layer>("late", log));
        CHECK(stack.layers().size() == 1);
        CHECK(stack.pending_count() == 1);
        stack.apply_pending(context);
        CHECK(stack.layers().size() == 2);
        const std::vector<std::string> expected = {"first.attach", "late.attach"};
        CHECK(log == expected);
        stack.detach_all(context);
        CHECK_FALSE(stack.is_attached());
        CHECK(log.back() == "first.detach");
        CHECK(log[log.size() - 2] == "late.detach");
    }

    TEST_CASE("layer_stack - apply_pending with nothing queued allocates nothing") {
        // The frame loop calls it every frame (step 13); the deferred pushes and pops are the rare case and the
        // only one that may touch the heap.
        std::vector<std::string> log = {};
        layer_stack stack = {};
        engine_context context = {};
        stack.push_layer(std::make_unique<probe_layer>("first", log));
        stack.push_overlay(std::make_unique<probe_layer>("overlay", log));
        CONJURE_REQUIRE_OK(stack.attach_all(context));
        for (u32 frame = 0; frame < 3; ++frame) {
            CAPTURE(frame);
            CONJURE_CHECK_NO_ALLOCATIONS(stack.apply_pending(context));
        }
        CHECK(stack.layers().size() == 2);
        CHECK(stack.pending_count() == 0);
        stack.detach_all(context);
    }

    TEST_CASE("layer_stack - destruction runs top first") {
        std::vector<std::string> log = {};
        {
            layer_stack stack = {};
            stack.push_layer(std::make_unique<probe_layer>("layer", log));
            stack.push_overlay(std::make_unique<probe_layer>("overlay", log));
        }
        const std::vector<std::string> expected = {"overlay.destroyed", "layer.destroyed"};
        CHECK(log == expected);
    }

    TEST_CASE("layer_stack - a failing attach unwinds the attached layers and returns LAYER_ATTACH_FAILED") {
        std::vector<std::string> log = {};
        layer_stack stack = {};
        stack.push_layer(std::make_unique<probe_layer>("ok", log));
        stack.push_layer(std::make_unique<probe_layer>("bad", log, false, true));
        stack.push_layer(std::make_unique<probe_layer>("never", log));
        engine_context context = {};
        CONJURE_CHECK_ERROR(stack.attach_all(context), engine_error::LAYER_ATTACH_FAILED);
        CHECK_FALSE(stack.is_attached());
        const std::vector<std::string> expected = {"ok.attach", "bad.attach", "ok.detach"};
        CHECK(log == expected);
    }

    TEST_CASE("layer_stack - a failing late push is dropped with an error log") {
        log_capture logs = {};
        logs.allow_errors();
        std::vector<std::string> log = {};
        layer_stack stack = {};
        engine_context context = {};
        CONJURE_REQUIRE_OK(stack.attach_all(context));
        stack.push_layer(std::make_unique<probe_layer>("bad", log, false, true));
        stack.apply_pending(context);
        CHECK(stack.layers().empty());
        CHECK(logs.contains(core::log_level::ERR, "bad"));
        CHECK(log.back() == "bad.destroyed");
    }

    TEST_CASE("regression - review finding 2: popping an attached layer outside an iteration detaches it") {
        std::vector<std::string> log = {};
        layer_stack stack = {};
        engine_context context = {};
        layer *first = stack.push_layer(std::make_unique<probe_layer>("first", log));
        stack.push_layer(std::make_unique<probe_layer>("second", log));
        REQUIRE(first != nullptr);
        CONJURE_REQUIRE_OK(stack.attach_all(context));

        // Between frames (a main-thread task, editor code): the pop waits for apply_pending so on_detach
        // runs with the real context instead of the layer being destroyed attached.
        stack.pop(*first);
        CHECK(stack.layers().size() == 2);
        CHECK(stack.pending_count() == 1);
        CHECK(engine_test::filter_log(log, "first.detach").empty());
        CHECK(engine_test::filter_log(log, "first.destroyed").empty());
        stack.apply_pending(context);
        REQUIRE(stack.layers().size() == 1);
        CHECK(stack.layers()[0]->name() == "second");
        CHECK(stack.pending_count() == 0);
        const std::vector<std::string> expected = {"first.attach", "second.attach", "first.detach", "first.destroyed"};
        CHECK(log == expected);
        stack.detach_all(context);
        CHECK(log.back() == "second.detach");
    }

    TEST_CASE("layer_stack - popping before attach_all destroys the layer at once") {
        std::vector<std::string> log = {};
        layer_stack stack = {};
        layer *only = stack.push_layer(std::make_unique<probe_layer>("only", log));
        REQUIRE(only != nullptr);
        stack.pop(*only);
        CHECK(stack.layers().empty());
        CHECK(stack.pending_count() == 0);
        const std::vector<std::string> expected = {"only.destroyed"};
        CHECK(log == expected);
    }

    TEST_CASE("layer_stack - popping a pending push forgets it") {
        std::vector<std::string> log = {};
        layer_stack stack = {};
        engine_context context = {};
        CONJURE_REQUIRE_OK(stack.attach_all(context));
        layer *pending = stack.push_layer(std::make_unique<probe_layer>("pending", log));
        REQUIRE(pending != nullptr);
        stack.pop(*pending);
        CHECK(stack.pending_count() == 0);
        CHECK(stack.layers().empty());
        CHECK(log.back() == "pending.destroyed");
    }
}
