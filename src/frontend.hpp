#pragma once

#include "logic.hpp"
#include "optimizer.hpp"
#include <map>
#include <string>
#include <vector>

namespace machxl {
struct Pin
{
    std::string name;
    unsigned number = 0;
    bool registered = false;
    bool node       = false;
    bool latched    = false;
    bool inverted   = false;
    std::string pair;
    std::string pair_kind;
    // The node's transfer equation was generated from a declared input
    // pairing (B1062); the design has no equation for it.
    bool generated_transfer = false;
    bool storage_declared = false;
};
struct Equation
{
    std::string signal;
    std::string function;
    ExprPtr expression;
    ExprPtr care;
    bool inverted  = false;
    bool minimize  = true;
    bool inherited = false;
    // Explicit opposite-polarity equations are independent covers.
    ExprPtr opposite_expression;
    ExprPtr opposite_care;
    bool opposite_minimize = true;
    // Individual source assignments retain expansion order with MINIMIZE_OFF.
    std::vector<ExprPtr> source_clauses;
    // STATE expansion creates explicit negative covers before the
    // one-equation pass. Retain them only for compatibility pairing.
    ExprPtr pairing_opposite_expression;
    // Retained through JK/RS lowering for excitation-specific policies.
    std::string excitation_origin;
    // D/T conversion adds feedback after source XOR orientation. Its new
    // legs do not repeat the source dual-complement preparation pass.
    bool converted_excitation = false;
};
struct Design
{
    std::string chip;
    std::string device;
    std::map<std::string, std::string> metadata;
    std::vector<Pin> pins;
    std::vector<Equation> equations;
    std::string signature;
    std::string simulation;
    std::vector<std::pair<std::string, std::vector<std::string>>> groups;
    // STATE names and CONDITIONS remain usable by simulation CHECK/IF.
    std::map<std::string, ExprPtr> simulation_symbols;
    // Corrected START_UP.OUTF requirements, in logical signal polarity.
    std::map<std::string, bool> requested_power_up;
    // Logical node/pad phase of validated source output pairs. Synthesis
    // conversion may add different feedback names to their equivalent ASTs.
    std::map<std::string, bool> output_pair_phases;
    std::vector<std::string> diagnostics;
    bool dos_compatibility = false;
};

struct FrontendOptions
{
    // Reproduce confirmed DOS defects, including CASE numeric conversion.
    bool dos_compatibility = false;
    // BPP_SIGNAL Off supplies zero for uncovered conditional branches.
    // Otherwise the care expression preserves only explicitly covered inputs.
    bool conditional_default_off  = true;
    bool automatic_input_pairing  = true;
    bool automatic_output_pairing = true;
};
Design parse_design(const std::string& source, FrontendOptions options = {},
                    const std::string* auxiliary_simulation = nullptr);
std::map<std::string, std::string> pla_signal_aliases(const Design& design);
std::string emit_pla(const Design& design, const std::string& source_name, OptimizerOptions options = {},
                     Design* expanded_design = nullptr);
}  // namespace machxl
