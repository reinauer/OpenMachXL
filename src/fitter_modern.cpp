#include "fitter_modern.hpp"
#include "device_family.hpp"
#include "fitter_budget.hpp"
#include "fitter_failure_report.hpp"
#include "fitter_inputs.hpp"
#include "fitter_live.hpp"
#include "fitter_metadata.hpp"
#include "fitter_modern_place.hpp"
#include "fitter_route.hpp"
#include "power_reports.hpp"
#include "simulation_products.hpp"
#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <sstream>

namespace machxl {
namespace {
[[noreturn]] void unsupported(const std::string& reason)
{
    throw LogicError("MACH3/4 fitter: " + reason);
}
struct ParsedPla
{
    std::vector<std::string> inputs, outputs;
    std::vector<std::vector<std::string>> terms;
};

// Consume our own synthesized PLA rather than duplicating the minimizer,
// don't-care handling, MINIMIZE_OFF semantics and shared-product ordering.
ParsedPla synthesized_pla(Design& design, OptimizerOptions options, FitterReportData* report = nullptr)
{
    const auto text = emit_pla(design, "design.pds", options, &design);
    if (report) {
        report->optimized_pla    = text;
        report->optimized_design = std::make_shared<Design>(design);
    }
    ParsedPla p;
    unsigned input_count = 0, output_count = 0;
    std::istringstream file(text);
    std::string line, labels;
    bool products = false;
    while (std::getline(file, line)) {
        std::istringstream fields(line);
        std::string token;
        if (!(fields >> token) || token[0] == '#')
            continue;
        if (token[0] == '.') {
            labels.clear();
            if (token == ".i")
                fields >> input_count;
            else if (token == ".o") {
                fields >> output_count;
                p.terms.resize(output_count);
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
            if (input_count) {
                pattern = token;
                fields >> outputs;
            } else
                outputs = token;
            if (pattern.size() != input_count || outputs.size() != output_count)
                unsupported("invalid synthesized PLA dimensions");
            for (unsigned out = 0; out < output_count; ++out)
                if (outputs[out] == '1' && (input_count || p.terms[out].empty()))
                    p.terms[out].push_back(pattern);
            continue;
        } else if (!labels.empty()) {
            (labels == ".ilb" ? p.inputs : p.outputs).push_back(token);
        }
        if (!labels.empty())
            while (fields >> token)
                (labels == ".ilb" ? p.inputs : p.outputs).push_back(token);
    }
    if (p.inputs.size() != input_count || p.outputs.size() != output_count)
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
            while (length && (output[length - 1] == '-' || output[length - 1] == '%'))
                --length;
        }
        output = original_names.at(output.substr(0, length)) + output.substr(length);
    }
    return p;
}

}  // namespace
FuseImage fit_mach_modern(const Design& original_design, OptimizerOptions options, FitterOptions fitting)
{
    Design design = original_design;
    const auto& d = mach_device_layout(design.device);
    if (d.device != "MACH355" && d.device != "MACH435" && d.device != "MACH445" && d.device != "MACH446" &&
        d.device != "MACH465")
        unsupported("configuration rules for " + d.device);
    const bool mach355  = d.device == "MACH355";
    const unsigned ios  = mach355 ? 16 : 8;
    const auto data_row = [&](unsigned cell) { return cell * 5 + cell / (mach355 ? 1 : 2) + 1; };
    const auto oe_row   = [&](unsigned io) { return io * (mach355 ? 6 : 11); };
    auto pla            = synthesized_pla(design, options, fitting.report.get());
    expand_global_node(design, pla.inputs, pla.outputs, pla.terms);
    discard_combinatorial_storage_controls(pla.outputs, pla.terms);
    validate_node_output_enables(design, pla.outputs);
    validate_node_equations(design, pla.inputs, pla.outputs);
    validate_equation_fanin(pla.inputs, pla.outputs, pla.terms, d.muxes);
    validate_storage_control_products(pla.outputs, pla.terms);
    FittingDeadline deadline(fitting.runtime_quarters);
    check_fitting_budget();
    if (fitting.use_saved_placement || !fitting.saved_placement.empty())
        apply_saved_placement(design, fitting.saved_placement);
    apply_preplacement_policy(design, fitting.preplacement_policy);
    validate_fixed_locations(design);
    if (fitting.report)
        fitting.report->optimized_design = std::make_shared<Design>(design);
    const auto original = design;
    if (pla.outputs.empty())
        unsupported("output-free design");
    if (design.dos_compatibility && pla.inputs.empty() && pla.outputs.size() == 1)
        unsupported("z5017: original single-output input-free PLA omits .ilb");
    if (design.dos_compatibility && pla.inputs.empty() &&
        std::all_of(pla.terms.begin(), pla.terms.end(), [](const auto& terms) { return terms.empty(); }))
        unsupported("z5015: original zero-product PLA is rejected");
    struct Output
    {
        std::string name, function;
        unsigned column;
        bool inverted, negative;
    };
    std::map<std::string, Output> outputs;
    std::map<std::string, unsigned> xor_left;
    std::map<std::string, std::map<std::string, unsigned>> controls;
    for (unsigned out = 0; out < pla.outputs.size(); ++out) {
        auto name     = pla.outputs[out];
        bool inverted = name.back() == '%';
        if (inverted)
            name.pop_back();
        const bool negative = name.back() == '-';
        if (negative) {
            name.pop_back();
            inverted = !inverted;
        }
        const auto dot = name.find('.');
        auto fn        = dot == std::string::npos ? "COM" : name.substr(dot + 1);
        if (dot != std::string::npos)
            name.resize(dot);
        if (fn.compare(0, 4, "XORL") == 0) {
            xor_left[name] = out;
            continue;
        }
        if (fn.compare(0, 4, "XORR") == 0)
            fn = fn.size() == 4 ? "COM" : fn.substr(5);
        if (fn == "COM" || fn == "REG" || fn == "T" || fn == "G")
            outputs.emplace(name, Output{name, fn, out, inverted, negative});
        else if (fn == "C" || fn == "OE" || fn == "RE" || fn == "PR")
            controls[name][fn] = out;
        else
            unsupported("control equation " + fn);
    }
    // Create implicit clocks before pruning unused nodes. MACH3/4 cannot
    // merge an existing named pin with that clock, even if the register
    // requesting it is subsequently discarded.
    const auto default_clock_pin = std::find_if(design.pins.begin(), design.pins.end(), [&](const auto& pin) {
        return !pin.node && pin.number == d.clock_pins.front();
    });
    if (default_clock_pin != design.pins.end())
        for (const auto& output : outputs)
            if (output.second.function != "COM" && !controls[output.first].count("C"))
                unsupported("z5061: default clock pin " + default_clock_pin->name + " is unavailable for " +
                            output.first);
    // A constant XOR leg is absorbed before resource allocation. A true
    // left leg toggles the output phase; neither constant needs an XOR
    // auxiliary or an OR product group.
    for (auto it = xor_left.begin(); it != xor_left.end();) {
        const auto& terms        = pla.terms[it->second];
        const bool constant_true = terms.size() == 1 && terms.front().find_first_not_of('-') == std::string::npos;
        if (terms.empty() || constant_true) {
            if (constant_true)
                outputs.at(it->first).inverted = !outputs.at(it->first).inverted;
            it = xor_left.erase(it);
        } else
            ++it;
    }
    // Zero-fanout pruning retains unobserved self/mutual feedback cycles.
    std::set<std::string> primary;
    std::map<std::string, std::set<std::string>> dependencies;
    for (const auto& output : outputs) {
        const auto& name = output.first;
        primary.insert(name);
        auto follow = [&](unsigned column) {
            for (const auto& term : pla.terms[column])
                for (unsigned i = 0; i < term.size(); ++i)
                    if (term[i] != '-' && outputs.count(pla.inputs[i]))
                        dependencies[name].insert(pla.inputs[i]);
        };
        follow(output.second.column);
        if (xor_left.count(name))
            follow(xor_left.at(name));
        for (const auto& control : controls[name])
            follow(control.second);
    }
    // The original classifies clock signals before it removes unused buried
    // equations. A signal fixed on a dedicated clock pin keeps its pin, and
    // its place in the reports, even when its only register is removed.
    std::set<std::string> fixed_clock_inputs, retained_clock_pins;
    for (const auto& entry : controls) {
        const auto clock = entry.second.find("C");
        if (clock == entry.second.end() || pla.terms[clock->second].size() != 1)
            continue;
        const auto& term = pla.terms[clock->second].front();
        const auto input = term.find_first_not_of('-');
        if (input == std::string::npos || term.find_first_not_of('-', input + 1) != std::string::npos)
            continue;
        const auto pin = std::find_if(design.pins.begin(), design.pins.end(),
                                      [&](const auto& item) { return item.name == pla.inputs[input]; });
        if (pin != design.pins.end() && !pin->node &&
            std::find(d.clock_pins.begin(), d.clock_pins.end(), pin->number) != d.clock_pins.end())
            fixed_clock_inputs.insert(pin->name);
    }
    const auto retain_removed_clocks = [&](std::set<std::string>& active) {
        for (const auto& name : fixed_clock_inputs)
            if (active.insert(name).second)
                retained_clock_pins.insert(name);
    };
    const auto reachable = retained_equation_names(design, primary, dependencies);
    for (auto it = outputs.begin(); it != outputs.end();)
        if (!reachable.count(it->first))
            it = outputs.erase(it);
        else
            ++it;
    for (auto it = xor_left.begin(); it != xor_left.end();)
        if (!reachable.count(it->first))
            it = xor_left.erase(it);
        else
            ++it;
    for (auto it = controls.begin(); it != controls.end();)
        if (!reachable.count(it->first))
            it = controls.erase(it);
        else
            ++it;
    std::vector<bool> live_inputs(pla.inputs.size());
    auto retain_inputs = [&](unsigned column) {
        for (const auto& term : pla.terms[column])
            for (unsigned i = 0; i < term.size(); ++i)
                if (term[i] != '-')
                    live_inputs[i] = true;
    };
    for (const auto& output : outputs)
        retain_inputs(output.second.column);
    for (const auto& output : xor_left)
        retain_inputs(output.second);
    for (const auto& output : controls)
        for (const auto& control : output.second)
            retain_inputs(control.second);
    for (std::size_t i = live_inputs.size(); i-- > 0;)
        if (!live_inputs[i]) {
            pla.inputs.erase(pla.inputs.begin() + i);
            for (auto& terms : pla.terms)
                for (auto& term : terms)
                    term.erase(i, 1);
        }
    std::set<std::string> automatic_input_pairs;
    // MACHFITR performs a second input-pairing pass after MACHBPP. In
    // particular, BPP leaves a fixed pad and floating node unpaired, while
    // the fitter can allocate that pad's dedicated input register.
    if (!mach355 && fitting.automatic_input_pairing) {
        std::set<std::string> paired;
        for (const auto& pin : design.pins)
            if (!pin.pair.empty()) {
                paired.insert(pin.name);
                paired.insert(pin.pair);
            }
        for (const auto& label : pla.outputs) {
            const auto dot = label.find('.');
            if (dot == std::string::npos)
                continue;
            const auto name = label.substr(0, dot);
            if (!outputs.count(name) || paired.count(name))
                continue;
            const auto& out = outputs.at(name);
            if (out.function != "REG" && out.function != "G")
                continue;
            auto node =
                std::find_if(design.pins.begin(), design.pins.end(), [&](const auto& p) { return p.name == name; });
            if (node == design.pins.end() || !node->node || node->number)
                continue;
            const auto& terms = pla.terms[out.column];
            const auto input  = terms.size() == 1 ? terms[0].find_first_not_of('-') : std::string::npos;
            if (input == std::string::npos || terms[0].find_first_not_of('-', input + 1) != std::string::npos)
                continue;
            auto pad = std::find_if(design.pins.begin(), design.pins.end(),
                                    [&](const auto& p) { return p.name == pla.inputs[input]; });
            if (pad == design.pins.end() || pad->node || outputs.count(pad->name) || paired.count(pad->name))
                continue;
            if (pad->number && std::find(d.io_pins.begin(), d.io_pins.end(), pad->number) == d.io_pins.end())
                continue;
            bool eligible = true;
            for (const auto& control : controls[name]) {
                const auto& ts = pla.terms[control.second];
                if (control.first == "RE" || control.first == "PR")
                    eligible = false;
                if (control.first == "C") {
                    const auto ci = ts.size() == 1 ? ts[0].find_first_not_of('-') : std::string::npos;
                    if (ci == std::string::npos || ts[0].find_first_not_of('-', ci + 1) != std::string::npos)
                        eligible = false;
                    else {
                        const auto clock = std::find_if(design.pins.begin(), design.pins.end(),
                                                        [&](const auto& p) { return p.name == pla.inputs[ci]; });
                        if (clock == design.pins.end() || clock->node ||
                            (clock->number &&
                             std::find(d.clock_pins.begin(), d.clock_pins.end(), clock->number) == d.clock_pins.end()))
                            eligible = false;
                    }
                }
            }
            if (eligible) {
                pad->pair      = node->name;
                pad->pair_kind = "IPAIR";
                automatic_input_pairs.insert(node->name);
                paired.insert(pad->name);
                paired.insert(node->name);
            }
        }
    }
    std::map<std::string, std::string> output_aliases, input_registers, input_pairs;
    std::set<std::string> paired_pins;
    for (const auto& pin : design.pins)
        if (!pin.pair.empty()) {
            const auto target = std::find_if(design.pins.begin(), design.pins.end(),
                                             [&](const auto& other) { return other.name == pin.pair; });
            if (!pin.node && target != design.pins.end() && target->node) {
                if (mach355)
                    continue;  // No dedicated input registers in MACH3.
                if (!outputs.count(target->name))
                    continue;
                if (outputs.at(target->name).function == "COM")
                    unsupported("paired input has no register excitation");
                const auto& excitation = outputs.at(target->name);
                const auto& terms      = pla.terms[excitation.column];
                const auto input       = terms.size() == 1 ? terms[0].find_first_not_of('-') : std::string::npos;
                bool eligible =
                    (excitation.function == "REG" || excitation.function == "G") && input != std::string::npos &&
                    terms[0].find_first_not_of('-', input + 1) == std::string::npos && pla.inputs[input] == pin.name;
                for (const auto& control : controls[target->name]) {
                    if ((control.first == "RE" || control.first == "PR") && !pla.terms[control.second].empty())
                        eligible = false;
                    if (control.first == "C") {
                        const auto& ts = pla.terms[control.second];
                        const auto ci  = ts.size() == 1 ? ts[0].find_first_not_of('-') : std::string::npos;
                        if (ci == std::string::npos || ts[0].find_first_not_of('-', ci + 1) != std::string::npos)
                            eligible = false;
                        else {
                            const auto cp = std::find_if(design.pins.begin(), design.pins.end(),
                                                         [&](const auto& p) { return p.name == pla.inputs[ci]; });
                            if (cp != design.pins.end() && cp->number &&
                                std::find(d.clock_pins.begin(), d.clock_pins.end(), cp->number) == d.clock_pins.end())
                                eligible = false;
                        }
                    }
                }
                if (!eligible)
                    continue;
                input_registers[target->name] = pin.name;
                input_pairs[pin.name]         = target->name;
                if (pin.number) {
                    const auto io = std::find(d.io_pins.begin(), d.io_pins.end(), pin.number);
                    if (io == d.io_pins.end())
                        unsupported("input register requires an I/O pin");
                    const auto number = d.blocks * d.cells + unsigned(io - d.io_pins.begin()) + 2;
                    if (target->number && target->number != number)
                        unsupported("conflicting input-register node");
                    target->number = number;
                }
                continue;
            }
            if (!pin.node || pin.pair_kind == "IPAIR" || target == design.pins.end() || target->node)
                unsupported("invalid paired resource " + pin.name);
            if (!outputs.count(pin.name) || !outputs.count(pin.pair))
                unsupported("output pair has no primary equation");
            const auto& node = outputs.at(pin.name);
            const auto& pad  = outputs.at(pin.pair);
            if (node.function != pad.function || pla.terms[node.column] != pla.terms[pad.column])
                continue;
            if (bool(xor_left.count(pin.name)) != bool(xor_left.count(pin.pair)) ||
                (xor_left.count(pin.name) && pla.terms[xor_left.at(pin.name)] != pla.terms[xor_left.at(pin.pair)]))
                unsupported("output pair has different XOR excitation equations");
            for (const auto& control : controls[pin.name]) {
                const auto existing = controls[pin.pair].find(control.first);
                if (existing != controls[pin.pair].end() && pla.terms[existing->second] != pla.terms[control.second])
                    unsupported("output pair has conflicting " + control.first + " controls");
                controls[pin.pair][control.first] = control.second;
            }
            controls[pin.name]       = controls[pin.pair];
            output_aliases[pin.name] = pin.pair;
            paired_pins.insert(pin.pair);
        }
    // The input-pair normalization reserves physical clock resources before
    // the general clock reduction pass. Excess floating pairs become ordinary
    // macrocells (original z5112), retaining their data and clock equations.
    std::set<std::string> paired_clocks, demoted_clocks, fixed_input_clocks;
    std::set<std::string> forced_sync, forced_async;
    for (const auto& group : design.groups) {
        if (group.first == "SYNC_LIST")
            forced_sync.insert(group.second.begin(), group.second.end());
        if (group.first == "ASYNC_LIST")
            forced_async.insert(group.second.begin(), group.second.end());
    }
    for (const auto& out : outputs)
        if (out.second.function != "COM") {
            const auto& ctrl = controls[out.first];
            if (!ctrl.count("C")) {
                paired_clocks.insert("<default>");
                continue;
            }
            const bool both = ctrl.count("RE") && ctrl.count("PR") && !pla.terms[ctrl.at("RE")].empty() &&
                              !pla.terms[ctrl.at("PR")].empty();
            if ((forced_sync.count(out.first) && !forced_async.count(out.first)) || both ||
                pla.terms[out.second.column].size() > 18) {
                const auto& ts = pla.terms[ctrl.at("C")];
                if (ts.size() == 1) {
                    const auto i = ts[0].find_first_not_of('-');
                    if (i != std::string::npos && ts[0].find_first_not_of('-', i + 1) == std::string::npos)
                        paired_clocks.insert(pla.inputs[i]);
                }
            }
        }
    for (const auto& pin : design.pins)
        if (pin.number && std::find(d.clock_pins.begin(), d.clock_pins.end(), pin.number) != d.clock_pins.end())
            paired_clocks.insert(pin.name);
    std::vector<const Pin*> pair_nodes;
    for (const auto& pin : design.pins)
        if (input_registers.count(pin.name))
            pair_nodes.push_back(&pin);
    std::stable_sort(pair_nodes.begin(), pair_nodes.end(), [](const auto* a, const auto* b) {
        if (bool(a->number) != bool(b->number))
            return bool(a->number);
        return a->number && a->number < b->number;
    });
    for (const auto* node : pair_nodes) {
        const auto& pin = *node;
        std::string clock;
        if (controls[pin.name].count("C")) {
            const auto& ts = pla.terms[controls[pin.name].at("C")];
            if (ts.size() == 1) {
                const auto i = ts[0].find_first_not_of('-');
                if (i != std::string::npos)
                    clock = pla.inputs[i];
            }
        }
        if (clock.empty())
            continue;
        if (pin.number)
            fixed_input_clocks.insert(clock);
        if (paired_clocks.count(clock) || paired_clocks.size() < d.clock_pins.size()) {
            paired_clocks.insert(clock);
            continue;
        }
        demoted_clocks.insert(clock);
        const auto pad = input_registers.at(pin.name);
        input_registers.erase(pin.name);
        input_pairs.erase(pad);
        for (auto& declaration : design.pins) {
            if (declaration.name == pad) {
                declaration.pair.clear();
                declaration.pair_kind.clear();
            }
            if (declaration.name == pin.name && declaration.number >= d.blocks * d.cells + 2)
                declaration.number = 0;
        }
    }
    if (fitting.ground_implicit_controls)
        for (const auto& entry : outputs)
            if (entry.second.function != "COM")
                for (const auto* function : {"RE", "PR"})
                    if (!controls[entry.first].count(function)) {
                        controls[entry.first][function] = unsigned(pla.terms.size());
                        pla.terms.emplace_back();
                    }
    std::vector<Pin*> declared;
    for (auto& pin : design.pins)
        declared.push_back(&pin);
    std::stable_sort(declared.begin(), declared.end(), [&](auto* a, auto* b) {
        if (bool(a->number) != bool(b->number))
            return bool(a->number);
        if (!a->number && a->node != b->node)
            return !a->node;
        if (!a->number && a->node && bool(input_registers.count(a->name)) != bool(input_registers.count(b->name)))
            return bool(input_registers.count(a->name));
        return a->number && a->number + (a->node ? d.pins : 0) < b->number + (b->node ? d.pins : 0);
    });
    std::map<std::string, unsigned> ids, feedback_ids;
    unsigned next = d.pins + d.blocks * d.cells + (mach355 ? 0 : unsigned(d.io_pins.size())) + 1;
    for (const auto* pin : declared)
        ids[pin->name] = pin->number ? pin->number - 1 + (pin->node ? d.pins : 0) : next++;
    for (const auto* pin : declared)
        if (!pin->node && !paired_pins.count(pin->name) && outputs.count(pin->name) &&
            outputs.at(pin->name).function != "COM" &&
            std::find(pla.inputs.begin(), pla.inputs.end(), pin->name) != pla.inputs.end())
            feedback_ids[pin->name] = next++;
    std::set<std::string> clocks;
    for (const auto& entry : controls)
        if (entry.second.count("C")) {
            const auto& terms = pla.terms[entry.second.at("C")];
            if (terms.size() != 1)
                continue;
            const auto input = terms[0].find_first_not_of('-');
            if (input != std::string::npos && terms[0].find_first_not_of('-', input + 1) == std::string::npos)
                clocks.insert(pla.inputs[input]);
        }
    std::map<std::string, int> constrained;
    std::set<std::string> low_power;
    for (const auto& group : design.groups)
        if (group.first == "LOW_POWER_LIST")
            low_power.insert(group.second.begin(), group.second.end());
    // Paired NODE and pad name one physical output. The original imports
    // either LOW_POWER request before allocating its product groups.
    for (const auto& alias : output_aliases)
        if (low_power.count(alias.first))
            low_power.insert(alias.second);
    for (const auto& group : design.groups)
        if (group.first.compare(0, 9, "MACH_SEG_") == 0) {
            if (group.first.size() != 10 || group.first.back() < 'A' || unsigned(group.first.back() - 'A') >= d.blocks)
                unsupported("invalid block constraint " + group.first);
            for (const auto& name : group.second) {
                const auto result = constrained.emplace(name, group.first.back() - 'A');
                if (!result.second && result.first->second != group.first.back() - 'A')
                    unsupported("conflicting block constraints");
            }
        }
    // Select forced clocks first, then stably rank remaining clocks by
    // register-user count. Assign physical clock pins in logical-ID order.
    std::set<std::string> synchronous, asynchronous;
    for (const auto& group : design.groups) {
        if (group.first == "SYNC_LIST")
            synchronous.insert(group.second.begin(), group.second.end());
        if (group.first == "ASYNC_LIST")
            asynchronous.insert(group.second.begin(), group.second.end());
    }
    struct ClockCandidate
    {
        std::string name;
        unsigned users = 0;
        bool forced = false, excluded = false;
    };
    std::map<std::string, ClockCandidate> clock_candidates;
    unsigned reserved_default = 0;
    for (const auto& output : outputs) {
        if (output_aliases.count(output.first) || output.second.function == "COM")
            continue;
        const auto& control = controls[output.first];
        if (!control.count("C")) {
            reserved_default = 1;
            continue;
        }
        const auto& terms = pla.terms[control.at("C")];
        if (terms.size() != 1)
            continue;
        const auto input = terms.front().find_first_not_of('-');
        if (input == std::string::npos || terms.front().find_first_not_of('-', input + 1) != std::string::npos)
            continue;
        const auto& name = pla.inputs[input];
        auto& candidate  = clock_candidates[name];
        candidate.name   = name;
        ++candidate.users;
        const auto pin = std::find_if(declared.begin(), declared.end(), [&](const auto* p) { return p->name == name; });
        const bool fixed_global =
            (*pin)->number && std::find(d.clock_pins.begin(), d.clock_pins.end(), (*pin)->number) != d.clock_pins.end();
        candidate.excluded |= demoted_clocks.count(name) || (*pin)->node || outputs.count(name) ||
                              constrained.count(name) || ((*pin)->number && !fixed_global);
        // An input-register pad must stay on an I/O pin: a dedicated clock
        // pin has no input register behind it.
        candidate.excluded |= input_pairs.count(name) != 0;
        const bool async = asynchronous.count(output.first) && !synchronous.count(output.first);
        const bool sync  = synchronous.count(output.first) && !asynchronous.count(output.first);
        candidate.excluded |= async && !fixed_global && !fitting.product_term_clocks;
        const bool both = control.count("RE") && control.count("PR") && !pla.terms[control.at("RE")].empty() &&
                          !pla.terms[control.at("PR")].empty();
        candidate.forced |= fixed_global || input_registers.count(output.first) || sync || both ||
                            pla.terms[output.second.column].size() > 18;
    }
    if (fitting.reduce_global_clocks > 2)
        unsupported("global-clock reduction must be 0, 1 or 2");
    const auto capacity = unsigned(d.clock_pins.size()) - fitting.reduce_global_clocks;
    clocks.clear();
    std::vector<ClockCandidate> candidates;
    for (const auto& item : clock_candidates) {
        const auto& c = item.second;
        if (c.excluded)
            continue;
        if (c.forced)
            clocks.insert(c.name);
        else
            candidates.push_back(c);
    }
    std::stable_sort(candidates.begin(), candidates.end(), [&](const auto& a, const auto& b) {
        return a.users != b.users ? a.users > b.users : ids.at(a.name) < ids.at(b.name);
    });
    for (const auto& candidate : candidates)
        if (clocks.size() + reserved_default < capacity)
            clocks.insert(candidate.name);
    std::vector<Mach111PlaceSignal> signals;
    std::vector<Pin*> included;
    std::map<std::string, std::uint16_t> control_ids;
    bool default_clock = false;
    const auto aliases = pla_signal_aliases(design);
    for (auto* pin : declared) {
        if (output_aliases.count(pin->name))
            continue;
        const bool output = outputs.count(pin->name);
        if (!output && std::find(pla.inputs.begin(), pla.inputs.end(), pin->name) == pla.inputs.end())
            continue;
        Mach111PlaceSignal s;
        s.name            = pin->name;
        s.sort_name       = aliases.at(pin->name);
        s.input_register  = input_registers.count(pin->name);
        s.clocked_storage = output && outputs.at(pin->name).function != "COM";
        s.output          = output && !s.input_register;
        s.buried          = pin->node;
        s.order.serial    = std::int16_t(ids.at(pin->name));
        s.order.type      = s.input_register ? 8 : output ? (pin->node ? 3 : 0) : clocks.count(pin->name) ? 9 : 1;
        if (s.input_register) {
            s.order.product_terms = -1;
            default_clock |= !controls[pin->name].count("C");
        }
        if (constrained.count(pin->name))
            s.order.block = std::int16_t(constrained.at(pin->name));
        if (pin->number) {
            if (pin->node) {
                if (!s.input_register)
                    s.fixed_cell = int(pin->number) - 2;
                s.order.fixed_node = std::int16_t(pin->number + d.pins - 1);
            } else
                s.order.fixed_pin = std::int16_t(pin->number - 1);
        }
        if (output && !s.input_register) {
            for (const auto& alias : output_aliases)
                if (alias.second == pin->name) {
                    s.paired_output = true;
                    const auto node = std::find_if(design.pins.begin(), design.pins.end(),
                                                   [&](const auto& p) { return p.name == alias.first; });
                    if (node->number) {
                        if (s.fixed_cell >= 0 && s.fixed_cell != int(node->number) - 2)
                            unsupported("conflicting paired node locations");
                        s.fixed_cell       = int(node->number) - 2;
                        s.order.fixed_node = std::int16_t(node->number + d.pins - 1);
                    }
                }
            const auto& out       = outputs.at(pin->name);
            s.force_async         = asynchronous.count(pin->name) && !synchronous.count(pin->name);
            s.force_sync          = synchronous.count(pin->name) && !asynchronous.count(pin->name);
            s.xor_product         = xor_left.count(pin->name);
            s.order.product_terms = std::int16_t(std::max<std::size_t>(1, pla.terms[out.column].size()));
            // DOS builds asynchronous resources through 18 data terms without
            // accounting for a separate XOR product. That exceeds the asynchronous
            // table's 17-term limit, although synchronous 18-term allocation is legal.
            if (design.dos_compatibility && s.clocked_storage && s.xor_product && s.order.product_terms == 18)
                unsupported(
                    "z8004: original asynchronous XOR descriptor exceeds18 products; z8007: cannot find a partition");
            std::set<std::uint16_t> fanins;
            auto add = [&](unsigned column) {
                for (const auto& term : pla.terms[column])
                    for (unsigned i = 0; i < term.size(); ++i)
                        if (term[i] != '-')
                            fanins.insert(std::uint16_t(feedback_ids.count(pla.inputs[i])
                                                            ? feedback_ids.at(pla.inputs[i])
                                                            : ids.at(pla.inputs[i])));
            };
            add(out.column);
            if (s.xor_product)
                add(xor_left.at(pin->name));
            for (const auto& control : controls[pin->name])
                if (control.first != "C") {
                    add(control.second);
                    if (control.first == "RE" || control.first == "PR") {
                        std::string key;
                        for (const auto& term : pla.terms[control.second])
                            key += term + ";";
                        const auto id =
                            control_ids.emplace(key, key.empty() ? 0 : std::uint16_t(control_ids.size() + 1))
                                .first->second;
                        (control.first == "RE" ? s.reset : s.preset)               = id;
                        (control.first == "RE" ? s.global_reset : s.global_preset) = id;
                    }
                }
            if (controls[pin->name].count("C")) {
                const auto& terms = pla.terms[controls[pin->name].at("C")];
                const auto input  = terms.size() == 1 ? terms[0].find_first_not_of('-') : std::string::npos;
                s.local_clock     = input == std::string::npos ||
                                    terms[0].find_first_not_of('-', input + 1) != std::string::npos ||
                                    !clocks.count(pla.inputs[input]);
                if (!s.local_clock) {
                    const auto clock_pin = std::find_if(declared.begin(), declared.end(),
                                                        [&](const auto* p) { return p->name == pla.inputs[input]; });
                    if (clock_pin != declared.end() && (*clock_pin)->number)
                        s.local_clock = std::find(d.clock_pins.begin(), d.clock_pins.end(), (*clock_pin)->number) ==
                                        d.clock_pins.end();
                }
            }
            if (s.local_clock && controls[pin->name].count("C"))
                add(controls[pin->name].at("C"));
            if (s.local_clock || s.force_async) {
                s.global_reset.reset();
                s.global_preset.reset();
            }
            s.fanins.assign(fanins.begin(), fanins.end());
            default_clock |= out.function != "COM" && !controls[pin->name].count("C");
        }
        signals.push_back(s);
        included.push_back(pin);
    }
    std::set<std::string> routed_inputs;
    auto collect_routed = [&](unsigned column) {
        for (const auto& term : pla.terms[column])
            for (unsigned i = 0; i < term.size(); ++i)
                if (term[i] != '-')
                    routed_inputs.insert(pla.inputs[i]);
    };
    for (const auto& output : outputs) {
        if (input_registers.count(output.first) || output_aliases.count(output.first))
            continue;
        collect_routed(output.second.column);
        if (xor_left.count(output.first))
            collect_routed(xor_left.at(output.first));
        for (const auto& control : controls[output.first]) {
            if (control.first == "C") {
                const auto& ts = pla.terms[control.second];
                const auto at  = ts.size() == 1 ? ts[0].find_first_not_of('-') : std::string::npos;
                if (at != std::string::npos && ts[0].find_first_not_of('-', at + 1) == std::string::npos &&
                    clocks.count(pla.inputs[at]))
                    continue;
            }
            collect_routed(control.second);
        }
    }
    for (auto& signal : signals) {
        signal.raw_input_live = input_pairs.count(signal.name) && routed_inputs.count(signal.name);
        signal.feedback_live =
            routed_inputs.count(signal.name) &&
            (signal.input_register ||
             (outputs.count(signal.name) &&
              (signal.buried || (outputs.at(signal.name).function != "COM" && !paired_pins.count(signal.name)))));
        for (const auto& alias : output_aliases)
            if (alias.second == signal.name && routed_inputs.count(alias.first))
                signal.feedback_live = true;
    }
    for (unsigned i = 0; i < signals.size(); ++i)
        if (signals[i].input_register) {
            const auto& paired = input_registers.at(signals[i].name);
            const auto target =
                std::find_if(signals.begin(), signals.end(), [&](const auto& signal) { return signal.name == paired; });
            if (target == signals.end())
                unsupported("paired input has no declaration");
            signals[i].paired_input = int(target - signals.begin());
        }
    // Clock assignment precedes control feasibility in the original.
    // Compute the same serial-ordered dedicated-pin choices for partition
    // metadata without changing the user's original placement constraints.
    std::map<std::string, unsigned> clock_numbers;
    std::set<unsigned> occupied;
    for (const auto& signal : signals)
        if (signal.order.fixed_pin >= 0)
            occupied.insert(unsigned(signal.order.fixed_pin) + 1);
    if (default_clock)
        occupied.insert(d.clock_pins.front());
    std::vector<unsigned> clock_signals;
    for (unsigned i = 0; i < signals.size(); ++i)
        if (signals[i].order.type == 9)
            clock_signals.push_back(i);
    std::stable_sort(clock_signals.begin(), clock_signals.end(), [&](auto a, auto b) {
        if (bool(fixed_input_clocks.count(signals[a].name)) != bool(fixed_input_clocks.count(signals[b].name)))
            return bool(fixed_input_clocks.count(signals[a].name));
        return signals[a].order.serial < signals[b].order.serial;
    });
    for (auto i : clock_signals) {
        const auto& signal = signals[i];
        unsigned number    = signal.order.fixed_pin >= 0 ? unsigned(signal.order.fixed_pin) + 1 : 0;
        if (!number)
            for (auto candidate : d.clock_pins)
                if (!occupied.count(candidate)) {
                    number = candidate;
                    occupied.insert(number);
                    break;
                }
        if (number)
            clock_numbers[signal.name] = number;
    }
    for (auto& signal : signals)
        if (signal.order.type == 9 && clock_numbers.count(signal.name))
            signal.assigned_clock_pin = clock_numbers.at(signal.name);
    for (auto& signal : signals)
        if (signal.output || signal.input_register) {
            const auto& out = outputs.at(signal.name);
            if (out.function == "COM")
                continue;
            const auto& control = controls[signal.name];
            if (!control.count("C"))
                signal.global_clock = 0;
            else {
                const auto& terms = pla.terms[control.at("C")];
                const auto input  = terms.size() == 1 ? terms.front().find_first_not_of('-') : std::string::npos;
                if (input == std::string::npos || terms.front().find_first_not_of('-', input + 1) != std::string::npos)
                    continue;
                const auto& name = pla.inputs[input];
                if (!clock_numbers.count(name))
                    continue;
                const auto at = std::find(d.clock_pins.begin(), d.clock_pins.end(), clock_numbers.at(name));
                if (at == d.clock_pins.end())
                    continue;
                signal.global_clock = int(at - d.clock_pins.begin());
                signal.clock_fanin  = std::uint16_t(ids.at(name));
                const auto pin =
                    std::find_if(design.pins.begin(), design.pins.end(), [&](const auto& p) { return p.name == name; });
                signal.clock_positive = (terms.front()[input] == '1') != pin->inverted;
            }
        }
    Mach111PlaceOptions place_options;
    place_options.balanced_partition          = fitting.balanced_partition;
    place_options.spread_placement            = fitting.spread_placement;
    place_options.reserve_default_clock       = default_clock;
    place_options.allow_global_product_clocks = fitting.product_term_clocks;
    place_options.iterate_partition           = fitting.iterate_partition;
    place_options.collect_report_visits       = bool(fitting.report);
    if (fitting.report) {
        for (const auto& entry : ids)
            if (output_aliases.count(entry.first)) {
                FitterOrderKey key;
                key.serial     = std::int16_t(entry.second);
                key.type       = 3;
                const auto pin = std::find_if(declared.begin(), declared.end(),
                                              [&](const auto* p) { return p->name == entry.first; });
                if (pin != declared.end() && (*pin)->node && (*pin)->number)
                    key.fixed_node = std::int16_t(entry.second);
                place_options.report_extra_records.push_back(key);
            }
        for (const auto& entry : feedback_ids) {
            FitterOrderKey key;
            key.serial = std::int16_t(entry.second);
            key.type   = 3;
            place_options.report_extra_records.push_back(key);
        }
    }
    if (fitting.report)
        place_options.partition_observer = [&](const auto& observed_signals, const auto& cluster_signals,
                                               const auto& frame) {
            if (frame.failed_cluster)
                capture_partition_failure_report(*fitting.report, observed_signals, cluster_signals, frame, ids,
                                                 feedback_ids);
        };
    const auto original_controls = controls;
    const auto original_terms    = pla.terms;
    std::optional<FuseImage> fitted_image;
    const auto evaluate_placement = [&](const ModernPlacement& placement, const std::vector<bool>& can_move,
                                        bool failure_dump = false) -> PlacementRouteFeedback {
        FittingLogicalObservationPause report_pause(failure_dump);
        // Diagnostic feasibility routing must not trigger production repartition
        // thresholds or add logical search visits. Preserve the observed counter.
        std::unique_ptr<FittingIterationCounters> reporting_counters;
        if (failure_dump) {
            const auto visits                  = fitting_logical_visits();
            reporting_counters                 = std::make_unique<FittingIterationCounters>(d.blocks, false);
            reporting_counters->logical_visits = visits;
        }
        controls  = original_controls;
        pla.terms = original_terms;

        for (unsigned i = 0; i < signals.size(); ++i) {
            const auto mask = placement.implicit_controls.at(i);
            for (const auto& control : std::array<std::pair<unsigned, const char*>, 2>{{{1, "RE"}, {2, "PR"}}})
                if (mask & control.first) {
                    controls[signals[i].name][control.second] = unsigned(pla.terms.size());
                    pla.terms.emplace_back();
                    control_ids.emplace("", 0);
                }
        }
        for (const auto& alias : output_aliases)
            controls[alias.first] = controls[alias.second];
        std::map<std::string, unsigned> cells, register_ios;
        std::map<std::string, const Pin*> pins;
        for (unsigned i = 0; i < signals.size(); ++i) {
            if (signals[i].output)
                cells[signals[i].name] = unsigned(placement.cells[i]);
            if (signals[i].input_register)
                register_ios[signals[i].name] = unsigned(placement.input_registers[i]);
            included[i]->number   = signals[i].input_register
                                        ? d.blocks * d.cells + unsigned(placement.input_registers[i]) + 2
                                    : signals[i].buried ? unsigned(placement.cells[i]) + 2
                                                        : unsigned(placement.pins[i]);
            pins[signals[i].name] = included[i];
        }
        for (const auto& alias : output_aliases) {
            cells[alias.first] = cells.at(alias.second);
            for (auto& pin : design.pins)
                if (pin.name == alias.first) {
                    pin.number     = cells.at(alias.first) + 2;
                    pins[pin.name] = &pin;
                    break;
                }
        }
        auto io_index = [&](unsigned pin) {
            const auto at = std::find(d.io_pins.begin(), d.io_pins.end(), pin);
            if (at == d.io_pins.end())
                unsupported("pin is not an I/O: " + std::to_string(pin));
            return unsigned(at - d.io_pins.begin());
        };
        auto pin_source = [&](unsigned pin) {
            const auto at = std::find(d.io_pins.begin(), d.io_pins.end(), pin);
            if (at != d.io_pins.end())
                return unsigned(at - d.io_pins.begin()) * (mach355 ? 2 : 3);
            const auto fixed = std::find(d.dedicated_pins.begin(), d.dedicated_pins.end(), pin);
            if (fixed == d.dedicated_pins.end()) {
                if (std::find(d.clock_pins.begin(), d.clock_pins.end(), pin) != d.clock_pins.end())
                    return ~0u;
                unsupported("invalid dedicated input");
            }
            return d.dedicated_sources[unsigned(fixed - d.dedicated_pins.begin())];
        };
        auto hw = empty_mach_device(d);
        if (!hw.global_configuration.empty())
            hw.global_configuration.front() = fitting.zero_hold_time;
        std::fill(hw.low_power.begin(), hw.low_power.end(), 1);
        std::vector<bool> used(d.blocks * d.cells);
        std::vector<int> clock_indices(used.size(), -1), input_clocks(d.io_pins.size(), -1);
        std::vector<bool> input_positive(d.io_pins.size(), true);
        std::vector<bool> local_clocks(used.size()), positive_clocks(used.size(), true);
        std::vector<std::array<int, 4>> clock_polarity(d.blocks, {-1, -1, -1, -1});
        auto reserve_clock = [&](unsigned block, unsigned clock, bool positive) {
            auto& selected = clock_polarity[block][clock];
            if (selected >= 0 && selected != int(positive))
                unsupported("shared block-clock polarity requires placement retry");
            selected = int(positive);
        };
        std::vector<unsigned> modes(used.size(), 0x3cf);
        struct Product
        {
            unsigned block, row, column, term;
        };
        std::vector<Product> products;
        std::map<std::pair<unsigned, unsigned>, std::string> shared;
        auto add_control = [&](unsigned block, unsigned row, unsigned column) {
            if (pla.terms[column].size() > 1)
                unsupported("multi-product control");
            const std::string term = pla.terms[column].empty() ? "" : pla.terms[column][0];
            const auto key         = std::make_pair(block, row);
            const auto result      = shared.emplace(key, term);
            if (!result.second && result.first->second != term)
                unsupported("conflicting shared controls");
            if (!term.empty())
                products.push_back({block, row, column, 0});
        };
        // Retain the partitioner's reference-counted slot history, including
        // moves and swaps that leave the surviving product in the preset slot.
        std::vector<std::array<int, 2>> global_controls(d.blocks, {-1, -1});
        auto control_term = [&](unsigned column) {
            if (pla.terms[column].size() > 1)
                unsupported("multi-product set/reset");
            return pla.terms[column].empty() ? std::string() : pla.terms[column][0];
        };
        std::map<std::uint16_t, unsigned> control_columns;
        for (const auto& entry : controls)
            for (const auto& control : entry.second)
                if (!input_registers.count(entry.first) && (control.first == "RE" || control.first == "PR")) {
                    std::string key;
                    for (const auto& term : pla.terms[control.second])
                        key += term + ";";
                    control_columns.emplace(control_ids.at(key), control.second);
                }
        for (unsigned b = 0; b < d.blocks; ++b)
            for (unsigned index = 0; index < 2; ++index)
                if (placement.global_controls[b][index])
                    global_controls[b][index] = int(control_columns.at(*placement.global_controls[b][index]));
        for (unsigned b = 0; b < d.blocks; ++b)
            for (unsigned index = 0; index < 2; ++index)
                if (global_controls[b][index] >= 0)
                    add_control(b, d.rows - 2 + index, unsigned(global_controls[b][index]));
        for (const auto& entry : outputs) {
            if (output_aliases.count(entry.first) || input_registers.count(entry.first))
                continue;
            const auto& out     = entry.second;
            const unsigned cell = cells.at(out.name), block = cell / d.cells, local = cell % d.cells;
            used[cell] = true;
            if (!hw.low_power.empty())
                hw.low_power[cell] = low_power.count(out.name);
            const auto count = pla.terms[out.column].size();
            const bool split = xor_left.count(out.name);
            unsigned word    = 0x240;
            if (out.function == "COM")
                word |= 0x180;
            else if (out.function == "T")
                word |= 0x80;
            else if (out.function == "G")
                word |= 0x100;
            const bool pin_inverted = pins.at(out.name)->inverted;
            const bool buried       = pins.at(out.name)->node;
            if ((out.function == "T" || buried) ? (out.inverted != out.negative) : (out.inverted != pin_inverted))
                word |= 2;
            // Apply external equation phase to S6 in 22V10-compatible mode.
            // T registers always apply it.
            if (!buried && (out.negative != pin_inverted) && (options.external_best_polarity || out.function == "T"))
                word &= ~64u;
            // Use a cell's auxiliary directly only while it owns that resource.
            // Dense steering can lend even a one-term cell's auxiliary to a neighbour,
            // moving the cell's data into an ordinary OR group.
            const bool direct_aux = placement.product_aux_owner[cell] == int(cell) && placement.product_aux_data[cell];
            if (count > 1 || split || !direct_aux)
                word |= split ? 4 : 5;
            modes[cell]          = word;
            const unsigned first = data_row(local);
            if (split) {
                const auto column = xor_left.at(out.name);
                if (pla.terms[column].size() > 1)
                    unsupported("multi-product XOR left leg");
                if (!pla.terms[column].empty())
                    products.push_back({block, first, column, 0});
            }
            const auto& control = controls[out.name];
            if (!pins.at(out.name)->node && !control.count("OE")) {
                auto& row = hw.blocks[block].products[oe_row(io_index(pins.at(out.name)->number) % ios)];
                std::fill(row.begin(), row.end(), 3);
            }
            if (out.function != "COM") {
                unsigned clock = 0;
                if (control.count("C")) {
                    const auto& terms = pla.terms[control.at("C")];
                    if (terms.size() > 1)
                        unsupported("multi-product clock");
                    const auto i   = terms.empty() ? std::string::npos : terms[0].find_first_not_of('-');
                    bool dedicated = false;
                    if (i != std::string::npos && terms[0].find_first_not_of('-', i + 1) == std::string::npos) {
                        const auto at =
                            std::find(d.clock_pins.begin(), d.clock_pins.end(), pins.at(pla.inputs[i])->number);
                        if (at != d.clock_pins.end() && clocks.count(pla.inputs[i])) {
                            positive_clocks[cell] = (terms[0][i] == '1') != pins.at(pla.inputs[i])->inverted;
                            clock     = unsigned(at - d.clock_pins.begin()) ^ (positive_clocks[cell] ? 0u : 1u);
                            dedicated = true;
                        }
                    }
                    if (!dedicated || placement.product_clock_cells[cell]) {
                        local_clocks[cell] = true;
                        clock              = 3;
                        add_control(block, first + 1, control.at("C"));
                    }
                }
                clock_indices[cell] = int(clock);
            }
            for (const auto& c : control) {
                if (c.first == "OE") {
                    if (pins.at(out.name)->node)
                        unsupported("output enable on buried node");
                    add_control(block, oe_row(io_index(pins.at(out.name)->number) % ios), c.second);
                }
            }
            auto matches_global = [&](const char* fn, unsigned index) {
                if (!control.count(fn))
                    return true;
                const auto term     = control_term(control.at(fn));
                const auto selected = global_controls[block][index];
                return term == (selected < 0 ? std::string() : control_term(unsigned(selected)));
            };
            bool swap              = false;
            const bool force_async = asynchronous.count(out.name) && !synchronous.count(out.name);
            if (!force_async && !local_clocks[cell] && matches_global("RE", 0) && matches_global("PR", 1)) {
            } else if (!force_async && !local_clocks[cell] && matches_global("RE", 1) && matches_global("PR", 0))
                swap = true;
            else {
                const bool reset  = control.count("RE") && !control_term(control.at("RE")).empty();
                const bool preset = control.count("PR") && !control_term(control.at("PR")).empty();
                if (reset && preset)
                    unsupported("independent local reset and preset");
                swap              = preset;
                const auto chosen = control.find(preset ? "PR" : "RE");
                if (chosen != control.end())
                    add_control(block, first + 2, chosen->second);
                modes[cell] &= ~512u;
            }
            if (!(modes[cell] & 512) && !local_clocks[cell] && clock_indices[cell] >= 2) {
                if (!fitting.product_term_clocks)
                    unsupported("global clock cannot supply a local control cell with PTERM_CLK=N");
                if (!control.count("C"))
                    unsupported("local default clock");
                local_clocks[cell]  = true;
                clock_indices[cell] = 3;
                add_control(block, first + 1, control.at("C"));
            }
            if (swap)
                modes[cell] ^= 64;
            if (clock_indices[cell] >= 0 && !local_clocks[cell])
                reserve_clock(block, unsigned(clock_indices[cell]), positive_clocks[cell]);
        }
        for (const auto& entry : input_registers) {
            const auto& name = entry.first;
            const auto io    = register_ios.at(name);
            unsigned clock   = 0;
            if (controls[name].count("C")) {
                const auto& term = pla.terms[controls[name].at("C")].front();
                const auto input = term.find_first_not_of('-');
                const auto* pin  = pins.at(pla.inputs[input]);
                const auto at    = std::find(d.clock_pins.begin(), d.clock_pins.end(), pin->number);
                if (at == d.clock_pins.end())
                    unsupported("input-register clock requires a dedicated pin");
                input_positive[io] = (term[input] == '1') != pin->inverted;
                clock              = unsigned(at - d.clock_pins.begin()) ^ (input_positive[io] ? 0u : 1u);
            }
            input_clocks[io] = int(clock);
            reserve_clock(io / ios, clock, input_positive[io]);
        }
        // Global products are emitted only when a placed register actually
        // uses that control bank; local clocks reserve independent rows.
        for (unsigned block = 0; block < d.blocks; ++block) {
            bool global = false;
            for (const auto& entry : outputs)
                if (!output_aliases.count(entry.first) && !input_registers.count(entry.first) &&
                    entry.second.function != "COM") {
                    const auto cell = cells.at(entry.first);
                    global |= cell / d.cells == block && bool(modes[cell] & 512);
                }
            if (!global) {
                products.erase(std::remove_if(products.begin(), products.end(),
                                              [&](const auto& p) { return p.block == block && p.row >= d.rows - 2; }),
                               products.end());
                shared.erase({block, d.rows - 2});
                shared.erase({block, d.rows - 1});
            }
        }
        std::vector<unsigned> steering_owners(used.size());
        // Choose an adjacent unused node for unowned product groups.
        for (unsigned b = 0; b < d.blocks; ++b) {
            auto& block = hw.blocks[b];
            std::fill(block.mux_codes.begin(), block.mux_codes.end(), (std::uint64_t(1) << d.slots) - 1);
            for (unsigned local = 0; local < d.cells; ++local) {
                const auto cell  = b * d.cells + local;
                const auto owner = placement.product_group_owner[cell];
                int delta        = owner < 0 ? 0 : owner - int(cell);
                if (owner < 0) {
                    for (unsigned distance = 1; distance <= 2 && distance <= local; ++distance)
                        if (!used[cell - distance]) {
                            delta = -int(distance);
                            break;
                        }
                    if (delta == 0 && local + 1 < d.cells && !used[cell + 1])
                        delta = 1;
                    const unsigned first = data_row(local);
                    if (!used[unsigned(int(cell) + delta)])
                        for (unsigned row = first; row < first + 5; ++row) {
                            if (!(modes[cell] & 512) && (row == first + 1 || row == first + 2))
                                continue;
                            std::fill(block.products[row].begin(), block.products[row].end(), 3);
                        }
                }
                steering_owners[cell] = unsigned(int(cell) + delta);
                modes[cell] |= 12;
                if (delta == -2 || delta == 1)
                    modes[cell] &= ~4u;
                if (delta == -2 || delta == 0)
                    modes[cell] &= ~8u;
            }
            // Interleave macrocell and input-register clock balancing.
            std::vector<unsigned> loads(d.clock_pins.size());
            for (unsigned local = 0; local < d.cells; ++local) {
                const auto cell = b * d.cells + local;
                if (clock_indices[cell] >= 0 && !local_clocks[cell]) {
                    ++loads[unsigned(clock_indices[cell])];
                    if (positive_clocks[cell])
                        block.control_word |= std::uint8_t(1u << clock_indices[cell]);
                }
            }
            if (!mach355)
                for (unsigned io = 0; io < ios; ++io)
                    if (input_clocks[b * ios + io] >= 0) {
                        const auto clock = unsigned(input_clocks[b * ios + io]);
                        ++loads[clock];
                        if (input_positive[b * ios + io])
                            block.control_word |= std::uint8_t(1u << clock);
                    }
            auto least = [&]() {
                const auto clock = unsigned(std::min_element(loads.begin(), loads.end()) - loads.begin());
                ++loads[clock];
                return clock;
            };
            for (unsigned local = 0; local < d.cells; ++local) {
                const auto cell  = b * d.cells + local;
                const auto clock = clock_indices[cell] < 0 ? least() : unsigned(clock_indices[cell]);
                modes[cell] |= clock << 4;
                if (!mach355 && local < 8) {
                    const auto input_clock =
                        input_clocks[b * ios + local] < 0 ? least() : unsigned(input_clocks[b * ios + local]);
                    modes[b * d.cells + 2 * local] |= (input_clock >> 1) << 13;
                    modes[b * d.cells + 2 * local + 1] |= (input_clock & 1) << 13;
                }
            }
            for (unsigned io = 0; io < ios; ++io) {
                const auto selection = placement.output_muxes[b * ios + io];
                if (selection >= 0)
                    modes[b * d.cells + (mach355 ? io : 2 * io)] |= unsigned(selection) << 10;
                if (!mach355)
                    modes[b * d.cells + 2 * io + 1] |= 7u << 10;
            }
        }
        // Scan physical product rows rather than allocator units. The first row
        // can join an OR group unless another output owns its XOR leg. Local
        // clock and reset controls independently reserve the next two rows.
        for (const auto& entry : outputs) {
            if (output_aliases.count(entry.first) || input_registers.count(entry.first))
                continue;
            const auto& out     = entry.second;
            const unsigned cell = cells.at(out.name), block = cell / d.cells;
            const auto count = pla.terms[out.column].size();
            unsigned term    = 0;
            if (count <= 1 && !xor_left.count(out.name) && placement.product_aux_owner[cell] == int(cell) &&
                placement.product_aux_data[cell]) {
                if (count)
                    products.push_back({block, data_row(cell % d.cells), out.column, term++});
            } else
                for (unsigned group = 0; group < d.cells && term < count; ++group) {
                    const unsigned index = block * d.cells + group;
                    if (placement.product_group_owner[index] != int(cell))
                        continue;
                    const auto aux = placement.product_aux_owner[index];
                    for (unsigned offset = 0; offset < 5 && term < count; ++offset) {
                        if (offset == 0 && aux >= 0 && (aux != int(cell) || !placement.product_aux_data[index]))
                            continue;
                        if ((offset == 1 || offset == 2) && !(modes[index] & 512))
                            continue;
                        products.push_back({block, data_row(group) + offset, out.column, term++});
                    }
                }
            if (term != count)
                unsupported("allocated product rows are insufficient");
        }
        if (!hw.low_power.empty()) {
            // Derive physical power from the final steering owner, including unused
            // group steering. The resident's request matters only while its auxiliary
            // or local control rows remain active. Never merge requests transitively.
            const auto requested = hw.low_power;
            for (unsigned group = 0; group < used.size(); ++group) {
                hw.low_power[group] = requested[steering_owners[group]];
                if (!(modes[group] & 1) || !(modes[group] & 512))
                    hw.low_power[group] &= requested[group];
            }
        }
        for (const auto& entry : input_registers) {
            const auto io = register_ios.at(entry.first), word = (io / ios) * d.cells + 2 * (io % ios) + 1;
            if (outputs.at(entry.first).function == "REG")
                modes[word] &= ~(1u << 12);
        }
        auto input_pair_phase_flip = [&](const std::string& name) {
            if (!design.dos_compatibility || !automatic_input_pairs.count(name) || !controls[name].count("C"))
                return false;
            const auto& data  = pla.terms[outputs.at(name).column].front();
            const auto& clock = pla.terms[controls[name].at("C")].front();
            // DOS input pairing mistakenly reads data polarity using the clock
            // product index. Compatibility mode preserves that indexing defect.
            return (data[data.find_first_not_of('-')] == '0') != (clock[clock.find_first_not_of('-')] == '0');
        };
        const auto modes_before_inputs = modes;
        std::vector<RouteSourceChoice> source_choices;
        std::map<unsigned, unsigned> source_choice_words;
        std::vector<unsigned> sources;
        std::vector<unsigned> imx_congested_blocks;
        std::set<unsigned> unavailable_inputs;
        std::vector<bool> inverted;
        std::vector<FitterOrderKey> keys;
        std::vector<std::vector<bool>> fanout(pla.inputs.size(), std::vector<bool>(d.blocks));
        // Check live feedback records rather than occupied macrocells. An output
        // pair can drive its pad while leaving the buried feedback path available
        // to the raw input of an adjacent input register.
        std::vector<bool> feedback_used(used.size());
        for (unsigned i = 0; i < pla.inputs.size(); ++i) {
            const auto& name = pla.inputs[i];
            if (input_registers.count(name) || !outputs.count(name))
                continue;
            const auto* pin     = pins.at(name);
            const bool feedback = pin->node || (outputs.at(name).function != "COM" && !paired_pins.count(name));
            if (feedback && std::any_of(products.begin(), products.end(), [&](const auto& product) {
                    return pla.terms[product.column][product.term][i] != '-';
                }))
                feedback_used[cells.at(name)] = true;
        }
        for (unsigned i = 0; i < pla.inputs.size(); ++i) {
            const auto& name          = pla.inputs[i];
            const auto* pin           = pins.at(name);
            const bool input_register = input_registers.count(name);
            const bool feedback       = !input_register && outputs.count(name) &&
                                        (pin->node || (outputs.at(name).function != "COM" && !paired_pins.count(name)));
            const auto cell           = feedback ? cells.at(name) : 0;
            unsigned source           = input_register ? 3 * register_ios.at(name)
                                        : feedback     ? (mach355 ? 2 * cell + 1 : 3 * (cell / 2) + 1 + (cell & 1))
                                                       : pin_source(pin->number);
            if (input_pairs.count(name) && std::any_of(products.begin(), products.end(), [&](const auto& product) {
                    return pla.terms[product.column][product.term][i] != '-';
                })) {
                const auto io = register_ios.at(input_pairs.at(name)), even = (io / ios) * d.cells + 2 * (io % ios);
                if (!feedback_used[even]) {
                    source = 3 * io + 1;
                    modes[even + 1] &= ~(1u << 11);
                    if (!feedback_used[even + 1]) {
                        source_choices.push_back({source, {source, source + 1}, {3 * io, source}});
                        source_choice_words[source] = even + 1;
                    }
                } else if (!feedback_used[even + 1]) {
                    source = 3 * io + 2;
                    modes[even + 1] &= ~(1u << 10);
                } else if (failure_dump) {
                    unavailable_inputs.insert(i);
                    const unsigned block = io / ios;
                    if (std::find(imx_congested_blocks.begin(), imx_congested_blocks.end(), block) ==
                        imx_congested_blocks.end())
                        imx_congested_blocks.push_back(block);
                } else
                    unsupported("paired raw input requires a free feedback path");
            }
            sources.push_back(source);
            // MACH3/4 polarity is before the register/feedback path. Unlike
            // MACH1, a negative selected cover does not invert feedback literals.
            bool phase = pin->node && !output_aliases.count(name) ? outputs.at(name).negative : pin->inverted;
            if (output_aliases.count(name)) {
                const auto& pad = output_aliases.at(name);
                phase           = (outputs.at(name).negative != outputs.at(pad).negative) != pins.at(pad)->inverted;
            }
            if (input_register) {
                const auto& out  = outputs.at(name);
                const auto& term = pla.terms[out.column].front();
                phase            = out.inverted != pins.at(input_registers.at(name))->inverted;
                phase            = (phase != (term[term.find_first_not_of('-')] == '0')) != input_pair_phase_flip(name);
            }
            inverted.push_back(phase);
            if (!mach355 && !pin->node && !feedback && !input_pairs.count(name)) {
                const auto at = std::find(d.io_pins.begin(), d.io_pins.end(), pin->number);
                if (at != d.io_pins.end()) {
                    const unsigned io   = unsigned(at - d.io_pins.begin());
                    const unsigned even = (io / ios) * d.cells + 2 * (io % ios), word = even + 1;
                    modes[word] &= ~(3u << 10);
                    if (std::any_of(products.begin(), products.end(), [&](const auto& product) {
                            return pla.terms[product.column][product.term][i] != '-';
                        })) {
                        // MACH4 input-mux selector 0 routes the raw pad directly; selectors 1/2
                        // use an idle feedback path. Try these alternatives before outer placement
                        // retries, even without an input register. Only paired macrocell feedback
                        // belongs to the pad's relevance test; independent buried nodes do not.
                        RouteSourceChoice choice{source, {source}, {source}};
                        if (!feedback_used[even])
                            choice.alternatives.push_back(3 * io + 1);
                        if (!feedback_used[even + 1])
                            choice.alternatives.push_back(3 * io + 2);
                        if (choice.alternatives.size() > 1) {
                            source_choices.push_back(std::move(choice));
                            source_choice_words[source] = word;
                        }
                    }
                }
            }
            for (const auto& product : products)
                if (pla.terms[product.column][product.term][i] != '-')
                    fanout[i][product.block] = true;
            FitterOrderKey key;
            key.type      = input_register ? 8 : outputs.count(name) ? (feedback ? 3 : 5) : clocks.count(name) ? 9 : 1;
            key.serial    = std::int16_t(feedback_ids.count(name) ? feedback_ids.at(name) : ids.at(name));
            key.fixed_pin = pin->node ? -1 : std::int16_t(pin->number - 1);
            const auto original_declaration =
                std::find_if(original.pins.begin(), original.pins.end(), [&](const auto& p) { return p.name == name; });
            key.fixed_node = pin->node && original_declaration != original.pins.end() && original_declaration->number
                                 ? std::int16_t(original_declaration->number + d.pins - 1)
                                 : -1;
            key.block      = input_register        ? std::int16_t(register_ios.at(name) / ios)
                             : outputs.count(name) ? std::int16_t(cells.at(name) / d.cells)
                                                   : -1;
            if (!outputs.count(name)) {
                const auto original_pin = std::find_if(original.pins.begin(), original.pins.end(),
                                                       [&](const auto& p) { return p.name == name; });
                if (original_pin != original.pins.end() && original_pin->number) {
                    const auto io = std::find(d.io_pins.begin(), d.io_pins.end(), original_pin->number);
                    if (io != d.io_pins.end())
                        key.block = std::int16_t(unsigned(io - d.io_pins.begin()) / ios);
                }
                if (constrained.count(name))
                    key.block = std::int16_t(constrained.at(name));
            }
            key.fanout_count = std::int16_t(std::count(fanout[i].begin(), fanout[i].end(), true));
            if (outputs.count(name) && !input_register)
                key.product_terms = std::int16_t(pla.terms[outputs.at(name).column].size());
            keys.push_back(key);
        }
        auto order = fitter_signal_order(keys, false);
        std::reverse(order.begin(), order.end());
        // Sort the complete imported signal array. Mixed type-1/type-8 comparisons
        // are not antisymmetric, so sorting only PLA columns changes their order.
        // Retain the placer array and separate feedback-alias records.
        if (!input_registers.empty()) {
            std::vector<std::size_t> combined;
            for (auto at = placement.global_order.rbegin(); at != placement.global_order.rend(); ++at) {
                const auto input = std::find(pla.inputs.begin(), pla.inputs.end(), signals[*at].name);
                if (input == pla.inputs.end())
                    continue;
                const auto i = std::size_t(input - pla.inputs.begin());
                if (keys[i].type == 1 || keys[i].type == 2 || keys[i].type == 8 || keys[i].type == 9)
                    combined.push_back(i);
            }
            for (auto i : order)
                if (keys[i].type != 1 && keys[i].type != 2 && keys[i].type != 8 && keys[i].type != 9)
                    combined.push_back(i);
            order = std::move(combined);
        }
        unsigned max_source = 0;
        for (const auto& table : d.mux_sources)
            for (auto source : table)
                if (source != 1000)
                    max_source = std::max(max_source, source);
        std::vector<RouteProblem> route_problems;
        std::vector<std::vector<unsigned>> route_inputs;
        for (unsigned b = 0; b < d.blocks; ++b) {
            RouteProblem problem;
            problem.mux_count = d.muxes;
            problem.sources.resize(max_source + 1);
            for (unsigned mux = 0; mux < d.muxes; ++mux)
                for (unsigned slot = 0; slot < d.slots; ++slot)
                    problem.sources[d.source(b, mux, slot)].muxes.push_back(mux);
            for (auto& source : problem.sources)
                source.primary_count = unsigned(source.muxes.size());
            std::vector<unsigned> inputs;
            for (auto input : order)
                if (fanout[input][b] && !unavailable_inputs.count(unsigned(input))) {
                    if (sources[input] > max_source)
                        unsupported("dedicated clock has no CSM data connection");
                    inputs.push_back(unsigned(input));
                    problem.signal_sources.push_back(sources[input]);
                }
            problem.allow_alternates.resize(inputs.size());
            route_problems.push_back(std::move(problem));
            route_inputs.push_back(std::move(inputs));
        }
        InputPinProblem input_problem;
        input_problem.blocks        = route_problems;
        input_problem.block_signals = route_inputs;
        input_problem.pins.resize(pla.inputs.size());
        input_problem.candidates.resize(pla.inputs.size());
        input_problem.feedback.resize(pla.inputs.size());
        input_problem.occupied_pins.assign(d.pins + 1, false);
        input_problem.pin_sources.assign(d.pins + 1, -1);
        for (unsigned pin : d.io_pins)
            input_problem.pin_sources[pin] = int(pin_source(pin));
        for (unsigned pin : d.dedicated_pins)
            input_problem.pin_sources[pin] = int(pin_source(pin));
        if (place_options.reserve_default_clock)
            input_problem.occupied_pins[d.clock_pins.front()] = true;
        for (unsigned i = 0; i < signals.size(); ++i)
            if (placement.pins[i] > 0)
                input_problem.occupied_pins[unsigned(placement.pins[i])] = true;
        for (unsigned i = 0; i < pla.inputs.size(); ++i) {
            const auto& name          = pla.inputs[i];
            const auto* pin           = pins.at(name);
            input_problem.pins[i]     = pin->node ? 0 : pin->number;
            input_problem.feedback[i] = keys[i].type == 3;
            const auto found =
                std::find_if(signals.begin(), signals.end(), [&](const auto& v) { return v.name == name; });
            if (found == signals.end() || found->output || found->input_register || found->order.type == 9 ||
                found->order.fixed_pin >= 0 || input_pairs.count(name))
                continue;
            for (unsigned candidate = 1; candidate <= d.pins; ++candidate) {
                if (input_problem.pin_sources[candidate] < 0)
                    continue;
                const auto io = std::find(d.io_pins.begin(), d.io_pins.end(), candidate);
                if (found->order.block >= 0 &&
                    (io == d.io_pins.end() || int(unsigned(io - d.io_pins.begin()) / ios) != found->order.block))
                    continue;
                input_problem.candidates[i].push_back(candidate);
            }
            input_problem.occupied_pins[pin->number] = false;
        }
        for (auto i : placement.global_order) {
            const auto found = std::find(pla.inputs.begin(), pla.inputs.end(), signals[i].name);
            if (found != pla.inputs.end()) {
                const auto index = unsigned(found - pla.inputs.begin());
                if (!input_problem.candidates[index].empty())
                    input_problem.placement_order.push_back(index);
            }
        }
        RouteChoiceResult chosen;
        std::vector<RouteSourceChoice> current_choices;
        std::map<unsigned, unsigned> current_words;
        input_problem.route_attempt = [&](const std::vector<unsigned>& assigned) {
            auto problems = route_problems;
            current_choices.clear();
            current_words.clear();
            std::set<unsigned> moving_sources;
            for (auto i : input_problem.placement_order)
                moving_sources.insert(sources[i]);
            for (const auto& choice : source_choices)
                if (!moving_sources.count(choice.source)) {
                    current_choices.push_back(choice);
                    current_words[choice.source] = source_choice_words.at(choice.source);
                }
            for (auto i : input_problem.placement_order) {
                const unsigned source = unsigned(input_problem.pin_sources[assigned[i]]);
                for (unsigned b = 0; b < d.blocks; ++b)
                    for (unsigned n = 0; n < route_inputs[b].size(); ++n)
                        if (route_inputs[b][n] == i)
                            problems[b].signal_sources[n] = source;
                const auto io = std::find(d.io_pins.begin(), d.io_pins.end(), assigned[i]);
                if (mach355 || io == d.io_pins.end())
                    continue;
                const unsigned index = unsigned(io - d.io_pins.begin()),
                               even  = (index / ios) * d.cells + 2 * (index % ios);
                RouteSourceChoice choice{source, {source}, {source}};
                if (!feedback_used[even])
                    choice.alternatives.push_back(3 * index + 1);
                if (!feedback_used[even + 1])
                    choice.alternatives.push_back(3 * index + 2);
                if (choice.alternatives.size() > 1) {
                    current_choices.push_back(std::move(choice));
                    current_words[source] = even + 1;
                }
            }
            std::sort(current_choices.begin(), current_choices.end(),
                      [](const auto& a, const auto& b) { return a.source < b.source; });
            chosen = route_source_choices(problems, current_choices, fitting.source_route_limit);
            return InputPinAttempt{chosen.blocks, chosen.signal_sources};
        };
        std::vector<int> logical_owner(pla.inputs.size(), -1);
        input_problem.external_movable.assign(pla.inputs.size(), false);
        for (unsigned i = 0; i < pla.inputs.size(); ++i) {
            auto name = pla.inputs[i];
            if (output_aliases.count(name))
                name = output_aliases.at(name);
            const auto signal =
                std::find_if(signals.begin(), signals.end(), [&](const auto& v) { return v.name == name; });
            if (signal != signals.end()) {
                const auto at =
                    signal->input_register ? unsigned(signal->paired_input) : unsigned(signal - signals.begin());
                logical_owner[i]                  = int(at);
                input_problem.external_movable[i] = can_move[at];
            }
        }
        for (auto at : placement.global_order)
            for (unsigned i = 0; i < logical_owner.size(); ++i)
                if (logical_owner[i] == int(at))
                    input_problem.retry_order.push_back(i);
        if (!placement.reverse_visit_costs.empty()) {
            input_problem.reverse_visit_costs.resize(pla.inputs.size());
            for (unsigned i = 0; i < logical_owner.size(); ++i)
                if (logical_owner[i] >= 0)
                    input_problem.reverse_visit_costs[i] = placement.reverse_visit_costs.at(unsigned(logical_owner[i]));
        }
        const auto routed         = route_input_pins(input_problem);
        const auto capture_report = [&](FuseImage& image, bool successful) {
            if (!fitting.report)
                return;
            FitterReportData report;
            report.optimized_pla        = fitting.report->optimized_pla;
            report.optimized_design     = fitting.report->optimized_design;
            report.routing_attempts     = chosen.failed_sweeps;
            report.placement_attempts   = fitting_logical_visits();
            report.successful           = successful;
            report.imx_congested_blocks = imx_congested_blocks;
            report.blocks.resize(d.blocks);
            for (unsigned block = 0; block < d.blocks; ++block)
                report.blocks[block].partition_clusters = placement.partition_product_groups.at(block);
            report.group_owner     = placement.product_group_owner;
            report.auxiliary_owner = placement.product_aux_owner;
            report.auxiliary_data  = placement.product_aux_data;
            for (const auto& entry : outputs) {
                if (output_aliases.count(entry.first))
                    continue;
                const auto& out = entry.second;
                FitterReportNode node;
                node.name           = out.name;
                node.input_register = input_registers.count(out.name) != 0;
                if (node.input_register) {
                    const auto io = register_ios.at(out.name);
                    node.block    = int(io / ios);
                    node.cell     = int(io % ios);
                    node.number   = int(d.blocks * d.cells + io + 2);
                    node.pin      = int(pins.at(input_registers.at(out.name))->number);
                } else {
                    const auto cell   = cells.at(out.name);
                    node.block        = int(cell / d.cells);
                    node.cell         = int(cell % d.cells);
                    node.number       = int(cell + 2);
                    node.pin          = pins.at(out.name)->node ? -1 : int(pins.at(out.name)->number);
                    node.asynchronous = !(modes[cell] & 512);
                    node.low_power    = !hw.low_power.empty() && hw.low_power[cell];
                    // The selected auxiliary input, not the source term count,
                    // distinguishes a product XOR input from grounded XOR input.
                    // Borrowing can move even a one-term equation into the OR leg.
                    node.xor_mode = (modes[cell] & 1) ? 'G' : 'P';
                    if (d.device == "MACH446")
                        node.power_annotation = mach446_power_annotation(
                            cell, Mach446PowerRequest::source(low_power.count(out.name) != 0), hw, steering_owners);
                }
                node.implied              = !pins.at(out.name)->node;
                node.paired               = node.pin >= 0;
                node.explicit_output_pair = paired_pins.count(out.name) != 0;
                for (const auto& alias : output_aliases)
                    if (alias.second == out.name &&
                        std::find(pla.inputs.begin(), pla.inputs.end(), alias.first) != pla.inputs.end()) {
                        node.name    = alias.first;
                        node.implied = false;
                        break;
                    }
                node.logic = out.function == "COM" ? 'C' : out.function == "T" ? 'T' : out.function == "G" ? 'L' : 'D';
                // Report the external phase, including declared pin inversion.
                // Buried nodes always report H.
                node.negative      = !pins.at(out.name)->node && (out.negative != pins.at(out.name)->inverted);
                node.product_terms = node.input_register ? 0 : unsigned(pla.terms[out.column].size());
                if (xor_left.count(out.name))
                    node.xor_terms = unsigned(pla.terms[xor_left.at(out.name)].size());
                if (!node.input_register)
                    report.blocks[unsigned(node.block)].product_terms += node.product_terms + node.xor_terms;
                auto append_column = [&](unsigned column) {
                    for (const auto& term : pla.terms[column])
                        for (unsigned i = 0; i < term.size(); ++i)
                            if (term[i] != '-' &&
                                std::find(node.fanins.begin(), node.fanins.end(), pla.inputs[i]) == node.fanins.end())
                                node.fanins.push_back(pla.inputs[i]);
                };
                append_column(out.column);
                if (xor_left.count(out.name))
                    append_column(xor_left.at(out.name));
                node.data_fanins = node.fanins;
                for (const auto& control : controls[out.name]) {
                    if (control.first != "C" || (!node.input_register && local_clocks[cells.at(out.name)]))
                        append_column(control.second);
                    std::string key;
                    for (const auto& term : pla.terms[control.second])
                        key += term + ";";
                    if (control.first == "RE")
                        node.reset = key;
                    if (control.first == "PR")
                        node.preset = key;
                    if (control.first == "OE")
                        node.oe_expression = key;
                }
                if (out.function != "COM") {
                    if (node.input_register)
                        node.clock = "Ck" + std::to_string(input_clocks[register_ios.at(out.name)]);
                    else {
                        const auto cell = cells.at(out.name);
                        node.clock      = local_clocks[cell] ? "P"
                                          : clock_indices[cell] > 0
                                              ? "Ck" + std::to_string(clock_indices[cell])
                                              : std::string(1, char('A' + node.block)) + std::to_string(node.cell);
                    }
                }
                if (!node.input_register) {
                    const auto cell    = cells.at(out.name);
                    auto control_label = [&](const char* kind, const std::string& key) {
                        if (key.empty())
                            return out.function != "COM" ? std::string("G") : std::string{};
                        if (!(modes[cell] & 512))
                            return std::string("P");
                        const auto column = controls[out.name].at(kind);
                        for (unsigned slot = 0; slot < 2; ++slot)
                            if (global_controls[node.block][slot] >= 0 &&
                                control_term(column) == control_term(unsigned(global_controls[node.block][slot])))
                                return std::string("B") + std::to_string(slot);
                        return std::string{};
                    };
                    node.reset  = control_label("RE", node.reset);
                    node.preset = control_label("PR", node.preset);
                    if (node.negative && (options.external_best_polarity || out.function == "T"))
                        std::swap(node.reset, node.preset);
                }
                report.nodes.push_back(std::move(node));
            }
            for (unsigned b = 0; b < d.blocks; ++b) {
                for (unsigned i = 0; i < route_inputs[b].size(); ++i) {
                    const auto input = route_inputs[b][i];
                    FitterReportRoute route;
                    route.signal = pla.inputs[input];
                    route.mux    = routed.blocks[b].signal_to_mux[i];
                    route.source = int(routed.signal_sources[b][i]);
                    if (keys[input].type == 3 && cells.count(route.signal)) {
                        const auto cell    = cells.at(route.signal);
                        route.source_block = int(cell / d.cells);
                        route.source_cell  = int(cell % d.cells);
                    } else
                        route.source_pin = int(pins.at(route.signal)->number);
                    report.blocks[b].routes.push_back(std::move(route));
                }
                for (auto input : unavailable_inputs)
                    if (fanout[input][b]) {
                        FitterReportRoute route;
                        route.signal     = pla.inputs[input];
                        route.source_pin = int(pins.at(route.signal)->number);
                        report.blocks[b].routes.push_back(std::move(route));
                    }
                for (unsigned clock = 0; clock < clock_polarity[b].size(); ++clock)
                    if (clock_polarity[b][clock] >= 0) {
                        const auto pin = d.clock_pins.at(clock);
                        report.blocks[b].clocks.push_back(
                            {image.pin_signals[pin], int(clock), int(pin), !bool(clock_polarity[b][clock])});
                    }
            }
            finalize_fitter_report_data(design, image, report);
            for (auto& pin : report.pins)
                pin.retained_clock = retained_clock_pins.count(pin.name) != 0;
            if (successful && d.device == "MACH446")
                report.estimated_static_icc_milliamps = corrected_mach446_icc(hw);
            if (!successful) {
                report.partitioned = report.partition_total = unsigned(report.nodes.size());
                report.placed = report.placement_total = unsigned(report.nodes.size() + report.pins.size());
                for (const auto& b : routed.blocks) {
                    report.routing_total += unsigned(b.signal_to_mux.size());
                    report.routed += unsigned(std::count_if(b.signal_to_mux.begin(), b.signal_to_mux.end(),
                                                            [](int mux) { return mux >= 0; }));
                }
                for (auto input : unavailable_inputs)
                    for (bool used : fanout[input])
                        if (used)
                            ++report.routing_total;
                report.retained_image = std::make_shared<FuseImage>(image);
            }
            *fitting.report     = report;
            image.fitter_report = std::move(report);
        };
        if (fitting.report && (!routed.success || failure_dump)) {
            for (auto input : input_problem.placement_order)
                for (auto& declaration : design.pins)
                    if (declaration.name == pla.inputs[input])
                        declaration.number = routed.pins[input];
            for (unsigned b = 0; b < d.blocks; ++b)
                for (unsigned c = 0; c < d.cells; ++c)
                    hw.blocks[b].macrocell_words[c] = std::uint16_t(modes[b * d.cells + c]);
            FuseImage trial;
            trial.device    = d.device;
            trial.design    = design.chip;
            trial.pin_count = d.pins;
            std::set<std::string> active(pla.inputs.begin(), pla.inputs.end());
            for (const auto& entry : outputs)
                active.insert(entry.first);
            retain_removed_clocks(active);
            apply_placement_metadata(trial, design, active);
            capture_report(trial, false);
        }
        if (failure_dump)
            return {false, -1};
        if (!routed.success) {
            if (routed.outer_signal >= 0)
                return {false, logical_owner[unsigned(routed.outer_signal)]};
            // If no live fanin owner can move, retry the last movable output
            // assigned to the failed block.
            int failed = -1;
            for (unsigned b = 0; b < routed.blocks.size(); ++b)
                if (routed.blocks[b].unrouted) {
                    failed = int(b);
                    break;
                }
            for (auto at = placement.global_order.rbegin(); at != placement.global_order.rend(); ++at)
                if (signals[*at].output && can_move[*at] && placement.cells[*at] / int(d.cells) == failed)
                    return {false, int(*at)};
            return {false, -1};
        }
        for (auto i : input_problem.placement_order) {
            const unsigned old = input_problem.pins[i], pin = routed.pins[i];
            if (!mach355) {
                const auto old_io = std::find(d.io_pins.begin(), d.io_pins.end(), old);
                if (old_io != d.io_pins.end()) {
                    const unsigned io   = unsigned(old_io - d.io_pins.begin()),
                                   word = (io / ios) * d.cells + 2 * (io % ios) + 1;
                    modes[word]         = (modes[word] & ~(3u << 10)) | (modes_before_inputs[word] & (3u << 10));
                }
            }
            for (auto& declaration : design.pins)
                if (declaration.name == pla.inputs[i])
                    declaration.number = pin;
        }
        if (!mach355)
            for (auto i : input_problem.placement_order) {
                const auto at = std::find(d.io_pins.begin(), d.io_pins.end(), routed.pins[i]);
                if (at == d.io_pins.end())
                    continue;
                const unsigned io = unsigned(at - d.io_pins.begin());
                modes[(io / ios) * d.cells + 2 * (io % ios) + 1] &= ~(3u << 10);
            }
        for (unsigned i = 0; i < current_choices.size(); ++i) {
            const auto word   = current_words.at(current_choices[i].source);
            const auto source = current_choices[i].alternatives[chosen.selected[i]];
            modes[word]       = (modes[word] & ~(3u << 10)) | ((source % 3) << 10);
        }
        for (unsigned b = 0; b < d.blocks; ++b) {
            const auto& inputs = route_inputs[b];
            const auto& route  = routed.blocks[b];
            std::map<unsigned, unsigned> input_mux;
            for (unsigned i = 0; i < inputs.size(); ++i) {
                const auto mux       = unsigned(route.signal_to_mux[i]);
                input_mux[inputs[i]] = mux;
                unsigned slot        = 0;
                while (slot < d.slots && d.source(b, mux, slot) != routed.signal_sources[b][i])
                    ++slot;
                hw.blocks[b].mux_codes[mux] = d.mux_code(mux, slot);
            }
            for (const auto& product : products)
                if (product.block == b) {
                    auto& row = hw.blocks[b].products[product.row];
                    std::fill(row.begin(), row.end(), 3);
                    const auto& term = pla.terms[product.column][product.term];
                    for (unsigned i = 0; i < term.size(); ++i)
                        if (term[i] != '-')
                            row[input_mux.at(i)] = ((term[i] == '1') != inverted[i]) ? 2 : 1;
                }
            for (const auto& entry : outputs) {
                if (output_aliases.count(entry.first) || input_registers.count(entry.first))
                    continue;
                const auto& out = entry.second;
                const auto cell = cells.at(out.name);
                if (cell / d.cells == b && pla.terms[out.column].empty() && !xor_left.count(out.name) &&
                    placement.product_aux_owner[cell] == int(cell) && placement.product_aux_data[cell]) {
                    const auto local = cell % d.cells;
                    std::fill(hw.blocks[b].products[data_row(local)].begin(),
                              hw.blocks[b].products[data_row(local)].end(), 0);
                }
            }
            for (unsigned local = 0; local < d.cells; ++local)
                hw.blocks[b].macrocell_words[local] = std::uint16_t(modes[b * d.cells + local]);
        }
        check_fitting_budget();
        auto image = encode_mach_device(d, hw, design.chip);
        for (const auto& entry : outputs) {
            const auto& out = entry.second;
            if (input_registers.count(out.name)) {
                auto& model          = image.simulation_model.registers[out.name];
                model.input_register = true;
                const auto& term     = pla.terms[out.column].front();
                const bool phase = ((pins.at(out.name)->inverted != pins.at(input_registers.at(out.name))->inverted) !=
                                    (term[term.find_first_not_of('-')] == '0')) != input_pair_phase_flip(out.name);
                model.power_up = model.reset_value = simulation_bit(phase);
                model.preset_value                 = simulation_bit(!phase);
                model.next_state                   = simulation_cover(pla.inputs, pla.terms[out.column]);
                if (pins.at(out.name)->inverted != input_pair_phase_flip(out.name))
                    model.next_state = make_not(model.next_state);
                model.reset = model.preset = make_constant(false);
                model.clock = controls[out.name].count("C")
                                  ? simulation_cover(pla.inputs, pla.terms[controls[out.name].at("C")])
                                  : simulation_clock_input(image.simulation_model, design, d.clock_pins.front());
                continue;
            }
            const auto cell = cells.at(out.name), block = cell / d.cells, local = cell % d.cells;
            auto expression = simulation_cover(pla.inputs, pla.terms[out.column]);
            if (xor_left.count(out.name))
                expression = make_binary(ExprKind::Xor, simulation_cover(pla.inputs, pla.terms[xor_left.at(out.name)]),
                                         expression);
            if (out.inverted != (out.function == "T" && out.negative))
                expression = make_not(expression);
            if (out.function == "COM") {
                image.simulation_model.combinatorial[out.name] = expression;
                continue;
            }
            auto& model = image.simulation_model.registers[out.name];
            // Buried feedback uses the synthesized storage phase, not the NODE
            // declaration's observation phase. S0/internal XOR complementation
            // changes the D input but not the flip-flop's power-up value.
            bool phase = pins.at(out.name)->node ? out.negative : pins.at(out.name)->inverted;
            if (output_aliases.count(out.name)) {
                const auto& pad = output_aliases.at(out.name);
                phase           = (out.negative != outputs.at(pad).negative) != pins.at(pad)->inverted;
            }
            const bool swapped = (!(modes[cell] & 64)) != phase;
            model.power_up = model.reset_value = simulation_bit(swapped);
            model.preset_value                 = simulation_bit(!swapped);
            model.next_state =
                out.function == "T" ? make_binary(ExprKind::Xor, make_identifier(out.name), expression) : expression;
            const auto row_expression = [&](unsigned row) {
                const auto found = shared.find({block, row});
                return found == shared.end() ? make_constant(false) : simulation_product(pla.inputs, found->second);
            };
            model.reset         = row_expression((modes[cell] & 512) ? d.rows - 2 : data_row(local) + 2);
            model.preset        = (modes[cell] & 512) ? row_expression(d.rows - 1) : make_constant(false);
            const auto& control = controls[out.name];
            if (control.count("C"))
                model.clock = simulation_cover(pla.inputs, pla.terms[control.at("C")]);
            else
                model.clock = simulation_clock_input(image.simulation_model, design, d.clock_pins.front());
        }
        std::set<std::string> active(pla.inputs.begin(), pla.inputs.end());
        for (const auto& entry : outputs)
            active.insert(entry.first);
        retain_removed_clocks(active);
        validate_requested_power_up(image.simulation_model, design);
        apply_placement_metadata(image, design, active);
        capture_report(image, true);
        fitted_image = std::move(image);
        return {};
    };
    (void)place_mach_modern(
        d, signals, place_options,
        [&](const auto& placement, const auto& movable) { return evaluate_placement(placement, movable); },
        fitting.report ? ModernPlacementFailureCallback([&](const auto& placement, const auto& movable) {
            try {
                (void)evaluate_placement(placement, movable, true);
            } catch (const LogicError& error) {
                // A best-effort dump cannot replace the proven physical
                // failure when its diagnostic routing hits the deadline.
                if (std::string(error.what()) != "Fitting run-time limit exceeded")
                    throw;
            }
        })
                       : ModernPlacementFailureCallback{});
    if (!fitted_image)
        throw std::logic_error("modern placement completed without a fitted image");
    return std::move(*fitted_image);
}
}  // namespace machxl
