#pragma once
#include "device_family.hpp"
#include "fitter_family_place.hpp"
#include <functional>

namespace machxl {
struct ModernPlacement
{
    std::vector<int> cells, pins;
    // Signal-indexed input register IO ordinal; no output macrocell used.
    std::vector<int> input_registers;
    std::vector<std::size_t> global_order;
    std::vector<std::vector<std::size_t>> output_order;
    std::vector<int> product_group_owner;
    std::vector<int> product_aux_owner;
    std::vector<bool> product_aux_data;
    std::vector<bool> asynchronous_cells;
    std::vector<bool> product_clock_cells;
    // IO-indexed PIN_OMX selection, including final unused-pin matching.
    std::vector<int> output_muxes;
    // RESET/PRESET row identities after all partition mutations.
    std::vector<GlobalControlSlots> global_controls;
    // Signal-indexed implicit-control rewrites: reset=1, preset=2.
    std::vector<unsigned> implicit_controls;
    std::vector<unsigned> partition_product_groups;
    std::vector<unsigned> reverse_visit_costs;
};
using ModernPlacementCallback = std::function<PlacementRouteFeedback(const ModernPlacement&, const std::vector<bool>&)>;
using ModernPlacementFailureCallback = std::function<void(const ModernPlacement&, const std::vector<bool>&)>;
// Nodes and IO pins are separate resources; fixed pins constrain a circular
// output multiplexer. The callback runs with retained candidate frames live
// and can request continuation at a particular output signal.
ModernPlacement place_mach_modern(const MachDeviceLayout& layout, const std::vector<Mach111PlaceSignal>& signals,
                                  Mach111PlaceOptions options = {}, const ModernPlacementCallback& route_callback = {},
                                  const ModernPlacementFailureCallback& failure_callback = {});
// Final unused-IO matching. Input is eight slots per
// block, -1 for an IO without a used output. Buried cells remain eligible.
std::vector<int> complete_mach435_output_muxes(const std::vector<int>& used);
std::vector<int> complete_modern_output_muxes(const MachDeviceLayout& layout, const std::vector<int>& used);
}  // namespace machxl
