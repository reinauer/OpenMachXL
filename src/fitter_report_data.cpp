#include "fitter_report_data.hpp"
#include "device_family.hpp"
#include "frontend.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

namespace machxl {
void finalize_fitter_report_data(const Design& design, const FuseImage& image, FitterReportData& data)
{
    const auto& d = mach_device_layout(image.device);
    if (data.blocks.size() != d.blocks)
        data.blocks.resize(d.blocks);
    const unsigned ios = unsigned(d.io_pins.size()) / d.blocks;
    if (data.pins.empty())
        for (unsigned p = 1; p < image.pin_signals.size(); ++p) {
            const auto& name = image.pin_signals[p];
            if (name.empty() || name == "GND" || name == "VCC")
                continue;
            FitterReportPin pin;
            pin.name      = name;
            pin.number    = p;
            const auto at = std::find(d.io_pins.begin(), d.io_pins.end(), p);
            pin.block     = at == d.io_pins.end() ? -1 : int(unsigned(at - d.io_pins.begin()) / ios);
            for (const auto& node : data.nodes)
                if (node.pin == int(p)) {
                    if (node.input_register)
                        pin.input_node = node.number;
                    else {
                        pin.output      = true;
                        pin.output_node = node.number;
                    }
                }
            pin.input = !pin.output;
            data.pins.push_back(std::move(pin));
        }
    std::map<std::string, std::set<unsigned>> fanout;
    for (const auto& node : data.nodes)
        if (node.block >= 0 && !node.input_register)
            for (const auto& name : node.fanins)
                fanout[name].insert(unsigned(node.block));
    for (auto& node : data.nodes)
        node.fanout_blocks.assign(fanout[node.name].begin(), fanout[node.name].end());
    for (auto& pin : data.pins) {
        pin.fanout_blocks.assign(fanout[pin.name].begin(), fanout[pin.name].end());
        for (const auto& node : data.nodes)
            if (node.pin == int(pin.number) && node.implied && node.logic != 'C' && !node.explicit_output_pair)
                pin.fanout_blocks.clear();
        for (unsigned b = 0; b < data.blocks.size(); ++b)
            for (const auto& clock : data.blocks[b].clocks)
                if (clock.pin == int(pin.number) &&
                    std::find(pin.clock_blocks.begin(), pin.clock_blocks.end(), b) == pin.clock_blocks.end())
                    pin.clock_blocks.push_back(b);
    }
    if (data.optimized_design) {
        std::map<std::string, const Pin*> declarations;
        for (const auto& pin : data.optimized_design->pins)
            declarations[pin.name] = &pin;
        for (auto& pin : data.pins) {
            const auto found = declarations.find(pin.name);
            pin.fixed        = found != declarations.end() && !found->second->node && found->second->number;
        }
        for (auto& node : data.nodes) {
            const auto found = declarations.find(node.name);
            node.fixed       = found != declarations.end() && found->second->node && found->second->number;
            if (node.input_register)
                for (const auto& pin : data.pins)
                    if (int(pin.number) == node.pin)
                        node.fixed = pin.fixed;
        }
    }
    for (unsigned b = 0; b < d.blocks; ++b) {
        auto& block           = data.blocks[b];
        block.io_pins         = 0;
        block.input_registers = 0;
        block.macrocells      = 0;
        if (!d.one_hot)
            for (const auto& node : data.nodes)
                if (node.input_register && node.block == int(b))
                    for (auto& pin : data.pins)
                        if (node.pin > 0 && pin.number == unsigned(node.pin)) {
                            pin.input      = false;
                            pin.output     = false;
                            pin.input_node = pin.output_node = node.number;
                        }
        std::set<int> cells;
        for (const auto& pin : data.pins)
            if (pin.block == int(b) && (d.one_hot || pin.input_node < 0))
                ++block.io_pins;
        for (const auto& node : data.nodes)
            if (node.block == int(b)) {
                if (node.input_register)
                    ++block.input_registers;
                if (node.cell >= 0 && (!node.input_register || (!d.one_hot && d.device != "MACH215")))
                    cells.insert(node.cell);
            }
        block.macrocells = unsigned(cells.size());
        block.fanin      = unsigned(block.routes.size());
        if (!data.group_owner.empty()) {
            block.clusters = 0;
            for (unsigned c = 0; c < d.cells; ++c)
                if (data.group_owner.at(b * d.cells + c) >= 0)
                    ++block.clusters;
            const unsigned capacity = d.device == "MACH215" ? d.cells / 2 : d.cells;
            block.free_clusters     = capacity - block.clusters;
            block.available_terms.resize(d.cells);
            unsigned unusable = 0, free_multi = 0, free_single = 0;
            for (unsigned c = 0; c < d.cells; ++c) {
                if (d.device == "MACH215" && (c % 2)) {
                    block.available_terms[c] = 0;
                    continue;
                }
                const int target   = int(b * d.cells + c);
                unsigned available = 0;
                const auto offsets = d.device == "MACH215" ? std::vector<int>{0, 2, -2}
                                     : d.device[4] == '1'  ? std::vector<int>{0, 1, -1}
                                                           : std::vector<int>{0, 1, 2, -1};
                for (int delta : offsets) {
                    const int local = int(c) + delta;
                    if (local < 0 || local >= int(d.cells))
                        continue;
                    if ((d.device == "MACH110" || d.device == "MACH111") &&
                        local / int(d.cells / 2) != int(c) / int(d.cells / 2))
                        continue;
                    const auto group       = b * d.cells + unsigned(local);
                    const int owner        = data.group_owner[group];
                    unsigned main_capacity = 4;
                    if (d.one_hot)
                        for (const auto& node : data.nodes)
                            if (!node.input_register && node.block == int(b) && node.cell == local && node.asynchronous)
                                main_capacity = 2;
                    if (owner < 0 || owner == target)
                        available += main_capacity;
                    if (d.one_hot && !data.auxiliary_owner.empty() &&
                        ((data.auxiliary_owner[group] < 0 &&
                          ((int(group) == target && !cells.count(int(c))) || owner < 0 || owner == target)) ||
                         data.auxiliary_owner[group] == target))
                        ++available;
                }
                unsigned used = 0;
                for (auto& node : data.nodes)
                    if (!node.input_register && node.block == int(b) && node.cell == int(c))
                        used = std::max(used, node.product_terms + node.xor_terms);
                available                = available >= used ? available - used : 0;
                block.available_terms[c] = available;
                for (auto& node : data.nodes)
                    if (!node.input_register && node.block == int(b) && node.cell == int(c))
                        node.available_terms = available;
                if (!cells.count(int(c))) {
                    if (!available)
                        ++unusable;
                    else if (d.one_hot && available == 1)
                        ++free_single;
                    else
                        ++free_multi;
                }
            }
            block.unusable        = unusable;
            block.free_macrocells = free_multi;
            block.free_single_pt  = free_single;
            if (!d.one_hot)
                for (auto& node : data.nodes)
                    if (node.input_register && node.block == int(b) && node.cell >= 0)
                        node.available_terms = block.available_terms.at(unsigned(node.cell)) + 1;
        }
    }
    // Completion is set only by the accepted physical backend frame.
    if (data.successful) {
        data.partitioned = data.partition_total = unsigned(data.nodes.size());
        data.placed = data.placement_total = unsigned(data.nodes.size() + data.pins.size());
        data.routed                        = 0;
        for (const auto& b : data.blocks)
            data.routed += unsigned(b.routes.size());
        data.routing_total = data.routed;
    }
    (void)design;
    data.complete = true;
}
}  // namespace machxl
