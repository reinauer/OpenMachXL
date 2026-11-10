#include "fitter_legacy_failure_report.hpp"
#include "fitter_budget.hpp"
#include "fitter_failure_report.hpp"
#include "fitter_metadata.hpp"
#include <algorithm>
#include <set>

namespace machxl {
namespace {
template <class Placement>
void capture(FitterReportData& report, const std::vector<Mach111PlaceSignal>& signals, const Placement& frame,
             unsigned failed_block, const std::vector<bool>& assigned, const std::map<std::string, unsigned>& ids,
             const std::map<std::string, unsigned>& feedback_ids)
{
    const auto& d                 = mach_device_layout(report.optimized_design->device);
    const unsigned ios            = unsigned(d.io_pins.size()) / d.blocks;
    const auto physically_located = [&](unsigned i) {
        if (!signals[i].input_register)
            return assigned[i] && frame.cells[i] >= 0;
        const auto pad = signals[i].paired_input;
        if (pad < 0 || unsigned(pad) >= signals.size() || frame.pins[unsigned(pad)] <= 0)
            return false;
        const auto pin = std::find(d.io_pins.begin(), d.io_pins.end(), unsigned(frame.pins[unsigned(pad)]));
        return pin != d.io_pins.end() && frame.cells[i] == int(2 * unsigned(pin - d.io_pins.begin()) + 1);
    };
    PartitionObservation state;
    state.members.resize(d.blocks);
    state.fanins.resize(d.blocks);
    state.product_groups.resize(d.blocks);
    state.clock_members.resize(d.blocks);
    std::vector<std::size_t> mapping;
    for (unsigned i = 0; i < signals.size(); ++i)
        if (signals[i].output || signals[i].input_register) {
            const unsigned cluster = unsigned(mapping.size());
            mapping.push_back(i);
            if (frame.cells[i] < 0)
                continue;
            const auto b = unsigned(frame.cells[i]) / d.cells;
            state.members.at(b).push_back(cluster);
            ++state.progress;
            if (!signals[i].input_register)
                state.product_groups[b] += unsigned((signals[i].order.product_terms + 3) / 4);
            for (auto input : signals[i].fanins)
                if (std::find(state.fanins[b].begin(), state.fanins[b].end(), input) == state.fanins[b].end())
                    state.fanins[b].push_back(input);
        }
    if (frame.partition_product_groups.size() == d.blocks)
        state.product_groups = frame.partition_product_groups;
    state.asynchronous.resize(mapping.size());
    capture_fitter_partition_state(report, signals, mapping, state, ids, feedback_ids);
    report.placement_attempts = fitting_logical_visits();
    report.routing_attempts   = 0;
    report.group_owner.assign(frame.product_group_owner.begin(), frame.product_group_owner.end());
    report.placement_failed_blocks.push_back(failed_block);
    if (frame.failed_signal >= 0) {
        const auto& signal = signals.at(unsigned(frame.failed_signal));
        report.cell_placement_failure =
            FitterReportCellFailure{signal.name, failed_block, unsigned(signal.order.product_terms)};
    }
    report.last_oe_constraint = frame.last_oe_constraint;
    for (unsigned b = 0; b < d.blocks; ++b)
        report.blocks[b].sequential_search = frame.sequential_search[b];
    std::map<std::string, unsigned> index;
    for (unsigned i = 0; i < signals.size(); ++i)
        index[signals[i].name] = i;
    // Inputs and already fixed pads take precedence over an output whose
    // candidate cell maps to the same pin. Keep that output visibly unplaced.
    std::set<unsigned> occupied;
    std::map<std::string, unsigned> assigned_pins;
    for (bool outputs : {false, true})
        for (unsigned i = 0; i < signals.size(); ++i)
            if (signals[i].output == outputs && !signals[i].buried && !signals[i].input_register && frame.pins[i] > 0) {
                if (outputs && signals[i].order.fixed_pin < 0) {
                    if (!assigned[i])
                        continue;
                    if (!frame.failed_pad_assignment || frame.cells[i] < 0 ||
                        unsigned(frame.cells[i]) / d.cells >= failed_block)
                        continue;
                }
                const auto pin = outputs && signals[i].order.fixed_pin >= 0 ? unsigned(signals[i].order.fixed_pin + 1)
                                                                            : unsigned(frame.pins[i]);
                if (occupied.insert(pin).second)
                    assigned_pins[signals[i].name] = pin;
            }
    for (auto& pin : report.pins) {
        pin.number    = assigned_pins.count(pin.name) ? assigned_pins.at(pin.name) : 0;
        const auto io = std::find(d.io_pins.begin(), d.io_pins.end(), pin.number);
        if (pin.number)
            pin.block = io == d.io_pins.end() ? -1 : int(unsigned(io - d.io_pins.begin()) / ios);
        pin.output_node = pin.input_node = -1;
    }
    for (auto& node : report.nodes) {
        const auto i       = index.at(node.name);
        const auto& signal = signals[i];
        const bool located = physically_located(i);
        if (located) {
            const auto cell = unsigned(frame.cells[i]);
            node.block      = int(cell / d.cells);
            node.cell       = int(cell % d.cells);
            node.number     = int(cell + 2);
        } else {
            node.cell   = -1;
            node.number = -1;
        }
        auto pad =
            node.input_register && signal.paired_input >= 0 ? signals[unsigned(signal.paired_input)].name : node.name;
        node.pin = assigned_pins.count(pad) ? int(assigned_pins.at(pad)) : -1;
        if (!node.input_register &&
            (!frame.failed_pad_assignment || node.block < 0 || unsigned(node.block) >= failed_block))
            node.pin = -1;
        node.paired = node.pin >= 0;
        if (!signal.oe_product.empty())
            node.oe_expression = signal.oe_product + ";";
        if (located && i < frame.oe_slots.size())
            node.oe_slot = frame.oe_slots[i];
        if (located && i < frame.oe_keys.size())
            node.oe_key = frame.oe_keys[i];
        for (auto& pin : report.pins)
            if ((node.pin > 0 && int(pin.number) == node.pin) ||
                (!node.input_register && pin.output_signal == node.name)) {
                if (node.input_register)
                    pin.input_node = node.number;
                else
                    pin.output_node = node.number;
            }
        for (const auto& pin : report.optimized_design->pins)
            if (pin.node && pin.pair == node.name) {
                node.name    = pin.name;
                node.implied = false;
                break;
            }
    }
    // A block can already have a known dedicated clock source while its
    // macrocell clock-selector assignment has not yet been assembled.
    for (const auto& node : report.nodes)
        if (node.block >= 0 && node.logic != 'C') {
            auto owner = index.find(node.name);
            if (owner == index.end())
                for (const auto& pin : report.optimized_design->pins)
                    if (pin.name == node.name && !pin.pair.empty())
                        owner = index.find(pin.pair);
            if (owner == index.end())
                continue;
            const auto& signal = signals[owner->second];
            if (!signal.clock_fanin)
                continue;
            const auto source =
                std::find_if(ids.begin(), ids.end(), [&](const auto& id) { return id.second == *signal.clock_fanin; });
            if (source == ids.end())
                continue;
            const auto pad = assigned_pins.find(source->first);
            if (pad == assigned_pins.end())
                continue;
            const auto clock = std::find(d.clock_pins.begin(), d.clock_pins.end(), pad->second);
            if (clock == d.clock_pins.end())
                continue;
            auto& clocks = report.blocks[unsigned(node.block)].clocks;
            if (std::none_of(clocks.begin(), clocks.end(),
                             [&](const auto& entry) { return entry.pin == int(pad->second); }))
                clocks.push_back({source->first, -1, int(pad->second), !signal.clock_positive});
        }
    FuseImage image;
    image.device    = d.device;
    image.pin_count = d.pins;
    Design placed   = *report.optimized_design;
    std::set<std::string> active;
    for (auto& pin : placed.pins) {
        if (pin.node)
            continue;
        const auto at = assigned_pins.find(pin.name);
        if (at != assigned_pins.end()) {
            pin.number = at->second;
            active.insert(pin.name);
        }
    }
    apply_placement_metadata(image, placed, active);
    finalize_fitter_report_data(placed, image, report);
    // This search stage completes macrocell assignment before assigning pads.
    // DOS reports that macrocell progress even if the subsequent IO assignment
    // fails; unresolved output pads remain explicit zero-location records.
    report.partitioned = report.partition_total = unsigned(mapping.size());
    report.placement_total                      = unsigned(report.nodes.size());
    report.placed                               = 0;
    for (const auto& node : report.nodes)
        if (node.cell >= 0)
            ++report.placed;
    for (const auto& pin : report.pins)
        if (!pin.output && pin.input_node < 0) {
            ++report.placement_total;
            if (pin.number)
                ++report.placed;
        }
    for (unsigned b = 0; b < d.blocks; ++b) {
        unsigned required = 0;
        for (const auto& node : report.nodes)
            if (node.block == int(b) && !node.input_register)
                ++required;
        report.blocks[b].required_macrocells = required;
        report.blocks[b].io_pins             = 0;
        for (const auto& pin : report.pins)
            if (pin.block == int(b) && pin.input_node < 0)
                ++report.blocks[b].io_pins;
        for (auto& route : report.blocks[b].routes) {
            route.mux = -1;
            // Preserve the partition record's raw-pad versus feedback identity,
            // but replace declaration locations with this retained physical frame.
            if (route.source_pin >= 0) {
                const auto pin   = assigned_pins.find(route.signal);
                route.source_pin = pin == assigned_pins.end() ? -1 : int(pin->second);
            } else {
                const auto i       = index.at(route.signal);
                const bool located = physically_located(i);
                if (located && frame.cells[i] >= 0) {
                    route.source_block = frame.cells[i] / int(d.cells);
                    route.source_cell  = frame.cells[i] % int(d.cells);
                } else
                    route.source_block = route.source_cell = -1;
            }
        }
        report.routing_total += unsigned(report.blocks[b].routes.size());
    }
    report.retained_image = std::make_shared<FuseImage>(std::move(image));
}
}  // namespace
void capture_legacy_placement_failure(FitterReportData& report, const std::vector<Mach111PlaceSignal>& signals,
                                      const Mach111Placement& frame, unsigned block, const std::vector<bool>& assigned,
                                      const std::map<std::string, unsigned>& ids,
                                      const std::map<std::string, unsigned>& feedback_ids)
{
    capture(report, signals, frame, block, assigned, ids, feedback_ids);
}
void capture_legacy_placement_failure(FitterReportData& report, const std::vector<Mach111PlaceSignal>& signals,
                                      const FamilyPlacement& frame, unsigned block, const std::vector<bool>& assigned,
                                      const std::map<std::string, unsigned>& ids,
                                      const std::map<std::string, unsigned>& feedback_ids)
{
    capture(report, signals, frame, block, assigned, ids, feedback_ids);
}
}  // namespace machxl
