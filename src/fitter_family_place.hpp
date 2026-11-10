#pragma once
#include "device_family.hpp"
#include "fitter_place.hpp"
#include <array>
namespace machxl {
using GlobalControlSlots = std::array<std::optional<std::uint16_t>, 2>;
struct FamilyPlacement
{
    std::vector<int> cells, pins;
    std::vector<int> oe_slots;
    std::vector<std::array<std::string, 4>> oe_products;
    std::vector<int> product_group_owner;
    std::vector<std::size_t> global_order;
    std::vector<std::vector<std::size_t>> output_order;
    std::vector<unsigned> implicit_controls;
    // Signal-indexed OE dictionary key before physical output reordering.
    std::vector<int> oe_keys;
    std::vector<bool> sequential_search;  // Spread search fell back to sequential cells.
    std::vector<unsigned> reverse_visit_costs;
    std::optional<std::array<int, 2>> last_oe_constraint;
    int failed_signal          = -1;
    bool failed_pad_assignment = false;
    std::vector<unsigned> partition_product_groups;
    // Physical import walk: final partition cluster order,
    // pad then logic/input-register node, followed by ordinary inputs.
    std::vector<std::size_t> import_order;
};
// Native partition, rescue and failed-cluster rank promotion. Output
// capacity is independent of cell/group capacity, allowing buried clusters.
std::vector<std::vector<std::size_t>> partition_mach_family_first_success(
    const MachDeviceLayout& layout, const std::vector<Mach111PartitionCluster>& clusters,
    const std::vector<unsigned>& output_capacity, bool balanced,
    std::vector<GlobalControlSlots>* global_controls = nullptr, std::vector<std::size_t>* final_cluster_order = nullptr,
    std::vector<bool>* product_clocks = nullptr, std::vector<unsigned>* implicit_controls = nullptr,
    PartitionIterationState* iteration = nullptr, std::vector<std::vector<std::string>>* oe_order = nullptr,
    std::vector<unsigned>* used_product_groups = nullptr, const PartitionObserver& observer = {});

// Original MACH1/2 partition/place path, including adjacent-group borrowing.
FamilyPlacement place_mach_family(
    const MachDeviceLayout& layout, const std::vector<Mach111PlaceSignal>& signals, Mach111PlaceOptions options = {},
    std::function<PlacementRouteFeedback(const FamilyPlacement&, const std::vector<bool>&)> route = {},
    std::function<void(const FamilyPlacement&, unsigned, const std::vector<bool>&)> failure       = {});
}  // namespace machxl
