#pragma once
#include "fitter_report_data.hpp"
#include "frontend.hpp"
#include <string>
#include <vector>

namespace machxl {
// The label and product sections of an espresso PLA as emit_pla() writes it.
struct PlaText
{
    // .ilb and .ob labels exactly as written, including phase suffixes and
    // ".FUNCTION" parts.
    std::vector<std::string> inputs, outputs;
    // Per output column the input patterns of its product rows, in file
    // order. A PLA without inputs has rows that consist of the output mask
    // only; each such row contributes an empty pattern.
    std::vector<std::vector<std::string>> terms;
    // A row or label list did not have the size that .i/.o or the labels
    // announce. Such rows are skipped.
    bool malformed = false;
};

// Reads the directives .i, .o, .ilb, .ob (with continuation lines), .p and
// .e. Comment lines and unknown directives are ignored.
PlaText parse_pla_text(const std::string& text);

// An output label is NAME[.FUNCTION] followed by "-" for a negative-phase
// cover and "%" for an inverted output. Returns the label without those.
inline std::string strip_pla_phase(std::string label)
{
    while (!label.empty() && (label.back() == '-' || label.back() == '%'))
        label.pop_back();
    return label;
}

// The PLA a fitter works on: labels carry the design's signal names, and
// each output column lists its product terms.
struct FitterPla
{
    std::vector<std::string> inputs, outputs;
    std::vector<std::vector<std::string>> terms;
};

// Minimizes `design` through emit_pla() and reads the result back, so the
// fitters share the minimizer, don't-care handling, MINIMIZE_OFF semantics
// and product ordering of the PLA output. `design` receives the synthesized
// design; `report`, when given, keeps the PLA text and that design. Throws
// LogicError if the text is not a well-formed PLA.
FitterPla synthesize_fitter_pla(Design& design, OptimizerOptions options, FitterReportData* report = nullptr);
}  // namespace machxl
