#include "pla_text.hpp"
#include "logic.hpp"
#include <map>
#include <sstream>

namespace machxl {
PlaText parse_pla_text(const std::string& text)
{
    PlaText p;
    std::istringstream file(text);
    std::string line, labels;
    bool products = false, counted_inputs = false, counted_outputs = false;
    unsigned input_count = 0, output_count = 0;
    while (std::getline(file, line)) {
        std::istringstream fields(line);
        std::string token;
        if (!(fields >> token) || token[0] == '#')
            continue;
        if (token[0] == '.') {
            labels.clear();
            if (token == ".i")
                counted_inputs = bool(fields >> input_count);
            else if (token == ".o")
                counted_outputs = bool(fields >> output_count);
            else if (token == ".ilb" || token == ".ob")
                labels = token;
            else if (token == ".p")
                products = true;
            else if (token == ".e")
                break;
            if (labels.empty())
                continue;
        } else if (products) {
            const auto inputs  = counted_inputs ? input_count : unsigned(p.inputs.size());
            const auto outputs = counted_outputs ? output_count : unsigned(p.outputs.size());
            std::string pattern, mask;
            if (inputs) {
                pattern = token;
                fields >> mask;
            } else
                mask = token;
            if (pattern.size() != inputs || mask.size() != outputs) {
                p.malformed = true;
                continue;
            }
            p.terms.resize(outputs);
            for (unsigned out = 0; out < outputs; ++out)
                if (mask[out] == '1')
                    p.terms[out].push_back(pattern);
            continue;
        } else if (!labels.empty())
            (labels == ".ilb" ? p.inputs : p.outputs).push_back(token);
        if (!labels.empty())
            while (fields >> token)
                (labels == ".ilb" ? p.inputs : p.outputs).push_back(token);
    }
    if (counted_inputs && p.inputs.size() != input_count && !(p.inputs.empty() && !input_count))
        p.malformed = true;
    if (counted_outputs && p.outputs.size() != output_count)
        p.malformed = true;
    p.terms.resize(counted_outputs ? output_count : p.outputs.size());
    return p;
}
FitterPla synthesize_fitter_pla(Design& design, OptimizerOptions options, FitterReportData* report)
{
    const auto text = emit_pla(design, "design.pds", options, &design);
    if (report) {
        report->optimized_pla    = text;
        report->optimized_design = std::make_shared<Design>(design);
    }
    auto parsed = parse_pla_text(text);
    if (parsed.malformed)
        throw LogicError("invalid synthesized PLA");
    // A constant PLA has no input patterns to tell its rows apart.
    if (parsed.inputs.empty())
        for (auto& terms : parsed.terms)
            if (terms.size() > 1)
                terms.resize(1);
    FitterPla p{std::move(parsed.inputs), std::move(parsed.outputs), std::move(parsed.terms)};
    // emit_pla() writes PLA-safe aliases; the fitters use the design's names.
    std::map<std::string, std::string> original_names;
    for (const auto& alias : pla_signal_aliases(design))
        original_names.emplace(alias.second, alias.first);
    for (auto& input : p.inputs)
        input = original_names.at(input);
    for (auto& output : p.outputs) {
        auto length = output.find('.');
        if (length == std::string::npos)
            length = strip_pla_phase(output).size();
        output = original_names.at(output.substr(0, length)) + output.substr(length);
    }
    return p;
}
}  // namespace machxl
