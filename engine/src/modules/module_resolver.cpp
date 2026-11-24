#include "conjure/modules/module_resolver.h"

#include "conjure/modules/module_error.h"
#include "conjure/modules/module_manifest.h"
#include "conjure/modules/module_registry.h"

#include "conjure/core/assert.h"
#include "conjure/core/containers/flat_map.h"
#include "conjure/core/string_id.h"

#include <algorithm>
#include <format>
#include <string>
#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

namespace {

    /*=======================================================================*/
    /*=======================================================================*/
    // Graph node

    struct node {
        module_id id = {};
        const module_manifest *manifest = nullptr;
        std::vector<usize> dependsOn = {};      // indices of candidate nodes this one needs
        std::vector<usize> dependents = {};     // indices of candidate nodes needing this one
        usize inDegree = 0;
        bool failed = false;
        bool ordered = false;
    };

    class resolver {
        private:
            const resolution_input &mInput;          // no default: a reference bound by the constructor
            const module_registry &mRegistry;        // no default: a reference bound by the constructor
            std::vector<node> mNodes = {};
            flat_map<std::string_view, usize> mCandidateByName = {};
            resolution_result mResult = {};

        public:
            explicit resolver(const resolution_input &input) :
                mInput(input),
                mRegistry(*input.registry) {}

        public:
            resolution_result run() {
                build_nodes();
                check_host_compatibility();
                check_game_modules();
                build_edges();
                order_nodes();
                report_cycles();
                sort_failures();
                return std::move(mResult);
            }

        private:
            [[nodiscard]] const std::string &name_of(usize index) const noexcept {
                return mNodes[index].manifest->name;
            }

            void fail(usize index, error failure) {
                node &entry = mNodes[index];
                if (entry.failed) {
                    return;
                }
                entry.failed = true;
                const std::string rootMessage = failure.message();
                mResult.failures.push_back(resolution_failure{entry.id, std::move(failure)});

                // Dependents that still have a path to this module inherit the failure; an optional
                // dependency never drags its dependent down.
                for (const usize dependentIndex : entry.dependents) {
                    if (depends_optionally(dependentIndex, index)) {
                        continue;
                    }
                    fail(
                      dependentIndex,
                      core::make_error(
                        module_error::DEPENDENCY_FAILED,
                        "module '{}' depends on '{}', which cannot load: {}",
                        name_of(dependentIndex),
                        name_of(index),
                        rootMessage
                      )
                    );
                }
            }

            [[nodiscard]] bool depends_optionally(usize dependentIndex, usize dependencyIndex) const noexcept {
                for (const module_dependency &dependency : mNodes[dependentIndex].manifest->dependencies) {
                    if (dependency.name == name_of(dependencyIndex)) {
                        return dependency.optional;
                    }
                }
                return false;
            }

            void build_nodes() {
                mNodes.reserve(mInput.candidates.size());
                for (const module_id id : mInput.candidates) {
                    if (!mRegistry.is_valid(id)) {
                        continue;
                    }
                    node entry = {};
                    entry.id = id;
                    entry.manifest = &mRegistry.manifest(id);
                    mNodes.push_back(std::move(entry));
                }
                for (usize index = 0; index < mNodes.size(); ++index) {
                    mCandidateByName.insert_or_assign(std::string_view(name_of(index)), index);
                }
            }

            void check_host_compatibility() {
                for (usize index = 0; index < mNodes.size(); ++index) {
                    const module_manifest &manifest = *mNodes[index].manifest;
                    if (!manifest.engineVersion.contains(mInput.engineVersion)) {
                        fail(
                          index,
                          core::make_error(
                            module_error::ENGINE_VERSION_MISMATCH,
                            "module '{}' requires engine {} but the host is {}",
                            manifest.name,
                            manifest.engineVersion,
                            mInput.engineVersion
                          )
                        );
                        continue;
                    }
                    const module_feature_flags missing = manifest.requiredFeatures & ~mInput.engineFeatures;
                    if (missing != module_feature_flags::NONE) {
                        fail(
                          index,
                          core::make_error(
                            module_error::FEATURE_UNAVAILABLE,
                            "module '{}' requires the engine feature(s) {} which this build lacks",
                            manifest.name,
                            to_string(missing)
                          )
                        );
                    }
                }
            }

            /// <summary>
            /// One GAME module per process: a loaded one or the first candidate by name wins.
            /// </summary>
            void check_game_modules() {
                std::string existingGame = {};
                for (const module_id id : mInput.loaded) {
                    if (mRegistry.is_valid(id) && mRegistry.manifest(id).kind == module_kind::GAME) {
                        existingGame = mRegistry.manifest(id).name;
                        break;
                    }
                }

                std::vector<usize> gameNodes = {};
                for (usize index = 0; index < mNodes.size(); ++index) {
                    if (mNodes[index].manifest->kind == module_kind::GAME && !mNodes[index].failed) {
                        gameNodes.push_back(index);
                    }
                }
                std::sort(gameNodes.begin(), gameNodes.end(), [this](usize left, usize right) {
                    return name_of(left) < name_of(right);
                });

                for (const usize index : gameNodes) {
                    if (existingGame.empty()) {
                        existingGame = name_of(index);
                        continue;
                    }
                    fail(
                      index,
                      core::make_error(
                        module_error::MULTIPLE_GAME_MODULES,
                        "module '{}' is a game module but '{}' is already the game module",
                        name_of(index),
                        existingGame
                      )
                    );
                }
            }

            [[nodiscard]] bool is_loaded(module_id id) const noexcept {
                return std::find(mInput.loaded.begin(), mInput.loaded.end(), id) != mInput.loaded.end();
            }

            void build_edges() {
                for (usize index = 0; index < mNodes.size(); ++index) {
                    for (const module_dependency &dependency : mNodes[index].manifest->dependencies) {
                        resolve_dependency(index, dependency);
                    }
                }
            }

            void resolve_dependency(usize index, const module_dependency &dependency) {
                const std::string &name = name_of(index);

                // A loaded module satisfies the dependency without an edge.
                const module_id loadedId = mRegistry.find(string_id(dependency.name));
                if (loadedId.is_valid() && is_loaded(loadedId)) {
                    const semantic_version found = mRegistry.manifest(loadedId).version;
                    if (!dependency.version.contains(found)) {
                        fail(index, version_error(name, dependency, found));
                    }
                    return;
                }

                const auto candidate = mCandidateByName.find(std::string_view(dependency.name));
                if (candidate == mCandidateByName.end()) {
                    if (dependency.optional) {
                        return;
                    }
                    fail(index, missing_error(name, dependency.name, loadedId));
                    return;
                }

                const usize dependencyIndex = candidate->second;
                const semantic_version found = mNodes[dependencyIndex].manifest->version;
                if (!dependency.version.contains(found)) {
                    fail(index, version_error(name, dependency, found));
                    return;
                }

                mNodes[index].dependsOn.push_back(dependencyIndex);
                mNodes[dependencyIndex].dependents.push_back(index);
                if (mNodes[dependencyIndex].failed && !dependency.optional) {
                    fail(
                      index,
                      core::make_error(
                        module_error::DEPENDENCY_FAILED,
                        "module '{}' depends on '{}', which cannot load: {}",
                        name,
                        dependency.name,
                        failure_message(mNodes[dependencyIndex].id)
                      )
                    );
                }
            }

            /// <summary>
            /// DEPENDENCY_MISSING naming what is actually wrong: a module that is not registered
            /// at all, one SKIPPED on this platform, one FAULTED (with its error), or one that is
            /// registered but outside this resolution.
            /// </summary>
            [[nodiscard]] error missing_error(
              const std::string &name,
              const std::string &dependencyName,
              module_id registeredId
            ) const {
                if (!registeredId.is_valid()) {
                    return core::make_error(
                      module_error::DEPENDENCY_MISSING,
                      "module '{}' depends on '{}', which is not registered on this host",
                      name,
                      dependencyName
                    );
                }
                const module_state state = mRegistry.state(registeredId);
                if (state == module_state::SKIPPED) {
                    return core::make_error(
                      module_error::DEPENDENCY_MISSING,
                      "module '{}' depends on '{}', which is SKIPPED on this platform",
                      name,
                      dependencyName
                    );
                }
                if (state == module_state::FAULTED) {
                    return core::make_error(
                      module_error::DEPENDENCY_MISSING,
                      "module '{}' depends on '{}', which is FAULTED: {}",
                      name,
                      dependencyName,
                      mRegistry.last_error(registeredId).message()
                    );
                }
                return core::make_error(
                  module_error::DEPENDENCY_MISSING,
                  "module '{}' depends on '{}', which is {} and not part of this load",
                  name,
                  dependencyName,
                  state
                );
            }

            [[nodiscard]] static error version_error(
              const std::string &name,
              const module_dependency &dependency,
              semantic_version found
            ) {
                return core::make_error(
                  module_error::DEPENDENCY_VERSION,
                  "module '{}' depends on '{}' {} but version {} is registered",
                  name,
                  dependency.name,
                  dependency.version,
                  found
                );
            }

            [[nodiscard]] std::string failure_message(module_id id) const {
                for (const resolution_failure &failure : mResult.failures) {
                    if (failure.id == id) {
                        return failure.error.message();
                    }
                }
                return {};
            }

            /// <summary>
            /// Kahn's algorithm. Ties are broken by name, and a GAME module is only picked when no
            /// other module is ready, so it ends up last unless something depends on it.
            /// </summary>
            void order_nodes() {
                for (node &entry : mNodes) {
                    entry.inDegree = 0;
                    for (const usize dependencyIndex : entry.dependsOn) {
                        // Edges to failed or optional-and-failed nodes still count until the failed node
                        // is "released" below; failed nodes are released immediately.
                        if (!mNodes[dependencyIndex].failed) {
                            ++entry.inDegree;
                        }
                    }
                }

                std::vector<usize> ready = {};
                for (usize index = 0; index < mNodes.size(); ++index) {
                    if (!mNodes[index].failed && mNodes[index].inDegree == 0) {
                        ready.push_back(index);
                    }
                }

                while (!ready.empty()) {
                    const auto best = std::min_element(ready.begin(), ready.end(), [this](usize left, usize right) {
                        const bool leftGame = mNodes[left].manifest->kind == module_kind::GAME;
                        const bool rightGame = mNodes[right].manifest->kind == module_kind::GAME;
                        if (leftGame != rightGame) {
                            return !leftGame;
                        }
                        return name_of(left) < name_of(right);
                    });
                    const usize index = *best;
                    ready.erase(best);

                    mNodes[index].ordered = true;
                    mResult.order.push_back(mNodes[index].id);
                    for (const usize dependentIndex : mNodes[index].dependents) {
                        node &dependent = mNodes[dependentIndex];
                        if (dependent.failed) {
                            continue;
                        }
                        CONJURE_ASSERT(dependent.inDegree > 0, "resolver: in-degree underflow");
                        --dependent.inDegree;
                        if (dependent.inDegree == 0) {
                            ready.push_back(dependentIndex);
                        }
                    }
                }
            }

            /// <summary>
            /// Every node that is neither ordered nor failed sits on or behind a cycle. The cycle
            /// itself is found by walking dependencies until a node repeats.
            /// </summary>
            void report_cycles() {
                for (usize index = 0; index < mNodes.size(); ++index) {
                    if (mNodes[index].ordered || mNodes[index].failed) {
                        continue;
                    }
                    const std::vector<usize> cycle = find_cycle(index);
                    if (cycle.empty()) {
                        // Blocked by a cycle it is not part of: the member's failure propagates to it
                        // through fail(); make sure the member is reported first.
                        continue;
                    }
                    std::string text = {};
                    for (const usize member : cycle) {
                        if (!text.empty()) {
                            text.append(" -> ");
                        }
                        text.append(name_of(member));
                    }
                    text.append(" -> ");
                    text.append(name_of(cycle.front()));
                    fail(
                      cycle.front(),
                      core::make_error(module_error::DEPENDENCY_CYCLE, "dependency cycle: {}", text)
                    );
                }

                // Anything still unresolved depends on a cycle through an optional edge chain or
                // was released late; report it as DEPENDENCY_FAILED on its first unresolved dependency.
                for (usize index = 0; index < mNodes.size(); ++index) {
                    if (mNodes[index].ordered || mNodes[index].failed) {
                        continue;
                    }
                    for (const usize dependencyIndex : mNodes[index].dependsOn) {
                        if (!mNodes[dependencyIndex].ordered) {
                            fail(
                              index,
                              core::make_error(
                                module_error::DEPENDENCY_FAILED,
                                "module '{}' depends on '{}', which cannot load: {}",
                                name_of(index),
                                name_of(dependencyIndex),
                                failure_message(mNodes[dependencyIndex].id)
                              )
                            );
                            break;
                        }
                    }
                }
            }

            /// <summary>
            /// Follows unresolved dependencies from 'start'; returns the cycle 'start' is on, or
            /// empty when 'start' only leads into a cycle it is not part of.
            /// </summary>
            [[nodiscard]] std::vector<usize> find_cycle(usize start) const {
                std::vector<usize> path = {};
                std::vector<bool> visited(mNodes.size(), false);
                usize current = start;
                while (!visited[current]) {
                    visited[current] = true;
                    path.push_back(current);
                    bool advanced = false;
                    for (const usize dependencyIndex : mNodes[current].dependsOn) {
                        if (!mNodes[dependencyIndex].ordered && !mNodes[dependencyIndex].failed) {
                            current = dependencyIndex;
                            advanced = true;
                            break;
                        }
                    }
                    if (!advanced) {
                        return {};
                    }
                }
                const auto loopStart = std::find(path.begin(), path.end(), current);
                if (loopStart != path.begin()) {
                    // 'start' leads into the cycle without being on it.
                    return {};
                }
                return std::vector<usize>(loopStart, path.end());
            }

            void sort_failures() {
                std::sort(
                  mResult.failures.begin(),
                  mResult.failures.end(),
                  [this](const resolution_failure &left, const resolution_failure &right) {
                      return mRegistry.manifest(left.id).name < mRegistry.manifest(right.id).name;
                  }
                );
            }
    };
}

resolution_result conjure::modules::resolve_load_order(const resolution_input &input) {
    CONJURE_ASSERT(input.registry != nullptr, "resolve_load_order: the registry is required");
    if (input.registry == nullptr) {
        return {};
    }
    resolver instance(input);
    return instance.run();
}
