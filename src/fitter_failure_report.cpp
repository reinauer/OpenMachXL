#include "fitter_failure_report.hpp"
#include "device_family.hpp"
#include "frontend.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace machxl {
void capture_fitter_partition_state(FitterReportData& report, const std::vector<Mach111PlaceSignal>& signals,
                                    const std::vector<std::size_t>& cluster_signals,
                                    const PartitionObservation& partition, const std::map<std::string, unsigned>& ids,
                                    const std::map<std::string, unsigned>& feedback_ids)
{
    if (!report.optimized_design)
        throw std::logic_error("Partition report requires captured design");
    const auto& design = *report.optimized_design;
    const auto& d      = mach_device_layout(design.device);
    const unsigned ios = unsigned(d.io_pins.size()) / d.blocks;
    FitterReportData result;
    result.optimized_pla    = report.optimized_pla;
    result.optimized_design = report.optimized_design;
    result.blocks.resize(d.blocks);
    std::map<std::string, std::string> original_names;
    for (const auto& alias : pla_signal_aliases(design))
        original_names[alias.second] = alias.first;
    struct Column
    {
        std::string name, kind;
        unsigned terms = 0;
        std::set<std::string> fanins;
        bool negative                     = false;
        std::vector<std::string> products = {};
    };
    std::vector<std::string> inputs;
    std::vector<Column> columns;
    std::istringstream file(report.optimized_pla);
    std::string line, labels;
    bool products = false;
    while (std::getline(file, line)) {
        std::istringstream fields(line);
        std::string token;
        if (!(fields >> token) || token[0] == '#')
            continue;
        if (token[0] == '.') {
            labels.clear();
            if (token == ".ilb" || token == ".ob")
                labels = token;
            else if (token == ".p")
                products = true;
            else if (token == ".e")
                break;
            if (labels.empty())
                continue;
        } else if (products) {
            std::string pattern = token, mask;
            fields >> mask;
            for (unsigned c = 0; c < columns.size(); ++c)
                if (c < mask.size() && mask[c] == '1') {
                    ++columns[c].terms;
                    columns[c].products.push_back(pattern);
                    for (unsigned i = 0; i < inputs.size(); ++i)
                        if (i < pattern.size() && pattern[i] != '-')
                            columns[c].fanins.insert(inputs[i]);
                }
            continue;
        } else
            fields.seekg(0);
        while (fields >> token) {
            if (labels == ".ilb")
                inputs.push_back(original_names.at(token));
            else if (labels == ".ob") {
                const bool negative = token.find('-') != std::string::npos;
                while (!token.empty() && (token.back() == '%' || token.back() == '-'))
                    token.pop_back();
                const auto dot  = token.find('.');
                const auto name = token.substr(0, dot);
                columns.push_back({original_names.at(name),
                                   dot == std::string::npos ? "COM" : token.substr(dot + 1),
                                   0,
                                   {},
                                   negative});
            }
        }
    }
    std::map<std::string, const Pin*> declarations;
    for (const auto& pin : design.pins)
        declarations[pin.name] = &pin;
    std::vector<int> signal_blocks(signals.size(), -1);
    for (unsigned b = 0; b < partition.members.size(); ++b) {
        auto& block              = result.blocks[b];
        block.fanin              = unsigned(partition.fanins[b].size());
        block.partition_clusters = partition.product_groups[b];
        block.clusters           = partition.product_groups[b];
        // MACH215 has one product-term cluster per two cells, as in the
        // capacity the report prints for a fitted block.
        const unsigned capacity = d.device == "MACH215" ? d.cells / 2 : d.cells;
        block.free_clusters     = capacity > partition.product_groups[b] ? capacity - partition.product_groups[b] : 0u;
        for (unsigned cell = 0; cell < d.cells; ++cell) {
            unsigned available = 0;
            const auto offsets = d.device == "MACH215" ? std::vector<int>{0, 2, -2}
                                 : d.device[4] == '1'  ? std::vector<int>{0, 1, -1}
                                                       : std::vector<int>{0, 1, 2, -1};
            for (int offset : offsets) {
                const int target = int(cell) + offset;
                if (target < 0 || target >= int(d.cells))
                    continue;
                if ((d.device == "MACH110" || d.device == "MACH111") &&
                    target / int(d.cells / 2) != int(cell) / int(d.cells / 2))
                    continue;
                if (d.device != "MACH215" || cell % 2 == 0)
                    available += d.one_hot ? 5 : 4;
            }
            block.available_terms.push_back(available);
        }
        for (auto cluster : partition.members[b])
            signal_blocks.at(cluster_signals.at(cluster)) = int(b);
    }
    const auto block_fanout = [&](std::uint16_t serial) {
        std::set<int> destinations;
        for (unsigned j = 0; j < signals.size(); ++j)
            if (signal_blocks[j] >= 0 &&
                std::find(signals[j].fanins.begin(), signals[j].fanins.end(), serial) != signals[j].fanins.end())
                destinations.insert(signal_blocks[j]);
        return std::int16_t(destinations.size());
    };
    std::vector<FitterOrderKey> order_keys, pad_order_keys;
    for (unsigned i = 0; i < signals.size(); ++i) {
        auto key  = signals[i].order;
        key.block = std::int16_t(signal_blocks[i]);
        pad_order_keys.push_back(key);
        if (d.one_hot && partition.failed_cluster) {
            pad_order_keys.back().fanout_count = block_fanout(std::uint16_t(key.serial));
            // Emit unplaced nodes in logical import order. Registered pads use
            // separate feedback records, ranked by distinct block fanout, products
            // and serial ID rather than the pad's empty fanout.
            if (feedback_ids.count(signals[i].name)) {
                key.type      = 3;
                key.serial    = std::int16_t(feedback_ids.at(signals[i].name));
                key.fixed_pin = -1;
            }
            key.fanout_count = block_fanout(std::uint16_t(key.serial));
        }
        order_keys.push_back(key);
    }
    const auto report_order = fitter_signal_order(order_keys, false);
    std::vector<unsigned> traversal(signals.size());
    for (unsigned i = 0; i < report_order.size(); ++i)
        traversal[report_order[i]] = i;
    const auto pad_order = fitter_signal_order(pad_order_keys, false);
    std::vector<unsigned> pad_traversal(signals.size());
    for (unsigned i = 0; i < pad_order.size(); ++i)
        pad_traversal[pad_order[i]] = i;
    for (unsigned i = 0; i < signals.size(); ++i) {
        const auto& signal      = signals[i];
        const auto* declaration = declarations.at(signal.name);
        if (signal.output || signal.input_register) {
            FitterReportNode node;
            node.name            = signal.name;
            node.block           = signal_blocks[i];
            node.traversal_order = traversal[i];
            node.pin             = -1;
            node.paired          = !signal.buried;
            node.input_register  = signal.input_register;
            node.implied         = !signal.buried;
            node.logic           = signal.clocked_storage ? 'D' : 'C';
            const auto cluster   = std::find(cluster_signals.begin(), cluster_signals.end(), i);
            if (cluster != cluster_signals.end())
                node.asynchronous = partition.asynchronous.at(unsigned(cluster - cluster_signals.begin()));
            for (const auto& column : columns)
                if (column.name == signal.name) {
                    if (!signal.clocked_storage && (column.kind == "C" || column.kind == "RE" || column.kind == "PR"))
                        continue;
                    const bool data     = column.kind == "COM" || column.kind == "REG" || column.kind == "T" ||
                                          column.kind == "G" || column.kind.compare(0, 4, "XORR") == 0;
                    const bool xor_left = column.kind.compare(0, 4, "XORL") == 0;
                    if (data) {
                        node.product_terms = column.terms;
                        node.negative      = !declaration->node && (column.negative != declaration->inverted);
                        if (column.kind == "T")
                            node.logic = 'T';
                        else if (column.kind == "G")
                            node.logic = 'L';
                    }
                    if (xor_left)
                        node.xor_terms = column.terms;
                    if (column.kind == "OE" || column.kind == "RE" || column.kind == "PR") {
                        std::string key;
                        for (const auto& product : column.products)
                            if (product.find_first_not_of('-') != std::string::npos)
                                key += product + ";";
                        if (column.kind == "OE")
                            node.oe_expression = key;
                        else if (column.kind == "RE")
                            node.reset = key;
                        else
                            node.preset = key;
                    }
                    if (column.kind != "C" || signal.local_clock)
                        for (const auto& name : column.fanins)
                            if (std::find(node.fanins.begin(), node.fanins.end(), name) == node.fanins.end())
                                node.fanins.push_back(name);
                    if (data || xor_left)
                        for (const auto& name : column.fanins)
                            if (std::find(node.data_fanins.begin(), node.data_fanins.end(), name) ==
                                node.data_fanins.end())
                                node.data_fanins.push_back(name);
                }
            if (node.block >= 0) {
                auto& block = result.blocks[unsigned(node.block)];
                if (node.input_register)
                    ++block.input_registers;
                else {
                    ++block.macrocells;
                    block.product_terms += node.product_terms + node.xor_terms;
                }
            }
            result.nodes.push_back(std::move(node));
        }
        if (!declaration->node) {
            FitterReportPin pin;
            pin.name   = signal.name;
            pin.number = declaration->number;
            if (signal.order.type == 9 && signal.assigned_clock_pin)
                pin.number = signal.assigned_clock_pin;
            pin.output          = signal.output;
            pin.input           = !signal.output;
            pin.dedicated_clock = signal.order.type == 9;
            pin.traversal_order = pad_traversal[i];
            if (pin.output) {
                pin.number        = 0;
                pin.block         = signal_blocks[i];
                pin.output_signal = signal.name;
            } else {
                const auto at = std::find(d.io_pins.begin(), d.io_pins.end(), pin.number);
                if (partition.fixed_seed_complete && at != d.io_pins.end())
                    pin.block = int(unsigned(at - d.io_pins.begin()) / ios);
            }
            if (pin.block >= 0)
                ++result.blocks[unsigned(pin.block)].io_pins;
            result.pins.push_back(std::move(pin));
        }
    }
    std::map<std::string, std::set<unsigned>> fanout;
    for (const auto& node : result.nodes)
        if (node.block >= 0)
            for (const auto& name : node.fanins)
                fanout[name].insert(unsigned(node.block));
    for (auto& node : result.nodes)
        node.fanout_blocks.assign(fanout[node.name].begin(), fanout[node.name].end());
    for (auto& pin : result.pins) {
        pin.fanout_blocks.assign(fanout[pin.name].begin(), fanout[pin.name].end());
        for (const auto& node : result.nodes)
            if (node.name == pin.output_signal && node.implied && node.logic != 'C')
                pin.fanout_blocks.clear();
    }
    std::map<unsigned, std::string> names;
    for (const auto& id : ids)
        names[id.second] = id.first;
    for (const auto& id : feedback_ids)
        names[id.second] = id.first;
    for (unsigned b = 0; b < partition.members.size(); ++b) {
        auto& block = result.blocks[b];
        for (auto id : partition.fanins[b]) {
            FitterReportRoute route;
            route.signal        = names.at(id);
            route.logical_id    = id;
            const auto node     = std::find_if(result.nodes.begin(), result.nodes.end(),
                                               [&](const auto& n) { return n.name == route.signal; });
            const auto feedback = feedback_ids.find(route.signal);
            const bool raw_pad  = node != result.nodes.end() && node->implied && node->logic != 'C' &&
                                  feedback != feedback_ids.end() && feedback->second != id;
            if (node != result.nodes.end() && !raw_pad) {
                route.source_block = node->block;
                route.source_cell  = -1;
            } else
                route.source_pin = int(declarations.at(route.signal)->number);
            block.routes.push_back(std::move(route));
        }
        std::set<int> clocks;
        for (auto cluster : partition.clock_members[b]) {
            const auto& user = signals.at(cluster_signals.at(cluster));
            if (d.one_hot && partition.failed_cluster && partition.asynchronous.at(cluster))
                continue;
            if (user.global_clock < 0 || !clocks.insert(user.global_clock).second)
                continue;
            const auto pin    = d.clock_pins.at(unsigned(user.global_clock));
            const auto source = std::find_if(signals.begin(), signals.end(), [&](const auto& s) {
                return s.assigned_clock_pin == pin || s.order.fixed_pin == int(pin) - 1;
            });
            if (source != signals.end())
                block.clocks.push_back({source->name, -1, int(pin), !user.clock_positive});
        }
    }
    if (partition.failed_cluster) {
        FitterReportFailure failure;
        failure.signal = signals.at(cluster_signals.at(*partition.failed_cluster)).name;
        if (!partition.fixed_seed_complete) {
            const auto& failed = signals.at(cluster_signals.at(*partition.failed_cluster));
            if (failed.order.block >= 0)
                failure.fixed_block = failed.order.block;
            else if (failed.fixed_cell >= 0)
                failure.fixed_block = failed.fixed_cell / int(d.cells);
            else {
                const auto pin = std::find(d.io_pins.begin(), d.io_pins.end(), unsigned(failed.order.fixed_pin + 1));
                if (pin != d.io_pins.end())
                    failure.fixed_block = int(unsigned(pin - d.io_pins.begin()) / ios);
            }
        }
        failure.block_reasons = partition.failure_reasons;
        for (unsigned i = 0; i < signals.size(); ++i)
            if ((signals[i].output || signals[i].input_register) && signal_blocks[i] < 0)
                failure.remaining_signals.push_back(signals[i].name);
        std::sort(failure.remaining_signals.begin(), failure.remaining_signals.end());
        result.partition_failure = std::move(failure);
    }
    // Count one logical record per signal plus each registered output
    // feedback alias. Only records assigned to a block advance the numerator;
    // input-only pins stay outside block lists. The denominator excludes
    // dedicated clocks but includes input-only pins.
    result.partition_total = unsigned(signals.size() + feedback_ids.size());
    for (unsigned i = 0; i < signals.size(); ++i) {
        const auto& signal = signals[i];
        if (signal.order.type == 9) {
            --result.partition_total;
            continue;
        }
        if (signal_blocks[i] >= 0)
            result.partitioned += 1u + unsigned(feedback_ids.count(signal.name));
        else if (!signal.output && !signal.input_register) {
            const auto pin = std::find_if(result.pins.begin(), result.pins.end(),
                                          [&](const auto& p) { return p.name == signal.name; });
            if (pin != result.pins.end() && pin->block >= 0)
                ++result.partitioned;
        }
    }
    result.complete = true;
    report          = std::move(result);
}
void capture_partition_failure_report(FitterReportData& report, const std::vector<Mach111PlaceSignal>& signals,
                                      const std::vector<std::size_t>& cluster_signals,
                                      const PartitionObservation& partition, const std::map<std::string, unsigned>& ids,
                                      const std::map<std::string, unsigned>& feedback_ids)
{
    if (partition.failed_cluster)
        capture_fitter_partition_state(report, signals, cluster_signals, partition, ids, feedback_ids);
}
}  // namespace machxl
