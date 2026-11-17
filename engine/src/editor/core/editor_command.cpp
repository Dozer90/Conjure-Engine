#include "conjure/editor/editor_command.h"

#include "conjure/editor/command_dispatcher.h"
#include "conjure/editor/protocol/protocol_message.h"

#include "conjure/core/assert.h"

#include <array>
#include <utility>

namespace conjure::editor {

    namespace {
        struct flag_name {
            command_flags flag = command_flags::NONE;
            std::string_view name = "";
        };

        constexpr std::array<flag_name, 11> FLAG_NAMES = {
            flag_name{command_flags::MUTATES, "mutates"},
            flag_name{command_flags::REQUIRES_PROJECT, "requires_project"},
            flag_name{command_flags::REQUIRES_SCENE, "requires_scene"},
            flag_name{command_flags::EDITING_ONLY, "editing_only"},
            flag_name{command_flags::PLAYING_ONLY, "playing_only"},
            flag_name{command_flags::ASYNC, "async"},
            flag_name{command_flags::UI_ONLY, "ui_only"},
            flag_name{command_flags::NOT_IN_BATCH, "not_in_batch"},
            flag_name{command_flags::NO_UNDO, "no_undo"},
            flag_name{command_flags::ON_DISK, "on_disk"},
            flag_name{command_flags::READ_ONLY, "read_only"},
        };
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // async_reply

    async_reply::async_reply() noexcept = default;

    async_reply::async_reply(command_dispatcher &dispatcher, u64 ticket) noexcept :
        mDispatcher(&dispatcher),
        mTicket(ticket) {}

    async_reply::~async_reply() = default;

    void async_reply::succeed(core::json::document result) const {
        if (!valid()) {
            return;
        }
        protocol_response response = {};
        response.ok = true;
        response.result = std::move(result);
        mDispatcher->complete(mTicket, std::move(response));
    }

    void async_reply::fail(protocol_error_code code, std::string message, core::json::document details) const {
        if (!valid()) {
            return;
        }
        protocol_response response = {};
        response.ok = false;
        response.errorCode = code;
        response.errorMessage = std::move(message);
        response.errorDetails = std::move(details);
        mDispatcher->complete(mTicket, std::move(response));
    }

    bool async_reply::valid() const noexcept {
        return mDispatcher != nullptr && mTicket != 0 && mDispatcher->is_pending(mTicket);
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // Flags

    core::json::document command_flag_names(command_flags flags) {
        core::json::document names = core::json::document::array();
        for (const flag_name &entry : FLAG_NAMES) {
            if (has_flag(flags, entry.flag)) {
                names.push_back(entry.name);
            }
        }
        return names;
    }

    std::string_view command_flag_name(command_flags flag) noexcept {
        for (const flag_name &entry : FLAG_NAMES) {
            if (entry.flag == flag) {
                return entry.name;
            }
        }
        return "";
    }
}
