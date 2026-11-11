#include "timing_report.hpp"
#include "pla_text.hpp"
#include "fitter_live.hpp"
#include "fitter_report_data.hpp"
#include "frontend.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>

namespace machxl {
namespace {
struct Signal
{
    std::string name, pair;
    bool pad = false, reg = false, input_pair = false, output = false;
    unsigned pin = 0;
    std::array<std::vector<std::string>, 2> fanins;
    std::array<int, 8> delays{{-1, -1, -1, -1, -1, -1, -1, -1}};
};
std::string strip_signal_phase_suffix(std::string s)
{
    while (!s.empty() && (s.back() == '-' || s.back() == '%'))
        s.pop_back();
    return s;
}
void record_delay_bounds(Signal& s, unsigned kind, int value)
{
    auto& lo = s.delays[kind * 2];
    auto& hi = s.delays[kind * 2 + 1];
    if (lo < 0 || value < lo)
        lo = value;
    hi = std::max(hi, value);
}
const char* key = R"(Timing Analysis KEY:
One unit of delay time is equivalent to one pass 
     through the Central Switch Matrix.
..   Delay ( in this column ) not applicable to the indicated signal.
TSU, Set-Up Time ( 0 for input-paired signals ),
     represents the number of switch matrix passes between
     an input pin and a register setup before clock.
     TSU is reported on the register.
TCO, Clocked Output-to-Pin Time ( 0 for output-paired signals ),
     represents the number of switch matrix passes between
     a clocked register and an output pin.
     TCO is reported on the register.
TPD, Propagation Delay Time ( calculated only for combinatorial eqns.),
     represents the number of switch matrix passes between
     an input pin and an output pin.
     TPD is reported on the output pin.
TCR, Clocked Output-to-Register Time,
     represents the number of switch matrix passes between
     a clocked register and the register it drives ( before clock ).
     TCR is reported on the driving register.
)";
}  // namespace
std::string render_timing_report(const Design& design, const std::string& pla, const FitterReportData* fitted,
                                 const std::string& source_filename)
{
    std::map<std::string, Signal> signals;
    std::vector<std::string> declaration_order, inputs, outputs;
    std::vector<std::vector<std::string>> terms;
    Design declarations;
    std::istringstream text(pla);
    std::string line;
    while (std::getline(text, line)) {
        std::istringstream words(line);
        std::string first;
        words >> first;
        if (first.empty())
            continue;
        if (first == "#AMD") {
            std::string kind;
            unsigned count;
            words >> kind;
            if (kind != "PIN" && kind != "NODE")
                continue;
            words >> count;
            std::string field;
            while (words >> field) {
                auto colon = field.find(':');
                if (colon == std::string::npos)
                    continue;
                auto name = strip_signal_phase_suffix(field.substr(0, colon));
                if (name == "GND" || name == "VCC")
                    continue;
                auto slash = field.find('/', colon);
                Signal s;
                s.name = name;
                s.pad  = kind == "PIN";
                s.pin  = unsigned(std::stoul(field.substr(colon + 1)));
                if (slash != std::string::npos)
                    s.pair = strip_signal_phase_suffix(field.substr(slash + 1));
                if (!signals.count(name))
                    declaration_order.push_back(name);
                signals[name] = s;
                Pin pin;
                pin.name = name;
                pin.node = !s.pad;
                pin.pair = s.pair;
                declarations.pins.push_back(pin);
            }
            continue;
        }
    }
    {
        auto parsed = parse_pla_text(pla);
        inputs      = std::move(parsed.inputs);
        outputs     = std::move(parsed.outputs);
        terms       = std::move(parsed.terms);
    }
    discard_combinatorial_storage_controls(outputs, terms);
    retain_reachable_equations(declarations, inputs, outputs, terms);
    std::set<std::string> primary, used, enabled;
    std::map<std::string, std::array<std::set<std::string>, 2>> dependencies;
    for (unsigned col = 0; col < outputs.size(); ++col) {
        auto label = strip_signal_phase_suffix(outputs[col]);
        auto dot   = label.find('.');
        auto name = label.substr(0, dot), fn = dot == std::string::npos ? "COM" : label.substr(dot + 1);
        if (fn == "OE")
            enabled.insert(name);
        const bool data = fn == "COM" || fn == "REG" || fn == "T" || fn == "G" || fn.compare(0, 3, "XOR") == 0;
        if (!data)
            continue;
        primary.insert(name);
        auto& signal = signals[name];
        signal.name  = name;
        if (fn == "REG" || fn == "T" || fn == "G" || fn.find("_REG") != std::string::npos ||
            fn.find("_T") != std::string::npos || fn.find("_G") != std::string::npos)
            signal.reg = true;
        for (const auto& term : terms[col])
            for (unsigned bit = 0; bit < inputs.size(); ++bit)
                if (term[bit] != '-')
                    dependencies[name][fn.compare(0, 4, "XORL") == 0 ? 0 : 1].insert(inputs[bit]);
    }
    for (const auto& cover : terms)
        for (const auto& term : cover)
            for (unsigned bit = 0; bit < inputs.size(); ++bit)
                if (term[bit] != '-')
                    used.insert(inputs[bit]);
    for (const auto& item : signals) {
        const auto& s = item.second;
        if (!s.pad && !s.pair.empty() && signals.count(s.pair) && signals.at(s.pair).pad && !used.count(s.name))
            primary.erase(s.name);
    }
    for (auto& item : signals) {
        auto& s  = item.second;
        s.output = s.pad && primary.count(s.name);
        if (!s.pair.empty() && signals.count(s.pair)) {
            auto& partner = signals.at(s.pair);
            if (s.pad && !primary.count(s.name))
                partner.input_pair = true;
            if (!s.pad && primary.count(s.pair))
                s.output = true;
        }
    }
    // Physical input-register allocation can reject an otherwise valid
    // automatic pair (for example the fifth clock domain in a block).
    if (fitted && fitted->successful) {
        auto aliases = pla_signal_aliases(design);
        for (const auto& node : fitted->nodes) {
            const auto name = aliases.count(node.name) ? aliases.at(node.name) : node.name;
            if (signals.count(name))
                signals[name].input_pair = node.input_register;
            if (node.input_register)
                for (const auto& pin : fitted->pins)
                    if (int(pin.number) == node.pin) {
                        auto pad = aliases.count(pin.name) ? aliases.at(pin.name) : pin.name;
                        if (signals.count(pad))
                            signals[pad].pair = name;
                    }
        }
    }
    std::map<std::string, std::string> feedback, partner;
    std::vector<std::string> order;
    for (const auto& name : declaration_order) {
        auto& s = signals.at(name);
        if (!primary.count(name)) {
            if (s.pad && !s.pair.empty() && primary.count(s.pair))
                order.push_back(s.pair);
            continue;
        }
        if (std::find(order.begin(), order.end(), name) != order.end())
            continue;
        if (!s.pad && !s.pair.empty() && signals.count(s.pair) && signals.at(s.pair).output)
            continue;
        order.push_back(name);
        std::string alias;
        if (s.pad) {
            for (const auto& p : signals)
                if (!p.second.pad && p.second.pair == name && primary.count(p.first)) {
                    alias = p.first;
                    break;
                }
            if (alias.empty() && s.reg && used.count(name)) {
                alias     = "RN_" + name;
                auto copy = s;
                copy.name = alias;
                copy.pad  = false;
                signals.emplace(alias, std::move(copy));
                dependencies[alias] = dependencies[name];
                primary.insert(alias);
            }
            if (!alias.empty()) {
                order.push_back(alias);
                partner[name]  = alias;
                partner[alias] = name;
                feedback[name] = alias;
            }
        }
    }
    for (const auto& name : primary)
        if (std::find(order.begin(), order.end(), name) == order.end())
            order.push_back(name);
    for (auto& item : signals) {
        auto& s = item.second;
        for (unsigned leg = 0; leg < 2; ++leg)
            for (const auto& input : dependencies[s.name][leg])
                s.fanins[leg].push_back(feedback.count(input) ? feedback.at(input) : input);
        if (s.reg && s.output)
            record_delay_bounds(s, 1, 0);
        if (s.input_pair)
            record_delay_bounds(s, 0, 0);
    }
    bool indeterminate = false;
    for (const auto& root : order) {
        unsigned visits = 0;
        std::set<std::string> stack{root};
        std::function<void(const std::string&, int)> walk = [&](const std::string& name, int depth) {
            auto& current = signals.at(name);
            auto& target  = signals.at(root);
            // MACHFITR walks the left and right covers independently;
            // ordinary equations occupy the right cover only.
            for (unsigned leg = 0; leg < 2; ++leg)
                for (const auto& input : current.fanins[leg]) {
                    if (++visits > current.fanins[leg].size() * 1000) {
                        indeterminate = true;
                        continue;
                    }
                    if (stack.count(input))
                        continue;
                    const auto found = signals.find(input);
                    const auto paired =
                        found != signals.end() && found->second.pad ? signals.find(found->second.pair) : signals.end();
                    const bool input_pad = paired != signals.end() && paired->second.input_pair;
                    const bool reg       = found != signals.end() && (found->second.reg || input_pad);
                    const bool comb      = primary.count(input) && !reg;
                    if (comb) {
                        stack.insert(input);
                        walk(input, depth + 1);
                        stack.erase(input);
                    }
                    if (input == root)
                        continue;
                    const bool raw = !input_pad && (!primary.count(input) || enabled.count(input));
                    if (raw && !target.reg)
                        record_delay_bounds(target, 2, depth);
                    else if (raw && target.reg && !target.input_pair)
                        record_delay_bounds(target, 0, depth);
                    else if (reg && ((target.output && !target.reg) || target.reg)) {
                        const unsigned kind = target.reg ? 3 : 1;
                        record_delay_bounds(found->second, kind, depth);
                        if (partner.count(input))
                            record_delay_bounds(signals.at(partner.at(input)), kind, depth);
                    }
                }
        };
        if (!indeterminate)
            walk(root, 1);
    }
    auto filename    = source_filename.substr(source_filename.find_last_of("/\\") + 1);
    const auto title = "Design Name = " + filename;
    std::ostringstream out;
    out << '\n'
        << std::string(title.size() + 4, '*') << "\n* " << title << " *\n"
        << std::string(title.size() + 4, '*')
        << "\n\n\n*******************\n* TIMING ANALYSIS *\n*******************\n\n"
        << key;
    if (indeterminate)
        out << "\nDesign contains recursive combinatorial paths of indeterminate timing.\n\n";
    else {
        out << "\n                    TSU       TCO       TPD       TCR\n                  #passes   #passes   #passes "
               "  #passes\nSIGNAL NAME       min  max  min  max  min  max  min  max";
        bool any = false;
        for (int maximum = 50; maximum > 0; --maximum)
            for (const auto& name : order) {
                const auto& s = signals.at(name);
                if (*std::max_element(s.delays.begin(), s.delays.end()) != maximum)
                    continue;
                any = true;
                out << '\n' << std::setw(16) << name;
                for (int value : s.delays)
                    out << std::setw(4) << (value < 0 ? ".." : std::to_string(value)) << ' ';
                out << "  ";
            }
        if (!any)
            out << "\n*** NO DELAYS ****";
    }
    std::string result;
    for (char c : out.str()) {
        if (c == '\n')
            result += '\r';
        result += c;
    }
    return result;
}
}  // namespace machxl
