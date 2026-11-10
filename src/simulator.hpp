#pragma once

#include "frontend.hpp"
#include "simulation_model.hpp"
#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace machxl {
struct SimulationSignal
{
    SimulationValue value = SimulationValue::Unknown;
    bool output           = false;
    // A pulse is represented as one JEDEC C/K vector, after propagation.
    int clock_pulse = 0;  // +/-1 full pulse; +/-2 SETF rising/falling edge
};
struct SimulationVector
{
    std::map<std::string, SimulationSignal> signals;
    // Forces on physical pins not necessarily named by the source.
    std::map<unsigned, SimulationSignal> physical_inputs;
};
struct SimulationMismatch
{
    std::size_t vector = 0;
    std::string signal;
    SimulationValue expected = SimulationValue::Unknown;
    SimulationValue actual   = SimulationValue::Unknown;
};
struct SimulationTraceSignal
{
    std::string name;
    bool inverted = false;
};
struct SimulationReportEvent
{
    SimulationVector state;
    char command = ' ';
    int trace    = -1;
    std::vector<std::string> clashes;
};
struct SimulationReportClash
{
    std::size_t event = 0;
    std::string signal;
    SimulationValue expected = SimulationValue::Unknown;
    SimulationValue actual   = SimulationValue::Unknown;
    bool register_check      = false;
};
struct SimulationResult
{
    std::vector<SimulationVector> vectors;
    std::vector<SimulationMismatch> mismatches;
    bool vectors_disabled_by_preload = false;
    std::string device;
    std::vector<SimulationReportEvent> report_events;
    std::vector<std::vector<SimulationTraceSignal>> report_traces;
    std::vector<SimulationReportClash> report_clashes;
};
struct SimulationPinMap;
struct SimulationOptions
{
    const SimulationPinMap* placement      = nullptr;
    const SimulationFitModel* fitted_model = nullptr;
    bool require_fitted_registers          = false;
    // Optional logical register power-up values from the actual fit.
    // Modern devices select these with each macrocell's S6 control swap.
    std::map<std::string, SimulationValue> register_power_up;
    // Legacy set/reset simulation selected by MXL EQN_22V10.
    bool legacy_set_reset             = true;
    bool collect_reports              = false;
    std::size_t max_commands          = 1000000;
    std::size_t max_vectors           = 100000;
    std::size_t max_propagation_steps = 1024;
};
struct SimulationPinMap
{
    // Index zero unused. GND/VCC identify supplies; empty names are unassigned.
    std::vector<std::string> signals;
    std::vector<bool> inverted;
    // Physical node number indexes; entries0/1 unused.
    std::vector<std::string> nodes;
};
SimulationResult simulate_design(const Design& design, SimulationOptions options = {});
std::vector<std::string> encode_simulation_vectors(const SimulationResult& simulation,
                                                   const SimulationPinMap& placement);
}  // namespace machxl
