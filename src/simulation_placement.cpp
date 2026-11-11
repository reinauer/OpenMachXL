#include "simulation_placement.hpp"
#include "device_family.hpp"
#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace machxl {
SimulationPlacement prepare_simulation_placement(Design& design, const std::string* saved)
{
    const auto uppercase_signal_name = [](std::string text) {
        for (auto& c : text)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return text;
    };
    const bool modern = design.device[4] == '3' || design.device[4] == '4';
    std::map<std::string, bool> s6;
    std::map<unsigned, std::string> input_pairs, node_names;
    if (saved) {
        std::map<std::string, std::string> names;
        for (const auto& alias : pla_signal_aliases(design))
            names[alias.second] = alias.first;
        std::istringstream input(*saved);
        std::string line;
        const std::regex s6_field("S6=([01])", std::regex::icase);
        const std::regex input_pair_field("IPair +([0-9]+)", std::regex::icase);
        while (std::getline(input, line)) {
            std::istringstream row(line);
            std::string kind, name;
            unsigned number;
            if (!(row >> kind))
                continue;
            kind = uppercase_signal_name(kind);
            if (kind != "PIN" && kind != "NODE")
                continue;
            if (!(row >> number >> name))
                throw LogicError("Malformed simulation placement record: " + line);
            const bool inverted = !name.empty() && name.front() == '/';
            if (inverted)
                name.erase(0, 1);
            name = uppercase_signal_name(name);
            if (names.count(name))
                name = names.at(name);
            auto pin =
                std::find_if(design.pins.begin(), design.pins.end(), [&](const auto& p) { return p.name == name; });
            if (pin == design.pins.end())
                continue;
            if (pin->node != (kind == "NODE"))
                throw LogicError("Simulation placement pin/node type differs for " + name);
            pin->number   = number;
            pin->inverted = inverted;
            std::smatch match;
            s6[name] = std::regex_search(line, match, s6_field) && match[1] == "1";
            if (kind == "NODE")
                node_names[number] = name;
            else if (std::regex_search(line, match, input_pair_field))
                input_pairs[std::stoul(match[1])] = name;
        }
    }
    SimulationPlacement result;
    const std::map<std::string, std::pair<unsigned, std::string>> devices = {
#include "frontend_devices.inc"
    };
    const auto& device    = devices.at(design.device);
    const auto node_limit = mach_node_limit(mach_device_layout(design.device));
    result.pins.signals.resize(device.first + 1);
    result.pins.inverted.resize(device.first + 1);
    std::istringstream supplies(device.second);
    std::string supply;
    while (supplies >> supply) {
        const auto colon                                             = supply.find(':');
        result.pins.signals.at(std::stoul(supply.substr(colon + 1))) = supply.substr(0, colon);
    }
    for (const auto& pin : design.pins) {
        // MACHSIMU reads S6 from package-pin records. Buried-node records
        // retain source initialization, even when their printed S6 differs.
        if (saved && modern && !pin.node)
            for (const auto& equation : design.equations)
                if (equation.signal == pin.name &&
                    (equation.function == "REG" || equation.function == "T" || equation.function == "G"))
                    result.register_power_up[pin.name] =
                        (!s6[pin.name] != pin.inverted) ? SimulationValue::High : SimulationValue::Low;
        if (pin.node) {
            // The number sizes and indexes the node table below.
            if (pin.number > node_limit)
                throw LogicError("Invalid simulation node placement for " + pin.name);
            if (pin.number) {
                if (result.pins.nodes.size() <= pin.number)
                    result.pins.nodes.resize(pin.number + 1);
                result.pins.nodes[pin.number] = pin.name;
            }
            continue;
        }
        if (!pin.number) {
            result.floating_pins = true;
            continue;
        }
        if (pin.number > device.first || !result.pins.signals[pin.number].empty())
            throw LogicError("Invalid simulation pin placement for " + pin.name);
        result.pins.signals[pin.number]  = pin.name;
        result.pins.inverted[pin.number] = pin.inverted;
    }
    for (const auto& pair : input_pairs) {
        if (!node_names.count(pair.first))
            throw LogicError("Saved input pair has no node placement");
        const auto& name    = node_names.at(pair.first);
        bool input_negative = false, node_negative = false;
        for (const auto& pin : design.pins) {
            if (pin.name == pair.second)
                input_negative = pin.inverted;
            if (pin.name == name)
                node_negative = pin.inverted;
        }
        for (const auto& equation : design.equations)
            if (equation.signal == name && (equation.function == "REG" || equation.function == "G")) {
                auto& model          = result.model.registers[name];
                model.input_register = true;
                model.next_state     = equation.expression;
                if (node_negative)
                    model.next_state = make_not(model.next_state);
                const bool phase = evaluate(model.next_state, {{pair.second, input_negative}});
                model.power_up = model.reset_value = phase ? SimulationValue::High : SimulationValue::Low;
                model.preset_value                 = phase ? SimulationValue::Low : SimulationValue::High;
                model.reset = model.preset = make_constant(false);
            }
    }
    return result;
}
}  // namespace machxl
