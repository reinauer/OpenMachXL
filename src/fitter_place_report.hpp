#pragma once
#include "fitter_report_data.hpp"
#include "settings.hpp"
#include <string>
namespace machxl {
struct FuseImage;
// Native observations provide counters; absent counters are printed as
// unavailable, never replaced by an invented original attempt count.
std::string render_fitter_place_report(const FuseImage& image, const FitterReportData& data,
                                       const CompilerSettings& settings, const std::string& source_filename,
                                       const std::string& start, const std::string& end, const std::string& elapsed);
}  // namespace machxl
