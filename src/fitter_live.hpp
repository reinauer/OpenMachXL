#pragma once
#include "frontend.hpp"
#include <map>
#include <set>

namespace machxl {
// MACHFITR checks set/reset product counts before unused-node removal,
// after discarding controls that do not apply to combinatorial outputs.
inline void validate_storage_control_products(const std::vector<std::string>& outputs,
                                              const std::vector<std::vector<std::string>>& terms)
{
    for (std::size_t column = 0; column < outputs.size(); ++column) {
        auto label = outputs[column];
        while (!label.empty() && (label.back() == '-' || label.back() == '%'))
            label.pop_back();
        const auto dot = label.find('.');
        if (dot == std::string::npos || terms[column].size() <= 1)
            continue;
        const auto function = label.substr(dot);
        if (function == ".RE" || function == ".PR")
            throw LogicError(std::string(function == ".RE" ? "z5520: RESET" : "z5519: SET") +
                             " requires at most one product term for " + label.substr(0, dot));
    }
}

// The original z5045 DRC runs before unused-equation elimination. Output
// pairs may supply their pad's enable, but an independent node cannot.
inline void validate_node_output_enables(const Design& design, const std::vector<std::string>& outputs)
{
    std::set<std::string> independent_nodes;
    for (const auto& pin : design.pins)
        if (pin.node && pin.pair.empty())
            independent_nodes.insert(pin.name);
    for (auto label : outputs) {
        while (!label.empty() && (label.back() == '-' || label.back() == '%'))
            label.pop_back();
        const auto dot = label.find('.');
        if (dot != std::string::npos && label.substr(dot) == ".OE" && independent_nodes.count(label.substr(0, dot)))
            throw LogicError("z5045: node " + label.substr(0, dot) + " cannot have an output enable equation");
    }
}

// MACHFITR z5072: a node that some equation reads must have an equation of
// its own. A buried node has no pad that could supply a value.
inline void validate_node_equations(const Design& design, const std::vector<std::string>& inputs,
                                    const std::vector<std::string>& outputs)
{
    std::set<std::string> driven;
    for (auto label : outputs) {
        while (!label.empty() && (label.back() == '-' || label.back() == '%'))
            label.pop_back();
        driven.insert(label.substr(0, label.find('.')));
    }
    const std::set<std::string> read(inputs.begin(), inputs.end());
    for (const auto& pin : design.pins)
        if (pin.node && read.count(pin.name) && !driven.count(pin.name))
            throw LogicError("z5072: Missing equation for Node " + pin.name);
}

// z5091/5092/5093 are warnings for a combinatorial output: set/reset/clock
// equations are discarded before DRC, routing and input-use classification.
inline void discard_combinatorial_storage_controls(std::vector<std::string>& outputs,
                                                   std::vector<std::vector<std::string>>& terms)
{
    std::set<std::string> combinatorial;
    for (auto label : outputs) {
        while (!label.empty() && (label.back() == '-' || label.back() == '%'))
            label.pop_back();
        const auto dot = label.find('.');
        if (dot == std::string::npos || label.substr(dot) == ".XORR")
            combinatorial.insert(label.substr(0, dot));
    }
    for (std::size_t column = outputs.size(); column-- > 0;) {
        auto label = outputs[column];
        while (!label.empty() && (label.back() == '-' || label.back() == '%'))
            label.pop_back();
        const auto dot = label.find('.');
        if (dot != std::string::npos &&
            (label.substr(dot) == ".C" || label.substr(dot) == ".RE" || label.substr(dot) == ".PR") &&
            combinatorial.count(label.substr(0, dot))) {
            outputs.erase(outputs.begin() + column);
            terms.erase(terms.begin() + column);
        }
    }
}

// MACHFITR repeatedly removes buried equations with no users. Self and
// mutual feedback count as users even when no package output observes them.
// Keep source declarations intact: even a discarded signal participates in
// the original logical-ID allocation before physical resource assignment.
inline std::set<std::string> retained_equation_names(const Design& design, const std::set<std::string>& primary,
                                                     const std::map<std::string, std::set<std::string>>& dependencies)
{
    std::set<std::string> pads;
    for (const auto& pin : design.pins)
        if (!pin.node && primary.count(pin.name))
            pads.insert(pin.name);
    auto retained = primary;
    for (;;) {
        auto used = pads;
        for (const auto& owner : retained) {
            auto found = dependencies.find(owner);
            if (found != dependencies.end())
                for (const auto& input : found->second)
                    if (retained.count(input))
                        used.insert(input);
        }
        for (const auto& pin : design.pins)
            if (pin.node && !pin.pair.empty() && retained.count(pin.pair) && retained.count(pin.name))
                used.insert(pin.name);
        if (used == retained)
            return retained;
        retained = std::move(used);
    }
}

inline std::set<std::string> retain_reachable_equations(const Design& design, std::vector<std::string>& inputs,
                                                        std::vector<std::string>& outputs,
                                                        std::vector<std::vector<std::string>>& terms)
{
    std::map<std::string, std::vector<unsigned>> columns;
    std::set<std::string> primary;
    std::vector<std::string> names;
    for (unsigned column = 0; column < outputs.size(); ++column) {
        auto label = outputs[column];
        while (!label.empty() && (label.back() == '-' || label.back() == '%'))
            label.pop_back();
        const auto dot      = label.find('.');
        const auto name     = label.substr(0, dot);
        const auto function = dot == std::string::npos ? "COM" : label.substr(dot + 1);
        columns[name].push_back(column);
        names.push_back(name);
        if (function == "COM" || function == "REG" || function == "T" || function == "G" ||
            function.compare(0, 4, "XORR") == 0)
            primary.insert(name);
    }
    std::map<std::string, std::set<std::string>> dependencies;
    for (const auto& name : primary)
        for (auto column : columns.at(name))
            for (const auto& term : terms[column])
                for (unsigned i = 0; i < inputs.size(); ++i)
                    if (term[i] != '-' && primary.count(inputs[i]))
                        dependencies[name].insert(inputs[i]);
    const auto reachable = retained_equation_names(design, primary, dependencies);
    for (std::size_t column = outputs.size(); column-- > 0;)
        if (!reachable.count(names[column])) {
            outputs.erase(outputs.begin() + column);
            terms.erase(terms.begin() + column);
        }
    std::vector<bool> live(inputs.size());
    for (const auto& cover : terms)
        for (const auto& term : cover)
            for (unsigned i = 0; i < inputs.size(); ++i)
                if (term[i] != '-')
                    live[i] = true;
    for (std::size_t i = inputs.size(); i-- > 0;)
        if (!live[i]) {
            inputs.erase(inputs.begin() + i);
            for (auto& cover : terms)
                for (auto& term : cover)
                    term.erase(i, 1);
        }
    return reachable;
}
}  // namespace machxl
