#pragma once
#include "fitter_report_data.hpp"
#include "settings.hpp"
#include <string>
namespace machxl {
// timestamp is supplied by the caller, e.g. "Nov 10 11:47:46 2026".
// This keeps report rendering deterministic and makes no timing claims.
// The filename's capitalization is preserved, as in the original PLA header.
std::string render_fitter_report(const std::string& device, const FitterReportData& data,
                                 const CompilerSettings& settings, const std::string& source_filename,
                                 const std::string& timestamp);
}  // namespace machxl
