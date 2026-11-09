#pragma once

#include "conjure/modules/module_context.h"
#include "conjure/modules/module_entry.h"
#include "conjure/modules/module_registrar.h"
#include "conjure/modules/module_service.h"

// Umbrella header for module authors: everything an entry function needs.
//
//   #include "conjure/modules/modules.h"
//
//   namespace {
//       conjure::modules::module_entry_status my_entry(conjure::modules::module_context &context) {
//           context.registrar().register_extension(my_component_desc());
//           return conjure::modules::module_entry_status::OK;
//       }
//   }
//
//   CONJURE_DECLARE_MODULE(my_entry)
//
// Hosts include module_manager.h, module_consumer.h and module_events.h directly.
