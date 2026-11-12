#pragma once
#include "fitter_partition.hpp"
#include <functional>
#include <string>

namespace machxl {
struct PartitionIterationState;

// Input and output records are separate, including a bidirectional pin's
// input record and its output macrocell. Serial/fanin IDs are original
// logical IDs, never the physical pin assigned during placement.
struct Mach111PlaceSignal
{
    std::string name;
    FitterOrderKey order;
    bool output = false;
    bool buried = false;
    std::vector<std::uint16_t> fanins;
    std::optional<std::uint16_t> reset;
    unsigned oe_members = 0;
    bool xor_product    = false;
    // Fixed NODE placement in global cell coordinates. Keep this constraint
    // separate from sorting keys, which use local cell indices after placement.
    int fixed_cell = -1;
    // Canonical nonconstant OE cube in the synthesized PLA input order.
    // Equal cubes share a physical enable product within a half-block.
    std::string oe_product;
    std::optional<std::uint16_t> preset;
    // PLA cube of a single-product reset or set equation, else empty.
    std::string reset_product, preset_product;
    // Physical comparator uses the parser's abbreviated vector spelling.
    std::string sort_name;
    // Modern global-row identities, including explicit constants.
    std::optional<std::uint16_t> global_reset;
    std::optional<std::uint16_t> global_preset;
    bool local_clock                         = false;
    bool force_async                         = false;
    bool paired_output                       = false;
    bool input_register                      = false;
    int paired_input                         = -1;  // Signal index of the physical input pin.
    int global_clock                         = -1;  // Dedicated physical clock ordinal before phase.
    bool clock_positive                      = true;
    std::optional<std::uint16_t> clock_fanin = {};
    bool explicit_async_control              = false;  // OE/RE/PR, including constants.
    // Live CSM sources, independent of physical macrocell occupancy.
    bool feedback_live          = false;
    bool raw_input_live         = false;
    bool force_sync             = false;
    unsigned assigned_clock_pin = 0;      // Clock normalization result, not a source constraint.
    bool clocked_storage        = false;  // excludes COM and input records.
};
struct Mach111PlaceOptions
{
    bool balanced_partition            = true;     // +s / BALANCED_PAR
    bool spread_placement              = true;     // x bit19 / SPREAD_PL
    bool reserve_default_clock         = false;    // Unspecified CLKF uses pin35.
    bool allow_global_product_clocks   = false;    // PTERM_CLK=Y permits CSM fallback.
    bool iterate_partition             = true;     // ITERATE_FITR / +r.
    PartitionIterationState* iteration = nullptr;  // Internal retained rank state.
    // The mapping translates partition cluster indices to placer signals.
    std::function<void(const std::vector<Mach111PlaceSignal>&, const std::vector<std::size_t>&,
                       const PartitionObservation&)>
        partition_observer                           = {};
    bool collect_report_visits                       = false;
    std::vector<FitterOrderKey> report_extra_records = {};
};
struct PlacementRouteFeedback
{
    bool success     = true;
    int retry_signal = -1;  // Placer signal index, or ordinary latest unwind.
    // An explicit input-mux retry target cannot fall back to another owner
    // when its retained candidate list is exhausted.
    bool selected_only = false;
};
struct Mach111Placement
{
    // Signal-indexed, -1 for no physical resource. Pins are one-based;
    // cells are A0..A15=0..15, B0..B15=16..31.
    std::vector<int> cells, pins;
    // Signal-indexed physical OE slots0..3 within the assigned block.
    // -1 denotes implicit/constant enable or an input/buried signal.
    std::vector<int> oe_slots;
    // All serialized OE cubes, including unused slots retained from the
    // partition resource assignment by the original report conversion.
    std::array<std::array<std::string, 4>, 2> oe_products;
    std::vector<std::size_t> global_order;
    std::array<std::vector<std::size_t>, 2> output_order;
    // Physical product group -> global macrocell owner, or -1. Candidate
    // trials can move already allocated groups even when their cell map
    // is unchanged, so this cannot be derived from cells alone.
    std::array<int, 32> product_group_owner;
    std::vector<unsigned> implicit_controls;
    // Signal-indexed OE dictionary key before physical output reordering.
    std::vector<int> oe_keys;
    std::array<bool, 2> sequential_search{};  // Spread search fell back to sequential cells.
    std::vector<unsigned> reverse_visit_costs;
    std::optional<std::array<int, 2>> last_oe_constraint;
    int failed_signal          = -1;
    bool failed_pad_assignment = false;
    std::vector<unsigned> partition_product_groups;
};

// Native first-success path. Unsupported resource/retry cases fail
// explicitly rather than silently choosing a different legal placement.
Mach111Placement place_mach111(
    const std::vector<Mach111PlaceSignal>& signals, Mach111PlaceOptions options = {},
    std::function<PlacementRouteFeedback(const Mach111Placement&, const std::vector<bool>&)> route = {},
    std::function<void(const Mach111Placement&, unsigned, const std::vector<bool>&)> failure       = {});
}  // namespace machxl
