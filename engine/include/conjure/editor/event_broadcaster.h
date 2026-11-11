#pragma once

#include "conjure/editor/protocol/protocol_message.h"

#include "conjure/core/api.h"
#include "conjure/core/json/document.h"
#include "conjure/core/log.h"
#include "conjure/core/types.h"

#include <functional>
#include <optional>
#include <string_view>
#include <vector>

// Events (docs/design/04_editor.md 2.4, 4.4): one monotonic sequence shared by every client, a subscription
// bitmask per client (default everything except `log` and `command.executed`) and a sink per client that
// receives the protocol_event. The protocol server registers its remote clients here; the local UI and the
// headless runner register with in-process sinks.

namespace conjure::editor {

    enum class editor_event : u8 {
        LOG = 0,
        SELECTION_CHANGED,
        PLAY_STATE,
        SCENE_CHANGED,
        SCENE_LOADED,
        PROGRESS,
        SCRIPT_COMPILED,
        SCRIPT_RELOADED,
        ASSET_IMPORTED,
        MODULE_STATE,
        COMMAND_EXECUTED,
        CLIENT_CONNECTED,
        CLIENT_DISCONNECTED,
        EVENTS_DROPPED,
        WARNING,
        COUNT
    };

    /// <summary>
    /// The wire name of an event ("selection.changed"); "" for COUNT.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(editor_event event) noexcept;

    /// <summary>
    /// Parses a wire name; nullopt for unknown names.
    /// </summary>
    [[nodiscard]] CONJURE_API std::optional<editor_event> parse_editor_event(std::string_view name) noexcept;

    /// <summary>
    /// The bit of an event inside a subscription mask.
    /// </summary>
    [[nodiscard]] constexpr u64 event_bit(editor_event event) noexcept {
        return u64{1} << static_cast<u32>(event);
    }

    /// <summary>
    /// Every event except LOG and COMMAND_EXECUTED.
    /// </summary>
    [[nodiscard]] constexpr u64 default_subscriptions() noexcept {
        u64 mask = 0;
        for (u32 index = 0; index < static_cast<u32>(editor_event::COUNT); ++index) {
            mask |= u64{1} << index;
        }
        mask &= ~event_bit(editor_event::LOG);
        mask &= ~event_bit(editor_event::COMMAND_EXECUTED);
        return mask;
    }

    /// <summary>
    /// Receives one event for one client.
    /// </summary>
    using event_sink = std::function<void(const protocol_event &event)>;

    class CONJURE_API event_broadcaster {
        private:
            struct client {
                u32 id = 0;
                u64 subscriptions = default_subscriptions();
                core::log_level minLogLevel = core::log_level::INFO;
                event_sink sink = {};
            };

            std::vector<client> mClients = {};
            u64 mNextSeq = 1;
            u32 mNextClientId = 1;
            u64 mFrame = 0;
            u64 mEmitted = 0;

        public:
            event_broadcaster();
            ~event_broadcaster();

            event_broadcaster(const event_broadcaster &) = delete;
            event_broadcaster &operator=(const event_broadcaster &) = delete;

        public:
            /// <summary>
            /// Registers a client with a fresh id (1, 2, ...) and the default subscriptions.
            /// </summary>
            [[nodiscard]] u32 register_client(event_sink sink);

            /// <summary>
            /// Registers a client under a chosen id (0 is the local UI); an existing client keeps its
            /// subscriptions and gets the new sink.
            /// </summary>
            void register_client(u32 clientId, event_sink sink);

            /// <summary>
            /// Forgets a client; unknown ids are ignored.
            /// </summary>
            void unregister_client(u32 clientId);

            [[nodiscard]] bool has_client(u32 clientId) const noexcept;

            /// <summary>
            /// Replaces the subscription set of a client; unknown ids are ignored.
            /// </summary>
            void set_subscriptions(u32 clientId, u64 mask);

            [[nodiscard]] u64 subscriptions(u32 clientId) const noexcept;

            /// <summary>
            /// The lowest level of `log` events the client receives.
            /// </summary>
            void set_min_log_level(u32 clientId, core::log_level level);

            [[nodiscard]] core::log_level min_log_level(u32 clientId) const noexcept;

            /// <summary>
            /// Delivers an event to every subscribed client, with the next sequence number and the current
            /// frame. Returns the sequence number used.
            /// </summary>
            u64 emit(editor_event event, core::json::document data);

            /// <summary>
            /// Delivers a `log` event to the subscribed clients whose minimum level allows it.
            /// </summary>
            u64 emit_log(core::log_level level, core::json::document data);

            /// <summary>
            /// Delivers an event to one client only (events.dropped, client.connected acknowledgements).
            /// </summary>
            u64 emit_to(u32 clientId, editor_event event, core::json::document data);

            /// <summary>
            /// The frame stamped on the next events.
            /// </summary>
            void set_frame(u64 frame) noexcept {
                mFrame = frame;
            }

            [[nodiscard]] u64 frame() const noexcept {
                return mFrame;
            }

            [[nodiscard]] u64 next_seq() const noexcept {
                return mNextSeq;
            }

            [[nodiscard]] usize client_count() const noexcept {
                return mClients.size();
            }

            /// <summary>
            /// Events delivered to at least one client since construction.
            /// </summary>
            [[nodiscard]] u64 emitted_count() const noexcept {
                return mEmitted;
            }

            /// <summary>
            /// Every registered client id.
            /// </summary>
            [[nodiscard]] std::vector<u32> client_ids() const;

        private:
            [[nodiscard]] client *find(u32 clientId) noexcept;
            [[nodiscard]] const client *find(u32 clientId) const noexcept;
            [[nodiscard]] protocol_event make_event(editor_event event, core::json::document data);
    };
}
