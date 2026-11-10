#include "frontend.hpp"
#include "synthesis.hpp"
#include "fitter.hpp"
#include "fitter_family.hpp"
#include "fitter_modern.hpp"
#include "timing_report.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
std::string read(const char* path) {
    std::ifstream file(path);
    if(!file)throw std::runtime_error("Cannot read report fixture");
    std::stringstream contents;contents<<file.rdbuf();return contents.str();
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=3)throw std::runtime_error("Expected PDS MXL");
        machxl::CompilerSettings settings;settings.parse(read(argv[2]),true);
        auto options=machxl::synthesis_options(settings);
        options.frontend.dos_compatibility=true;
        auto design=machxl::parse_design(read(argv[1]),options.frontend);
        options=machxl::synthesis_options(settings,design.device);
        options.frontend.dos_compatibility=true;options.optimizer.dos_compatibility=true;
        design=machxl::transform_synthesis(design,options);
        machxl::FitterOptions fitting;
        fitting.report=std::make_shared<machxl::FitterReportData>();
        fitting.balanced_partition=settings.yes_no("BALANCED_PAR");
        fitting.spread_placement=settings.yes_no("SPREAD_PL");
        fitting.ground_implicit_controls=!settings.yes_no("M3XX_SET_RST");
        fitting.zero_hold_time=settings.yes_no("ZERO_HOLD_TM");
        fitting.product_term_clocks=settings.yes_no("PTERM_CLK");
        fitting.reduce_global_clocks=settings.yes_no("SIZE_REDUCE")?settings.integer("SIZE_REDUCE_N"):0;
        fitting.source_route_limit=settings.yes_no("ROUTE_PER_PL")?2:4096;
        fitting.iterate_partition=settings.yes_no("ITERATE_FITR");
        fitting.automatic_input_pairing=settings.fitter_yes_no("P_N");
        fitting.automatic_legacy_input_pairing=settings.fitter_yes_no("PAIR");
        fitting.preplacement_policy=settings.get("M3XX_PREPLACE");
        try {
            if(design.device=="MACH111")machxl::fit_mach111_fixed(design,options.optimizer,fitting);
            else if(design.device[4]=='1'||design.device[4]=='2')machxl::fit_mach_family_fixed(design,options.optimizer,fitting);
            else machxl::fit_mach_modern(design,options.optimizer,fitting);
        } catch(const std::exception&) {
            if(fitting.report->optimized_pla.empty())throw;
        }
        const auto& report=*fitting.report;
        if(!report.optimized_design)throw std::runtime_error("Missing optimizer report capture");
        std::cout<<machxl::render_timing_report(*report.optimized_design,report.optimized_pla,
            &report,argv[1]);
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
