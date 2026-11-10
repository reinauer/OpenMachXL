#pragma once
#include "fitter_place.hpp"
#include "fitter_report_data.hpp"
#include <map>
namespace machxl {
// Build the semantic report graph from a retained partition, including a
// complete partition whose later physical placement failed.
void capture_fitter_partition_state(FitterReportData& report, const std::vector<Mach111PlaceSignal>& signals,
                                    const std::vector<std::size_t>& cluster_signals,
                                    const PartitionObservation& partition, const std::map<std::string, unsigned>& ids,
                                    const std::map<std::string, unsigned>& feedback_ids);
// Convert an actual retained failed partition into report observations.
// Uses the already captured optimized PLA; never re-runs synthesis or fitting.
void capture_partition_failure_report(FitterReportData& report, const std::vector<Mach111PlaceSignal>& signals,
                                      const std::vector<std::size_t>& cluster_signals,
                                      const PartitionObservation& partition, const std::map<std::string, unsigned>& ids,
                                      const std::map<std::string, unsigned>& feedback_ids);
}  // namespace machxl
