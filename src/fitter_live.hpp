#pragma once
#include "device_family.hpp"
#include "fitter.hpp"
#include "frontend.hpp"
#include <algorithm>
#include <exception>
#include <functional>
#include <map>
#include <set>

namespace machxl {
// MACHFITR checks set/reset product counts before unused-node removal,
// after discarding controls that do not apply to combinatorial outputs.
inline void validate_storage_control_products(const std::vector<std::string>& outputs,
                                              const std::vector<std::vector<std::string>>& terms)
{
    for (std::size_t column = 0; column < outputs.size(); ++column) {
        auto label = outputs[column];
        while (!label.empty() && (label.back() == '-' || label.back() == '%'))
            label.pop_back();
        const auto dot = label.find('.');
        if (dot == std::string::npos || terms[column].size() <= 1)
            continue;
        const auto function = label.substr(dot);
        if (function == ".RE" || function == ".PR")
            throw LogicError(std::string(function == ".RE" ? "z5520: RESET" : "z5519: SET") +
                             " requires at most one product term for " + label.substr(0, dot));
    }
}

// The original z5045 DRC runs before unused-equation elimination. Output
// pairs may supply their pad's enable, but an independent node cannot.
inline void validate_node_output_enables(const Design& design, const std::vector<std::string>& outputs)
{
    std::set<std::string> independent_nodes;
    for (const auto& pin : design.pins)
        if (pin.node && pin.pair.empty())
            independent_nodes.insert(pin.name);
    for (auto label : outputs) {
        while (!label.empty() && (label.back() == '-' || label.back() == '%'))
            label.pop_back();
        const auto dot = label.find('.');
        if (dot != std::string::npos && label.substr(dot) == ".OE" && independent_nodes.count(label.substr(0, dot)))
            throw LogicError("z5045: node " + label.substr(0, dot) + " cannot have an output enable equation");
    }
}

// Node 1 is the global node. It has no location in the device: its reset and
// set equations stand for those of every register that has none of its own.
// Replace it by these per-register controls before anything else looks at
// the design, and drop the node itself.
// What the global node did to a design: which registers took its reset and
// its set, and whether a register kept a reset of its own (warning z5037).
struct GlobalNodeUse
{
    bool reset_overridden = false;
    std::set<std::string> reset, preset;
};
inline void expand_global_node(Design& design, const std::vector<std::string>& inputs,
                               std::vector<std::string>& outputs, std::vector<std::vector<std::string>>& terms,
                               GlobalNodeUse* use = nullptr)
{
    const auto global = std::find_if(design.pins.begin(), design.pins.end(),
                                     [](const Pin& pin) { return pin.node && pin.number == 1; });
    if (global == design.pins.end())
        return;
    const auto name = global->name;
    const auto base = [](std::string label) {
        while (!label.empty() && (label.back() == '-' || label.back() == '%'))
            label.pop_back();
        return label;
    };
    // MACHFITR z5040/z5041: only set and reset equations, and no readers.
    for (const auto& output : outputs) {
        const auto label = base(output);
        if (label.substr(0, label.find('.')) == name && label != name + ".RE" && label != name + ".PR")
            throw LogicError("z5040: Invalid global node defining equation.");
    }
    const auto read = std::find(inputs.begin(), inputs.end(), name);
    if (read != inputs.end()) {
        const auto column = std::size_t(read - inputs.begin());
        for (const auto& products : terms)
            for (const auto& term : products)
                if (column < term.size() && term[column] != '-')
                    throw LogicError(
                        "z5041: Global node cannot be used as an input (i.e. referenced on right "
                        "hand side of equations).");
    }
    for (const char* function : {"RE", "PR"}) {
        const auto column = std::find_if(outputs.begin(), outputs.end(),
                                         [&](const auto& label) { return base(label) == name + "." + function; });
        if (column == outputs.end())
            continue;
        const auto control = terms[std::size_t(column - outputs.begin())];
        std::vector<std::string> registers;
        std::set<std::string> controlled;
        for (const auto& output : outputs) {
            const auto label = base(output);
            const auto dot   = label.find('.');
            if (dot == std::string::npos)
                continue;
            const auto signal = label.substr(0, dot), kind = label.substr(dot + 1);
            if (kind == function)
                controlled.insert(signal);
            const bool storage = kind == "REG" || kind == "T" || kind == "G" || kind == "XORR_REG" ||
                                 kind == "XORR_T" || kind == "XORR_G";
            if (storage && signal != name && std::find(registers.begin(), registers.end(), signal) == registers.end())
                registers.push_back(signal);
        }
        for (const auto& signal : registers) {
            if (controlled.count(signal)) {
                if (use && std::string(function) == "RE")
                    use->reset_overridden = true;
                continue;
            }
            if (use)
                (std::string(function) == "RE" ? use->reset : use->preset).insert(signal);
            std::size_t last = 0;
            for (std::size_t i = 0; i < outputs.size(); ++i) {
                const auto label = base(outputs[i]);
                if (label.substr(0, label.find('.')) == signal)
                    last = i;
            }
            outputs.insert(outputs.begin() + std::ptrdiff_t(last + 1), signal + "." + function);
            terms.insert(terms.begin() + std::ptrdiff_t(last + 1), control);
        }
    }
    for (std::size_t i = outputs.size(); i-- > 0;) {
        const auto label = base(outputs[i]);
        if (label.substr(0, label.find('.')) == name) {
            outputs.erase(outputs.begin() + std::ptrdiff_t(i));
            terms.erase(terms.begin() + std::ptrdiff_t(i));
        }
    }
    design.equations.erase(std::remove_if(design.equations.begin(), design.equations.end(),
                                          [&](const Equation& e) { return e.signal == name; }),
                           design.equations.end());
    design.pins.erase(
        std::remove_if(design.pins.begin(), design.pins.end(), [&](const Pin& pin) { return pin.name == name; }),
        design.pins.end());
}

// MACHFITR z5029: MACH1 and MACH355 have no input registers. An input
// pairing declared on a pin is discarded there and the node is fitted as
// ordinary logic. Returns the text of one warning per such pairing.
inline std::vector<std::string> discarded_input_pairs(const Design& design)
{
    std::vector<std::string> result;
    if (design.device.compare(0, 5, "MACH1") != 0 && design.device != "MACH355")
        return result;
    for (const auto& pad : design.pins) {
        if (pad.node || pad.pair.empty())
            continue;
        const auto node = std::find_if(design.pins.begin(), design.pins.end(),
                                       [&](const Pin& other) { return other.node && other.name == pad.pair; });
        if (node != design.pins.end())
            result.push_back("z5029: Invalid pairing of signals " + pad.name + " and " + pad.pair +
                             " - discard pairing.");
    }
    return result;
}

// MACHFITR's INFO lines, and warning z5064, about what the fitter assumes for
// equations a design leaves out. The original walks the signals in its
// logical order: fixed pins by number, floating pins as declared, then the
// nodes. A node that a pin declares as its input pair is looked at with that
// pin. Each signal is checked for an implicit output enable first and for
// implicit set/reset second, and the default clock is announced last.
//
//   z5109  the device has no input registers (MACH1, MACH355)
//   z5065  an output pin has no output enable equation
//   z5064  an output enable equation is constant GND
//   z5070  a register lacks a set or reset equation; z5068 when unspecified
//          set/reset is not a don't care, z5128 on MACH215. MACH2 input
//          registers count as well, except on MACH215.
//   z5121  a macrocell register or latch has no clock equation and gets the
//          default clock pin; z5056 on MACH3/4
//
// `source` is the design as written, `outputs` and `terms` the prepared PLA,
// `input_registers` the nodes the fitter realizes as input registers.
inline void report_fitter_notices(const Design& source, const std::vector<std::string>& outputs,
                                  const std::vector<std::vector<std::string>>& terms,
                                  const std::set<std::string>& input_registers, const MachDeviceLayout& d,
                                  const FitterOptions& fitting)
{
    const bool mach1    = d.device.compare(0, 5, "MACH1") == 0;
    const bool mach2    = d.device.compare(0, 5, "MACH2") == 0;
    const bool mach215  = d.device == "MACH215";
    const bool pairless = mach1 || d.device == "MACH355";
    if (pairless)
        fitting.info("z5109: For this device, all input pairing is ignored since input registers or latches do "
                     "not exist. All signals must pass through the central switch matrix before being latched "
                     "or registered.");
    std::map<std::string, std::set<std::string>> functions;
    std::set<std::string> grounded_enable;
    for (unsigned column = 0; column < outputs.size(); ++column) {
        auto label = outputs[column];
        while (!label.empty() && (label.back() == '-' || label.back() == '%'))
            label.pop_back();
        const auto dot      = label.find('.');
        const auto name     = label.substr(0, dot);
        const auto function = dot == std::string::npos ? std::string("COM") : label.substr(dot + 1);
        functions[name].insert(function);
        if (function == "OE" && column < terms.size() && terms[column].empty())
            grounded_enable.insert(name);
    }
    const auto has = [&](const std::string& name, const char* function) {
        const auto found = functions.find(name);
        return found != functions.end() && found->second.count(function);
    };
    const auto driven = [&](const std::string& name) {
        return has(name, "COM") || has(name, "REG") || has(name, "G") || has(name, "T");
    };
    bool default_clock      = false;
    const auto set_reset    = [&] {
        if (mach215)
            fitting.info("z5128: Any unspecified SET/RESET equation will be defaulted to GND.");
        else if (fitting.ground_implicit_controls)
            fitting.info("z5068: Since unspecified SET/RESET is NOT to be treated as DON'T CARE, any unspecified "
                         "SET/RESET will be defaulted to GND.");
        else
            fitting.info("z5070: Unspecified SET/RESETs are to be treated as DON'T CARE. Hence, a register with "
                         "unspecified SET/RESET equation(s) may be initialized via SET/RESET product terms that "
                         "are inherited from partitioning/placement to the same PAL block as other registers for "
                         "which SET/RESET equations have been written. Therefore, simulation results may be "
                         "unexpected for any register uninitialized via explicit SET/RESET in the design file.");
    };
    const auto storage_of = [&](const Pin& pin, bool through_pair) {
        if (input_registers.count(pin.name)) {
            if (mach2 && !mach215)
                set_reset();
            return;
        }
        bool storage = has(pin.name, "REG") || has(pin.name, "G") || has(pin.name, "T");
        // A discarded input pairing leaves a declared register without
        // equations; it is fitted as a macrocell register all the same.
        if (through_pair && pairless && !driven(pin.name) && (pin.registered || pin.latched))
            storage = true;
        if (!storage)
            return;
        if (!has(pin.name, "PR") || !has(pin.name, "RE"))
            set_reset();
        default_clock |= !has(pin.name, "C");
    };
    std::vector<const Pin*> order;
    for (const bool node : {false, true}) {
        const auto first = order.size();
        for (const auto& pin : source.pins)
            if (pin.node == node && pin.number)
                order.push_back(&pin);
        std::stable_sort(order.begin() + long(first), order.end(),
                         [](const Pin* a, const Pin* b) { return a->number < b->number; });
        for (const auto& pin : source.pins)
            if (pin.node == node && !pin.number)
                order.push_back(&pin);
    }
    for (const auto* pin : order) {
        if (pin->node) {
            storage_of(*pin, false);
            continue;
        }
        if (!pin->pair.empty())
            for (const auto& partner : source.pins)
                if (partner.node && partner.name == pin->pair) {
                    if (pairless)
                        fitting.warn("z5029: Invalid pairing of signals " + pin->name + " and " + pin->pair +
                                     " - discard pairing.");
                    storage_of(partner, true);
                }
        if (driven(pin->name)) {
            if (grounded_enable.count(pin->name))
                fitting.warn("z5064: Output signal " + pin->name + " has output enable equal GND.");
            else if (!has(pin->name, "OE"))
                fitting.info("z5065: For outputs, implicit output enables will be set to VCC.");
        }
        storage_of(*pin, false);
    }
    if (!default_clock || d.clock_pins.empty())
        return;
    const auto pin = std::to_string(mach215 || !(mach1 || mach2) ? d.clock_pins.front() : d.clock_pins.back());
    if (mach1 || mach2)
        fitting.info("z5121: Implicit clock or latch enable will be assigned to the default Clock/LE Pin " + pin +
                     ".");
    else
        fitting.info("z5056: If a signal is preplaced on the default clock pin or the default clock pin is "
                     "required for an explicit clock due to clocking requirements, then all clock and latch "
                     "enable equations must be explicitly defined in the design file. However, if the default "
                     "clock is not used and a clock equation for a register, or a latch enable equation for a "
                     "latch, is not written, then a clock or latch enable will be created for the signal and "
                     "will be assigned to the default CLOCK/LE Pin " + pin + ".");
}

// Reports the notices on the way out when a check rejects the design before
// the fitter has settled its input registers; the original has printed them
// by then. `report` is the best estimate available at that point.
class FitterNoticeGuard
{
  public:
    explicit FitterNoticeGuard(std::function<void()> report) : report_(std::move(report)) {}
    FitterNoticeGuard(const FitterNoticeGuard&)            = delete;
    FitterNoticeGuard& operator=(const FitterNoticeGuard&) = delete;
    void done() { report_ = nullptr; }
    ~FitterNoticeGuard()
    {
        if (report_ && std::uncaught_exceptions() > exceptions_)
            try {
                report_();
            } catch (...) {
            }
    }

  private:
    std::function<void()> report_;
    int exceptions_ = std::uncaught_exceptions();
};

// MACHFITR z5073: a declared pin that no remaining equation drives or reads
// is dropped, which frees its package pin. Returns such pins in declaration
// order; labels of outputs name the driven signals.
inline std::vector<std::string> unreferenced_pins(const Design& design, const std::vector<std::string>& inputs,
                                                  const std::vector<std::string>& output_labels,
                                                  const std::set<std::string>& kept = {})
{
    std::set<std::string> used(inputs.begin(), inputs.end());
    for (auto label : output_labels) {
        while (!label.empty() && (label.back() == '-' || label.back() == '%'))
            label.pop_back();
        used.insert(label.substr(0, label.find('.')));
    }
    std::vector<std::string> result;
    for (const auto& pin : design.pins)
        if (!pin.node && !used.count(pin.name) && !kept.count(pin.name))
            result.push_back(pin.name);
    return result;
}

// MACHFITR z5072: a node that some equation reads must have an equation of
// its own. A buried node has no pad that could supply a value.
inline void validate_node_equations(const Design& design, const std::vector<std::string>& inputs,
                                    const std::vector<std::string>& outputs,
                                    std::vector<std::string>* unreferenced = nullptr)
{
    std::set<std::string> driven;
    for (auto label : outputs) {
        while (!label.empty() && (label.back() == '-' || label.back() == '%'))
            label.pop_back();
        driven.insert(label.substr(0, label.find('.')));
    }
    const std::set<std::string> read(inputs.begin(), inputs.end());
    for (const auto& pin : design.pins)
        if (pin.node && read.count(pin.name) && !driven.count(pin.name))
            throw LogicError("z5072: Missing equation for Node " + pin.name);
    // A declared node with neither an equation nor a reader is dropped.
    if (unreferenced)
        for (const auto& pin : design.pins)
            if (pin.node && !read.count(pin.name) && !driven.count(pin.name))
                unreferenced->push_back(pin.name);
}

// Block that a design's fixed location puts a signal in, or -1: the block of
// a fixed node's macrocell, or of the I/O pin a pad is fixed on.
inline int fixed_block_of(const Design& design, const MachDeviceLayout& layout, const std::string& name)
{
    const auto pin =
        std::find_if(design.pins.begin(), design.pins.end(), [&](const Pin& item) { return item.name == name; });
    if (pin == design.pins.end() || !pin->number)
        return -1;
    if (pin->node)
        return pin->number >= 2 && pin->number - 2 < layout.blocks * layout.cells
                   ? int((pin->number - 2) / layout.cells)
                   : -1;
    const auto io = std::find(layout.io_pins.begin(), layout.io_pins.end(), pin->number);
    return io == layout.io_pins.end()
               ? -1
               : int(std::size_t(io - layout.io_pins.begin()) / (layout.io_pins.size() / layout.blocks));
}

// With "SET/RESET treated as DONT_CARE" off, a register without a set or
// reset equation gets a constant GND one. `skip` names input registers,
// which have neither.
inline void ground_implicit_set_reset(std::vector<std::string>& outputs, std::vector<std::vector<std::string>>& terms,
                                      const std::set<std::string>& skip = {})
{
    std::map<std::string, std::set<std::string>> functions;
    std::vector<std::string> storage;
    for (auto label : outputs) {
        while (!label.empty() && (label.back() == '-' || label.back() == '%'))
            label.pop_back();
        const auto dot = label.find('.');
        if (dot == std::string::npos)
            continue;
        const auto name = label.substr(0, dot), function = label.substr(dot + 1);
        if ((function == "REG" || function == "G" || function == "T") && !skip.count(name) &&
            functions.find(name) == functions.end())
            storage.push_back(name);
        functions[name].insert(function);
    }
    for (const auto& name : storage)
        for (const auto* function : {"RE", "PR"})
            if (!functions[name].count(function)) {
                outputs.push_back(name + "." + function);
                terms.emplace_back();
            }
}

// MACHFITR z5514/z5515: the registers of a MACH1/2 block share one set and
// one reset product term. Signals fixed in the same block must not ask for
// different ones; constant GND counts as a term of its own, an absent
// equation does not count.
inline void validate_block_set_reset(const Design& design, const MachDeviceLayout& layout,
                                     const std::vector<std::string>& outputs,
                                     const std::vector<std::vector<std::string>>& terms)
{
    for (const auto* function : {"PR", "RE"}) {
        std::map<int, std::set<std::vector<std::string>>> used;
        for (unsigned column = 0; column < outputs.size() && column < terms.size(); ++column) {
            const auto& label = outputs[column];
            const auto dot    = label.find('.');
            if (dot == std::string::npos || label.substr(dot + 1) != function)
                continue;
            const int block = fixed_block_of(design, layout, label.substr(0, dot));
            if (block >= 0)
                used[block].insert(terms[column]);
        }
        for (const auto& block : used)
            if (block.second.size() > 1)
                throw LogicError(std::string(function == std::string("PR") ? "z5514" : "z5515") + ": Block " +
                                 char('A' + block.first) + " contains " + std::to_string(block.second.size()) +
                                 (function == std::string("PR") ? " SET" : " RESET") +
                                 " signals: block limit is 1 signals.");
    }
}

// MACHFITR DRC z5510: signals fixed in one block must together read no more
// signals than the block routes. This is a warning; the fit goes on and
// fails later if the block cannot be relieved. Clock equations are left out
// as in z5524. `block_of` maps a signal name to its fixed block or -1.
template <class BlockOf>
std::vector<std::string> preplaced_fanin_warnings(const std::vector<std::string>& inputs,
                                                  const std::vector<std::string>& outputs,
                                                  const std::vector<std::vector<std::string>>& terms,
                                                  std::size_t capacity, const BlockOf& block_of)
{
    std::map<int, std::set<std::size_t>> fanin;
    for (std::size_t column = 0; column < outputs.size(); ++column) {
        auto label = outputs[column];
        while (!label.empty() && (label.back() == '-' || label.back() == '%'))
            label.pop_back();
        const auto dot = label.find('.');
        if (dot != std::string::npos && label.substr(dot) == ".C")
            continue;
        const int block = block_of(label.substr(0, dot));
        if (block < 0)
            continue;
        for (const auto& term : terms[column])
            for (std::size_t input = 0; input < term.size() && input < inputs.size(); ++input)
                if (term[input] != '-')
                    fanin[block].insert(input);
    }
    std::vector<std::string> result;
    for (const auto& block : fanin)
        if (block.second.size() > capacity)
            result.push_back(std::string("z5510: Block ") + char('A' + block.first) + " has a logic fanin of " +
                             std::to_string(block.second.size()) +
                             " due to preplaced or block-restricted signals: routing capacity is " +
                             std::to_string(capacity) + " signals.");
    return result;
}

// MACHFITR DRC z5524: all equations of one signal are placed in one block,
// so together they may read no more signals than a block's switch matrix
// routes. A dedicated clock does not pass through the matrix and is left
// out of the count.
inline void validate_equation_fanin(const std::vector<std::string>& inputs, const std::vector<std::string>& outputs,
                                    const std::vector<std::vector<std::string>>& terms, std::size_t capacity)
{
    std::map<std::string, std::set<std::size_t>> fanin;
    std::vector<std::string> order;
    for (std::size_t column = 0; column < outputs.size(); ++column) {
        auto label = outputs[column];
        while (!label.empty() && (label.back() == '-' || label.back() == '%'))
            label.pop_back();
        const auto dot = label.find('.');
        if (dot != std::string::npos && label.substr(dot) == ".C")
            continue;
        const auto name = label.substr(0, dot);
        if (!fanin.count(name))
            order.push_back(name);
        auto& used = fanin[name];
        for (const auto& term : terms[column])
            for (std::size_t input = 0; input < term.size() && input < inputs.size(); ++input)
                if (term[input] != '-')
                    used.insert(input);
    }
    for (const auto& name : order)
        if (fanin.at(name).size() > capacity)
            throw LogicError("z5524: The logic and functional equations for " + name + " require " +
                             std::to_string(fanin.at(name).size()) + " distinct signals: routing capacity is " +
                             std::to_string(capacity) + " signals.");
}

// z5091/5092/5093 are warnings for a combinatorial output: set/reset/clock
// equations are discarded before DRC, routing and input-use classification.
inline void discard_combinatorial_storage_controls(std::vector<std::string>& outputs,
                                                   std::vector<std::vector<std::string>>& terms)
{
    std::set<std::string> combinatorial;
    for (auto label : outputs) {
        while (!label.empty() && (label.back() == '-' || label.back() == '%'))
            label.pop_back();
        const auto dot = label.find('.');
        if (dot == std::string::npos || label.substr(dot) == ".XORR")
            combinatorial.insert(label.substr(0, dot));
    }
    for (std::size_t column = outputs.size(); column-- > 0;) {
        auto label = outputs[column];
        while (!label.empty() && (label.back() == '-' || label.back() == '%'))
            label.pop_back();
        const auto dot = label.find('.');
        if (dot != std::string::npos &&
            (label.substr(dot) == ".C" || label.substr(dot) == ".RE" || label.substr(dot) == ".PR") &&
            combinatorial.count(label.substr(0, dot))) {
            outputs.erase(outputs.begin() + column);
            terms.erase(terms.begin() + column);
        }
    }
}

// MACHFITR repeatedly removes buried equations with no users. Self and
// mutual feedback count as users even when no package output observes them.
// Keep source declarations intact: even a discarded signal participates in
// the original logical-ID allocation before physical resource assignment.
inline std::set<std::string> retained_equation_names(const Design& design, const std::set<std::string>& primary,
                                                     const std::map<std::string, std::set<std::string>>& dependencies)
{
    std::set<std::string> pads;
    for (const auto& pin : design.pins)
        if (!pin.node && primary.count(pin.name))
            pads.insert(pin.name);
    auto retained = primary;
    for (;;) {
        auto used = pads;
        for (const auto& owner : retained) {
            auto found = dependencies.find(owner);
            if (found != dependencies.end())
                for (const auto& input : found->second)
                    if (retained.count(input))
                        used.insert(input);
        }
        for (const auto& pin : design.pins)
            if (pin.node && !pin.pair.empty() && retained.count(pin.pair) && retained.count(pin.name))
                used.insert(pin.name);
        if (used == retained)
            return retained;
        retained = std::move(used);
    }
}

inline std::set<std::string> retain_reachable_equations(const Design& design, std::vector<std::string>& inputs,
                                                        std::vector<std::string>& outputs,
                                                        std::vector<std::vector<std::string>>& terms,
                                                        std::vector<std::string>* discarded = nullptr)
{
    std::map<std::string, std::vector<unsigned>> columns;
    std::set<std::string> primary;
    std::vector<std::string> names;
    for (unsigned column = 0; column < outputs.size(); ++column) {
        auto label = outputs[column];
        while (!label.empty() && (label.back() == '-' || label.back() == '%'))
            label.pop_back();
        const auto dot      = label.find('.');
        const auto name     = label.substr(0, dot);
        const auto function = dot == std::string::npos ? "COM" : label.substr(dot + 1);
        columns[name].push_back(column);
        names.push_back(name);
        if (function == "COM" || function == "REG" || function == "T" || function == "G" ||
            function.compare(0, 4, "XORR") == 0)
            primary.insert(name);
    }
    std::map<std::string, std::set<std::string>> dependencies;
    for (const auto& name : primary)
        for (auto column : columns.at(name))
            for (const auto& term : terms[column])
                for (unsigned i = 0; i < inputs.size(); ++i)
                    if (term[i] != '-' && primary.count(inputs[i]))
                        dependencies[name].insert(inputs[i]);
    const auto reachable = retained_equation_names(design, primary, dependencies);
    if (discarded)
        for (const auto& name : names)
            if (primary.count(name) && !reachable.count(name) &&
                std::find(discarded->begin(), discarded->end(), name) == discarded->end())
                discarded->push_back(name);
    for (std::size_t column = outputs.size(); column-- > 0;)
        if (!reachable.count(names[column])) {
            outputs.erase(outputs.begin() + column);
            terms.erase(terms.begin() + column);
        }
    std::vector<bool> live(inputs.size());
    for (const auto& cover : terms)
        for (const auto& term : cover)
            for (unsigned i = 0; i < inputs.size(); ++i)
                if (term[i] != '-')
                    live[i] = true;
    for (std::size_t i = inputs.size(); i-- > 0;)
        if (!live[i]) {
            inputs.erase(inputs.begin() + i);
            for (auto& cover : terms)
                for (auto& term : cover)
                    term.erase(i, 1);
        }
    return reachable;
}
}  // namespace machxl
