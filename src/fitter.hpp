#pragma once

#include "frontend.hpp"
#include "jedec.hpp"
#include <memory>

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
};

// Native MACH111 placement, routing and fuse assembly. Unsupported device
// resources and retry paths fail before any JEDEC is emitted.
FuseImage fit_mach111_fixed(const Design& design, OptimizerOptions options = {}, FitterOptions fitting = {});

}  // namespace machxl
