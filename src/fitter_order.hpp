#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace machxl {

// Placement sorting keys retain signed 16-bit values for DOS compatibility.
struct FitterOrderKey
{
    std::int16_t type          = 0;      // Physical signal class.
    std::int16_t serial        = 0;      // Logical import identifier.
    std::int16_t block         = -1;     // Assigned block; -1 for dedicated inputs.
    std::int16_t fixed_pin     = -1;     // Source pin constraint; -1 if unconstrained.
    std::int16_t fixed_node    = -1;     // Source node constraint; -1 if unconstrained.
    std::int16_t fanout_count  = 0;      // Number of destination blocks.
    std::int16_t product_terms = 0;      // Data product-term count.
    bool has_group             = false;  // Explicit group constraint.
};

// Optionally prioritize explicit pin, node and group constraints.
// The original comparator is not a general-purpose strict weak ordering
// for all hypothetical type pairs; use only original valid signal states.
int compare_fitter_signals(const FitterOrderKey& a, const FitterOrderKey& b, bool prioritize_constraints);

// Exact index permutation of the DOS quicksort,
// including equal-key swaps. Replacing this with std::sort changes ties.
std::vector<std::size_t> fitter_signal_order(const std::vector<FitterOrderKey>& keys, bool prioritize_constraints);

// Partition-cluster keys are a separate original ordering from place/route
// signals: compare rank descending, fanout sum ascending, then signal ID
// ascending, using unsigned 16-bit values.
struct PartitionOrderKey
{
    std::uint16_t rank       = 0;
    std::uint16_t fanout_sum = 0;
    std::uint16_t signal_id  = 0;
};
std::vector<std::size_t> partition_signal_order(const std::vector<PartitionOrderKey>& keys);
// Ascending failure-count keys, preserving the original qsort tie swaps.
std::vector<std::size_t> original_unsigned_order(const std::vector<unsigned>& keys);

// Used by the MACH111 control-section placement pass even
// when every output has implicit OE. These are independent of the
// earlier global signal and partition ordering keys.
struct PhysicalOutputOrderKey
{
    std::string name;
    bool fixed_node = false, fixed_pin = false;
    unsigned oe_members        = 0;
    std::int16_t product_terms = 0, fanout_count = 0;
    bool xor_product = false;
};
std::vector<std::size_t> physical_output_order(const std::vector<PhysicalOutputOrderKey>& keys);

// Placement retries walk the complete imported logical array backwards, including
// paired feedback records which do not own a separate placement frame.
// Physical output ordering replaces only the corresponding output slots.
std::vector<unsigned> placement_reverse_visit_costs(const std::vector<FitterOrderKey>& physical,
                                                    const std::vector<FitterOrderKey>& extra,
                                                    const std::vector<std::vector<std::size_t>>& output_order);

}  // namespace machxl
