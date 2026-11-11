#include "fitter_reports.hpp"
#include "banner.hpp"
#include "device_family.hpp"
#include "frontend.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace machxl {
namespace {
template <class... A>
std::string format_fields(const char* format, A... args)
{
    const int count = std::snprintf(nullptr, 0, format, args...);
    if (count < 0)
        throw std::runtime_error("Fitter report formatting failed");
    std::vector<char> buffer(std::size_t(count) + 1);
    std::snprintf(buffer.data(), buffer.size(), format, args...);
    return std::string(buffer.data(), std::size_t(count));
}
std::string format_number(std::optional<unsigned> n)
{
    return n ? std::to_string(*n) : "..";
}
std::string format_number(int n, const char* empty = "..")
{
    return n < 0 ? empty : std::to_string(n);
}
std::string format_text(const std::string& s, const char* empty = ".")
{
    return s.empty() ? empty : s;
}
std::string format_fanout(unsigned count, const std::vector<unsigned>& blocks, char empty = '-')
{
    std::string result(count, empty);
    for (auto block : blocks)
        if (block < count)
            result[block] = char('A' + block);
    return result;
}
std::string report_heading(const std::string& title)
{
    return std::string(title.size() + 4, '*') + "\n* " + title + " *\n" + std::string(title.size() + 4, '*') + "\n";
}
std::string pin_type(const FitterReportPin& pin)
{
    if (!pin.input && !pin.output)
        return "..";
    if (pin.input && pin.output)
        return "bidir";
    return pin.output ? "output" : "input";
}
std::string node_type(const FitterReportNode& node)
{
    return node.input_register ? "ipair" : node.implied ? "implied" : node.paired ? "opair" : "buried";
}
std::string logic_type(const FitterReportNode& node, bool modern)
{
    if (!modern)
        return " " + std::string(1, node.logic) + " ";
    return std::string(1, node.logic) + "/" + (node.logic == 'C' ? "." : node.asynchronous ? "A" : "S");
}
}  // namespace
std::string render_fitter_report(const std::string& device, const FitterReportData& input_data,
                                 const CompilerSettings& settings, const std::string& source_filename,
                                 const std::string& timestamp)
{
    auto data = input_data;
    if (!data.complete)
        throw std::runtime_error("Fitter report metadata is incomplete");
    const auto& layout = mach_device_layout(device);
    if (data.blocks.size() != layout.blocks)
        throw std::runtime_error("Fitter report block count mismatch");
    const bool modern = layout.one_hot, dedicated_ir = device[4] == '4' || device == "MACH215";
    const bool has_ir    = device[4] == '2' || device[4] == '4';
    const bool has_power = (!modern && layout.electrical_count) || device == "MACH446";
    // Legacy failed search frames precede power-bit assembly and omit
    // the unobserved power column.
    const bool power_column    = has_power && (modern || data.successful);
    const bool separate_clocks = device == "MACH465";
    const unsigned cells = layout.cells, blocks = layout.blocks, ios = unsigned(layout.io_pins.size()) / blocks;
    const unsigned pts_per_cluster = modern ? 5 : 4;
    const unsigned logic_cells     = device == "MACH215" ? cells / 2 : cells;
    std::map<std::string, unsigned> logical_order, route_order;
    if (data.optimized_design) {
        const auto aliases = pla_signal_aliases(*data.optimized_design);
        const auto display = [&](std::string& name) {
            const auto found = aliases.find(name);
            if (found != aliases.end())
                name = found->second;
        };
        for (auto& node : data.nodes) {
            display(node.name);
            for (auto& name : node.fanins)
                display(name);
        }
        for (auto& pin : data.pins) {
            display(pin.name);
            if (!pin.output_signal.empty())
                display(pin.output_signal);
        }
        for (auto& block : data.blocks) {
            for (auto& route : block.routes)
                display(route.signal);
            for (auto& clock : block.clocks)
                display(clock.signal);
        }
        auto pins = data.optimized_design->pins;
        std::stable_sort(pins.begin(), pins.end(), [](const auto& a, const auto& b) {
            const auto group = [](const auto& p) { return p.number ? (p.node ? 1 : 0) : (p.node ? 3 : 2); };
            if (group(a) != group(b))
                return group(a) < group(b);
            return a.number && b.number ? a.number < b.number : false;
        });
        for (const auto& pin : pins) {
            auto name = pin.name;
            display(name);
            logical_order[name] = 2 * unsigned(logical_order.size());
        }
        route_order = logical_order;
        for (const auto& node : data.nodes)
            if (node.paired || node.input_register)
                for (const auto& pin : data.pins)
                    if (int(pin.number) == node.pin)
                        logical_order[node.name] = logical_order[pin.name] + 1;
        std::stable_sort(data.nodes.begin(), data.nodes.end(),
                         [&](const auto& a, const auto& b) { return logical_order[a.name] < logical_order[b.name]; });
    }
    std::map<std::string, std::string> display_names;
    for (const auto& node : data.nodes)
        if (node.implied && node.logic != 'C' && !node.explicit_output_pair && !node.fanout_blocks.empty())
            display_names[node.name] = "RN_" + node.name;
    for (auto& node : data.nodes) {
        if (display_names.count(node.name)) {
            logical_order[display_names.at(node.name)] = logical_order[node.name];
            node.name                                  = display_names.at(node.name);
        }
        for (auto& name : node.fanins)
            if (display_names.count(name))
                name = display_names.at(name);
    }
    for (auto& block : data.blocks)
        for (auto& route : block.routes)
            if (display_names.count(route.signal)) {
                route_order[display_names.at(route.signal)] = route_order[route.signal];
                route.signal                                = display_names.at(route.signal);
            }
    for (auto& pin : data.pins)
        if (display_names.count(pin.output_signal))
            pin.output_signal = display_names.at(pin.output_signal);
    const auto xor_type = [&](const FitterReportNode& node) {
        if (node.input_register || !data.successful)
            return '.';
        if (node.xor_mode)
            return *node.xor_mode;
        if (node.xor_terms)
            return 'P';
        return node.product_terms <= 1 ? 'P' : 'G';
    };
    struct Control
    {
        std::string name;
        std::set<unsigned> blocks;
    };
    std::map<std::string, std::vector<Control>> control_summaries;
    for (const auto& category : {std::string("PRESET"), std::string("RESET"), std::string("OUTPUT ENABLE")}) {
        if (modern && category != "OUTPUT ENABLE")
            continue;
        std::map<std::string, unsigned> indices;
        std::vector<std::size_t> control_order;
        for (std::size_t i = 0; i < data.nodes.size(); ++i)
            control_order.push_back(i);
        if (!data.successful)
            std::stable_sort(control_order.begin(), control_order.end(), [&](auto a, auto b) {
                return data.nodes[a].traversal_order && data.nodes[b].traversal_order
                           ? *data.nodes[a].traversal_order < *data.nodes[b].traversal_order
                           : a > b;
            });
        for (auto index : control_order) {
            auto& node = data.nodes[index];
            auto& key  = category == "PRESET" ? node.preset : category == "RESET" ? node.reset : node.oe_expression;
            if (key.empty())
                continue;
            auto found = indices.find(key);
            if (found == indices.end()) {
                const auto index = unsigned(indices.size());
                indices[key]     = index;
                control_summaries[category].push_back(
                    {node.name.compare(0, 3, "RN_") == 0 ? node.name.substr(3) : node.name, {}});
                found = indices.find(key);
            }
            auto& control = control_summaries[category][found->second];
            if (node.block >= 0)
                control.blocks.insert(unsigned(node.block));
            else if (data.partition_failure && data.partition_failure->fixed_block >= 0 &&
                     node.name == data.partition_failure->signal)
                control.blocks.insert(unsigned(data.partition_failure->fixed_block));
            key = std::to_string(found->second + 1);
        }
    }
    for (auto& pin : data.pins)
        if (pin.output)
            for (const auto& node : data.nodes)
                if (node.pin == int(pin.number) && !node.oe_expression.empty())
                    pin.enable = node.oe_expression;
    std::string filename = source_filename.substr(source_filename.find_last_of("/\\") + 1);
    std::string out = program_banner("MACHFITR", "\n") + "\n\n";
    out += report_heading("Design Name = " + filename + ", Device = " + device + ", " + timestamp) + "\n";
    if (device == "MACH446") {
        if (data.optimized_design && data.optimized_design->dos_compatibility)
            out += "  Static ICC estimate unavailable in compatibility mode.\n\n";
        else if (data.estimated_static_icc_milliamps) {
            out += format_fields("  Estimated static ICC: %u milliamps.\n",
                                 unsigned(*data.estimated_static_icc_milliamps + 0.5f));
            out += "  Calculated from the MACH446 coefficient model; not a measurement.\n\n";
        }
    }
    out += report_heading("MACH FITTER OPTIONS");
    out += "SIGNAL PLACEMENT:\n  Handling of Preplacements                      " + settings.get("M3XX_PREPLACE") +
           "\n  Use placement data from                        " + settings.get("FITR_PLACE") +
           "\n\nFITTING OPTIONS:\n";
    const auto yes = [&](const char* key) { return settings.yes_no(key) ? "Y" : "N"; };
    if (modern) {
        if (!separate_clocks)
            out += format_fields("  Global clocks routable as PT clocks?           %s\n", yes("PTERM_CLK"));
        if (device == "MACH445" || device == "MACH446" || separate_clocks)
            out += format_fields("  Zero hold time for input register?             %s\n", yes("ZERO_HOLD_TM"));
        out += format_fields("  22V10/MACH1XX/2XX S/R Compatibility?           %s\n", yes("EQN_22V10"));
    }
    out += format_fields("  SET/RESET treated as DONT_CARE?                %s\n",
                         device == "MACH215" ? "N" : yes("M3XX_SET_RST"));
    if (!modern && !has_power && device != "MACH215" && device != "MACH220")
        out += format_fields("  Unused I/O pins configured as outputs?         %s\n", yes("UNUSED_IO"));
    if (modern)
        out += "  Reduce Unforced Global Clocks?                 " + settings.get("SIZE_REDUCE") + " \n";
    out += format_fields("  Iterate between partitioning and place/route?  %s\n", yes("ITERATE_FITR"));
    out += format_fields("  Balanced partitioning?                         %s\n", yes("BALANCED_PAR"));
    out += modern ? format_fields("  Reduce Routes Per Placement?                   %s\n", yes("ROUTE_PER_PL"))
                  : format_fields("  Spread Placement?                              %s\n", yes("SPREAD_PL"));
    const auto runtime = settings.integer("M3XX_RUNTIME");
    out += "  Maximun Run Time                               " +
           (runtime ? std::to_string(runtime) + " minutes" : "run until completion") + "\n\n";
    out += report_heading("DEVICE RESOURCE SUMMARY");
    out +=
        "                                 Total\n                               Available  Used  Available  "
        "Utilization\nDedicated Pins\n";
    unsigned clock_used = 0, input_used = 0, io_used = 0, ir_used = 0, mux_used = 0, pt_used = 0, cell_used = 0,
             cluster_used = 0, single_used = 0;
    const auto available_total = [&](std::optional<unsigned> FitterReportBlock::* member) -> std::optional<unsigned> {
        unsigned total = 0;
        for (const auto& block : data.blocks) {
            const auto& value = block.*member;
            if (!value)
                return std::nullopt;
            total += *value;
        }
        return total;
    };
    const auto unusable    = available_total(&FitterReportBlock::unusable);
    const auto free_cells  = available_total(&FitterReportBlock::free_macrocells);
    const auto free_single = available_total(&FitterReportBlock::free_single_pt);
    for (const auto& pin : data.pins) {
        if (std::find(layout.clock_pins.begin(), layout.clock_pins.end(), pin.number) != layout.clock_pins.end())
            ++clock_used;
        else if (std::find(layout.dedicated_pins.begin(), layout.dedicated_pins.end(), pin.number) !=
                 layout.dedicated_pins.end())
            ++input_used;
        else if (pin.number)
            ++io_used;
    }
    for (const auto& block : data.blocks) {
        ir_used += block.input_registers;
        mux_used += block.fanin;
        pt_used += block.product_terms;
        cell_used += block.macrocells;
        cluster_used += block.partition_clusters.value_or(block.clusters);
    }
    for (const auto& node : data.nodes)
        if (!node.input_register && node.product_terms <= 1)
            ++single_used;
    if (data.partition_failure) {
        io_used = 0;
        for (const auto& block : data.blocks)
            io_used += block.io_pins;
        input_used = 0;
    }
    const auto resource = [&](const char* label, unsigned total, unsigned used) {
        out += format_fields("%-31s%4u     %4u   %4u    -->   %3u%%\n", label, total, used,
                             total >= used ? total - used : 0, total ? 100 * used / total : 0);
    };
    resource("  Input-Only Pins",
             unsigned(layout.dedicated_pins.size() - (separate_clocks ? 0 : layout.clock_pins.size())), input_used);
    resource(separate_clocks ? "  Clock Pins" : "  Clock/Input Pins", unsigned(layout.clock_pins.size()), clock_used);
    resource("I/O Pins", unsigned(layout.io_pins.size()), io_used);
    if (dedicated_ir)
        resource("Input Registers", unsigned(layout.io_pins.size()), ir_used);
    resource("Central Switch Matrix Outputs", blocks * layout.muxes, mux_used);
    resource("Product Term Clusters", blocks * logic_cells, cluster_used);
    resource("Logical Product Terms", blocks * logic_cells * pts_per_cluster, pt_used);
    resource("Logic Macrocells", blocks * logic_cells, cell_used + (modern ? unusable.value_or(0) : 0));
    if (modern) {
        out += format_fields(
            "  > 1 PT Macrocells              ..     %4s   %4s\n",
            data.successful || data.retained_image ? std::to_string(cell_used - single_used).c_str() : "..",
            format_number(free_cells).c_str());
        out += format_fields("  1 PT Macrocells                ..     %4s   %4s\n",
                             data.successful || data.retained_image ? std::to_string(single_used).c_str() : "..",
                             format_number(free_single).c_str());
    } else if (!dedicated_ir)
        out += format_fields("  Input Registers                ..     %4s     ..\n",
                             has_ir ? std::to_string(ir_used).c_str() : "..");
    out += format_fields("  Unusable Macrocells            ..     %4s     ..\n\n", format_number(unusable).c_str());
    out += report_heading("BLOCK PARTITIONING SUMMARY");
    if (modern)
        out += has_ir ? "                                                      Macrocells  # of PT  \n                 "
                        " Logic   I/O    Inp    Macrocells    available   clusters \n          Fanin    PTs    Pins   "
                        "Reg   Used Unusable  1PT   >1PT  available\n"
                      : "                                                      Macrocells  # of PT  \n                 "
                        " Logic   I/O           Macrocells    available   clusters \n          Fanin    PTs    Pins    "
                        "     Used Unusable  1PT   >1PT  available\n";
    else if (has_ir)
        out +=
            "                                                                  # of PT  \n                  Logic   "
            "I/O    Inp    Macrocells    Macrocells  clusters \n          Fanin    PTs    Pins   Reg   Used Unusable  "
            "available   available\n";
    else
        out +=
            "                                                                  # of PT  \n                  Logic   "
            "I/O           Macrocells    Macrocells  clusters \n          Fanin    PTs    Pins         Used Unusable  "
            "available   available\n";
    if (data.partition_failure) {
        const auto label = out.rfind("Used Unusable");
        if (label != std::string::npos)
            out.replace(label + 5, 8, 8, ' ');
    }
    const std::string line = "|---------------------------------------------------------------------------|\n";
    out += line;
    out += format_fields("Maximum   %4u   %4u   %4u   %4s     ..     ..       %4u      %4u\n", layout.muxes,
                         logic_cells * pts_per_cluster, ios, has_ir ? std::to_string(ios).c_str() : "", logic_cells,
                         logic_cells);
    out += line;
    for (unsigned b = 0; b < blocks; ++b) {
        const auto& v = data.blocks[b];
        out += format_fields("Block %c   %4u   %4u   %4u   %4s   %4u   %4s", char('A' + b), v.fanin, v.product_terms,
                             v.io_pins, has_ir ? std::to_string(v.input_registers).c_str() : "", v.macrocells,
                             format_number(v.unusable).c_str());
        if (modern)
            out += format_fields("   %4s   %4s   %4s\n", format_number(v.free_single_pt).c_str(),
                                 format_number(v.free_macrocells).c_str(), format_number(v.free_clusters).c_str());
        else
            out += format_fields("       %4s      %4s\n", format_number(v.free_macrocells).c_str(),
                                 format_number(v.free_clusters).c_str());
    }
    if (data.partition_failure)
        out += '\n';
    else
        out += "\n> " + std::string(modern ? "Four" : "Two") +
               " rightmost columns above reflect last status of the placement process.\n\n";
    out += report_heading("SIGNAL SUMMARY");
    if (modern)
        out +=
            "                                 Pin/Node Logic\n       Signals Block Loc PTs XOR   Type   Type    "
            "Fanout" +
            std::string(power_column ? " Power" : "") + "\n";
    else if (device == "MACH215")
        out += "                             Pin/Node Logic\n       Signals Block Loc PTs   Type   Type    Fanout \n";
    else
        out +=
            "                             Pin/Node Logic              Output\n       Signals Block Loc PTs   Type   "
            "Type  Preset  Reset Enable Fanout" +
            std::string(power_column ? " Power" : "") + "\n";
    out += line;
    struct Summary
    {
        std::string name;
        const FitterReportPin* pin   = nullptr;
        const FitterReportNode* node = nullptr;
    };
    std::vector<Summary> summary;
    for (const auto& pin : data.pins) {
        const bool ordinary =
            std::find(layout.dedicated_pins.begin(), layout.dedicated_pins.end(), pin.number) ==
                layout.dedicated_pins.end() &&
            std::find(layout.clock_pins.begin(), layout.clock_pins.end(), pin.number) == layout.clock_pins.end();
        const bool visible =
            modern && data.partition_failure
                ? pin.output || (!pin.dedicated_clock && std::find(layout.io_pins.begin(), layout.io_pins.end(),
                                                                   pin.number) == layout.io_pins.end())
                : ordinary;
        if (visible)
            summary.push_back({pin.name, &pin, nullptr});
    }
    for (const auto& node : data.nodes)
        summary.push_back({node.name, nullptr, &node});
    std::stable_sort(summary.begin(), summary.end(), [](const auto& a, const auto& b) {
        if (bool(a.node) != bool(b.node))
            return !a.node;
        return a.name < b.name;
    });
    for (const auto& row : summary) {
        const auto node = row.node ? row.node : [&]() -> const FitterReportNode* {
            for (const auto& n : data.nodes)
                if (!n.input_register && ((row.pin->number && n.pin == int(row.pin->number)) ||
                                          (!row.pin->output_signal.empty() && n.name == row.pin->output_signal)))
                    return &n;
            return nullptr;
        }();
        const int block  = row.node ? row.node->block : row.pin->block;
        const auto loc   = row.node          ? (block < 0 ? "?" : std::string(1, char('A' + block))) +
                                                   (modern && row.node->input_register ? "ir-" : "") +
                                                   format_number(row.node->cell, "?")
                           : row.pin->number ? std::to_string(row.pin->number)
                                             : "?";
        const auto terms = row.node || row.pin->output
                               ? (node && !node->input_register ? std::to_string(node->product_terms) : ".")
                               : ".";
        const auto kind  = row.node ? node_type(*row.node) : !row.pin->enable.empty() ? "i/o pin" : pin_type(*row.pin);
        const auto logic = row.node ? logic_type(*row.node, modern) : "...";
        const auto fans  = format_fanout(blocks, row.node ? row.node->fanout_blocks : row.pin->fanout_blocks);
        out +=
            format_fields(row.node && modern && row.node->input_register ? "%14s   %c %5s %2s" : "%14s   %c  %4s %2s",
                          row.name.c_str(), block < 0 ? '?' : char('A' + block), loc.c_str(), terms.c_str());
        if (modern) {
            out += format_fields("   %c %8s   %3s   %s", row.node ? xor_type(*node) : '.', kind.c_str(), logic.c_str(),
                                 fans.c_str());
            if (power_column)
                out += format_fields(" %5s", node ? format_text(node->power_annotation).c_str() : ".");
            out += '\n';
        } else if (device == "MACH215")
            out += format_fields(" %8s   %3s   %s\n", kind.c_str(), logic.c_str(), fans.c_str());
        else {
            out += format_fields(" %8s   %3s   %3s    %3s   %3s    %s", kind.c_str(), logic.c_str(),
                                 row.node && data.successful ? format_text(node->preset, "...").c_str() : "...",
                                 row.node && data.successful ? format_text(node->reset, "...").c_str() : "...",
                                 row.node && data.successful ? format_text(node->oe_expression, "...").c_str() : "...",
                                 fans.c_str());
            if (power_column)
                out += format_fields("     %c", node ? (node->low_power ? 'L' : 'H') : '.');
            out += '\n';
        }
    }
    out += '\n';
    if (!modern && device != "MACH215") {
        out += report_heading("PRESET, RESET and OUTPUT ENABLE signal SUMMARY");
        out += '\n';
        for (const auto& category : {std::string("PRESET"), std::string("RESET"), std::string("OUTPUT ENABLE")}) {
            out += category +
                   " signal summary:\n\nNumber      Pin or Node Name      Block "
                   "\n|--------------------------------------------------|\n";
            // The backend supplies canonical shared control identities. A
            // constant/implicit control has no named signal summary entry.
            const auto& controls = control_summaries[category];
            for (unsigned i = 0; i < controls.size(); ++i) {
                const auto used =
                    format_fanout(blocks, std::vector<unsigned>(controls[i].blocks.begin(), controls[i].blocks.end()));
                out += format_fields("%2u %21s          %s\n", i + 1, controls[i].name.c_str(), used.c_str());
            }
            out += '\n';
        }
    }
    out += report_heading("TABULAR INFORMATION");
    out += '\n';
    auto dedicated_pins = layout.dedicated_pins;
    for (auto p : layout.clock_pins)
        if (std::find(dedicated_pins.begin(), dedicated_pins.end(), p) == dedicated_pins.end())
            dedicated_pins.push_back(p);
    std::sort(dedicated_pins.begin(), dedicated_pins.end());
    bool any_dedicated = false;
    for (const auto& p : data.pins)
        any_dedicated |= std::find(layout.dedicated_pins.begin(), layout.dedicated_pins.end(), p.number) !=
                         layout.dedicated_pins.end();
    if (any_dedicated && !(modern && data.partition_failure)) {
        out +=
            "DEDICATED PINS\n                                             Logic             Clock\nPin              "
            "Signal        Type          Fanout            "
            "Fanout\n|------------------------------------------------------------------------|\n";
        for (auto number : dedicated_pins) {
            const FitterReportPin* pin = nullptr;
            for (const auto& p : data.pins)
                if (p.number == number)
                    pin = &p;
            const auto clock = std::find(layout.clock_pins.begin(), layout.clock_pins.end(), number);
            const auto label = !modern && pin && clock != layout.clock_pins.end()
                                   ? format_fields("(Ck%u)", unsigned(clock - layout.clock_pins.begin()))
                                   : "";
            out += format_fields(
                blocks > 8 ? "%3u%-5s %14s  %10s  %14s  %14s\n" : "%3u%-5s %14s  %10s    %14s    %14s\n", number,
                label.c_str(), pin ? pin->name.c_str() : "..............",
                pin ? (pin->clock_blocks.empty() ? "input"
                       : separate_clocks         ? "clock"
                                                 : "clk/inp")
                    : "..",
                pin ? format_fanout(blocks, pin->fanout_blocks).c_str() : std::string(blocks, '.').c_str(),
                pin ? format_fanout(blocks, pin->clock_blocks).c_str() : std::string(blocks, '.').c_str());
        }
        out += '\n';
    }
    out += report_heading("Signals - Equations Where Used");
    out += "Signal Source :    Fanout List\n" + line;
    std::map<std::string, std::vector<const FitterReportNode*>> consumers;
    for (const auto& node : data.nodes)
        if (!node.input_register)
            for (const auto& input : node.fanins)
                consumers[input].push_back(&node);
    std::vector<std::string> source_order;
    for (const auto& pair : consumers)
        if (std::none_of(data.nodes.begin(), data.nodes.end(), [&](const auto& n) { return n.name == pair.first; }))
            source_order.push_back(pair.first);
    for (const auto& node : data.nodes)
        if (consumers.count(node.name))
            source_order.push_back(node.name);
    if (!logical_order.empty())
        std::stable_sort(source_order.begin(), source_order.end(),
                         [&](const auto& a, const auto& b) { return logical_order[a] < logical_order[b]; });
    const auto equation_name = [&](const auto& node) {
        if (node.paired)
            for (const auto& pin : data.pins)
                if ((pin.number && int(pin.number) == node.pin) ||
                    (!pin.output_signal.empty() && node.name == pin.output_signal))
                    return pin.name;
        return node.name.compare(0, 3, "RN_") == 0 ? node.name.substr(3) : node.name;
    };
    for (const auto& source : source_order) {
        const auto& destinations = consumers.at(source);
        out += format_fields("%14s:", source.c_str());
        std::string fanout_blocks;
        for (unsigned i = 0; i < destinations.size(); ++i) {
            auto node = destinations[i];
            if (i && i % 3 == 0) {
                out += "\n              :";
                fanout_blocks += ' ';
            }
            out += format_fields(" %14s{%c}", equation_name(*node).c_str(),
                                 node->block < 0 ? (data.partition_failure ? '.' : '?') : char('A' + node->block));
            fanout_blocks += node->block < 0 ? (data.partition_failure ? '.' : '?') : char('A' + node->block);
        }
        out += "\n              {" + fanout_blocks + "}\n";
    }
    for (unsigned b = 0; b < blocks; ++b) {
        bool title = false;
        for (const auto& source : source_order) {
            std::vector<const FitterReportNode*> local;
            for (auto node : consumers.at(source))
                if (node->block == int(b))
                    local.push_back(node);
            if (local.size() != 1)
                continue;
            const bool input_register_source = std::any_of(data.nodes.begin(), data.nodes.end(), [&](const auto& n) {
                return n.input_register && n.name == source;
            });
            if (!title && !input_register_source) {
                out +=
                    format_fields("\nBlock %c singular list (Input drives only one logic equation)\n", char('A' + b));
                title = true;
            }
            out += format_fields("%14s: %14s\n", source.c_str(), equation_name(*local.front()).c_str());
        }
    }
    out += "\n\n";
    if (modern && data.partition_failure) {
        std::vector<const FitterReportPin*> clocks;
        for (const auto& pin : data.pins)
            if (pin.dedicated_clock)
                clocks.push_back(&pin);
        std::sort(clocks.begin(), clocks.end(), [](auto a, auto b) { return a->number < b->number; });
        if (!clocks.empty()) {
            if (out.size() >= 2 && out.substr(out.size() - 2) == "\n\n")
                out.pop_back();
            out += report_heading("Global Clocks");
            for (auto pin : clocks)
                out += format_fields("Signal %s is placed at pin %u\n", pin->name.c_str(), pin->number);
            out += "\n\n";
        }
    }
    for (unsigned b = 0; b < blocks; ++b) {
        const auto& block = data.blocks[b];
        if (!block.macrocells && !block.input_registers && !block.io_pins && !block.fanin)
            continue;
        if (modern && !block.clocks.empty()) {
            if (!out.empty() && out.size() >= 2 && out.substr(out.size() - 2) == "\n\n")
                out.pop_back();
            out +=
                format_fields("BLOCK %c CLOCK MUX%s\n", char('A' + b), data.successful ? " " : " (may be incomplete!)");
            out += "Block Clocks     Signal    Pin   Pol\n|-----------------------------------|\n";
            for (unsigned slot = 0; slot < layout.clock_pins.size(); ++slot) {
                const FitterReportClock* clock = nullptr;
                for (const auto& c : block.clocks)
                    if (c.slot == int(slot))
                        clock = &c;
                if (data.partition_failure && slot < block.clocks.size())
                    clock = &block.clocks[slot];
                out += format_fields("Ck%s   %17s    %3s    %c\n", data.successful ? std::to_string(slot).c_str() : "?",
                                     clock ? clock->signal.c_str() : "...",
                                     clock ? format_number(clock->pin).c_str() : "..",
                                     clock ? (clock->negative ? 'L' : 'H') : '.');
            }
            out += "\n\n";
        }
        if (block.macrocells || block.input_registers || block.required_macrocells.value_or(0)) {
            if (b)
                while (out.size() < 3 || out.substr(out.size() - 3) != "\n\n\n")
                    out += '\n';
            out += format_fields("BLOCK %c LOGIC MACROCELLS%s\n", char('A' + b), has_ir ? " & INPUT REGISTERS" : " ");
            out += "                                  PTS \n                             T   U   A\n";
            if (modern)
                out +=
                    "L                            Y   S   V  X  P   C  S  R   P\nO                            P   E   "
                    "A  O  O   L  E  E   I\nC     Node          Signal   E   D   L  R  L   K  T  S   N   Fanout\n";
            else if (device == "MACH215")
                out +=
                    "L                            Y   S   V  P   C    P\nO                            P   E   A  O   L "
                    "   I\nC     Node          Signal   E   D   L  L   K    N   Fanout\n";
            else
                out +=
                    "L                            Y   S   V  P   C  S  R   P\nO                            P   E   A  "
                    "O   L  E  E   I\nC     Node          Signal   E   D   L  L   K  T  S   N   Fanout\n";
            out += line;
            // DOS defect: on MACH1/2 the original prints the macrocell's own
            // location in the clock column when the clock is Ck0. Clocks 1-3
            // are labelled correctly. Reproduced only in compatibility mode.
            const auto first_clock_defect = [&](const FitterReportNode* node) {
                return !modern && device != "MACH215" && data.optimized_design &&
                       data.optimized_design->dos_compatibility && node->clock == "Ck0";
            };
            const auto node_row = [&](const FitterReportNode* node, unsigned c, bool input_register) {
                std::string location     = std::string(1, char('A' + b)) + std::to_string(c);
                const auto cell_location = location;
                if (input_register)
                    location = modern ? std::string(1, char('A' + b)) + "ir-" + std::to_string(c) : location + "-ir";
                const bool unknown = node && node->cell < 0;
                if (unknown)
                    location = std::string(1, char('A' + b)) + (input_register && modern ? "ir-?" : "?");
                const unsigned node_number =
                    input_register && modern ? blocks * cells + 2 + b * ios + c : 2 + b * cells + c;
                const auto available = unknown                         ? std::string(".")
                                       : node && node->available_terms ? std::to_string(*node->available_terms)
                                       : c < block.available_terms.size() && !input_register
                                           ? std::to_string(block.available_terms[c])
                                       : input_register ? (modern ? "." : "0")
                                                        : "..";
                out += format_fields(
                    "%-6s%3s %16s  %3s %2s %3s", location.c_str(), unknown ? ".." : std::to_string(node_number).c_str(),
                    node ? node->name.c_str() : "................", node ? logic_type(*node, modern).c_str() : "...",
                    node && !input_register ? std::to_string(node->product_terms).c_str() : ".", available.c_str());
                if (modern)
                    out += format_fields("  %c", node ? xor_type(*node) : '.');
                out += format_fields(
                    modern && input_register && node && data.successful                      ? "  %c%5s"
                    : device == "MACH215" && node && (data.successful || node->logic == 'C') ? "  %c %4s"
                                                                                             : "  %c %3s",
                    node ? (node->negative ? 'L' : 'H') : '.',
                    node ? (modern && input_register ? (data.successful ? location : ".")
                            : first_clock_defect(node)
                                ? cell_location
                                : format_text(node->clock, device == "MACH215" && node->logic != 'C' ? ".." : "."))
                               .c_str()
                    : modern || device != "MACH215" ? "."
                                                    : "..");
                if (device != "MACH215")
                    out += format_fields(" %2s %2s",
                                         node && !data.partition_failure ? format_text(node->preset).c_str() : ".",
                                         node && !data.partition_failure ? format_text(node->reset).c_str() : ".");
                out += format_fields(
                    device == "MACH215" && (!node || (!data.successful && node->logic != 'C')) ? "   %2s  %s\n"
                                                                                               : " %3s  %s\n",
                    node ? format_number(node->pin).c_str() : "..",
                    node ? format_fanout(blocks, node->fanout_blocks).c_str() : std::string(blocks, '.').c_str());
            };
            for (unsigned c = 0; c < cells; ++c) {
                const bool ir                = device == "MACH215" && (c % 2);
                const FitterReportNode* node = nullptr;
                for (const auto& n : data.nodes)
                    if (n.block == int(b) && n.cell == int(c) && (!modern || !n.input_register))
                        node = &n;
                node_row(node, c, ir || (node && node->input_register));
            }
            if (modern && has_ir)
                for (unsigned c = 0; c < ios; ++c) {
                    const FitterReportNode* node = nullptr;
                    for (const auto& n : data.nodes)
                        if (n.block == int(b) && n.cell == int(c) && n.input_register)
                            node = &n;
                    node_row(node, c, true);
                }
            std::vector<const FitterReportNode*> unknown_nodes;
            for (const auto& node : data.nodes)
                if (node.block == int(b) && node.cell < 0)
                    unknown_nodes.push_back(&node);
            std::stable_sort(unknown_nodes.begin(), unknown_nodes.end(), [](auto a, auto b) {
                return a->traversal_order.value_or(0) < b->traversal_order.value_or(0);
            });
            for (auto node : unknown_nodes)
                node_row(node, 0, node->input_register);
        } else {
            while (out.size() >= 2 && out.substr(out.size() - 2) == "\n\n")
                out.pop_back();
        }
        if (block.io_pins || block.input_registers) {
            out += format_fields("\nBLOCK %c I/O PINS\n", char('A' + b));
            if (modern)
                out += has_ir ? "Pin onode inode           Signal     Type    Fanout\n"
                              : "Pin onode                 Signal     Type    Fanout\n";
            else if (has_ir)
                out += "                  Output\nPin onode inode  Enable           Signal     Type    Fanout\n";
            else
                out += "                  Output\nPin onode        Enable           Signal     Type    Fanout\n";
            out += "|----------------------------------------------------------------|\n";
            std::vector<unsigned> block_pins(layout.io_pins.begin() + b * ios, layout.io_pins.begin() + (b + 1) * ios);
            std::sort(block_pins.begin(), block_pins.end());
            for (const auto p : block_pins) {
                const FitterReportPin* pin = nullptr;
                for (const auto& candidate : data.pins)
                    if (candidate.number == p &&
                        (!data.partition_failure || (candidate.block == int(b) && (!modern || candidate.output))))
                        pin = &candidate;
                out += format_fields("%3u %4s", p, pin ? format_number(pin->output_node, "...").c_str() : "...");
                if (has_ir)
                    out += format_fields("  %4s", pin ? format_number(pin->input_node, "...").c_str() : "...");
                else
                    out += "      ";
                if (!modern)
                    out += format_fields("    %3s ",
                                         pin && data.successful ? format_text(pin->enable, "...").c_str() : "...");
                out += format_fields(
                    "  %16s  %8s  %s\n", pin ? pin->name.c_str() : "................",
                    pin ? (!pin->enable.empty() ? "i/o pin" : pin_type(*pin).c_str()) : "..",
                    pin ? format_fanout(blocks, pin->fanout_blocks).c_str() : std::string(blocks, '.').c_str());
            }
            std::vector<const FitterReportPin*> unknown_pins;
            for (const auto& pin : data.pins)
                if (pin.block == int(b) && pin.number == 0)
                    unknown_pins.push_back(&pin);
            const auto pin_order = [&](const auto* pin) {
                if (pin->traversal_order)
                    return *pin->traversal_order;
                for (const auto& node : data.nodes)
                    if (node.name == pin->output_signal)
                        return node.traversal_order.value_or(
                            unsigned(logical_order.size() * 2 - logical_order[node.name]));
                return 0u;
            };
            if (data.partition_failure)
                std::stable_sort(unknown_pins.begin(), unknown_pins.end(),
                                 [&](auto a, auto c) { return pin_order(a) < pin_order(c); });
            for (const auto* pin : unknown_pins) {
                out += format_fields("  ? %4s", format_number(pin->output_node, "...").c_str());
                if (has_ir)
                    out += format_fields("  %4s", format_number(pin->input_node, "...").c_str());
                else
                    out += "      ";
                if (!modern)
                    out += "    ... ";
                out += format_fields("  %16s  %8s  %s\n", pin->name.c_str(), pin_type(*pin).c_str(),
                                     format_fanout(blocks, pin->fanout_blocks).c_str());
            }
        }
        if (!block.fanin) {
            out += modern ? "\n\n" : "\n";
            continue;
        }
        out += format_fields("\n\nBLOCK %c LOGIC ARRAY FANIN\n", char('A' + b));
        out +=
            "CSM             Signal    Source        CSM             Signal    "
            "Source\n|----------------------------------|    |----------------------------------|\n";
        std::vector<const FitterReportRoute*> report_routes;
        for (const auto& route : block.routes)
            report_routes.push_back(&route);
        // Failed RPT enumerates the logical signal records. The physical
        // router uses a different, fanout-dependent traversal (shown in PRD).
        if (!modern && !data.successful && !data.partition_failure)
            std::stable_sort(report_routes.begin(), report_routes.end(), [&](auto a, auto c) {
                return a->logical_id.value_or(route_order[a->signal]) < c->logical_id.value_or(route_order[c->signal]);
            });
        const auto route_row = [&](unsigned mux) {
            const FitterReportRoute* route = nullptr;
            for (const auto& r : block.routes)
                if (r.mux == int(mux))
                    route = &r;
            if (!data.successful)
                route = mux < report_routes.size() ? report_routes[mux] : nullptr;
            const auto source =
                route ? (route->source_pin >= 0 && (route->source_block < 0 || route->source_input_register)
                             ? (route->source_input_register || route->source_pin > int(layout.pins) ? "inode "
                                                                                                     : "pin ") +
                                   std::to_string(route->source_pin)
                             : "mcell " +
                                   (route->source_block < 0 ? "?" : std::string(1, char('A' + route->source_block))) +
                                   "-" + format_number(route->source_cell, "?"))
                      : "...";
            return format_fields("mx%c%-2s %15s  %12s", char('A' + b),
                                 data.successful ? std::to_string(mux).c_str() : "?",
                                 route ? route->signal.c_str() : "...", source.c_str());
        };
        const unsigned half = (layout.muxes + 1) / 2;
        for (unsigned i = 0; i < half; ++i) {
            out += route_row(i);
            if (i + half < layout.muxes)
                out += "     " + route_row(i + half);
            out += '\n';
        }
        out += "\n\n";
    }
    if (!data.placement_failed_blocks.empty()) {
        if (out.size() >= 2 && out.substr(out.size() - 2) == "\n\n")
            out.pop_back();
        out += report_heading("PLACEMENT FAILURE REPORT");
        for (auto b : data.placement_failed_blocks) {
            if (data.cell_placement_failure && data.cell_placement_failure->block == b)
                out += format_fields("Not enough PTs in block [%c] for sig [%s].\n", char('A' + b),
                                     data.cell_placement_failure->signal.c_str());
            else
                out += format_fields("Could not assign IO pins to nodes in block [%c].\n", char('A' + b));
        }
        out += "\n\n\n";
    }
    if (!data.route_failed_blocks.empty()) {
        while (out.size() >= 3 && out.substr(out.size() - 3) == "\n\n\n")
            out.pop_back();
        out += report_heading("ROUTING FAILURE REPORT");
        out += "Reason(s): Not enough routing resources.\n";
        for (auto b : data.route_failed_blocks) {
            std::vector<const FitterReportRoute*> failed_routes;
            for (const auto& route : data.blocks[b].routes)
                if (route.mux < 0)
                    failed_routes.push_back(&route);
            // The RPT error list prepends failed logical records; it is
            // independent of both the fanin table and the PRD search order.
            if (!modern)
                std::stable_sort(failed_routes.begin(), failed_routes.end(), [&](auto a, auto c) {
                    return a->logical_id.value_or(route_order[a->signal]) >
                           c->logical_id.value_or(route_order[c->signal]);
                });
            for (auto route : failed_routes)
                out += route->signal + " can not be routed to block " + char('A' + b) + "\n";
        }
        out += '\n';
    }
    if (data.partition_failure) {
        const auto& f = *data.partition_failure;
        if (out.size() >= 2 && out.substr(out.size() - 2) == "\n\n")
            out.pop_back();
        out += report_heading("PARTITIONING FAILURE REPORT");
        out += f.fixed_block >= 0
                   ? "Grouped or preplaced signal '" + f.signal + "' cannot be partitioned in specified block\n"
                   : "Signal '" + f.signal + "' cannot be placed in any block partition\n";
        out += "for the following reasons:\n";
        for (unsigned b = 0; b < f.block_reasons.size(); ++b)
            if (f.fixed_block < 0 || f.fixed_block == int(b)) {
                out += format_fields("BLOCK %c - \n", char('A' + b));
                for (const auto& reason : f.block_reasons[b])
                    out += "     " + reason + "\n";
            }
        out += "\nThe following signals remain to be partitioned\n( excluding pins used only as inputs )\n";
        for (unsigned i = 0; i < f.remaining_signals.size(); ++i) {
            out += format_fields(i % 2 ? "%36s" : "%35s", f.remaining_signals[i].c_str());
            if (i % 2 || i + 1 == f.remaining_signals.size())
                out += '\n';
        }
        out += '\n';
    }
    if (!data.partition_failure && (out.size() < 3 || out.substr(out.size() - 3) != "\n\n\n"))
        out += '\n';
    out += device + " report file key:\n\n";
    if (modern)
        out += "A             - Asynchronous mode\n";
    out +=
        "AVAL          - Additional product terms available within the current\n                steering allocation, "
        "plus those potentially available\n                through resteering of free clusters.\n";
    if (modern)
        out +=
            "B0            - Block Asynchronous Reset/Preset product term 0\nB1            - Block Asynchronous "
            "Reset/Preset product term 1\n";
    out += "C             - Combinatorial\n";
    for (unsigned c = 0; c < layout.clock_pins.size(); ++c) {
        if (modern) {
            const auto pair = (c / 2) * 2;
            out += format_fields("Ck%u           - Block clock generated from pin %u or pin %u\n", c,
                                 layout.clock_pins[pair],
                                 layout.clock_pins[std::min(pair + 1, unsigned(layout.clock_pins.size() - 1))]);
        } else
            out += format_fields("Ck%u           - Dedicated clock pin\n", c);
    }
    out +=
        "clk           - Clock\nCSM           - Central Switch Matrix\nD             - D-type flip flop\nG             "
        "- Ground\nH             - High\n";
    if (power_column && !modern)
        out += "H             - onode of signal is NOT powered down.\n";
    if (device == "MACH446")
        out +=
            "H             - Signal not specified as low power, driven\n                and controlled only by pterms "
            "from \n                high power clusters.\n#H            - Signal specified as low power, driven\n      "
            "          and controlled only by pterms from \n                high power clusters.\n";
    out +=
        "implied       - Node occupying the macrocell drives the output pin\n                but not defined in the "
        "design file.\ninode         - Input node\nInp           - Input\nipair         - Input paired node\nI/O       "
        "    - Input or Output\nL             - Low\n";
    if (device == "MACH446")
        out +=
            "L             - Signal driven or controlled only by\n                pterms from low power "
            "cluster(s).\nLHc           - Asynchronous signal driven only by pterm(s)\n                from low power "
            "cluster(s) and having\n                clock and set/reset pterms in a high\n                power "
            "cluster.\nLHd           - Signal driven by pterms from both high\n                and low power clusters. "
            "If asynchronous,\n                having clock and set/reset product terms\n                in a low "
            "power cluster.\nLHdc          - Asynchronous signal driven by pterms from both\n                high and "
            "low power clusters. Also,\n                having clock and set/reset product terms\n                in a "
            "high power cluster.\n";
    out += "L             - Latch\n";
    if (power_column && !modern)
        out += "L             - onode of signal is powered down.\n";
    out +=
        "LOC           - Location\nmcell <X>     - Source is macrocell from block <X>\nMux           - Multiplexer\nmx "
        "           - Block Array input multiplexer\nonode         - Output node\nopair         - Output paired "
        "node\nP             - Product Term\nPol           - Polarity\nPT(s)         - Product term(s)\nReg           "
        "- Register\n";
    if (device != "MACH215")
        out += "Res           - Reset control\n";
    out += "RN_<pin_name> - Output node paired with <pin_name> created by Fitter.\n";
    if (modern)
        out += "S             - Synchronous mode\n";
    if (device != "MACH215")
        out += "Set           - Preset control\n";
    out += "T             - T-type flip flop\n";
    if (modern)
        out += "XOR           - Exclusive OR gate\n<X>ir         - Input register in block <X>\n";
    else if (device == "MACH215")
        out += "<Y>XX-ir      - Input register XX in block Y\n";
    else
        out += "<Y>XX-ir      - Macrocell XX in block Y is used as an input register.\n";
    out += ".             - Not available or Not applicable\n\n";
    const auto percent = [&](unsigned done, unsigned total) {
        return total ? 100 * done / total : data.successful ? 100u : 0u;
    };
    out += format_fields(
        "Partitioning %3u%% - Completed\nPlacement    %3u%% - Completed\nRouting      %3u%% - Completed\n",
        percent(data.partitioned, data.partition_total), percent(data.placed, data.placement_total),
        percent(data.routed, data.routing_total));
    out += data.successful ? "%%% Fitting process is successful %%%\n" : "%%% Fitting process has failed %%%\n";
    std::string dos;
    dos.reserve(out.size() + out.size() / 30);
    for (char c : out) {
        if (c == '\n')
            dos += '\r';
        dos += c;
    }
    return dos;
}
}  // namespace machxl
