#pragma once
#include "simulator.hpp"

namespace machxl {
struct SimulationPlacement
{
    SimulationPinMap pins;
    std::map<std::string, SimulationValue> register_power_up;
    SimulationFitModel model;
    bool floating_pins = false;
};
// A saved simulator placement can change both location and declared phase.
// Without one, retain the source declarations, including floating pins.
SimulationPlacement prepare_simulation_placement(Design& design, const std::string* saved);
}  // namespace machxl
