#pragma once
#include <chrono>
#include <cstdint>
#include <exception>
#include <optional>
#include <vector>

namespace machxl {
// A nested fit shares its caller's earlier deadline. Each host thread has
// an independent budget, so compiling one design cannot cancel another.
class FittingDeadline
{
   public:
    using Clock = std::chrono::steady_clock;
    explicit FittingDeadline(unsigned quarter_hours);
    explicit FittingDeadline(Clock::time_point deadline);
    ~FittingDeadline();
    FittingDeadline(const FittingDeadline&)            = delete;
    FittingDeadline& operator=(const FittingDeadline&) = delete;

   private:
    std::optional<Clock::time_point> previous_;
};
void check_fitting_budget();

// Physical search counters are separate from elapsed time. The caller
// checks request_repartition between complete place/route attempts; the
// routing kernel must finish its current attempt even at the threshold.
class FittingIterationCounters
{
   public:
    explicit FittingIterationCounters(unsigned blocks, bool enabled = true);
    ~FittingIterationCounters();
    FittingIterationCounters(const FittingIterationCounters&)            = delete;
    FittingIterationCounters& operator=(const FittingIterationCounters&) = delete;
    std::vector<std::uint32_t> failed_routes;
    std::vector<std::uint32_t> failed_placements;
    bool request_repartition = false;
    std::optional<std::uint64_t> logical_visits;
    unsigned logical_records = 0;
    void routing_result(unsigned block, unsigned unrouted);
    void placement_result(unsigned block, bool success);
    // Test hook: failures already counted towards the bound that applies
    // while iteration is off.
    void assume_uniterated_failures(unsigned block, std::uint32_t routes, std::uint32_t placements)
    {
        uniterated_routes_.at(block)     = routes;
        uniterated_placements_.at(block) = placements;
    }

   private:
    FittingIterationCounters* previous_;
    bool enabled_;
    // Failures counted while iteration is off; they only bound the search.
    std::vector<std::uint32_t> uniterated_routes_;
    std::vector<std::uint32_t> uniterated_placements_;
};
// Called only for full physical block routing, never individual candidate trials.
void record_fitting_route(unsigned block, unsigned unrouted);
void record_fitting_placement(unsigned block, bool success);
void begin_fitting_logical_walk(unsigned records);
void record_fitting_logical_revisit(unsigned reverse_walk_cost);
void record_fitting_complete_logical_walk();
std::optional<std::uint64_t> fitting_logical_visits();
class FittingLogicalObservationPause
{
   public:
    explicit FittingLogicalObservationPause(bool pause = true);
    ~FittingLogicalObservationPause();

   private:
    bool previous_;
};
struct RepartitionRequested : std::exception
{
    const char* what() const noexcept override { return "Physical search requested repartitioning"; }
};
// Check only between complete physical attempts, not inside a route sweep.
void check_fitting_iteration();
}  // namespace machxl
