#pragma once
#include "frontend.hpp"
#include "jedec.hpp"
#include <set>
namespace machxl {
void apply_saved_placement(Design& design, const std::string& placement);
void apply_preplacement_policy(Design& design, const std::string& policy);
void apply_placement_metadata(FuseImage& image, const Design& placed_design,
                              const std::set<std::string>& active_signals);
}  // namespace machxl
