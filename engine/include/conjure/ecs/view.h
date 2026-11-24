#pragma once

#include "conjure/ecs/chunk_view.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/query.h"

#include "conjure/core/assert.h"
#include "conjure/core/types.h"

#include <array>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

// Typed views (docs/design/01_ecs.md 2.4, 4.4).
//
//   world.view<const position, velocity>().for_each([](entity id, const position &p, velocity &v) { ... });
//   world.view<position, maybe<const health>>().for_each([](entity, position &p, const health *h) { ... });
//
// A const type in the pack is read, a non-const type is written (its column is stamped), maybe<T> is
// optional and arrives as a pointer that is null when the archetype lacks it. A tag has no column, so
// a tag in the pack (plain or maybe<Tag>) does not compile. Builder methods narrow the match:
// with<Tag>() requires a tag, without<T>() excludes a component, changed<T>(since) visits only chunks
// whose T column changed after 'since'. On a named view they narrow it in place and may stand alone as
// statements; on a temporary they return the narrowed view by value and must be used. A required type
// (pack or builder) that is not registered asserts naming the type and makes the view match nothing; an
// unregistered maybe<T> is simply absent everywhere.

namespace conjure::ecs {

    class command_buffer;
    class system_context;
    class world;

    /// <summary>
    /// Marks an optional component in a view pack. The callback receives T * (null when absent).
    /// </summary>
    template <typename ComponentType>
    struct maybe {};

    /// <summary>
    /// The component sets a view reads and writes, derived from its pack.
    /// </summary>
    struct view_access {
        component_mask reads = {};
        component_mask writes = {};
    };

    namespace detail {
        template <typename PackType>
        struct view_arg_traits {
            using component_type = std::remove_cv_t<PackType>;
            using span_type = std::span<PackType>;
            using callback_type = PackType &;
            static constexpr bool IS_CONST = std::is_const_v<PackType>;
            static constexpr bool IS_OPTIONAL = false;
        };

        template <typename ComponentType>
        struct view_arg_traits<maybe<ComponentType>> {
            using component_type = std::remove_cv_t<ComponentType>;
            using span_type = std::span<ComponentType>;
            using callback_type = ComponentType *;
            static constexpr bool IS_CONST = std::is_const_v<ComponentType>;
            static constexpr bool IS_OPTIONAL = true;
        };

        /// <summary>
        /// False for a tag (an empty type, plain or in maybe&lt;&gt;): it has no column a view could
        /// hand to the callback, so it is required with view::with&lt;Tag&gt;() instead.
        /// </summary>
        template <typename PackType>
        inline constexpr bool HAS_COLUMN = !std::is_empty_v<typename view_arg_traits<PackType>::component_type>;

        template <typename PackType>
        [[nodiscard]] typename view_arg_traits<PackType>::span_type fetch_column(chunk_view &view) noexcept {
            using traits = view_arg_traits<PackType>;
            if constexpr (traits::IS_OPTIONAL) {
                return view.get_optional<typename traits::span_type::element_type>();
            } else {
                return view.get<typename traits::span_type::element_type>();
            }
        }

        template <typename PackType>
        [[nodiscard]] typename view_arg_traits<PackType>::callback_type row_argument(
          typename view_arg_traits<PackType>::span_type column,
          u32 row
        ) noexcept {
            using traits = view_arg_traits<PackType>;
            if constexpr (traits::IS_OPTIONAL) {
                return column.empty() ? nullptr : &column[row];
            } else {
                return column[row];
            }
        }
    }

    template <typename... ComponentTypes>
    class view {
        private:
            static_assert(
              (detail::HAS_COLUMN<ComponentTypes> && ...),
              "a tag has no column: require it with view::with<Tag>() instead of the pack"
            );

            static constexpr usize PACK_SIZE = sizeof...(ComponentTypes);

            query mQuery;                    // no default: built from the world in the constructor
            std::array<component_id, PACK_SIZE> mIds = {};
            bool mValid = true;              // false once an unregistered type was required: matches nothing

        public:
            /// <summary>
            /// A view over every entity of 'target' holding the required components of the pack.
            /// </summary>
            explicit view(world &target) :
                mQuery(target, query_desc{}) {
                const component_registry &registry = mQuery.registry();
                query_desc desc = {};
                usize slot = 0;
                (fill_slot<ComponentTypes>(registry, desc, slot), ...);
                if (!mValid) {
                    desc.with = MATCH_NOTHING;
                }
                mQuery.reset(std::move(desc));
            }

            ~view() = default;

            view(const view &) = default;
            view &operator=(const view &) = default;
            view(view &&) noexcept = default;
            view &operator=(view &&) noexcept = default;

        public:
            // Each builder comes as a pair. On a named view it narrows that view in place and returns it, so a
            // refinement can stand alone as a statement ('enemies.without<scene::disabled>();'). On a temporary
            // it narrows the temporary and moves it into the result, so a chain bound to a reference
            // ('auto &&refined = world.view<A>().with<B>();') extends the lifetime of a whole view instead of
            // dangling; [[nodiscard]] there keeps catching a refined temporary that is dropped unused.

            /// <summary>
            /// Requires a component (typically a tag) without reading it. Narrows this named view in place and
            /// returns it for chaining.
            /// </summary>
            template <typename TagType>
            view &with() & {
                require<TagType>();
                return *this;
            }

            /// <summary>
            /// Requires a component (typically a tag) without reading it. Narrows this temporary view and
            /// returns it by value (a move), so binding the chain to a reference cannot dangle; discarding the
            /// result warns.
            /// </summary>
            template <typename TagType>
            [[nodiscard]] view with() && {
                require<TagType>();
                return std::move(*this);
            }

            /// <summary>
            /// Excludes entities holding a component. Narrows this named view in place and returns it for
            /// chaining.
            /// </summary>
            template <typename OtherType>
            view &without() & {
                exclude<OtherType>();
                return *this;
            }

            /// <summary>
            /// Excludes entities holding a component. Narrows this temporary view and returns it by value (a
            /// move), so binding the chain to a reference cannot dangle; discarding the result warns.
            /// </summary>
            template <typename OtherType>
            [[nodiscard]] view without() && {
                exclude<OtherType>();
                return std::move(*this);
            }

            /// <summary>
            /// Visits only chunks whose column of OtherType was stamped after 'sinceVersion'. Narrows this
            /// named view in place and returns it for chaining.
            /// </summary>
            template <typename OtherType>
            view &changed(u64 sinceVersion) & {
                filter_changed<OtherType>(sinceVersion);
                return *this;
            }

            /// <summary>
            /// Visits only chunks whose column of OtherType was stamped after 'sinceVersion'. Narrows this
            /// temporary view and returns it by value (a move), so binding the chain to a reference cannot
            /// dangle; discarding the result warns.
            /// </summary>
            template <typename OtherType>
            [[nodiscard]] view changed(u64 sinceVersion) && {
                filter_changed<OtherType>(sinceVersion);
                return std::move(*this);
            }

            /// <summary>
            /// Invokes callable(entity, ComponentTypes&...) for every matching entity (maybe<T> as T *).
            /// </summary>
            template <typename Callable>
            void for_each(Callable &&callable) {
                mQuery.for_each_chunk([&callable](chunk_view &chunk) {
                    std::tuple<typename detail::view_arg_traits<ComponentTypes>::span_type...> columns = {
                        detail::fetch_column<ComponentTypes>(chunk)...
                    };
                    const std::span<const entity> entities = chunk.entities();
                    const u32 count = chunk.count();
                    for (u32 row = 0; row < count; ++row) {
                        invoke_row(callable, entities[row], columns, row, std::index_sequence_for<ComponentTypes...>{});
                    }
                });
            }

            /// <summary>
            /// Invokes callable(chunk_view &) for every matching chunk.
            /// </summary>
            template <typename Callable>
            void for_each_chunk(Callable &&callable) {
                mQuery.for_each_chunk(std::forward<Callable>(callable));
            }

            /// <summary>
            /// Runs callable(chunk_view &, command_buffer &) over the matching chunks through the
            /// system's job system, one command buffer per grain (system_context::parallel_for_chunks).
            /// Defined in system.h, which callers using it must include.
            /// </summary>
            template <typename Callable>
            void parallel_for_each_chunk(system_context &context, Callable &&callable);

            /// <summary>
            /// Number of matching entities.
            /// </summary>
            [[nodiscard]] u32 entity_count() {
                return mQuery.entity_count();
            }

            /// <summary>
            /// The read and write sets derived from the pack.
            /// </summary>
            [[nodiscard]] view_access access() const noexcept {
                view_access result = {};
                usize slot = 0;
                (collect_access<ComponentTypes>(result, slot), ...);
                return result;
            }

            /// <summary>
            /// The query behind this view.
            /// </summary>
            [[nodiscard]] query &underlying() noexcept {
                return mQuery;
            }

            /// <summary>
            /// False once the pack or a builder call required a type the registry does not know; such
            /// a view matches nothing.
            /// </summary>
            [[nodiscard]] bool is_valid() const noexcept {
                return mValid;
            }

        private:
            /// <summary>
            /// A 'with' mask no archetype satisfies: the match set of a view over an unregistered type.
            /// </summary>
            static constexpr component_mask MATCH_NOTHING = ~component_mask{};

            template <typename PackType>
            void fill_slot(const component_registry &registry, query_desc &desc, usize &slot) {
                using traits = detail::view_arg_traits<PackType>;
                using component_type = typename traits::component_type;
                const component_id id = registry.id_of<component_type>();
                mIds[slot] = id;
                ++slot;
                if (!id.is_valid()) {
                    // An optional component nobody can have is absent everywhere; a required one is a
                    // programming error and the view matches nothing (the mask is set afterwards).
                    if constexpr (!traits::IS_OPTIONAL) {
                        CONJURE_ASSERT(
                          false,
                          "view requires unregistered component '{}'",
                          component_name<component_type>::VALUE
                        );
                        mValid = false;
                    }
                    return;
                }
                if constexpr (traits::IS_OPTIONAL) {
                    desc.optional.set(id);
                } else {
                    desc.with.set(id);
                }
                if constexpr (!traits::IS_CONST) {
                    desc.writes.set(id);
                }
            }

            template <typename TagType>
            void require() {
                const component_id id = mQuery.registry().id_of<TagType>();
                if (!id.is_valid()) {
                    require_unregistered<TagType>();
                    return;
                }
                query_desc desc = mQuery.desc();
                desc.with.set(id);
                mQuery.reset(std::move(desc));
            }

            template <typename OtherType>
            void exclude() {
                const component_id id = mQuery.registry().id_of<OtherType>();
                if (!id.is_valid()) {
                    require_unregistered<OtherType>();
                    return;
                }
                query_desc desc = mQuery.desc();
                desc.without.set(id);
                mQuery.reset(std::move(desc));
            }

            template <typename OtherType>
            void filter_changed(u64 sinceVersion) {
                const component_id id = mQuery.registry().id_of<OtherType>();
                if (!id.is_valid()) {
                    require_unregistered<OtherType>();
                    return;
                }
                query_desc desc = mQuery.desc();
                desc.changed.set(id);
                desc.sinceVersion = sinceVersion;
                mQuery.reset(std::move(desc));
            }

            template <typename OtherType>
            void require_unregistered() {
                CONJURE_ASSERT(
                  false,
                  "view requires unregistered component '{}'",
                  component_name<std::remove_cvref_t<OtherType>>::VALUE
                );
                mValid = false;
                query_desc desc = mQuery.desc();
                desc.with = MATCH_NOTHING;
                mQuery.reset(std::move(desc));
            }

            template <typename PackType>
            void collect_access(view_access &result, usize &slot) const noexcept {
                using traits = detail::view_arg_traits<PackType>;
                const component_id id = mIds[slot];
                ++slot;
                if (!id.is_valid()) {
                    return;
                }
                if constexpr (traits::IS_CONST) {
                    result.reads.set(id);
                } else {
                    result.writes.set(id);
                }
            }

            template <typename Callable, typename Columns, usize... Indices>
            static void invoke_row(
              Callable &callable,
              entity target,
              Columns &columns,
              u32 row,
              std::index_sequence<Indices...>
            ) {
                callable(target, detail::row_argument<ComponentTypes>(std::get<Indices>(columns), row)...);
            }
    };
}
