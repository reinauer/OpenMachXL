#include "fitter_route.hpp"
#include "device_mach111.hpp"
#include "fitter_budget.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace machxl {
namespace {

unsigned count_mux_candidates(const RouteProblem& p, unsigned signal)
{
    const auto& source = p.sources[p.signal_sources[signal]];
    return p.allow_alternates[signal] ? static_cast<unsigned>(source.muxes.size()) : source.primary_count;
}

// The original keeps cursors on source records, not signals. Keep that detail
// even though normal placed designs use each physical source at most once.
bool reroute_augmenting_path(const RouteProblem& p, RouteResult& r, unsigned start)
{
    const auto old_signal_to_mux = r.signal_to_mux;
    std::vector<int> predecessor(p.mux_count, -1);
    std::vector<unsigned> cursor(p.sources.size(), 0);
    unsigned signal                  = start;
    cursor[p.signal_sources[signal]] = 0;
    for (;;) {
        check_fitting_budget();
        const auto source_id = p.signal_sources[signal];
        const auto& source   = p.sources[source_id];
        if (cursor[source_id] < count_mux_candidates(p, signal)) {
            const unsigned mux         = source.muxes[cursor[source_id]++];
            const int occupying_signal = r.mux_to_signal[mux];
            if (occupying_signal == -1) {
                predecessor[mux] = static_cast<int>(signal);
                int path_mux     = static_cast<int>(mux);
                do {
                    const int path_signal        = predecessor[path_mux];
                    r.mux_to_signal[path_mux]    = path_signal;
                    r.signal_to_mux[path_signal] = path_mux;
                    path_mux                     = old_signal_to_mux[path_signal];
                } while (path_mux != -1);
                return true;
            }
            if (predecessor[mux] == -1) {
                predecessor[mux]                 = static_cast<int>(signal);
                signal                           = static_cast<unsigned>(occupying_signal);
                cursor[p.signal_sources[signal]] = 0;
            }
        } else {
            const int previous_mux = old_signal_to_mux[signal];
            if (previous_mux == -1)
                return false;
            signal = static_cast<unsigned>(predecessor[previous_mux]);
        }
    }
}

void validate_route_problem(const RouteProblem& p)
{
    if (p.signal_sources.size() > p.mux_count)
        throw std::invalid_argument("routing signal count exceeds original array capacity");
    if (p.signal_sources.size() != p.allow_alternates.size())
        throw std::invalid_argument("routing alternate flags do not match signal count");
    std::vector<bool> seen(p.sources.size(), false);
    for (auto source : p.signal_sources) {
        if (source >= p.sources.size())
            throw std::invalid_argument("routing source index out of range");
        // Reject duplicate source IDs when constructing the routing list.
        if (seen[source])
            throw std::invalid_argument("routing source occurs more than once");
        seen[source] = true;
    }
    for (const auto& source : p.sources) {
        if (source.primary_count > source.muxes.size())
            throw std::invalid_argument("routing primary count exceeds candidate count");
        for (auto mux : source.muxes)
            if (mux >= p.mux_count)
                throw std::invalid_argument("routing candidate mux out of range");
    }
}
}  // namespace

RouteResult route_ordered(const RouteProblem& p)
{
    validate_route_problem(p);
    RouteResult r;
    r.signal_to_mux.assign(p.signal_sources.size(), -1);
    r.mux_to_signal.assign(p.mux_count, -1);
    // Route all signals greedily before attempting augmenting paths for
    // the remaining signals, preserving the supplied signal order.
    for (unsigned signal = 0; signal < p.signal_sources.size(); ++signal) {
        const auto& source = p.sources[p.signal_sources[signal]];
        for (unsigned n = 0; n < count_mux_candidates(p, signal); ++n) {
            const auto mux = source.muxes[n];
            if (r.mux_to_signal[mux] == -1) {
                r.mux_to_signal[mux]    = static_cast<int>(signal);
                r.signal_to_mux[signal] = static_cast<int>(mux);
                break;
            }
        }
        if (r.signal_to_mux[signal] == -1)
            ++r.unrouted;
    }
    for (unsigned signal = 0; r.unrouted && signal < p.signal_sources.size(); ++signal)
        if (r.signal_to_mux[signal] == -1 && reroute_augmenting_path(p, r, signal))
            --r.unrouted;
    return r;
}

RouteChoiceResult route_source_choices(const std::vector<RouteProblem>& blocks,
                                       const std::vector<RouteSourceChoice>& choices, unsigned limit)
{
    if (!limit)
        throw std::invalid_argument("routing source-choice limit must be positive");
    std::set<unsigned> changed;
    for (const auto& choice : choices) {
        if (choice.alternatives.empty() || choice.alternatives.front() != choice.source ||
            !changed.insert(choice.source).second)
            throw std::invalid_argument("invalid routing source-choice group");
        for (const auto& block : blocks)
            for (auto source : choice.alternatives)
                if (source >= block.sources.size())
                    throw std::invalid_argument("routing source-choice index out of range");
    }
    RouteChoiceResult result;
    result.selected.assign(choices.size(), 0);
    result.blocks.resize(blocks.size());
    result.signal_sources.resize(blocks.size());
    for (;;) {
        check_fitting_budget();
        ++result.attempts;
        int failed = -1;
        for (unsigned b = 0; b < blocks.size(); ++b) {
            auto problem = blocks[b];
            for (unsigned signal = 0; signal < problem.signal_sources.size(); ++signal)
                for (unsigned group = 0; group < choices.size(); ++group)
                    if (blocks[b].signal_sources[signal] == choices[group].source)
                        problem.signal_sources[signal] = choices[group].alternatives[result.selected[group]];
            result.signal_sources[b] = problem.signal_sources;
            result.blocks[b]         = route_ordered(problem);
            record_fitting_route(b, result.blocks[b].unrouted);
            if (failed < 0 && result.blocks[b].unrouted)
                failed = int(b);
        }
        if (failed < 0) {
            result.success = true;
            return result;
        }
        ++result.failed_sweeps;
        if (result.attempts >= limit)
            return result;
        const auto old = result.selected;
        bool advanced  = false;
        for (unsigned group = 0; group < choices.size(); ++group) {
            const auto& choice = choices[group];
            const auto& inputs = blocks[unsigned(failed)].signal_sources;
            // Toggle relevance once per live logical member. This is a parity test:
            // a raw pad and its input register feeding the same block cancel.
            const bool relevant =
                std::count_if(inputs.begin(), inputs.end(),
                              [&](unsigned source) {
                                  return source == choice.source ||
                                         std::find(choice.relevant_sources.begin(), choice.relevant_sources.end(),
                                                   source) != choice.relevant_sources.end();
                              }) %
                2;
            if (!relevant)
                continue;
            if (result.selected[group] + 1 < choice.alternatives.size()) {
                ++result.selected[group];
                advanced = true;
                break;
            }
            result.selected[group] = 0;
        }
        if (!advanced) {
            result.selected = old;
            return result;
        }
    }
}

RouteResult route_mach111(const std::vector<unsigned>& sources)
{
    RouteProblem p;
    p.mux_count      = 26;
    p.signal_sources = sources;
    p.allow_alternates.assign(sources.size(), false);
    p.sources.resize(70);
    for (unsigned source = 0; source < 70; ++source) {
        // PHY_111 source occurrences in increasing mux order equal the
        // explicitly ordered SIG-MUX records in MACH111.DEV.
        for (const auto choice : mach111_mux_choices(source))
            p.sources[source].muxes.push_back(choice.mux);
        p.sources[source].primary_count = static_cast<unsigned>(p.sources[source].muxes.size());
    }
    return route_ordered(p);
}

}  // namespace machxl
