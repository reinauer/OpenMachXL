#pragma once
#include "fitter.hpp"
namespace machxl {
FuseImage fit_mach_modern(const Design& design, OptimizerOptions options = {}, FitterOptions fitting = {});
}
