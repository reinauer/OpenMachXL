#include "fitter_inputs.hpp"
#include "fitter_budget.hpp"
#include <stdexcept>
#include <iostream>
using namespace machxl;
static void check(bool condition,const char* message) {
    if(!condition)throw std::runtime_error(message);
}
int main() {
    InputPinProblem p;
    RouteProblem block;block.mux_count=2;
    block.sources={{1,{0}},{1,{0}},{1,{1}}};
    block.signal_sources={0,1};block.allow_alternates={false,false};
    p.blocks={block};p.block_signals={{0,1}};p.pins={1,2};
    p.candidates={{1,2,3},{1,2,3}};p.placement_order={0,1};
    p.occupied_pins={false,false,false,false};p.pin_sources={-1,0,1,2};
    p.feedback={false,false};
    auto r=route_input_pins(p);
    check(r.success&&r.attempts==3,"blocking owner retry sequence changed");
    check(r.pins==std::vector<unsigned>({3,1}),"stored list advance or suffix refill changed");
    check(r.blocks[0].signal_to_mux==std::vector<int>({1,0}),"final input routing changed");
    {
        FittingIterationCounters counters(1);
        counters.failed_routes[0]=499999;
        bool stopped=false;
        try {route_input_pins(p);}catch(const RepartitionRequested&) {stopped=true;}
        check(stopped&&counters.failed_routes[0]==500000,
              "repartition must stop before the next physical input placement");
        FittingIterationCounters disabled(1,false);
        r=route_input_pins(p);
        check(r.success&&r.attempts==3,"ITERATE_FITR=N must retain input retries");
    }
    p.route_attempt=[&](const std::vector<unsigned>& pins) {
        auto trial=block;
        for(unsigned i=0;i<pins.size();++i)trial.signal_sources[i]=unsigned(p.pin_sources[pins[i]]);
        return InputPinAttempt{{route_ordered(trial)},{trial.signal_sources}};
    };
    r=route_input_pins(p);
    check(r.success&&r.attempts==3&&r.pins==std::vector<unsigned>({3,1}),"inner routing callback changed outer retry");
    check(r.signal_sources==std::vector<std::vector<unsigned>>({{2,0}}),"callback source snapshot lost");
    p.route_attempt={};
    r=route_input_pins(p,1);
    check(!r.success&&r.attempts==1&&r.pins==p.pins,"retry budget mutated initial placement");
    p.candidates[0].clear();p.placement_order={1};p.occupied_pins[1]=true;
    p.external_movable={true,false};p.retry_order={0,1};
    r=route_input_pins(p);
    check(!r.success&&r.attempts==1&&r.outer_signal==0&&r.pins==p.pins,
          "external blocker must precede unrelated input movement");
    p.external_movable.clear();p.retry_order.clear();
    r=route_input_pins(p);
    check(r.success&&r.attempts==2&&r.pins==std::vector<unsigned>({1,3}),"fixed blocker fallback changed");
    p.candidates[1]={2};
    r=route_input_pins(p);
    check(!r.success&&r.pins==p.pins,"exhausted input lists changed fixed placement");
    std::cout<<"input candidate retries, blocker selection and exhaustion verified\n";
}
