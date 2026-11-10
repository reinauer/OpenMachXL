#pragma once
#include "frontend.hpp"
#include "simulation_model.hpp"
#include <vector>

namespace machxl {
inline ExprPtr simulation_product(const std::vector<std::string>& inputs, const std::string& term)
{
    if (term.empty())
        return make_constant(false);
    if (term.size() != inputs.size())
        throw LogicError("Invalid fitted simulation product width");
    auto result = make_constant(true);
    for (unsigned i = 0; i < term.size(); ++i)
        if (term[i] != '-') {
            auto literal = make_identifier(inputs[i]);
            if (term[i] == '0')
                literal = make_not(literal);
            result = make_binary(ExprKind::And, result, literal);
        }
    return result;
}
inline ExprPtr simulation_cover(const std::vector<std::string>& inputs, const std::vector<std::string>& terms)
{
    auto result = make_constant(false);
    for (const auto& term : terms)
        result = make_binary(ExprKind::Or, result, simulation_product(inputs, term));
    return result;
}
inline ExprPtr simulation_clock_input(SimulationFitModel& model, const Design& design, unsigned number,
                                      bool negative = false)
{
    for (const auto& pin : design.pins)
        if (!pin.node && pin.number == number) {
            auto expression = make_identifier(pin.name);
            return pin.inverted != negative ? make_not(expression) : expression;
        }
    const auto name               = "$PIN" + std::to_string(number);
    model.implicit_inputs[number] = name;
    auto expression               = make_identifier(name);
    return negative ? make_not(expression) : expression;
}
inline void validate_requested_power_up(const SimulationFitModel& model, const Design& design)
{
    for (const auto& requested : design.requested_power_up) {
        const auto found = model.registers.find(requested.first);
        const auto value = requested.second ? SimulationValue::High : SimulationValue::Low;
        if (found == model.registers.end() || !found->second.power_up || *found->second.power_up != value)
            throw LogicError("Placement cannot implement requested power-up value for " + requested.first);
    }
}
inline SimulationValue simulation_bit(bool value)
{
    return value ? SimulationValue::High : SimulationValue::Low;
}
}  // namespace machxl
