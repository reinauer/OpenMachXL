#pragma once
#include <string>
namespace machxl {
struct Design;
struct FitterReportData;
// Structural CSM-pass counts from the actual optimizer output. Accepted
// placement supplies input-register decisions; a failed fit may pass null.
std::string render_timing_report(const Design& design, const std::string& optimized_pla, const FitterReportData* fitted,
                                 const std::string& source_filename);
}  // namespace machxl
