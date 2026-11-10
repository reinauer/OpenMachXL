#pragma once
#include "fitter_budget.hpp"
#include <stdexcept>
#include <vector>

namespace machxl {
// Start with four physical attempts. Successful partitioning advances the
// ranking phase, consumes an attempt and restricts the final two attempts.
struct PartitionIterationState
{
    unsigned remaining = 4, phase = 0, completed = 0;
    bool restricted = false, previous_success = false;
    bool balanced_phase    = false;
    bool grounded_implicit = false, auxiliary_retry = false;
    bool best_product_pressure = false, best_fixed_failure = false;
    std::size_t best_progress = 0;
    std::vector<unsigned> grounded;
    // Cell-allocation failure reserves one cluster in the affected block
    // on later partition runs. The reservation does not accumulate.
    std::vector<bool> product_reservations;
    void reserve_product_cluster(unsigned block)
    {
        if (product_reservations.size() <= block)
            product_reservations.resize(block + 1);
        product_reservations[block] = true;
    }
    explicit PartitionIterationState(bool iterate = true) : remaining(iterate ? 4 : 1) {}
    void succeeded(unsigned current_phase, bool balanced = false)
    {
        balanced_phase = balanced;
        phase          = restricted || current_phase == 4 ? 0 : current_phase + 1;
        if (!restricted && current_phase == 0 && balanced) {
            phase          = 0;
            balanced_phase = false;
        }
        if (restricted || current_phase == 4)
            restricted = true;
        previous_success = true;
        ++completed;
        if (remaining)
            --remaining;
        if (remaining < 3)
            restricted = true;
    }
};
// Only exhaustion of a valid physical search may start a new partition.
// Invalid source constraints and partition failure remain ordinary errors.
struct PhysicalPlacementFailure : std::runtime_error
{
    using std::runtime_error::runtime_error;
};
}  // namespace machxl
