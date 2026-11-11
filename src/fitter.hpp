#pragma once

#include "frontend.hpp"
#include "jedec.hpp"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace machxl {

struct FitterOptions
{
    bool balanced_partition         = true;
    bool spread_placement           = true;
    bool ground_implicit_controls   = false;
    bool zero_hold_time             = false;
    bool product_term_clocks        = false;
    unsigned reduce_global_clocks   = 0;
    unsigned runtime_quarters       = 0;
    unsigned source_route_limit     = 4096;
    std::string preplacement_policy = "No Change";
    std::string saved_placement;
    bool use_saved_placement     = false;
    bool automatic_input_pairing = true;
    bool iterate_partition       = true;
    // Optional native report observations, also retained on fitting failure.
    std::shared_ptr<FitterReportData> report;
    bool automatic_legacy_input_pairing = true;
    // Optional sink for the original fitter's warnings and INFO lines, each
    // stored with its severity. The fitters run their preparation once per
    // placement candidate; each text is kept once.
    std::shared_ptr<std::vector<std::string>> warnings;
    void warn(const std::string& text) const { message("warning: " + text); }
    void info(const std::string& text) const { message("info: " + text); }
    void message(const std::string& text) const
    {
        if (warnings && std::find(warnings->begin(), warnings->end(), text) == warnings->end())
            warnings->push_back(text);
    }
};

// Native MACH111 placement, routing and fuse assembly. Unsupported device
// resources and retry paths fail before any JEDEC is emitted.
FuseImage fit_mach111_fixed(const Design& design, OptimizerOptions options = {}, FitterOptions fitting = {});

}  // namespace machxl
