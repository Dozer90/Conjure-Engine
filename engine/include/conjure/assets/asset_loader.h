#pragma once

#include "conjure/assets/asset_payload.h"
#include "conjure/assets/asset_ref.h"
#include "conjure/assets/asset_types.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <optional>
#include <span>
#include <utility>

// Loaders turn a cooked blob into a published payload in two stages (docs/design/05_assets.md
// 4.3): load_cpu on a worker thread, finalise on the main thread. typed_asset_loader does the type
// erasure so implementers write two typed functions.

namespace conjure::assets {

    /// <summary>
    /// What load_cpu knows about the asset being loaded.
    /// </summary>
    struct load_context {
        asset_id id = {};
        asset_type_id type = asset_type_id::INVALID;
        std::span<const asset_id> dependencies = {};   // same order as the blob header
    };

    /// <summary>
    /// What finalise knows. Every dependency handle is terminal (LOADED or FAILED) when finalise
    /// runs; a FAILED dependency reads nullptr (use the manager's fallback).
    /// </summary>
    struct finalise_context {
        asset_id id = {};
        std::span<const asset_handle> dependencies = {};
        u64 frameIndex = 0;
        bool isReload = false;
    };

    /// <summary>
    /// Type-erased loader interface. One instance per registered type, owned by the registry,
    /// stateless with respect to individual loads.
    /// </summary>
    class CONJURE_API asset_loader {
        public:
            asset_loader();
            virtual ~asset_loader();

            asset_loader(const asset_loader &) = delete;
            asset_loader &operator=(const asset_loader &) = delete;

        public:
            /// <summary>
            /// The type this loader produces.
            /// </summary>
            [[nodiscard]] virtual asset_type_id type() const noexcept = 0;

            /// <summary>
            /// Worker thread. Decodes the payload bytes into an intermediate that owns its data;
            /// the bytes are valid only for the duration of the call.
            /// </summary>
            [[nodiscard]] virtual core::result<asset_payload> load_cpu(
              const load_context &context,
              std::span<const std::byte> payloadBytes
            ) const = 0;

            /// <summary>
            /// Main thread. Turns the intermediate into the published payload.
            /// </summary>
            [[nodiscard]] virtual core::result<asset_payload> finalise(
              const finalise_context &context,
              asset_payload &&cpuOutput
            ) const = 0;

            /// <summary>
            /// Main thread, once per type, lazily (or at registration with PINNED_FALLBACK). The
            /// default returns a null payload, meaning "no fallback".
            /// </summary>
            [[nodiscard]] virtual core::result<asset_payload> create_fallback() const;
    };

    /// <summary>
    /// Typed convenience base: implements type(), load_cpu(), finalise() and create_fallback() as
    /// final overrides that erase through make_payload(); implementers write the typed functions.
    /// </summary>
    template <declared_asset_type AssetType, typename Intermediate>
    class typed_asset_loader : public asset_loader {
        public:
            typed_asset_loader() = default;
            ~typed_asset_loader() override = default;

        public:
            [[nodiscard]] asset_type_id type() const noexcept final {
                return asset_type_traits<AssetType>::id();
            }

            [[nodiscard]] core::result<asset_payload> load_cpu(
              const load_context &context,
              std::span<const std::byte> payloadBytes
            ) const final {
                core::result<Intermediate> intermediate = load_cpu_typed(context, payloadBytes);
                if (!intermediate.has_value()) {
                    return std::move(intermediate).get_error();
                }
                return make_payload<Intermediate>(std::move(intermediate).value());
            }

            [[nodiscard]] core::result<asset_payload> finalise(
              const finalise_context &context,
              asset_payload &&cpuOutput
            ) const final {
                Intermediate *intermediate = payload_as<Intermediate>(cpuOutput);
                if (intermediate == nullptr) {
                    return make_asset_error(asset_error_code::FINALISE_FAILED, "finalise received no CPU output");
                }
                core::result<AssetType> finished = finalise_typed(context, std::move(*intermediate));
                cpuOutput.reset();
                if (!finished.has_value()) {
                    return std::move(finished).get_error();
                }
                return make_payload<AssetType>(std::move(finished).value());
            }

            [[nodiscard]] core::result<asset_payload> create_fallback() const final {
                std::optional<AssetType> fallback = create_fallback_typed();
                if (!fallback.has_value()) {
                    return make_null_payload();
                }
                return make_payload<AssetType>(std::move(*fallback));
            }

        protected:
            /// <summary>
            /// Worker thread. Decodes the bytes into an owning intermediate.
            /// </summary>
            [[nodiscard]] virtual core::result<Intermediate> load_cpu_typed(
              const load_context &context,
              std::span<const std::byte> payloadBytes
            ) const = 0;

            /// <summary>
            /// Main thread. Produces the published value from the intermediate.
            /// </summary>
            [[nodiscard]] virtual core::result<AssetType> finalise_typed(
              const finalise_context &context,
              Intermediate &&intermediate
            ) const = 0;

            /// <summary>
            /// Main thread, once per type. nullopt (the default) means the type has no fallback.
            /// </summary>
            [[nodiscard]] virtual std::optional<AssetType> create_fallback_typed() const {
                return std::nullopt;
            }
    };
}
