#pragma once
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
}  // namespace machxl
