#include "conjure/editor/protocol/protocol_transport.h"

#include <utility>

namespace conjure::editor {

    std::string_view to_string(transport_kind kind) noexcept {
        switch (kind) {
            case transport_kind::LOCAL:
                return "local";
            case transport_kind::TCP:
                return "tcp";
            case transport_kind::STDIO:
                return "stdio";
            case transport_kind::LOOPBACK:
                return "loopback";
        }
        return "";
    }

    protocol_transport::protocol_transport() = default;
    protocol_transport::~protocol_transport() = default;

    void protocol_transport::set_client_id_allocator(client_id_allocator allocator) {
        mAllocator = std::move(allocator);
    }

    u32 protocol_transport::allocate_client_id() {
        if (mAllocator) {
            return mAllocator();
        }
        const u32 id = mNextLocalId;
        ++mNextLocalId;
        return id;
    }
}
