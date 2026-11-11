#pragma once

// The status codes every bindings-owned native entry may return (docs/design/15_bindings.md 2.2, B4):
// one CONJURE_BINDINGS_STATUS(NAME) per enumerator, in value order starting at NONE = 0, inside the
// CONJURE_BINDINGS_FOR_EACH_STATUS(X) list macro. bindings_error.h expands it into the enum and the name
// table; tools/generate_native_api.py --status reads the same lines and emits
// managed/Conjure.Managed/Interop/BindingStatus.g.cs. The list is append-only and is not part of the
// native API layout hash.
//
// clang-format off
#define CONJURE_BINDINGS_FOR_EACH_STATUS(CONJURE_BINDINGS_STATUS)                                                      \
    CONJURE_BINDINGS_STATUS(NONE)                /* success */                                                         \
    CONJURE_BINDINGS_STATUS(NOT_SUPPORTED)       /* subsystem compiled out, disabled or null backend */              \
    CONJURE_BINDINGS_STATUS(ENTITY_NOT_ALIVE)                                                                          \
    CONJURE_BINDINGS_STATUS(COMPONENT_MISSING)   /* entity lacks the component the entry operates on */                \
    CONJURE_BINDINGS_STATUS(INVALID_ARGUMENT)    /* null pointer with a length, bad capacity or enum value */        \
    CONJURE_BINDINGS_STATUS(NOT_FOUND)           /* path, name, layer, module, guid */                                 \
    CONJURE_BINDINGS_STATUS(BUFFER_TOO_SMALL)    /* record-array entries that cannot report a count */                 \
    CONJURE_BINDINGS_STATUS(WRONG_THREAD)                                                                              \
    CONJURE_BINDINGS_STATUS(NOT_IN_PHASE)        /* called outside a script phase (e.g. from a Task) */                \
    CONJURE_BINDINGS_STATUS(STRUCTURE_LOCKED)    /* structural request inside a query scope that cannot be deferred */ \
    CONJURE_BINDINGS_STATUS(ASSET_NOT_LOADED)                                                                          \
    CONJURE_BINDINGS_STATUS(ASSET_TYPE_MISMATCH)                                                                       \
    CONJURE_BINDINGS_STATUS(SCENE_NOT_FOUND)                                                                           \
    CONJURE_BINDINGS_STATUS(PREFAB_INVALID)      /* asset loaded but the payload is not an entity archive */           \
    CONJURE_BINDINGS_STATUS(TABLE_INCOMPLETE)    /* startup: an owned slot could not be filled (tests only) */         \
    CONJURE_BINDINGS_STATUS(INTERNAL)            /* any other core::error from a called module; the message says */
// clang-format on
