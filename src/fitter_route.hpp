#pragma once

#include <cstddef>
#include <vector>

namespace machxl {

// Ordered routing candidates: primary count, alternate count and mux list.
// Order is significant, including when an augmenting path moves an already
// routed signal. These lists must not be sorted by the routing algorithm.
struct RouteCandidates
{
    unsigned primary_count = 0;
    std::vector<unsigned> muxes;
};

struct RouteProblem
{
    unsigned mux_count = 0;
    std::vector<RouteCandidates> sources;
    // Signal order is supplied by the placement stage, not
    // inferred from names or physical source numbers by the routing kernel.
    std::vector<unsigned> signal_sources;
    std::vector<bool> allow_alternates;
};

struct RouteResult
{
    std::vector<int> signal_to_mux;
    std::vector<int> mux_to_signal;
    unsigned unrouted = 0;
};

// CSM routing kernel. It does not perform partitioning, placement, IMX
// configuration selection, or placement retries.
RouteResult route_ordered(const RouteProblem& problem);

// One variable IMX source. Alternatives follow the original valid-selector
// order; relevant_sources lists all live sources belonging to that IMX
// group, including sources whose number does not change with this choice.
struct RouteSourceChoice
{
    unsigned source = 0;
    std::vector<unsigned> alternatives;
    std::vector<unsigned> relevant_sources;
};
struct RouteChoiceResult
{
    std::vector<RouteResult> blocks;
    std::vector<std::vector<unsigned>> signal_sources;
    std::vector<unsigned> selected;
    unsigned attempts = 0;
    // Failed complete sweeps in this routing invocation. Unlike
    // attempts-1 this also includes the last sweep on exhaustion.
    unsigned failed_sweeps = 0;
    bool success           = false;
};
// Route all blocks, then advance the first
// failed-block-relevant IMX selector, carrying exhausted selectors in
// physical group order. This precedes outer pin/node placement retries.
RouteChoiceResult route_source_choices(const std::vector<RouteProblem>& blocks,
                                       const std::vector<RouteSourceChoice>& choices, unsigned limit = 4096);

// MACH111 has no alternate mux candidates. The caller supplies the
// placed signal order; output -1 means that signal could not be routed.
RouteResult route_mach111(const std::vector<unsigned>& ordered_sources);

}  // namespace machxl
