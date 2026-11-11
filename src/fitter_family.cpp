#include "fitter_family.hpp"
#include "device_family.hpp"
#include "fitter_budget.hpp"
#include "fitter_failure_report.hpp"
#include "fitter_family_place.hpp"
#include "fitter_inputs.hpp"
#include "fitter_legacy_failure_report.hpp"
#include "fitter_live.hpp"
#include "fitter_metadata.hpp"
#include "fitter_order.hpp"
#include "fitter_route.hpp"
#include "simulation_products.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <sstream>

namespace machxl {
namespace {
[[noreturn]] void unsupported(const std::string& s)
{
    throw LogicError("MACH1/2 fitter: " + s);
}
struct Pla
{
    std::vector<std::string> inputs, outputs;
    std::vector<std::vector<std::string>> terms;
};
Pla synthesize_pla(Design& design, OptimizerOptions options, FitterReportData* report = nullptr)
{
    Pla p;
    unsigned ni = 0, no = 0;
    bool products = false;
    std::string line, labels;
    const auto pla = emit_pla(design, "design.pds", options, &design);
    if (report) {
        report->optimized_pla    = pla;
        report->optimized_design = std::make_shared<Design>(design);
    }
    std::istringstream text(pla);
    while (std::getline(text, line)) {
        std::istringstream fields(line);
        std::string token;
        if (!(fields >> token) || token[0] == '#')
            continue;
        if (token[0] == '.') {
            labels.clear();
            if (token == ".i")
                fields >> ni;
            else if (token == ".o") {
                fields >> no;
                p.terms.resize(no);
            } else if (token == ".ilb" || token == ".ob")
                labels = token;
            else if (token == ".p")
                products = true;
            else if (token == ".e")
                break;
            if (labels.empty())
                continue;
        } else if (products) {
            std::string pattern, outputs;
            if (ni) {
                pattern = token;
                fields >> outputs;
            } else
                outputs = token;
            if (pattern.size() != ni || outputs.size() != no)
                unsupported("invalid synthesized PLA");
            for (unsigned out = 0; out < no; ++out)
                if (outputs[out] == '1' && (ni || p.terms[out].empty()))
                    p.terms[out].push_back(pattern);
            continue;
        } else if (!labels.empty())
            (labels == ".ilb" ? p.inputs : p.outputs).push_back(token);
        if (!labels.empty())
            while (fields >> token)
                (labels == ".ilb" ? p.inputs : p.outputs).push_back(token);
    }
    if (p.inputs.size() != ni || p.outputs.size() != no)
        unsupported("invalid synthesized PLA labels");
    std::map<std::string, std::string> original_names;
    for (const auto& alias : pla_signal_aliases(design))
        original_names.emplace(alias.second, alias.first);
    for (auto& input : p.inputs)
        input = original_names.at(input);
    for (auto& output : p.outputs) {
        auto length = output.find('.');
        if (length == std::string::npos) {
            length = output.size();
            if (length && (output.back() == '-' || output.back() == '%'))
                --length;
        }
        output = original_names.at(output.substr(0, length)) + output.substr(length);
    }
    return p;
}
}  // namespace
namespace {
struct FamilyRouteRetry
{
    int signal = -1;
};
FuseImage fit_family_prepared(const Design& original_design, const Pla& prepared, FitterOptions fitting,
                              const FamilyPlacement* forced = nullptr, const std::vector<bool>* movable = nullptr)
{
    Design design      = original_design;
    const auto& d      = mach_device_layout(design.device);
    const bool mach215 = d.device == "MACH215";
    const bool mach2   = mach215 || d.device == "MACH210" || d.device == "MACH211" || d.device == "MACH220" ||
                         d.device == "MACH221" || d.device == "MACH231";
    if (!mach2 && d.device != "MACH110" && d.device != "MACH120" && d.device != "MACH121" && d.device != "MACH130" &&
        d.device != "MACH131")
        unsupported("configuration rules are not implemented for " + d.device);
    auto p = prepared;

    GlobalNodeUse global_node;
    expand_global_node(design, p.inputs, p.outputs, p.terms, &global_node);
    if (global_node.reset_overridden)
        fitting.warn("z5037: Invalid global reset equation.");
    discard_combinatorial_storage_controls(p.outputs, p.terms);
    validate_node_output_enables(design, p.outputs);
    std::vector<std::string> unreferenced_nodes;
    validate_node_equations(design, p.inputs, p.outputs, &unreferenced_nodes);
    for (const auto& warning : discarded_input_pairs(design))
        fitting.warn(warning);
    for (const auto& name : unreferenced_nodes)
        fitting.warn("z5071: Nodes not referenced - discarded: " + name);
    for (const auto& warning :
         preplaced_fanin_warnings(p.inputs, p.outputs, p.terms, d.muxes,
                                  [&](const std::string& name) { return fixed_block_of(design, d, name); }))
        fitting.warn(warning);
    validate_equation_fanin(p.inputs, p.outputs, p.terms, d.muxes);
    validate_storage_control_products(p.outputs, p.terms);
    check_fitting_budget();
    if (design.dos_compatibility && p.inputs.empty() && p.outputs.size() == 1)
        unsupported("original single constant PLA omits the input label directive");
    if (design.dos_compatibility && p.inputs.empty() &&
        std::all_of(p.terms.begin(), p.terms.end(), [](const auto& terms) { return terms.empty(); }))
        unsupported("z5015: original constant PLA has zero product terms");
    if (fitting.use_saved_placement || !fitting.saved_placement.empty())
        apply_saved_placement(design, fitting.saved_placement);
    apply_preplacement_policy(design, fitting.preplacement_policy);
    validate_fixed_locations(design);
    if (fitting.report)
        fitting.report->optimized_design = std::make_shared<Design>(design);
    std::map<std::string, int> constrained_blocks;
    std::set<std::string> low_power;
    for (const auto& group : design.groups) {
        if (group.first == "LOW_POWER_LIST")
            low_power.insert(group.second.begin(), group.second.end());
        if (group.first.compare(0, 9, "MACH_SEG_") == 0) {
            if (group.first.size() != 10 || group.first.back() < 'A' || unsigned(group.first.back() - 'A') >= d.blocks)
                unsupported("invalid block constraint " + group.first);
            const int block = group.first.back() - 'A';
            for (const auto& name : group.second) {
                const auto previous = constrained_blocks.emplace(name, block);
                if (!previous.second && previous.first->second != block)
                    unsupported("conflicting block constraints for " + name);
            }
        }
        // Ordinary GROUPs name vectors already expanded by the frontend.
        // MACH1/2 have no selectable synchronous/asynchronous allocation.
    }
    std::map<std::string, std::string> input_pairs;
    if (mach2 && fitting.automatic_legacy_input_pairing) {
        std::set<std::string> paired, driven;
        for (const auto& pin : design.pins)
            if (!pin.pair.empty()) {
                paired.insert(pin.name);
                paired.insert(pin.pair);
            }
        for (const auto& equation : design.equations)
            if (equation.function == "COM" || equation.function == "REG" || equation.function == "G" ||
                equation.function == "T")
                driven.insert(equation.signal);
        for (unsigned column = 0; column < p.outputs.size(); ++column) {
            auto label = p.outputs[column];
            if (!label.empty() && (label.back() == '-' || label.back() == '%'))
                label.pop_back();
            const auto dot = label.find('.');
            if (dot == std::string::npos)
                continue;
            const auto name = label.substr(0, dot), function = label.substr(dot + 1);
            if (function != "REG" && function != "G" && function != "T")
                continue;
            auto node =
                std::find_if(design.pins.begin(), design.pins.end(), [&](const auto& pin) { return pin.name == name; });
            if (node == design.pins.end() || !node->node || node->number || paired.count(name))
                continue;
            const auto& terms = p.terms[column];
            const auto bit    = terms.size() == 1 ? terms[0].find_first_not_of('-') : std::string::npos;
            if (bit == std::string::npos || terms[0].find_first_not_of('-', bit + 1) != std::string::npos)
                continue;
            auto pad = std::find_if(design.pins.begin(), design.pins.end(),
                                    [&](const auto& pin) { return pin.name == p.inputs[bit]; });
            if (pad == design.pins.end() || pad->node || paired.count(pad->name) || driven.count(pad->name))
                continue;
            if (pad->number && std::find(d.io_pins.begin(), d.io_pins.end(), pad->number) == d.io_pins.end())
                continue;
            pad->pair      = name;
            pad->pair_kind = "IPAIR";
            paired.insert(name);
            paired.insert(pad->name);
        }
    }
    for (const auto& pad : design.pins)
        if (!pad.node && !pad.pair.empty()) {
            // MACH1 has no dedicated input register; original MACHBPP
            // retains its NODE equation as independent macrocell logic.
            if (!mach2)
                continue;
            auto node = std::find_if(design.pins.begin(), design.pins.end(),
                                     [&](const auto& pin) { return pin.name == pad.pair; });
            if (node == design.pins.end() || !node->node)
                unsupported("invalid input pair " + pad.name);
            if (!input_pairs.emplace(node->name, pad.name).second)
                unsupported("duplicate input pair");
            if (pad.number) {
                const auto io = std::find(d.io_pins.begin(), d.io_pins.end(), pad.number);
                if (io == d.io_pins.end())
                    unsupported("input pair requires an I/O pad");
                const unsigned number = 3 + 2 * unsigned(io - d.io_pins.begin());
                if (node->number && node->number != number)
                    unsupported("conflicting input pair node");
                node->number = number;
            }
        }
    if (mach215)
        for (const auto& pin : design.pins)
            if (pin.node && pin.number && (pin.number & 1) && !input_pairs.count(pin.name))
                unsupported("MACH215 odd buried cell");
    if (p.outputs.empty())
        unsupported("output-free design");
    // Single-literal clock polarity is checked before unused-node removal.
    // Complex clock shape and dedicated-pin checks apply after pruning.
    if (!mach215) {
        std::map<std::string, bool> storage;
        for (const auto& equation : design.equations)
            if (equation.function == "G" || equation.function == "REG" || equation.function == "T")
                storage[equation.signal] = equation.function == "G";
        for (unsigned out = 0; out < p.outputs.size(); ++out) {
            const auto& label = p.outputs[out];
            if (label.size() < 2 || label.substr(label.size() - 2) != ".C" ||
                !storage.count(label.substr(0, label.size() - 2)))
                continue;
            const bool latched = storage.at(label.substr(0, label.size() - 2));
            const auto input   = p.terms[out].size() == 1 ? p.terms[out][0].find_first_not_of('-') : std::string::npos;
            if (input == std::string::npos || p.terms[out][0].find_first_not_of('-', input + 1) != std::string::npos)
                continue;
            const auto pin = std::find_if(design.pins.begin(), design.pins.end(),
                                          [&](const auto& item) { return item.name == p.inputs[input]; });
            if (pin != design.pins.end() && ((p.terms[out][0][input] == '0') != pin->inverted) != latched)
                unsupported("z5093: invalid register clock polarity");
            if (latched && !mach2)
                unsupported("latch on a MACH1 device");
        }
    }
    // The original classifies clock signals before it removes unused buried
    // equations. A signal fixed on a dedicated clock pin therefore keeps its
    // pin, and its place in the reports, even when the only register it
    // clocked is removed. Observed on MACH2; MACH111 drops such a signal.
    std::set<std::string> fixed_clock_inputs, retained_clock_pins;
    if (mach2 && !mach215)
        for (unsigned out = 0; out < p.outputs.size(); ++out) {
            const auto& label = p.outputs[out];
            if (label.size() < 2 || label.substr(label.size() - 2) != ".C" || p.terms[out].size() != 1)
                continue;
            const auto input = p.terms[out][0].find_first_not_of('-');
            if (input == std::string::npos || p.terms[out][0].find_first_not_of('-', input + 1) != std::string::npos)
                continue;
            const auto pin = std::find_if(design.pins.begin(), design.pins.end(),
                                          [&](const auto& item) { return item.name == p.inputs[input]; });
            if (pin != design.pins.end() && !pin->node &&
                std::find(d.clock_pins.begin(), d.clock_pins.end(), pin->number) != d.clock_pins.end())
                fixed_clock_inputs.insert(pin->name);
        }
    const auto retain_removed_clocks = [&](std::set<std::string>& active) {
        for (const auto& name : fixed_clock_inputs)
            if (active.insert(name).second)
                retained_clock_pins.insert(name);
    };
    std::vector<std::string> discarded_nodes;
    const auto reachable = retain_reachable_equations(design, p.inputs, p.outputs, p.terms, &discarded_nodes);
    for (const auto& name : discarded_nodes)
        fitting.warn("z5071: Nodes not referenced - discarded: " + name);
    for (const auto& name : unreferenced_pins(design, p.inputs, p.outputs, fixed_clock_inputs))
        fitting.warn("z5073: Input Pins not referenced - discarded: " + name);
    std::map<std::string, unsigned> signal_ids, original_feedback_ids;
    std::map<std::string, const Equation*> primary;
    for (const auto& e : design.equations)
        if (reachable.count(e.signal) &&
            (e.function == "COM" || e.function == "REG" || e.function == "T" || e.function == "G"))
            primary[e.signal] = &e;
    std::map<std::string, std::string> output_aliases;
    std::map<std::string, bool> alias_phases;
    std::map<std::string, unsigned> paired_fixed_pins;
    std::set<std::string> paired_pins;
    std::map<std::string, unsigned> primary_columns;
    for (unsigned out = 0; out < p.outputs.size(); ++out) {
        auto label = p.outputs[out];
        if (label.back() == '-')
            label.pop_back();
        const auto dot = label.find('.');
        const auto fn  = dot == std::string::npos ? "COM" : label.substr(dot + 1);
        if (fn == "COM" || fn == "REG" || fn == "T" || fn == "G")
            primary_columns[label.substr(0, dot)] = out;
    }
    for (const auto& pin : design.pins)
        if (pin.node && !pin.pair.empty() && reachable.count(pin.name)) {
            const auto target = std::find_if(design.pins.begin(), design.pins.end(),
                                             [&](const auto& other) { return other.name == pin.pair; });
            if (!pin.node || pin.pair_kind == "IPAIR" || target == design.pins.end() || target->node)
                unsupported("input-register pairing " + pin.name);
            if (pin.number) {
                // MACHBPP discards an output pair on a MACH2 odd input
                // macrocell; its independent NODE equation still fits there.
                if (mach2 && !mach215 && ((pin.number - 2) & 1))
                    continue;
                if (pin.number < 2 || pin.number >= 2 + d.blocks * d.cells || (mach2 && ((pin.number - 2) & 1)))
                    unsupported("output pair node has no output pad");
                const auto physical_pin = d.io_pins.at((pin.number - 2) / (mach2 ? 2 : 1));
                if (target->number && target->number != physical_pin)
                    unsupported("conflicting output pair constraints");
                paired_fixed_pins[pin.pair] = physical_pin;
            }
            if (!primary_columns.count(pin.name) || !primary_columns.count(pin.pair))
                unsupported("output pair has no equation");
            const auto node = primary_columns.at(pin.name), pad = primary_columns.at(pin.pair);
            if (primary.at(pin.name)->function != primary.at(pin.pair)->function || p.terms[node] != p.terms[pad])
                unsupported("output pair has different excitation equations");
            alias_phases[pin.name]   = p.outputs[node].back() == '-';
            output_aliases[pin.name] = pin.pair;
            // The node of an output pair is only another name for the pad's
            // macrocell. If nothing reads it, the original discards it.
            if (std::find(p.inputs.begin(), p.inputs.end(), pin.name) == p.inputs.end())
                fitting.warn("z5071: Nodes not referenced - discarded: " + pin.name);
            paired_pins.insert(pin.pair);
        }
    for (unsigned out = 0; out < p.outputs.size();) {
        auto label = p.outputs[out];
        if (label.back() == '-')
            label.pop_back();
        const auto dot   = label.find('.');
        const auto alias = output_aliases.find(label.substr(0, dot));
        if (alias != output_aliases.end()) {
            const auto function = dot == std::string::npos ? "COM" : label.substr(dot + 1);
            if (function == "C" || function == "RE" || function == "PR" || function == "OE") {
                const auto target   = alias->second + "." + function;
                const auto existing = std::find(p.outputs.begin(), p.outputs.end(), target);
                if (existing == p.outputs.end()) {
                    p.outputs[out] = target;
                    ++out;
                    continue;
                }
                if (p.terms[unsigned(existing - p.outputs.begin())] != p.terms[out])
                    unsupported("output pair has different " + function + " controls");
            }
            p.outputs.erase(p.outputs.begin() + out);
            p.terms.erase(p.terms.begin() + out);
        } else
            ++out;
    }
    std::vector<Pin*> declared_pins;
    for (auto& pin : design.pins)
        declared_pins.push_back(&pin);
    std::stable_sort(declared_pins.begin(), declared_pins.end(), [&](const auto* a, const auto* b) {
        if (bool(a->number) != bool(b->number))
            return bool(a->number);
        return a->number && (a->number + (a->node ? d.pins : 0)) < (b->number + (b->node ? d.pins : 0));
    });
    unsigned next_id = d.pins + d.blocks * d.cells + 1;
    FamilyPlacement placement;
    std::map<std::string, std::size_t> placement_signals;
    std::map<std::string, std::int16_t> routing_fixed_nodes;
    std::vector<std::string> placement_names;
    std::set<std::string> global_clock_names;
    bool reserve_default_clock = false;
    std::vector<std::string> floating_input_order;
    std::map<std::string, std::vector<unsigned>> floating_input_candidates;
    std::map<std::string, int> placed_oe_slots, placed_oe_keys;
    std::vector<bool> placed_sequential_search(d.blocks, false);
    for (const auto* pin : declared_pins) {
        signal_ids[pin->name] = pin->number ? pin->number - 1 + (pin->node ? d.pins : 0) : next_id++;
    }
    for (const auto* pin : declared_pins)
        if (!pin->node && !paired_pins.count(pin->name) && primary.count(pin->name) &&
            primary.at(pin->name)->function != "COM" &&
            std::find(p.inputs.begin(), p.inputs.end(), pin->name) != p.inputs.end())
            original_feedback_ids[pin->name] = next_id++;
    {
        std::map<std::string, std::vector<unsigned>> outputs;
        std::set<std::string> clocks;
        if (!mach215)
            for (const auto& output : primary)
                if (!output_aliases.count(output.first) && output.second->function != "COM" &&
                    std::find(p.outputs.begin(), p.outputs.end(), output.first + ".C") == p.outputs.end())
                    reserve_default_clock = true;
        if (mach215) {
            if (fitting.reduce_global_clocks > 2)
                unsupported("global-clock reduction must be 0, 1 or 2");
            struct Candidate
            {
                std::string name;
                unsigned users = 0;
                bool forced = false, excluded = false;
            };
            std::map<std::string, Candidate> candidates;
            std::set<std::string> clocked;
            for (unsigned out = 0; out < p.outputs.size(); ++out) {
                const auto& label = p.outputs[out];
                if (label.size() < 2 || label.substr(label.size() - 2) != ".C")
                    continue;
                const auto output = label.substr(0, label.size() - 2);
                clocked.insert(output);
                const auto& terms = p.terms[out];
                const auto input  = terms.size() == 1 ? terms[0].find_first_not_of('-') : std::string::npos;
                if (input == std::string::npos || terms[0].find_first_not_of('-', input + 1) != std::string::npos)
                    continue;
                const auto& name = p.inputs[input];
                auto& c          = candidates[name];
                c.name           = name;
                ++c.users;
                const auto pin = std::find_if(declared_pins.begin(), declared_pins.end(),
                                              [&](const auto* pin) { return pin->name == name; });
                if (pin == declared_pins.end())
                    unsupported("undeclared clock");
                const bool fixed_global = (*pin)->number && std::find(d.clock_pins.begin(), d.clock_pins.end(),
                                                                      (*pin)->number) != d.clock_pins.end();
                c.excluded |= (*pin)->node || primary.count(name) || constrained_blocks.count(name) ||
                              ((*pin)->number && !fixed_global);
                c.forced |= fixed_global || input_pairs.count(output);
            }
            for (const auto& output : primary)
                if (!output_aliases.count(output.first) && output.second->function != "COM" &&
                    !clocked.count(output.first))
                    reserve_default_clock = true;
            std::vector<Candidate> optional;
            for (const auto& item : candidates)
                if (!item.second.excluded) {
                    if (item.second.forced)
                        clocks.insert(item.first);
                    else
                        optional.push_back(item.second);
                }
            std::stable_sort(optional.begin(), optional.end(), [&](const auto& a, const auto& b) {
                return a.users != b.users ? a.users > b.users : signal_ids.at(a.name) < signal_ids.at(b.name);
            });
            const auto capacity = unsigned(d.clock_pins.size()) - fitting.reduce_global_clocks;
            for (const auto& candidate : optional)
                if (clocks.size() + unsigned(reserve_default_clock) < capacity)
                    clocks.insert(candidate.name);
            global_clock_names = clocks;
        }
        for (unsigned out = 0; out < p.outputs.size(); ++out) {
            auto name = p.outputs[out];
            if (name.back() == '-' || name.back() == '%')
                name.pop_back();
            const auto dot = name.find('.');
            const auto fn  = dot == std::string::npos ? "COM" : name.substr(dot + 1);
            if (dot != std::string::npos)
                name.resize(dot);
            if (fn == "C") {
                const auto input =
                    p.terms[out].size() == 1 ? p.terms[out][0].find_first_not_of('-') : std::string::npos;
                const bool single  = input != std::string::npos &&
                                     p.terms[out][0].find_first_not_of('-', input + 1) == std::string::npos;
                unsigned clock_pin = 0;
                if (single) {
                    if (!mach215)
                        clocks.insert(p.inputs[input]);
                    for (const auto* pin : declared_pins)
                        if (pin->name == p.inputs[input])
                            clock_pin = pin->number;
                }
                if (mach215 &&
                    (!single || !clocks.count(p.inputs[input]) || (clock_pin && clock_pin != d.clock_pins.front())))
                    outputs[name].push_back(out);
            } else
                outputs[name].push_back(out);
        }
        const auto aliases = pla_signal_aliases(design);
        std::map<std::vector<std::string>, std::uint16_t> control_ids;
        std::vector<Mach111PlaceSignal> signals;
        std::vector<Pin*> included;
        for (auto* pin : declared_pins) {
            if (output_aliases.count(pin->name))
                continue;
            const bool input_register = input_pairs.count(pin->name);
            const bool output         = primary.count(pin->name) && !input_register;
            if (!output && std::find(p.inputs.begin(), p.inputs.end(), pin->name) == p.inputs.end())
                continue;
            Mach111PlaceSignal signal;
            signal.clocked_storage = output && primary.at(pin->name)->function != "COM";
            signal.name            = pin->name;
            signal.sort_name       = aliases.at(pin->name);
            signal.output          = output;
            signal.buried          = pin->node;
            signal.input_register  = input_register;
            signal.order.serial    = std::int16_t(signal_ids.at(pin->name));
            signal.order.type = input_register ? 8 : output ? (pin->node ? 3 : 0) : clocks.count(pin->name) ? 9 : 1;
            for (unsigned out = 0; out < p.outputs.size(); ++out)
                if (p.outputs[out] == pin->name + ".C" && p.terms[out].size() == 1) {
                    const auto& term   = p.terms[out][0];
                    const auto literal = term.find_first_not_of('-');
                    if (literal != std::string::npos && term.find_first_not_of('-', literal + 1) == std::string::npos) {
                        signal.clock_fanin    = std::uint16_t(signal_ids.at(p.inputs[literal]));
                        signal.clock_positive = term[literal] == '1';
                    }
                }
            if (constrained_blocks.count(pin->name))
                signal.order.block = std::int16_t(constrained_blocks.at(pin->name));
            if (pin->number) {
                if (pin->node) {
                    signal.order.fixed_node = std::int16_t(pin->number + d.pins - 1);
                    signal.fixed_cell       = int(pin->number) - 2;
                } else
                    signal.order.fixed_pin = std::int16_t(pin->number - 1);
            } else if (paired_fixed_pins.count(pin->name))
                signal.order.fixed_pin = std::int16_t(paired_fixed_pins.at(pin->name) - 1);
            if (signal.order.fixed_node >= 0)
                routing_fixed_nodes[pin->name] = signal.order.fixed_node;
            if (output) {
                std::set<std::uint16_t> inputs;
                for (auto out : outputs[pin->name]) {
                    const auto& label  = p.outputs[out];
                    const auto dot     = label.find('.');
                    const auto fn      = dot == std::string::npos ? "COM" : label.substr(dot + 1);
                    const bool control = fn == "RE" || fn == "PR" || fn == "OE" || fn == "C";
                    if (fn == "OE" || fn == "RE" || fn == "PR")
                        signal.explicit_async_control = true;
                    if (!control)
                        signal.order.product_terms = std::int16_t(std::max<std::size_t>(1, p.terms[out].size()));
                    if (fn == "OE" && !p.terms[out].empty() &&
                        p.terms[out][0].find_first_not_of('-') != std::string::npos) {
                        signal.oe_members = 1;
                        signal.oe_product = p.terms[out][0];
                    }
                    if (fn == "RE" || fn == "PR") {
                        // Retry ranking phases two and three distinguish explicit GND from
                        // an absent control.
                        const auto id = p.terms[out].empty()
                                            ? std::uint16_t(0)
                                            : control_ids.emplace(p.terms[out], std::uint16_t(control_ids.size() + 1))
                                                  .first->second;
                        (fn == "RE" ? signal.reset : signal.preset) = id;
                    }
                    for (const auto& term : p.terms[out])
                        for (unsigned input = 0; input < term.size(); ++input)
                            if (term[input] != '-') {
                                const auto& name = p.inputs[input];
                                const auto id    = original_feedback_ids.count(name) ? original_feedback_ids.at(name)
                                                                                     : signal_ids.at(name);
                                inputs.insert(std::uint16_t(id));
                            }
                }
                signal.fanins.assign(inputs.begin(), inputs.end());
            }
            signals.push_back(signal);
            included.push_back(pin);
        }
        for (auto& signal : signals)
            if (signal.input_register) {
                const auto& pad = input_pairs.at(signal.name);
                const auto found =
                    std::find_if(signals.begin(), signals.end(), [&](const auto& item) { return item.name == pad; });
                if (found == signals.end())
                    unsupported("input pair pad missing");
                signal.paired_input = int(found - signals.begin());
            }
        for (std::size_t i = 0; i < signals.size(); ++i) {
            placement_signals[signals[i].name] = i;
            placement_names.push_back(signals[i].name);
        }
        if (forced)
            placement = *forced;
        else {
            std::optional<FuseImage> result;
            Mach111PlaceOptions place_options{fitting.balanced_partition, fitting.spread_placement,
                                              reserve_default_clock, false, fitting.iterate_partition};
            place_options.collect_report_visits = bool(fitting.report);
            if (fitting.report)
                place_options.partition_observer = [&](const auto& observed, const auto& mapping, const auto& frame) {
                    if (frame.failed_cluster)
                        capture_partition_failure_report(*fitting.report, observed, mapping, frame, signal_ids,
                                                         original_feedback_ids);
                };
            if (fitting.report) {
                for (const auto& entry : signal_ids)
                    if (output_aliases.count(entry.first)) {
                        FitterOrderKey key;
                        key.serial     = std::int16_t(entry.second);
                        key.type       = 3;
                        const auto pin = std::find_if(declared_pins.begin(), declared_pins.end(),
                                                      [&](const auto* p) { return p->name == entry.first; });
                        if (pin != declared_pins.end() && (*pin)->node && (*pin)->number)
                            key.fixed_node = std::int16_t(entry.second);
                        place_options.report_extra_records.push_back(key);
                    }
                for (const auto& entry : original_feedback_ids) {
                    FitterOrderKey key;
                    key.serial = std::int16_t(entry.second);
                    key.type   = 3;
                    place_options.report_extra_records.push_back(key);
                }
            }
            std::function<void(const FamilyPlacement&, unsigned, const std::vector<bool>&)> failure;
            if (fitting.report)
                failure = [&](const auto& frame, unsigned block, const auto& assigned) {
                    if (!fitting.report->retained_image || !fitting.report->placement_failed_blocks.empty())
                        capture_legacy_placement_failure(*fitting.report, signals, frame, block, assigned, signal_ids,
                                                         original_feedback_ids);
                    fitting.report->placement_attempts = fitting_logical_visits();
                };
            place_mach_family(
                d, signals, place_options,
                [&](const FamilyPlacement& candidate, const std::vector<bool>& can_move) {
                    try {
                        result = fit_family_prepared(original_design, prepared, fitting, &candidate, &can_move);
                        return PlacementRouteFeedback{};
                    } catch (const FamilyRouteRetry& retry) {
                        return PlacementRouteFeedback{false, retry.signal};
                    }
                },
                failure);
            if (!result)
                throw std::logic_error("family placement returned without routed result");
            return std::move(*result);
        }
        for (unsigned i = 0; i < signals.size(); ++i)
            for (unsigned bit = 0; bit < 2; ++bit)
                if (placement.implicit_controls[i] & (1u << bit)) {
                    p.outputs.push_back(signals[i].name + (bit ? ".PR" : ".RE"));
                    p.terms.emplace_back();
                }
        for (auto i : placement.global_order) {
            const auto& signal = signals[i];
            if (signal.output || signal.input_register || signal.order.fixed_pin >= 0 || signal.order.type == 9 ||
                !included[i]->pair.empty())
                continue;
            floating_input_order.push_back(signal.name);
            auto& candidates = floating_input_candidates[signal.name];
            for (unsigned pin = 1; pin <= d.pins; ++pin) {
                const auto io = std::find(d.io_pins.begin(), d.io_pins.end(), pin);
                if (io == d.io_pins.end() &&
                    std::find(d.dedicated_pins.begin(), d.dedicated_pins.end(), pin) == d.dedicated_pins.end())
                    continue;
                if (signal.order.block >= 0 &&
                    (io == d.io_pins.end() || unsigned(io - d.io_pins.begin()) / (mach2 ? d.cells / 2 : d.cells) !=
                                                  unsigned(signal.order.block)))
                    continue;
                candidates.push_back(pin);
            }
        }
        for (unsigned i = 0; i < signals.size(); ++i) {
            included[i]->number = signals[i].buried ? unsigned(placement.cells[i] + 2) : unsigned(placement.pins[i]);
            placed_oe_slots[signals[i].name] = placement.oe_slots[i];
            placed_oe_keys[signals[i].name]  = placement.oe_keys[i];
            placed_sequential_search         = placement.sequential_search;
        }
        for (auto& pin : design.pins)
            if (output_aliases.count(pin.name)) {
                const auto target = std::find_if(signals.begin(), signals.end(),
                                                 [&](const auto& signal) { return signal.name == pin.pair; });
                pin.number        = unsigned(placement.cells.at(unsigned(target - signals.begin())) + 2);
            }
    }
    std::map<std::string, const Pin*> pins;
    std::set<unsigned> declared;
    for (const auto& pin : design.pins) {
        if (!primary.count(pin.name) && std::find(p.inputs.begin(), p.inputs.end(), pin.name) == p.inputs.end())
            continue;
        if (!pin.number || (pin.latched && !mach2))
            unsupported("requires assigned I/O pins without unsupported latches or pairing");
        if (!declared.insert(pin.number + (pin.node ? d.pins : 0)).second)
            unsupported("duplicate pin assignment");
        pins.emplace(pin.name, &pin);
    }
    for (const auto& equation : design.equations)
        if (equation.function != "COM" && equation.function != "REG" && equation.function != "T" &&
            equation.function != "G" && equation.function != "C" && equation.function != "RE" &&
            equation.function != "PR" && equation.function != "OE")
            unsupported("control equation " + equation.function);
    auto io_cell = [&](const std::string& name) {
        if (!pins.count(name))
            unsupported("undeclared signal " + name);
        if (pins.at(name)->node) {
            const auto number = pins.at(name)->number;
            if (number < 2 || number >= 2 + d.blocks * d.cells)
                unsupported("invalid node number");
            if (mach215 && (number & 1) && !input_pairs.count(name))
                unsupported("MACH215 odd buried cell");
            return number - 2;
        }
        const auto found = std::find(d.io_pins.begin(), d.io_pins.end(), pins.at(name)->number);
        if (found == d.io_pins.end())
            unsupported("dedicated input mapping is not implemented");
        return unsigned(found - d.io_pins.begin()) * (mach2 ? 2u : 1u);
    };
    auto pin_source = [&](const std::string& name) {
        if (!pins.count(name))
            unsupported("undeclared signal " + name);
        const auto pin = pins.at(name)->number;
        const auto io  = std::find(d.io_pins.begin(), d.io_pins.end(), pin);
        if (io != d.io_pins.end())
            return (mach2 ? 3u : 2u) * unsigned(io - d.io_pins.begin()) + 1;
        const auto dedicated = std::find(d.dedicated_pins.begin(), d.dedicated_pins.end(), pin);
        if (dedicated == d.dedicated_pins.end())
            unsupported("invalid input pin " + std::to_string(pin));
        return d.dedicated_sources[unsigned(dedicated - d.dedicated_pins.begin())];
    };
    struct SharedControl
    {
        unsigned block, row;
        std::string term;
    };
    std::vector<SharedControl> controls;
    std::map<std::string, std::vector<std::string>> enables;
    std::set<std::string> reset_outputs;
    std::map<std::string, unsigned> signal_clocks;
    std::map<std::string, bool> signal_clock_negative;
    std::map<std::string, std::vector<std::string>> clock_terms;
    std::set<std::string> product_clocks;
    std::set<std::string> complemented_product_clocks;
    std::set<unsigned> clock_inputs;
    for (unsigned out = 0; out < p.outputs.size();) {
        const auto label = p.outputs[out];
        if (label.size() > 3 && (label.substr(label.size() - 3) == ".OE" || label.substr(label.size() - 3) == ".RE" ||
                                 label.substr(label.size() - 3) == ".PR")) {
            const auto name = label.substr(0, label.size() - 3), function = label.substr(label.size() - 2);
            if (function == "OE") {
                if (p.terms[out].size() > 1)
                    unsupported("multi-product output enable");
                enables[name] = p.terms[out];
            } else {
                if (mach215 && input_pairs.count(name)) {
                    p.outputs.erase(p.outputs.begin() + out);
                    p.terms.erase(p.terms.begin() + out);
                    continue;
                }
                if (p.terms[out].size() > 1)
                    unsupported("reset/preset requires at most one product");
                reset_outputs.insert(name);
                const auto block    = io_cell(name) / d.cells;
                const unsigned row  = mach215       ? 4 * (io_cell(name) % d.cells) + (function == "RE" ? 2 : 3)
                                      : d.legacy110 ? (function == "RE" ? 2 : 67)
                                                    : (function == "RE" ? d.rows - 2 : d.rows - 1);
                const auto existing = std::find_if(controls.begin(), controls.end(),
                                                   [&](const auto& c) { return c.block == block && c.row == row; });
                if (!p.terms[out].empty()) {
                    if (existing == controls.end())
                        controls.push_back({block, row, p.terms[out][0]});
                    else if (existing->term != p.terms[out][0])
                        unsupported("different shared reset/preset terms in a block");
                }
            }
            p.outputs.erase(p.outputs.begin() + out);
            p.terms.erase(p.terms.begin() + out);
            continue;
        }
        if (label.size() < 2 || label.substr(label.size() - 2) != ".C") {
            ++out;
            continue;
        }
        const auto name    = label.substr(0, label.size() - 2);
        clock_terms[name]  = p.terms[out];
        const bool latched = primary.count(name) && primary.at(name)->function == "G";
        const auto input   = p.terms[out].size() == 1 ? p.terms[out][0].find_first_not_of('-') : std::string::npos;
        const bool single =
            input != std::string::npos && p.terms[out][0].find_first_not_of('-', input + 1) == std::string::npos;
        const Pin* pin = single ? pins.at(p.inputs[input]) : nullptr;
        if (mach215 && !input_pairs.count(name) &&
            (!single || !global_clock_names.count(p.inputs[input]) || pin->number != d.clock_pins.front())) {
            auto terms = p.terms[out];
            if (terms.size() > 1) {
                // A sum of literals uses one complemented clock product.
                // The clock-polarity bit restores the original expression.
                std::string complement(p.inputs.size(), '-');
                for (const auto& term : terms) {
                    const auto literal = term.find_first_not_of('-');
                    if (literal == std::string::npos || term.find_first_not_of('-', literal + 1) != std::string::npos)
                        unsupported("product clock and complement both exceed one product");
                    complement[literal] = term[literal] == '1' ? '0' : '1';
                }
                terms = {complement};
                complemented_product_clocks.insert(name);
            }
            const auto cell = io_cell(name);
            if (!terms.empty())
                controls.push_back({cell / d.cells, 4 * (cell % d.cells) + 1, terms[0]});
            signal_clocks[name] = 1;
            product_clocks.insert(name);
            if (single && std::find(d.clock_pins.begin(), d.clock_pins.end(), pin->number) != d.clock_pins.end())
                clock_inputs.insert(unsigned(input));
        } else {
            if (!single)
                unsupported("clock must use one dedicated input");
            const bool negative = (p.terms[out][0][input] == '1') == pin->inverted;
            if (!mach215 && (negative != latched || (!mach2 && latched)))
                unsupported("invalid clock polarity");
            signal_clock_negative[name] = negative;
            const auto clock            = std::find(d.clock_pins.begin(), d.clock_pins.end(), pin->number);
            if (clock == d.clock_pins.end())
                unsupported("clock must use a dedicated clock pin");
            signal_clocks[name] = unsigned(clock - d.clock_pins.begin());
            clock_inputs.insert(unsigned(input));
        }
        p.outputs.erase(p.outputs.begin() + out);
        p.terms.erase(p.terms.begin() + out);
    }
    auto physical = empty_mach_device(d);
    std::fill(physical.low_power.begin(), physical.low_power.end(), 1);
    std::vector<bool> allocated(d.blocks * d.cells), used_pins(d.pins + 1);
    std::vector<unsigned> cells;
    std::vector<bool> inverted;
    std::vector<int> assigned_clocks(d.blocks * d.cells, -1);
    std::vector<bool> clock_negative(d.blocks * d.cells), product_clock(d.blocks * d.cells),
        product_clock_complement(d.blocks * d.cells);
    std::vector<unsigned> mode_words(d.blocks * d.cells, 0x2c);
    std::map<std::string, unsigned> output_indices;
    std::vector<bool> registered, input_storage;
    std::vector<bool> input_storage_cell(d.blocks * d.cells), input_storage_phase;
    for (unsigned out = 0; out < p.outputs.size(); ++out) {
        auto name        = p.outputs[out];
        const bool phase = name.back() == '-';
        if (phase)
            name.pop_back();
        const auto dot      = name.find('.');
        const auto function = dot == std::string::npos ? "COM" : name.substr(dot + 1);
        if (dot != std::string::npos)
            name.resize(dot);
        if (function != "COM" && function != "REG" && function != "T" && function != "G")
            unsupported("synthesized function " + function);
        const auto cell = io_cell(name);
        if (allocated[cell])
            unsupported("duplicate output cell");
        if (p.terms[out].size() > 16)
            unsupported("product-term demand exceeds family steering capacity");
        const bool input_register = input_pairs.count(name);
        bool storage_phase        = phase;
        if (input_register) {
            if (function != "REG" && function != "G" && function != "T")
                unsupported("input pair requires a register or latch");
            if (function == "T" && mach215)
                unsupported("z5530: T input register is illegal on MACH215");
            const auto pad     = std::find(p.inputs.begin(), p.inputs.end(), input_pairs.at(name));
            const auto literal = pad - p.inputs.begin();
            if (pad == p.inputs.end() || p.terms[out].size() != 1 || p.terms[out][0][literal] == '-' ||
                std::count_if(p.terms[out][0].begin(), p.terms[out][0].end(), [](char c) { return c != '-'; }) != 1)
                unsupported("input register excitation must be its paired pad");
            storage_phase            = phase != (p.terms[out][0][literal] == '0');
            storage_phase            = storage_phase != pins.at(*pad)->inverted;
            input_storage_cell[cell] = true;
        } else
            allocated[cell] = true;
        input_storage.push_back(input_register);
        input_storage_phase.push_back(storage_phase);
        if (!pins.at(name)->node)
            used_pins[pins.at(name)->number] = true;
        cells.push_back(cell);
        inverted.push_back(phase != pins.at(name)->inverted);
        output_indices[name] = out;
        registered.push_back(function != "COM");
        mode_words[cell] = function == "COM" ? 0xad : function == "REG" ? 0xa1 : function == "G" ? 0xa9 : 0xa5;
        if (pins.at(name)->node && !mach215)
            mode_words[cell] = mach2 && (cell & 1) ? (d.device == "MACH210" ? mode_words[cell] | 0x40u
                                                                            : (mode_words[cell] & ~0xe0u) | 0x20u)
                                                   : mode_words[cell] & ~0xe0u;
        if (function != "COM") {
            assigned_clocks[cell] =
                signal_clocks.count(name) ? int(signal_clocks.at(name)) : int(mach215 ? 0 : d.clock_pins.size() - 1);
            clock_negative[cell] =
                signal_clock_negative.count(name) ? signal_clock_negative.at(name) : (function == "G" && !mach215);
            product_clock[cell]            = product_clocks.count(name) != 0;
            product_clock_complement[cell] = complemented_product_clocks.count(name) != 0;
        }
        if (!physical.low_power.empty() && !input_register)
            physical.low_power[cell] = low_power.count(name) ? 1 : 0;
    }
    std::vector<std::vector<unsigned>> product_groups(p.outputs.size());
    std::vector<bool> used_groups(d.blocks * d.cells);
    for (unsigned group = 0; group < placement.product_group_owner.size(); ++group) {
        const auto owner = placement.product_group_owner[group];
        if (owner < 0)
            continue;
        const auto found = std::find(cells.begin(), cells.end(), unsigned(owner));
        if (found == cells.end())
            unsupported("product group has no logical owner");
        used_groups[group] = true;
        product_groups[unsigned(found - cells.begin())].push_back(group);
    }
    for (unsigned out = 0; out < p.outputs.size(); ++out)
        if (!input_storage[out] && product_groups[out].size() * 4 < std::max<std::size_t>(1, p.terms[out].size()))
            unsupported("insufficient product groups");
    for (const auto& alias : output_aliases)
        output_indices[alias.first] = output_indices.at(alias.second);
    for (const auto& name : reset_outputs)
        if (!output_indices.count(name) || !registered[output_indices.at(name)])
            unsupported("reset/preset on nonregistered output");
    for (const auto& clock : signal_clocks)
        if (!output_indices.count(clock.first) || !registered[output_indices.at(clock.first)])
            unsupported("clock on nonregistered output");
    for (const auto& enable : enables)
        if (!output_indices.count(enable.first))
            unsupported("enable without an output equation");
    std::vector<FitterOrderKey> output_keys(p.outputs.size());
    std::vector<std::string> output_names(p.outputs.size());
    for (const auto& entry : output_indices) {
        if (output_aliases.count(entry.first))
            continue;
        const auto out    = entry.second;
        output_names[out] = entry.first;
        auto& key         = output_keys[out];
        key.serial        = static_cast<std::int16_t>(signal_ids.at(entry.first));
        key.fixed_pin     = static_cast<std::int16_t>(pins.at(entry.first)->number - 1);
        key.block         = static_cast<std::int16_t>(cells[out] / d.cells);
        key.product_terms = static_cast<std::int16_t>(p.terms[out].size());
    }
    std::vector<std::vector<std::string>> oe_terms(d.blocks, std::vector<std::string>(4));
    std::vector<std::vector<bool>> oe_used(d.blocks, std::vector<bool>(4));
    const auto& oe_rows = d.oe_product_rows;
    if (!mach215)
        for (unsigned block = 0; block < d.blocks; ++block)
            for (unsigned slot = 0; slot < 4; ++slot)
                if (!placement.oe_products[block][slot].empty()) {
                    oe_used[block][slot]  = true;
                    oe_terms[block][slot] = placement.oe_products[block][slot];
                    controls.push_back({block, oe_rows[slot], oe_terms[block][slot]});
                }
    for (auto out : fitter_signal_order(output_keys, false)) {
        const auto found = enables.find(output_names[out]);
        if (found == enables.end())
            continue;
        const auto cell = cells[out], block = cell / d.cells, local = cell % d.cells;
        if (mach215) {
            if (!found->second.empty())
                controls.push_back({block, 4 * local, found->second[0]});
            continue;
        }
        unsigned code = 2;
        if (found->second.empty())
            code = 0;
        else if (found->second[0].find_first_not_of('-') != std::string::npos) {
            const auto assigned = placed_oe_slots.at(output_names[out]);
            if (assigned < 0 || assigned > 3)
                unsupported("missing output-enable placement");
            const unsigned slot = unsigned(assigned);
            if (oe_used[block][slot] && oe_terms[block][slot] != found->second[0])
                unsupported("output-enable placement conflict");
            if (!oe_used[block][slot]) {
                oe_used[block][slot]  = true;
                oe_terms[block][slot] = found->second[0];
                controls.push_back({block, oe_rows[slot], found->second[0]});
            }
            code = (slot & 1) ? 3 : 1;
        }
        mode_words[cell] = (mode_words[cell] & ~0xc0u) | (code << 6);
    }
    const auto& feedback_ids = original_feedback_ids;
    std::vector<unsigned> input_sources;
    std::vector<bool> input_inverted;
    std::vector<std::vector<bool>> fanout(p.inputs.size(), std::vector<bool>(d.blocks));
    std::vector<FitterOrderKey> keys(p.inputs.size());
    for (unsigned input = 0; input < p.inputs.size(); ++input) {
        const auto& name           = p.inputs[input];
        const auto feedback        = output_indices.find(name);
        const bool output_feedback = feedback != output_indices.end() && !paired_pins.count(name);
        // Ordinary COM outputs feed back through the package pad,
        // after its polarity mux. Buried/registered outputs use the
        // internal macrocell source before that mux, as on MACH111.
        const bool is_feedback = output_feedback && (pins.at(name)->node || registered[feedback->second]);
        input_sources.push_back(is_feedback
                                    ? (mach2 ? 3 * (cells[feedback->second] / 2) + (cells[feedback->second] & 1 ? 2 : 0)
                                             : 2 * cells[feedback->second])
                                    : pin_source(name));
        input_inverted.push_back(input_pairs.count(name)      ? input_storage_phase.at(feedback->second)
                                 : output_aliases.count(name) ? alias_phases.at(name)
                                 : is_feedback                ? (pins.at(name)->inverted != inverted[feedback->second])
                                                              : pins.at(name)->inverted);
        if (!pins.at(name)->node)
            used_pins[pins.at(name)->number] = true;
        for (unsigned out = 0; out < p.outputs.size(); ++out)
            if (!input_storage[out])
                for (const auto& term : p.terms[out])
                    if (term[input] != '-')
                        fanout[input][cells[out] / d.cells] = true;
        for (const auto& control : controls)
            if (control.term[input] != '-')
                fanout[input][control.block] = true;
        auto& key = keys[input];
        key.type  = input_pairs.count(name)     ? 8
                    : output_feedback           ? (is_feedback ? 3 : 5)
                    : clock_inputs.count(input) ? 9
                                                : 1;
        // Retype the raw half of an input pair from input class 1 to raw-pair
        // class 10 before sorting. It traverses with output-class records.
        const bool paired_raw =
            std::any_of(input_pairs.begin(), input_pairs.end(), [&](const auto& pair) { return pair.second == name; });
        if (paired_raw) {
            key.type          = 10;
            key.product_terms = -1;
        }
        key.block = output_feedback             ? static_cast<std::int16_t>(cells[feedback->second] / d.cells)
                    : clock_inputs.count(input) ? -1
                                                : 0;
        // Keep dedicated package inputs in block -1 even when they carry data
        // or a product clock. Sorting uses source constraints: floating inputs
        // have not yet acquired a physical block.
        if (!is_feedback) {
            const auto declared = std::find_if(original_design.pins.begin(), original_design.pins.end(),
                                               [&](const auto& pin) { return pin.name == name; });
            if (declared != original_design.pins.end() && declared->number && !declared->node &&
                std::find(d.dedicated_pins.begin(), d.dedicated_pins.end(), declared->number) != d.dedicated_pins.end())
                key.block = -1;
        }
        // MACH215 sorts a global clock with the block's ordinary inputs, also
        // when it sits on a dedicated pin. It is then routed after them and
        // yields a switch-matrix column that an input could use as well.
        if (mach215 && clock_inputs.count(input))
            key.block = 0;
        key.fixed_pin = static_cast<std::int16_t>(pins.at(name)->number - 1);
        if (routing_fixed_nodes.count(name))
            key.fixed_node = routing_fixed_nodes.at(name);
        key.serial = is_feedback && feedback_ids.count(name) ? static_cast<std::int16_t>(feedback_ids.at(name))
                                                             : static_cast<std::int16_t>(signal_ids.at(name));
        if (output_feedback)
            key.product_terms =
                input_pairs.count(name) ? -1 : static_cast<std::int16_t>(p.terms[feedback->second].size());
        key.fanout_count = static_cast<std::int16_t>(std::count(fanout[input].begin(), fanout[input].end(), true));
    }
    auto order = fitter_signal_order(keys, mach2);
    if (!input_pairs.empty()) {
        // Sort the complete imported signal array before filtering fanin lists.
        // The type-1/type-8 comparison is not antisymmetric, so sorting a filtered
        // PLA subset changes the resulting order.
        std::vector<FitterOrderKey> imported;
        std::vector<std::size_t> input_indices;
        std::set<std::int16_t> inserted;
        const auto append_record = [&](FitterOrderKey key) {
            if (!inserted.insert(key.serial).second)
                return;
            const auto found =
                std::find_if(keys.begin(), keys.end(), [&](const auto& item) { return item.serial == key.serial; });
            const auto index = std::size_t(found - keys.begin());
            // A COM pad may also be a fanin, but remains one logical record.
            if (found != keys.end()) {
                key.fanout_count = found->fanout_count;
            }
            imported.push_back(key);
            input_indices.push_back(index);
        };
        for (auto signal : placement.import_order) {
            const auto& name = placement_names.at(signal);
            const auto id    = std::int16_t(signal_ids.at(name));
            const auto input =
                std::find_if(keys.begin(), keys.end(), [&](const auto& key) { return key.serial == id; });
            if (primary.count(name) && !input_pairs.count(name)) {
                auto key = output_keys.at(output_indices.at(name));
                key.type = pins.at(name)->node ? 3 : 0;
                append_record(key);
            } else if (input != keys.end())
                append_record(*input);
            for (const auto& alias : output_aliases)
                if (alias.second == name) {
                    FitterOrderKey key;
                    key.type   = 3;
                    key.serial = std::int16_t(signal_ids.at(alias.first));
                    key.block  = std::int16_t(cells.at(output_indices.at(name)) / d.cells);
                    if (routing_fixed_nodes.count(alias.first))
                        key.fixed_node = routing_fixed_nodes.at(alias.first);
                    append_record(key);
                }
            // Emit the separate registered feedback record immediately after its
            // cluster's pad record.
            if (original_feedback_ids.count(name)) {
                const auto feedback = std::find_if(keys.begin(), keys.end(), [&](const auto& key) {
                    return key.serial == std::int16_t(original_feedback_ids.at(name));
                });
                if (feedback != keys.end())
                    append_record(*feedback);
            }
        }
        order.clear();
        for (auto i : fitter_signal_order(imported, mach2))
            if (input_indices[i] < p.inputs.size())
                order.push_back(input_indices[i]);
    }
    std::reverse(order.begin(), order.end());
    std::vector<std::map<unsigned, unsigned>> input_mux(d.blocks);
    unsigned max_source = 0;
    for (const auto& table : d.mux_sources)
        for (auto source : table)
            if (source != 1000)
                max_source = std::max(max_source, source);
    std::vector<unsigned> unused_loads(max_source + 1);
    InputPinProblem input_problem;
    input_problem.pins.resize(p.inputs.size());
    input_problem.candidates.resize(p.inputs.size());
    input_problem.feedback.resize(p.inputs.size());
    input_problem.occupied_pins = used_pins;
    input_problem.pin_sources.assign(d.pins + 1, -1);
    input_problem.filter_candidates = !d.legacy110;
    for (unsigned i = 0; i < d.io_pins.size(); ++i)
        input_problem.pin_sources[d.io_pins[i]] = int(mach2 ? 3 * i + 1 : 2 * i + 1);
    for (unsigned i = 0; i < d.dedicated_pins.size(); ++i)
        input_problem.pin_sources[d.dedicated_pins[i]] = int(d.dedicated_sources[i]);
    for (unsigned i = 0; i < p.inputs.size(); ++i) {
        const auto& name          = p.inputs[i];
        input_problem.pins[i]     = pins.at(name)->node ? 0 : pins.at(name)->number;
        input_problem.feedback[i] = output_indices.count(name) && !paired_pins.count(name) && !input_pairs.count(name);
        if (floating_input_candidates.count(name)) {
            input_problem.candidates[i]                        = floating_input_candidates.at(name);
            input_problem.occupied_pins[input_problem.pins[i]] = false;
        }
    }
    for (const auto& name : floating_input_order) {
        const auto found = std::find(p.inputs.begin(), p.inputs.end(), name);
        if (found != p.inputs.end())
            input_problem.placement_order.push_back(unsigned(found - p.inputs.begin()));
    }
    for (unsigned block = 0; block < d.blocks; ++block) {
        RouteProblem routing;
        routing.mux_count = d.muxes;
        routing.sources.resize(max_source + 1);
        for (unsigned mux = 0; mux < d.muxes; ++mux)
            for (unsigned slot = 0; slot < d.slots; ++slot)
                if (d.mux_sources[d.legacy110 ? block : 0][mux * d.slots + slot] != 1000)
                    routing.sources[d.source(block, mux, slot)].muxes.push_back(mux);
        for (auto& source : routing.sources)
            source.primary_count = unsigned(source.muxes.size());
        std::vector<unsigned> inputs;
        for (auto input : order)
            if (fanout[input][block]) {
                inputs.push_back(unsigned(input));
                routing.signal_sources.push_back(input_sources[input]);
            }
        routing.allow_alternates.resize(inputs.size());
        input_problem.blocks.push_back(std::move(routing));
        input_problem.block_signals.push_back(std::move(inputs));
    }
    if (movable) {
        input_problem.external_movable.assign(p.inputs.size(), false);
        for (unsigned input = 0; input < p.inputs.size(); ++input) {
            const auto& name = p.inputs[input];
            const auto owner = output_aliases.count(name) ? output_aliases.at(name) : name;
            if (placement_signals.count(owner))
                input_problem.external_movable[input] = movable->at(placement_signals.at(owner));
        }
        for (auto index : placement.global_order) {
            const auto& name = placement_names.at(index);
            for (unsigned input = 0; input < p.inputs.size(); ++input)
                if (p.inputs[input] == name ||
                    (output_aliases.count(p.inputs[input]) && output_aliases.at(p.inputs[input]) == name))
                    input_problem.retry_order.push_back(input);
        }
    }
    if (!placement.reverse_visit_costs.empty()) {
        input_problem.reverse_visit_costs.resize(p.inputs.size());
        for (unsigned i = 0; i < p.inputs.size(); ++i)
            if (placement_signals.count(p.inputs[i]))
                input_problem.reverse_visit_costs[i] =
                    placement.reverse_visit_costs.at(placement_signals.at(p.inputs[i]));
    }
    const auto input_route    = route_input_pins(input_problem);
    const auto capture_report = [&](FuseImage& image, bool successful) {
        if (!fitting.report)
            return;
        FitterReportData report;
        report.successful = successful;
        report.blocks.resize(d.blocks);
        report.routing_attempts   = input_route.failed_route_sweeps;
        report.placement_attempts = fitting_logical_visits();
        for (unsigned b = 0; b < d.blocks; ++b)
            report.blocks[b].sequential_search = placed_sequential_search[b];
        report.optimized_pla    = fitting.report->optimized_pla;
        report.optimized_design = fitting.report->optimized_design;
        report.group_owner      = placement.product_group_owner;
        for (unsigned out = 0; out < output_names.size(); ++out) {
            const auto& name = output_names[out];
            const auto cell  = cells[out];
            FitterReportNode node;
            node.name           = name;
            node.block          = int(cell / d.cells);
            node.cell           = int(cell % d.cells);
            node.number         = int(cell + 2);
            node.input_register = input_storage[out];
            node.pin            = pins.at(name)->node ? -1 : int(pins.at(name)->number);
            if (input_pairs.count(name))
                node.pin = int(pins.at(input_pairs.at(name))->number);
            node.implied = !pins.at(name)->node;
            // A paired node that nothing reads is discarded (z5071): the cell
            // is then reported as the pad's implied macrocell.
            for (const auto& alias : output_aliases)
                if (alias.second == name &&
                    std::find(p.inputs.begin(), p.inputs.end(), alias.first) != p.inputs.end()) {
                    node.name    = alias.first;
                    node.implied = false;
                    node.paired  = true;
                }
            // A constant enable is not an output enable resource.
            const bool constant_enable =
                enables.count(name) &&
                std::all_of(enables.at(name).begin(), enables.at(name).end(),
                            [](const std::string& term) { return term.find_first_not_of('-') == std::string::npos; });
            if (placed_oe_slots.count(name) && !constant_enable)
                node.oe_slot = placed_oe_slots.at(name);
            if (enables.count(name) && !constant_enable)
                for (const auto& term : enables.at(name))
                    node.oe_expression += term + ";";
            if (node.oe_slot >= 0)
                node.oe_key = placed_oe_keys.at(name);
            const auto function = primary.at(name)->function;
            node.logic          = function == "COM" ? 'C' : function == "G" ? 'L' : function == "T" ? 'T' : 'D';
            node.negative       = inverted[out];
            node.low_power      = low_power.count(name) != 0;
            // An equation without products still occupies one grounded term.
            node.product_terms = input_storage[out] ? 0 : std::max<unsigned>(1, unsigned(p.terms[out].size()));
            if (registered[out]) {
                node.clock = product_clocks.count(name) ? "P" : "Ck" + std::to_string(assigned_clocks[cell]);
                // MACH215 reports mark a global clock that acts on the falling
                // edge, or a latch that is open while the clock is high, with a
                // slash. A product-term clock keeps that position blank.
                if (mach215)
                    node.clock = product_clocks.count(name)                  ? "P "
                                 : clock_negative[cell] != (function == "G") ? "/" + node.clock
                                                                             : node.clock;
            }
            std::set<std::string> fanins;
            for (const auto& term : p.terms[out])
                for (unsigned i = 0; i < term.size(); ++i)
                    if (term[i] != '-')
                        fanins.insert(p.inputs[i]);
            node.fanins.assign(fanins.begin(), fanins.end());
            node.data_fanins  = node.fanins;
            const auto append = [&](const std::string& term) {
                for (unsigned i = 0; i < term.size(); ++i)
                    if (term[i] != '-')
                        fanins.insert(p.inputs[i]);
            };
            if (enables.count(name))
                for (const auto& term : enables.at(name))
                    append(term);
            if (registered[out]) {
                const auto reset_row  = mach215 ? 4 * (cell % d.cells) + 2 : d.legacy110 ? 2 : d.rows - 2;
                const auto preset_row = mach215 ? 4 * (cell % d.cells) + 3 : d.legacy110 ? 67 : d.rows - 1;
                for (const auto& control : controls)
                    if (control.block == cell / d.cells) {
                        if (control.row == reset_row) {
                            node.reset = control.term;
                            append(control.term);
                        }
                        if (control.row == preset_row) {
                            node.preset = control.term;
                            append(control.term);
                        }
                    }
                node.global_reset  = global_node.reset.count(name) != 0;
                node.global_preset = global_node.preset.count(name) != 0;
                if (product_clocks.count(name) && clock_terms.count(name)) {
                    const auto logic = fanins;
                    for (const auto& term : clock_terms.at(name))
                        append(term);
                    for (const auto& source : fanins)
                        if (!logic.count(source) && !pins.at(source)->node &&
                            std::find(d.clock_pins.begin(), d.clock_pins.end(), pins.at(source)->number) !=
                                d.clock_pins.end())
                            node.clock_pin_fanins.push_back(source);
                }
            }
            node.fanins.assign(fanins.begin(), fanins.end());
            report.blocks[cell / d.cells].product_terms += node.product_terms;
            if (!successful)
                node.clock.clear();
            report.nodes.push_back(std::move(node));
        }
        for (unsigned block = 0; block < d.blocks; ++block) {
            const auto& inputs = input_problem.block_signals[block];
            for (unsigned i = 0; i < inputs.size(); ++i) {
                FitterReportRoute route;
                route.signal     = p.inputs[inputs[i]];
                route.mux        = input_route.blocks[block].signal_to_mux[i];
                route.logical_id = unsigned(keys[inputs[i]].serial);
                route.source     = int(input_route.signal_sources[block][i]);
                const auto found = output_indices.find(route.signal);
                if (input_pairs.count(route.signal) && found != output_indices.end()) {
                    route.source_input_register = true;
                    route.source_pin            = int(cells[found->second] + 2);
                } else if (found != output_indices.end() && !paired_pins.count(route.signal) &&
                           (pins.at(route.signal)->node || registered[found->second])) {
                    // The same rule that routes the signal: a buried or
                    // registered output is read from its macrocell, a
                    // combinatorial or paired pad from its pin.
                    route.source_block = int(cells[found->second] / d.cells);
                    route.source_cell  = int(cells[found->second] % d.cells);
                } else if (found != output_indices.end()) {
                    route.source_pin = int(pins.at(route.signal)->number);
                } else {
                    // input_pairs is built before unused buried equations are
                    // removed. A pad whose input register was removed is an
                    // ordinary pin source: there is no cell to report.
                    const auto pair = std::find_if(input_pairs.begin(), input_pairs.end(),
                                                   [&](const auto& item) { return item.second == route.signal; });
                    const auto node =
                        pair == input_pairs.end() ? output_indices.end() : output_indices.find(pair->first);
                    route.source_pin = int(pins.at(route.signal)->number);
                    if (node != output_indices.end()) {
                        const auto cell    = cells[node->second];
                        route.source_block = int(cell / d.cells);
                        route.source_cell  = int(cell % d.cells);
                    }
                }
                report.blocks[block].routes.push_back(std::move(route));
            }
            std::set<unsigned> clocks;
            for (unsigned out = 0; out < output_names.size(); ++out)
                if (registered[out] && cells[out] / d.cells == block && !product_clocks.count(output_names[out]))
                    clocks.insert(unsigned(assigned_clocks[cells[out]]));
            for (auto clock : clocks) {
                const auto pin = d.clock_pins.at(clock);
                report.blocks[block].clocks.push_back({image.pin_signals.at(pin), int(clock), int(pin), false});
            }
        }
        finalize_fitter_report_data(design, image, report);
        for (auto& pin : report.pins)
            pin.retained_clock = retained_clock_pins.count(pin.name) != 0;
        if (!successful) {
            report.partitioned = report.partition_total = unsigned(report.nodes.size());
            report.placed = report.placement_total = unsigned(report.nodes.size() + report.pins.size());
            for (const auto& routed : input_route.blocks) {
                report.routing_total += unsigned(routed.signal_to_mux.size());
                report.routed += unsigned(std::count_if(routed.signal_to_mux.begin(), routed.signal_to_mux.end(),
                                                        [](int mux) { return mux >= 0; }));
            }
            for (unsigned b = 0; b < report.blocks.size(); ++b)
                if (std::any_of(report.blocks[b].routes.begin(), report.blocks[b].routes.end(),
                                [](const auto& route) { return route.mux < 0; }))
                    report.route_failed_blocks.push_back(b);
            report.retained_image = std::make_shared<FuseImage>(image);
        }
        image.fitter_report = report;
        *fitting.report     = std::move(report);
    };
    if (fitting.report && !input_route.success) {
        for (unsigned input = 0; input < p.inputs.size(); ++input)
            if (!input_problem.candidates[input].empty())
                for (auto& pin : design.pins)
                    if (pin.name == p.inputs[input])
                        pin.number = input_route.pins[input];
        FuseImage trial;
        trial.device    = d.device;
        trial.design    = design.chip;
        trial.pin_count = d.pins;
        std::set<std::string> active(p.inputs.begin(), p.inputs.end());
        active.insert(output_names.begin(), output_names.end());
        retain_removed_clocks(active);
        apply_placement_metadata(trial, design, active);
        capture_report(trial, false);
    }
    if (!input_route.success) {
        int target = -1;
        if (input_route.outer_signal >= 0) {
            const auto& name = p.inputs[unsigned(input_route.outer_signal)];
            target           = int(placement_signals.at(output_aliases.count(name) ? output_aliases.at(name) : name));
        }
        throw FamilyRouteRetry{target};
    }
    used_pins = input_problem.occupied_pins;
    for (unsigned input = 0; input < p.inputs.size(); ++input)
        if (!input_problem.candidates[input].empty()) {
            const auto pin = input_route.pins[input];
            used_pins[pin] = true;
            for (auto& declared_pin : design.pins)
                if (declared_pin.name == p.inputs[input])
                    declared_pin.number = pin;
            input_sources[input] = unsigned(input_problem.pin_sources[pin]);
        }
    for (unsigned block = 0; block < d.blocks; ++block) {
        auto& hw           = physical.blocks[block];
        auto routing       = input_problem.blocks[block];
        const auto& inputs = input_problem.block_signals[block];
        for (unsigned i = 0; i < inputs.size(); ++i)
            routing.signal_sources[i] = input_sources[inputs[i]];
        const auto& routed = input_route.blocks[block];
        for (unsigned i = 0; i < inputs.size(); ++i)
            input_mux[block][inputs[i]] = unsigned(routed.signal_to_mux[i]);
        std::set<unsigned> selected_sources(routing.signal_sources.begin(), routing.signal_sources.end());
        for (unsigned mux = 0; mux < d.muxes; ++mux) {
            unsigned selected = 0;
            if (d.legacy110 && (mux == 16 || mux == 17)) {
                hw.mux_codes[mux] = 0;
                continue;
            }
            if (routed.mux_to_signal[mux] >= 0) {
                const auto source = routing.signal_sources[unsigned(routed.mux_to_signal[mux])];
                while (d.source(block, mux, selected) != source)
                    ++selected;
            } else if (mach2 && d.slots == 12) {
                hw.mux_codes[mux] = 15;
                continue;  // Disconnect unused 12-way CSM muxes.
            } else if (d.device != "MACH131" && d.device != "MACH231") {
                // Choose the first source not already selected in this block,
                // including sources assigned to previous unused muxes.
                while (selected < d.slots &&
                       (d.mux_sources[d.legacy110 ? block : 0][mux * d.slots + selected] == 1000 ||
                        selected_sources.count(d.source(block, mux, selected))))
                    ++selected;
                if (selected == d.slots)
                    unsupported("unused CSM source selection exhausted");
            } else {
                bool found = false;
                for (unsigned slot = 0; slot < d.slots; ++slot) {
                    const auto source   = d.source(block, mux, slot);
                    const auto limit    = mach2 ? unsigned(d.io_pins.size()) * 3 : unsigned(allocated.size()) * 2;
                    const bool feedback = source < limit && (mach2 ? source % 3 != 1 : !(source & 1));
                    const auto cell     = mach2 ? 2 * (source / 3) + (source % 3 == 2) : source / 2;
                    if (feedback && !allocated[cell] && !input_storage_cell[cell]) {
                        selected = slot;
                        found    = true;
                        break;
                    }
                }
                if (!found) {
                    for (unsigned slot = 0; slot < d.slots; ++slot) {
                        const auto source     = d.source(block, mux, slot);
                        unsigned physical_pin = 0;
                        if (mach2 && source < d.io_pins.size() * 3 && source % 3 == 1)
                            physical_pin = d.io_pins[source / 3];
                        else if (!mach2 && source < allocated.size() * 2 && (source & 1))
                            physical_pin = d.io_pins[source / 2];
                        else {
                            const auto dedicated =
                                std::find(d.dedicated_sources.begin(), d.dedicated_sources.end(), source);
                            if (dedicated != d.dedicated_sources.end())
                                physical_pin = d.dedicated_pins[unsigned(dedicated - d.dedicated_sources.begin())];
                        }
                        if (physical_pin && !used_pins[physical_pin]) {
                            selected = slot;
                            found    = true;
                            break;
                        }
                    }
                }
                if (!found) {
                    for (unsigned slot = 1; slot < d.slots; ++slot)
                        if (unused_loads[d.source(block, mux, slot)] < unused_loads[d.source(block, mux, selected)])
                            selected = slot;
                    ++unused_loads[d.source(block, mux, selected)];
                }
            }
            hw.mux_codes[mux] = d.mux_code(mux, selected);
            selected_sources.insert(d.source(block, mux, selected));
        }
        if (mach215) {
            // MACH215 has seven-bit output and three-bit input register fields.
            // Balance only input clocks; unused output clocks retain their high-bit
            // initial value.
            std::vector<unsigned> loads(d.clock_pins.size());
            for (unsigned local = 0; local < d.cells; ++local)
                if (assigned_clocks[block * d.cells + local] >= 0)
                    ++loads[unsigned(assigned_clocks[block * d.cells + local])];
            for (unsigned local = 0; local < d.cells; local += 2) {
                const auto cell = block * d.cells + local;
                unsigned word   = 0x4b;
                if (allocated[cell]) {
                    const auto abstract = mode_words[cell];
                    word                = 0x20 | ((abstract & 12) >> 2) | ((abstract & 32) << 1);
                    word |= assigned_clocks[cell] >= 0 ? unsigned(assigned_clocks[cell]) << 3 : 8;
                    if (assigned_clocks[cell] >= 0 &&
                        (product_clock[cell] ? (((abstract & 12) == 8) != product_clock_complement[cell])
                                             : (clock_negative[cell] != ((abstract & 12) == 8))))
                        word |= 4;
                    bool implicit = true;
                    for (const auto& entry : output_indices)
                        if (!output_aliases.count(entry.first) && cells[entry.second] == cell &&
                            (enables.count(entry.first) || pins.at(entry.first)->node))
                            implicit = false;
                    if (implicit)
                        std::fill(hw.products[4 * local].begin(), hw.products[4 * local].end(), 3);  // implicit OE
                } else
                    for (unsigned row = 4 * (local + 1); row < 4 * (local + 2); ++row)
                        std::fill(hw.products[row].begin(), hw.products[row].end(), 3);
                hw.macrocell_words[local] = std::uint16_t(word);
                const auto input_cell     = cell + 1;
                const auto clock = assigned_clocks[input_cell] >= 0
                                       ? unsigned(assigned_clocks[input_cell])
                                       : unsigned(std::min_element(loads.begin(), loads.end()) - loads.begin());
                if (assigned_clocks[input_cell] < 0)
                    ++loads[clock];
                unsigned input_word = 1;
                if (input_storage_cell[input_cell]) {
                    const bool latch = (mode_words[input_cell] & 12) == 8;
                    input_word       = unsigned(latch) | ((clock_negative[input_cell] != latch) ? 2u : 0u);
                }
                hw.macrocell_words[local + 1] = std::uint16_t(input_word | (clock << 2));
            }
        } else if (mach2) {
            const bool old210 = d.device == "MACH210";
            auto convert      = [&](unsigned word) {
                return old210 ? word
                              : ((word & 3) << 4) | ((word & 12) >> 2) | ((word & 16) >> 1) | ((word & 32) >> 3) |
                                    (word & 192) | (word & 256);
            };
            std::vector<unsigned> loads(d.clock_pins.size());
            for (unsigned local = 0; local < d.cells; ++local)
                if (assigned_clocks[block * d.cells + local] >= 0)
                    ++loads[unsigned(assigned_clocks[block * d.cells + local])];
            for (unsigned local = 0; local < d.cells; ++local) {
                const auto cell = block * d.cells + local;
                unsigned word = allocated[cell] ? convert(mode_words[cell]) : old210 ? (local & 1 ? 0xef : 0x2f) : 0x37;
                if (!allocated[cell]) {
                    // Steering reaches two preceding groups and one following group.
                    int delta     = 0;
                    bool previous = false;
                    for (unsigned distance = 1; distance <= 2 && distance <= local; ++distance)
                        if (!allocated[cell - distance]) {
                            delta    = -int(distance);
                            previous = true;
                            break;
                        }
                    if (!previous && local + 1 < d.cells && !allocated[cell + 1])
                        delta = 1;
                    const unsigned s0 = old210 ? 0 : 4, s1 = old210 ? 1 : 5;
                    if (delta == -2 || delta == 1)
                        word &= ~(1u << s0);
                    if (delta == -2 || delta == 0)
                        word &= ~(1u << s1);
                    for (unsigned row = 2 + 4 * local; row < 6 + 4 * local; ++row)
                        std::fill(hw.products[row].begin(), hw.products[row].end(), 3);
                }
                if (input_storage_cell[cell]) {
                    const bool latch  = (mode_words[cell] & 12) == 8;
                    const bool toggle = (mode_words[cell] & 12) == 4;
                    word &= ~(old210 ? 44u : 7u);
                    if (latch)
                        word |= old210 ? 8u : 2u;
                    if (toggle)
                        word |= old210 ? 4u : 1u;
                }
                hw.macrocell_words[local] = std::uint16_t(word);
            }
            auto least_clock = [&]() {
                const auto clock = unsigned(std::min_element(loads.begin(), loads.end()) - loads.begin());
                ++loads[clock];
                return clock;
            };
            for (unsigned local = 0; local < d.cells; ++local) {
                const auto cell = block * d.cells + local;
                // Interleave output and input register clock assignment in physical
                // cell order, using a shared load table.
                if (!(local & 1)) {
                    const auto clock   = assigned_clocks[cell] >= 0 ? unsigned(assigned_clocks[cell]) : least_clock();
                    const unsigned low = old210 ? 4 : 3;
                    hw.macrocell_words[local] &= std::uint16_t(~((1u << low) | (!old210 ? 256u : 0u)));
                    hw.macrocell_words[local] |=
                        std::uint16_t(((clock & 1) << low) | (!old210 ? (clock >> 1) << 8 : 0u));
                }
                if (local < d.cells / 2) {
                    const unsigned target = local * 2 + 1, low = old210 ? 4 : 3;
                    const auto clock = assigned_clocks[block * d.cells + target] >= 0
                                           ? unsigned(assigned_clocks[block * d.cells + target])
                                           : least_clock();
                    hw.macrocell_words[target] &= std::uint16_t(~((1u << low) | (!old210 ? 64u : 0u)));
                    hw.macrocell_words[target] |=
                        std::uint16_t(((clock & 1) << low) | (!old210 ? (clock >> 1) << 6 : 0u));
                }
            }
        } else {
            std::vector<unsigned> clock_load(d.clock_pins.size());
            for (unsigned local = 0; local < d.cells; ++local)
                if (assigned_clocks[block * d.cells + local] >= 0)
                    ++clock_load[unsigned(assigned_clocks[block * d.cells + local])];
            for (unsigned local = 0; local < d.cells; ++local) {
                const auto cell = block * d.cells + local;
                unsigned clock;
                if (assigned_clocks[cell] >= 0)
                    clock = unsigned(assigned_clocks[cell]);
                else {
                    clock = unsigned(std::min_element(clock_load.begin(), clock_load.end()) - clock_load.begin());
                    ++clock_load[clock];
                }
                hw.macrocell_words[local] =
                    static_cast<std::uint16_t>(mode_words[cell] | ((clock & 1) << 4) | ((clock >> 1) << 8));
                if (!allocated[cell] && (d.device == "MACH130" || d.device == "MACH131")) {
                    // Initialize both steering bits. Prefer the previous unused group,
                    // then the next, then the current group. Clear S0 for the next group
                    // and S1 for the current group.
                    hw.macrocell_words[local] |= 3;
                    if (local && !allocated[cell - 1]) { /* Previous:11. */
                    } else if (local + 1 < d.cells && !allocated[cell + 1])
                        hw.macrocell_words[local] &= ~1u;
                    else
                        hw.macrocell_words[local] &= ~2u;
                }
                if (d.legacy110) {
                    const auto word = hw.macrocell_words[local];
                    hw.macrocell_words[local] =
                        static_cast<std::uint16_t>(((word & 3) << 4) | ((word & 12) >> 2) | ((word & 16) >> 1) |
                                                   ((word & 32) >> 3) | (word & 192));
                }
                const unsigned first = d.legacy110 ? 3 : 2;
                if (!allocated[cell])
                    for (unsigned row = first + 4 * local; row < first + 4 + 4 * local; ++row)
                        std::fill(hw.products[row].begin(), hw.products[row].end(), 3);
            }
        }
    }
    // Product-group control bits belong to the group being steered, not
    // necessarily the output macrocell. Terms are emitted in physical order.
    for (unsigned group = 0; group < d.blocks * d.cells; ++group) {
        if (mach215 && (group & 1))
            continue;
        const auto block = group / d.cells, local = group % d.cells;
        auto& hw             = physical.blocks[block];
        const int owner      = placement.product_group_owner[group];
        const unsigned first = (mach215 ? 4 : d.legacy110 ? 3 : 2) + 4 * local;
        for (unsigned t = 0; t < 4; ++t)
            std::fill(hw.products[first + t].begin(), hw.products[first + t].end(), used_groups[group] ? 0 : 3);
        unsigned steering = 0;
        if (owner >= 0) {
            const int delta = owner - int(group);
            steering        = delta == 0 ? 1 : delta > 0 ? 2 : 3;
            if (mach2 && !mach215 && delta == -2)
                steering = 0;
        } else if (d.device == "MACH130" || d.device == "MACH131" || (mach2 && !mach215)) {
            int delta     = 0;
            bool previous = false;
            for (unsigned distance = 1; distance <= (mach2 ? 2u : 1u) && distance <= local; ++distance)
                if (!allocated[group - distance]) {
                    delta    = -int(distance);
                    previous = true;
                    break;
                }
            if (!previous && local + 1 < d.cells && !allocated[group + 1])
                delta = 1;
            steering = delta == 0 ? 1 : delta > 0 ? 2 : 3;
            if (delta == -2)
                steering = 0;
        }
        const unsigned s0 = mach215 ? 5 : (d.legacy110 || (mach2 && d.device != "MACH210")) ? 4 : 0;
        const unsigned s1 = mach215 ? 4 : s0 + 1;
        hw.macrocell_words[local] &= std::uint16_t(~((1u << s0) | (1u << s1)));
        hw.macrocell_words[local] |= std::uint16_t(((steering & 1) << s0) | ((steering >> 1) << s1));
        if (owner >= 0 && !allocated[group] && !mach215 && (!mach2 || !(local & 1)))
            hw.macrocell_words[local] &=
                std::uint16_t(~(1u << ((d.legacy110 || (mach2 && d.device != "MACH210")) ? 2 : 5)));
    }
    for (unsigned out = 0; out < p.outputs.size(); ++out) {
        if (input_storage[out])
            continue;
        const auto block = cells[out] / d.cells, local = cells[out] % d.cells;
        auto& hw = physical.blocks[block];
        if (inverted[out] && !pins.at(output_names[out])->node)
            hw.macrocell_words[local] &= ~(1u << (mach215                                             ? 6
                                                  : (d.legacy110 || (mach2 && d.device != "MACH210")) ? 2
                                                                                                      : 5));
        for (unsigned t = 0; t < p.terms[out].size(); ++t) {
            const auto group = product_groups[out][t / 4] % d.cells;
            auto& row        = hw.products[(mach215 ? 4 : d.legacy110 ? 3 : 2) + 4 * group + t % 4];
            std::fill(row.begin(), row.end(), 3);
            for (unsigned input = 0; input < p.inputs.size(); ++input)
                if (p.terms[out][t][input] != '-')
                    row[input_mux[block].at(input)] =
                        ((p.terms[out][t][input] == '1') != input_inverted[input]) ? 2 : 1;
        }
    }
    for (const auto& control : controls) {
        auto& row = physical.blocks[control.block].products[control.row];
        std::fill(row.begin(), row.end(), 3);
        for (unsigned input = 0; input < p.inputs.size(); ++input)
            if (control.term[input] != '-')
                row[input_mux[control.block].at(input)] =
                    ((control.term[input] == '1') != input_inverted[input]) ? 2 : 1;
    }
    check_fitting_budget();
    auto image = encode_mach_device(d, physical, design.chip);
    std::set<std::string> active(p.inputs.begin(), p.inputs.end());
    active.insert(output_names.begin(), output_names.end());
    retain_removed_clocks(active);
    apply_placement_metadata(image, design, active);
    auto expression = [&](const std::vector<std::string>& terms) { return simulation_cover(p.inputs, terms); };
    for (unsigned out = 0; out < output_names.size(); ++out) {
        const auto& name = output_names[out];
        const bool phase = inverted[out] != pins.at(name)->inverted;
        auto next        = expression(p.terms[out]);
        if (!registered[out]) {
            image.simulation_model.combinatorial[name] = phase ? make_not(next) : next;
            continue;
        }
        auto& model          = image.simulation_model.registers[name];
        model.input_register = input_storage[out];
        model.power_up = model.reset_value = simulation_bit(input_storage_phase[out]);
        model.preset_value                 = simulation_bit(!input_storage_phase[out]);
        const bool toggle                  = p.outputs[out].find(".T") != std::string::npos;
        model.next_state                   = toggle  ? make_binary(ExprKind::Xor, make_identifier(name), next)
                                             : phase ? make_not(next)
                                                     : next;
        model.reset                        = make_constant(false);
        model.preset                       = make_constant(false);
        const auto cell = cells[out], block = cell / d.cells, local = cell % d.cells;
        const unsigned reset_row  = mach215 ? 4 * local + 2 : d.legacy110 ? 2 : d.rows - 2;
        const unsigned preset_row = mach215 ? 4 * local + 3 : d.legacy110 ? 67 : d.rows - 1;
        for (const auto& control : controls)
            if (control.block == block) {
                if (control.row == reset_row)
                    model.reset = expression({control.term});
                if (control.row == preset_row)
                    model.preset = expression({control.term});
            }
        if (clock_terms.count(name))
            model.clock = expression(clock_terms.at(name));
        else
            model.clock = simulation_clock_input(
                image.simulation_model, design, d.clock_pins.at(unsigned(assigned_clocks[cell])), clock_negative[cell]);
    }
    for (const auto& alias : output_aliases) {
        const auto storage = image.simulation_model.registers.find(alias.second);
        const auto out     = output_indices.at(alias.second);
        const bool phase   = alias_phases.at(alias.first);
        auto next          = expression(p.terms[out]);
        if (storage != image.simulation_model.registers.end()) {
            auto& model    = image.simulation_model.registers[alias.first];
            model          = storage->second;
            model.power_up = model.reset_value = simulation_bit(phase);
            model.preset_value                 = simulation_bit(!phase);
            model.next_state                   = p.outputs[out].find(".T") != std::string::npos
                                                     ? make_binary(ExprKind::Xor, make_identifier(alias.first), next)
                                                 : phase ? make_not(next)
                                                         : next;
        } else
            image.simulation_model.combinatorial[alias.first] = phase ? make_not(next) : next;
    }
    validate_requested_power_up(image.simulation_model, design);
    capture_report(image, true);
    return image;
}
}  // namespace
FuseImage fit_mach_family_fixed(const Design& source, OptimizerOptions options, FitterOptions fitting)
{
    Design design = source;
    auto prepared = synthesize_pla(design, options, fitting.report.get());
    FittingDeadline deadline(fitting.runtime_quarters);
    return fit_family_prepared(design, prepared, fitting);
}
}  // namespace machxl
