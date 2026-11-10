#include "simulation_reports.hpp"
#include "banner.hpp"
#include <algorithm>
#include <iomanip>
#include <set>
#include <sstream>

namespace machxl {
namespace {
constexpr const char* nl = "\r\n";
char history_value_code(SimulationValue value)
{
    switch (value) {
    case SimulationValue::Low:
        return 'L';
    case SimulationValue::High:
        return 'H';
    case SimulationValue::HighImpedance:
        return 'Z';
    default:
        return 'X';
    }
}
char invert_history_code(char value)
{
    return value == 'H' ? 'L' : value == 'L' ? 'H' : value;
}
std::string pad_report_field(std::string value, std::size_t width)
{
    value.resize(std::min(value.size(), width));
    value.append(width - value.size(), ' ');
    return value;
}
std::string simulation_report_header(const Design& design, bool trace)
{
    auto metadata = [&](const char* key) {
        auto at = design.metadata.find(key);
        return at == design.metadata.end() ? std::string() : at->second;
    };
    std::string text = program_banner("MACHSIMU", "\r\n") + "\r\n\r\nMACHXL SIMULATION ";
    text += trace ? "SELECTIVE TRACE LISTING" : "HISTORY LISTING";
    text += "\r\n\r\n";
    text += "Title     : " + pad_report_field(metadata("TITLE"), 24) +
            " Author   : " + pad_report_field(metadata("AUTHOR"), 17) + nl;
    text += "Pattern   : " + pad_report_field(metadata("PATTERN"), 24) +
            " Company  : " + pad_report_field(metadata("COMPANY"), 17) + nl;
    text += "Revision  : " + pad_report_field(metadata("REVISION"), 24) +
            " Date     : " + pad_report_field(metadata("DATE"), 17) + nl + nl;
    return text;
}
struct Row
{
    std::string name, label;
    bool inverted = false;
    unsigned pin  = 0;
};
std::string render_pages(const Design& design, const SimulationResult& result, const std::vector<Row>& rows,
                         const std::vector<std::size_t>& events, unsigned& page, bool trace)
{
    if (events.empty())
        return {};
    std::size_t width = 0;
    for (const auto& row : rows)
        width = std::max(width, row.label.size());
    width += 3;
    std::string text;
    for (std::size_t start = 0; start < events.size(); start += 60) {
        text += pad_report_field(design.device, 15) + nl + "Page : " + std::to_string(++page) + " " + nl;
        if (trace && start == 0)
            text += "\x0b\r\n";
        std::string markers(width, ' ');
        for (std::size_t j = 0; j < 60; ++j)
            markers += start + j < events.size() ? result.report_events[events[start + j]].command : ' ';
        text += markers + nl;
        for (const auto& row : rows) {
            text += pad_report_field((row.inverted ? "" : " ") + row.label, width);
            for (std::size_t j = 0; j < 60; ++j) {
                if (start + j >= events.size()) {
                    text += ' ';
                    continue;
                }
                const auto& event = result.report_events[events[start + j]];
                char value        = 'X';
                if (row.name == "GND")
                    value = 'L';
                else if (row.name == "VCC")
                    value = 'H';
                else {
                    const auto at = event.state.signals.find(row.name);
                    if (at != event.state.signals.end())
                        value = history_value_code(at->second.value);
                    else if (row.pin && event.state.physical_inputs.count(row.pin))
                        value = history_value_code(event.state.physical_inputs.at(row.pin).value);
                    if (row.inverted)
                        value = invert_history_code(value);
                    if (std::find(event.clashes.begin(), event.clashes.end(), row.name) != event.clashes.end())
                        value = '?';
                }
                text += value;
            }
            text += nl;
        }
        text += "\x0c\r\n";
    }
    return text;
}
}  // namespace
std::string render_simulation_history(const Design& design, const SimulationResult& result,
                                      const SimulationPinMap& placement)
{
    auto aliases = pla_signal_aliases(design);
    std::vector<Row> rows;
    auto add = [&](const std::string& name, unsigned pin) {
        if (!pin && std::none_of(design.pins.begin(), design.pins.end(), [&](const auto& p) { return p.name == name; }))
            return;
        bool inverted = false;
        for (const auto& p : design.pins)
            if (p.name == name) {
                inverted = p.inverted;
                break;
            }
        const auto alias = aliases.find(name);
        const auto label = (inverted ? "/" : "") + (alias == aliases.end() ? name : alias->second);
        rows.push_back({name, label, inverted, pin});
    };
    auto names = placement.signals;
    for (const auto& pin : design.pins)
        if (!pin.node && pin.number && pin.number < names.size() && names[pin.number].empty())
            names[pin.number] = pin.name;
    for (unsigned i = 1; i < names.size(); ++i)
        if (!names[i].empty())
            add(names[i], i);
    if (!placement.nodes.empty()) {
        for (const auto& name : placement.nodes)
            if (!name.empty())
                add(name, 0);
    } else {
        for (const auto& pin : design.pins)
            if (pin.node)
                add(pin.name, 0);
    }
    std::set<std::string> listed;
    for (const auto& row : rows)
        listed.insert(row.name);
    for (const auto& pin : design.pins)
        if (!listed.count(pin.name))
            add(pin.name, 0);
    std::string text = simulation_report_header(design, false);
    for (std::size_t i = 0; i < result.report_clashes.size(); ++i) {
        const auto& clash = result.report_clashes[i];
        std::ostringstream line;
        bool negative = false;
        for (const auto& pin : design.pins)
            if (pin.name == clash.signal) {
                negative = pin.inverted;
                break;
            }
        const auto name = (negative ? "/" : "") + clash.signal;
        line << "Check clash " << std::setw(4) << i + 1 << ":" << name;
        if (clash.register_check)
            line << " of Register " << name;
        line << nl << "Expect  "
             << (negative ? invert_history_code(history_value_code(clash.expected))
                          : history_value_code(clash.expected))
             << "  Actual  "
             << (negative ? invert_history_code(history_value_code(clash.actual)) : history_value_code(clash.actual))
             << nl << nl;
        line << "Total number of check clashes   ====>>>    " << i + 1 << nl;
        line << "Clash detected at vector number ====>>>    " << clash.event + 1 << nl << nl;
        text += line.str();
    }
    std::vector<std::size_t> events;
    for (std::size_t i = 0; i < result.report_events.size(); ++i)
        events.push_back(i);
    unsigned page = 0;
    return text + render_pages(design, result, rows, events, page, false);
}
std::string render_simulation_trace(const Design& design, const SimulationResult& result, const SimulationPinMap&)
{
    if (result.report_traces.empty())
        return {};
    auto aliases     = pla_signal_aliases(design);
    std::string text = simulation_report_header(design, true);
    unsigned page    = 0;
    for (std::size_t trace = 0; trace < result.report_traces.size(); ++trace) {
        std::vector<Row> rows;
        for (const auto& signal : result.report_traces[trace]) {
            const auto alias = aliases.find(signal.name);
            rows.push_back({signal.name,
                            (signal.inverted ? "/" : "") + (alias == aliases.end() ? signal.name : alias->second),
                            signal.inverted, 0});
        }
        std::vector<std::size_t> events;
        for (std::size_t i = 0; i < result.report_events.size(); ++i)
            if (result.report_events[i].trace == int(trace))
                events.push_back(i);
        text += render_pages(design, result, rows, events, page, true);
    }
    return text;
}
}  // namespace machxl
