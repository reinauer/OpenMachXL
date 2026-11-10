#pragma once

#include "frontend.hpp"
#include "settings.hpp"

namespace machxl {
enum class RegisterSelection
{
    AsSpecified,
    D,
    T,
    JkRsT
};
enum class PolaritySelection
{
    AsSpecified,
    High,
    Low
};
struct SynthesisOptions
{
    FrontendOptions frontend;
    OptimizerOptions optimizer;
    RegisterSelection registers = RegisterSelection::AsSpecified;
    PolaritySelection polarity  = PolaritySelection::AsSpecified;
};

// Decode frontend and optimizer policies. Gate parameters are decoded
// only when a device is supplied, using its architecture-specific keys.
SynthesisOptions synthesis_options(const CompilerSettings& settings, const std::string& device = {});
Design transform_synthesis(Design design, const SynthesisOptions& options);
}  // namespace machxl
