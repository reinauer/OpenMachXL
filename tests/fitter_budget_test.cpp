#include "fitter_budget.hpp"
#include "logic.hpp"
#include "fitter_route.hpp"
#include <cassert>

int main() {
    using namespace machxl;
    check_fitting_budget();
    check_fitting_iteration();
    {
        FittingDeadline unlimited(0);
        check_fitting_budget();
        {
            FittingDeadline expired(FittingDeadline::Clock::now()-std::chrono::seconds(1));
            for(unsigned nested_quarters:{0u,1u}) {
                FittingDeadline nested(nested_quarters);
                bool stopped=false;
                try {check_fitting_budget();}catch(const LogicError&) {stopped=true;}
                assert(stopped);
            }
        }
        check_fitting_budget();
    }
    bool rejected=false;
    try {FittingDeadline invalid(1000);}catch(const LogicError&) {rejected=true;}
    assert(rejected);
    check_fitting_budget();
    const RouteProblem congested{2,{{1,{0}},{1,{0}}},{0,1},{false,false}};
    const RouteProblem routable{2,{{1,{0}},{1,{0}}},{0},{false}};
    {
        FittingIterationCounters counters(2);
        counters.failed_routes[0]=499998;
        // Candidate feasibility probes use the bare kernel and do not
        // consume the original complete-routing invocation budget.
        assert(route_ordered(congested).unrouted==1);
        assert(counters.failed_routes[0]==499998);
        assert(!route_source_choices({congested,routable},{},4).success);
        assert(counters.failed_routes[0]==499999&&!counters.request_repartition);
        assert(counters.failed_routes[1]==0);
        {
            FittingIterationCounters nested(2);
            route_source_choices({routable,congested},{},4);
            assert(nested.failed_routes[1]==1&&counters.failed_routes[0]==499999);
        }
        route_source_choices({congested,routable},{},4);
        assert(counters.failed_routes[0]==500000&&counters.request_repartition);
        bool repartition=false;
        try {check_fitting_iteration();}catch(const RepartitionRequested&) {repartition=true;}
        assert(repartition);
        {
            FittingIterationCounters disabled(2,false);
            disabled.failed_routes[0]=499999;
            disabled.failed_placements[0]=400000;
            route_source_choices({congested,routable},{},4);
            record_fitting_placement(0,false);
            check_fitting_iteration();
            assert(disabled.failed_routes[0]==499999);
            assert(disabled.failed_placements[0]==400000);
            assert(!disabled.request_repartition);
        }
        {
            // With iteration off a search that keeps failing is abandoned
            // with an error far beyond the repartition thresholds.
            FittingIterationCounters disabled(2,false);
            disabled.assume_uniterated_failures(0,4999998,3999999);
            route_source_choices({congested,routable},{},4);
            record_fitting_placement(0,false);
            bool abandoned=false;
            try {route_source_choices({congested,routable},{},4);}catch(const LogicError&) {abandoned=true;}
            assert(abandoned);
            abandoned=false;
            try {record_fitting_placement(0,false);}catch(const LogicError&) {abandoned=true;}
            assert(abandoned);
            record_fitting_placement(1,false);
            assert(!disabled.request_repartition);
        }
        repartition=false;
        try {check_fitting_iteration();}catch(const RepartitionRequested&) {repartition=true;}
        assert(repartition);
        counters.routing_result(0,0);
        assert(counters.failed_routes[0]==500000);
    }
    {
        FittingIterationCounters counters(1);
        counters.failed_placements[0]=399999;
        counters.placement_result(0,false);
        assert(counters.failed_placements[0]==400000&&!counters.request_repartition);
        counters.placement_result(0,true);
        assert(counters.failed_placements[0]==0&&!counters.request_repartition);
        counters.failed_placements[0]=400000;
        counters.placement_result(0,false);
        assert(counters.request_repartition);
        counters.placement_result(0,true);
        assert(counters.failed_placements[0]==0&&counters.request_repartition);
    }
    {
        // A diagnostic feasibility route must neither consume a production
        // retry budget nor change its logical visit count. An expired dump
        // must restore both scopes while unwinding its deadline exception.
        FittingIterationCounters production(1);
        production.failed_routes[0]=499999;
        begin_fitting_logical_walk(17);
        bool expired=false;
        try {
            FittingLogicalObservationPause paused;
            const auto visits=fitting_logical_visits();
            FittingIterationCounters diagnostic(1,false);
            diagnostic.logical_visits=visits;
            assert(!route_source_choices({congested},{},4).success);
            record_fitting_logical_revisit(9);
            {
                FittingLogicalObservationPause nested(false);
                record_fitting_logical_revisit(11);
            }
            assert(fitting_logical_visits()==17);
            assert(diagnostic.failed_routes[0]==0);
            check_fitting_iteration();
            FittingDeadline deadline(FittingDeadline::Clock::now()-std::chrono::seconds(1));
            check_fitting_budget();
        } catch(const LogicError& error) {
            expired=std::string(error.what())=="Fitting run-time limit exceeded";
        }
        assert(expired);
        assert(production.failed_routes[0]==499999&&!production.request_repartition);
        assert(fitting_logical_visits()==17);
        check_fitting_budget();check_fitting_iteration();
        record_fitting_logical_revisit(5);
        assert(fitting_logical_visits()==22);
        record_fitting_route(0,1);
        assert(production.failed_routes[0]==500000&&production.request_repartition);
    }
    // Both nested scopes restore the absence of an active counter set.
    route_source_choices({congested},{},4);
    check_fitting_iteration();
}
