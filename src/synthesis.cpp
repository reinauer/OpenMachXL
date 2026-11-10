#include "synthesis.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace machxl {
namespace {
std::string uppercase(std::string text)
{
    for (char& c : text)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return text;
}
}  // namespace

SynthesisOptions synthesis_options(const CompilerSettings& settings, const std::string& device)
{
    SynthesisOptions result;
    result.frontend.automatic_input_pairing  = settings.yes_no("P_N");
    result.frontend.automatic_output_pairing = settings.yes_no("PAIR");
    const auto conditional                   = uppercase(settings.get("BPP_SIGNAL"));
    if (conditional == "OFF")
        result.frontend.conditional_default_off = true;
    else if (conditional == "DON'T CARE")
        result.frontend.conditional_default_off = false;
    else
        throw std::invalid_argument("Unsupported BPP_SIGNAL setting: " + settings.get("BPP_SIGNAL"));
    result.optimizer.fast                   = settings.yes_no("FAST_MIN");
    result.optimizer.external_best_polarity = settings.yes_no("EQN_22V10");
    const auto registers                    = uppercase(settings.get("D_T_REG_OPTM"));
    if (registers == "CHANGE ALL TO D-TYPE")
        result.registers = RegisterSelection::D;
    else if (registers == "CHANGE ALL TO T-TYPE")
        result.registers = RegisterSelection::T;
    else if (registers == "BEST TYPE FOR DEVICE")
        result.optimizer.best_registers = true;
    else if (registers == "D,T AS SPECIFIED. JK,RS TO T")
        result.registers = RegisterSelection::JkRsT;
    else if (registers == "D,T AS SPECIFIED. JK,RS TO BEST")
        result.optimizer.best_jk_rs = true;
    else if (registers != "D,T AS SPECIFIED. JK,RS TO D")
        throw std::invalid_argument("Register selection policy is not implemented: " + settings.get("D_T_REG_OPTM"));
    const auto polarity = uppercase(settings.get("M_POLAR"));
    if (polarity == "HIGH, ACTIVE HIGH")
        result.polarity = PolaritySelection::High;
    else if (polarity == "LOW, ACTIVE LOW")
        result.polarity = PolaritySelection::Low;
    else if (polarity == "BEST FOR DEVICE")
        result.optimizer.best_polarity = true;
    else if (polarity != "AS SPECIFIED IN DESIGN FILE")
        throw std::invalid_argument("Polarity selection policy is not implemented: " + settings.get("M_POLAR"));
    if (result.polarity != PolaritySelection::AsSpecified)
        result.optimizer.forced_polarity = result.polarity == PolaritySelection::Low ? 1 : 0;
    result.optimizer.gate_split          = settings.yes_no("GATE_SPLIT");
    result.optimizer.product_term_clocks = settings.yes_no("PTERM_CLK");
    if (result.optimizer.gate_split && !device.empty()) {
        auto positive = [&](const char* key) {
            auto value = settings.integer(key);
            if (!settings.dos_compatibility() && (value == 0 || value > 960))
                throw std::invalid_argument(std::string("Invalid gate splitting setting: ") + key);
            return unsigned(value);
        };
        if (device.compare(0, 5, "MACH1") == 0) {
            result.optimizer.gate_threshold_mach1 = positive("THRESHOLD_12");
            result.optimizer.gate_bucket_mach1    = positive("BUCKET_SIZE0");
        } else if (device.compare(0, 5, "MACH2") == 0) {
            result.optimizer.gate_threshold_mach2 = positive(device == "MACH215" ? "THRESHOLD_12" : "THRESHOLD_2X");
            result.optimizer.gate_bucket_mach2    = positive(device == "MACH215" ? "BUCKET_SIZE0" : "BUCKET_SIZE1");
        } else {
            result.optimizer.gate_threshold = positive("THRESHOLD");
            result.optimizer.gate_bucket    = positive("BUCKET_SIZE");
        }
    }
    return result;
}

Design transform_synthesis(Design design, const SynthesisOptions& options)
{
    auto primary = [&](const std::string& name) -> const Equation* {
        for (const auto& e : design.equations)
            if (e.signal == name &&
                (e.function == "REG" || e.function == "T" || e.function == "COM" || e.function == "G"))
                return &e;
        return nullptr;
    };
    for (const auto& node : design.pins)
        if (node.node && !node.pair.empty() && node.pair_kind != "IPAIR") {
            const auto* a = primary(node.name);
            const auto* b = primary(node.pair);
            if (!a || !b || a->function != b->function)
                continue;
            auto left = sum_of_products(a->expression), right = sum_of_products(b->expression);
            std::sort(left.begin(), left.end());
            std::sort(right.begin(), right.end());
            if (left == right)
                design.output_pair_phases[node.name] = a->inverted != b->inverted;
        }
    for (auto& equation : design.equations) {
        const bool to_d = options.registers == RegisterSelection::D && equation.function == "T";
        const bool to_t = (options.registers == RegisterSelection::T ||
                           (options.registers == RegisterSelection::JkRsT && !equation.excitation_origin.empty())) &&
                          equation.function == "REG";
        if (to_d || to_t) {
            equation.source_clauses.clear();
            // The hardware-XOR constructor puts feedback on the left;
            // the legacy plain-cover converter appends it on the right. A
            // negative target names the complemented storage observation,
            // so its feedback literal is negative as well. T excitation
            // itself is invariant under complementing the stored value.
            auto feedback = make_identifier(equation.signal);
            if (equation.inverted)
                feedback = make_not(feedback);
            const bool hardware = design.device[4] == '3' || design.device[4] == '4';
            auto convert        = [&](const ExprPtr& expression) {
                return hardware ? make_binary(ExprKind::Xor, feedback, expression)
                                : make_binary(ExprKind::Xor, expression, feedback);
            };
            equation.expression = convert(equation.expression);
            if (equation.opposite_expression)
                equation.opposite_expression = convert(equation.opposite_expression);
            equation.function             = to_d ? "REG" : "T";
            equation.converted_excitation = true;
        }
        // Legacy devices minimize the requested cover directly. MACH3/4
        // search both phases, then express the requested phase using XOR.
        if (design.device.compare(0, 5, "MACH1") == 0 || design.device.compare(0, 5, "MACH2") == 0) {
            if (equation.function != "REG" && equation.function != "COM" && equation.function != "G")
                continue;
            // Best builds both excitation candidates from the source phase.
            // Its D candidate receives the requested phase independently;
            // changing the source here would also change the T candidate.
            if (equation.function == "REG" && equation.minimize &&
                (options.optimizer.best_registers ||
                 (options.optimizer.best_jk_rs && !equation.excitation_origin.empty())) &&
                !design.requested_power_up.count(equation.signal))
                continue;
            if (options.polarity == PolaritySelection::AsSpecified)
                continue;
            const bool negative = options.polarity == PolaritySelection::Low;
            if (equation.inverted != negative) {
                equation.source_clauses.clear();
                if (equation.opposite_expression) {
                    std::swap(equation.expression, equation.opposite_expression);
                    std::swap(equation.care, equation.opposite_care);
                    std::swap(equation.minimize, equation.opposite_minimize);
                } else
                    equation.expression = make_not(equation.expression);
                equation.inverted = negative;
            }
        }
    }
    return design;
}
}  // namespace machxl
