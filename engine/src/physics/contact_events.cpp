#include "conjure/physics/contact_events.h"

namespace conjure::physics {

std::string_view to_string(contact_kind kind) noexcept {
    switch (kind) {
        case contact_kind::ENTER:
            return "ENTER";
        case contact_kind::STAY:
            return "STAY";
        case contact_kind::EXIT:
            return "EXIT";
        case contact_kind::TRIGGER_ENTER:
            return "TRIGGER_ENTER";
        case contact_kind::TRIGGER_EXIT:
            return "TRIGGER_EXIT";
    }
    return "UNKNOWN";
}
}
