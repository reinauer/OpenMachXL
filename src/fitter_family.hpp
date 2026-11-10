#pragma once
#include "fitter.hpp"
#include "jedec.hpp"
namespace machxl {
// Native MACH1/2 I/O fitting: COM/D/T equations with adjacent product groups, dedicated
// clocks and controls. Automatic first-pass placement is supported.
// Unsupported placement or control forms fail explicitly.
FuseImage fit_mach_family_fixed(const Design& design, OptimizerOptions options = {}, FitterOptions fitting = {});
}  // namespace machxl
