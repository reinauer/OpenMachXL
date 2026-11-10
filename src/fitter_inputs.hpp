#pragma once
#include "fitter_route.hpp"
#include <functional>

namespace machxl {
struct InputPinAttempt
{
    std::vector<RouteResult> blocks;
    std::vector<std::vector<unsigned>> signal_sources;
};
struct InputPinProblem
{
    std::vector<RouteProblem> blocks;
    // Each block's routing signal index -> logical input index.
    std::vector<std::vector<unsigned>> block_signals;
    std::vector<unsigned> pins;
    // Empty for an immovable input or a feedback signal. Otherwise these
    // are electrically eligible physical pins, in original ascending order.
    std::vector<std::vector<unsigned>> candidates;
    std::vector<unsigned> placement_order;
    std::vector<bool> occupied_pins;
    std::vector<int> pin_sources;
    // Only feedback signals participate in the preliminary route. The DOS
    // fitter excludes all input types, including already fixed input pins.
    std::vector<bool> feedback;
    bool filter_candidates = true;
    // Modern IMX alternatives are exhausted inside each physical pin trial.
    // Return the final tested sources when identifying routing blockers.
    std::function<InputPinAttempt(const std::vector<unsigned>&)> route_attempt;
    // Retained candidate cursors owned by the enclosing output placer.
    // Consider these owners before advancing an input pin.
    std::vector<bool> external_movable;
    std::vector<unsigned> retry_order;
    std::vector<unsigned> reverse_visit_costs;
};
struct InputPinResult
{
    std::vector<unsigned> pins;
    std::vector<RouteResult> blocks;
    std::vector<std::vector<unsigned>> signal_sources;
    unsigned attempts            = 0;
    unsigned failed_route_sweeps = 0;  // Final routing call, not total pin trials.
    bool success                 = false;
    int outer_signal             = -1;  // Logical input index selected for retry.
};
// A zero limit follows the finite retained lists until exhaustion; the
// shared fitting deadline can still stop the search.
// Select a movable owner blocking the first unrouted signal, advance its
// retained pin list, and recreate the later input lists. Exhaustion requires
// an output placement retry.
InputPinResult route_input_pins(const InputPinProblem& problem, unsigned limit = 0);
}  // namespace machxl
