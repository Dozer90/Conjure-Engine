#include "conjure/editor/event_broadcaster.h"

#include "conjure/editor/command_registry.h"

#include "conjure/core/assert.h"

#include <algorithm>
#include <utility>

namespace conjure::editor {

    std::string_view to_string(editor_event event) noexcept {
        const std::span<const std::string_view> names = protocol_event_names();
        const usize index = static_cast<usize>(event);
        if (index >= names.size()) {
            return "";
        }
        return names[index];
    }

    std::optional<editor_event> parse_editor_event(std::string_view name) noexcept {
        const std::span<const std::string_view> names = protocol_event_names();
        for (usize index = 0; index < names.size(); ++index) {
            if (names[index] == name) {
                return static_cast<editor_event>(index);
            }
        }
        return std::nullopt;
    }

    event_broadcaster::event_broadcaster() = default;
    event_broadcaster::~event_broadcaster() = default;

    u32 event_broadcaster::register_client(event_sink sink) {
        while (find(mNextClientId) != nullptr || mNextClientId == 0) {
            ++mNextClientId;
        }
        const u32 id = mNextClientId;
        ++mNextClientId;
        register_client(id, std::move(sink));
        return id;
    }

    void event_broadcaster::register_client(u32 clientId, event_sink sink) {
        if (client *existing = find(clientId); existing != nullptr) {
            // Re-registering replaces the sink and keeps the subscriptions (the server installs its serialising
            // sink over the placeholder the transport's allocator registered).
            existing->sink = std::move(sink);
            return;
        }
        client added = {};
        added.id = clientId;
        added.sink = std::move(sink);
        mClients.push_back(std::move(added));
        if (clientId >= mNextClientId) {
            mNextClientId = clientId + 1;
        }
    }

    void event_broadcaster::unregister_client(u32 clientId) {
        std::erase_if(mClients, [clientId](const client &entry) {
            return entry.id == clientId;
        });
    }

    bool event_broadcaster::has_client(u32 clientId) const noexcept {
        return find(clientId) != nullptr;
    }

    void event_broadcaster::set_subscriptions(u32 clientId, u64 mask) {
        client *entry = find(clientId);
        if (entry != nullptr) {
            entry->subscriptions = mask;
        }
    }

    u64 event_broadcaster::subscriptions(u32 clientId) const noexcept {
        const client *entry = find(clientId);
        return entry == nullptr ? 0 : entry->subscriptions;
    }

    void event_broadcaster::set_min_log_level(u32 clientId, core::log_level level) {
        client *entry = find(clientId);
        if (entry != nullptr) {
            entry->minLogLevel = level;
        }
    }

    core::log_level event_broadcaster::min_log_level(u32 clientId) const noexcept {
        const client *entry = find(clientId);
        return entry == nullptr ? core::log_level::OFF : entry->minLogLevel;
    }

    u64 event_broadcaster::emit(editor_event event, core::json::document data) {
        const protocol_event message = make_event(event, std::move(data));
        const u64 bit = event_bit(event);
        bool delivered = false;
        // Sinks may register or unregister clients: iterate over a copy of the ids.
        const std::vector<u32> ids = client_ids();
        for (const u32 id : ids) {
            const client *entry = find(id);
            if (entry == nullptr || (entry->subscriptions & bit) == 0 || !entry->sink) {
                continue;
            }
            event_sink sink = entry->sink;
            sink(message);
            delivered = true;
        }
        if (delivered) {
            ++mEmitted;
        }
        return message.seq;
    }

    u64 event_broadcaster::emit_log(core::log_level level, core::json::document data) {
        const protocol_event message = make_event(editor_event::LOG, std::move(data));
        const u64 bit = event_bit(editor_event::LOG);
        bool delivered = false;
        const std::vector<u32> ids = client_ids();
        for (const u32 id : ids) {
            const client *entry = find(id);
            if (entry == nullptr || (entry->subscriptions & bit) == 0 || !entry->sink || level < entry->minLogLevel) {
                continue;
            }
            event_sink sink = entry->sink;
            sink(message);
            delivered = true;
        }
        if (delivered) {
            ++mEmitted;
        }
        return message.seq;
    }

    u64 event_broadcaster::emit_to(u32 clientId, editor_event event, core::json::document data) {
        const protocol_event message = make_event(event, std::move(data));
        const client *entry = find(clientId);
        if (entry != nullptr && entry->sink) {
            event_sink sink = entry->sink;
            sink(message);
            ++mEmitted;
        }
        return message.seq;
    }

    std::vector<u32> event_broadcaster::client_ids() const {
        std::vector<u32> ids = {};
        ids.reserve(mClients.size());
        for (const client &entry : mClients) {
            ids.push_back(entry.id);
        }
        return ids;
    }

    event_broadcaster::client *event_broadcaster::find(u32 clientId) noexcept {
        for (client &entry : mClients) {
            if (entry.id == clientId) {
                return &entry;
            }
        }
        return nullptr;
    }

    const event_broadcaster::client *event_broadcaster::find(u32 clientId) const noexcept {
        for (const client &entry : mClients) {
            if (entry.id == clientId) {
                return &entry;
            }
        }
        return nullptr;
    }

    protocol_event event_broadcaster::make_event(editor_event event, core::json::document data) {
        protocol_event message = {};
        message.name = std::string(to_string(event));
        message.seq = mNextSeq;
        ++mNextSeq;
        message.frame = mFrame;
        message.data = std::move(data);
        if (!message.data.root().is_object()) {
            message.data = core::json::document::object();
        }
        return message;
    }
}
