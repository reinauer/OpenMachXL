#pragma once

#include "logic.hpp"
#include <map>
#include <optional>
#include <string>

namespace machxl {
enum class SimulationValue
{
    Low,
    High,
    Unknown,
    HighImpedance
};

// Values and expressions use original logical signal names and polarity.
// A null expression retains the corresponding source equation. next_state,
// when supplied, is the logical D next-state function, including T feedback.
struct SimulationRegisterModel
{
    bool input_register = false;
    std::optional<SimulationValue> power_up;
    std::optional<SimulationValue> reset_value;
    std::optional<SimulationValue> preset_value;
    ExprPtr next_state;
    ExprPtr clock;
    ExprPtr reset;
    ExprPtr preset;
};
struct SimulationFitModel
{
    // Physical input pins with no declared source signal, such as default clocks.
    std::map<unsigned, std::string> implicit_inputs;
    std::map<std::string, SimulationRegisterModel> registers;
    std::map<std::string, ExprPtr> combinatorial;
};
}  // namespace machxl
