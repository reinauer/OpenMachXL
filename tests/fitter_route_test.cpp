#include "fitter_route.hpp"

#include <algorithm>
#include <stdexcept>

using namespace machxl;
#define CHECK(condition) do { \
    if (!(condition)) throw std::runtime_error("check failed: " #condition); \
} while (false)

unsigned maximum(const RouteProblem& p, unsigned signal, unsigned used) {
    if (signal == p.signal_sources.size()) return 0;
    unsigned best = maximum(p,signal+1,used);
    const auto& source = p.sources[p.signal_sources[signal]];
    const auto count = p.allow_alternates[signal] ? source.muxes.size() : source.primary_count;
    for (unsigned n = 0; n < count; ++n) {
        const auto bit = 1u << source.muxes[n];
        if (!(used & bit)) best = std::max(best,1+maximum(p,signal+1,used|bit));
    }
    return best;
}

int main() {
    // The original depth-first path takes 2->0->1 even though signal 0
    // has a shorter alternative at mux 2. A shortest-path substitute gives
    // different placements here.
    RouteProblem p{4,{{3,{0,1,2}},{2,{1,3}},{1,{0}}},{0,1,2},{false,false,false}};
    auto r = route_ordered(p);
    CHECK(r.unrouted == 0);
    CHECK((r.signal_to_mux == std::vector<int>{1,3,0}));
    CHECK((r.mux_to_signal == std::vector<int>{2,0,-1,1}));

    // Alternate candidates are included only when this signal permits them.
    p = {2,{{1,{0,1}},{1,{0}}},{0,1},{false,false}};
    r = route_ordered(p);
    CHECK(r.unrouted == 1);
    CHECK((r.signal_to_mux == std::vector<int>{0,-1}));
    p.allow_alternates[0] = true;
    r = route_ordered(p);
    CHECK(r.unrouted == 0);
    CHECK((r.signal_to_mux == std::vector<int>{1,0}));

    // These source choices are independently decoded from original 3COUNT_D.
    r = route_mach111({3,5,0,38,32});
    CHECK(r.unrouted == 0);
    CHECK((r.signal_to_mux == std::vector<int>{8,0,17,2,3}));

    // IMX choices precede physical pin retries. Carry resets only earlier
    // relevant groups; an unrelated failed block preserves earlier choices.
    {
        RouteProblem a{2,{{1,{0}},{1,{0}},{1,{1}},{1,{0}},{1,{0}},{1,{1}}},
            {0,1},{false,false}};
        auto b=a;b.signal_sources={3,4};
        const std::vector<RouteSourceChoice> choices={{0,{0,2},{}},{3,{3,5},{}}};
        auto selected=route_source_choices({a,b},choices);
        CHECK(selected.success && selected.attempts==3);
        CHECK(selected.failed_sweeps==2);
        CHECK((selected.selected==std::vector<unsigned>{1,1}));
        CHECK((selected.signal_sources[0]==std::vector<unsigned>{2,1}));
        CHECK((selected.signal_sources[1]==std::vector<unsigned>{5,4}));
        a.mux_count=3;a.sources[5].muxes={2};a.sources.push_back({1,{1}});
        a.signal_sources={0,3,6};a.allow_alternates={false,false,false};
        selected=route_source_choices({a},choices);
        CHECK(selected.success && selected.attempts==3);
        CHECK((selected.selected==std::vector<unsigned>{0,1}));
        selected=route_source_choices({a},choices,2);
        CHECK(!selected.success && selected.attempts==2);
        CHECK(selected.failed_sweeps==2);
        CHECK((selected.selected==std::vector<unsigned>{1,0}));
        CHECK((selected.signal_sources[0]==std::vector<unsigned>{2,3,6}));
    }

    // Relevance is tracked per logical member. An IMX containing two live
    // members in the failed block is skipped, even if it has a physical
    // alternative; a later one-member group must advance instead.
    {
        RouteProblem block{3,{{1,{0}},{1,{0}},{1,{1}},{1,{1}},{1,{2}}},
            {0,1,3},{false,false,false}};
        const auto result=route_source_choices({block},
            {{0,{0,2},{0,1}},{1,{1,4},{1}}});
        CHECK(result.success && result.failed_sweeps==1);
        CHECK((result.selected==std::vector<unsigned>{0,1}));
    }

    // Exhaustively compare feasibility on all three-by-three graphs with
    // an independent enumeration, including failures after partial routing.
    for (unsigned graph = 0; graph < 512; ++graph) {
        p = {3,{}, {0,1,2}, {false,false,false}};
        for (unsigned signal = 0; signal < 3; ++signal) {
            RouteCandidates source;
            for (unsigned mux = 0; mux < 3; ++mux)
                if (graph & (1u << (3*signal+mux))) source.muxes.push_back(mux);
            source.primary_count = static_cast<unsigned>(source.muxes.size());
            p.sources.push_back(source);
        }
        r = route_ordered(p);
        CHECK(r.unrouted == 3-maximum(p,0,0));
        for (unsigned signal = 0; signal < 3; ++signal) {
            const int mux = r.signal_to_mux[signal];
            if (mux != -1) {
                CHECK(r.mux_to_signal[mux] == static_cast<int>(signal));
                CHECK(graph & (1u << (3*signal+mux)));
            }
        }
    }
}
