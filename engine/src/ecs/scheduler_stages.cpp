#include "conjure/ecs/scheduler.h"

#include "ecs/scheduler_internal.h"

#include "conjure/ecs/log_category.h"

#include "conjure/core/log.h"

#include <algorithm>
#include <limits>
#include <vector>

// Stage construction (docs/design/01_ecs.md 3.5): edges from after/before names, Kahn's topological
// sort with registration order as the tiebreak, then greedy append-only packing into conflict-free
// stages.

using namespace conjure;
using namespace conjure::ecs;

namespace {
    bool conflicts(const system_desc &candidate, const system_desc &member) noexcept {
        const bool writesClash =
          candidate.writes.contains_any(member.reads) || candidate.writes.contains_any(member.writes);
        const bool readsClash = candidate.reads.contains_any(member.writes);
        return writesClash || readsClash;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Private methods

void scheduler::resolve_edges(
  system_phase target,
  std::vector<std::vector<u32>> &predecessors,
  const system_record *logFor
) {
    // Members: every alive system of the phase in registration order (disabled ones included so a
    // latent cycle is still refused). Positions in 'members' index the predecessor lists. Only the
    // unknown names of 'logFor' (the system being added) are logged, so each is logged once.
    std::vector<u32> members = {};
    for (u32 index = 0; index < mSystems.size(); ++index) {
        const system_record *record = mSystems[index].get();
        if (record != nullptr && record->alive && record->desc.phase == target) {
            members.push_back(index);
        }
    }
    std::sort(members.begin(), members.end(), [this](u32 left, u32 right) {
        return mSystems[left]->registrationOrder < mSystems[right]->registrationOrder;
    });

    // The null name stands for no system: unnamed systems may repeat in a phase (01_ecs.md D13), so an
    // edge to it would bind to whichever was registered first. It is resolved like an unknown name.
    predecessors.assign(members.size(), {});
    const auto position_of = [&](core::string_id name) -> usize {
        if (name.is_null()) {
            return members.size();
        }
        for (usize position = 0; position < members.size(); ++position) {
            if (mSystems[members[position]]->desc.name == name) {
                return position;
            }
        }
        return members.size();
    };

    for (usize position = 0; position < members.size(); ++position) {
        const system_desc &desc = mSystems[members[position]]->desc;
        const bool logUnknown = mSystems[members[position]].get() == logFor;
        for (const core::string_id &name : desc.after) {
            const usize other = position_of(name);
            if (other == members.size()) {
                if (logUnknown) {
                    CONJURE_LOG_DEBUG(LOG_ECS, "system '{}' runs after unknown system '{}'; ignored", desc.name, name);
                }
                continue;
            }
            predecessors[position].push_back(static_cast<u32>(other));
        }
        for (const core::string_id &name : desc.before) {
            const usize other = position_of(name);
            if (other == members.size()) {
                if (logUnknown) {
                    CONJURE_LOG_DEBUG(LOG_ECS, "system '{}' runs before unknown system '{}'; ignored", desc.name, name);
                }
                continue;
            }
            predecessors[other].push_back(static_cast<u32>(position));
        }
    }
}

core::result<std::vector<u32>> scheduler::topological_order(
  system_phase target,
  const std::vector<std::vector<u32>> &predecessors
) {
    // Members in the same order resolve_edges used.
    std::vector<u32> members = {};
    for (u32 index = 0; index < mSystems.size(); ++index) {
        const system_record *record = mSystems[index].get();
        if (record != nullptr && record->alive && record->desc.phase == target) {
            members.push_back(index);
        }
    }
    std::sort(members.begin(), members.end(), [this](u32 left, u32 right) {
        return mSystems[left]->registrationOrder < mSystems[right]->registrationOrder;
    });

    const usize count = members.size();
    std::vector<u32> indegree(count, 0);
    for (usize position = 0; position < count; ++position) {
        indegree[position] = static_cast<u32>(predecessors[position].size());
    }

    std::vector<bool> emitted(count, false);
    std::vector<u32> order = {};
    order.reserve(count);
    while (order.size() < count) {
        // Registration order is the tiebreak: the first ready member wins.
        usize next = count;
        for (usize position = 0; position < count; ++position) {
            if (!emitted[position] && indegree[position] == 0) {
                next = position;
                break;
            }
        }
        if (next == count) {
            return core::make_error(
              ecs_error::SYSTEM_CYCLE,
              "after/before edges form a cycle in phase " + std::string(to_string(target))
            );
        }
        emitted[next] = true;
        order.push_back(members[next]);
        for (usize position = 0; position < count; ++position) {
            for (const u32 predecessor : predecessors[position]) {
                if (predecessor == next) {
                    --indegree[position];
                }
            }
        }
    }
    return order;
}

core::result<void> scheduler::build_stages(system_phase target) {
    phase_state &state = (*mPhases)[static_cast<usize>(target)];
    state.stages.clear();
    state.timings.clear();

    std::vector<std::vector<u32>> predecessors = {};
    resolve_edges(target, predecessors, nullptr);
    CONJURE_TRY_ASSIGN(const std::vector<u32> order, topological_order(target, predecessors));

    // Position of each record in the predecessor lists, for the "predecessor in an earlier stage" rule.
    std::vector<u32> members = {};
    for (u32 index = 0; index < mSystems.size(); ++index) {
        const system_record *record = mSystems[index].get();
        if (record != nullptr && record->alive && record->desc.phase == target) {
            members.push_back(index);
        }
    }
    std::sort(members.begin(), members.end(), [this](u32 left, u32 right) {
        return mSystems[left]->registrationOrder < mSystems[right]->registrationOrder;
    });
    const auto position_of = [&](u32 recordIndex) -> usize {
        for (usize position = 0; position < members.size(); ++position) {
            if (members[position] == recordIndex) {
                return position;
            }
        }
        return members.size();
    };

    std::vector<i32> stageOf(members.size(), -1);
    for (const u32 recordIndex : order) {
        const system_record &record = *mSystems[recordIndex];
        const usize position = position_of(recordIndex);
        if (has_flag(record.desc.flags, system_flags::DISABLED)) {
            continue;
        }
        const bool exclusive = has_flag(record.desc.flags, system_flags::EXCLUSIVE);

        bool joins = !state.stages.empty() && !exclusive && !state.stages.back().exclusive;
        if (joins) {
            const i32 current = static_cast<i32>(state.stages.size()) - 1;
            for (const u32 predecessor : predecessors[position]) {
                if (stageOf[predecessor] >= current) {
                    joins = false;
                    break;
                }
            }
        }
        if (joins) {
            for (const system_id memberId : state.stages.back().systems) {
                if (conflicts(record.desc, mSystems[memberId.index]->desc)) {
                    joins = false;
                    break;
                }
            }
        }
        if (!joins) {
            stage_info stage = {};
            stage.exclusive = exclusive;
            state.stages.push_back(std::move(stage));
        }
        state.stages.back().systems.push_back(system_id{recordIndex, record.generation});
        stageOf[position] = static_cast<i32>(state.stages.size()) - 1;
        state.timings.push_back(system_timing{record.desc.name, 0});
    }

    state.dirty = false;
    return core::result<void>::success();
}
