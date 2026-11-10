#include "fitter_budget.hpp"
#include "logic.hpp"

namespace machxl {
namespace {
thread_local std::optional<FittingDeadline::Clock::time_point> deadline;
thread_local FittingIterationCounters* iteration_counters = nullptr;
thread_local bool logical_observation_paused              = false;
}  // namespace
FittingDeadline::FittingDeadline(unsigned quarter_hours) : previous_(deadline)
{
    if (quarter_hours > 999)
        throw LogicError("M3XX_RUNTIME must be between 0 and 999");
    if (quarter_hours) {
        const auto requested = Clock::now() + std::chrono::seconds(900 * quarter_hours);
        if (!deadline || requested < *deadline)
            deadline = requested;
    }
}
FittingDeadline::FittingDeadline(Clock::time_point requested) : previous_(deadline)
{
    if (!deadline || requested < *deadline)
        deadline = requested;
}
FittingDeadline::~FittingDeadline()
{
    deadline = previous_;
}
void check_fitting_budget()
{
    if (deadline && FittingDeadline::Clock::now() >= *deadline)
        throw LogicError("Fitting run-time limit exceeded");
}
FittingIterationCounters::FittingIterationCounters(unsigned blocks, bool enabled)
    : failed_routes(blocks), failed_placements(blocks), previous_(iteration_counters), enabled_(enabled)
{
    iteration_counters = this;
}
FittingIterationCounters::~FittingIterationCounters()
{
    iteration_counters = previous_;
}
void FittingIterationCounters::routing_result(unsigned block, unsigned unrouted)
{
    if (enabled_ && unrouted && ++failed_routes.at(block) >= 500000)
        request_repartition = true;
}
void FittingIterationCounters::placement_result(unsigned block, bool success)
{
    if (!enabled_)
        return;
    if (success)
        failed_placements.at(block) = 0;
    else if (++failed_placements.at(block) > 400000)
        request_repartition = true;
}
void record_fitting_route(unsigned block, unsigned unrouted)
{
    if (iteration_counters)
        iteration_counters->routing_result(block, unrouted);
}
void record_fitting_placement(unsigned block, bool success)
{
    if (iteration_counters)
        iteration_counters->placement_result(block, success);
}
void begin_fitting_logical_walk(unsigned records)
{
    if (iteration_counters) {
        iteration_counters->logical_visits  = records;
        iteration_counters->logical_records = records;
    }
}
void record_fitting_complete_logical_walk()
{
    if (iteration_counters)
        record_fitting_logical_revisit(iteration_counters->logical_records);
}
void record_fitting_logical_revisit(unsigned cost)
{
    if (!logical_observation_paused && iteration_counters && iteration_counters->logical_visits)
        *iteration_counters->logical_visits += cost;
}
FittingLogicalObservationPause::FittingLogicalObservationPause(bool pause) : previous_(logical_observation_paused)
{
    logical_observation_paused |= pause;
}
FittingLogicalObservationPause::~FittingLogicalObservationPause()
{
    logical_observation_paused = previous_;
}
std::optional<std::uint64_t> fitting_logical_visits()
{
    return iteration_counters ? iteration_counters->logical_visits : std::nullopt;
}
void check_fitting_iteration()
{
    if (iteration_counters && iteration_counters->request_repartition)
        throw RepartitionRequested{};
}
}  // namespace machxl
