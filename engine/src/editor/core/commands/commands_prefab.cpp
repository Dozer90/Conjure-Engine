#include "editor/core/commands/command_support.h"
#include "editor/core/component_access.h"

#include "conjure/editor/command_dispatcher.h"
#include "conjure/editor/entity_ref.h"
#include "conjure/editor/log_category.h"
#include "conjure/editor/play_controller.h"

#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_meta.h"
#include "conjure/assets/assets_error.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/string_utils.h"
#include "conjure/core/vfs/path_utils.h"
#include "conjure/scene/components.h"
#include "conjure/scene/prefab.h"
#include "conjure/scene/register.h"
#include "conjure/scene/scene_serializer.h"

#include <algorithm>
#include <format>
#include <functional>
#include <memory>
#include <utility>

// prefab.* (docs/design/04_editor.md 4.4, 13_scene.md 3.12): create a prefab asset from a subtree (and mark
// it as the first instance), instantiate, apply an instance back to the asset and unpack an instance.
//
// prefab.create, prefab.instantiate and prefab.apply need the prefab's payload, and are asynchronous (code sweep
// F736): the handler checks its arguments, starts asset_manager::load() and parks its reply in a frame task that
// keeps the asset handle alive; once the asset is loaded the task re-checks what the command depends on and makes
// the change, and the dispatcher records it then. Inside editor.batch, an undo or redo step and a batch rollback
// (command_dispatcher::requires_synchronous_completion) they finish inside the handler with a blocking load that
// is expected there and logs no hitch WARN. A prefab that is already loaded finishes inside the handler too. Only
// a command that waits counts toward the client's cap of pending requests (command_dispatcher, BUSY beyond it).

namespace conjure::editor::support {

    namespace {
        std::string normalise_prefab_path(std::string_view text) {
            std::string path = std::string(core::str::trim(text));
            if (path.find("://") == std::string::npos) {
                while (!path.empty() && (path.front() == '/' || path.front() == '\\')) {
                    path.erase(path.begin());
                }
                path = "assets://" + core::str::replace_all(path, "\\", "/");
            }
            if (!core::str::ends_with(path, ".prefab.json")) {
                if (core::str::ends_with(path, ".json")) {
                    path.erase(path.size() - 5);
                }
                path += ".prefab.json";
            }
            return path;
        }

        core::result<assets::asset_database *> require_database(editor_context &context) {
            assets::asset_database *database = context.asset_database();
            if (database == nullptr || context.assets() == nullptr) {
                return not_available("prefabs need the asset database (no project or a cooked project)");
            }
            return database;
        }

        core::result<assets::asset_id> resolve_prefab(assets::asset_database &database, std::string_view text) {
            const std::optional<core::uuid> parsed = core::uuid::parse(text);
            if (parsed.has_value()) {
                if (!database.contains(*parsed)) {
                    return core::make_error(
                      protocol_error_code::ASSET_NOT_FOUND,
                      std::format("no prefab asset {}", parsed->to_string())
                    );
                }
                return *parsed;
            }
            const std::string path = normalise_prefab_path(text);
            const std::optional<assets::asset_id> found = database.find_by_path(path);
            if (!found.has_value()) {
                return core::make_error(protocol_error_code::ASSET_NOT_FOUND, std::format("no prefab at '{}'", path));
            }
            return *found;
        }

        core::result<std::string> prefab_text(const scene::prefab_document &document) {
            core::json::writer out = {};
            const core::result<void> written = scene::scene_serializer::write_prefab(document, out);
            if (!written.has_value()) {
                return wrap(written.get_error());
            }
            return out.finish();
        }

        /// <summary>
        /// INVALID_ARGS unless the relative part of a prefab asset path stays inside its mount: no "." or ".."
        /// segment, no backslash (the path utilities treat it as a separator) and not absolute. Checked before
        /// anything is written.
        /// </summary>
        core::result<void> check_prefab_path(std::string_view pathText, std::string_view path) {
            const std::optional<assets::asset_path_parts> parts = assets::split_asset_path(path);
            bool contained = parts.has_value() && !parts->relativePath.empty() &&
              parts->relativePath.find('\\') == std::string_view::npos &&
              !core::vfs::path_utils::is_absolute(parts->relativePath);
            if (contained) {
                for (const std::string_view segment : core::str::split(parts->relativePath, '/', false)) {
                    if (segment == ".." || segment == ".") {
                        contained = false;
                        break;
                    }
                }
            }
            if (!contained) {
                return invalid_args(std::format(
                  "args.path: '{}' must stay inside its mount (no '..' or '.' segments, no backslash, no absolute "
                  "path)",
                  pathText
                ));
            }
            return core::result<void>::success();
        }

        /*-------------------------------------------------------------------*/
        // Completion

        /// <summary>
        /// STALE_STATE: what an asynchronous prefab command depended on changed between dispatch and completion
        /// (owner answer Q2 of code sweep F736). 'what' says what changed; nothing was changed by the command.
        /// </summary>
        core::error stale(std::string_view command, std::string_view what) {
            return core::make_error(
              protocol_error_code::STALE_STATE,
              std::format("'{}' did not complete: {} while its prefab loaded; nothing was changed", command, what)
            );
        }

        /// <summary>
        /// The editor state an asynchronous prefab command starts from: the scene it runs on and the play state.
        /// </summary>
        struct dispatch_state {
            u64 sceneGeneration = 0;
            u64 playStateChanges = 0;
        };

        dispatch_state capture_state(editor_context &context) {
            dispatch_state state = {};
            state.sceneGeneration = context.scene_generation();
            state.playStateChanges = context.play().state_changes();
            return state;
        }

        /// <summary>
        /// STALE_STATE when the scene was replaced or closed or the play state changed (also a stop and a start
        /// that came back to the same state) since 'dispatched' was captured.
        /// </summary>
        core::result<void> check_unchanged(
          editor_context &context,
          const dispatch_state &dispatched,
          std::string_view command
        ) {
            if (!context.has_scene() || context.scene_generation() != dispatched.sceneGeneration) {
                return stale(command, "the scene was replaced or closed");
            }
            if (context.play().state_changes() != dispatched.playStateChanges) {
                return stale(
                  command,
                  std::format("the play state changed (it is {} now)", to_string(context.play().state()))
                );
            }
            return core::result<void>::success();
        }

        using prefab_finish = std::function<
          core::result<command_outcome>(editor_context &context, const scene::prefab_asset &asset)
        >;
        using prefab_cleanup = std::function<void(editor_context &context)>;

        /// <summary>
        /// The second half of a prefab command: the asset it waits for, the change it makes once the payload is
        /// there (finish re-checks its own target and leaves the scene as it found it when it fails), and what
        /// undoes the work the dispatch already did on disk (prefab.create's file), run whenever the command
        /// fails or is abandoned without its change.
        /// </summary>
        struct prefab_completion {
            std::string command = "";
            assets::asset_id id = {};
            prefab_finish finish = {};
            prefab_cleanup cleanup = {};
        };

        /// <summary>
        /// The payload of a loaded prefab handle, or ASSET_NOT_FOUND for an asset of another type.
        /// </summary>
        core::result<const scene::prefab_asset *> prefab_payload(const assets::asset_handle &handle) {
            const scene::prefab_asset *asset = handle.get<scene::prefab_asset>();
            if (asset == nullptr) {
                return core::make_error(
                  protocol_error_code::ASSET_NOT_FOUND,
                  std::format("asset {} did not load as a prefab", handle.id().to_string())
                );
            }
            return asset;
        }

        /// <summary>
        /// Waits for a prefab command's asset over frames, holding its handle so asset_manager::collect() cannot
        /// unload it meanwhile, then completes the command. A reply that timed out or was cancelled first makes
        /// the task undo the dispatch's work and change nothing; so does the engine shutting down
        /// (release_engine), which also lets go of the handle while the asset manager still exists.
        /// </summary>
        class prefab_load_task final : public frame_task {
            private:
                prefab_completion mCompletion = {};
                async_reply mReply = {};
                assets::asset_handle mHandle = {};
                dispatch_state mDispatched = {};
                bool mDone = false;

            public:
                prefab_load_task(
                  prefab_completion completion,
                  async_reply reply,
                  assets::asset_handle handle,
                  dispatch_state dispatched
                ) :
                    mCompletion(std::move(completion)),
                    mReply(reply),
                    mHandle(std::move(handle)),
                    mDispatched(dispatched) {}

                ~prefab_load_task() override {
                    if (mReply.is_valid()) {
                        mReply.fail(
                          protocol_error_code::CANCELLED,
                          std::format("'{}' was dropped before its prefab loaded", mCompletion.command)
                        );
                    }
                }

            public:
                bool tick(editor_context &context) override {
                    if (mDone) {
                        return true;
                    }
                    if (!mReply.is_valid()) {
                        // Timed out or cancelled: the client has its answer, so the command must not change anything.
                        abandon(context);
                        return true;
                    }
                    const assets::asset_state state = mHandle.state();
                    if (state == assets::asset_state::UNLOADED && context.assets() != nullptr) {
                        // Nothing holds the queue entry any more (the slot was returned to UNLOADED): ask again.
                        mHandle = context.assets()->load(mCompletion.id);
                        return false;
                    }
                    if (!assets::is_terminal(state)) {
                        return false;
                    }
                    core::result<command_outcome> finished = complete(context, state);
                    if (!finished.has_value()) {
                        const core::error failure = finished.get_error();
                        abandon(context);
                        mReply.fail(classify_error(failure), failure.message());
                        return true;
                    }
                    mReply.succeed_mutation(std::move(finished).value());
                    mHandle.reset();
                    mDone = true;
                    return true;
                }

                void release_engine(editor_context &context) override {
                    if (mDone) {
                        return;
                    }
                    abandon(context);
                    if (mReply.is_valid()) {
                        mReply.fail(
                          protocol_error_code::CANCELLED,
                          std::format(
                            "'{}' was cancelled: the engine shut down before its prefab loaded",
                            mCompletion.command
                          )
                        );
                    }
                }

            private:
                core::result<command_outcome> complete(editor_context &context, assets::asset_state state) {
                    CONJURE_TRY(check_unchanged(context, mDispatched, mCompletion.command));
                    if (state == assets::asset_state::FAILED) {
                        const core::error *failure = context.assets()->last_error(mCompletion.id);
                        if (failure != nullptr) {
                            return wrap(*failure);
                        }
                        return core::make_error(
                          protocol_error_code::ASSET_NOT_FOUND,
                          std::format("prefab {} failed to load", mCompletion.id.to_string())
                        );
                    }
                    CONJURE_TRY_ASSIGN(const scene::prefab_asset *asset, prefab_payload(mHandle));
                    return mCompletion.finish(context, *asset);
                }

                /// <summary>
                /// Undoes the dispatch's work once, drops the handle and marks the task finished.
                /// </summary>
                void abandon(editor_context &context) {
                    if (mCompletion.cleanup) {
                        mCompletion.cleanup(context);
                        mCompletion.cleanup = {};
                    }
                    mHandle.reset();
                    mDone = true;
                }
        };

        /// <summary>
        /// Finishes a prefab command inside its handler with a loaded handle: completes the reply with the
        /// change, or runs the cleanup and returns the error (the dispatcher answers it).
        /// </summary>
        core::result<command_outcome> finish_now(
          command_invocation &invocation,
          prefab_completion &completion,
          const assets::asset_handle &handle
        ) {
            const auto failed = [&invocation, &completion](const core::error &error) -> core::result<command_outcome> {
                if (completion.cleanup) {
                    completion.cleanup(invocation.context);
                }
                return error;
            };
            const core::result<const scene::prefab_asset *> asset = prefab_payload(handle);
            if (!asset.has_value()) {
                return failed(asset.get_error());
            }
            core::result<command_outcome> finished = completion.finish(invocation.context, *asset.value());
            if (!finished.has_value()) {
                return failed(finished.get_error());
            }
            invocation.reply.succeed_mutation(std::move(finished).value());
            return command_outcome{};
        }

        /// <summary>
        /// Runs the second half of a prefab command. Where the dispatcher needs the answer inside the handler (a
        /// batch, an undo or redo step, a rollback) the prefab is loaded with a blocking load that is expected
        /// there (no hitch WARN) and the command finishes at once; elsewhere asset_manager::load() is started, a
        /// prefab that is already LOADED finishes at once and any other one is waited for by a prefab_load_task,
        /// unless the client already has as many parked requests as it may (BUSY, the cleanup run).
        /// </summary>
        core::result<command_outcome> run_completion(command_invocation &invocation, prefab_completion completion) {
            editor_context &context = invocation.context;
            assets::asset_manager &manager = *context.assets();
            if (context.dispatcher().requires_synchronous_completion()) {
                const core::result<assets::asset_handle> loaded = manager.load_blocking(
                  completion.id,
                  assets::blocking_load_warning::EXPECTED
                );
                if (!loaded.has_value()) {
                    if (completion.cleanup) {
                        completion.cleanup(context);
                    }
                    return wrap(loaded.get_error());
                }
                return finish_now(invocation, completion, loaded.value());
            }
            if (manager.is_shut_down()) {
                if (completion.cleanup) {
                    completion.cleanup(context);
                }
                return wrap(
                  core::make_error(assets::assets_error::SHUTTING_DOWN, "the asset manager is shut down")
                );
            }
            assets::asset_handle handle = manager.load(completion.id);
            if (handle.state() == assets::asset_state::LOADED) {
                return finish_now(invocation, completion, handle);
            }
            const command_dispatcher &dispatcher = context.dispatcher();
            if (dispatcher.exceeds_pending_limit(invocation.clientId)) {
                // Only a command that waits counts toward the client's cap of pending requests: this one is refused
                // before it parks, with the dispatch's work undone.
                if (completion.cleanup) {
                    completion.cleanup(context);
                }
                return core::make_error(
                  protocol_error_code::BUSY,
                  std::format(
                    "client {} already has {} pending asynchronous requests; '{}' would wait for its prefab and was "
                    "not run",
                    invocation.clientId,
                    dispatcher.max_pending_per_client(),
                    completion.command
                  )
                );
            }
            context.add_frame_task(std::make_unique<prefab_load_task>(
              std::move(completion),
              invocation.reply,
              std::move(handle),
              capture_state(context)
            ));
            return command_outcome{};
        }

        /// <summary>
        /// What prefab.create's completion needs: the root it marks, the name and the prefab text the dispatch
        /// wrote (the subtree must still serialise to it), the asset path and the new asset's uuid.
        /// </summary>
        struct prefab_create_target {
            ecs::entity root = ecs::NULL_ENTITY;
            std::string name = "";
            std::string text = "";
            std::string path = "";
            assets::asset_id id = {};
        };

        /// <summary>
        /// Marks the subtree as the first instance of the prefab prefab.create wrote, unless the entity was
        /// destroyed or its subtree changed since (STALE_STATE).
        /// </summary>
        core::result<command_outcome> finish_create(
          const prefab_create_target &target,
          editor_context &context,
          const scene::prefab_asset &asset
        ) {
            (void) asset;
            scene::scene &scene = context.scene();
            if (!scene.contains(target.root)) {
                return stale("prefab.create", "the entity was destroyed");
            }
            // The file holds the subtree as it was at dispatch: an instance marked over a changed subtree would
            // link entities the prefab does not have.
            const core::result<scene::prefab_document> now = scene::prefab::build_document(
              scene,
              target.root,
              target.name
            );
            if (!now.has_value()) {
                return wrap(now.get_error());
            }
            CONJURE_TRY_ASSIGN(const std::string nowText, prefab_text(now.value()));
            if (nowText != target.text) {
                return stale("prefab.create", "the entity's subtree changed");
            }
            const core::result<void> marked = scene::prefab::mark_instance(
              scene,
              target.root,
              context.assets()->bind<scene::prefab_asset>(target.id)
            );
            if (!marked.has_value()) {
                return wrap(marked.get_error());
            }
            CONJURE_TRY_ASSIGN(const std::string guid, guid_string(scene, target.root));
            const std::string uuid = target.id.to_string();
            context.events().emit(
              editor_event::ASSET_IMPORTED,
              core::json::object({{"uuid", uuid}, {"path", target.path}})
            );
            std::vector<command_inverse> inverses = {};
            inverses.push_back(command_inverse{"asset.delete", core::json::object({{"asset", uuid}})});
            inverses.push_back(command_inverse{"prefab.unpack", core::json::object({{"entity", guid}})});
            command_outcome outcome = {};
            outcome.label = std::format("Create prefab '{}'", target.path);
            outcome.result = core::json::object({{"uuid", uuid}, {"path", target.path}, {"guid", guid}});
            outcome.inverse = command_inverse{"editor.batch", batch_args(inverses, "Undo prefab")};
            command_request replay = {};
            replay.name = "prefab.create";
            replay.args = core::json::object({{"entity", guid}, {"path", target.path}, {"uuid", uuid}});
            outcome.replay = std::move(replay);
            return outcome;
        }

        /// <summary>
        /// What prefab.instantiate's completion needs: the asset, the count, the options checked at dispatch
        /// (whether a parent was named) and the requested root guids.
        /// </summary>
        struct prefab_instantiate_target {
            assets::asset_id id = {};
            u32 count = 1;
            scene::instantiate_options options = {};
            bool hasParent = false;
            std::vector<core::uuid> wantedGuids = {};
        };

        /// <summary>
        /// Instantiates the loaded prefab, unless the parent was destroyed or a requested guid came into use
        /// since dispatch (STALE_STATE). All or nothing: a failure takes the new roots out again.
        /// </summary>
        core::result<command_outcome> finish_instantiate(
          const prefab_instantiate_target &target,
          editor_context &context,
          const scene::prefab_asset &asset
        ) {
            scene::scene &scene = context.scene();
            if (target.hasParent && !scene.contains(target.options.parent)) {
                return stale("prefab.instantiate", "the parent entity was destroyed");
            }
            for (const core::uuid &guid : target.wantedGuids) {
                if (!scene.find_guid(guid).is_null()) {
                    return stale("prefab.instantiate", std::format("guid {} came into use", guid.to_string()));
                }
            }
            std::vector<ecs::entity> roots(target.count, ecs::NULL_ENTITY);
            const core::result<void> instantiated = scene::prefab::instantiate_many(
              scene,
              asset,
              target.count,
              target.options,
              roots
            );
            if (!instantiated.has_value()) {
                return wrap(instantiated.get_error());
            }
            // Anything that fails after the instantiation takes the new roots out again (D4: all or nothing).
            const auto failed = [&roots, &scene](core::error error) -> core::result<command_outcome> {
                for (const ecs::entity root : roots) {
                    if (!root.is_null() && scene.contains(root)) {
                        (void) scene.destroy_now(root);
                    }
                }
                return error;
            };
            std::vector<core::uuid> guids = {};
            std::vector<command_inverse> restores = {};
            for (usize index = 0; index < roots.size(); ++index) {
                const ecs::entity root = roots[index];
                if (index < target.wantedGuids.size()) {
                    const core::result<void> renamed = scene.set_guid(root, target.wantedGuids[index]);
                    if (!renamed.has_value()) {
                        return failed(wrap(renamed.get_error()));
                    }
                }
                const core::result<core::uuid> guid = entity_ref::guid_of(scene, root);
                if (!guid.has_value()) {
                    return failed(guid.get_error());
                }
                guids.push_back(guid.value());
                core::result<command_inverse> restore = restore_inverse(scene, root);
                if (!restore.has_value()) {
                    return failed(restore.get_error());
                }
                restores.push_back(std::move(restore).value());
            }
            command_outcome outcome = {};
            outcome.label = std::format("Instantiate '{}'", asset.name);
            outcome.result = core::json::object({{"guids", guid_array(guids)}, {"prefab", target.id.to_string()}});
            outcome.inverse = command_inverse{"entity.destroy_many", core::json::object(
              {{"entities", guid_array(guids)}}
            )};
            command_request replay = {};
            replay.name = "editor.batch";
            replay.args = batch_args(restores, "Instantiate prefab");
            outcome.replay = std::move(replay);
            return outcome;
        }

        /// <summary>
        /// Writes the instance holding 'entity' back into prefab 'id' as the instance is now, unless the entity
        /// was destroyed or no longer belongs to an instance of that prefab (STALE_STATE).
        /// </summary>
        core::result<command_outcome> finish_apply(
          ecs::entity entity,
          const assets::asset_id &id,
          editor_context &context,
          const scene::prefab_asset &asset
        ) {
            (void) asset;
            scene::scene &scene = context.scene();
            if (!scene.contains(entity)) {
                return stale("prefab.apply", "the entity was destroyed");
            }
            const ecs::entity root = scene::prefab::instance_root_of(scene, entity);
            const scene::prefab_instance *instance = nullptr;
            if (!root.is_null()) {
                instance = scene.world().get<scene::prefab_instance>(root);
            }
            if (instance == nullptr || instance->prefab.id() != id) {
                return stale("prefab.apply", "the entity stopped being part of an instance of the prefab");
            }
            CONJURE_TRY_ASSIGN(assets::asset_database *database, require_database(context));
            const core::result<scene::prefab_document> document = scene::prefab::apply(scene, root);
            if (!document.has_value()) {
                return wrap(document.get_error());
            }
            const std::optional<std::string> filePath = database->file_path_of(id);
            const std::optional<std::string> assetPath = database->asset_path_of(id);
            if (!filePath.has_value() || !assetPath.has_value()) {
                return core::make_error(protocol_error_code::ASSET_NOT_FOUND, "the prefab asset has no source file");
            }
            core::result<std::vector<std::byte>> previous = database->vfs().read_file(*filePath);
            if (!previous.has_value()) {
                return read_failure(*filePath, previous.get_error());
            }
            CONJURE_TRY_ASSIGN(const std::string text, prefab_text(document.value()));
            const std::span<const std::byte> bytes(reinterpret_cast<const std::byte *>(text.data()), text.size());
            const core::result<void> written = database->vfs().write_file_atomic(*filePath, bytes);
            if (!written.has_value()) {
                return io_error(std::format("cannot write '{}': {}", *filePath, written.get_error().message()));
            }
            CONJURE_TRY(import_now(context, *database, id));
            context.assets()->reload(id);
            CONJURE_TRY_ASSIGN(const std::string guid, guid_string(scene, root));
            // The previous file content, base64 encoded through asset.restore's contract.
            const std::string encoded = base64_encode(previous.value());
            command_outcome outcome = {};
            outcome.label = std::format("Apply prefab '{}'", scene.name_of(root).to_string());
            outcome.result = core::json::object({{"uuid", id.to_string()}, {"path", *assetPath}, {"guid", guid}});
            outcome.inverse = command_inverse{"asset.restore", core::json::object({
              {"path", *assetPath},
              {"contentBase64", encoded},
            })};
            if (!context.play().is_editing()) {
                // The redo runs after play.stop restored the scene (D21): the instance may be gone or hold other
                // values by then, so it writes the bytes applied here back instead of applying again.
                command_request replay = {};
                replay.name = "asset.restore";
                replay.args = core::json::object({
                  {"path", *assetPath},
                  {"contentBase64", base64_encode(bytes)},
                });
                outcome.replay = std::move(replay);
            }
            return outcome;
        }

        /*-------------------------------------------------------------------*/
        // Commands

        core::result<command_outcome> prefab_create(command_invocation &invocation) {
            editor_context &context = invocation.context;
            scene::scene &scene = context.scene();
            CONJURE_TRY_ASSIGN(assets::asset_database *database, require_database(context));
            CONJURE_TRY_ASSIGN(const ecs::entity root, resolve_entity(invocation, invocation.args["entity"]));
            CONJURE_TRY_ASSIGN(const std::string_view pathText, require_string(invocation.args, "path"));
            const std::string path = normalise_prefab_path(pathText);
            CONJURE_TRY(check_prefab_path(pathText, path));
            if (database->find_by_path(path).has_value()) {
                return invalid_args(std::format("'{}' already exists", path));
            }
            std::optional<core::uuid> wanted = std::nullopt;
            if (invocation.args.contains("uuid")) {
                wanted = core::uuid::parse(string_or(invocation.args, "uuid", ""));
                if (!wanted.has_value() || wanted->is_null()) {
                    return invalid_args("args.uuid: expected a uuid");
                }
            }
            const std::string name = scene.name_of(root).to_string();
            const core::result<scene::prefab_document> document = scene::prefab::build_document(scene, root, name);
            if (!document.has_value()) {
                return wrap(document.get_error());
            }
            CONJURE_TRY_ASSIGN(const std::string text, prefab_text(document.value()));
            const std::span<const std::byte> bytes(reinterpret_cast<const std::byte *>(text.data()), text.size());

            // The file and its .meta are written the same way whether the uuid is chosen (a redo) or fresh, so an
            // undo / redo round trip leaves identical bytes on disk.
            CONJURE_TRY_ASSIGN(const std::string filePath, file_path_for(*database, path));
            const core::result<void> written = database->vfs().write_file(filePath, bytes);
            if (!written.has_value()) {
                return io_error(std::format("cannot write '{}': {}", filePath, written.get_error().message()));
            }
            assets::asset_meta meta = {};
            meta.id = wanted.has_value() ? *wanted : assets::asset_id::generate();
            meta.importerId = std::string(scene::PREFAB_IMPORTER_ID);
            const core::result<void> metaWritten = assets::write_asset_meta(
              database->vfs(),
              assets::meta_path_of(filePath),
              meta
            );
            // From here on every failure, and a completion that finds its world changed, removes what was written
            // (D4: a failed command leaves nothing behind). The database is looked up again when the cleanup runs:
            // the frame task runs it on the same engine (release_engine before an engine shuts down).
            const prefab_cleanup cleanup = [path, filePath](editor_context &owner) {
                assets::asset_database *current = owner.asset_database();
                if (current == nullptr) {
                    return;
                }
                const std::optional<assets::asset_id> registered = current->find_by_path(path);
                if (registered.has_value()) {
                    (void) current->delete_asset(*registered);
                    return;
                }
                (void) current->vfs().remove(filePath);
                (void) current->vfs().remove(assets::meta_path_of(filePath));
            };
            const auto failed = [&cleanup, &context](const core::error &error) -> core::result<command_outcome> {
                cleanup(context);
                return error;
            };
            if (!metaWritten.has_value()) {
                return failed(io_error(std::format("cannot write the .meta of '{}'", filePath)));
            }
            const core::result<assets::asset_id> added = database->add_file(path);
            if (!added.has_value()) {
                return failed(wrap(added.get_error()));
            }
            const assets::asset_id id = added.value();
            const core::result<void> imported = import_now(context, *database, id);
            if (!imported.has_value()) {
                return failed(imported.get_error());
            }

            prefab_completion completion = {};
            completion.command = "prefab.create";
            completion.id = id;
            completion.cleanup = cleanup;
            prefab_create_target target = {};
            target.root = root;
            target.name = name;
            target.text = text;
            target.path = path;
            target.id = id;
            completion.finish = [target](editor_context &owner, const scene::prefab_asset &asset) {
                return finish_create(target, owner, asset);
            };
            return run_completion(invocation, std::move(completion));
        }

        core::result<command_outcome> prefab_instantiate(command_invocation &invocation) {
            editor_context &context = invocation.context;
            scene::scene &scene = context.scene();
            CONJURE_TRY_ASSIGN(assets::asset_database *database, require_database(context));
            CONJURE_TRY_ASSIGN(const std::string_view reference, require_string(invocation.args, "prefab"));
            CONJURE_TRY_ASSIGN(const assets::asset_id id, resolve_prefab(*database, reference));
            CONJURE_TRY_ASSIGN(const u32 count, u32_or(invocation.args, "count", 1));
            if (count == 0 || count > 100000) {
                return invalid_args("args.count: expected 1..100000");
            }
            scene::instantiate_options options = {};
            const bool hasParent = invocation.args.contains("parent") && !invocation.args["parent"].is_null();
            if (hasParent) {
                CONJURE_TRY_ASSIGN(options.parent, resolve_entity(invocation, invocation.args["parent"]));
            }
            if (invocation.args.contains("name")) {
                const std::string_view rootName = string_or(invocation.args, "name", "");
                CONJURE_TRY(check_entity_name(rootName, "args.name"));
                options.rootName = core::string_id(rootName);
            }
            if (invocation.args.contains("transform")) {
                CONJURE_TRY_ASSIGN(
                  const math::transform transform,
                  read_transform(invocation.args["transform"], "args.transform", math::transform())
                );
                options.rootTransform = scene::local_transform::from_transform(transform);
            }
            // Optional root guids (one per instance): a command file replays deterministically with them.
            std::vector<core::uuid> wantedGuids = {};
            const core::json::value wanted = invocation.args["guids"];
            if (wanted.is_valid()) {
                if (!wanted.is_array() || wanted.size() != count) {
                    return invalid_args("args.guids: expected one guid per instance");
                }
                for (const core::json::value entry : wanted) {
                    const std::optional<core::uuid> parsed = core::uuid::parse(entry.as_string(""));
                    if (!parsed.has_value() || parsed->is_null()) {
                        return invalid_args("args.guids: expected uuid strings");
                    }
                    if (!scene.find_guid(*parsed).is_null()) {
                        return invalid_args(std::format("args.guids: {} is already in use", parsed->to_string()));
                    }
                    if (std::find(wantedGuids.begin(), wantedGuids.end(), *parsed) != wantedGuids.end()) {
                        return invalid_args(std::format("args.guids: {} is listed twice", parsed->to_string()));
                    }
                    wantedGuids.push_back(*parsed);
                }
            }

            prefab_completion completion = {};
            completion.command = "prefab.instantiate";
            completion.id = id;
            prefab_instantiate_target target = {};
            target.id = id;
            target.count = count;
            target.options = options;
            target.hasParent = hasParent;
            target.wantedGuids = std::move(wantedGuids);
            completion.finish = [target](editor_context &owner, const scene::prefab_asset &asset) {
                return finish_instantiate(target, owner, asset);
            };
            return run_completion(invocation, std::move(completion));
        }

        core::result<command_outcome> prefab_apply(command_invocation &invocation) {
            editor_context &context = invocation.context;
            scene::scene &scene = context.scene();
            CONJURE_TRY(require_database(context));
            CONJURE_TRY_ASSIGN(const ecs::entity entity, resolve_entity(invocation, invocation.args["entity"]));
            const ecs::entity root = scene::prefab::instance_root_of(scene, entity);
            if (root.is_null()) {
                return invalid_args("the entity is not part of a prefab instance");
            }
            const scene::prefab_instance *instance = scene.world().get<scene::prefab_instance>(root);
            if (instance == nullptr) {
                return invalid_args("the entity is not part of a prefab instance");
            }
            const assets::asset_id id = instance->prefab.id();

            prefab_completion completion = {};
            completion.command = "prefab.apply";
            completion.id = id;
            // The instance is applied as it is at completion (owner answer Q2): edits made while the prefab
            // loaded are part of what is written.
            completion.finish = [entity, id](editor_context &owner, const scene::prefab_asset &asset) {
                return finish_apply(entity, id, owner, asset);
            };
            return run_completion(invocation, std::move(completion));
        }

        core::result<command_outcome> prefab_unpack(command_invocation &invocation) {
            editor_context &context = invocation.context;
            scene::scene &scene = context.scene();
            CONJURE_TRY_ASSIGN(const ecs::entity entity, resolve_entity(invocation, invocation.args["entity"]));
            const ecs::entity root = scene::prefab::instance_root_of(scene, entity);
            if (root.is_null()) {
                return invalid_args("the entity is not part of a prefab instance");
            }
            const bool recursive = bool_or(invocation.args, "recursive", false);
            // Capture the markers so undo can put them back through component.add.
            std::vector<command_inverse> restores = {};
            const ecs::component_id instanceId = scene.ids().prefabInstance;
            const ecs::component_id linkId = scene.ids().prefabLink;
            std::vector<ecs::entity> members = {};
            scene.tree().collect_subtree(root, members);
            for (const ecs::entity member : members) {
                if (!recursive && member != root && scene.world().has_raw(member, instanceId)) {
                    // A nested instance keeps its markers (and so does its subtree).
                    continue;
                }
                const std::string guid = guid_or_empty(scene, member);
                if (scene.world().has_raw(member, linkId)) {
                    CONJURE_TRY_ASSIGN(const core::json::document link, access::read_component(scene, member, linkId));
                    restores.push_back(command_inverse{
                      "component.add",
                      core::json::object({{"entity", guid}, {"type", "conjure.scene.prefab_link"}, {"value", link}}),
                    });
                }
                if (scene.world().has_raw(member, instanceId)) {
                    CONJURE_TRY_ASSIGN(
                      const core::json::document instance,
                      access::read_component(scene, member, instanceId)
                    );
                    restores.push_back(command_inverse{
                      "component.add",
                      core::json::object({
                        {"entity", guid},
                        {"type", "conjure.scene.prefab_instance"},
                        {"value", instance},
                      }),
                    });
                }
            }
            const core::result<void> unpacked = scene::prefab::unpack(scene, root, recursive);
            if (!unpacked.has_value()) {
                return wrap(unpacked.get_error());
            }
            CONJURE_TRY_ASSIGN(const std::string guid, guid_string(scene, root));
            command_outcome outcome = {};
            outcome.label = std::format("Unpack '{}'", scene.name_of(root).to_string());
            outcome.result = core::json::object({{"guid", guid}, {"entities", members.size()}});
            outcome.inverse = command_inverse{"editor.batch", batch_args(restores, "Repack prefab")};
            return outcome;
        }
    }

    void register_prefab_commands(command_registry &registry) {
        const std::string entityRef = std::string(ENTITY_REF_SCHEMA);
        add_command(
          registry,
          make_command(
            "prefab.create",
            "Saves an entity's subtree as a .prefab.json asset and marks the subtree as its first instance. Editing "
            "state only: play.stop would discard the marked instance and keep the file. Asynchronous: it answers "
            "once the new prefab has loaded (inside editor.batch at once), with STALE_STATE and the file removed "
            "when the scene, the play state, the entity or its subtree changed meanwhile.",
            std::format(
              R"({{"type":"object","properties":{{"entity":{},"path":{{"type":"string"}},"uuid":{{"type":"string"}}}},)"
              R"("required":["entity","path"]}})",
              entityRef
            ),
            R"({"type":"object","properties":{"uuid":{"type":"string"},"path":{"type":"string"},)"
            R"("guid":{"type":"string"},"revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_PROJECT | command_flags::REQUIRES_SCENE |
              command_flags::EDITING_ONLY | command_flags::ASYNC | command_flags::SYNC_IN_BATCH,
            &prefab_create
          )
        );
        add_command(
          registry,
          make_command(
            "prefab.instantiate",
            "Instantiates a prefab (uuid or path) count times under a parent, with an optional root name, transform "
            "and root guids. Asynchronous: it answers once the prefab is loaded (at once when it is resident or "
            "inside editor.batch), with STALE_STATE when the scene, the play state, the parent or a requested guid "
            "changed meanwhile.",
            std::format(
              R"({{"type":"object","properties":{{"prefab":{{"type":"string"}},"parent":{{"type":["string","null"]}},)"
              R"("name":{{"type":"string"}},)"
              R"("transform":{{"type":"object","additionalProperties":true}},"count":{{"type":"integer","minimum":1}},)"
              R"("guids":{{"type":"array","items":{{"type":"string"}}}}}},"required":["prefab"]}})",
              entityRef
            ),
            R"({"type":"object","properties":{"guids":{"type":"array"},"prefab":{"type":"string"},)"
            R"("revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_SCENE | command_flags::ASYNC |
              command_flags::SYNC_IN_BATCH,
            &prefab_instantiate
          )
        );
        add_command(
          registry,
          make_command(
            "prefab.apply",
            "Writes an instance's current state back into its prefab asset. While playing the file change is kept: "
            "play.stop records it for undo with the session's other file changes. Asynchronous: it answers once the "
            "prefab is loaded (at once when it is resident or inside editor.batch) and applies the instance as it is "
            "then, with STALE_STATE when the scene or the play state changed or the entity left the instance.",
            std::format(R"({{"type":"object","properties":{{"entity":{}}},"required":["entity"]}})", entityRef),
            R"({"type":"object","properties":{"uuid":{"type":"string"},"path":{"type":"string"},)"
            R"("guid":{"type":"string"},"revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_PROJECT | command_flags::REQUIRES_SCENE |
              command_flags::ON_DISK | command_flags::ASYNC | command_flags::SYNC_IN_BATCH,
            &prefab_apply
          )
        );
        add_command(
          registry,
          make_command(
            "prefab.unpack",
            "Removes the prefab markers of an instance so it becomes plain entities.",
            std::format(R"({{"type":"object","properties":{{"entity":{},"recursive":{{"type":"boolean"}}}},)"
            R"("required":["entity"]}})", entityRef),
            R"({"type":"object","properties":{"guid":{"type":"string"},"entities":{"type":"integer"},)"
            R"("revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_SCENE,
            &prefab_unpack
          )
        );
    }
}
