#pragma once
#include "frontend.hpp"
#include "jedec.hpp"
#include <set>
namespace machxl {
void apply_saved_placement(Design& design, const std::string& placement);
void apply_preplacement_policy(Design& design, const std::string& policy);
// Reject fixed locations outside the device before a fitter narrows them.
void validate_fixed_locations(const Design& design);
void apply_placement_metadata(FuseImage& image, const Design& placed_design,
                              const std::set<std::string>& active_signals);
}  // namespace machxl
