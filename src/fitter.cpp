#include "fitter.hpp"
#include "pla_text.hpp"
#include "device_mach111.hpp"
#include "fitter_budget.hpp"
#include "fitter_failure_report.hpp"
#include "fitter_inputs.hpp"
#include "fitter_legacy_failure_report.hpp"
#include "fitter_live.hpp"
#include "fitter_metadata.hpp"
#include "fitter_order.hpp"
#include "fitter_place.hpp"
#include "fitter_route.hpp"
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
    throw LogicError("MACH111 fixed-pin fitter: " + reason);
}

using ParsedPla = FitterPla;


unsigned output_cell(unsigned pin)
{
    for (unsigned cell = 0; cell < 32; ++cell)
        if (mach111_io_pin(cell) == pin)
            return cell;
    unsupported("output pin " + std::to_string(pin) + " is not a macrocell I/O");
}

unsigned pin_cell(const Pin& pin)
{
    if (pin.node) {
        if (pin.number < 2 || pin.number > 33)
            unsupported("invalid MACH111 node number");
        return pin.number - 2;
    }
    return output_cell(pin.number);
}

struct Mach111RouteRetry
{
    int signal = -1;
};
struct LogicalIds
{
    std::map<std::string, std::size_t> placement_signals;
    std::vector<std::string> placement_order;
    std::vector<std::string> floating_input_order;
    std::map<std::string, std::vector<unsigned>> floating_input_candidates;
    std::map<std::string, unsigned> signal, feedback;
    std::array<int, 32> product_group_owner;
    std::map<std::string, int> constrained_blocks;
    std::map<std::string, int> oe_slots, oe_keys;
    std::map<std::string, unsigned> implicit_controls;
    std::array<std::array<std::string, 4>, 2> oe_products;
    std::array<bool, 2> sequential_search{};
    std::map<std::string, unsigned> reverse_visit_costs;
};

LogicalIds place_pins(Design& design, const ParsedPla& pla, const std::map<std::string, const Equation*>& equations,
                      FitterOptions fitting, const std::map<std::string, std::string>& output_aliases,
                      const std::set<std::string>& paired_pins, const Mach111Placement* forced,
                      std::function<PlacementRouteFeedback(const Mach111Placement&, const std::vector<bool>&)> route)
{
    LogicalIds ids;
    for (const auto& group : design.groups)
        if (group.first.compare(0, 9, "MACH_SEG_") == 0) {
            if (group.first.size() != 10 || group.first.back() < 'A' || group.first.back() > 'B')
                unsupported("invalid MACH111 block constraint " + group.first);
            const int block = group.first.back() - 'A';
            for (const auto& name : group.second) {
                const auto previous = ids.constrained_blocks.emplace(name, block);
                if (!previous.second && previous.first->second != block)
                    unsupported("conflicting block constraints for " + name);
            }
        }
    unsigned next = 77;
    std::vector<Pin*> pins;
    for (auto& pin : design.pins)
        pins.push_back(&pin);
    std::stable_sort(pins.begin(), pins.end(), [](const Pin* a, const Pin* b) {
        if (bool(a->number) != bool(b->number))
            return bool(a->number);
        if (!a->number)
            return false;
        return (a->node ? a->number + 44 : a->number) < (b->node ? b->number + 44 : b->number);
    });
    for (const auto* pin : pins) {
        ids.signal[pin->name] = pin->number ? (pin->node ? pin->number + 43 : pin->number - 1) : next++;
    }
    for (const auto* pin : pins)
        if (!pin->node && !paired_pins.count(pin->name) && equations.count(pin->name) &&
            equations.at(pin->name)->function != "COM" &&
            std::find(pla.inputs.begin(), pla.inputs.end(), pin->name) != pla.inputs.end())
            ids.feedback[pin->name] = next++;
    std::map<std::string, std::vector<unsigned>> outputs;
    std::set<std::string> clocks, clocked_outputs;
    std::map<std::string, std::pair<std::uint16_t, bool>> report_clock_inputs;
    for (unsigned out = 0; out < pla.outputs.size(); ++out) {
        auto name = pla.outputs[out];
        name = strip_pla_phase(name);
        const auto dot      = name.find('.');
        const auto function = dot == std::string::npos ? "COM" : name.substr(dot + 1);
        name.resize(dot == std::string::npos ? name.size() : dot);
        if (function == "C") {
            clocked_outputs.insert(name);
            if (pla.terms[out].size() == 1) {
                const auto& term   = pla.terms[out][0];
                const auto literal = single_literal(term);
                if (literal != std::string::npos)
                    report_clock_inputs[name] = {std::uint16_t(ids.signal.at(pla.inputs[literal])),
                                                 term[literal] == '1'};
            }
            for (const auto& term : pla.terms[out])
                for (unsigned input = 0; input < term.size(); ++input)
                    if (term[input] != '-')
                        clocks.insert(pla.inputs[input]);
        } else
            outputs[name].push_back(out);
    }
    std::vector<Mach111PlaceSignal> signals;
    std::vector<Pin*> included;
    std::map<unsigned, unsigned> input_fanout;
    std::map<std::string, std::uint16_t> control_ids;
    const auto aliases = pla_signal_aliases(design);
    for (auto* pin : pins) {
        if (output_aliases.count(pin->name))
            continue;
        const bool output = equations.count(pin->name);
        if (!output && std::find(pla.inputs.begin(), pla.inputs.end(), pin->name) == pla.inputs.end())
            continue;
        Mach111PlaceSignal signal;
        signal.name            = pin->name;
        signal.output          = output;
        signal.buried          = pin->node;
        signal.sort_name       = aliases.at(pin->name);
        signal.order.serial    = static_cast<std::int16_t>(ids.signal.at(pin->name));
        signal.order.type      = output ? (pin->node ? 3 : 0) : clocks.count(pin->name) ? 9 : 1;
        signal.clocked_storage = output && equations.at(pin->name)->function != "COM";
        if (report_clock_inputs.count(pin->name)) {
            signal.clock_fanin    = report_clock_inputs.at(pin->name).first;
            signal.clock_positive = report_clock_inputs.at(pin->name).second;
        }
        if (ids.constrained_blocks.count(pin->name))
            signal.order.block = static_cast<std::int16_t>(ids.constrained_blocks.at(pin->name));
        if (pin->number) {
            if (pin->node) {
                signal.order.fixed_node = static_cast<std::int16_t>(pin->number + 43);
                signal.fixed_cell       = static_cast<int>(pin->number) - 2;
            } else
                signal.order.fixed_pin = static_cast<std::int16_t>(pin->number - 1);
        }
        if (output) {
            std::set<std::uint16_t> fanins;
            for (auto out : outputs[pin->name]) {
                const auto& label   = pla.outputs[out];
                const auto dot      = label.find('.');
                const auto function = dot == std::string::npos ? "COM" : label.substr(dot + 1);
                const bool control  = function == "RE" || function == "PR" || function == "OE";
                if (!control)
                    signal.order.product_terms =
                        static_cast<std::int16_t>(std::max<std::size_t>(1, pla.terms[out].size()));
                if (function == "RE" || function == "PR") {
                    if (pla.terms[out].size() > 1)
                        unsupported("reset/preset requires more than one product");
                    const auto term = pla.terms[out].empty() ? std::string{} : pla.terms[out][0];
                    const auto id =
                        control_ids.emplace(term, static_cast<std::uint16_t>(control_ids.size())).first->second;
                    (function == "RE" ? signal.reset : signal.preset) = id;
                    (function == "RE" ? signal.reset_product : signal.preset_product) = term;
                }
                if (function == "OE") {
                    if (pla.terms[out].size() > 1)
                        unsupported("output enable requires more than one product");
                    if (!pla.terms[out].empty() && pla.terms[out][0].find_first_not_of('-') != std::string::npos) {
                        signal.oe_members = 1;
                        signal.oe_product = pla.terms[out][0];
                    }
                }
                for (const auto& term : pla.terms[out])
                    for (unsigned input = 0; input < term.size(); ++input)
                        if (term[input] != '-') {
                            const auto& name = pla.inputs[input];
                            const auto id    = ids.feedback.count(name) ? ids.feedback.at(name) : ids.signal.at(name);
                            fanins.insert(static_cast<std::uint16_t>(id));
                        }
            }
            signal.fanins.assign(fanins.begin(), fanins.end());
            for (auto id : fanins)
                ++input_fanout[id];
        }
        signals.push_back(signal);
        included.push_back(pin);
    }
    for (auto& signal : signals)
        if (!signal.output || (signal.buried || equations.at(signal.name)->function == "COM"))
            signal.order.fanout_count = static_cast<std::int16_t>(input_fanout[ids.signal.at(signal.name)]);
    const bool implicit_clock = std::any_of(equations.begin(), equations.end(), [&](const auto& equation) {
        return !output_aliases.count(equation.first) && equation.second->function != "COM" &&
               !clocked_outputs.count(equation.first);
    });
    Mach111PlaceOptions place_options{fitting.balanced_partition, fitting.spread_placement, implicit_clock, false,
                                      fitting.iterate_partition};
    place_options.collect_report_visits = bool(fitting.report);
    if (fitting.report)
        place_options.partition_observer = [&](const auto& observed, const auto& mapping, const auto& frame) {
            if (frame.failed_cluster)
                capture_partition_failure_report(*fitting.report, observed, mapping, frame, ids.signal, ids.feedback);
        };
    if (fitting.report) {
        for (const auto& entry : ids.signal)
            if (output_aliases.count(entry.first)) {
                FitterOrderKey key;
                key.serial = std::int16_t(entry.second);
                key.type   = 3;
                const auto pin =
                    std::find_if(pins.begin(), pins.end(), [&](const auto* p) { return p->name == entry.first; });
                if (pin != pins.end() && (*pin)->node && (*pin)->number)
                    key.fixed_node = std::int16_t(entry.second);
                place_options.report_extra_records.push_back(key);
            }
        for (const auto& entry : ids.feedback) {
            FitterOrderKey key;
            key.serial = std::int16_t(entry.second);
            key.type   = 3;
            place_options.report_extra_records.push_back(key);
        }
    }
    std::function<void(const Mach111Placement&, unsigned, const std::vector<bool>&)> failure;
    if (fitting.report)
        failure = [&](const auto& frame, unsigned block, const auto& assigned) {
            if (!fitting.report->retained_image || !fitting.report->placement_failed_blocks.empty())
                capture_legacy_placement_failure(*fitting.report, signals, frame, block, assigned, ids.signal,
                                                 ids.feedback);
            fitting.report->placement_attempts = fitting_logical_visits();
        };
    const auto placement = forced ? *forced : place_mach111(signals, place_options, std::move(route), failure);
    for (std::size_t i = 0; i < signals.size(); ++i) {
        ids.placement_signals[signals[i].name] = i;
        if (!placement.reverse_visit_costs.empty())
            ids.reverse_visit_costs[signals[i].name] = placement.reverse_visit_costs[i];
    }
    for (auto i : placement.global_order)
        ids.placement_order.push_back(signals[i].name);
    ids.product_group_owner             = placement.product_group_owner;
    ids.oe_products                     = placement.oe_products;
    std::vector<unsigned> eligible_pins = {10, 11, 13, 32, 33, 35};
    for (unsigned cell = 0; cell < 32; ++cell)
        eligible_pins.push_back(mach111_io_pin(cell));
    std::sort(eligible_pins.begin(), eligible_pins.end());
    for (auto i : placement.global_order) {
        const auto& signal = signals[i];
        if (signal.output || signal.order.fixed_pin >= 0 || signal.order.type == 9 || !included[i]->pair.empty())
            continue;
        ids.floating_input_order.push_back(signal.name);
        auto& candidates = ids.floating_input_candidates[signal.name];
        for (auto pin : eligible_pins) {
            if (signal.order.block >= 0 &&
                (mach111_pin_source(pin) >= 64 || output_cell(pin) / 16 != unsigned(signal.order.block)))
                continue;
            candidates.push_back(pin);
        }
    }
    for (unsigned i = 0; i < signals.size(); ++i) {
        ids.oe_slots[signals[i].name] = placement.oe_slots[i];
        ids.oe_keys[signals[i].name]  = placement.oe_keys[i];
        ids.sequential_search         = placement.sequential_search;
        if (i < placement.implicit_controls.size() && placement.implicit_controls[i])
            ids.implicit_controls[signals[i].name] = placement.implicit_controls[i];
        included[i]->number = signals[i].buried ? placed_location(placement.cells, i, "a macrocell") + 2
                                                : placed_location(placement.pins, i, "a pin");
    }
    for (auto& pin : design.pins)
        if (output_aliases.count(pin.name)) {
            const auto target = std::find_if(signals.begin(), signals.end(), [&](const auto& signal) {
                return signal.name == output_aliases.at(pin.name);
            });
            pin.number =
                placed_location(placement.cells, std::size_t(target - signals.begin()), "a macrocell") + 2;
        }
    return ids;
}
}  // namespace

namespace {
FuseImage fit_mach111_prepared(const Design& original_design, const ParsedPla& prepared, FitterOptions fitting,
                               const Mach111Placement* forced = nullptr, const std::vector<bool>* movable = nullptr)
{
    Design design = original_design;
    if (design.device != "MACH111")
        unsupported("device " + design.device + " is not implemented");
    // The original ignores SIGNATURE on non-JTAG MACH111 devices.
    std::set<std::string> low_power;
    for (const auto& group : design.groups) {
        if (group.first == "LOW_POWER_LIST")
            low_power.insert(group.second.begin(), group.second.end());
        // SYNC_LIST/ASYNC_LIST have no effect on MACH111, whose macrocells
        // do not provide selectable synchronous/asynchronous allocation.
        // Ordinary GROUP declarations only name vectors. Their equations
        // have already been expanded by the front end (manual, chapter5).
    }
    if (design.equations.empty())
        unsupported("design has no equations");
    auto pla = prepared;

    GlobalNodeUse global_node;
    expand_global_node(design, pla.inputs, pla.outputs, pla.terms, &global_node);
    if (global_node.reset_overridden)
        fitting.warn("z5037: Invalid global reset equation.");
    discard_combinatorial_storage_controls(pla.outputs, pla.terms);
    report_fitter_notices(original_design, pla.outputs, pla.terms, {}, mach_device_layout("MACH111"), fitting);
    if (fitting.ground_implicit_controls)
        ground_implicit_set_reset(pla.outputs, pla.terms);
    validate_block_set_reset(design, mach_device_layout("MACH111"), pla.outputs, pla.terms);
    validate_node_output_enables(design, pla.outputs);
    std::vector<std::string> unreferenced_nodes;
    validate_node_equations(design, pla.inputs, pla.outputs, &unreferenced_nodes);
    for (const auto& warning : discarded_input_pairs(design))
        fitting.warn(warning);
    for (const auto& name : unreferenced_nodes)
        fitting.warn("z5071: Nodes not referenced - discarded: " + name);
    for (const auto& warning : preplaced_fanin_warnings(
             pla.inputs, pla.outputs, pla.terms, mach_device_layout("MACH111").muxes,
             [&](const std::string& name) { return fixed_block_of(design, mach_device_layout("MACH111"), name); }))
        fitting.warn(warning);
    validate_equation_fanin(pla.inputs, pla.outputs, pla.terms, mach_device_layout("MACH111").muxes);
    validate_storage_control_products(pla.outputs, pla.terms);
    check_fitting_budget();
    if (design.dos_compatibility && pla.inputs.empty() && pla.outputs.size() == 1)
        unsupported("original single constant PLA omits the input label directive");
    if (design.dos_compatibility && pla.inputs.empty() &&
        std::all_of(pla.terms.begin(), pla.terms.end(), [](const auto& terms) { return terms.empty(); }))
        unsupported("z5015: original constant PLA has zero product terms");
    if (fitting.use_saved_placement || !fitting.saved_placement.empty())
        apply_saved_placement(design, fitting.saved_placement);
    apply_preplacement_policy(design, fitting.preplacement_policy);
    validate_fixed_locations(design);
    if (fitting.report)
        fitting.report->optimized_design = std::make_shared<Design>(design);
    const auto placement_design = design;
    // Clock polarity DRC precedes unused-node removal in MACHFITR. A
    // negative single-literal clock remains invalid on an unused node.
    // Clock shape and dedicated-pin checks apply only to retained nodes.
    for (unsigned out = 0; out < pla.outputs.size(); ++out) {
        const auto& label = pla.outputs[out];
        if (label.size() < 2 || label.substr(label.size() - 2) != ".C")
            continue;
        if (pla.terms[out].size() != 1)
            continue;
        const auto input = single_literal(pla.terms[out][0]);
        if (input == std::string::npos)
            continue;
        const auto pin = std::find_if(design.pins.begin(), design.pins.end(),
                                      [&](const auto& item) { return item.name == pla.inputs[input]; });
        if (pin != design.pins.end() && (pla.terms[out][0][input] == '0') != pin->inverted)
            unsupported("z5093: invalid negative register clock");
    }
    std::vector<std::string> discarded_nodes;
    const auto reachable = retain_reachable_equations(design, pla.inputs, pla.outputs, pla.terms, &discarded_nodes);
    for (const auto& name : discarded_nodes)
        fitting.warn("z5071: Nodes not referenced - discarded: " + name);
    for (const auto& name : unreferenced_pins(design, pla.inputs, pla.outputs))
        fitting.warn("z5073: Input Pins not referenced - discarded: " + name);
    std::map<std::string, const Pin*> pins;
    for (const auto& pin : design.pins) {
        if (pin.latched)
            unsupported("MACH111 has no hardware latches");
        pins.emplace(pin.name, &pin);
    }
    std::map<std::string, const Equation*> equations;
    for (const auto& equation : design.equations) {
        if (!reachable.count(equation.signal))
            continue;
        if (equation.function == "C" || equation.function == "PR" || equation.function == "RE" ||
            equation.function == "OE")
            continue;
        if (equation.function != "COM" && equation.function != "REG" && equation.function != "T")
            unsupported("control equation " + equation.signal + "." + equation.function);
        if (!equations.emplace(equation.signal, &equation).second)
            unsupported("multiple combinational equations for " + equation.signal);
    }
    std::map<std::string, std::string> output_aliases;
    std::map<std::string, bool> alias_phases;
    std::set<std::string> paired_pins;
    std::map<std::string, unsigned> primary_columns;
    for (unsigned out = 0; out < pla.outputs.size(); ++out) {
        auto name = pla.outputs[out];
        name = strip_pla_phase(name);
        const auto dot      = name.find('.');
        const auto function = dot == std::string::npos ? "COM" : name.substr(dot + 1);
        if (function == "COM" || function == "REG" || function == "T")
            primary_columns[name.substr(0, dot)] = out;
    }
    for (const auto& pin : design.pins)
        if (!pin.pair.empty() && reachable.count(pin.name)) {
            // MACH1 input pairing has no dedicated hardware and leaves the
            // NODE equation as an ordinary independent logic macrocell.
            if (!pin.node)
                continue;
            const auto target = pins.find(pin.pair);
            if (target == pins.end() || target->second->node)
                unsupported("invalid output pair");
            if (!primary_columns.count(pin.name) || !primary_columns.count(pin.pair))
                unsupported("output pair has no equation");
            const auto node = primary_columns.at(pin.name), pad = primary_columns.at(pin.pair);
            if (equations.at(pin.name)->function != equations.at(pin.pair)->function ||
                pla.terms[node] != pla.terms[pad])
                unsupported("output pair has different excitation equations");
            if (pin.number && target->second->number && pin_cell(pin) != pin_cell(*target->second))
                unsupported("conflicting output pair constraints");
            output_aliases[pin.name] = pin.pair;
            // The node of an output pair is only another name for the pad's
            // macrocell. If nothing reads it, the original discards it.
            if (std::find(pla.inputs.begin(), pla.inputs.end(), pin.name) == pla.inputs.end())
                fitting.warn("z5071: Nodes not referenced - discarded: " + pin.name);
            paired_pins.insert(pin.pair);
            alias_phases[pin.name] = pla.outputs[node].back() == '-';
        }
    for (unsigned out = 0; out < pla.outputs.size();) {
        auto name = pla.outputs[out];
        name = strip_pla_phase(name);
        const auto dot   = name.find('.');
        const auto alias = output_aliases.find(name.substr(0, dot));
        if (alias != output_aliases.end()) {
            const auto function = dot == std::string::npos ? "COM" : name.substr(dot + 1);
            if (function == "C" || function == "RE" || function == "PR" || function == "OE") {
                const auto target   = alias->second + "." + function;
                const auto existing = std::find(pla.outputs.begin(), pla.outputs.end(), target);
                if (existing == pla.outputs.end()) {
                    pla.outputs[out] = target;
                    ++out;
                    continue;
                }
                if (pla.terms[unsigned(existing - pla.outputs.begin())] != pla.terms[out])
                    unsupported("output pair has different " + function + " controls");
            }
            pla.outputs.erase(pla.outputs.begin() + out);
            pla.terms.erase(pla.terms.begin() + out);
        } else
            ++out;
    }
    std::optional<FuseImage> completed;
    const auto ids =
        place_pins(design, pla, equations, fitting, output_aliases, paired_pins, forced,
                   [&](const Mach111Placement& candidate, const std::vector<bool>& can_move) {
                       try {
                           completed = fit_mach111_prepared(original_design, prepared, fitting, &candidate, &can_move);
                           return PlacementRouteFeedback{};
                       } catch (const Mach111RouteRetry& retry) {
                           return PlacementRouteFeedback{false, retry.signal};
                       }
                   });
    if (!forced) {
        if (!completed)
            throw std::logic_error("MACH111 placement returned without routed result");
        return std::move(*completed);
    }
    for (const auto& control : ids.implicit_controls)
        for (unsigned bit = 0; bit < 2; ++bit)
            if (control.second & (1u << bit)) {
                pla.outputs.push_back(control.first + (bit ? ".PR" : ".RE"));
                pla.terms.emplace_back();
            }
    std::map<std::string, unsigned> signal_clocks;
    std::set<unsigned> clock_inputs;
    struct SharedControl
    {
        unsigned block, row;
        std::string term;
    };
    std::vector<SharedControl> shared_controls;
    // Registers with a reset or set equation of their own. Others share the
    // block's term physically, but the reports do not list it for them.
    std::set<std::string> reset_named, preset_named;
    std::map<std::string, std::vector<std::string>> enables;
    for (unsigned out = 0; out < pla.outputs.size();) {
        const auto& label = pla.outputs[out];
        if (label.size() > 3 && label.substr(label.size() - 3) == ".OE") {
            if (pla.terms[out].size() > 1)
                unsupported("output enable requires one product term");
            enables.emplace(label.substr(0, label.size() - 3), pla.terms[out]);
            pla.outputs.erase(pla.outputs.begin() + out);
            pla.terms.erase(pla.terms.begin() + out);
            continue;
        }
        if (label.size() > 3 && (label.substr(label.size() - 3) == ".PR" || label.substr(label.size() - 3) == ".RE")) {
            const auto signal = label.substr(0, label.size() - 3);
            if (!equations.count(signal) || equations.at(signal)->function == "COM")
                unsupported("reset/preset on nonregistered signal " + signal);
            if (pla.terms[out].size() > 1)
                unsupported("reset/preset must contain at most one product term");
            const auto term = pla.terms[out].empty() ? std::string{} : pla.terms[out][0];
            const auto pin  = pins.find(signal);
            if (pin == pins.end())
                unsupported("unmapped control output " + signal);
            (label.substr(label.size() - 3) == ".RE" ? reset_named : preset_named).insert(signal);
            const unsigned block = pin_cell(*pin->second) / 16;
            const unsigned row   = label.substr(label.size() - 3) == ".RE" ? 2 : 67;
            const auto existing  = std::find_if(
                shared_controls.begin(), shared_controls.end(),
                [&](const SharedControl& control) { return control.block == block && control.row == row; });
            if (existing == shared_controls.end())
                shared_controls.push_back({block, row, term});
            else if (existing->term != term)
                unsupported("multiple distinct reset/preset terms in one block");
            pla.outputs.erase(pla.outputs.begin() + out);
            pla.terms.erase(pla.terms.begin() + out);
            continue;
        }
        if (label.size() < 2 || label.substr(label.size() - 2) != ".C") {
            ++out;
            continue;
        }
        const auto signal = label.substr(0, label.size() - 2);
        if (!equations.count(signal) || equations.at(signal)->function == "COM")
            unsupported("clock on nonregistered signal " + signal);
        if (pla.terms[out].size() != 1)
            unsupported("clock must be one dedicated input");
        const auto& term = pla.terms[out][0];
        const auto input = single_literal(term);
        if (input == std::string::npos)
            unsupported("clock must be one dedicated input");
        const auto pin = pins.find(pla.inputs[input]);
        if (pin == pins.end() || ((term[input] == '1') == pin->second->inverted))
            unsupported("inverted clocks are not implemented");
        const std::array<unsigned, 4> clock_pins = {11, 13, 33, 35};
        const auto clock                         = std::find(clock_pins.begin(), clock_pins.end(), pin->second->number);
        if (clock == clock_pins.end())
            unsupported("clock must use pin 11, 13, 33 or 35");
        signal_clocks.emplace(signal, static_cast<unsigned>(clock - clock_pins.begin()));
        clock_inputs.insert(static_cast<unsigned>(input));
        pla.outputs.erase(pla.outputs.begin() + out);
        pla.terms.erase(pla.terms.begin() + out);
    }
    std::array<bool, 45> allocated_pins{};
    for (const auto& equation : equations)
        if (equation.second->function != "COM" && !signal_clocks.count(equation.first))
            allocated_pins[35] = true;
    for (const auto& pin : design.pins) {
        if (!equations.count(pin.name) && std::find(pla.inputs.begin(), pla.inputs.end(), pin.name) == pla.inputs.end())
            continue;  // Original DRC omits unused declarations from placement.
        if (!pin.node)
            allocated_pins.at(pin.number) = true;
    }

    Mach111Physical physical;
    physical.low_power.fill(true);
    std::array<bool, 32> used_cells{};
    std::vector<unsigned> output_cells;
    std::map<std::string, unsigned> output_indices;
    std::vector<bool> inverted_outputs;
    std::array<int, 32> assigned_clocks;
    assigned_clocks.fill(-1);
    std::array<std::uint16_t, 32> mode_words;
    mode_words.fill(0x2c);
    for (unsigned out = 0; out < pla.outputs.size(); ++out) {
        std::string signal  = pla.outputs[out];
        const bool inverted = !signal.empty() && signal.back() == '-';
        if (inverted)
            signal.pop_back();
        const auto dot      = signal.find('.');
        const auto function = dot == std::string::npos ? "COM" : signal.substr(dot + 1);
        if (dot != std::string::npos)
            signal.resize(dot);
        if (function != "COM" && function != "REG" && function != "T")
            unsupported("synthesized function " + function);
        const auto pin = pins.find(signal);
        if (pin == pins.end() || !equations.count(signal))
            unsupported("unmapped synthesized output " + signal);
        const auto cell = pin_cell(*pin->second);
        if (used_cells[cell])
            unsupported("multiple signals assigned to one output cell");
        if (pla.terms[out].size() > 12)
            unsupported("product-term demand exceeds three groups (" + signal + ")");
        used_cells[cell] = true;
        output_cells.push_back(cell);
        output_indices.emplace(signal, out);
        inverted_outputs.push_back(inverted != pin->second->inverted);
        mode_words[cell] = function == "COM" ? 0xad : function == "REG" ? 0xa1 : 0xa5;
        if (pin->second->node)
            mode_words[cell] &= ~0xe0u;
        if (function != "COM") {
            const auto clock      = signal_clocks.find(signal);
            assigned_clocks[cell] = clock == signal_clocks.end() ? 3 : static_cast<int>(clock->second);
        }
    }

    for (const auto& alias : output_aliases)
        output_indices[alias.first] = output_indices.at(alias.second);

    // Assign fixed output enables in global output order before the later
    // physical output sort. MACH111 candidates [0,1] and [2,3] map to AND
    // rows [1,0,68,69] and mux codes [1,3,1,3].
    std::vector<FitterOrderKey> output_keys(pla.outputs.size());
    std::vector<std::string> output_names(pla.outputs.size());
    for (const auto& entry : output_indices) {
        if (output_aliases.count(entry.first))
            continue;
        const auto& signal = entry.first;
        const auto out     = entry.second;
        output_names[out]  = signal;
        auto& key          = output_keys[out];
        key.serial         = static_cast<std::int16_t>(ids.signal.at(signal));
        key.fixed_pin      = static_cast<std::int16_t>(pins.at(signal)->number - 1);
        key.block          = static_cast<std::int16_t>(output_cells[out] / 16);
        key.product_terms  = static_cast<std::int16_t>(pla.terms[out].size());
        if (equations.at(signal)->function == "COM" && !paired_pins.count(signal)) {
            const auto input = std::find(pla.inputs.begin(), pla.inputs.end(), signal);
            if (input != pla.inputs.end()) {
                key.type          = 5;
                const auto column = static_cast<unsigned>(input - pla.inputs.begin());
                std::array<bool, 2> fanout{};
                for (unsigned target = 0; target < pla.outputs.size(); ++target)
                    for (const auto& term : pla.terms[target])
                        if (term[column] != '-')
                            fanout[output_cells[target] / 16] = true;
                for (const auto& control : shared_controls)
                    if (!control.term.empty() && control.term[column] != '-')
                        fanout[control.block] = true;
                for (const auto& enable : enables)
                    for (const auto& term : enable.second)
                        if (term[column] != '-')
                            fanout[output_cells.at(output_indices.at(enable.first)) / 16] = true;
                key.fanout_count = fanout[0] + fanout[1];
            }
        }
    }
    auto oe_products = ids.oe_products;
    std::array<std::array<bool, 4>, 2> oe_used{};
    constexpr unsigned oe_rows[4] = {1, 0, 68, 69};
    // The original transfers the complete partition OE table, retaining
    // unused entries when final placement overwrites only occupied slots.
    for (unsigned block = 0; block < 2; ++block)
        for (unsigned slot = 0; slot < 4; ++slot)
            if (!oe_products[block][slot].empty()) {
                oe_used[block][slot] = true;
                shared_controls.push_back({block, oe_rows[slot], oe_products[block][slot]});
            }
    for (const auto out : fitter_signal_order(output_keys, false)) {
        const auto enable = enables.find(output_names[out]);
        if (enable == enables.end())
            continue;
        const auto cell = output_cells[out], block = cell / 16;
        unsigned code = 2;  // Implicit or explicitly constant-one enable.
        if (enable->second.empty())
            code = 0;
        else if (enable->second[0].find_first_not_of('-') != std::string::npos) {
            const int assigned = ids.oe_slots.at(output_names[out]);
            if (assigned < 0 || assigned > 3)
                unsupported("missing output-enable placement");
            const auto slot = static_cast<unsigned>(assigned);
            if (oe_used[block][slot] && oe_products[block][slot] != enable->second[0])
                unsupported("conflicting output-enable placement");
            if (!oe_used[block][slot]) {
                oe_used[block][slot]     = true;
                oe_products[block][slot] = enable->second[0];
                shared_controls.push_back({block, oe_rows[slot], enable->second[0]});
            }
            code = (slot & 1) ? 3 : 1;
        }
        mode_words[cell] = static_cast<std::uint16_t>((mode_words[cell] & ~0xc0u) | (code << 6));
    }

    // Placement trials can reassign earlier product groups. Preserve the
    // allocator's final owners, including groups displaced off their own
    // output cell, and serialize terms in physical group order.
    std::array<bool, 32> used_groups{};
    std::vector<std::vector<unsigned>> product_groups(pla.outputs.size());
    for (auto& word : mode_words)
        word &= ~3u;
    for (unsigned group = 0; group < 32; ++group) {
        const auto owner = ids.product_group_owner[group];
        if (owner < 0)
            continue;
        const auto output = std::find(output_cells.begin(), output_cells.end(), static_cast<unsigned>(owner));
        if (output == output_cells.end())
            unsupported("product group has no output owner");
        const auto out     = static_cast<unsigned>(output - output_cells.begin());
        used_groups[group] = true;
        product_groups[out].push_back(group);
        mode_words[group] |= group == static_cast<unsigned>(owner)  ? 1u
                             : group < static_cast<unsigned>(owner) ? 2u
                                                                    : 3u;
        if (!used_cells[group])
            mode_words[group] &= ~0x20u;
    }
    for (unsigned out = 0; out < pla.outputs.size(); ++out)
        if (product_groups[out].size() * 4 < std::max<std::size_t>(1, pla.terms[out].size()))
            unsupported("insufficient allocated product groups for " + output_names[out]);

    std::vector<FitterOrderKey> keys(pla.inputs.size());
    std::vector<std::array<bool, 2>> fanout(pla.inputs.size());
    std::vector<bool> input_inverted(pla.inputs.size());
    for (unsigned input = 0; input < pla.inputs.size(); ++input) {
        for (unsigned out = 0; out < pla.outputs.size(); ++out)
            for (const auto& term : pla.terms[out])
                if (term[input] != '-')
                    fanout[input][output_cells[out] / 16] = true;
        for (const auto& control : shared_controls)
            if (!control.term.empty() && control.term[input] != '-')
                fanout[input][control.block] = true;
        const auto pin = pins.find(pla.inputs[input]);
        if (pin == pins.end())
            unsupported("unmapped synthesized input " + pla.inputs[input]);
        const auto feedback            = output_indices.find(pla.inputs[input]);
        const bool registered_feedback = feedback != output_indices.end() && !paired_pins.count(pla.inputs[input]) &&
                                         (pin->second->node || equations.at(pla.inputs[input])->function != "COM");
        // Registered feedback precedes the output polarity mux. Its phase
        // follows the synthesized register equation, not the declared pin.
        input_inverted[input] = pin->second->inverted;
        if (registered_feedback)
            input_inverted[input] = output_aliases.count(pla.inputs[input])
                                        ? alias_phases.at(pla.inputs[input])
                                        : pin->second->inverted != inverted_outputs[feedback->second];
        keys[input].type   = paired_pins.count(pla.inputs[input]) ? 1
                             : feedback != output_indices.end()   ? (registered_feedback ? 3 : 5)
                             : clock_inputs.count(input)          ? 9
                                                                  : 1;
        keys[input].serial = static_cast<std::int16_t>(registered_feedback && ids.feedback.count(pla.inputs[input])
                                                           ? ids.feedback.at(pla.inputs[input])
                                                           : ids.signal.at(pla.inputs[input]));
        keys[input].block =
            feedback != output_indices.end() ? static_cast<std::int16_t>(output_cells[feedback->second] / 16) : -1;
        if (feedback == output_indices.end()) {
            const auto original_pin =
                std::find_if(placement_design.pins.begin(), placement_design.pins.end(),
                             [&](const Pin& candidate) { return candidate.name == pla.inputs[input]; });
            if (original_pin != placement_design.pins.end() && original_pin->number &&
                mach111_pin_source(original_pin->number) < 64)
                keys[input].block = static_cast<std::int16_t>(output_cell(original_pin->number) / 16);
            if (ids.constrained_blocks.count(pla.inputs[input]))
                keys[input].block = static_cast<std::int16_t>(ids.constrained_blocks.at(pla.inputs[input]));
        }
        if (feedback != output_indices.end())
            keys[input].product_terms = static_cast<std::int16_t>(pla.terms[feedback->second].size());
        keys[input].fixed_pin = static_cast<std::int16_t>(pin->second->number - 1);
        const auto constrained_node =
            std::find_if(placement_design.pins.begin(), placement_design.pins.end(), [&](const Pin& candidate) {
                return candidate.name == pla.inputs[input] && candidate.node && candidate.number;
            });
        if (constrained_node != placement_design.pins.end())
            keys[input].fixed_node = static_cast<std::int16_t>(constrained_node->number + 43);
        keys[input].fanout_count = fanout[input][0] + fanout[input][1];
    }
    auto sorted = fitter_signal_order(keys, false);
    // Prepend signals to each block's fanin list.
    std::reverse(sorted.begin(), sorted.end());
    std::array<std::map<unsigned, unsigned>, 2> input_muxes;
    std::array<unsigned, 70> unused_source_loads{};
    InputPinProblem input_problem;
    input_problem.pins.resize(pla.inputs.size());
    input_problem.candidates.resize(pla.inputs.size());
    input_problem.feedback.resize(pla.inputs.size());
    input_problem.occupied_pins.assign(allocated_pins.begin(), allocated_pins.end());
    input_problem.pin_sources.assign(45, -1);
    for (unsigned cell = 0; cell < 32; ++cell) {
        const auto pin                 = mach111_io_pin(cell);
        input_problem.pin_sources[pin] = int(mach111_pin_source(pin));
    }
    for (auto pin : {10u, 11u, 13u, 32u, 33u, 35u})
        input_problem.pin_sources[pin] = int(mach111_pin_source(pin));
    for (unsigned i = 0; i < pla.inputs.size(); ++i) {
        const auto& name          = pla.inputs[i];
        input_problem.pins[i]     = pins.at(name)->node ? 0 : pins.at(name)->number;
        input_problem.feedback[i] = keys[i].type == 3 || keys[i].type == 5;
        if (ids.floating_input_candidates.count(name)) {
            input_problem.candidates[i]                        = ids.floating_input_candidates.at(name);
            input_problem.occupied_pins[input_problem.pins[i]] = false;
        }
    }
    for (const auto& name : ids.floating_input_order) {
        const auto found = std::find(pla.inputs.begin(), pla.inputs.end(), name);
        if (found != pla.inputs.end())
            input_problem.placement_order.push_back(unsigned(found - pla.inputs.begin()));
    }
    RouteProblem route_base;
    route_base.mux_count = 26;
    route_base.sources.resize(70);
    for (unsigned source = 0; source < 70; ++source) {
        for (const auto& choice : mach111_mux_choices(source))
            route_base.sources[source].muxes.push_back(choice.mux);
        route_base.sources[source].primary_count = unsigned(route_base.sources[source].muxes.size());
    }
    for (unsigned block = 0; block < 2; ++block) {
        std::vector<unsigned> sources, input_indices;
        for (const auto input : sorted)
            if (fanout[input][block]) {
                const auto feedback = output_indices.find(pla.inputs[input]);
                sources.push_back(
                    feedback == output_indices.end() || paired_pins.count(pla.inputs[input]) ||
                            (!pins.at(pla.inputs[input])->node && equations.at(pla.inputs[input])->function == "COM")
                        ? mach111_pin_source(pins.at(pla.inputs[input])->number)
                        : mach111_feedback_source(output_cells[feedback->second]));
                input_indices.push_back(static_cast<unsigned>(input));
            }
        auto routing           = route_base;
        routing.signal_sources = std::move(sources);
        routing.allow_alternates.resize(input_indices.size());
        input_problem.blocks.push_back(std::move(routing));
        input_problem.block_signals.push_back(std::move(input_indices));
    }
    if (movable) {
        input_problem.external_movable.assign(pla.inputs.size(), false);
        for (unsigned input = 0; input < pla.inputs.size(); ++input) {
            const auto& name = pla.inputs[input];
            const auto owner = output_aliases.count(name) ? output_aliases.at(name) : name;
            if (ids.placement_signals.count(owner))
                input_problem.external_movable[input] = movable->at(ids.placement_signals.at(owner));
        }
        for (const auto& name : ids.placement_order)
            for (unsigned input = 0; input < pla.inputs.size(); ++input)
                if (pla.inputs[input] == name ||
                    (output_aliases.count(pla.inputs[input]) && output_aliases.at(pla.inputs[input]) == name))
                    input_problem.retry_order.push_back(input);
    }
    if (!ids.reverse_visit_costs.empty()) {
        input_problem.reverse_visit_costs.resize(pla.inputs.size());
        for (unsigned i = 0; i < pla.inputs.size(); ++i)
            if (ids.reverse_visit_costs.count(pla.inputs[i]))
                input_problem.reverse_visit_costs[i] = ids.reverse_visit_costs.at(pla.inputs[i]);
    }
    const auto input_route    = route_input_pins(input_problem);
    const auto capture_report = [&](FuseImage& image, bool successful) {
        if (!fitting.report)
            return;
        FitterReportData report;
        report.optimized_pla    = fitting.report->optimized_pla;
        report.optimized_design = fitting.report->optimized_design;
        report.successful       = successful;
        report.blocks.resize(2);
        report.routing_attempts   = input_route.failed_route_sweeps;
        report.placement_attempts = fitting_logical_visits();
        for (unsigned b = 0; b < 2; ++b)
            report.blocks[b].sequential_search = ids.sequential_search[b];
        report.group_owner.assign(ids.product_group_owner.begin(), ids.product_group_owner.end());
        for (unsigned out = 0; out < output_names.size(); ++out) {
            const auto& name = output_names[out];
            const auto cell  = output_cells[out];
            FitterReportNode node;
            node.name    = name;
            node.block   = int(cell / 16);
            node.cell    = int(cell % 16);
            node.number  = int(cell) + 2;
            node.pin     = pins.at(name)->node ? -1 : int(pins.at(name)->number);
            node.implied = !pins.at(name)->node;
            node.paired  = node.pin >= 0;
            for (const auto& alias : output_aliases)
                if (alias.second == name) {
                    node.name    = alias.first;
                    node.implied = false;
                    break;
                }
            const auto& function = equations.at(name)->function;
            node.logic           = function == "COM" ? 'C' : function == "T" ? 'T' : 'D';
            node.negative        = inverted_outputs[out];
            node.low_power       = physical.low_power[cell];
            // An equation without products still occupies one grounded term.
            node.product_terms = std::max<unsigned>(1, unsigned(pla.terms[out].size()));
            report.blocks[cell / 16].product_terms += node.product_terms;
            auto append_fanins = [&](const std::string& term) {
                for (unsigned i = 0; i < term.size(); ++i)
                    if (term[i] != '-' &&
                        std::find(node.fanins.begin(), node.fanins.end(), pla.inputs[i]) == node.fanins.end())
                        node.fanins.push_back(pla.inputs[i]);
            };
            for (const auto& term : pla.terms[out])
                append_fanins(term);
            node.data_fanins = node.fanins;
            // A constant enable is not an output enable resource.
            const auto constant_enable = [&] {
                const auto& terms = enables.at(name);
                return std::all_of(terms.begin(), terms.end(), [](const std::string& term) {
                    return term.find_first_not_of('-') == std::string::npos;
                });
            };
            if (enables.count(name) && !constant_enable()) {
                for (const auto& term : enables.at(name)) {
                    node.oe_expression += term + ";";
                    append_fanins(term);
                }
                if (ids.oe_slots.count(name))
                    node.oe_slot = ids.oe_slots.at(name);
                if (node.oe_slot >= 0)
                    node.oe_key = ids.oe_keys.at(name);
            }
            if (function != "COM") {
                node.clock = "Ck" + std::to_string(assigned_clocks[cell] < 0 ? 3 : assigned_clocks[cell]);
                for (const auto& control : shared_controls)
                    if (control.block == cell / 16) {
                        if ((control.row == 2 && !reset_named.count(name)) ||
                            (control.row == 67 && !preset_named.count(name)))
                            continue;
                        if (control.row == 2)
                            node.reset = control.term;
                        if (control.row == 67)
                            node.preset = control.term;
                        append_fanins(control.term);
                    }
                node.global_reset  = global_node.reset.count(name) != 0;
                node.global_preset = global_node.preset.count(name) != 0;
            }
            if (!successful)
                node.clock.clear();
            report.nodes.push_back(std::move(node));
        }
        for (unsigned b = 0; b < 2; ++b) {
            const auto& routed  = input_route.blocks[b];
            const auto& indices = input_problem.block_signals[b];
            for (unsigned i = 0; i < indices.size(); ++i) {
                const auto input = indices[i];
                FitterReportRoute route;
                route.signal     = pla.inputs[input];
                route.logical_id = unsigned(keys[input].serial);
                route.mux        = routed.signal_to_mux[i];
                route.source     = int(input_route.signal_sources[b][i]);
                if (keys[input].type == 3) {
                    const auto cell    = output_cells[output_indices.at(pla.inputs[input])];
                    route.source_block = int(cell / 16);
                    route.source_cell  = int(cell % 16);
                } else
                    route.source_pin = int(pins.at(pla.inputs[input])->number);
                report.blocks[b].routes.push_back(std::move(route));
            }
            std::set<unsigned> seen_clocks;
            for (unsigned cell = 16 * b; cell < 16 * b + 16; ++cell)
                if (used_cells[cell] && assigned_clocks[cell] >= 0)
                    seen_clocks.insert(unsigned(assigned_clocks[cell]));
            for (auto clock : seen_clocks) {
                const unsigned pin = std::array<unsigned, 4>{11, 13, 33, 35}[clock];
                report.blocks[b].clocks.push_back({image.pin_signals[pin], int(clock), int(pin), false});
            }
        }
        finalize_fitter_report_data(design, image, report);
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
        *fitting.report     = report;
        image.fitter_report = std::move(report);
    };
    if (fitting.report && !input_route.success) {
        for (unsigned input = 0; input < pla.inputs.size(); ++input)
            if (!input_problem.candidates[input].empty())
                for (auto& pin : design.pins)
                    if (pin.name == pla.inputs[input])
                        pin.number = input_route.pins[input];
        FuseImage trial;
        trial.device    = "MACH111";
        trial.design    = design.chip;
        trial.pin_count = 44;
        std::set<std::string> active(pla.inputs.begin(), pla.inputs.end());
        active.insert(output_names.begin(), output_names.end());
        // The snapshot is diagnostic only. A failed route can leave two
        // inputs on one pin, which the metadata step rejects; that must not
        // replace the retry below and end a fit that would have continued.
        try {
            apply_placement_metadata(trial, design, active);
            capture_report(trial, false);
        } catch (const LogicError&) {
        }
    }
    if (!input_route.success) {
        int target = -1;
        if (input_route.outer_signal >= 0) {
            const auto& name = pla.inputs[unsigned(input_route.outer_signal)];
            target = int(ids.placement_signals.at(output_aliases.count(name) ? output_aliases.at(name) : name));
        }
        throw Mach111RouteRetry{target};
    }
    std::copy(input_problem.occupied_pins.begin(), input_problem.occupied_pins.end(), allocated_pins.begin());
    for (unsigned i = 0; i < pla.inputs.size(); ++i)
        if (!input_problem.candidates[i].empty()) {
            const auto pin      = input_route.pins[i];
            allocated_pins[pin] = true;
            for (auto& declared_pin : design.pins)
                if (declared_pin.name == pla.inputs[i])
                    declared_pin.number = pin;
        }
    for (unsigned block = 0; block < 2; ++block) {
        auto sources              = input_problem.blocks[block].signal_sources;
        const auto& input_indices = input_problem.block_signals[block];
        for (unsigned i = 0; i < input_indices.size(); ++i)
            if (!input_problem.candidates[input_indices[i]].empty())
                sources[i] = unsigned(input_problem.pin_sources[input_route.pins[input_indices[i]]]);
        const auto& routes = input_route.blocks[block];
        for (unsigned i = 0; i < sources.size(); ++i) {
            const auto mux                       = static_cast<unsigned>(routes.signal_to_mux[i]);
            input_muxes[block][input_indices[i]] = mux;
            for (unsigned slot = 0; slot < 8; ++slot)
                if (mach111_mux_source(mux, slot) == sources[i])
                    physical.blocks[block].mux_slots[mux] = std::uint8_t(slot);
        }
        for (unsigned mux = 0; mux < 26; ++mux)
            if (routes.mux_to_signal[mux] == -1)
                physical.blocks[block].mux_slots[mux] = static_cast<std::uint8_t>(
                    mach111_unused_mux_slot(mux, used_cells, allocated_pins, unused_source_loads));
        std::array<int, 16> clocks;
        for (unsigned cell = 0; cell < 16; ++cell)
            clocks[cell] = assigned_clocks[16 * block + cell];
        const auto balanced = mach111_balance_clocks(clocks);
        for (unsigned cell = 0; cell < 16; ++cell) {
            const auto global_cell = 16 * block + cell;
            const auto clock       = balanced[cell];
            auto word              = mode_words[global_cell];
            word |= static_cast<std::uint16_t>(((clock & 1) << 4) | ((clock >> 1) << 8));
            physical.blocks[block].macrocell_words[cell] = word;
            physical.low_power[global_cell]              = !used_cells[global_cell];
            if (!used_groups[global_cell])
                for (unsigned row = 3 + 4 * cell; row < 7 + 4 * cell; ++row)
                    physical.blocks[block].products[row].fill(Mach111Physical::Literal::DontCare);
        }
    }
    for (unsigned out = 0; out < pla.outputs.size(); ++out) {
        const auto cell = output_cells[out], block = cell / 16, local = cell % 16;
        if (low_power.count(output_names[out]))
            physical.low_power[cell] = true;
        if (inverted_outputs[out])
            physical.blocks[block].macrocell_words[local] &= ~(1u << 5);
        for (unsigned term = 0; term < pla.terms[out].size(); ++term) {
            const auto group = product_groups[out][term / 4] % 16;
            auto& product    = physical.blocks[block].products[3 + 4 * group + term % 4];
            product.fill(Mach111Physical::Literal::DontCare);
            for (unsigned input = 0; input < pla.inputs.size(); ++input) {
                const char literal = pla.terms[out][term][input];
                if (literal == '-')
                    continue;
                const bool positive = (literal == '1') != input_inverted[input];
                product[input_muxes[block].at(input)] =
                    positive ? Mach111Physical::Literal::Positive : Mach111Physical::Literal::Negative;
            }
        }
    }
    for (const auto& control : shared_controls) {
        auto& product = physical.blocks[control.block].products[control.row];
        if (control.term.empty())
            continue;  // Explicit constant-false control.
        product.fill(Mach111Physical::Literal::DontCare);
        for (unsigned input = 0; input < pla.inputs.size(); ++input) {
            const auto literal = control.term[input];
            if (literal == '-')
                continue;
            const bool positive = (literal == '1') != input_inverted[input];
            product[input_muxes[control.block].at(input)] =
                positive ? Mach111Physical::Literal::Positive : Mach111Physical::Literal::Negative;
        }
    }
    check_fitting_budget();
    auto image = encode_mach111(physical, design.chip);
    for (unsigned out = 0; out < pla.outputs.size(); ++out) {
        const auto& name    = output_names[out];
        const auto function = equations.at(name)->function;
        const bool negative = inverted_outputs[out] != pins.at(name)->inverted;
        auto expression     = simulation_cover(pla.inputs, pla.terms[out]);
        if (function == "COM") {
            image.simulation_model.combinatorial[name] = negative ? make_not(expression) : expression;
            continue;
        }
        auto& model    = image.simulation_model.registers[name];
        model.power_up = model.reset_value = simulation_bit(negative);
        model.preset_value                 = simulation_bit(!negative);
        model.next_state = function == "T" ? make_binary(ExprKind::Xor, make_identifier(name), expression)
                           : negative      ? make_not(expression)
                                           : expression;
        model.reset = model.preset = make_constant(false);
        for (const auto& control : shared_controls)
            if (control.block == output_cells[out] / 16) {
                if (control.row == 2)
                    model.reset = simulation_product(pla.inputs, control.term);
                else if (control.row == 67)
                    model.preset = simulation_product(pla.inputs, control.term);
            }
        const unsigned clock_pin =
            std::array<unsigned, 4>{11, 13, 33, 35}[unsigned(assigned_clocks[output_cells[out]])];
        model.clock = simulation_clock_input(image.simulation_model, design, clock_pin);
    }
    for (const auto& alias : output_aliases) {
        const auto out      = output_indices.at(alias.second);
        const bool negative = alias_phases.at(alias.first);
        auto expression     = simulation_cover(pla.inputs, pla.terms[out]);
        const auto storage  = image.simulation_model.registers.find(alias.second);
        if (storage == image.simulation_model.registers.end()) {
            image.simulation_model.combinatorial[alias.first] = negative ? make_not(expression) : expression;
        } else {
            auto& model    = image.simulation_model.registers[alias.first];
            model          = storage->second;
            model.power_up = model.reset_value = simulation_bit(negative);
            model.preset_value                 = simulation_bit(!negative);
            model.next_state = equations.at(alias.first)->function == "T"
                                   ? make_binary(ExprKind::Xor, make_identifier(alias.first), expression)
                               : negative ? make_not(expression)
                                          : expression;
        }
    }
    std::set<std::string> active_signals(pla.inputs.begin(), pla.inputs.end());
    for (const auto& output : output_indices)
        active_signals.insert(output.first);
    validate_requested_power_up(image.simulation_model, design);
    apply_placement_metadata(image, design, active_signals);
    capture_report(image, true);
    return image;
}
}  // namespace
FuseImage fit_mach111_fixed(const Design& source, OptimizerOptions options, FitterOptions fitting)
{
    Design design = source;
    auto prepared = synthesize_fitter_pla(design, options, fitting.report.get());
    FittingDeadline deadline(fitting.runtime_quarters);
    return fit_mach111_prepared(design, prepared, fitting);
}
}  // namespace machxl
