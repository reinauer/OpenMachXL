#include "fitter_place.hpp"
#include "device_mach111.hpp"
#include "fitter_budget.hpp"
#include "fitter_family_place.hpp"
#include "fitter_iteration.hpp"
#include "fitter_output_search.hpp"
#include <algorithm>
#include <array>
#include <functional>
#include <set>
#include <stdexcept>

namespace machxl {
namespace {
[[noreturn]] void unsupported(const std::string& text)
{
    throw std::runtime_error("MACH111 placement: " + text);
}
int cell_for_pin(unsigned pin)
{
    for (unsigned c = 0; c < 32; ++c)
        if (mach111_io_pin(c) == pin)
            return static_cast<int>(c);
    return -1;
}
bool clock_pin(unsigned pin)
{
    return pin == 11 || pin == 13 || pin == 33 || pin == 35;
}
bool data_pin(unsigned pin)
{
    return cell_for_pin(pin) >= 0 || pin == 10 || pin == 32 || clock_pin(pin);
}

// MACH111 output-enable capacity arithmetic preserves the DOS zero-capacity shortcut.
bool oe_half_capacity_fits(int a, int b, const std::vector<unsigned>& groups)
{
    if (a < 0 || b < 0)
        return false;
    if (a == 0 || b == 0)
        return true;
    unsigned count[4] = {};
    unsigned sum      = 0;
    for (auto n : groups) {
        ++count[n];
        sum += n;
    }
    if (static_cast<unsigned>(a + b) >= sum + 2 || (!count[2] && !count[3]))
        return true;
    if (!count[1]) {
        if (!count[2])
            return static_cast<unsigned>(a / 3 + b / 3) >= count[3];
        if (!count[3])
            return static_cast<unsigned>(a / 2 + b / 2) >= count[2];
        if (static_cast<unsigned>(a + b) >= sum + 1)
            return true;
        if (a == 1 || b == 1)
            return false;
        if (a == 2 || b == 2 || a == 3 || b == 3 || count[2] != 1)
            return true;
        return (a - 2) % 3 == 0 || (b - 2) % 3 == 0;
    }
    return count[2] || !count[3] || static_cast<unsigned>(a / 3 + b / 3) >= count[3];
}
bool insert_output_enable(std::array<std::string, 4>& table, unsigned half, const std::string& key)
{
    if (key.empty())
        return true;
    // Stop at the first matching or empty slot.
    for (unsigned slot = half * 2; slot < half * 2 + 2; ++slot)
        if (table[slot] == key || table[slot].empty()) {
            table[slot] = key;
            return true;
        }
    return false;
}
std::array<std::string, 4> partition_oe(const std::array<std::string, 4>& fixed, const std::vector<std::string>& order,
                                        const std::vector<Mach111PlaceSignal>& signals,
                                        const std::vector<std::size_t>& members)
{
    if (order.size() > 4)
        unsupported("more than four partition OE products");
    std::array<std::string, 4> key;
    std::copy(order.begin(), order.end(), key.begin());
    std::array<int, 4> size{};
    std::array<std::vector<unsigned>, 4> group_sizes;
    std::vector<unsigned> plain;
    int available[2] = {8, 8};
    for (auto id : members) {
        const auto& s         = signals[id];
        const unsigned groups = static_cast<unsigned>((s.order.product_terms + 3) / 4);
        if (s.order.fixed_pin >= 0) {
            available[(cell_for_pin(static_cast<unsigned>(s.order.fixed_pin) + 1) % 16) / 8] -=
                static_cast<int>(groups);
        } else if (s.oe_product.empty())
            plain.push_back(groups);
        else {
            const auto it = std::find(key.begin(), key.end(), s.oe_product);
            if (it == key.end())
                unsupported("missing partition OE resource group");
            const auto index = static_cast<unsigned>(it - key.begin());
            size[index] += static_cast<int>(groups);
            group_sizes[index].push_back(groups);
        }
    }
    const auto pair = [&](std::array<std::string, 4>& table, const std::array<unsigned, 4>& indexes) {
        return insert_output_enable(table, 0, key[indexes[0]]) && insert_output_enable(table, 0, key[indexes[1]]) &&
               insert_output_enable(table, 1, key[indexes[2]]) && insert_output_enable(table, 1, key[indexes[3]]);
    };
    // The first branch uses keys [0,1,1,2] while subtracting sizes [0,1,2,3].
    // Preserve this mismatch for compatibility: only used slots are overwritten,
    // so a duplicated cube in an unused slot survives.
    constexpr unsigned permutations[3][4] = {{0, 1, 2, 3}, {0, 3, 1, 2}, {0, 2, 1, 3}};
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        const auto& p = permutations[attempt];
        auto table    = fixed;
        const std::array<unsigned, 4> keys =
            attempt == 0 ? std::array<unsigned, 4>{0, 1, 1, 2} : std::array<unsigned, 4>{p[0], p[1], p[2], p[3]};
        if (oe_half_capacity_fits(available[0] - size[p[0]] - size[p[1]], available[1] - size[p[2]] - size[p[3]],
                                  plain) &&
            pair(table, keys))
            return table;
    }
    if (order.size() == 1) {
        auto table = fixed;
        insert_output_enable(table, 0, key[0]);
        insert_output_enable(table, 1, key[0]);
        return table;
    }
    if (order.size() == 2) {
        auto plain_both = plain;
        for (unsigned n = 0; n < 2; ++n)
            plain_both.insert(plain_both.end(), group_sizes[n].begin(), group_sizes[n].end());
        constexpr unsigned split[5][4] = {{0, 1, 0, 1}, {0, 0, 0, 1}, {0, 1, 0, 0}, {0, 1, 1, 1}, {1, 1, 0, 1}};
        for (const auto& p : split) {
            auto table = fixed;
            if (pair(table, {p[0], p[1], p[2], p[3]}) && oe_half_capacity_fits(available[0], available[1], plain_both))
                return table;
        }
    }
    if (order.size() == 3) {
        // Try shared groups in order, with the two remaining groups assigned
        // to opposite halves in this sequence.
        constexpr unsigned split[6][5] = {{0, 1, 2, 1, 0}, {0, 2, 1, 2, 0}, {1, 0, 2, 0, 1},
                                          {1, 2, 0, 2, 1}, {2, 0, 1, 2, 0}, {2, 1, 0, 2, 1}};
        for (const auto& p : split) {
            auto pending = plain;
            pending.insert(pending.end(), group_sizes[p[0]].begin(), group_sizes[p[0]].end());
            auto table = fixed;
            // Last two branches intentionally put the shared group first.
            const std::array<unsigned, 4> keys =
                p[0] == 2 ? std::array<unsigned, 4>{2, p[1], 2, p[2]} : std::array<unsigned, 4>{p[1], p[0], p[2], p[0]};
            if (oe_half_capacity_fits(available[0] - size[p[1]], available[1] - size[p[2]], pending) &&
                pair(table, keys))
                return table;
        }
    }
    throw PhysicalPlacementFailure("partition output-enable half-block assignment failed");
}

// Allocate ordinary MACH111 data equations with four products per group.
// Working and saved owners are separate: successful displaced frames commit
// before their parent resumes.
struct ProductAllocator
{
    std::array<int, 16> owner, saved, demand{};
    std::array<bool, 16> active{};
    ProductAllocator()
    {
        owner.fill(-1);
        saved.fill(-1);
    }
    static std::vector<int> candidates(int cell)
    {
        std::vector<int> result;
        for (int delta : {0, 1, -1}) {
            const int other = cell + delta;
            if (other >= 0 && other < 16 && other / 8 == cell / 8)
                result.push_back(other);
        }
        return result;
    }
    bool frame(int cell)
    {
        active[cell]      = true;
        const auto groups = candidates(cell);
        int remaining     = demand[cell];
        for (std::size_t cursor = 0; cursor < groups.size(); ++cursor) {
            // Count only entries preceding the current cursor.
            remaining = demand[cell];
            for (std::size_t p = 0; p < cursor; ++p)
                if (owner[groups[p]] == cell)
                    remaining -= 4;
            if (remaining <= 0)
                break;
            const int g = groups[cursor];
            if (owner[g] == cell)
                remaining -= 4;
            else if (owner[g] < 0) {
                owner[g] = cell;
                remaining -= 4;
            } else if (!active[owner[g]]) {
                const int displaced = owner[g];
                owner[g]            = cell;
                remaining -= 4;
                if (!frame(displaced))
                    owner[g] = saved[g];
            }
            // Synchronous data-only allocation skips groups owned by an ancestor
            // frame without an XOR product.
        }
        remaining = demand[cell];
        for (int g : groups)
            if (owner[g] == cell)
                remaining -= 4;
        const bool success = remaining <= 0;
        if (success) {
            // Retain the first sufficient groups in steering order, releasing surplus
            // groups, including earlier allocations.
            remaining = demand[cell];
            for (int g : groups) {
                if (owner[g] == cell) {
                    if (remaining <= 0)
                        owner[g] = saved[g] = -1;
                    else {
                        saved[g] = cell;
                        remaining -= 4;
                    }
                } else if (saved[g] == cell)
                    saved[g] = -1;
            }
        } else {
            // Frame failure restores only resources still owned by it;
            // successful nested commits remain in the saved owner table.
            for (int g : groups)
                if (owner[g] == cell)
                    owner[g] = saved[g];
        }
        active[cell] = false;
        return success;
    }
    bool allocate(int cell, int terms)
    {
        const auto previous       = owner;
        const auto previous_saved = saved;
        demand[cell]              = terms;
        if (frame(cell))
            return true;
        owner        = previous;
        saved        = previous_saved;
        demand[cell] = 0;
        return false;
    }
    void remove(int cell)
    {
        for (unsigned g = 0; g < 16; ++g) {
            if (owner[g] == cell)
                owner[g] = -1;
            if (saved[g] == cell)
                saved[g] = -1;
        }
        demand[cell] = 0;
    }
};
}  // namespace

Mach111Placement place_mach111(
    const std::vector<Mach111PlaceSignal>& signals, Mach111PlaceOptions options,
    std::function<PlacementRouteFeedback(const Mach111Placement&, const std::vector<bool>&)> route,
    std::function<void(const Mach111Placement&, unsigned, const std::vector<bool>&)> failure)
{
    if (!options.iteration) {
        PartitionIterationState iteration(options.iterate_partition);
        options.iteration = &iteration;
        for (;;) {
            FittingIterationCounters counters(2, options.iterate_partition);
            const auto previous = iteration.completed;
            try {
                return place_mach111(signals, options, route, failure);
            } catch (const PhysicalPlacementFailure&) {
                if (!iteration.remaining || iteration.completed == previous)
                    throw;
            } catch (const RepartitionRequested&) {
                if (!iteration.remaining || iteration.completed == previous)
                    throw PhysicalPlacementFailure("z7502: physical fitting unsuccessful after partition retries");
            }
        }
    }
    Mach111Placement result;
    result.product_group_owner.fill(-1);
    result.cells.assign(signals.size(), -1);
    result.pins.assign(signals.size(), -1);
    result.oe_slots.assign(signals.size(), -1);
    result.implicit_controls.assign(signals.size(), 0);
    std::vector<FitterOrderKey> keys;
    std::vector<std::size_t> outputs;
    std::vector<Mach111PartitionCluster> clusters;
    std::array<std::vector<std::string>, 2> partition_oe_order;
    bool automatic = false;
    std::array<bool, 45> occupied_pins{};
    occupied_pins[35] = options.reserve_default_clock;
    std::array<bool, 32> occupied_cells{};
    for (std::size_t i = 0; i < signals.size(); ++i) {
        const auto& s = signals[i];
        keys.push_back(s.order);
        if (s.buried && !s.output)
            unsupported("buried signal must have an output macrocell record");
        if (s.order.fixed_pin >= 0) {
            const unsigned pin = static_cast<unsigned>(s.order.fixed_pin) + 1;
            if (pin > 44 || !data_pin(pin))
                unsupported("invalid fixed pin for " + s.name);
            // Input/output records for a bidirectional pin may share it.
            occupied_pins[pin] = true;
            result.pins[i]     = static_cast<int>(pin);
        }
        if (!s.output)
            continue;
        if (s.order.product_terms < 1 || s.order.product_terms > 12 || s.xor_product)
            unsupported("output requires a different product allocation path: " + s.name);
        outputs.push_back(i);
        clusters.push_back({{0, 0, static_cast<std::uint16_t>(s.order.serial)},
                            s.fanins,
                            s.reset,
                            !s.buried,
                            -1,
                            static_cast<unsigned>((s.order.product_terms + 3) / 4),
                            s.oe_product});
        clusters.back().preset          = s.preset;
        clusters.back().data_terms      = unsigned(s.order.product_terms);
        clusters.back().clocked_storage = s.clocked_storage;
        if (s.order.fixed_pin >= 0) {
            const int cell = cell_for_pin(static_cast<unsigned>(result.pins[i]));
            if (cell < 0 || occupied_cells[cell])
                unsupported("conflicting output pin for " + s.name);
            result.cells[i]             = cell;
            occupied_cells[cell]        = true;
            clusters.back().fixed_block = cell / 16;
            clusters.back().fixed_half  = (cell % 16) / 8;
        } else if (s.order.fixed_node >= 0) {
            // The device has 33 node records: record 0 is reserved and records 1..32
            // map to cells A0..B15. PDS node numbers add one to the record index.
            const int cell = s.fixed_cell;
            if (!s.buried || cell < 0 || cell >= 32 || occupied_cells[cell])
                unsupported("conflicting or invalid fixed node for " + s.name);
            result.cells[i]             = cell;
            occupied_cells[cell]        = true;
            clusters.back().fixed_block = cell / 16;
        } else
            automatic = true;
        if (s.order.block >= 0) {
            if (s.order.block > 1 || (clusters.back().fixed_block >= 0 && clusters.back().fixed_block != s.order.block))
                unsupported("conflicting block constraint for " + s.name);
            clusters.back().fixed_block = s.order.block;
        }
    }
    if (outputs.empty())
        unsupported("no output macrocells");
    std::vector<std::size_t> clocks;
    for (std::size_t i = 0; i < signals.size(); ++i)
        if (!signals[i].output && signals[i].order.type == 9)
            clocks.push_back(i);
    // Assign clocks before importing partition and placement signals. A sole
    // global clock prefers the device default; fallback uses the first available
    // entry in the physical clock table.
    if (clocks.size() == 1 && result.pins[clocks[0]] < 0) {
        const auto i = clocks[0];
        for (unsigned pin : std::array<unsigned, 4>{35, 11, 13, 33})
            if (!occupied_pins[pin]) {
                occupied_pins[pin] = true;
                result.pins[i]     = static_cast<int>(pin);
                keys[i].fixed_pin  = static_cast<std::int16_t>(pin - 1);
                break;
            }
        if (result.pins[i] < 0)
            unsupported("no free dedicated clock pin");
    }
    // Create global clocks in logical signal order, assigning remaining clocks
    // to the first free physical clock slot before sorting placement signals.
    std::sort(clocks.begin(), clocks.end(), [&](auto a, auto b) { return keys[a].serial < keys[b].serial; });
    for (auto i : clocks)
        if (result.pins[i] < 0) {
            for (unsigned pin : std::array<unsigned, 4>{11, 13, 33, 35})
                if (!occupied_pins[pin]) {
                    occupied_pins[pin] = true;
                    result.pins[i]     = static_cast<int>(pin);
                    keys[i].fixed_pin  = static_cast<std::int16_t>(pin - 1);
                    break;
                }
            if (result.pins[i] < 0)
                unsupported("no free dedicated clock pin");
        }
    // Fixed designs normally skip partition search. An oversubscribed fixed
    // OE half must still run it: DOS rejects this at partition, retaining its
    // last logical frame rather than claiming physical placement occurred.
    std::array<std::set<std::string>, 4> fixed_enables;
    for (const auto& c : clusters)
        if (c.fixed_block >= 0 && c.fixed_half >= 0 && !c.oe_product.empty())
            fixed_enables[unsigned(c.fixed_block) * 2 + unsigned(c.fixed_half)].insert(c.oe_product);
    const bool fixed_oe_overflow = std::any_of(fixed_enables.begin(), fixed_enables.end(),
                                               [](const auto& products) { return products.size() > 2; });
    if (automatic || fixed_oe_overflow) {
        for (auto i : outputs)
            if (signals[i].oe_members && signals[i].oe_product.empty())
                unsupported("automatic OE placement needs the normalized product");
        rank_mach111_single_group(clusters);
        std::array<unsigned, 2> capacity = {16, 16};
        std::array<bool, 45> output_pins{};
        for (auto i : outputs)
            if (result.pins[i] >= 0)
                output_pins[result.pins[i]] = true;
        std::array<bool, 45> counted{};
        for (std::size_t i = 0; i < signals.size(); ++i)
            if (!signals[i].output && result.pins[i] >= 0) {
                const auto pin = static_cast<unsigned>(result.pins[i]);
                const int cell = cell_for_pin(pin);
                if (cell >= 0 && !output_pins[pin] && !counted[pin]) {
                    --capacity[cell / 16];
                    counted[pin] = true;
                }
            }
        std::vector<std::vector<std::string>> oe_order;
        std::vector<unsigned> implicit_controls;
        const auto partition = partition_mach_family_first_success(
            mach_device_layout("MACH111"), clusters, {capacity[0], capacity[1]}, options.balanced_partition, nullptr,
            nullptr, nullptr, &implicit_controls, options.iteration, &oe_order, &result.partition_product_groups,
            options.partition_observer
                ? PartitionObserver([&](const auto& frame) { options.partition_observer(signals, outputs, frame); })
                : PartitionObserver{});
        for (unsigned b = 0; b < 2; ++b)
            partition_oe_order[b] = std::move(oe_order[b]);
        for (unsigned c = 0; c < outputs.size(); ++c)
            result.implicit_controls[outputs[c]] = implicit_controls[c];
        for (unsigned b = 0; b < 2; ++b)
            for (auto c : partition[b])
                if (result.cells[outputs[c]] < 0)
                    result.cells[outputs[c]] = static_cast<int>(16 * b);
    }
    // Placement fanout counts destination blocks; partition ranking instead
    // sums equation fanouts. Destination counts require final block membership.
    for (std::size_t i = 0; i < signals.size(); ++i)
        if (keys[i].type != 9) {
            std::array<bool, 2> destinations{};
            for (auto o : outputs) {
                const auto& fanins = signals[o].fanins;
                if (std::find(fanins.begin(), fanins.end(), static_cast<std::uint16_t>(keys[i].serial)) != fanins.end())
                    destinations[static_cast<unsigned>(result.cells[o]) / 16] = true;
            }
            keys[i].fanout_count = static_cast<std::int16_t>(destinations[0] + destinations[1]);
            if (!signals[i].output && result.pins[i] >= 0) {
                const int cell = cell_for_pin(static_cast<unsigned>(result.pins[i]));
                if (cell >= 0)
                    keys[i].block = static_cast<std::int16_t>(cell / 16);
            }
        }
    result.global_order = fitter_signal_order(keys, false);
    for (auto i : result.global_order)
        if (signals[i].output)
            result.output_order[static_cast<unsigned>(result.cells[i]) / 16].push_back(i);
    constexpr std::array<unsigned, 16> spread = {0, 3, 6, 9, 12, 15, 1, 4, 7, 10, 13, 2, 5, 8, 11, 14};
    std::vector<bool> remaining_candidates(signals.size());
    int retry_signal = -1;
    auto finish      = [&]() {
        const auto saved      = result;
        const auto saved_pins = occupied_pins;
        auto restore          = [&]() {
            result        = saved;
            occupied_pins = saved_pins;
        };
        // Physical output sorting replaces only output slots. Inputs retain their
        // global order and eligible physical pins are scanned in ascending order.
        std::array<std::size_t, 2> cursor{};
        for (auto& i : result.global_order)
            if (signals[i].output) {
                const auto block = static_cast<unsigned>(result.cells[i]) / 16;
                i                = result.output_order[block][cursor[block]++];
            }
        for (auto i : result.global_order) {
            const auto& s = signals[i];
            if (s.output || result.pins[i] >= 0)
                continue;
            if (s.order.type != 1 && s.order.type != 9)
                unsupported("unsupported automatic input class for " + s.name);
            for (unsigned pin = 1; pin <= 44; ++pin) {
                if (occupied_pins[pin] || !(s.order.type == 9 ? clock_pin(pin) : data_pin(pin)))
                    continue;
                if (s.order.block >= 0) {
                    const auto cell = cell_for_pin(pin);
                    if (cell < 0 || cell / 16 != s.order.block)
                        continue;
                }
                occupied_pins[pin] = true;
                result.pins[i]     = static_cast<int>(pin);
                break;
            }
            if (result.pins[i] < 0) {
                restore();
                return false;
            }
        }
        if (route) {
            const auto feedback = route(result, remaining_candidates);
            if (!feedback.success) {
                retry_signal = feedback.retry_signal;
                restore();
                return false;
            }
        }
        return true;
    };
    const auto partition_order = result.output_order;
    result.oe_keys.assign(signals.size(), -1);
    for (const auto& order : partition_order) {
        std::map<std::string, int> oe_key_ids;
        for (auto i : order)
            if (!signals[i].oe_product.empty()) {
                const auto& key = signals[i].oe_product;
                if (!oe_key_ids.count(key))
                    oe_key_ids[key] = int(oe_key_ids.size());
                result.oe_keys[i] = oe_key_ids.at(key);
            }
    }
    std::vector<OutputSearchCallbacks> search_operations(signals.size());
    std::vector<std::function<void()>> publish(2);
    std::optional<Mach111Placement> failed_frame;
    std::vector<bool> failed_assigned;
    unsigned failed_block                        = 0;
    bool failed_complete                         = false;
    int failed_cell_block                        = -1;
    std::function<bool(unsigned)> prepare_blocks = [&](unsigned b) {
        if (b == 2) {
            std::vector<std::size_t> global;
            std::array<std::size_t, 2> cursor{};
            for (auto i : result.global_order)
                if (signals[i].output) {
                    const auto block = unsigned(result.cells[i]) / 16;
                    global.push_back(result.output_order[block][cursor[block]++]);
                }
            const std::vector<std::vector<std::size_t>> blocks(result.output_order.begin(), result.output_order.end());
            if (options.collect_report_visits) {
                auto extra = options.report_extra_records;
                for (auto& key : extra) {
                    std::set<unsigned> destinations;
                    for (std::size_t i = 0; i < signals.size(); ++i)
                        if (signals[i].output && std::find(signals[i].fanins.begin(), signals[i].fanins.end(),
                                                           std::uint16_t(key.serial)) != signals[i].fanins.end())
                            destinations.insert(unsigned(result.cells[i]) / 16);
                    key.fanout_count = std::int16_t(destinations.size());
                }
                result.reverse_visit_costs = placement_reverse_visit_costs(keys, extra, blocks);
                begin_fitting_logical_walk(unsigned(keys.size() + extra.size()));
            }
            return search_output_candidates(
                blocks, global, search_operations,
                [&](const std::vector<bool>& remaining) {
                    remaining_candidates = remaining;
                    retry_signal         = -1;
                    for (auto& write : publish)
                        write();
                    const auto success = finish();
                    return PlacementRouteFeedback{success, retry_signal};
                },
                result.reverse_visit_costs,
                [&](unsigned block) {
                    for (auto id : result.output_order[block])
                        if (!signals[id].buried) {
                            const auto pin = result.pins[id];
                            for (std::size_t other = 0; other < signals.size(); ++other)
                                if (other != id && !signals[other].output && result.pins[other] == pin)
                                    return false;
                        }
                    return true;
                },
                [&](unsigned block, const std::vector<bool>& assigned, int signal) {
                    if (!failure)
                        return;
                    if (std::none_of(assigned.begin(), assigned.end(), [](bool value) { return value; }))
                        return;
                    const bool complete =
                        std::all_of(result.output_order[block].begin(), result.output_order[block].end(),
                                    [&](auto id) { return assigned[id]; });
                    const bool empty =
                        std::none_of(result.output_order[block].begin(), result.output_order[block].end(),
                                     [&](auto id) { return assigned[id]; });
                    const bool preserve =
                        failed_frame && failed_block == block && ((failed_complete && !complete) || empty);
                    const auto saved = result;
                    for (auto& write : publish)
                        write();
                    if (preserve) {
                        // Initial cell allocation continues in later blocks
                        // even when this block has failed. Retain its last
                        // useful prefix while publishing those later blocks.
                        for (unsigned other = 0; other < 2; ++other)
                            if (other != block) {
                                for (auto id : result.output_order[other]) {
                                    failed_frame->cells[id]    = result.cells[id];
                                    failed_frame->pins[id]     = result.pins[id];
                                    failed_frame->oe_slots[id] = result.oe_slots[id];
                                    failed_assigned[id]        = assigned[id];
                                }
                                failed_frame->oe_products[other]       = result.oe_products[other];
                                failed_frame->sequential_search[other] = result.sequential_search[other];
                                for (unsigned group = other * 16; group < (other + 1) * 16; ++group)
                                    failed_frame->product_group_owner[group] = result.product_group_owner[group];
                            }
                        failed_frame->last_oe_constraint = result.last_oe_constraint;
                    } else {
                        failed_complete                     = complete;
                        failed_frame                        = result;
                        failed_assigned                     = assigned;
                        failed_block                        = block;
                        failed_frame->failed_signal         = signal;
                        failed_frame->failed_pad_assignment = signal < 0;
                    }
                    result = saved;
                },
                &failed_cell_block);
        }
        ProductAllocator products;
        // Fixed constraints reserve pins, but macrocells are allocated in
        // the same sorted walk as automatic signals, not all in advance.
        std::array<bool, 16> placed{};
        auto original = partition_order[b];
        std::array<std::string, 4> oe_products;
        std::array<unsigned, 4> oe_references{};
        const std::string device = "MACH111";
        const auto& geometry     = mach_device_layout(device);
        const auto oe_slot       = [&](unsigned cell, const std::string& product) {
            if (product.empty())
                return -1;
            result.last_oe_constraint =
                std::array<int, 2>{geometry.oe_constraint_indices.at(b), geometry.oe_constraint_count};
            const unsigned start = (cell % 16 < 8 ? 0 : 2);
            int free             = -1;
            for (unsigned slot = start; slot < start + 2; ++slot) {
                if (oe_products[slot] == product)
                    return static_cast<int>(slot);
                if (oe_products[slot].empty() && free < 0)
                    free = static_cast<int>(slot);
            }
            return free;
        };
        // Reserve fixed output enables before sorting physical outputs.
        // Assign automatic enables only when committing a candidate.
        for (auto i : original)
            if (signals[i].order.fixed_pin >= 0 && !signals[i].oe_product.empty()) {
                const int slot = oe_slot(static_cast<unsigned>(result.cells[i]), signals[i].oe_product);
                if (slot < 0) {
                    unsupported("fixed output-enable products exceed half-block capacity");
                }
                oe_products[slot]  = signals[i].oe_product;
                result.oe_slots[i] = slot;
                ++oe_references[slot];
            }
        result.oe_products[b] = partition_oe(oe_products, partition_oe_order[b], signals, original);
        std::vector<PhysicalOutputOrderKey> physical_keys;
        for (auto i : original) {
            const auto& s       = signals[i];
            unsigned oe_members = s.oe_members;
            if (!s.oe_product.empty()) {
                oe_members = 0;
                for (auto j : original)
                    if (signals[j].oe_product == s.oe_product)
                        ++oe_members;
            }
            physical_keys.push_back({s.sort_name.empty() ? s.name : s.sort_name, s.order.fixed_node >= 0,
                                     s.order.fixed_pin >= 0, oe_members, s.order.product_terms, keys[i].fanout_count,
                                     s.xor_product});
        }
        result.output_order[b].clear();
        for (auto index : physical_output_order(physical_keys))
            result.output_order[b].push_back(original[index]);
        unsigned spread_retries = 10;
        bool spread_enabled     = options.spread_placement;
        publish[b]              = [&]() {
            for (unsigned slot = 0; slot < 4; ++slot)
                if (!oe_products[slot].empty())
                    result.oe_products[b][slot] = oe_products[slot];
            for (unsigned group = 0; group < 16; ++group)
                result.product_group_owner[16 * b + group] =
                    products.owner[group] < 0 ? -1 : int(16 * b) + products.owner[group];
        };
        struct Previous
        {
            bool pin = false, cell = false;
            int location = -1, pin_number = -1, slot = -1;
        };
        std::map<std::size_t, Previous> previous;
        for (auto i : result.output_order[b]) {
            search_operations[i].candidates = [&, i]() {
                const bool fixed = signals[i].order.fixed_pin >= 0 || signals[i].order.fixed_node >= 0;
                std::vector<unsigned> candidates;
                bool physically_available = false;
                for (unsigned candidate = 0; candidate < 16; ++candidate) {
                    const unsigned cell = 16 * b + (spread_enabled ? spread[candidate] : candidate);
                    if (placed[cell % 16])
                        continue;
                    if (fixed) {
                        if (cell != static_cast<unsigned>(result.cells[i]))
                            continue;
                    } else if (occupied_cells[cell])
                        continue;
                    // Test every candidate before selecting one. Remove only the trial
                    // signal: successful displaced-owner assignments persist.
                    if (products.allocate(static_cast<int>(cell % 16), signals[i].order.product_terms)) {
                        physically_available = true;
                        // Check output-enable capacity after the product allocation trial. Even
                        // a rejected candidate can retain successful product-owner displacements.
                        if (signals[i].oe_product.empty() || oe_slot(cell, signals[i].oe_product) >= 0)
                            candidates.push_back(cell);
                        products.remove(static_cast<int>(cell % 16));
                    }
                }
                // After ten exhausted spread retries, generate new candidate lists in
                // ascending cell order. Existing lists retain their previous order.
                if (candidates.empty() && physically_available) {
                    if (spread_retries)
                        --spread_retries;
                    else {
                        spread_enabled              = false;
                        result.sequential_search[b] = true;
                    }
                }
                return candidates;
            };
            search_operations[i].commit = [&, i](unsigned cell) {
                if (!products.allocate(int(cell % 16), signals[i].order.product_terms))
                    return false;
                const unsigned pin = mach111_io_pin(cell);
                Previous old{occupied_pins[pin], occupied_cells[cell], result.cells[i], result.pins[i], -1};
                if (!signals[i].oe_product.empty()) {
                    const int slot = oe_slot(cell, signals[i].oe_product);
                    if (slot < 0) {
                        products.remove(int(cell % 16));
                        return false;
                    }
                    oe_products[slot]  = signals[i].oe_product;
                    result.oe_slots[i] = slot;
                    if (signals[i].order.fixed_pin < 0) {
                        ++oe_references[slot];
                        old.slot = slot;
                    }
                }
                previous[i]          = old;
                placed[cell % 16]    = true;
                occupied_cells[cell] = true;
                if (!signals[i].buried) {
                    occupied_pins[pin] = true;
                    result.pins[i]     = int(pin);
                }
                result.cells[i] = int(cell);
                return true;
            };
            search_operations[i].remove = [&, i]() {
                const auto cell = unsigned(result.cells[i]), pin = mach111_io_pin(cell);
                const auto old = previous.at(i);
                products.remove(int(cell % 16));
                placed[cell % 16]    = false;
                occupied_cells[cell] = old.cell;
                occupied_pins[pin]   = old.pin;
                result.cells[i]      = old.location;
                result.pins[i]       = old.pin_number;
                if (old.slot >= 0) {
                    if (--oe_references[unsigned(old.slot)] == 0)
                        oe_products[unsigned(old.slot)].clear();
                    result.oe_slots[i] = -1;
                }
            };
        }
        return prepare_blocks(b + 1);
    };
    if (!prepare_blocks(0)) {
        if (failed_cell_block >= 0)
            options.iteration->reserve_product_cluster(unsigned(failed_cell_block));
        if (failure && failed_frame)
            failure(*failed_frame, failed_block, failed_assigned);
        throw PhysicalPlacementFailure("MACH111 placement: output candidate search exhausted");
    }
    return result;
}
}  // namespace machxl
