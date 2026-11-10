#pragma once
#include "fitter_family_place.hpp"
#include "fitter_report_data.hpp"
#include <map>
namespace machxl {
void capture_legacy_placement_failure(FitterReportData& report, const std::vector<Mach111PlaceSignal>& signals,
                                      const Mach111Placement& frame, unsigned block, const std::vector<bool>& assigned,
                                      const std::map<std::string, unsigned>& ids,
                                      const std::map<std::string, unsigned>& feedback_ids);
void capture_legacy_placement_failure(FitterReportData& report, const std::vector<Mach111PlaceSignal>& signals,
                                      const FamilyPlacement& frame, unsigned block, const std::vector<bool>& assigned,
                                      const std::map<std::string, unsigned>& ids,
                                      const std::map<std::string, unsigned>& feedback_ids);
}  // namespace machxl
