#pragma once

#include "conjure/ecs/archetype.h"
#include "conjure/ecs/chunk_view.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/component_type_desc.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/entity_archive.h"
#include "conjure/ecs/entity_handle.h"
#include "conjure/ecs/log_category.h"
#include "conjure/ecs/observer.h"
#include "conjure/ecs/query.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/serialization.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/ecs/view.h"
#include "conjure/ecs/world.h"
#include "conjure/ecs/world_hash.h"

// Umbrella header of the ecs module (docs/design/01_ecs.md). Include the individual headers in engine
// code; this one is for tests, tools and samples.
