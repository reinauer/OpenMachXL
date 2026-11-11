#include "fitter_metadata.hpp"
#include "device_family.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace machxl {
void apply_saved_placement(Design& design, const std::string& placement)
{
    // MACHFITR accepts its generated PLC/BLC records, optional comments and
    // headers. Names and record types are case insensitive. Unknown names
    // are ignored; later records replace earlier locations.
    const auto upper = [](std::string value) {
        for (auto& c : value)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return value;
    };
    design.groups.erase(std::remove_if(design.groups.begin(), design.groups.end(),
                                       [](const auto& group) { return group.first.compare(0, 9, "MACH_SEG_") == 0; }),
                        design.groups.end());
    std::map<std::string, std::string> names;
    for (const auto& alias : pla_signal_aliases(design))
        names[alias.second] = alias.first;
    std::istringstream input(placement);
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream record(line);
        std::string type, name;
        unsigned location;
        if (!(record >> type))
            continue;
        type = upper(type);
        if (type != "PIN" && type != "NODE")
            continue;
        if (!(record >> location >> name))
            throw LogicError("Malformed placement record: " + line);
        name = upper(name);
        if (!name.empty() && name.front() == '/')
            name.erase(0, 1);
        if (names.count(name))
            name = names.at(name);
        auto pin =
            std::find_if(design.pins.begin(), design.pins.end(), [&](const auto& p) { return upper(p.name) == name; });
        if (pin == design.pins.end())
            continue;
        if (pin->node != (type == "NODE"))
            throw LogicError("z5097: placement pin/node type differs for " + pin->name);
        pin->number = location;
    }
}

void apply_preplacement_policy(Design& design, const std::string& policy)
{
    if (policy == "No Change")
        return;
    const bool all   = policy == "Float Pins, Nodes" || policy == "Float Pins, Nodes, Groups";
    const bool nodes = policy == "Float Nodes, Groups" || policy == "Float Non-Input Nodes, Groups";
    if (!all && !nodes)
        throw LogicError("Unknown preplacement policy: " + policy);
    std::set<std::string> input_nodes;
    for (const auto& pin : design.pins)
        if (!pin.node && !pin.pair.empty())
            input_nodes.insert(pin.pair);
    for (auto& pin : design.pins)
        if (all || (pin.node && !input_nodes.count(pin.name)))
            pin.number = 0;
    if (policy != "Float Pins, Nodes")
        design.groups.erase(
            std::remove_if(design.groups.begin(), design.groups.end(),
                           [](const auto& group) { return group.first.compare(0, 9, "MACH_SEG_") == 0; }),
            design.groups.end());
}

void validate_fixed_locations(const Design& design)
{
    // The fitters store locations in 16-bit placement keys. An unchecked
    // number would alias a real location or turn into "unconstrained".
    const auto& layout    = mach_device_layout(design.device);
    const auto node_limit = mach_node_limit(layout);
    for (const auto& pin : design.pins) {
        if (!pin.number)
            continue;
        if (pin.node ? pin.number < 2 || pin.number > node_limit : pin.number > layout.pins)
            throw LogicError(std::string("Invalid ") + (pin.node ? "node" : "pin") + " location " +
                             std::to_string(pin.number) + " for " + pin.name);
    }
}

void apply_placement_metadata(FuseImage& image, const Design& design, const std::set<std::string>& active_signals)
{
    const std::map<std::string, std::pair<unsigned, std::string>> devices = {
#include "frontend_devices.inc"
    };
    const auto& device = devices.at(design.device);
    if (device.first != image.pin_count)
        throw LogicError("Placement package pin count mismatch");
    image.pin_signals.resize(image.pin_count + 1);
    image.pin_inverted.resize(image.pin_count + 1);
    std::istringstream supplies(device.second);
    std::string supply;
    while (supplies >> supply) {
        const auto colon                                           = supply.find(':');
        image.pin_signals.at(std::stoul(supply.substr(colon + 1))) = supply.substr(0, colon);
    }
    for (const auto& pin : design.pins) {
        if (!active_signals.count(pin.name))
            continue;
        if (pin.node) {
            if (pin.number) {
                if (image.node_signals.size() <= pin.number)
                    image.node_signals.resize(pin.number + 1);
                image.node_signals[pin.number] = pin.name;
            }
            continue;
        }
        if (!pin.number || pin.number > image.pin_count || !image.pin_signals[pin.number].empty())
            throw LogicError("Invalid final pin placement for " + pin.name);
        image.pin_signals[pin.number]  = pin.name;
        image.pin_inverted[pin.number] = pin.inverted;
    }
}
}  // namespace machxl
