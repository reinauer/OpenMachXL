#pragma once
#include "fitter_budget.hpp"
#include "fitter_place.hpp"
#include <algorithm>
#include <functional>
#include <stdexcept>

namespace machxl {
// Initial placement solves each block independently. After routing fails,
// scan a global suffix but remove only owners in the selected block.
// Unrelated block owners remain untouched.
// Product/OE ownership belongs to the callbacks, never snapshots.
struct OutputSearchCallbacks
{
    std::function<std::vector<unsigned>()> candidates;
    std::function<bool(unsigned)> commit;
    std::function<void()> remove;
};
inline bool search_output_candidates(
    const std::vector<std::vector<std::size_t>>& blocks, const std::vector<std::size_t>& global,
    const std::vector<OutputSearchCallbacks>& operations,
    const std::function<PlacementRouteFeedback(const std::vector<bool>&)>& finish,
    const std::vector<unsigned>& reverse_visit_costs = {}, const std::function<bool(unsigned)>& complete_block = {},
    const std::function<void(unsigned, const std::vector<bool>&, int)>& observe_failure = {},
    int* failed_cell_block                                                              = nullptr)
{
    struct Frame
    {
        std::vector<unsigned> list;
        std::size_t cursor = 0;
        bool initialized = false, placed = false;
    };
    std::vector<Frame> frames(operations.size());
    std::vector<unsigned> block_of(operations.size(), unsigned(-1));
    for (unsigned b = 0; b < blocks.size(); ++b)
        for (auto id : blocks[b])
            block_of.at(id) = b;
    auto remove = [&](std::size_t id) {
        if (frames[id].placed) {
            operations[id].remove();
            frames[id].placed = false;
        }
    };
    std::vector<int> failed_signals(blocks.size(), -1);
    auto observe = [&](unsigned block, int signal = -1) {
        failed_signals[block] = signal;
        if (!observe_failure)
            return;
        std::vector<bool> assigned(frames.size());
        for (std::size_t i = 0; i < frames.size(); ++i)
            assigned[i] = frames[i].placed;
        observe_failure(block, assigned, signal);
    };
    // The original initial walk allocates cells in every block before
    // checking output pads. A later block's cell failure therefore retains
    // earlier cells even if their pads would subsequently conflict.
    bool check_pads = false;
    auto fill       = [&](unsigned block, const std::vector<std::size_t>& order, std::size_t start) {
        std::size_t position = start;
        for (;;) {
            check_fitting_budget();
            if (position == order.size()) {
                record_fitting_placement(block, true);
                if (!check_pads || !complete_block || complete_block(block))
                    return true;
                // Legacy cell placement precedes physical pad assignment.
                // A pad conflict unwinds the retained block suffix rather
                // than pruning that cell when its candidate list is built.
                observe(block);
                if (position == 0)
                    return false;
                --position;
                remove(order[position]);
                continue;
            }
            const auto id = order[position];
            auto& f       = frames[id];
            if (f.initialized && f.cursor < f.list.size() && !reverse_visit_costs.empty())
                record_fitting_logical_revisit(reverse_visit_costs.at(id));
            if (!f.initialized) {
                f.list        = operations[id].candidates();
                f.cursor      = 0;
                f.initialized = true;
            }
            bool committed = false;
            while (f.cursor < f.list.size())
                if (operations[id].commit(f.list[f.cursor++])) {
                    committed = f.placed = true;
                    break;
                }
            if (committed) {
                ++position;
                continue;
            }
            observe(block, int(id));
            // Exhausting a signal's complete candidate list causes one
            // placement failure before unwinding. Count that event once,
            // not every candidate rejection or only total exhaustion.
            record_fitting_placement(block, false);
            check_fitting_iteration();
            f.initialized = false;
            if (position == 0)
                return false;
            --position;
            remove(order[position]);
        }
    };
    int failed_block = -1;
    for (unsigned b = 0; b < blocks.size(); ++b)
        if (!fill(b, blocks[b], 0) && failed_block < 0)
            failed_block = int(b);
    if (failed_block >= 0) {
        if (failed_cell_block)
            *failed_cell_block = failed_block;
        if (std::any_of(frames.begin(), frames.end(), [](const auto& frame) { return frame.placed; }))
            observe(unsigned(failed_block), failed_signals[unsigned(failed_block)]);
        record_fitting_complete_logical_walk();
        return false;
    }
    check_pads = true;
    for (unsigned b = 0; b < blocks.size(); ++b)
        if (!fill(b, blocks[b], blocks[b].size())) {
            record_fitting_complete_logical_walk();
            return false;
        }
    for (;;) {
        std::vector<bool> remaining(operations.size());
        for (auto id : global)
            remaining[id] = frames[id].cursor < frames[id].list.size();
        const auto result = finish(remaining);
        if (result.success)
            return true;
        check_fitting_iteration();
        auto target = global.end();
        if (result.retry_signal >= 0)
            target = std::find(global.begin(), global.end(), std::size_t(result.retry_signal));
        else
            for (auto it = global.end(); it != global.begin();) {
                --it;
                if (remaining[*it]) {
                    target = it;
                    break;
                }
            }
        if (target == global.end())
            return false;
        if (result.selected_only && !remaining[*target])
            return false;
        std::vector<std::size_t> selected;
        for (auto id : global)
            if (block_of[id] == block_of[*target])
                selected.push_back(id);
        const auto selected_target = std::find(selected.begin(), selected.end(), *target);
        const auto start           = std::size_t(selected_target - selected.begin());
        for (auto it = selected.end(); it != selected_target;) {
            --it;
            remove(*it);
            if (it != selected_target)
                frames[*it].initialized = false;
        }
        if (!fill(block_of[*target], selected, start))
            return false;
    }
}
}  // namespace machxl
