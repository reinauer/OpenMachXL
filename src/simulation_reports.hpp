#pragma once
#include "simulator.hpp"

namespace machxl {
// DOS text histories: CRLF, sixty samples per page, FF separators.
std::string render_simulation_history(const Design& design, const SimulationResult& result,
                                      const SimulationPinMap& placement);
std::string render_simulation_trace(const Design& design, const SimulationResult& result,
                                    const SimulationPinMap& placement);
}  // namespace machxl
