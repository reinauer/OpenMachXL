#include "fitter_place_report.hpp"
#include "banner.hpp"
#include "device_family.hpp"
#include "frontend.hpp"
#include "jedec.hpp"
#include "synthesis.hpp"
#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace machxl {
namespace {
#include "fitter_place_report_headings.inc"
template <class... A>
std::string format_fields(const char* format, A... args)
{
    char local[256];
    const int n = std::snprintf(local, sizeof(local), format, args...);
    if (n < 0)
        throw std::runtime_error("Place report formatting failed");
    if (std::size_t(n) < sizeof(local))
        return std::string(local, std::size_t(n));
    std::vector<char> buffer(std::size_t(n) + 1);
    const int written = std::snprintf(buffer.data(), buffer.size(), format, args...);
    if (written != n)
        throw std::runtime_error("Place report formatting size changed");
    return std::string(buffer.data(), std::size_t(n));
}
const std::string bar(75, '='), dash(75, '-');
std::string report_section(const std::string& title)
{
    return bar + "\n\t" + title + "\n" + bar + "\n";
}
std::string format_percentage(unsigned n, unsigned d)
{
    return d ? format_fields("%3u%%", 100 * n / d) : " n/a";
}
const char* signal_type(const FitterReportNode* n)
{
    return !n ? " ? " : n->input_register ? "Rin" : n->implied ? (!n->fanout_blocks.empty() ? " IO" : "OUT") : "NOD";
}
const char* signal_type(const FitterReportPin* p)
{
    return !p ? " ? " : p->output ? (p->input ? " IO" : "OUT") : !p->clock_blocks.empty() ? "Cin" : "INP";
}
std::string signal_name(const FitterReportNode* n)
{
    return n ? n->name : "";
}
std::string signal_name(const FitterReportPin* p)
{
    return p ? p->name : "";
}
char constraint_marker(const FitterReportNode* n)
{
    return n && n->fixed ? '*' : ' ';
}
char constraint_marker(const FitterReportPin* p)
{
    return p && p->fixed ? '*' : ' ';
}
std::string format_positions(const std::vector<unsigned>& values, int used, bool end_trim = false)
{
    std::string s;
    for (auto value : values)
        s += int(value) == used ? format_fields("(%3u)", value) : format_fields(" %3u ", value);
    if (end_trim && !s.empty())
        while (s.back() == ' ')
            s.pop_back();
    return s;
}
}  // namespace
std::string render_fitter_place_report(const FuseImage& image, const FitterReportData& source_data,
                                       const CompilerSettings& settings, const std::string& filename,
                                       const std::string& start, const std::string& end, const std::string& elapsed)
{
    auto data = source_data;
    std::map<std::string, std::string> feedback_names;
    if (data.optimized_design) {
        const auto aliases = pla_signal_aliases(*data.optimized_design);
        const auto rename  = [&](std::string& name) {
            auto it = aliases.find(name);
            if (it != aliases.end())
                name = it->second;
        };
        for (auto& p : data.pins)
            rename(p.name);
        for (auto& n : data.nodes) {
            rename(n.name);
            for (auto& x : n.fanins)
                rename(x);
            for (auto& x : n.data_fanins)
                rename(x);
        }
        for (auto& b : data.blocks)
            for (auto& r : b.routes)
                rename(r.signal);
    }
    for (auto& p : data.pins)
        if (p.output && !p.fanout_blocks.empty())
            p.input = true;
    for (const auto& n : data.nodes)
        if (n.implied && !n.explicit_output_pair && !n.fanout_blocks.empty()) {
            for (auto& p : data.pins)
                if (int(p.number) == n.pin)
                    p.input = true;
            if (n.logic != 'C')
                feedback_names[n.name] = "RN_" + n.name;
        }
    if (!data.complete || (!data.successful && !data.retained_image))
        throw std::runtime_error("Place report requires retained physical metadata");
    const auto& d       = mach_device_layout(image.device);
    const bool modern   = d.one_hot;
    const unsigned ios  = unsigned(d.io_pins.size()) / d.blocks;
    const auto pin_type = [&](const FitterReportPin* p) {
        return !modern && p && p->input_node >= 0 ? "in2" : signal_type(p);
    };
    std::map<std::string, unsigned> product_ids;
    std::istringstream pla(data.optimized_pla);
    std::string line;
    std::vector<std::vector<std::string>> covers;
    while (std::getline(pla, line)) {
        std::istringstream row(line);
        std::string cube, outputs;
        row >> cube >> outputs;
        if (cube == ".o") {
            covers.resize(std::stoul(outputs));
            continue;
        }
        if (!cube.empty() && cube.find_first_not_of("01-") == std::string::npos && outputs.size() == covers.size())
            for (unsigned i = 0; i < outputs.size(); ++i)
                if (outputs[i] == '1')
                    covers[i].push_back(cube);
    }
    unsigned product_index = 3;
    for (auto& cover : covers) {
        std::sort(cover.begin(), cover.end());
        std::string key;
        for (const auto& cube : cover)
            key += cube + ";";
        if (cover.empty())
            product_ids.emplace(key, 0);
        else if (cover.size() == 1 && cover[0].find_first_not_of('-') == std::string::npos)
            product_ids.emplace(key, 1);
        else if (!product_ids.count(key))
            product_ids[key] = product_index++;
    }
    std::map<unsigned, const FitterReportPin*> pins;
    std::map<int, const FitterReportNode*> nodes, registers;
    std::map<std::string, const FitterReportNode*> byname;
    for (const auto& p : data.pins)
        pins[p.number] = &p;
    for (const auto& n : data.nodes) {
        byname[n.name] = &n;
        if (n.cell < 0)
            continue;
        if (n.input_register)
            registers[n.block * int(ios) + (modern ? n.cell : n.cell / 2)] = &n;
        if (!n.input_register || !modern)
            nodes[n.block * int(d.cells) + n.cell] = &n;
    }
    const auto pin = [&](unsigned p) {
        auto it = pins.find(p);
        return it == pins.end() ? nullptr : it->second;
    };
    const auto node = [&](unsigned b, unsigned c) {
        auto it = nodes.find(int(b * d.cells + c));
        return it == nodes.end() ? nullptr : it->second;
    };
    const auto reg = [&](unsigned b, unsigned c) {
        auto it = registers.find(int(b * ios + c));
        return it == registers.end() ? nullptr : it->second;
    };
    const auto blocktitle = [](unsigned b, const char* s) { return format_fields("< Block [%c] >\t%s", 'A' + b, s); };
    unsigned key          = settings.yes_no("SPREAD_PL") ? 526336 : 2048;
    if (settings.yes_no("ROUTE_PER_PL"))
        key += 16386;
    std::string out = program_banner("MACHFITR", "\n") + "\n";
    out += "Start: " + start + "\nEnd  : " + end + "    $$$ Elapsed time: " + elapsed + "\n" + bar + "\nC:\\DAT\\" +
           image.device + " Design [" + filename + "]\n\n";
    out += format_fields("* Place/Route options (keycode = %u)\n\t= Spread Placement:\t%8s\n\n", key,
                         settings.yes_no("SPREAD_PL") ? "ON" : "OFF");
    out += "* Placement Completion\n";
    out += legacy_placement;
    for (unsigned b = 0; b < d.blocks; ++b) {
        const auto& x = data.blocks.at(b);
        unsigned placed_io =
            modern ? x.io_pins : unsigned(std::count_if(data.pins.begin(), data.pins.end(), [&](const auto& p) {
                return p.number && p.block == int(b);
            }));
        const unsigned legacy_registers =
            modern ? 0 : unsigned(std::count_if(data.nodes.begin(), data.nodes.end(), [&](const auto& n) {
                return n.input_register && n.block == int(b);
            }));
        const unsigned placed_cells =
            modern ? x.macrocells : unsigned(std::count_if(data.nodes.begin(), data.nodes.end(), [&](const auto& n) {
                return n.block == int(b) && n.cell >= 0;
            }));
        const unsigned required_cells =
            x.required_macrocells ? *x.required_macrocells + legacy_registers : placed_cells;
        if (std::find(data.placement_failed_blocks.begin(), data.placement_failed_blocks.end(), b) !=
            data.placement_failed_blocks.end())
            placed_io = unsigned(std::count_if(data.pins.begin(), data.pins.end(),
                                               [&](const auto& p) { return p.number && p.block == int(b); }));
        out += format_fields(" %c%c|%3u |%3u |%3u => %s |%6u |%3u => %s |%4u |%3u => %s\n", 'A' + b,
                             x.sequential_search ? '.' : ' ', d.cells, required_cells, placed_cells,
                             format_percentage(placed_cells, required_cells).c_str(), ios, placed_io,
                             format_percentage(placed_io, ios).c_str(), d.muxes, x.fanin,
                             format_percentage(x.fanin, d.muxes).c_str());
    }
    out += "---|----|----|------------|-------|------------|-----|------------------\n\n";
    unsigned inputs = 0, placed_inputs = 0, clock_used = 0, input_used = 0;
    for (const auto& n : data.nodes)
        if (n.input_register) {
            ++inputs;
            if (n.cell >= 0)
                ++placed_inputs;
        }
    for (const auto& p : data.pins) {
        if (!p.output && (modern || p.input_node < 0)) {
            ++inputs;
            if (p.number)
                ++placed_inputs;
        }
        if (p.number && p.block < 0) {
            if (std::find(d.clock_pins.begin(), d.clock_pins.end(), p.number) != d.clock_pins.end())
                ++clock_used;
            else
                ++input_used;
        }
    }
    out += format_fields("* Input/Clock Signal count: %3u -> placed: %3u = %s\n\n", inputs, placed_inputs,
                         data.successful ? "100%" : format_percentage(placed_inputs, inputs).c_str());
    out +=
        "                              Available   "
        "Used\n-----------------------------------------------------------------\n";
    unsigned dedicated = 0;
    for (auto p : d.dedicated_pins)
        if (std::find(d.clock_pins.begin(), d.clock_pins.end(), p) == d.clock_pins.end())
            ++dedicated;
    out += format_fields("\tInput Pins            : %3u      %3u    => %s\n", dedicated, input_used,
                         format_percentage(input_used, dedicated).c_str());
    out += format_fields("\tClock, Clk/Input Pins : %3u      %3u    => %s\n\n", unsigned(d.clock_pins.size()),
                         clock_used, format_percentage(clock_used, unsigned(d.clock_pins.size())).c_str());
    if (data.successful)
        out += "* Routing Completion: 100%\n";
    else {
        out += format_fields(
            "* Routing Completion: %s (%u of %u signals routed)\n\t==> Design unplaced and/or unrouted\n\n",
            data.imx_congested_blocks.empty() ? format_percentage(data.routed, data.routing_total).c_str() : "  0%",
            data.routed, data.routing_total);
        if (!data.route_failed_blocks.empty())
            out += "Warning: {Unplaced and/or unrouted design.}\n";
        for (auto block : data.route_failed_blocks)
            out += format_fields("Errors : {No alternate placements for signals feeding the unrouted block [%c].}\n",
                                 'A' + block);
        if (data.last_oe_constraint)
            out +=
                format_fields("Warning: {OErc[%d:%d]}\n", (*data.last_oe_constraint)[0], (*data.last_oe_constraint)[1]);
        for (auto block : data.placement_failed_blocks) {
            if (data.cell_placement_failure && data.cell_placement_failure->block == block) {
                const auto& failure = *data.cell_placement_failure;
                out += format_fields("Warning: {Could not place Sig [%10s] with [%2u] PTs in blk [%c].}\n",
                                     failure.signal.c_str(), failure.product_terms, 'A' + block);
                out += format_fields("Errors : {Not enough PTs in block [%c] for sig [%s].}\n", 'A' + block,
                                     failure.signal.c_str());
                continue;
            }
            const auto& frame = data.blocks[block];
            out += format_fields(frame.required_macrocells && frame.macrocells == *frame.required_macrocells
                                     ? "Errors : {Could not assign IO pins to nodes in block [%c].}\n"
                                     : "Errors : {Signals in block [%c] could not all be placed.}\n",
                                 'A' + block);
        }
        for (auto block : data.imx_congested_blocks)
            out += format_fields(
                "Warning: {Block [%c] has Input Multiplexer congestion.}\nErrors : {Signals in block [%c] placed, but "
                "IMX is congested.}\n",
                'A' + block, 'A' + block);
    }
    out += "* Attempts: Place [" +
           (data.placement_attempts ? format_fields("%8llu", *data.placement_attempts) : "     n/a") + "] Route [" +
           (data.routing_attempts ? format_fields("%8llu", *data.routing_attempts) : "     n/a") + "]\n";
    if (!data.successful)
        out += "\n#$ Warnings or errors $#\n";
    out += report_section("Signal Fanout Table");
    out += legacy_fanout;
    struct Signal
    {
        std::string name;
        const FitterReportPin* p;
        const FitterReportNode* n;
    };
    std::map<std::string, Signal> signals;
    for (const auto& p : data.pins)
        signals[p.name] = {p.name, &p, nullptr};
    for (const auto& n : data.nodes)
        if (!n.implied)
            signals[n.name] = {n.name, nullptr, &n};
    for (const auto& n : data.nodes)
        if (feedback_names.count(n.name)) {
            const auto alias = feedback_names.at(n.name);
            signals[alias]   = {alias, nullptr, &n};
        }
    unsigned serial = 0;
    for (const auto& item : signals) {
        const auto& s = item.second;
        int b         = s.p ? s.p->block : s.n->block;
        int number    = s.p ? int(s.p->number) : s.n->input_register ? s.n->pin : s.n->number;
        out += format_fields("%3u|%-14s|%c|%s|%4d|=>", ++serial, s.name.c_str(), b < 0 ? '+' : char('A' + b),
                             s.p ? (image.device == "MACH465" && !s.p->clock_blocks.empty() ? "CLK" : pin_type(s.p))
                             : s.n->input_register ? "Rin"
                                                   : "NOD",
                             number);
        if ((s.n && !s.n->input_register) || number <= 0)
            out.replace(out.size() - 7, 4, number <= 0 && s.p ? "  ? " : " N/A");
        const auto& fanouts = s.p ? s.p->fanout_blocks : s.n->fanout_blocks;
        for (unsigned block = 0; block < d.blocks; ++block) {
            const bool unrouted = std::find(data.route_failed_blocks.begin(), data.route_failed_blocks.end(), block) !=
                                      data.route_failed_blocks.end() &&
                                  std::any_of(data.blocks[block].routes.begin(), data.blocks[block].routes.end(),
                                              [&](const auto& r) { return r.signal == s.name && r.mux < 0; });
            out += format_fields(
                "%c%c", unrouted ? '~' : ' ',
                std::find(fanouts.begin(), fanouts.end(), block) != fanouts.end() ? char('A' + block) : '.');
        }
        std::string paired;
        if (s.p) {
            if (s.p->input_node >= 0) {
                for (const auto& n : data.nodes)
                    if (n.number == s.p->input_node)
                        paired = n.name;
            }
            if (s.p->output_node >= 0) {
                for (const auto& n : data.nodes)
                    if (n.number == s.p->output_node) {
                        if (!n.implied)
                            paired = n.name;
                        else if (feedback_names.count(n.name))
                            paired = feedback_names.at(n.name);
                    }
            }
        } else if (s.n->pin >= 0 && pin(unsigned(s.n->pin)))
            paired = pin(unsigned(s.n->pin))->name;
        if (!paired.empty())
            out += format_fields(" => Paired w/: [%14s]", paired.c_str());
        out += '\n';
    }
    out += dash + "\n" + report_section("< C:\\DAT\\" + image.device + " Device Pin Assignments >");
    out += legacy_pins;
    for (unsigned p = 1; p <= d.pins; ++p) {
        const auto* s   = pin(p);
        const auto it   = std::find(d.io_pins.begin(), d.io_pins.end(), p);
        std::string loc = "   ", kind;
        if (it != d.io_pins.end()) {
            auto io = unsigned(it - d.io_pins.begin());
            loc     = format_fields("%c%02u", 'A' + io / ios, io % ios);
            kind    = " I_O";
        } else if (std::find(d.clock_pins.begin(), d.clock_pins.end(), p) != d.clock_pins.end())
            kind = image.device == "MACH465" ? " CLK" : "CkIn";
        else if (std::find(d.dedicated_pins.begin(), d.dedicated_pins.end(), p) != d.dedicated_pins.end())
            kind = " Inp";
        else
            kind = p < image.pin_signals.size() && image.pin_signals[p] == "VCC"   ? " Vcc"
                   : p < image.pin_signals.size() && image.pin_signals[p] == "GND" ? " GND"
                                                                                   : "Ctrl";
        const bool power = kind == " Vcc" || kind == " GND" || kind == "Ctrl";
        out += format_fields("%5u | %s |%s|%18s |%c|\n", p, kind.c_str(), loc.c_str(),
                             power ? "(pwr/test) "
                             : s   ? s->name.c_str()
                                   : "- ",
                             s && !s->clock_blocks.empty() ? '*' : constraint_marker(s));
    }
    out += dash + "\n";
    for (unsigned b = 0; b < d.blocks; ++b) {
        const bool has_io = data.blocks[b].io_pins || (!modern && data.blocks[b].input_registers);
        if (!data.blocks[b].macrocells && !data.blocks[b].input_registers && !has_io)
            continue;
        const auto prefix = [&](unsigned c, bool terms) {
            const auto* n   = node(b, c);
            const auto* pad = n && !n->input_register && n->pin >= 0 ? pin(unsigned(n->pin)) : nullptr;
            std::string s   = format_fields("%2u|%14s|%s|%c|", c, pad ? pad->name.c_str() : signal_name(n).c_str(),
                                            pad ? pin_type(pad) : signal_type(n), constraint_marker(n));
            if (terms) {
                if (modern || d.device == "MACH215")
                    s += format_fields(" %c |", n && n->asynchronous ? 'A' : 'S');
                if (n) {
                    if (!modern && n->input_register)
                        s += d.device == "MACH215" ? "-1      |" : "    -1      |";
                    else if (modern || d.device == "MACH215")
                        s += n->xor_terms ? format_fields("%2u :+: 1|", n->product_terms)
                                          : format_fields("%2u      |", n->product_terms);
                    else
                        s += format_fields("%6u      |", n->product_terms);
                } else
                    s += (modern || d.device == "MACH215") ? "        |" : "            |";
            }
            return s;
        };
        if (data.blocks[b].macrocells || (!modern && data.blocks[b].input_registers)) {
            out += report_section(blocktitle(b, "Macrocell (MCell) Cluster Assignments"));
            out += modern                  ? modern_clusters
                   : d.device == "MACH215" ? mach215_clusters
                   : d.device[4] == '2'    ? mach2_clusters
                                           : legacy_clusters;
            for (unsigned c = 0; c < d.cells; ++c) {
                out += prefix(c, true);
                unsigned g          = b * d.cells + c;
                int owner           = g < data.group_owner.size() ? data.group_owner[g] : -1;
                const unsigned size = d.device == "MACH215" && (c % 2)                   ? 0
                                      : modern && node(b, c) && node(b, c)->asynchronous ? 2
                                                                                         : 4;
                out += owner < 0 ? format_fields(" %u free   |", size)
                                 : format_fields(" %u to [%2u]|", size, unsigned(owner) % d.cells);
                if (modern) {
                    int aux = g < data.auxiliary_owner.size() ? data.auxiliary_owner[g] : -1;
                    out += aux < 0 ? " 1 XOR free"
                                   : format_fields(" 1 XOR to [%2u]%s", unsigned(aux) % d.cells,
                                                   g < data.auxiliary_data.size() && data.auxiliary_data[g]
                                                       ? (node(b, unsigned(aux) % d.cells) &&
                                                                  node(b, unsigned(aux) % d.cells)->product_terms == 1
                                                              ? " for 1 PT sig"
                                                              : " as logic PT")
                                                       : "");
                } else if (d.device[4] != '2') {
                    const auto* n = node(b, c);
                    const bool oe = n && !n->oe_expression.empty();
                    if (oe && n->oe_key >= 0 && product_ids.count(n->oe_expression))
                        out += format_fields("%3d =>%3u |", n->oe_key, product_ids.at(n->oe_expression));
                    else
                        out += " -  => -  |";
                    const unsigned first = c < d.cells / 2 ? 0 : 2;
                    for (unsigned slot = first; slot < first + 2; ++slot)
                        out += oe && n->pin >= 0 && n->oe_slot == int(slot) ? format_fields("(%2u)", slot)
                                                                            : format_fields(" %2u ", slot);
                }
                out += '\n';
            }
            out += dash + "\n";
            if (data.successful) {
                out += report_section(blocktitle(b, "Maximum PT Capacity"));
                out += modern ? modern_capacity : d.device == "MACH215" ? mach215_capacity : legacy_capacity;
                for (unsigned c = 0; c < d.cells; ++c) {
                    const auto* n     = node(b, c);
                    unsigned capacity = data.blocks[b].available_terms.at(c) + (n ? n->product_terms : 0);
                    if (modern) {
                        capacity = 0;
                        for (int delta : {0, 1, 2, -1}) {
                            const int local = int(c) + delta;
                            if (local < 0 || local >= int(d.cells))
                                continue;
                            const unsigned g = b * d.cells + unsigned(local);
                            const int target = int(b * d.cells + c);
                            const int main = data.group_owner.at(g), aux = data.auxiliary_owner.at(g);
                            const bool allowed = main < 0 || main == target;
                            if (allowed)
                                capacity += node(b, unsigned(local)) && node(b, unsigned(local))->asynchronous ? 2 : 4;
                            if (aux == target || (aux < 0 && (allowed || (local == int(c) && !n))))
                                ++capacity;
                        }
                        if (n && n->xor_terms && capacity)
                            --capacity;
                    }
                    out += prefix(c, true) + (capacity
                                                  ? format_fields("=> can support up to [%3u] logic PT(s)\n", capacity)
                                                  : format_fields("=> [%3u] PT capacity\n", capacity));
                }
                out += dash + "\n";
            }
            out += report_section(blocktitle(b, "Node-Pin Assignments"));
            std::string head           = legacy_nodepins;
            head[head.find("[A]") + 1] = char('A' + b);
            out += head;
            for (unsigned c = 0; c < d.cells; ++c) {
                const auto* n = node(b, c);
                std::vector<unsigned> local, physical;
                if (!modern && d.device[4] == '2' && (c % 2)) {
                    out += prefix(c, false) + " => |\t(internal node)\n";
                    continue;
                }
                if (modern) {
                    for (unsigned k = 0; k < 4; ++k)
                        local.push_back((c / 2 + ios - 3 + k) % ios);
                } else if (d.device[4] == '2' && d.device != "MACH215")
                    local.push_back(c / 2);
                else
                    local.push_back(d.device == "MACH215" ? c / 2 : c);
                for (auto io : local)
                    physical.push_back(d.io_pins[b * ios + io]);
                int used = -1;
                if (n && n->pin >= 0) {
                    auto it = std::find(physical.begin(), physical.end(), unsigned(n->pin));
                    if (it != physical.end())
                        used = int(local[unsigned(it - physical.begin())]);
                }
                out += prefix(c, false) + " => |" + format_fields("%-20s", format_positions(local, used).c_str()) +
                       "|" + format_positions(physical, n ? n->pin : -1) + "\n";
            }
            out += dash + "\n";
        }
        if (has_io) {
            out += report_section(blocktitle(b, "IO-to-Node Pin Mapping"));
            out += modern ? modern_pinnodes : legacy_pinnodes;
            for (unsigned i = 0; i < ios; ++i) {
                auto p        = d.io_pins[b * ios + i];
                const auto* s = pin(p);
                out += format_fields("%2u|%14s|%s|%c|%3u| => |", i, signal_name(s).c_str(), pin_type(s),
                                     constraint_marker(s), p);
                std::vector<unsigned> dest;
                if (modern)
                    for (unsigned k = 0; k < 8; ++k)
                        dest.push_back((2 * i + k) % d.cells);
                else if (d.device[4] == '2' && d.device != "MACH215") {
                    dest = {2 * i};
                } else
                    dest = {d.device == "MACH215" ? 2 * i : i};
                int used = -1;
                if (s && s->output_node >= 0)
                    used = (s->output_node - 2) % int(d.cells);
                for (auto value : dest)
                    out += int(value) == used ? format_fields(" (%2u)", value) : format_fields(" %3u ", value);
                out += "\n";
            }
            out += dash + "\n" + report_section(blocktitle(b, "IO/Node and IO/Input Macrocell Pairing Table"));
            out += legacy_pairs;
            for (unsigned i = 0; i < ios; ++i) {
                auto p        = d.io_pins[b * ios + i];
                const auto* s = pin(p);
                const auto* r = reg(b, i);
                out += format_fields("%2u|%14s|%s|%c|%3u| => | Input macrocell   [%14s]\n", i, signal_name(s).c_str(),
                                     pin_type(s), constraint_marker(s), p, r ? r->name.c_str() : "-");
                if (s) {
                    std::string pair;
                    for (const auto& n : data.nodes)
                        if (n.number == s->output_node && !n.input_register) {
                            if (!n.implied)
                                pair = n.name;
                            else if (feedback_names.count(n.name))
                                pair = feedback_names.at(n.name);
                        }
                    if (!pair.empty())
                        out +=
                            format_fields("  |              |   | |   |    | IO paired w/ node [%14s]\n", pair.c_str());
                }
            }
        }
        if (has_io)
            out += dash + "\n";
        out += report_section(blocktitle(b, "Input Multiplexer (IMX) Assignments"));
        out += legacy_imx;
        const auto imxline = [&](const char* label, unsigned local, unsigned number, const std::string& nm,
                                 const char* kind, char fix, bool live, const std::string& paired) {
            std::string s = format_fields("[%s %2u |%3u|%s %14s|%c|%c]", label, local, number, kind,
                                          nm.empty() ? "-" : nm.c_str(), fix, live ? '*' : ' ');
            if (!paired.empty())
                s += format_fields(" paired w/[%14s]", paired.c_str());
            return s + "\n";
        };
        for (unsigned i = 0; i < ios; ++i) {
            auto p        = d.io_pins[b * ios + i];
            const auto* s = pin(p);
            out += format_fields("IMX %2u\t", i) +
                   imxline("IOpin", i, p, signal_name(s), s ? pin_type(s) : "   ", constraint_marker(s),
                           s && !s->fanout_blocks.empty(),
                           s && feedback_names.count(s->name) ? feedback_names.at(s->name)
                           : s && s->output_node >= 0 && node(b, unsigned(s->output_node - 2) % d.cells) &&
                                   !node(b, unsigned(s->output_node - 2) % d.cells)->implied &&
                                   !node(b, unsigned(s->output_node - 2) % d.cells)->input_register
                               ? node(b, unsigned(s->output_node - 2) % d.cells)->name
                               : "");
            if (modern) {
                const auto* r = reg(b, i);
                out += '\t' + imxline("RegIn", i, d.blocks * d.cells + b * ios + i + 2, signal_name(r),
                                      r ? signal_type(r) : "   ", constraint_marker(r), r && !r->fanout_blocks.empty(),
                                      r && s ? s->name : "");
            }
            unsigned count = modern || d.device[4] == '2' ? 2 : 1;
            for (unsigned k = 0; k < count; ++k) {
                unsigned c    = count * i + k;
                const auto* n = node(b, c);
                std::string paired;
                if (n && (!n->implied || feedback_names.count(n->name)) && n->pin >= 0 && pin(unsigned(n->pin)))
                    paired = pin(unsigned(n->pin))->name;
                out += '\t' + imxline("MCell", c, b * d.cells + c + 2,
                                      n && feedback_names.count(n->name) ? feedback_names.at(n->name) : signal_name(n),
                                      n ? (feedback_names.count(n->name) ? "NOD" : signal_type(n)) : "   ",
                                      constraint_marker(n),
                                      n && !n->fanout_blocks.empty() && (!n->implied || n->logic != 'C'), paired);
            }
            if (i + 1 < ios)
                out += '\n';
        }
        out += dash + "\n";
        if (data.blocks[b].fanin && data.placement_failed_blocks.empty()) {
            out += report_section(blocktitle(b, "Logic Array Fan-in"));
            if (std::find(data.route_failed_blocks.begin(), data.route_failed_blocks.end(), b) !=
                data.route_failed_blocks.end()) {
                std::vector<std::string> missing;
                for (const auto& r : data.blocks[b].routes)
                    if (r.mux < 0)
                        missing.push_back(r.signal);
                out += format_fields("*** [%2u] Unrouted Signals\n", unsigned(missing.size()));
                for (const auto& signal : missing)
                    out += "\t[" + signal + "]\n";
            }
            out += legacy_fanin;
            for (unsigned m = 0; m < d.muxes; ++m) {
                const FitterReportRoute* r = nullptr;
                for (const auto& x : data.blocks[b].routes)
                    if (x.mux == int(m))
                        r = &x;
                if (!r) {
                    out += format_fields("Mux%02u|            ...         ...  |\n", m);
                    continue;
                }
                const auto ni             = byname.find(r->signal);
                const FitterReportNode* n = ni == byname.end() ? nullptr : ni->second;
                const FitterReportPin* p  = r->source_pin >= 0 ? pin(unsigned(r->source_pin)) : nullptr;
                if (n && (n->input_register || r->source_cell >= 0)) {
                    out += format_fields(
                        "Mux%02u| %14s  %s %c %2d | %4d\n", m,
                        (feedback_names.count(r->signal) ? feedback_names.at(r->signal) : r->signal).c_str(),
                        modern && n->input_register ? "RegInp" : " Mcell", 'A' + n->block, n->cell, n->number);
                } else if (p && p->block >= 0) {
                    auto it = std::find(d.io_pins.begin(), d.io_pins.end(), p->number);
                    out += format_fields("Mux%02u| %14s  IO Pin %c %2u | %4u\n", m, r->signal.c_str(), 'A' + p->block,
                                         unsigned(it - d.io_pins.begin()) % ios, p->number);
                } else
                    out += format_fields("Mux%02u| %14s   Input Pin  | %4d\n", m, r->signal.c_str(), r->source_pin);
            }
            out += dash + "\n";
        }
    }
    if (!out.empty() && out.back() == '\n')
        out.pop_back();
    std::string dos;
    dos.reserve(out.size() + 1000);
    for (char c : out) {
        if (c == '\n')
            dos += '\r';
        dos += c;
    }
    return dos;
}
}  // namespace machxl
