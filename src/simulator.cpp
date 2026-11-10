#include "simulator.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace machxl {
namespace {
using Value = SimulationValue;
[[noreturn]] void error(const std::string& message)
{
    throw std::invalid_argument("Simulation: " + message);
}
Value invert_value(Value v)
{
    return v == Value::Low ? Value::High : v == Value::High ? Value::Low : Value::Unknown;
}
bool is_known_value(Value v)
{
    return v == Value::Low || v == Value::High;
}
Value evaluate_binary_value(ExprKind kind, Value a, Value b)
{
    if (kind == ExprKind::And) {
        if (a == Value::Low || b == Value::Low)
            return Value::Low;
        return a == Value::High && b == Value::High ? Value::High : Value::Unknown;
    }
    if (kind == ExprKind::Or) {
        if (a == Value::High || b == Value::High)
            return Value::High;
        return a == Value::Low && b == Value::Low ? Value::Low : Value::Unknown;
    }
    if (!is_known_value(a) || !is_known_value(b))
        return Value::Unknown;
    bool value = (a != b) != (kind == ExprKind::Xnor);
    return value ? Value::High : Value::Low;
}
std::vector<std::string> tokenize_simulation(const std::string& source)
{
    std::vector<std::string> out;
    for (std::size_t i = 0; i < source.size();) {
        if (std::isspace(static_cast<unsigned char>(source[i]))) {
            ++i;
            continue;
        }
        if (source[i] == ';') {
            while (i < source.size() && source[i] != '\n')
                ++i;
            continue;
        }
        if (static_cast<unsigned char>(source[i]) == 26)
            break;
        bool found = false;
        for (const auto& op : {":+:", ":*:", ":=", "..", "<=", ">=", "<>"}) {
            std::string value = op;
            if (source.compare(i, value.size(), value) == 0) {
                out.push_back(value);
                i += value.size();
                found = true;
                break;
            }
        }
        if (found)
            continue;
        if (std::isalnum(static_cast<unsigned char>(source[i])) || source[i] == '_' || source[i] == '#') {
            std::string word;
            while (i < source.size() &&
                   (std::isalnum(static_cast<unsigned char>(source[i])) || source[i] == '_' || source[i] == '#'))
                word += static_cast<char>(std::toupper(static_cast<unsigned char>(source[i++])));
            if (i < source.size() && source[i] == '[') {
                do {
                    if (!std::isspace(static_cast<unsigned char>(source[i])))
                        word += static_cast<char>(std::toupper(static_cast<unsigned char>(source[i])));
                    ++i;
                } while (i < source.size() && source[i - 1] != ']');
                if (word.back() != ']')
                    error("unclosed signal vector");
            }
            out.push_back(word);
        } else
            out.emplace_back(1, source[i++]);
    }
    out.emplace_back();
    return out;
}
const std::set<std::string> commands = {"SETF",    "CLOCKF", "CHECK", "CHECKQ", "TESTF", "TRACE_ON", "TRACE_OFF",
                                        "PRELOAD", "FOR",    "IF",    "WHILE",  "END",   "ELSE"};
struct Command
{
    std::string op, variable, first, last;
    std::vector<std::string> arguments;
    std::vector<Command> body, otherwise;
};
class Parser
{
    std::vector<std::string> words;
    std::size_t at = 0;
    std::string pop()
    {
        auto v = words.at(at++);
        if (v.empty())
            error("unexpected end of commands");
        return v;
    }
    void expect(const std::string& s)
    {
        if (pop() != s)
            error("expected " + s);
    }
    std::vector<Command> block()
    {
        expect("BEGIN");
        auto result = sequence(true);
        expect("END");
        return result;
    }
    std::vector<Command> sequence(bool nested)
    {
        std::vector<Command> result;
        while (!words.at(at).empty() && words.at(at) != "END") {
            Command c;
            c.op = pop();
            if (c.op == "FOR") {
                c.variable = pop();
                expect(":=");
                c.first = pop();
                expect("TO");
                c.last = pop();
                expect("DO");
                c.body = block();
            } else if (c.op == "IF" || c.op == "WHILE") {
                auto stop = c.op == "IF" ? "THEN" : "DO";
                while (words.at(at) != stop)
                    c.arguments.push_back(pop());
                expect(stop);
                c.body = block();
                if (c.op == "IF" && words.at(at) == "ELSE") {
                    ++at;
                    c.otherwise = block();
                }
            } else if (c.op == "TRACE_OFF") {
            } else if (c.op == "SETF" || c.op == "CLOCKF" || c.op == "CHECK" || c.op == "CHECKQ" || c.op == "TESTF" ||
                       c.op == "TRACE_ON" || c.op == "PRELOAD") {
                while (!words.at(at).empty() && !commands.count(words.at(at)))
                    c.arguments.push_back(pop());
            } else
                error("unsupported command: " + c.op);
            // TESTF falls back to CHECKQ on MACH devices.
            if (c.op == "TESTF")
                c.op = "CHECKQ";
            result.push_back(std::move(c));
        }
        if (!nested && !words.at(at).empty())
            error("unmatched END");
        return result;
    }

   public:
    explicit Parser(const std::string& source) : words(tokenize_simulation(source)) {}
    std::vector<Command> parse() { return sequence(false); }
};
struct Literal
{
    std::string name;
    Value value;
};
class Simulator
{
    const Design& design;
    SimulationOptions options;
    SimulationResult result;
    std::map<std::string, const Pin*> pins;
    std::map<std::string, const Equation*> equations, primary;
    std::vector<Equation> fitted_combinatorial;
    std::map<std::string, Value> inputs, registers, values, clocks;
    std::map<std::string, long long> variables;
    std::map<std::string, std::vector<std::string>> groups;
    std::set<std::string> clock_inputs, physical_feedback;
    std::size_t command_count  = 0;
    int active_trace           = -1;
    unsigned default_clock_pin = 0;
    std::string default_clock_name;
    bool default_clock_inverted = false;
    std::map<unsigned, std::string> physical_inputs;
    std::map<unsigned, bool> physical_inverted;
    long long number(const std::string& text) const
    {
        auto var = variables.find(text);
        if (var != variables.end())
            return var->second;
        auto body = text;
        int radix = 10;
        if (body.size() > 2 && body[0] == '#') {
            switch (body[1]) {
            case 'B':
                radix = 2;
                break;
            case 'O':
                radix = 8;
                break;
            case 'D':
                break;
            case 'H':
                radix = 16;
                break;
            default:
                error("unknown radix");
            }
            body.erase(0, 2);
        }
        std::size_t end = 0;
        long long value = 0;
        try {
            value = std::stoll(body, &end, radix);
        } catch (const std::exception&) {
            error("invalid number: " + text);
        }
        if (end != body.size() || value < 0)
            error("invalid unsigned number: " + text);
        return value;
    }
    std::vector<std::string> expand(const std::string& name) const
    {
        auto group = groups.find(name);
        if (group != groups.end())
            return group->second;
        auto open = name.find('[');
        if (open == std::string::npos)
            return {name};
        if (name.back() != ']')
            error("invalid vector: " + name);
        std::vector<std::string> result;
        auto body = name.substr(open + 1, name.size() - open - 2);
        std::stringstream list(body);
        std::string part;
        while (std::getline(list, part, ',')) {
            auto range = part.find("..");
            auto first = number(part.substr(0, range));
            auto last  = range == std::string::npos ? first : number(part.substr(range + 2));
            if (first > 65535 || last > 65535 || std::max(first, last) - std::min(first, last) > 4096)
                error("vector range exceeds limit");
            for (auto index = first;; index += first <= last ? 1 : -1) {
                result.push_back(name.substr(0, open) + "[" + std::to_string(index) + "]");
                if (index == last)
                    break;
            }
        }
        return result;
    }
    Value evaluate(const ExprPtr& expression) const
    {
        if (!expression)
            error("missing expression");
        if (expression->kind == ExprKind::Constant)
            return expression->value ? Value::High : Value::Low;
        if (expression->kind == ExprKind::Identifier) {
            auto found = values.find(expression->name);
            if (found != values.end())
                return physical_feedback.count(expression->name) ? signal(expression->name) : found->second;
            auto alias = design.simulation_symbols.find(expression->name);
            if (alias != design.simulation_symbols.end())
                return evaluate(alias->second);
            error("unknown signal: " + expression->name);
        }
        if (expression->kind == ExprKind::Not)
            return invert_value(evaluate(expression->left));
        return evaluate_binary_value(expression->kind, evaluate(expression->left), evaluate(expression->right));
    }
    const SimulationRegisterModel* fitted_register(const std::string& name) const
    {
        if (!options.fitted_model)
            return nullptr;
        auto found = options.fitted_model->registers.find(name);
        return found == options.fitted_model->registers.end() ? nullptr : &found->second;
    }
    Value equation(const Equation& e) const
    {
        if (options.fitted_model && e.function == "COM") {
            auto found = options.fitted_model->combinatorial.find(e.signal);
            if (found != options.fitted_model->combinatorial.end())
                return evaluate(found->second);
        }
        if (e.function == "REG" || e.function == "T" || e.function == "G") {
            auto model = fitted_register(e.signal);
            if (model && model->next_state)
                return evaluate(model->next_state);
        }
        auto v = evaluate(e.expression);
        return e.inverted ? invert_value(v) : v;
    }
    Value control(const std::string& name, const std::string& function, Value fallback) const
    {
        if (auto model = fitted_register(name)) {
            auto expression = function == "C"    ? model->clock
                              : function == "RE" ? model->reset
                              : function == "PR" ? model->preset
                                                 : ExprPtr{};
            if (expression)
                return evaluate(expression);
        }
        auto found = equations.find(name + "." + function);
        if (found != equations.end())
            return equation(*found->second);
        if (function == "C" && !default_clock_name.empty()) {
            auto value = values.at(default_clock_name);
            if (default_clock_inverted)
                value = invert_value(value);
            auto storage = primary.find(name);
            if (storage != primary.end() && storage->second->function == "G" &&
                design.device.compare(0, 5, "MACH2") == 0 && design.device != "MACH215")
                value = invert_value(value);
            return value;
        }
        return fallback;
    }
    Value signal(const std::string& name, bool q = false) const
    {
        auto found = values.find(name);
        if (found == values.end()) {
            auto alias = design.simulation_symbols.find(name);
            if (alias != design.simulation_symbols.end())
                return evaluate(alias->second);
            error("unknown signal: " + name);
        }
        if (!q && primary.count(name) && control(name, "OE", Value::High) != Value::High) {
            auto driven = inputs.find(name);
            if (driven != inputs.end())
                return driven->second;
            return control(name, "OE", Value::High) == Value::Low ? Value::HighImpedance : Value::Unknown;
        }
        if (q && primary.count(name) && primary.at(name)->function != "COM" &&
            (design.device.compare(0, 5, "MACH1") == 0 || design.device.compare(0, 5, "MACH2") == 0)) {
            // CHECKQ observes the legacy flip-flop before its output XOR.
            // Legacy physical storage powers up low; the logical fitted
            // power-up value therefore records this observation's phase.
            bool phase = primary.at(name)->inverted;
            if (options.fitted_model) {
                auto model = options.fitted_model->registers.find(name);
                if (model != options.fitted_model->registers.end() && model->second.power_up)
                    phase = *model->second.power_up == Value::High;
            }
            return phase ? invert_value(found->second) : found->second;
        }
        return found->second;
    }
    void propagate(bool freeze_registers = false)
    {
        std::map<std::string, Value> previous_data;
        for (const auto& entry : primary)
            if (entry.second->function != "COM")
                previous_data[entry.first] = equation(*entry.second);
        for (std::size_t step = 0; step < options.max_propagation_steps; ++step) {
            auto next = values;
            for (const auto& input : inputs)
                if (!primary.count(input.first))
                    next[input.first] = input.second;
            for (const auto& entry : primary) {
                const auto& name = entry.first;
                const auto& e    = *entry.second;
                if (e.function == "COM")
                    next[name] = equation(e);
                else {
                    Value value = registers.at(name);
                    if (freeze_registers) {
                        next[name] = value;
                        continue;
                    }
                    auto reset = control(name, "RE", Value::Low), preset = control(name, "PR", Value::Low);
                    auto model     = fitted_register(name);
                    auto requested = design.requested_power_up.find(name);
                    const bool inverse =
                        (requested == design.requested_power_up.end() ? e.inverted : requested->second) &&
                        (options.legacy_set_reset || (design.device[4] != '3' && design.device[4] != '4'));
                    const auto reset_value  = model && model->reset_value ? *model->reset_value
                                              : inverse                   ? Value::High
                                                                          : Value::Low;
                    const auto preset_value = model && model->preset_value ? *model->preset_value
                                              : inverse                    ? Value::Low
                                                                           : Value::High;
                    auto clock              = control(name, "C", Value::Unknown);
                    if (e.function == "G" && clock == Value::High)
                        value = equation(e);
                    else if (reset == Value::High)
                        value = reset_value;
                    else if (preset == Value::High)
                        value = preset_value;
                    else if (reset != Value::Low || preset != Value::Low)
                        value = Value::Unknown;
                    else if (e.function != "G" && clock == Value::High && clocks[name] == Value::Low) {
                        auto data = previous_data[name];
                        value     = e.function == "T" && !(model && model->next_state)
                                        ? evaluate_binary_value(ExprKind::Xor, value, data)
                                        : data;
                    }
                    previous_data[name] = equation(e);
                    registers[name]     = value;
                    next[name]          = value;
                    clocks[name]        = clock;
                }
            }
            if (next == values)
                return;
            values = std::move(next);
        }
        error("logic did not settle within propagation limit");
    }
    std::vector<Literal> literals(const std::vector<std::string>& args) const
    {
        std::vector<Literal> result;
        for (std::size_t i = 0; i < args.size();) {
            if (args[i] == ",") {
                ++i;
                continue;
            }
            Value value   = Value::High;
            bool negative = false;
            if (args[i] == "/" || args[i] == "^" || args[i] == "%") {
                negative = args[i] == "/";
                value    = args[i] == "/" ? Value::Low : args[i] == "^" ? Value::HighImpedance : Value::Unknown;
                ++i;
            }
            if (i == args.size())
                error("missing signal after value prefix");
            auto names              = expand(args[i++]);
            bool numeric            = i < args.size() && args[i] == "=";
            unsigned long long data = 0;
            if (numeric) {
                ++i;
                if (i == args.size())
                    error("missing vector value");
                data = static_cast<unsigned long long>(number(args[i++]));
                if (names.size() > 63 || (data >> names.size()))
                    error("constant does not fit simulation vector");
            }
            for (std::size_t bit = 0; bit < names.size(); ++bit) {
                auto name            = names[bit];
                bool member_negative = !name.empty() && name[0] == '/';
                if (member_negative)
                    name.erase(0, 1);
                auto v =
                    numeric ? (((data >> (names.size() - bit - 1)) & 1) != negative ? Value::High : Value::Low) : value;
                if (member_negative)
                    v = invert_value(v);
                if (!pins.count(name) && !design.simulation_symbols.count(name))
                    error("undeclared signal: " + name);
                result.push_back({name, v});
            }
        }
        return result;
    }
    long long comparison_number(const std::string& token) const
    {
        // Numeric relation operands are loop indices or unsigned constants,
        // not signals. Compatibility mode uses signed 16-bit values and
        // treats nondecimal radix tokens as zero.
        if (!design.dos_compatibility)
            return number(token);
        auto variable  = variables.find(token);
        unsigned value = 0;
        if (variable != variables.end())
            value = static_cast<unsigned>(variable->second) & 0xffffu;
        else {
            std::string decimal = token;
            if (decimal.compare(0, 2, "#D") == 0)
                decimal.erase(0, 2);
            if (decimal.empty())
                error("invalid simulation comparison operand");
            if (decimal.front() == '#') {
                (void)number(token);  // Validate the radix even in bug compatibility.
                return 0;
            }
            for (unsigned char c : decimal) {
                if (!std::isdigit(c))
                    error("undefined index variable: " + token);
                value = (value * 10u + unsigned(c - '0')) & 0xffffu;
            }
        }
        return value < 0x8000u ? static_cast<long long>(value) : static_cast<long long>(value) - 0x10000;
    }
    bool condition(std::vector<std::string> args) const
    {
        // Parentheses may surround the complete comparison, but cannot turn
        // loop-index comparisons into arbitrary Boolean/arithmetic expressions.
        while (args.size() >= 2 && args.front() == "(" && args.back() == ")") {
            int depth     = 0;
            bool encloses = true;
            for (std::size_t i = 0; i < args.size(); ++i) {
                if (args[i] == "(")
                    ++depth;
                else if (args[i] == ")")
                    --depth;
                if (depth == 0 && i + 1 < args.size()) {
                    encloses = false;
                    break;
                }
            }
            if (!encloses || depth != 0)
                break;
            args.erase(args.begin());
            args.pop_back();
        }
        const std::set<std::string> comparisons = {"=", "<>", "<", "<=", ">", ">="};
        auto comparison =
            std::find_if(args.begin(), args.end(), [&](const auto& token) { return comparisons.count(token) != 0; });
        if (comparison != args.end()) {
            const auto index = static_cast<std::size_t>(comparison - args.begin());
            if (index != 1 || args.size() != 3)
                error("invalid simulation comparison");
            const auto& op = *comparison;
            const bool numeric =
                variables.count(args[0]) ||
                (!args[0].empty() && (std::isdigit(static_cast<unsigned char>(args[0][0])) || args[0][0] == '#'));
            if (op != "=" || numeric) {
                const auto left = comparison_number(args[0]);
                // Compatibility equality treats a named RHS as a decimal constant
                // when the LHS is an index. Other relations resolve both indices.
                const auto right =
                    design.dos_compatibility && op == "=" && variables.count(args[0]) && variables.count(args[2])
                        ? 0
                        : comparison_number(args[2]);
                if (op == "=")
                    return left == right;
                if (op == "<>")
                    return left != right;
                if (op == "<")
                    return left < right;
                if (op == "<=")
                    return left <= right;
                if (op == ">")
                    return left > right;
                return left >= right;
            }
            const auto expected_values = literals(args);
            // Original GROUP equality behaves like an empty comparison and
            // always succeeds; vector-name equality compares its members.
            // Normal mode also compares the actual GROUP members.
            if (design.dos_compatibility && groups.count(args[0]))
                return true;
            for (const auto& expected : expected_values) {
                auto actual = signal(expected.name);
                if (!is_known_value(actual))
                    error("unknown value in simulation condition");
                if (actual != expected.value)
                    return false;
            }
            return true;
        }
        std::string text;
        for (const auto& a : args)
            text += a;
        auto value = evaluate(parse_expression(text));
        if (!is_known_value(value))
            error("unknown value in simulation condition");
        return value == Value::High;
    }
    Value observed_signal(const std::string& name, bool q = false) const
    {
        auto value        = signal(name, q);
        const auto pin    = pins.find(name);
        const auto output = primary.find(name);
        const auto model  = fitted_register(name);
        if (pin != pins.end() && pin->second->node && output != primary.end() && !(model && model->input_register)) {
            if (output->second->inverted != pin->second->inverted)
                value = invert_value(value);
        }
        return value;
    }
    void report_snapshot(char command = ' ')
    {
        if (!options.collect_reports)
            return;
        if (result.report_events.size() >= options.max_vectors * 4)
            error("report event limit exceeded");
        SimulationReportEvent event;
        event.command = command;
        event.trace   = active_trace;
        for (const auto& p : design.pins) {
            auto value = observed_signal(p.name);
            // CLOCKF's first report sample precedes register propagation.
            if (!primary.count(p.name) && inputs.count(p.name))
                value = inputs.at(p.name);
            if (!primary.count(p.name) && !inputs.count(p.name) && options.placement &&
                std::find(options.placement->signals.begin(), options.placement->signals.end(), p.name) ==
                    options.placement->signals.end() &&
                std::find(options.placement->nodes.begin(), options.placement->nodes.end(), p.name) ==
                    options.placement->nodes.end())
                value = Value::Unknown;
            event.state.signals[p.name] = {value, primary.count(p.name) != 0, 0};
        }
        for (const auto& symbol : design.simulation_symbols)
            event.state.signals[symbol.first] = {signal(symbol.first), false, 0};
        for (const auto& physical : physical_inputs) {
            auto value = inputs.count(physical.second) ? inputs.at(physical.second) : Value::Low;
            if (physical_inverted.at(physical.first))
                value = invert_value(value);
            event.state.physical_inputs[physical.first] = {value, false, 0};
        }
        result.report_events.push_back(std::move(event));
    }
    void snapshot(const std::map<std::string, int>& pulses = {})
    {
        if (result.vectors_disabled_by_preload)
            return;
        if (result.vectors.size() >= options.max_vectors)
            error("vector limit exceeded");
        SimulationVector vector;
        for (const auto& p : design.pins) {
            bool output = primary.count(p.name) && control(p.name, "OE", Value::High) != Value::Low;
            auto pulse  = pulses.find(p.name);
            auto value  = signal(p.name);
            if (!primary.count(p.name) && !inputs.count(p.name))
                value = Value::Unknown;
            vector.signals.emplace(p.name, SimulationSignal{value, output, pulse == pulses.end() ? 0 : pulse->second});
        }
        for (const auto& entry : physical_inputs) {
            const auto pulse = pulses.find(entry.second);
            auto value       = inputs.count(entry.second) ? inputs.at(entry.second) : Value::Unknown;
            int code         = pulse == pulses.end() ? 0 : pulse->second;
            if (physical_inverted.at(entry.first)) {
                value = invert_value(value);
                code  = -code;
            }
            vector.physical_inputs[entry.first] = {value, false, code};
        }
        result.vectors.push_back(std::move(vector));
    }
    void execute(const std::vector<Command>& program)
    {
        for (const auto& command : program) {
            if (++command_count > options.max_commands)
                error("command limit exceeded");
            const auto& op = command.op;
            if (op == "FOR") {
                auto first = number(command.first), last = number(command.last);
                if (last < first)
                    error("FOR start value must not exceed its end value");
                auto old       = variables.find(command.variable);
                bool saved     = old != variables.end();
                auto old_value = saved ? old->second : 0;
                for (auto i = first; i <= last; ++i) {
                    if (++command_count > options.max_commands)
                        error("command limit exceeded");
                    variables[command.variable] = i;
                    execute(command.body);
                    if (i == std::numeric_limits<long long>::max())
                        break;
                }
                if (saved)
                    variables[command.variable] = old_value;
                else
                    variables.erase(command.variable);
            } else if (op == "IF")
                execute(condition(command.arguments) ? command.body : command.otherwise);
            else if (op == "WHILE") {
                while (condition(command.arguments)) {
                    if (++command_count > options.max_commands)
                        error("command limit exceeded");
                    execute(command.body);
                }
            } else if (op == "TRACE_OFF") {
                active_trace = -1;
            } else if (op == "TRACE_ON") {
                std::vector<SimulationTraceSignal> trace;
                for (const auto& literal : literals(command.arguments))
                    trace.push_back({literal.name, literal.value == Value::Low});
                if (options.collect_reports) {
                    active_trace = int(result.report_traces.size());
                    result.report_traces.push_back(std::move(trace));
                }
            } else if (op == "SETF" || op == "PRELOAD") {
                if (op == "PRELOAD")
                    result.vectors_disabled_by_preload = true;
                std::map<std::string, int> edges;
                for (const auto& literal : literals(command.arguments)) {
                    if (op == "PRELOAD") {
                        auto alias = design.simulation_symbols.find(literal.name);
                        if (alias != design.simulation_symbols.end()) {
                            auto cubes =
                                sum_of_products(literal.value == Value::High ? alias->second : make_not(alias->second));
                            if (cubes.size() != 1)
                                error("PRELOAD requires an unambiguous state encoding");
                            for (const auto& bit : cubes[0]) {
                                if (!registers.count(bit.first))
                                    error("PRELOAD state contains a nonregister: " + bit.first);
                                registers[bit.first] = values[bit.first] = bit.second ? Value::High : Value::Low;
                            }
                        } else {
                            if (!registers.count(literal.name))
                                error("PRELOAD requires a register: " + literal.name);
                            registers[literal.name] = literal.value;
                            values[literal.name]    = literal.value;
                        }
                    } else {
                        if (!is_known_value(literal.value))
                            error("SETF requires low or high values");
                        if (pins.at(literal.name)->node)
                            error("SETF cannot drive a buried node");
                        if (clock_inputs.count(literal.name) && literal.value != values.at(literal.name))
                            edges[literal.name] = literal.value == Value::High ? 2 : -2;
                        inputs[literal.name] = literal.value;
                    }
                }
                if (op == "PRELOAD")
                    propagate(true);
                propagate();
                snapshot(edges);
                report_snapshot(op == "PRELOAD" ? 'p' : 'g');
            } else if (op == "CHECK" || op == "CHECKQ") {
                bool report_clash = false;
                for (const auto& expected : literals(command.arguments)) {
                    if (!primary.count(expected.name) && !design.simulation_symbols.count(expected.name))
                        error("CHECK requires an output: " + expected.name);
                    auto actual = observed_signal(expected.name, op == "CHECKQ");
                    if (actual != expected.value) {
                        result.mismatches.push_back({result.vectors.size(), expected.name, expected.value, actual});
                        if (options.collect_reports) {
                            if (!report_clash) {
                                report_snapshot();
                                report_clash = true;
                            }
                            result.report_events.back().clashes.push_back(expected.name);
                            result.report_clashes.push_back({result.report_events.size() - 1, expected.name,
                                                             expected.value, actual, op == "CHECKQ"});
                        }
                    }
                }
            } else if (op == "CLOCKF") {
                auto clocks_to_pulse = literals(command.arguments);
                if (clocks_to_pulse.empty()) {
                    physical_inputs[default_clock_pin]   = default_clock_name;
                    physical_inverted[default_clock_pin] = default_clock_inverted;
                    clocks_to_pulse.push_back({default_clock_name, default_clock_inverted ? Value::Low : Value::High});
                }
                std::map<std::string, int> pulses;
                for (auto& clock : clocks_to_pulse) {
                    if ((pins.count(clock.name) && pins.at(clock.name)->node) || primary.count(clock.name))
                        error("CLOCKF requires an input pin");
                    if (!is_known_value(clock.value))
                        error("CLOCKF requires a known polarity");
                    // Negative physical clock pulses are supported only on MACH215 and
                    // MACH3/4 devices. Validate the requested polarity before falling
                    // back to a positive pulse on other devices.
                    const bool negative =
                        pins.count(clock.name) ? pins.at(clock.name)->inverted : default_clock_inverted;
                    const bool k_vectors = design.device == "MACH215" || design.device.compare(0, 5, "MACH3") == 0 ||
                                           design.device.compare(0, 5, "MACH4") == 0;
                    const auto requested = negative ? invert_value(clock.value) : clock.value;
                    auto current         = values.at(clock.name);
                    if (negative)
                        current = invert_value(current);
                    if (requested == Value::High && current == Value::High)
                        error("D3230: clock pin is high when CLOCKF is asserted: " + clock.name);
                    if (k_vectors && requested == Value::Low && current == Value::Low)
                        error("D3240: clock pin is low when negative CLOCKF is asserted: " + clock.name);
                    const auto physical = k_vectors ? requested : Value::High;
                    clock.value         = negative ? invert_value(physical) : physical;
                    pulses[clock.name]  = clock.value == Value::High ? 1 : -1;
                    inputs[clock.name]  = clock.value;
                }
                report_snapshot('c');
                propagate();
                report_snapshot();
                for (const auto& clock : clocks_to_pulse)
                    inputs[clock.name] = invert_value(clock.value);
                propagate();
                snapshot(pulses);
                report_snapshot();
            }
        }
    }

   public:
    Simulator(const Design& d, SimulationOptions o) : design(d), options(o)
    {
        result.device = design.device;
        for (const auto& p : design.pins) {
            pins[p.name]   = &p;
            values[p.name] = Value::Unknown;
        }
        // Fixed source inputs remain forceable even when synthesis removes
        // every use and the fitter consequently omits their signal metadata.
        if (options.placement)
            for (const auto& pin : design.pins)
                if (!pin.node && pin.number && pin.number < options.placement->signals.size() &&
                    options.placement->signals[pin.number].empty()) {
                    physical_inputs[pin.number]   = pin.name;
                    physical_inverted[pin.number] = pin.inverted;
                }
        // PLDSIM.PDF DEFAULT_CLOCK attributes. A parameterless CLOCKF
        // forces this physical pin even when no register currently uses it.
        static const std::map<std::string, unsigned> default_clocks = {
            {"MACH110", 35}, {"MACH111", 35}, {"MACH120", 50}, {"MACH121", 50}, {"MACH130", 65}, {"MACH131", 65},
            {"MACH210", 35}, {"MACH211", 35}, {"MACH215", 13}, {"MACH220", 50}, {"MACH221", 50}, {"MACH231", 65},
            {"MACH355", 8},  {"MACH435", 20}, {"MACH445", 13}, {"MACH446", 13}, {"MACH465", 187}};
        auto default_pin = default_clocks.find(design.device);
        if (default_pin == default_clocks.end())
            error("unsupported device clock architecture: " + design.device);
        default_clock_pin  = default_pin->second;
        default_clock_name = "$PIN" + std::to_string(default_clock_pin);
        for (const auto& pin : design.pins)
            if (!pin.node) {
                unsigned number = pin.number;
                if (options.placement)
                    for (unsigned i = 1; i < options.placement->signals.size(); ++i)
                        if (options.placement->signals[i] == pin.name) {
                            number = i;
                            break;
                        }
                if (number == default_clock_pin) {
                    default_clock_name     = pin.name;
                    default_clock_inverted = pin.inverted;
                    break;
                }
            }
        if (!values.count(default_clock_name))
            values[default_clock_name] = Value::Low;
        if (options.fitted_model)
            for (const auto& input : options.fitted_model->implicit_inputs) {
                values[input.second]           = Value::Low;
                physical_inputs[input.first]   = input.second;
                physical_inverted[input.first] = false;
            }
        for (const auto& g : design.groups)
            groups.emplace(g.first, g.second);
        for (const auto& e : design.equations) {
            equations[e.signal + "." + e.function] = &e;
            if (e.function == "COM" || e.function == "REG" || e.function == "T" || e.function == "G") {
                if (e.care && (e.care->kind != ExprKind::Constant || !e.care->value) &&
                    !(e.function == "COM" ? options.fitted_model && options.fitted_model->combinatorial.count(e.signal)
                                          : fitted_register(e.signal) && fitted_register(e.signal)->next_state))
                    error("simulation of optimized don't-care covers is not implemented");
                if (primary.count(e.signal))
                    error("multiple primary equations: " + e.signal);
                primary[e.signal] = &e;
                if (e.function != "COM") {
                    auto model = fitted_register(e.signal);
                    if (options.require_fitted_registers &&
                        (!model || !model->power_up || !model->reset_value || !model->preset_value))
                        error("fitted register initialization/control metadata is missing: " + e.signal);
                    auto requested      = design.requested_power_up.find(e.signal);
                    bool initial        = requested == design.requested_power_up.end() ? e.inverted : requested->second;
                    auto configured     = options.register_power_up.find(e.signal);
                    registers[e.signal] = configured == options.register_power_up.end()
                                              ? (initial ? Value::High : Value::Low)
                                              : configured->second;
                    if (model && model->power_up)
                        registers[e.signal] = *model->power_up;
                    clocks[e.signal] = Value::Unknown;
                }
            }
            if (e.function == "C") {
                auto clock = e.expression;
                while (clock->kind == ExprKind::Not)
                    clock = clock->left;
                if (clock->kind == ExprKind::Identifier)
                    clock_inputs.insert(clock->name);
            }
        }
        if (options.fitted_model) {
            fitted_combinatorial.reserve(options.fitted_model->combinatorial.size());
            for (const auto& entry : options.fitted_model->combinatorial)
                if (!primary.count(entry.first)) {
                    Equation generated;
                    generated.signal     = entry.first;
                    generated.function   = "COM";
                    generated.expression = entry.second;
                    fitted_combinatorial.push_back(generated);
                    primary[entry.first] = &fitted_combinatorial.back();
                    values[entry.first]  = Value::Unknown;
                }
        }
        for (const auto& pin : design.pins) {
            if (!primary.count(pin.name))
                values[pin.name] = pin.inverted ? Value::High : Value::Low;
            if (!pin.node && primary.count(pin.name) && primary.at(pin.name)->function == "COM")
                physical_feedback.insert(pin.name);
            if (pin.node && !pin.pair.empty() && pin.pair_kind != "IPAIR")
                physical_feedback.insert(pin.pair);
        }
        for (const auto& entry : registers)
            if (!equations.count(entry.first + ".C"))
                clock_inputs.insert(default_clock_name);
        propagate(true);
        for (const auto& entry : registers)
            clocks[entry.first] = control(entry.first, "C", Value::Unknown);
        propagate();
    }
    SimulationResult run()
    {
        execute(Parser(design.simulation).parse());
        return std::move(result);
    }
};
}  // namespace
SimulationResult simulate_design(const Design& design, SimulationOptions options)
{
    return Simulator(design, options).run();
}
std::vector<std::string> encode_simulation_vectors(const SimulationResult& simulation,
                                                   const SimulationPinMap& placement)
{
    if (placement.signals.empty() || placement.signals.size() != placement.inverted.size())
        error("invalid fitted pin map");
    // Original PLDSIM.PDF JTAG_HIGH/JTAG_LOW device attributes. These
    // are programmer test forces, independent of user logic placement.
    const std::map<std::string, std::map<unsigned, char>> reserved = {
        {"MACH355", {{31, '1'}, {77, '1'}, {5, '0'}, {32, '0'}, {103, '0'}}},
        {"MACH445", {{27, '1'}, {53, '1'}, {3, '0'}, {28, '0'}, {77, '0'}}},
        {"MACH446", {{27, '1'}, {53, '1'}, {3, '0'}, {28, '0'}, {77, '0'}}},
        {"MACH465", {{50, '1'}, {106, '1'}, {2, '0'}, {51, '0'}, {154, '0'}}}};
    auto defaults = reserved.find(simulation.device);
    std::vector<std::string> result;
    for (const auto& vector : simulation.vectors) {
        std::string text;
        for (std::size_t pin = 1; pin < placement.signals.size(); ++pin) {
            if (defaults != reserved.end() && defaults->second.count(pin)) {
                text += defaults->second.at(pin);
                continue;
            }
            auto physical = vector.physical_inputs.find(pin);
            if (physical != vector.physical_inputs.end()) {
                const auto& signal = physical->second;
                if (signal.clock_pulse)
                    text += std::abs(signal.clock_pulse) == 2 ? (signal.clock_pulse > 0 ? 'U' : 'D')
                                                              : (signal.clock_pulse > 0 ? 'C' : 'K');
                else
                    text += signal.value == Value::High ? '1' : signal.value == Value::Low ? '0' : 'X';
                continue;
            }
            const auto& name = placement.signals[pin];
            if (name == "GND" || name == "VCC") {
                text += 'N';
                continue;
            }
            if (name.empty()) {
                text += 'X';
                continue;
            }
            auto found = vector.signals.find(name);
            if (found == vector.signals.end())
                error("fitted signal absent from simulation: " + name);
            auto signal = found->second;
            if (signal.clock_pulse) {
                bool rising = signal.clock_pulse > 0;
                if (placement.inverted[pin])
                    rising = !rising;
                text += std::abs(signal.clock_pulse) == 2 ? (rising ? 'U' : 'D') : (rising ? 'C' : 'K');
                continue;
            }
            if (placement.inverted[pin] && is_known_value(signal.value))
                signal.value = invert_value(signal.value);
            if (signal.value == Value::Unknown)
                text += 'X';
            else if (signal.value == Value::HighImpedance)
                text += 'Z';
            else if (signal.output)
                text += signal.value == Value::High ? 'H' : 'L';
            else
                text += signal.value == Value::High ? '1' : '0';
        }
        result.push_back(std::move(text));
    }
    return result;
}
}  // namespace machxl
