#pragma once

#include "engine/module_consumers/module_content_file_system.h"

#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/assets/asset_types.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/modules/module_consumer.h"
#include "conjure/modules/module_registration_set.h"
#include "conjure/modules/module_registry.h"
#include "conjure/modules/module_types.h"

#include <memory>
#include <string>
#include <vector>

// The assets module consumer (docs/design/07_modules.md 9 "Needs from assets", R24, R37): instantiates the
// importer and asset-type factories a module registered (assets::importer_extension_desc /
// asset_type_extension_desc), mounts the manifest's content directories as modules://<name>/ and undoes
// both on detach. Asset residency is the manager's: a type whose slots are still loaded cannot be
// unregistered (INVALID_STATE from the registry), which is logged as an ERROR on detach.

namespace conjure::engine {

    class assets_module_consumer final : public modules::module_consumer {
        private:
            struct module_record {
                modules::module_id id = {};
                std::string name = {};
                std::vector<std::string> importerIds = {};
                std::vector<assets::asset_type_id> types = {};
                bool contentMounted = false;
            };

            assets::asset_type_registry *mTypes = nullptr;
            assets::importer_registry *mImporters = nullptr;
            std::shared_ptr<module_content_file_system> mContent = nullptr;
            core::string_id mName = {};
            std::vector<module_record> mRecords = {};

        public:
            assets_module_consumer(
              assets::asset_type_registry &types,
              assets::importer_registry &importers,
              std::shared_ptr<module_content_file_system> content
            );
            ~assets_module_consumer() override;

        public:
            /// <summary>
            /// "assets".
            /// </summary>
            [[nodiscard]] core::string_id name() const noexcept override;

            /// <summary>
            /// REGISTRATION_CONFLICT for an importer id or type name already registered, a record without a
            /// factory, or an importer id / type name that is empty.
            /// </summary>
            [[nodiscard]] core::result<void> validate(
              modules::module_id id,
              const modules::module_registration_set &set,
              const modules::module_registry &registry
            ) override;

            /// <summary>
            /// Registers the importers and types and mounts the first content directory as modules://<name>/.
            /// </summary>
            void attach(
              modules::module_id id,
              modules::module_context &context,
              const modules::module_registration_set &set
            ) override;

            [[nodiscard]] core::result<void> can_detach(modules::module_id id, modules::unload_policy policy) override;

            /// <summary>
            /// Unregisters the importers and types and unmounts the content.
            /// </summary>
            void detach(modules::module_id id, modules::reload_context *reload) override;

            void restore(modules::module_id id, const modules::reload_context &reload) override;

            [[nodiscard]] u32 live_use_count(modules::module_id id) const override;

            /// <summary>
            /// The backend behind modules://.
            /// </summary>
            [[nodiscard]] module_content_file_system &content() noexcept {
                return *mContent;
            }

        private:
            [[nodiscard]] module_record *find_record(modules::module_id id) noexcept;
    };
}
