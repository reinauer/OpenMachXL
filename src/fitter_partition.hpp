#pragma once
#include "fitter_order.hpp"
#include <array>
#include <functional>
#include <optional>
#include <string>

namespace machxl {

// Snapshot of the actual retained partition state, before physical fitting.
// Member indices refer to the partition input cluster vector.
struct PartitionObservation
{
    std::vector<std::vector<std::size_t>> members;
    std::vector<std::vector<std::uint16_t>> fanins;
    std::vector<unsigned> product_groups;
    std::vector<std::vector<std::string>> failure_reasons;
    std::vector<bool> asynchronous;
    std::vector<std::vector<std::size_t>> clock_members;
    std::vector<std::size_t> order;
    std::optional<std::size_t> failed_cluster;
    unsigned progress = 0;
    // Raw fixed input records follow fixed logic clusters during seeding.
    bool fixed_seed_complete = true;
};
using PartitionObserver = std::function<void(const PartitionObservation&)>;

// Explicit partition IR for the native single-group MACH111 path. Each
// cluster occupies one macrocell and one four-product group; a buried
// cluster does not occupy an output pin.
// Fanins include feedback/control inputs, but exclude dedicated clocks.
// These are partition signal IDs, not physical pin or CSM source numbers.
// OE, preset and product borrowing need additional cluster resources.
struct Mach111PartitionCluster
{
    PartitionOrderKey order;
    std::vector<std::uint16_t> fanins;
    std::optional<std::uint16_t> reset;
    bool output                         = true;
    int fixed_block                     = -1;
    unsigned product_groups             = 1;
    std::string oe_product              = {};
    std::optional<std::uint16_t> preset = {};
    int fixed_half                      = -1;  // Physical pin constraint within its block.
    // Modern global-row candidates include explicit constant controls.
    std::optional<std::uint16_t> global_reset  = {};
    std::optional<std::uint16_t> global_preset = {};
    bool macrocell                             = true;  // IPAIR input-register clusters only consume IO.
    // The cluster's pad also feeds a MACH4 input register. Such a cluster is
    // only exchanged with another one of its kind.
    bool input_register_pad                  = false;
    int global_clock                         = -1;
    bool clock_positive                      = true;
    bool local_clock                         = false;
    bool allow_global_product_clocks         = false;
    std::optional<std::uint16_t> clock_fanin = {};
    unsigned data_terms                      = 1;      // Constants count one; absent data counts zero.
    bool explicit_async_control              = false;  // Legacy215 swap precheck.
    bool force_async                         = false;
    bool force_sync                          = false;
    bool clocked_storage                     = false;
    // MACH215 materializes absent RE/PR as zero descriptors before ranking.
    bool implicit_storage_controls = false;
    // Partial-group flags: bit0 consumes the fixed-seed balance,
    // bit8 supplies it; bit12 enables deferred asynchronous sharing.
    unsigned sync_product_flags  = 0;
    unsigned async_product_flags = 0;
    // Modern asynchronous fallback has its own product descriptor; shared
    // control conflicts can select it after the initial synchronous rank.
    std::optional<unsigned> asynchronous_product_groups = std::nullopt;
};

// First-pass mode-zero keys for this IR. Keeps each order.signal_id;
// computes rank and summed fanout from all clusters' logical fanin sets.
// Other rank modes and independently specified preset/OE are not covered.
void rank_mach111_single_group(std::vector<Mach111PartitionCluster>& clusters);

// Common retry phases. Phase one weights actual data terms;
// later phases change fanin thresholds and the constant-control weights.
void rank_mach_modern_phase(std::vector<Mach111PartitionCluster>& clusters, unsigned phase,
                            bool product_pressure = false, bool retain_ground_controls = false);

// Preserve explicit controls; ground absent controls only on clocked
// non-COM macrocells. Return the descriptor changes (reset=1, preset=2).
std::vector<unsigned> ground_implicit_partition_controls(std::vector<Mach111PartitionCluster>& clusters);

// Return cluster indices in mutable block-list order. Reject retries needing unsupported
// partition strategies; this is not a physical pin placer.
std::array<std::vector<std::size_t>, 2> partition_mach111_single_group(
    const std::vector<Mach111PartitionCluster>& clusters, bool spread,
    const std::array<unsigned, 2>& output_capacity    = {16, 16},
    std::array<std::vector<std::string>, 2>* oe_order = nullptr);

}  // namespace machxl
