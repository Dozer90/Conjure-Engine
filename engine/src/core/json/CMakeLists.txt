# core/json - yyjson wrapper: value views, owning documents, streaming writer, canonical output.
#
# yyjson is a private dependency: it is included only from this directory's sources, never from
# the public headers, which carry opaque pointers instead.

conjure_add_core_area(json
    PUBLIC_HEADERS
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/json/document.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/json/json.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/json/value.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/json/writer.h
    SOURCES
        document.cpp
        json.cpp
        json_internal.h
        value.cpp
        writer.cpp
    PUBLIC_DEPS
        conjure::core_foundation
    PRIVATE_DEPS
        conjure::thirdparty::yyjson
)
