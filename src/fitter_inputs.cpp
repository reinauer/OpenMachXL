#include "fitter_inputs.hpp"
#include "fitter_budget.hpp"
#include <algorithm>
#include <stdexcept>

namespace machxl {
InputPinResult route_input_pins(const InputPinProblem& p, unsigned limit)
{
    const auto count = p.pins.size();
    if (p.candidates.size() != count || p.feedback.size() != count || p.blocks.size() != p.block_signals.size() ||
        p.occupied_pins.size() != p.pin_sources.size())
        throw std::invalid_argument("invalid input pin routing problem");
    std::vector<bool> movable(count);
    for (auto i : p.placement_order) {
        if (i >= count || movable[i] || p.candidates[i].empty())
            throw std::invalid_argument("invalid input pin placement order");
        movable[i] = true;
        for (auto pin : p.candidates[i])
            if (pin >= p.pin_sources.size() || p.pin_sources[pin] < 0)
                throw std::invalid_argument("invalid candidate input pin");
    }
    for (unsigned b = 0; b < p.blocks.size(); ++b) {
        if (p.block_signals[b].size() != p.blocks[b].signal_sources.size())
            throw std::invalid_argument("input routing signal map size mismatch");
        for (auto i : p.block_signals[b])
            if (i >= count)
                throw std::invalid_argument("input routing signal index out of range");
    }
    InputPinResult result;
    result.pins = p.pins;
    result.blocks.resize(p.blocks.size());
    if (!p.external_movable.empty() && p.external_movable.size() != count)
        throw std::invalid_argument("external routing owner count mismatch");
    const auto external     = [&](unsigned i) { return !p.external_movable.empty() && p.external_movable[i]; };
    const auto& retry_order = p.retry_order.empty() ? p.placement_order : p.retry_order;
    for (auto i : retry_order)
        if (i >= count)
            throw std::invalid_argument("invalid routing retry order");
    std::vector<std::vector<unsigned>> lists(count);
    std::vector<unsigned> cursors(count);
    std::vector<bool> occupied = p.occupied_pins;
    std::vector<RouteResult> partial(p.blocks.size());
    bool initialized = false;
    auto available   = [&](unsigned input, unsigned pin) {
        if (!p.filter_candidates)
            return true;
        bool live = false;
        for (unsigned b = 0; b < p.blocks.size(); ++b) {
            if (std::find(p.block_signals[b].begin(), p.block_signals[b].end(), input) == p.block_signals[b].end())
                continue;
            live               = true;
            const auto& source = p.blocks[b].sources.at(unsigned(p.pin_sources[pin]));
            for (unsigned n = 0; n < source.primary_count; ++n)
                if (partial[b].mux_to_signal[source.muxes[n]] < 0)
                    return true;
        }
        return !live;
    };
    auto create_list = [&](unsigned input) {
        auto& list = lists[input];
        list.clear();
        cursors[input] = 0;
        for (auto pin : p.candidates[input])
            if (!occupied[pin] && available(input, pin))
                list.push_back(pin);
    };
    for (;;) {
        check_fitting_budget();
        ++result.attempts;
        result.failed_route_sweeps = 0;
        int failed                 = -1;
        if (p.route_attempt) {
            auto attempt = p.route_attempt(result.pins);
            if (attempt.blocks.size() != p.blocks.size() || attempt.signal_sources.size() != p.blocks.size())
                throw std::invalid_argument("input route callback block count mismatch");
            result.blocks         = std::move(attempt.blocks);
            result.signal_sources = std::move(attempt.signal_sources);
            for (unsigned b = 0; b < p.blocks.size(); ++b) {
                if (result.signal_sources[b].size() != p.block_signals[b].size())
                    throw std::invalid_argument("input route callback signal count mismatch");
                if (failed < 0 && result.blocks[b].unrouted)
                    failed = int(b);
            }
        } else {
            result.signal_sources.resize(p.blocks.size());
            for (unsigned b = 0; b < p.blocks.size(); ++b) {
                auto block = p.blocks[b];
                for (unsigned n = 0; n < block.signal_sources.size(); ++n) {
                    const auto i = p.block_signals[b][n];
                    if (movable[i])
                        block.signal_sources[n] = unsigned(p.pin_sources.at(result.pins[i]));
                }
                result.signal_sources[b] = block.signal_sources;
                result.blocks[b]         = route_ordered(block);
                record_fitting_route(b, result.blocks[b].unrouted);
                if (failed < 0 && result.blocks[b].unrouted)
                    failed = int(b);
            }
        }
        if (failed < 0) {
            result.success = true;
            return result;
        }
        // Legacy devices use one selector configuration per complete routing
        // call. Modern routing supplies its own failed-sweep count.
        if (!p.route_attempt)
            result.failed_route_sweeps = 1;
        check_fitting_iteration();
        if (limit && result.attempts >= limit)
            return result;
        const auto b            = unsigned(failed);
        const auto select_outer = [&]() {
            const auto& route = result.blocks[b];
            for (unsigned n = 0; n < route.signal_to_mux.size(); ++n)
                if (route.signal_to_mux[n] < 0) {
                    const auto& candidates = p.blocks[b].sources[result.signal_sources[b][n]];
                    for (unsigned k = 0; k < candidates.primary_count; ++k) {
                        const int owner = route.mux_to_signal[candidates.muxes[k]];
                        if (owner >= 0) {
                            const auto i = p.block_signals[b][unsigned(owner)];
                            if (external(i)) {
                                result.outer_signal = int(i);
                                return;
                            }
                        }
                    }
                    break;
                }
            for (auto it = retry_order.rbegin(); it != retry_order.rend(); ++it)
                if (external(*it) &&
                    std::find(p.block_signals[b].begin(), p.block_signals[b].end(), *it) != p.block_signals[b].end()) {
                    result.outer_signal = int(*it);
                    return;
                }
        };
        if (!initialized) {
            // Preliminary routing excludes floating input pins and reserves
            // feedback CSM muxes for subsequent input-candidate feasibility checks.
            for (unsigned b = 0; b < p.blocks.size(); ++b) {
                auto block = p.blocks[b];
                block.signal_sources.clear();
                block.allow_alternates.clear();
                for (unsigned n = 0; n < p.block_signals[b].size(); ++n)
                    if (p.feedback[p.block_signals[b][n]]) {
                        block.signal_sources.push_back(p.blocks[b].signal_sources[n]);
                        block.allow_alternates.push_back(p.blocks[b].allow_alternates[n]);
                    }
                partial[b] = route_ordered(block);
                if (partial[b].unrouted) {
                    select_outer();
                    return result;
                }
            }
            for (auto i : p.placement_order) {
                create_list(i);
                // A discrepancy means preliminary output routing changed
                // placement eligibility and needs the enclosing output retry.
                if (lists[i].empty() || lists[i].front() != result.pins[i]) {
                    select_outer();
                    return result;
                }
                cursors[i]               = 1;
                occupied[result.pins[i]] = true;
            }
            initialized = true;
        }
        const auto& route = result.blocks[b];
        int selected      = -1;
        // Inspect the first unrouted signal and the owners of its primary mux
        // candidates before the global fallback scan.
        for (unsigned n = 0; n < route.signal_to_mux.size(); ++n)
            if (route.signal_to_mux[n] < 0) {
                const auto source      = result.signal_sources[b][n];
                const auto& candidates = p.blocks[b].sources[source];
                for (unsigned k = 0; k < candidates.primary_count; ++k) {
                    const int owner = route.mux_to_signal[candidates.muxes[k]];
                    if (owner < 0)
                        throw std::logic_error("unrouted input has a free primary mux");
                    const auto i = p.block_signals[b][unsigned(owner)];
                    if (external(i)) {
                        result.outer_signal = int(i);
                        return result;
                    }
                    if (movable[i] && cursors[i] < lists[i].size()) {
                        selected = int(i);
                        break;
                    }
                }
                break;
            }
        if (selected < 0)
            for (auto it = retry_order.rbegin(); it != retry_order.rend(); ++it)
                if (std::find(p.block_signals[b].begin(), p.block_signals[b].end(), *it) != p.block_signals[b].end()) {
                    if (external(*it)) {
                        result.outer_signal = int(*it);
                        return result;
                    }
                    if (movable[*it] && cursors[*it] < lists[*it].size()) {
                        selected = int(*it);
                        break;
                    }
                }
        if (selected < 0)
            return result;
        if (!p.reverse_visit_costs.empty())
            record_fitting_logical_revisit(p.reverse_visit_costs.at(unsigned(selected)));
        const auto start = std::find(p.placement_order.begin(), p.placement_order.end(), unsigned(selected));
        for (auto it = start; it != p.placement_order.end(); ++it)
            occupied[result.pins[*it]] = false;
        result.pins[unsigned(selected)]           = lists[unsigned(selected)][cursors[unsigned(selected)]++];
        occupied[result.pins[unsigned(selected)]] = true;
        for (auto it = start + 1; it != p.placement_order.end(); ++it) {
            create_list(*it);
            if (lists[*it].empty())
                return result;
            result.pins[*it]           = lists[*it][cursors[*it]++];
            occupied[result.pins[*it]] = true;
        }
    }
}
}  // namespace machxl
