#pragma once

#include "conjure/core/log.h"

// The editor's log category: every message of conjure_editor_core, conjure_editor_ui and the conjure_editor
// executable goes through LOG_EDITOR ("editor"), so the console panel and the protocol's `log` event can filter
// it like any other engine category.

namespace conjure::editor {

    CONJURE_DECLARE_ENGINE_LOG_CATEGORY(LOG_EDITOR);
}
