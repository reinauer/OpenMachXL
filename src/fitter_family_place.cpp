#include "fitter_family_place.hpp"
#include "fitter_budget.hpp"
#include "fitter_iteration.hpp"
#include "fitter_output_search.hpp"
#include <algorithm>
#include <functional>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
namespace machxl {
namespace {
using Clusters = std::vector<Mach111PartitionCluster>;
// Modern control resources retain their global row until the last reference
// leaves, including references added or removed during trial swaps.
struct ControlSlot
{
    std::optional<std::uint16_t> key;
    unsigned count = 0;
    bool add(std::uint16_t incoming)
    {
        if (key && *key != incoming)
            return false;
        key = incoming;
        ++count;
        return true;
    }
    bool remove(std::uint16_t incoming)
    {
        if (!key || *key != incoming)
            return false;
        if (--count == 0)
            key.reset();
        return true;
    }
};
struct Block
{
    // The member clusters. They change only through push_back() and erase()
    // so that the resource bookkeeping below cannot be bypassed.
    using Members = std::vector<std::size_t>;
    Members::const_iterator begin() const { return members_.begin(); }
    Members::const_iterator end() const { return members_.end(); }
    std::size_t size() const { return members_.size(); }
    bool empty() const { return members_.empty(); }
    std::size_t operator[](std::size_t index) const { return members_[index]; }
    std::size_t back() const { return members_.back(); }
    void erase(Members::const_iterator at) { members_.erase(at); }
    const Members& members() const { return members_; }
    Clusters* clusters               = nullptr;
    bool legacy_oe                   = false;
    unsigned legacy_oe_capacity      = 0;
    unsigned reserved_product_groups = 0;
    bool fixed_product_balance       = false;
    int product_balance              = 0;
    std::map<std::size_t, unsigned> product_charges;
    unsigned product_flags(const Mach111PartitionCluster& c) const
    {
        return c.local_clock || c.force_async || !shared_controls(c) || product_clock(c) ? c.async_product_flags
                                                                                         : c.sync_product_flags;
    }
    unsigned requested_product_groups(const Mach111PartitionCluster& c) const
    {
        if (c.asynchronous_product_groups &&
            (c.local_clock || c.force_async || !shared_controls(c) || product_clock(c)))
            return *c.asynchronous_product_groups;
        return c.product_groups;
    }
    unsigned product_charge(const Mach111PartitionCluster& c) const
    {
        const auto groups = requested_product_groups(c);
        if (!fixed_product_balance || c.fixed_block < 0 || !groups)
            return groups;
        const auto flags = product_flags(c);
        return groups - unsigned(((flags & 0x100) && product_balance < 0) || ((flags & 1) && product_balance > 0));
    }
    struct OeSlot
    {
        std::string key;
        unsigned count = 0;
    };
    std::array<OeSlot, 4> oe_resources;

    std::vector<std::string> oe_order;
    std::array<ControlSlot, 2> controls;
    std::array<ControlSlot, 4> clocks;
    std::set<std::size_t> clock_members, product_clock_members;
    // Members whose set or reset found no room in the two shared rows when
    // they were added. They use their own term and hold no row, also after
    // a row becomes free or another member brings the same term.
    std::set<std::size_t> local_control_members;
    bool shared_controls(const Mach111PartitionCluster& c) const
    {
        auto accepts = [&](const auto& value, unsigned index) {
            return !value || !controls[index].key || *value == *controls[index].key;
        };
        return (accepts(c.global_reset, 0) && accepts(c.global_preset, 1)) ||
               (accepts(c.global_reset, 1) && accepts(c.global_preset, 0));
    }
    bool clock_conflict(const Mach111PartitionCluster& c) const
    {
        if (c.global_clock < 0 || c.local_clock)
            return false;
        const auto& slot = clocks[unsigned(c.global_clock ^ (c.clock_positive ? 0 : 1))];
        return slot.key && *slot.key != unsigned(c.clock_positive);
    }
    bool product_clock(const Mach111PartitionCluster& c) const
    {
        return c.global_clock >= 0 && !c.local_clock &&
               (clock_conflict(c) || ((c.force_async || !shared_controls(c)) && c.global_clock >= 2));
    }
    void push_back(std::size_t id)
    {
        members_.push_back(id);
        if (!clusters)
            return;
        auto& c             = (*clusters)[id];
        product_charges[id] = product_charge(c);
        if (fixed_product_balance && c.fixed_block >= 0 && requested_product_groups(c)) {
            const auto flags = product_flags(c);
            // Dual-capable groups consume a positive balance and supply a negative
            // balance, including zero ties.
            if ((flags & 0x101) == 0x101) {
                // The commit differs from the precheck for a positive
                // balance: a dual-capable descriptor receives no refund.
                if (product_balance >= 0)
                    product_charges[id] = requested_product_groups(c);
                product_balance += product_balance < 0 ? 1 : -1;
            } else if (flags & 0x100)
                ++product_balance;
            else if (flags & 1)
                --product_balance;
        }
        if (!c.oe_product.empty())
            for (unsigned slot = 0; slot < legacy_oe_capacity; ++slot) {
                auto& resource = oe_resources[slot];
                // Stop at the first hole, even if a matching key exists later.
                // Duplicate records therefore remain significant.
                if (resource.key.empty() || resource.key == c.oe_product) {
                    resource.key = c.oe_product;
                    ++resource.count;
                    break;
                }
            }

        if (legacy_oe && c.fixed_half < 0 && !c.oe_product.empty() &&
            std::find(oe_order.begin(), oe_order.end(), c.oe_product) == oe_order.end())
            oe_order.push_back(c.oe_product);
        if (c.global_clock >= 0 && !c.local_clock) {
            const unsigned slot = unsigned(c.global_clock ^ (c.clock_positive ? 0 : 1));
            if (c.allow_global_product_clocks && product_clock(c)) {
                product_clock_members.insert(id);
                // Retain the new clock fanin on the cluster, including when a later
                // placement or partition trial moves it.
                if (c.clock_fanin && std::find(c.fanins.begin(), c.fanins.end(), *c.clock_fanin) == c.fanins.end())
                    c.fanins.push_back(*c.clock_fanin);
            } else if (clocks[slot].add(std::uint16_t(c.clock_positive)))
                clock_members.insert(id);
        }
        auto add = [&](unsigned primary, std::uint16_t key, bool has_other) {
            const unsigned alternate = 1 - primary;
            const unsigned first =
                !controls[primary].key && !has_other && controls[alternate].key ? alternate : primary;
            if (!controls[first].add(key))
                controls[1 - first].add(key);
        };
        if (!shared_controls(c)) {
            local_control_members.insert(id);
            return;
        }
        if (c.global_reset)
            add(0, *c.global_reset, bool(c.global_preset));
        if (c.global_preset)
            add(1, *c.global_preset, bool(c.global_reset));
    }
    void remove_control(std::size_t id)
    {
        if (!clusters)
            return;
        const auto& c = (*clusters)[id];
        product_charges.erase(id);
        if (!c.oe_product.empty())
            for (unsigned slot = 0; slot < legacy_oe_capacity; ++slot) {
                auto& resource = oe_resources[slot];
                if (resource.key == c.oe_product) {
                    if (--resource.count == 0)
                        resource.key.clear();
                    break;
                }
            }

        if (legacy_oe && c.fixed_half < 0 && !c.oe_product.empty() &&
            std::none_of(
                begin(), end(),
                [&](auto other) {
                    return other != id && (*clusters)[other].fixed_half < 0 &&
                           (*clusters)[other].oe_product == c.oe_product;
                })) {
            const auto at = std::find(oe_order.begin(), oe_order.end(), c.oe_product);
            if (at != oe_order.end())
                oe_order.erase(at);
        }
        if (clock_members.erase(id))
            clocks[unsigned(c.global_clock ^ (c.clock_positive ? 0 : 1))].remove(std::uint16_t(c.clock_positive));
        product_clock_members.erase(id);
        if (local_control_members.erase(id))
            return;
        if (c.global_reset && !controls[0].remove(*c.global_reset))
            controls[1].remove(*c.global_reset);
        if (c.global_preset && !controls[1].remove(*c.global_preset))
            controls[0].remove(*c.global_preset);
    }

  private:
    Members members_;
};
[[noreturn]] void fail(const std::string& s)
{
    throw std::runtime_error("MACH1/2 placement: " + s);
}
// MACH1 output-enable capacity arithmetic preserves the DOS zero-capacity shortcut.
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
std::array<std::string, 4> partition_oe(const MachDeviceLayout& d, const std::array<std::string, 4>& fixed,
                                        const std::vector<std::string>& order,
                                        const std::vector<Mach111PlaceSignal>& signals,
                                        const std::vector<std::size_t>& members)
{
    if (order.size() > 4)
        fail("more than four partition OE products");
    std::array<std::string, 4> key;
    std::copy(order.begin(), order.end(), key.begin());
    std::array<int, 4> size{};
    std::array<std::vector<unsigned>, 4> group_sizes;
    std::vector<unsigned> plain;
    const auto half  = int(d.cells / 2);
    int available[2] = {half, half};
    for (auto id : members) {
        const auto& s         = signals[id];
        const unsigned groups = static_cast<unsigned>((s.order.product_terms + 3) / 4);
        if (s.order.fixed_pin >= 0) {
            const auto at = std::find(d.io_pins.begin(), d.io_pins.end(), unsigned(s.order.fixed_pin) + 1);
            if (at == d.io_pins.end())
                fail("fixed OE pin is not an IO pad");
            available[(unsigned(at - d.io_pins.begin()) % d.cells) / unsigned(half)] -= static_cast<int>(groups);
        } else if (s.oe_product.empty())
            plain.push_back(groups);
        else {
            const auto it = std::find(key.begin(), key.end(), s.oe_product);
            if (it == key.end())
                fail("missing partition OE resource group");
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

std::set<std::uint16_t> collect_block_fanins(const Block& block, const Clusters& clusters)
{
    std::set<std::uint16_t> result;
    for (auto i : block)
        result.insert(clusters[i].fanins.begin(), clusters[i].fanins.end());
    return result;
}
unsigned macrocell_count(const Block& block, const Clusters& clusters)
{
    unsigned n = 0;
    for (auto i : block)
        n += clusters[i].macrocell;
    return n;
}
unsigned pin_count(const Block& block, const Clusters& clusters)
{
    unsigned n = 0;
    for (auto i : block)
        n += clusters[i].output;
    return n;
}
bool shared_controls_compatible(const Block& block, const Clusters& c, std::size_t incoming,
                                std::size_t outgoing = std::size_t(-1))
{
    for (auto i : block)
        if (i != outgoing) {
            if (c[incoming].reset && c[i].reset && c[i].reset != c[incoming].reset)
                return false;
            if (c[incoming].preset && c[i].preset && c[i].preset != c[incoming].preset)
                return false;
        }
    return true;
}
// A reset term beside a set term it is contained in, or contains.
bool set_reset_overlap(const Block& block, const Clusters& c, std::size_t incoming)
{
    for (auto i : block)
        if (set_reset_products_related(c[incoming].reset_product, c[i].preset_product) ||
            set_reset_products_related(c[incoming].preset_product, c[i].reset_product))
            return true;
    return false;
}
// Legacy shared output-enable resource tables: four rows on MACH1,
// two on MACH2; MACH215 and modern devices have local output enables.
bool oe_compatible(const Block& block, const Clusters& c, std::size_t incoming, const MachDeviceLayout&,
                   std::size_t outgoing = std::size_t(-1))
{
    if (!block.legacy_oe_capacity || c[incoming].oe_product.empty())
        return true;
    const auto& key = c[incoming].oe_product;
    if (block.legacy_oe && c[incoming].fixed_half >= 0) {
        std::set<std::string> fixed{key};
        for (auto id : block)
            if (id != outgoing && c[id].fixed_half == c[incoming].fixed_half && !c[id].oe_product.empty())
                fixed.insert(c[id].oe_product);
        if (fixed.size() > 2)
            return false;
    }
    if (outgoing == std::size_t(-1)) {
        for (unsigned slot = 0; slot < block.legacy_oe_capacity; ++slot)
            if (block.oe_resources[slot].key.empty() || block.oe_resources[slot].key == key)
                return true;
        return false;
    }
    // Count occupied records after the proposed swap without deduplicating
    // fragmented records carrying the same key.
    unsigned used = 0;
    for (unsigned slot = 0; slot < block.legacy_oe_capacity; ++slot) {
        const auto& resource = block.oe_resources[slot];
        if (resource.key.empty() || resource.key == key)
            continue;
        if (resource.key == c[outgoing].oe_product)
            used += resource.count != 1;
        else
            used += resource.count != 0;
    }
    return used < block.legacy_oe_capacity;
}
// Permit local reset/set only when its clock remains available in
// asynchronous mode. Global-clock-only policy forbids conversion to product
// clocks; asynchronous cells can then select only slots 0/1. Explicit zero
// controls still occupy slots.
unsigned modern_control_violations(const Block& block, const Clusters& clusters, std::size_t incoming)
{
    const auto& c             = clusters[incoming];
    const bool conflict       = block.clock_conflict(c);
    const bool requires_local = c.local_clock || c.force_async || !block.shared_controls(c) || conflict;
    unsigned violations       = 0;
    if (requires_local && c.force_sync)
        ++violations;
    if (requires_local && c.reset.value_or(0) != 0 && c.preset.value_or(0) != 0)
        ++violations;
    if (requires_local && c.global_clock >= 2 && !c.allow_global_product_clocks)
        ++violations;
    if (conflict && (!c.allow_global_product_clocks || !c.macrocell))
        ++violations;
    return violations;
}
unsigned product_groups(const Block& block, const Clusters& clusters)
{
    unsigned n = block.reserved_product_groups;
    for (auto i : block) {
        const auto at = block.product_charges.find(i);
        n += at == block.product_charges.end() ? clusters[i].product_groups : at->second;
    }
    return n;
}
bool block_accepts_cluster(const Block& block, const Clusters& c, std::size_t incoming, const MachDeviceLayout& d,
                           unsigned capacity)
{
    if (macrocell_count(block, c) + c[incoming].macrocell > (d.device == "MACH215" ? d.cells / 2 : d.cells) ||
        pin_count(block, c) + c[incoming].output > capacity ||
        (!d.one_hot && d.device != "MACH215" &&
         (!shared_controls_compatible(block, c, incoming) || set_reset_overlap(block, c, incoming))))
        return false;
    if (!oe_compatible(block, c, incoming, d))
        return false;
    if (d.one_hot && modern_control_violations(block, c, incoming))
        return false;
    if (product_groups(block, c) + block.product_charge(c[incoming]) > (d.device == "MACH215" ? d.cells / 2 : d.cells))
        return false;
    auto inputs = collect_block_fanins(block, c);
    inputs.insert(c[incoming].fanins.begin(), c[incoming].fanins.end());
    // Account for the extra CSM clock literal before asynchronous fallback.
    if (d.one_hot && block.product_clock(c[incoming]) && c[incoming].clock_fanin)
        inputs.insert(*c[incoming].clock_fanin);
    return inputs.size() <= d.muxes;
}
// Compare the maximum fanin count of the two participating blocks only.
unsigned maximum_pair_fanin_count(const std::vector<Block>& blocks, const Clusters& c, unsigned x, unsigned y)
{
    return unsigned(std::max(collect_block_fanins(blocks[x], c).size(), collect_block_fanins(blocks[y], c).size()));
}
void remove_cluster_from_block(Block& block, std::size_t id)
{
    auto at = std::find(block.begin(), block.end(), id);
    if (at == block.end())
        throw std::logic_error("family partition membership invariant");
    block.remove_control(id);
    block.erase(at);
}
unsigned violations(const Block& block, const Clusters& c, std::size_t incoming, const MachDeviceLayout& d,
                    unsigned capacity)
{
    const unsigned cells = d.device == "MACH215" ? d.cells / 2 : d.cells;
    unsigned result      = macrocell_count(block, c) + c[incoming].macrocell > cells;
    result += pin_count(block, c) + c[incoming].output > capacity;
    result += product_groups(block, c) + block.product_charge(c[incoming]) > cells;
    result += !d.one_hot && d.device != "MACH215" &&
              (!shared_controls_compatible(block, c, incoming) || set_reset_overlap(block, c, incoming));
    result += !oe_compatible(block, c, incoming, d);
    if (d.one_hot) {
        result += modern_control_violations(block, c, incoming);
        // A set or reset that the two shared rows cannot take does not keep
        // the cluster out, since it can use a term of its own, but it counts
        // against the block when the rescue orders the blocks.
        result += !block.shared_controls(c[incoming]);
    }
    auto inputs = collect_block_fanins(block, c);
    inputs.insert(c[incoming].fanins.begin(), c[incoming].fanins.end());
    if (d.one_hot && block.product_clock(c[incoming]) && c[incoming].clock_fanin)
        inputs.insert(*c[incoming].clock_fanin);
    return result + (inputs.size() > d.muxes);
}
int select(const std::vector<Block>& blocks, const Clusters& c, std::size_t incoming, const MachDeviceLayout& d,
           const std::vector<unsigned>& capacity, bool balanced, int excluded = -1)
{
    int chosen           = -1;
    unsigned best_shared = 0, best_inputs = 0;
    for (unsigned b = 0; b < d.blocks; ++b) {
        if (int(b) == excluded || !block_accepts_cluster(blocks[b], c, incoming, d, capacity[b]))
            continue;
        const auto inputs = collect_block_fanins(blocks[b], c);
        unsigned shared   = 0;
        for (auto input : c[incoming].fanins)
            shared += inputs.count(input) != 0;
        bool better = chosen < 0;
        if (!better) {
            // MACH215 leaves the remaining-cell count unchanged, so balanced size
            // comparisons always tie. Other MACH2 devices count input-register clusters
            // as cells; modern devices count only logic and auxiliary product groups.
            const auto size = [&](unsigned block) {
                return d.one_hot ? macrocell_count(blocks[block], c) : unsigned(blocks[block].size());
            };
            if (balanced && d.device != "MACH215" && size(b) != size(unsigned(chosen)))
                better = size(b) < size(unsigned(chosen));
            else if (shared != best_shared)
                better = shared > best_shared;
            else
                better = best_inputs == d.muxes || best_inputs > inputs.size();
        }
        if (better) {
            chosen      = int(b);
            best_shared = shared;
            best_inputs = unsigned(inputs.size());
        }
    }
    return chosen;
}
bool swap_resources(const std::vector<Block>& blocks, const Clusters& c, unsigned x, std::size_t a, unsigned y,
                    std::size_t b, const MachDeviceLayout& d, const std::vector<unsigned>& capacity)
{
    // On MACH3/4 both swapped clusters must carry an input-clock record, or
    // neither. An input-register pair has one, and so has the cluster of an
    // output pad that feeds an input register; those two kinds are exchanged
    // with each other. MACH2 exchanges an input register with a macrocell
    // cluster as well, which is why its input registers end up in the earlier
    // blocks.
    const auto input_clock_record = [&](std::size_t cluster) {
        return !c[cluster].macrocell || c[cluster].input_register_pad;
    };
    if (d.one_hot && input_clock_record(a) != input_clock_record(b))
        return false;
    if (d.one_hot) {
        // Check control rows before removing either node. If an incoming global
        // clock requires asynchronous fallback, the outgoing asynchronous node must
        // also have a global clock. A local product clock does not qualify. Checking
        // after removal would change list order when the outgoing node is restored.
        const auto missing_clock_record = [&](unsigned block, std::size_t old, std::size_t incoming) {
            return c[old].local_clock && c[incoming].global_clock >= 0 && !c[incoming].local_clock &&
                   !blocks[block].shared_controls(c[incoming]);
        };
        // This holds only for an incoming cluster whose set or reset is the
        // grounded constant. One with a real term, or with none, may take
        // the place of a locally clocked cluster and fall back to an
        // asynchronous macrocell there.
        const auto grounded = [&](std::size_t incoming) {
            return (c[incoming].global_reset && *c[incoming].global_reset == 0) ||
                   (c[incoming].global_preset && *c[incoming].global_preset == 0);
        };
        if ((grounded(b) && missing_clock_record(x, a, b)) || (grounded(a) && missing_clock_record(y, b, a)))
            return false;
    }
    if (d.one_hot) {
        // Two clusters on pin clocks: when the receiving block still uses all
        // of its clock lines once the outgoing cluster is gone, and none of
        // them carries the incoming clock, the exchange is refused here. With
        // a line to spare it goes on to the trial, which then fails on the
        // clock and puts both clusters back at the end of their lists.
        const auto pin_clocked = [&](std::size_t id) {
            return c[id].global_clock >= 0 && !c[id].local_clock && !c[id].force_async;
        };
        const auto line_of = [&](std::size_t id) {
            return unsigned(c[id].global_clock ^ (c[id].clock_positive ? 0 : 1));
        };
        const auto no_clock_line = [&](unsigned block, std::size_t outgoing, std::size_t incoming) {
            const auto& lines   = blocks[block].clocks;
            const bool releases = blocks[block].clock_members.count(outgoing) != 0;
            for (unsigned line = 0; line < lines.size(); ++line)
                if (!lines[line].key || (releases && line == line_of(outgoing) && lines[line].count == 1))
                    return false;
            return *lines[line_of(incoming)].key != unsigned(c[incoming].clock_positive);
        };
        if (pin_clocked(a) && pin_clocked(b) && (no_clock_line(x, a, b) || no_clock_line(y, b, a)))
            return false;
    }
    // Check the outgoing synchronous resources before removal. Forced
    // asynchronous cells with a retained global clock still have synchronous
    // block membership; product-clock membership selects asynchronous resources
    // and can participate in the swap.
    if (d.one_hot && ((c[a].force_async && !c[a].local_clock && !blocks[x].product_clock_members.count(a)) ||
                      (c[b].force_async && !c[b].local_clock && !blocks[y].product_clock_members.count(b))))
        return false;
    // MACH215 swap prechecks use zero-capacity shared control tables. Any
    // incoming OE, reset or preset therefore rejects the swap, including
    // constants, although ordinary insertion uses local controls. Unspecified
    // register controls become explicit GND before partitioning and also reject.
    if (d.device == "MACH215" && (c[a].explicit_async_control || c[b].explicit_async_control ||
                                  c[a].implicit_storage_controls || c[b].implicit_storage_controls))
        return false;
    if (!d.one_hot && d.device != "MACH215" &&
        (!shared_controls_compatible(blocks[x], c, b, a) || !shared_controls_compatible(blocks[y], c, a, b)))
        return false;
    if (!oe_compatible(blocks[x], c, b, d, a) || !oe_compatible(blocks[y], c, a, d, b))
        return false;
    if (pin_count(blocks[x], c) - c[a].output + c[b].output > capacity[x] ||
        pin_count(blocks[y], c) - c[b].output + c[a].output > capacity[y])
        return false;
    const unsigned groups = d.device == "MACH215" ? d.cells / 2 : d.cells;
    if (macrocell_count(blocks[x], c) - c[a].macrocell + c[b].macrocell > groups ||
        macrocell_count(blocks[y], c) - c[b].macrocell + c[a].macrocell > groups)
        return false;
    // Take out what the outgoing cluster is charged in its block, which is
    // what the block total holds, and add what the incoming one would be
    // charged there. Using the clusters' base counts instead could subtract
    // more than the total holds and wrap.
    const auto charged = [&](unsigned block, std::size_t member) {
        const auto at = blocks[block].product_charges.find(member);
        return at == blocks[block].product_charges.end() ? c[member].product_groups : at->second;
    };
    return product_groups(blocks[x], c) - charged(x, a) + blocks[x].product_charge(c[b]) <= groups &&
           product_groups(blocks[y], c) - charged(y, b) + blocks[y].product_charge(c[a]) <= groups;
}
bool rescue(std::vector<Block>& blocks, const Clusters& c, std::size_t incoming, const MachDeviceLayout& d,
            const std::vector<unsigned>& capacity, bool balanced)
{
    // Preserve incoming resource-failure masks across all trials.
    std::vector<unsigned> failures;
    for (unsigned b = 0; b < d.blocks; ++b)
        failures.push_back(violations(blocks[b], c, incoming, d, capacity[b]));
    const auto order = original_unsigned_order(failures);
    for (unsigned pass = 0; pass < d.blocks; ++pass) {
        // Select the best destination with the current source excluded.
        // Retain successful moves even if the incoming cluster still does not fit.
        for (auto x : order)
            for (std::size_t pos = blocks[x].size(); pos > 0; --pos) {
                const auto a = blocks[x][pos - 1];
                if (c[a].fixed_block >= 0)
                    continue;
                const int y = select(blocks, c, a, d, capacity, balanced, int(x));
                if (y < 0)
                    continue;
                blocks[y].push_back(a);
                remove_cluster_from_block(blocks[x], a);
                const int target = select(blocks, c, incoming, d, capacity, balanced);
                if (target >= 0) {
                    blocks[target].push_back(incoming);
                    return true;
                }
            }
        // Traverse sorted blocks, reverse members, later sorted blocks and
        // their reverse members, reading the mutable lists.
        for (std::size_t xi = 0; xi < order.size(); ++xi) {
            const auto x = order[xi];
            for (std::size_t ai = blocks[x].size(); ai > 0; --ai) {
                if (c[blocks[x][ai - 1]].fixed_block >= 0)
                    break;
                for (std::size_t yi = xi + 1; yi < order.size(); ++yi) {
                    const auto y = order[yi];
                    for (std::size_t bi = blocks[y].size(); bi > 0; --bi) {
                        const auto a = blocks[x][ai - 1], b = blocks[y][bi - 1];
                        if (c[b].fixed_block >= 0)
                            break;
                        if (!swap_resources(blocks, c, unsigned(x), a, unsigned(y), b, d, capacity))
                            continue;
                        const auto before =
                            collect_block_fanins(blocks[x], c).size() + collect_block_fanins(blocks[y], c).size();
                        remove_cluster_from_block(blocks[x], a);
                        if (!block_accepts_cluster(blocks[x], c, b, d, capacity[x])) {
                            blocks[x].push_back(a);
                            continue;
                        }
                        blocks[x].push_back(b);
                        remove_cluster_from_block(blocks[y], b);
                        if (!block_accepts_cluster(blocks[y], c, a, d, capacity[y])) {
                            remove_cluster_from_block(blocks[x], b);
                            blocks[x].push_back(a);
                            blocks[y].push_back(b);
                            continue;
                        }
                        blocks[y].push_back(a);
                        if (block_accepts_cluster(blocks[x], c, incoming, d, capacity[x])) {
                            blocks[x].push_back(incoming);
                            return true;
                        }
                        if (block_accepts_cluster(blocks[y], c, incoming, d, capacity[y])) {
                            blocks[y].push_back(incoming);
                            return true;
                        }
                        // Rescue compares the sum of the two fanin counts; optimization later
                        // compares their maximum.
                        if (before <
                            collect_block_fanins(blocks[x], c).size() + collect_block_fanins(blocks[y], c).size()) {
                            remove_cluster_from_block(blocks[y], a);
                            remove_cluster_from_block(blocks[x], b);
                            blocks[x].push_back(a);
                            blocks[y].push_back(b);
                        }
                    }
                }
            }
        }
    }
    return false;
}
std::vector<Block> partition(const MachDeviceLayout& d, Clusters clusters, const std::vector<unsigned>& capacity,
                             bool balanced, std::vector<GlobalControlSlots>* controls,
                             std::vector<std::size_t>* final_order, std::vector<bool>* product_clocks,
                             std::vector<std::vector<std::string>>* oe_order = nullptr,
                             std::vector<unsigned>* implicit_controls        = nullptr,
                             PartitionIterationState* iteration              = nullptr,
                             std::vector<unsigned>* used_product_groups      = nullptr,
                             const PartitionObserver& observer               = {})
{
    if (d.device == "MACH215")
        for (auto& c : clusters)
            c.implicit_storage_controls = c.macrocell && c.clocked_storage;
    const auto original_clusters = clusters;
    const bool resumed           = iteration && iteration->completed;
    if (resumed) {
        if (iteration->grounded.size() != clusters.size())
            throw std::logic_error("partition iteration cluster shape");
        for (std::size_t j = 0; j < clusters.size(); ++j) {
            if (iteration->grounded[j] & 1)
                clusters[j].reset = clusters[j].global_reset = 0;
            if (iteration->grounded[j] & 2)
                clusters[j].preset = clusters[j].global_preset = 0;
            if (iteration->auxiliary_retry && (clusters[j].async_product_flags & 0x1000))
                clusters[j].async_product_flags |= 1;
        }
        rank_mach_modern_phase(clusters, iteration->phase, iteration->best_product_pressure,
                               iteration->grounded_implicit);
    } else
        rank_mach111_single_group(clusters);
    std::vector<PartitionOrderKey> keys;
    for (const auto& c : clusters)
        keys.push_back(c.order);
    auto order = partition_signal_order(keys);
    std::vector<Block> blocks(d.blocks);
    for (auto& block : blocks) {
        block.clusters           = &clusters;
        block.legacy_oe          = !d.one_hot && d.device[4] == '1';
        block.legacy_oe_capacity = d.one_hot || d.device == "MACH215" ? 0 : d.device[4] == '1' ? 4 : 2;
    }
    std::vector<bool> promoted_predecessor(clusters.size());
    unsigned phase                 = resumed ? iteration->phase : 0;
    bool balanced_mode             = resumed ? iteration->balanced_phase : balanced;
    bool grounded_implicit         = resumed && iteration->grounded_implicit;
    bool auxiliary_retry           = resumed && iteration->auxiliary_retry;
    std::vector<unsigned> grounded = resumed ? iteration->grounded : std::vector<unsigned>(clusters.size());
    std::size_t best_progress      = resumed ? iteration->best_progress : 0;
    bool best_product_pressure     = resumed && iteration->best_product_pressure;
    bool best_fixed_failure        = resumed && iteration->best_fixed_failure;
    std::optional<PartitionObservation> best_observation;
    auto observe = [&](std::optional<std::size_t> failed, unsigned progress) {
        PartitionObservation frame;
        frame.order               = order;
        frame.failed_cluster      = failed;
        frame.progress            = 0;
        frame.fixed_seed_complete = !failed || clusters[*failed].fixed_block < 0;
        (void)progress;
        frame.asynchronous.resize(clusters.size());
        for (const auto& block : blocks) {
            frame.members.emplace_back(block.begin(), block.end());
            frame.progress += unsigned(block.size());
            const auto inputs = collect_block_fanins(block, clusters);
            frame.fanins.emplace_back(inputs.begin(), inputs.end());
            frame.product_groups.push_back(product_groups(block, clusters));
            frame.clock_members.emplace_back(block.clock_members.begin(), block.clock_members.end());
            for (auto c : block)
                frame.asynchronous[c] = clusters[c].local_clock || clusters[c].force_async ||
                                        !block.shared_controls(clusters[c]) || block.product_clock(clusters[c]);
            std::vector<std::string> reasons;
            if (failed) {
                const auto& c                 = clusters[*failed];
                const unsigned capacity_index = unsigned(frame.members.size() - 1);
                const auto available          = [&](const auto& key) {
                    return !key || !block.controls[0].key || !block.controls[1].key || block.controls[0].key == key ||
                           block.controls[1].key == key;
                };
                if (d.one_hot && modern_control_violations(block, clusters, *failed)) {
                    if (c.preset && !available(c.global_preset))
                        reasons.push_back("SET equation does not fit in the block.");
                    if (c.reset && !available(c.global_reset))
                        reasons.push_back("RESET equation does not fit in the block.");
                    if (reasons.empty())
                        reasons.push_back("Cannot satisfy all block clock requirements from dedicated clock pins.");
                } else if (!d.one_hot && d.device != "MACH215") {
                    bool preset = false, reset = false;
                    for (auto other : block) {
                        preset |= c.preset && clusters[other].preset && c.preset != clusters[other].preset;
                        reset |= c.reset && clusters[other].reset && c.reset != clusters[other].reset;
                    }
                    if (preset)
                        reasons.push_back("SET equation does not fit in the block.");
                    if (reset)
                        reasons.push_back("RESET equation does not fit in the block.");
                }
                if (!oe_compatible(block, clusters, *failed, d))
                    reasons.push_back("OUTPUT ENABLE equation does not fit in the block.");
                if (pin_count(block, clusters) + c.output > capacity[capacity_index])
                    reasons.push_back("OUTPUT PIN does not fit in the block.");
                auto incoming = inputs;
                incoming.insert(c.fanins.begin(), c.fanins.end());
                if (d.one_hot && block.product_clock(c) && c.clock_fanin)
                    incoming.insert(*c.clock_fanin);
                if (incoming.size() > d.muxes)
                    reasons.push_back("Block FANIN LIMIT ( " + std::to_string(d.muxes) + " ) exceeded.");
                const unsigned groups = d.device == "MACH215" ? d.cells / 2 : d.cells;
                if (product_groups(block, clusters) + block.product_charge(c) > groups) {
                    reasons.push_back("PRODUCT TERM does not fit in the block.");
                    reasons.push_back("   Insufficient clusters.  It requires " +
                                      std::to_string(block.product_charge(c)) + " but only " +
                                      std::to_string(groups - product_groups(block, clusters)) + " available.");
                }
                if (macrocell_count(block, clusters) + c.macrocell > groups)
                    reasons.push_back("BURIED REGISTER does not fit in the block.");
            }
            frame.failure_reasons.push_back(std::move(reasons));
        }
        return frame;
    };
    auto clear_blocks = [&] {
        for (auto& block : blocks) {
            const auto legacy           = block.legacy_oe;
            const auto oe_capacity      = block.legacy_oe_capacity;
            const auto b                = std::size_t(&block - blocks.data());
            block                       = Block{};
            block.clusters              = &clusters;
            block.legacy_oe             = legacy;
            block.legacy_oe_capacity    = oe_capacity;
            block.fixed_product_balance = d.one_hot;
            block.reserved_product_groups =
                iteration && b < iteration->product_reservations.size() && iteration->product_reservations[b] ? 1u : 0u;
        }
    };
    auto retry = [&](std::size_t position) {
        const auto id = order[position];
        // Reporting retains the most complete native frame, including
        // failed promotion trials. This does not alter the strategy's
        // separate exhausted-pass best_progress counter below.
        if (observer) {
            auto frame = observe(id, unsigned(position));
            if (!best_observation || frame.progress > best_observation->progress)
                best_observation = std::move(frame);
        }
        // Apply failed-cluster promotion after both fixed seeding and ordinary
        // insertion failures on every device family.
        for (std::size_t previous = position; previous > 0; --previous) {
            const auto earlier = order[previous - 1];
            if (promoted_predecessor[earlier])
                continue;
            if (keys[earlier].rank == keys[id].rank)
                keys[id].fanout_sum = std::uint16_t(keys[earlier].fanout_sum - 1);
            else
                keys[id].rank = std::uint16_t(keys[earlier].rank + 1);
            promoted_predecessor[earlier] = true;
            order                         = partition_signal_order(keys);
            return;
        }
        // Snapshot only an exhausted promotion pass, including passes that
        // failed while seeding fixed clusters.
        if (position > best_progress) {
            best_progress         = position;
            best_product_pressure = false;
            best_fixed_failure    = clusters[id].fixed_block >= 0;
            const auto groups     = d.device == "MACH215" ? d.cells / 2 : d.cells;
            for (unsigned b = 0; b < blocks.size(); ++b) {
                if (clusters[id].fixed_block >= 0 && unsigned(clusters[id].fixed_block) != b)
                    continue;
                best_product_pressure |=
                    product_groups(blocks[b], clusters) + blocks[b].product_charge(clusters[id]) > groups;
            }
        }
        if (phase == 4) {
            // Ground absent reset and preset controls on clocked macrocells, including
            // their global-row candidates. Keep these changes through all five ranks.
            bool changed = false;
            if (!grounded_implicit) {
                grounded = ground_implicit_partition_controls(clusters);
                changed  = std::any_of(grounded.begin(), grounded.end(), [](unsigned bits) { return bits != 0; });
            }
            if (changed)
                grounded_implicit = true;
            else {
                // Allow deferred asynchronous groups to consume the balance only
                // after a fixed-seed product allocation failure.
                if (grounded_implicit && best_fixed_failure && best_product_pressure && !auxiliary_retry) {
                    for (auto& c : clusters)
                        if (c.async_product_flags & 0x1000) {
                            c.async_product_flags |= 1;
                            changed = true;
                        }
                    if (changed) {
                        auxiliary_retry   = true;
                        grounded_implicit = false;
                        // Restore only controls introduced by implicit grounding; retain
                        // explicit GND controls.
                        for (std::size_t j = 0; j < clusters.size(); ++j) {
                            if (grounded[j] & 1) {
                                clusters[j].reset        = original_clusters[j].reset;
                                clusters[j].global_reset = original_clusters[j].global_reset;
                            }
                            if (grounded[j] & 2) {
                                clusters[j].preset        = original_clusters[j].preset;
                                clusters[j].global_preset = original_clusters[j].global_preset;
                            }
                        }
                        std::fill(grounded.begin(), grounded.end(), 0);
                    }
                }
                if (!changed && iteration && iteration->previous_success) {
                    // After a successful partition, restore implicit controls and reserve
                    // one final physical attempt while retaining auxiliary flags.
                    for (std::size_t j = 0; j < clusters.size(); ++j) {
                        if (grounded[j] & 1) {
                            clusters[j].reset        = original_clusters[j].reset;
                            clusters[j].global_reset = original_clusters[j].global_reset;
                        }
                        if (grounded[j] & 2) {
                            clusters[j].preset        = original_clusters[j].preset;
                            clusters[j].global_preset = original_clusters[j].global_preset;
                        }
                    }
                    std::fill(grounded.begin(), grounded.end(), 0);
                    grounded_implicit           = false;
                    iteration->previous_success = false;
                    iteration->remaining        = 1;
                    iteration->restricted       = true;
                    changed                     = true;
                }
                if (!changed) {
                    if (observer) {
                        auto frame = observe(id, unsigned(position));
                        observer(best_observation && frame.progress < best_observation->progress ? *best_observation
                                                                                                 : frame);
                    }
                    fail("z8007: partition failed after all rank strategies for " + d.device);
                }
            }
            phase         = 0;
            balanced_mode = balanced && !(iteration && iteration->restricted);
        } else if (phase == 0 && balanced_mode)
            balanced_mode = false;
        else
            ++phase;
        // Reset promotion marks and recompute ranking keys for all device families.
        rank_mach_modern_phase(clusters, phase, best_product_pressure, grounded_implicit);
        for (std::size_t j = 0; j < clusters.size(); ++j)
            keys[j] = clusters[j].order;
        std::fill(promoted_predecessor.begin(), promoted_predecessor.end(), false);
        order = partition_signal_order(keys);
    };
    for (;;) {
        check_fitting_budget();
        // For each retry, sort clusters, clear resource tables and seed fixed
        // clusters again. Never carry stale resource records into a new attempt.
        clear_blocks();
        bool complete = true;
        for (std::size_t p = 0; p < order.size(); ++p) {
            const auto i = order[p];
            if (clusters[i].fixed_block < 0)
                continue;
            const auto b = unsigned(clusters[i].fixed_block);
            if (block_accepts_cluster(blocks[b], clusters, i, d, capacity[b])) {
                blocks[b].push_back(i);
                continue;
            }
            retry(p);
            complete = false;
            break;
        }
        if (!complete)
            continue;
        for (std::size_t p = 0; p < order.size(); ++p) {
            const auto i = order[p];
            if (clusters[i].fixed_block >= 0)
                continue;
            const int chosen = select(blocks, clusters, i, d, capacity, balanced_mode);
            if (chosen >= 0) {
                blocks[chosen].push_back(i);
                continue;
            }
            if (rescue(blocks, clusters, i, d, capacity, balanced_mode))
                continue;
            retry(p);
            complete = false;
            break;
        }
        if (complete)
            break;
    }
    // Adding a cluster cannot improve maximum fanin before removing its old
    // membership. The rejected target insertion and removal can still change
    // ordered output-enable records; preserve these effects on legacy devices.
    if (!d.one_hot)
        for (unsigned x = 0; x < d.blocks; ++x)
            for (std::size_t pos = blocks[x].size(); pos > 0; --pos) {
                const auto id = blocks[x][pos - 1];
                if (clusters[id].fixed_block >= 0)
                    continue;
                const int target = select(blocks, clusters, id, d, capacity, balanced_mode, int(x));
                if (target >= 0) {
                    blocks[unsigned(target)].push_back(id);
                    remove_cluster_from_block(blocks[unsigned(target)], id);
                }
            }
    // Traverse blocks, reverse entries, later blocks and their reverse entries.
    // Rejected post-removal checks append restored entries, preserving DOS order.
    for (unsigned x = 0; x < d.blocks; ++x)
        for (std::size_t ai = blocks[x].size(); ai > 0; --ai) {
            if (clusters[blocks[x][ai - 1]].fixed_block >= 0)
                break;
            for (unsigned y = x + 1; y < d.blocks; ++y)
                for (std::size_t bi = blocks[y].size(); bi > 0; --bi) {
                    const auto a = blocks[x][ai - 1], b = blocks[y][bi - 1];
                    if (clusters[b].fixed_block >= 0)
                        break;
                    if (!swap_resources(blocks, clusters, x, a, y, b, d, capacity))
                        continue;
                    const auto before = maximum_pair_fanin_count(blocks, clusters, x, y);
                    remove_cluster_from_block(blocks[x], a);
                    if (!block_accepts_cluster(blocks[x], clusters, b, d, capacity[x])) {
                        blocks[x].push_back(a);
                        continue;
                    }
                    blocks[x].push_back(b);
                    remove_cluster_from_block(blocks[y], b);
                    if (!block_accepts_cluster(blocks[y], clusters, a, d, capacity[y])) {
                        remove_cluster_from_block(blocks[x], b);
                        blocks[x].push_back(a);
                        blocks[y].push_back(b);
                        continue;
                    }
                    blocks[y].push_back(a);
                    if (before < maximum_pair_fanin_count(blocks, clusters, x, y)) {
                        remove_cluster_from_block(blocks[y], a);
                        remove_cluster_from_block(blocks[x], b);
                        blocks[x].push_back(a);
                        blocks[y].push_back(b);
                    }
                }
        }
    if (observer)
        observer(observe({}, unsigned(order.size())));
    if (oe_order) {
        oe_order->clear();
        for (const auto& block : blocks) {
            if (d.device[4] == '2') {
                std::vector<std::string> rows;
                for (unsigned slot = 0; slot < block.legacy_oe_capacity; ++slot)
                    rows.push_back(block.oe_resources[slot].key);
                oe_order->push_back(std::move(rows));
            } else
                oe_order->push_back(block.oe_order);
        }
    }
    if (iteration) {
        iteration->grounded              = grounded;
        iteration->grounded_implicit     = grounded_implicit;
        iteration->auxiliary_retry       = auxiliary_retry;
        iteration->best_progress         = best_progress;
        iteration->best_product_pressure = best_product_pressure;
        iteration->best_fixed_failure    = best_fixed_failure;
        iteration->succeeded(phase, balanced_mode);
    }
    if (final_order)
        *final_order = order;
    if (implicit_controls)
        *implicit_controls = grounded;
    if (product_clocks) {
        product_clocks->assign(clusters.size(), false);
        for (const auto& block : blocks)
            for (auto id : block.product_clock_members)
                (*product_clocks)[id] = true;
    }
    if (controls) {
        controls->clear();
        for (const auto& block : blocks)
            controls->push_back({block.controls[0].key, block.controls[1].key});
    }
    if (used_product_groups) {
        used_product_groups->clear();
        for (const auto& block : blocks)
            used_product_groups->push_back(product_groups(block, clusters));
    }
    return blocks;
}
struct ProductAllocator
{
    const MachDeviceLayout& layout;
    std::vector<int> owner, saved, demand;
    std::vector<bool> active;
    ProductAllocator(const MachDeviceLayout& d)
        : layout(d), owner(d.cells, -1), saved(d.cells, -1), demand(d.cells), active(d.cells)
    {
    }
    std::vector<int> candidates(int cell) const { return mach_steering_clusters(layout, cell); }
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
        for (unsigned g = 0; g < layout.cells; ++g) {
            if (owner[g] == cell)
                owner[g] = -1;
            if (saved[g] == cell)
                saved[g] = -1;
        }
        demand[cell] = 0;
    }
};
}  // namespace
std::vector<std::vector<std::size_t>> partition_mach_family_first_success(
    const MachDeviceLayout& d, const std::vector<Mach111PartitionCluster>& clusters,
    const std::vector<unsigned>& capacity, bool balanced, std::vector<GlobalControlSlots>* controls,
    std::vector<std::size_t>* final_order, std::vector<bool>* product_clocks, std::vector<unsigned>* implicit_controls,
    PartitionIterationState* iteration, std::vector<std::vector<std::string>>* oe_order,
    std::vector<unsigned>* used_product_groups, const PartitionObserver& observer)
{
    if (capacity.size() != d.blocks)
        throw std::invalid_argument("family partition capacity shape");
    for (const auto& c : clusters) {
        if (c.fixed_block >= int(d.blocks))
            throw std::invalid_argument("family partition fixed block");
        if (c.global_clock < -1 || c.global_clock >= 4)
            throw std::invalid_argument("family partition clock ordinal");
    }
    const auto blocks = partition(d, clusters, capacity, balanced, controls, final_order, product_clocks, oe_order,
                                  implicit_controls, iteration, used_product_groups, observer);
    std::vector<std::vector<std::size_t>> result;
    for (const auto& block : blocks)
        result.emplace_back(block.begin(), block.end());
    return result;
}
FamilyPlacement place_mach_family(
    const MachDeviceLayout& d, const std::vector<Mach111PlaceSignal>& declared_signals, Mach111PlaceOptions options,
    std::function<PlacementRouteFeedback(const FamilyPlacement&, const std::vector<bool>&)> route,
    std::function<void(const FamilyPlacement&, unsigned, const std::vector<bool>&)> failure)
{
    if (!options.iteration) {
        PartitionIterationState iteration(options.iterate_partition);
        options.iteration = &iteration;
        for (;;) {
            FittingIterationCounters counters(d.blocks, options.iterate_partition);
            const auto previous = iteration.completed;
            try {
                return place_mach_family(d, declared_signals, options, route, failure);
            } catch (const PhysicalPlacementFailure&) {
                if (!iteration.remaining || iteration.completed == previous)
                    throw;
            } catch (const RepartitionRequested&) {
                if (!iteration.remaining || iteration.completed == previous)
                    throw PhysicalPlacementFailure("z7502: physical fitting unsuccessful after partition retries");
            }
        }
    }
    const bool mach2 = d.device.substr(4, 1) == "2";
    if (d.one_hot || (!mach2 && d.device.substr(4, 1) != "1"))
        fail("MACH1/2 placement geometry required");
    // A fixed input register behind an output pad fixes that pad: the
    // register is the odd cell of the pad's pin.
    auto signals = declared_signals;
    for (const auto& s : declared_signals) {
        if (!s.input_register || !mach2 || s.fixed_cell < 0 || s.paired_input < 0 ||
            unsigned(s.paired_input) >= signals.size() || !signals[unsigned(s.paired_input)].output)
            continue;
        if (unsigned(s.fixed_cell) >= d.blocks * d.cells || !(s.fixed_cell & 1))
            fail("invalid input register cell");
        auto& pad      = signals[unsigned(s.paired_input)];
        const auto pin = std::int16_t(d.io_pins[unsigned(s.fixed_cell) / 2] - 1);
        if (pad.order.fixed_pin >= 0 && pad.order.fixed_pin != pin)
            fail("input pair location conflict");
        pad.order.fixed_pin = pin;
    }
    const unsigned cell_step = mach2 ? 2 : 1;
    FamilyPlacement r;
    r.sequential_search.assign(d.blocks, false);
    r.implicit_controls.assign(signals.size(), 0);
    r.oe_products.resize(d.blocks);
    r.oe_slots.assign(signals.size(), -1);
    r.product_group_owner.assign(d.blocks * d.cells, -1);
    r.cells.assign(signals.size(), -1);
    r.pins.assign(signals.size(), -1);
    r.output_order.resize(d.blocks);
    auto cell_for_pin = [&](unsigned pin) {
        const auto at = std::find(d.io_pins.begin(), d.io_pins.end(), pin);
        return at == d.io_pins.end() ? -1 : int(at - d.io_pins.begin()) * int(cell_step);
    };
    auto clock_pin = [&](unsigned pin) {
        return std::find(d.clock_pins.begin(), d.clock_pins.end(), pin) != d.clock_pins.end();
    };
    auto data_pin = [&](unsigned pin) {
        return cell_for_pin(pin) >= 0 ||
               std::find(d.dedicated_pins.begin(), d.dedicated_pins.end(), pin) != d.dedicated_pins.end();
    };
    std::vector<bool> occupied_pins(d.pins + 1), occupied_cells(d.blocks * d.cells);
    std::vector<FitterOrderKey> keys;
    std::vector<std::size_t> outputs, clocks, input_nodes;
    Clusters clusters;
    bool automatic = false;
    for (std::size_t i = 0; i < signals.size(); ++i) {
        const auto& s = signals[i];
        keys.push_back(s.order);
        if (s.input_register) {
            input_nodes.push_back(i);
            continue;
        }
        if (s.order.fixed_pin >= 0) {
            const auto pin = unsigned(s.order.fixed_pin) + 1;
            if (pin > d.pins || !data_pin(pin))
                fail("invalid pin");
            occupied_pins[pin] = true;
            r.pins[i]          = int(pin);
        }
        if (!s.output) {
            if (s.order.type == 9)
                clocks.push_back(i);
            continue;
        }
        if (s.order.product_terms < 1 || s.order.product_terms > (mach2 && d.device != "MACH215" ? 16 : 12) ||
            s.xor_product)
            fail("single product group required");
        outputs.push_back(i);
        clusters.push_back({{0, 0, std::uint16_t(s.order.serial)},
                            s.fanins,
                            s.reset,
                            !s.buried,
                            -1,
                            unsigned((s.order.product_terms + 3) / 4),
                            s.oe_product,
                            s.preset});
        clusters.back().reset_product          = s.reset_product;
        clusters.back().preset_product         = s.preset_product;
        clusters.back().explicit_async_control = s.explicit_async_control;
        clusters.back().data_terms             = unsigned(s.order.product_terms);
        clusters.back().clocked_storage        = s.clocked_storage;
        if (r.pins[i] >= 0) {
            const auto cell = cell_for_pin(unsigned(r.pins[i]));
            if (cell < 0 || occupied_cells[unsigned(cell)])
                fail("conflicting output pin");
            r.cells[i]                     = cell;
            occupied_cells[unsigned(cell)] = true;
            clusters.back().fixed_block    = cell / int(d.cells);
        } else if (s.order.fixed_node >= 0) {
            const auto cell = s.fixed_cell;
            if (!s.buried || cell < 0 || unsigned(cell) >= d.blocks * d.cells || occupied_cells[unsigned(cell)])
                fail("conflicting node");
            r.cells[i]                     = cell;
            occupied_cells[unsigned(cell)] = true;
            clusters.back().fixed_block    = cell / int(d.cells);
        } else
            automatic = true;
        if (s.order.block >= 0) {
            if (unsigned(s.order.block) >= d.blocks ||
                (clusters.back().fixed_block >= 0 && clusters.back().fixed_block != s.order.block))
                fail("conflicting block constraint");
            clusters.back().fixed_block = s.order.block;
        }
    }
    if (outputs.empty())
        fail("no output macrocells");
    if (!mach2)
        for (unsigned c = 0; c < outputs.size(); ++c)
            if (signals[outputs[c]].order.fixed_pin >= 0)
                clusters[c].fixed_half = int(unsigned(r.cells[outputs[c]]) % d.cells / (d.cells / 2));
    std::vector<std::vector<std::string>> partition_oe_order(d.blocks);
    auto cluster_signals = outputs;
    std::map<std::size_t, std::size_t> paired_nodes;
    std::vector<std::pair<std::size_t, std::size_t>> pad_registers;
    for (auto node : input_nodes) {
        const auto& s = signals[node];
        if (!mach2 || s.paired_input < 0 || unsigned(s.paired_input) >= signals.size())
            fail("invalid input pair");
        const auto pad = std::size_t(s.paired_input);
        // A pad that is itself an output keeps its own macrocell. Its input
        // register is the odd cell behind the same pin: no further cluster,
        // block choice or pin is involved.
        if (signals[pad].output) {
            if (std::any_of(pad_registers.begin(), pad_registers.end(),
                            [&](const auto& pair) { return pair.first == pad; }))
                fail("conflicting input pair");
            pad_registers.emplace_back(pad, node);
            continue;
        }
        if (!paired_nodes.emplace(pad, node).second)
            fail("conflicting input pair");
        int cell = s.fixed_cell;
        if (r.pins[pad] >= 0) {
            const auto paired = cell_for_pin(unsigned(r.pins[pad]));
            if (paired < 0 || (cell >= 0 && cell != paired + 1))
                fail("input pair location conflict");
            cell = paired + 1;
        }
        if (cell >= 0) {
            if (unsigned(cell) >= d.blocks * d.cells || !(cell & 1) || occupied_cells[unsigned(cell)])
                fail("invalid input register cell");
            const auto pin = d.io_pins[unsigned(cell) / 2];
            if (r.pins[pad] < 0 && occupied_pins[pin])
                fail("input pair pin conflict");
            occupied_cells[unsigned(cell)] = true;
            occupied_pins[pin]             = true;
            r.cells[node]                  = cell;
            r.pins[pad]                    = int(pin);
        } else
            automatic = true;
        Mach111PartitionCluster c;
        c.order.signal_id = std::uint16_t(signals[pad].order.serial);
        c.output          = true;
        c.product_groups  = 0;
        c.macrocell       = false;
        c.data_terms      = 0;
        if (cell >= 0)
            c.fixed_block = cell / int(d.cells);
        const int block = s.order.block >= 0 ? s.order.block : signals[pad].order.block;
        if (block >= 0) {
            if (unsigned(block) >= d.blocks || (c.fixed_block >= 0 && c.fixed_block != block))
                fail("input pair block conflict");
            c.fixed_block = block;
        }
        clusters.push_back(c);
        cluster_signals.push_back(node);
    }
    if (options.reserve_default_clock)
        occupied_pins[d.device == "MACH215" ? d.clock_pins.front() : d.clock_pins.back()] = true;
    if (clocks.size() == 1 && r.pins[clocks[0]] < 0) {
        const auto i                     = clocks[0];
        std::vector<unsigned> preference = {d.device == "MACH215" ? d.clock_pins.front() : d.clock_pins.back()};
        preference.insert(preference.end(), d.clock_pins.begin(), d.clock_pins.end());
        for (auto pin : preference)
            if (!occupied_pins[pin]) {
                occupied_pins[pin] = true;
                r.pins[i]          = int(pin);
                keys[i].fixed_pin  = std::int16_t(pin - 1);
                break;
            }
    }
    std::sort(clocks.begin(), clocks.end(), [&](auto a, auto b) { return keys[a].serial < keys[b].serial; });
    for (auto i : clocks)
        if (r.pins[i] < 0) {
            for (auto pin : d.clock_pins)
                if (!occupied_pins[pin]) {
                    occupied_pins[pin] = true;
                    r.pins[i]          = int(pin);
                    keys[i].fixed_pin  = std::int16_t(pin - 1);
                    break;
                }
            if (r.pins[i] < 0)
                fail("no free clock pin");
        }
    std::vector<std::set<std::string>> fixed_enables(d.blocks * (mach2 ? 1 : 2));
    if (d.device != "MACH215")
        for (const auto& c : clusters)
            if (c.fixed_block >= 0 && !c.oe_product.empty() && (mach2 || c.fixed_half >= 0))
                fixed_enables[unsigned(c.fixed_block) * (mach2 ? 1 : 2) + (mach2 ? 0 : unsigned(c.fixed_half))].insert(
                    c.oe_product);
    const bool fixed_oe_overflow = std::any_of(fixed_enables.begin(), fixed_enables.end(),
                                               [](const auto& products) { return products.size() > 2; });
    std::vector<std::size_t> import_clusters;
    if (automatic || fixed_oe_overflow) {
        std::vector<unsigned> capacity(d.blocks, d.cells / cell_step);
        std::set<unsigned> output_pins, counted;
        for (auto i : outputs)
            if (r.pins[i] >= 0)
                output_pins.insert(unsigned(r.pins[i]));
        for (const auto& pair : paired_nodes)
            if (r.pins[pair.first] >= 0)
                output_pins.insert(unsigned(r.pins[pair.first]));
        for (std::size_t i = 0; i < signals.size(); ++i)
            if (!signals[i].output && r.pins[i] >= 0) {
                const auto pin  = unsigned(r.pins[i]);
                const auto cell = cell_for_pin(pin);
                if (cell >= 0 && !output_pins.count(pin) && counted.insert(pin).second)
                    --capacity[unsigned(cell) / d.cells];
            }
        std::vector<unsigned> implicit_controls;
        const auto blocks =
            partition(d, clusters, capacity, options.balanced_partition, nullptr, &import_clusters, nullptr,
                      &partition_oe_order, &implicit_controls, options.iteration, &r.partition_product_groups,
                      options.partition_observer ? PartitionObserver([&](const auto& frame) {
                          options.partition_observer(signals, cluster_signals, frame);
                      })
                                                 : PartitionObserver{});
        for (unsigned b = 0; b < d.blocks; ++b)
            for (auto c : blocks[b]) {
                const auto signal = cluster_signals[c];
                if (r.cells[signal] < 0)
                    r.cells[signal] = int(b * d.cells);
                r.implicit_controls[signal] = implicit_controls[c];
            }
    }
    if (import_clusters.empty()) {
        auto ranked = clusters;
        rank_mach111_single_group(ranked);
        std::vector<PartitionOrderKey> import_keys;
        for (const auto& cluster : ranked)
            import_keys.push_back(cluster.order);
        import_clusters = partition_signal_order(import_keys);
    }
    std::vector<bool> imported(signals.size());
    const auto import_signal = [&](std::size_t id) {
        if (!imported[id]) {
            r.import_order.push_back(id);
            imported[id] = true;
        }
    };
    for (auto cluster : import_clusters) {
        const auto id = cluster_signals[cluster];
        if (signals[id].input_register)
            import_signal(std::size_t(signals[id].paired_input));
        import_signal(id);
    }
    std::vector<std::size_t> ordinary_inputs;
    for (std::size_t i = 0; i < signals.size(); ++i)
        if (!imported[i])
            ordinary_inputs.push_back(i);
    std::sort(ordinary_inputs.begin(), ordinary_inputs.end(),
              [&](auto a, auto b) { return signals[a].order.serial < signals[b].order.serial; });
    for (auto id : ordinary_inputs)
        import_signal(id);
    for (const auto& pair : paired_nodes)
        keys[pair.first].block = keys[pair.second].block = std::int16_t(r.cells[pair.second] / int(d.cells));
    for (const auto& pair : pad_registers) {
        r.cells[pair.second]    = r.cells[pair.first];
        keys[pair.second].block = std::int16_t(r.cells[pair.first] / int(d.cells));
    }
    for (std::size_t i = 0; i < signals.size(); ++i)
        if (keys[i].type != 9) {
            std::set<unsigned> destinations;
            for (auto out : outputs)
                if (std::find(signals[out].fanins.begin(), signals[out].fanins.end(), std::uint16_t(keys[i].serial)) !=
                    signals[out].fanins.end())
                    destinations.insert(unsigned(r.cells[out]) / d.cells);
            keys[i].fanout_count = std::int16_t(destinations.size());
        }
    r.global_order = fitter_signal_order(keys, false);
    for (auto i : r.global_order)
        if (signals[i].output)
            r.output_order[unsigned(r.cells[i]) / d.cells].push_back(i);
    const unsigned stride = mach2 && d.device != "MACH215" ? 4 : 3;  // Maximum steering-list length for this device.
    std::vector<unsigned> spread;
    for (unsigned start = 0; start < stride; ++start)
        for (unsigned cell = start; cell < d.cells; cell += stride)
            spread.push_back(cell);
    std::vector<bool> remaining_candidates(signals.size());
    int retry_signal = -1;
    auto finish      = [&]() {
        const auto saved      = r;
        const auto saved_pins = occupied_pins, saved_cells = occupied_cells;
        std::vector<std::size_t> cursor(d.blocks);
        for (auto& i : r.global_order)
            if (signals[i].output) {
                const auto block = unsigned(r.cells[i]) / d.cells;
                i                = r.output_order[block][cursor[block]++];
            }
        auto inputs = [&]() {
            const auto old_pins     = r.pins;
            const auto old_occupied = occupied_pins;
            auto failed             = [&]() {
                r.pins        = old_pins;
                occupied_pins = old_occupied;
                return false;
            };
            for (auto i : r.global_order)
                if (!signals[i].output && r.pins[i] < 0) {
                    if (signals[i].input_register)
                        continue;
                    for (unsigned pin = 1; pin <= d.pins; ++pin) {
                        if (occupied_pins[pin] || !(signals[i].order.type == 9 ? clock_pin(pin) : data_pin(pin)))
                            continue;
                        if (signals[i].order.block >= 0) {
                            const auto cell = cell_for_pin(pin);
                            if (cell < 0 || cell / int(d.cells) != signals[i].order.block)
                                continue;
                        }
                        occupied_pins[pin] = true;
                        r.pins[i]          = int(pin);
                        break;
                    }
                    if (r.pins[i] < 0)
                        return failed();
                }
            if (route) {
                const auto feedback = route(r, remaining_candidates);
                if (!feedback.success) {
                    retry_signal = feedback.retry_signal;
                    return failed();
                }
            }
            return true;
        };
        for (const auto& pair : pad_registers) {
            const auto cell = unsigned(r.cells[pair.first]) + 1;
            if (!(cell & 1) || cell >= occupied_cells.size() || occupied_cells[cell]) {
                r              = saved;
                occupied_pins  = saved_pins;
                occupied_cells = saved_cells;
                return false;
            }
            occupied_cells[cell] = true;
            r.cells[pair.second] = int(cell);
        }
        std::vector<std::size_t> pairs;
        for (auto pad : r.global_order)
            if (paired_nodes.count(pad) && r.pins[pad] < 0)
                pairs.push_back(pad);
        std::function<bool(std::size_t)> place_pairs = [&](std::size_t position) {
            check_fitting_budget();
            if (position == pairs.size())
                return inputs();
            const auto pad = pairs[position], node = paired_nodes.at(pad);
            const auto block = unsigned(r.cells[node]) / d.cells;
            std::vector<unsigned> candidates;
            for (unsigned candidate = 0; candidate < d.cells; ++candidate) {
                const unsigned cell = block * d.cells + (options.spread_placement ? spread[candidate] : candidate);
                if (!(cell & 1) || occupied_cells[cell] || occupied_pins[d.io_pins[cell / 2]])
                    continue;
                candidates.push_back(cell);
            }
            const auto old_cell = r.cells[node];
            for (unsigned n = 0; n < candidates.size(); ++n) {
                const auto cell = candidates[n], pin = d.io_pins[cell / 2];
                remaining_candidates[pad] = remaining_candidates[node] = n + 1 < candidates.size();
                occupied_cells[cell] = occupied_pins[pin] = true;
                r.cells[node]                             = int(cell);
                r.pins[pad]                               = int(pin);
                if (place_pairs(position + 1))
                    return true;
                occupied_cells[cell] = occupied_pins[pin] = false;
                r.cells[node]                             = old_cell;
                r.pins[pad]                               = -1;
                if (retry_signal >= 0) {
                    if (retry_signal != int(pad) && retry_signal != int(node)) {
                        remaining_candidates[pad] = remaining_candidates[node] = false;
                        return false;
                    }
                    retry_signal = -1;
                }
            }
            remaining_candidates[pad] = remaining_candidates[node] = false;
            return false;
        };
        if (place_pairs(0))
            return true;
        r              = saved;
        occupied_pins  = saved_pins;
        occupied_cells = saved_cells;
        return false;
    };
    const auto partition_order = r.output_order;
    r.oe_keys.assign(signals.size(), -1);
    for (const auto& order : partition_order) {
        std::map<std::string, int> oe_key_ids;
        for (auto i : order)
            if (!signals[i].oe_product.empty()) {
                const auto& key = signals[i].oe_product;
                if (!oe_key_ids.count(key))
                    oe_key_ids[key] = int(oe_key_ids.size());
                r.oe_keys[i] = oe_key_ids.at(key);
            }
    }
    std::vector<OutputSearchCallbacks> search_operations(signals.size());
    std::vector<std::function<void()>> publish(d.blocks);
    std::optional<FamilyPlacement> failed_frame;
    std::vector<bool> failed_assigned;
    unsigned failed_block                        = 0;
    bool failed_complete                         = false;
    int failed_cell_block                        = -1;
    std::function<bool(unsigned)> prepare_blocks = [&](unsigned b) {
        if (b == d.blocks) {
            std::vector<std::size_t> global;
            std::vector<std::size_t> cursor(d.blocks);
            for (auto i : r.global_order)
                if (signals[i].output) {
                    const auto block = unsigned(r.cells[i]) / d.cells;
                    global.push_back(r.output_order[block][cursor[block]++]);
                }
            if (options.collect_report_visits) {
                auto extra = options.report_extra_records;
                for (auto& key : extra) {
                    std::set<unsigned> destinations;
                    for (std::size_t i = 0; i < signals.size(); ++i)
                        if (signals[i].output && std::find(signals[i].fanins.begin(), signals[i].fanins.end(),
                                                           std::uint16_t(key.serial)) != signals[i].fanins.end())
                            destinations.insert(unsigned(r.cells[i]) / d.cells);
                    key.fanout_count = std::int16_t(destinations.size());
                }
                r.reverse_visit_costs = placement_reverse_visit_costs(keys, extra, r.output_order);
                begin_fitting_logical_walk(unsigned(keys.size() + extra.size()));
            }
            return search_output_candidates(
                r.output_order, global, search_operations,
                [&](const std::vector<bool>& remaining) {
                    remaining_candidates = remaining;
                    retry_signal         = -1;
                    for (auto& write : publish)
                        write();
                    const auto success = finish();
                    return PlacementRouteFeedback{success, retry_signal};
                },
                r.reverse_visit_costs,
                [&](unsigned block) {
                    if (mach2)
                        return true;
                    for (auto id : r.output_order[block])
                        if (!signals[id].buried) {
                            const auto pin = unsigned(r.pins[id]);
                            for (std::size_t other = 0; other < signals.size(); ++other)
                                if (other != id && !signals[other].output && r.pins[other] == int(pin))
                                    return false;
                        }
                    return true;
                },
                [&](unsigned block, const std::vector<bool>& assigned, int signal) {
                    if (!failure)
                        return;
                    if (std::none_of(assigned.begin(), assigned.end(), [](bool value) { return value; }))
                        return;
                    const bool complete = std::all_of(r.output_order[block].begin(), r.output_order[block].end(),
                                                      [&](auto id) { return assigned[id]; });
                    const bool empty    = std::none_of(r.output_order[block].begin(), r.output_order[block].end(),
                                                       [&](auto id) { return assigned[id]; });
                    const bool preserve =
                        failed_frame && failed_block == block && ((failed_complete && !complete) || empty);
                    const auto saved = r;
                    for (auto& write : publish)
                        write();
                    if (preserve) {
                        // Initial cell allocation continues in later blocks
                        // even when this block has failed. Retain its last
                        // useful prefix while publishing those later blocks.
                        for (unsigned other = 0; other < d.blocks; ++other)
                            if (other != block) {
                                for (auto id : r.output_order[other]) {
                                    failed_frame->cells[id]    = r.cells[id];
                                    failed_frame->pins[id]     = r.pins[id];
                                    failed_frame->oe_slots[id] = r.oe_slots[id];
                                    failed_assigned[id]        = assigned[id];
                                }
                                failed_frame->oe_products[other]       = r.oe_products[other];
                                failed_frame->sequential_search[other] = r.sequential_search[other];
                                for (unsigned group = other * d.cells; group < (other + 1) * d.cells; ++group)
                                    failed_frame->product_group_owner[group] = r.product_group_owner[group];
                            }
                        failed_frame->last_oe_constraint = r.last_oe_constraint;
                    } else {
                        failed_complete                     = complete;
                        failed_frame                        = r;
                        failed_assigned                     = assigned;
                        failed_block                        = block;
                        failed_frame->failed_signal         = signal;
                        failed_frame->failed_pad_assignment = signal < 0;
                    }
                    r = saved;
                },
                &failed_cell_block);
        }
        ProductAllocator products(d);
        const auto original = partition_order[b];
        std::vector<PhysicalOutputOrderKey> physical_keys;
        std::array<std::string, 4> oe_products;
        std::array<unsigned, 4> oe_references{};
        // MACH2 maps the ordered shared OE resource rows directly;
        // MACH1 instead computes the later half-block assignment.
        if (mach2)
            for (unsigned slot = 0; slot < partition_oe_order[b].size(); ++slot)
                oe_products[slot] = partition_oe_order[b][slot];
        const auto oe_slot = [&](unsigned cell, const std::string& key) {
            if (key.empty() || d.device == "MACH215")
                return -1;
            if (d.oe_constraint_count > 0)
                r.last_oe_constraint = std::array<int, 2>{d.oe_constraint_indices.at(b), d.oe_constraint_count};
            const unsigned first = mach2 ? 0 : (cell % d.cells < d.cells / 2 ? 0 : 2);
            int free             = -1;
            for (unsigned slot = first; slot < first + 2; ++slot) {
                if (oe_products[slot] == key)
                    return int(slot);
                if (oe_products[slot].empty() && free < 0)
                    free = int(slot);
            }
            return free;
        };
        if (d.device != "MACH215")
            for (auto i : original)
                if (signals[i].order.fixed_pin >= 0 && !signals[i].oe_product.empty()) {
                    const auto slot = oe_slot(unsigned(r.cells[i]), signals[i].oe_product);
                    if (slot < 0) {
                        throw PhysicalPlacementFailure("fixed OE capacity exceeded");
                    }
                    oe_products[unsigned(slot)] = signals[i].oe_product;
                    r.oe_slots[i]               = slot;
                    ++oe_references[unsigned(slot)];
                }
        if (!mach2)
            r.oe_products[b] = partition_oe(d, oe_products, partition_oe_order[b], signals, original);
        for (auto i : original) {
            const auto& s = signals[i];
            unsigned oe   = s.oe_members;
            if (!s.oe_product.empty()) {
                oe = 0;
                for (auto j : original)
                    oe += signals[j].oe_product == s.oe_product;
            }
            physical_keys.push_back({s.sort_name.empty() ? s.name : s.sort_name, s.order.fixed_node >= 0,
                                     s.order.fixed_pin >= 0, oe, s.order.product_terms, keys[i].fanout_count, false});
        }
        r.output_order[b].clear();
        std::vector<bool> placed(d.cells);
        // Reorder by output enable and name only on devices with positive
        // partition constraint counts; MACH2 devices use a negative count.
        std::vector<std::size_t> physical_order(original.size());
        std::iota(physical_order.begin(), physical_order.end(), 0);
        if (!mach2)
            physical_order = physical_output_order(physical_keys);
        for (auto index : physical_order)
            r.output_order[b].push_back(original[index]);
        unsigned spread_retries = 10;
        bool spread_enabled     = options.spread_placement;
        publish[b]              = [&]() {
            // Only occupied slots are written. A slot that a later attempt
            // leaves empty keeps the product of an earlier one, as in the
            // original: clearing it here changes the fuses of reference
            // designs that the original fits (AO1203, DO1202, DP1 and others).
            for (unsigned slot = 0; slot < 4; ++slot)
                if (!oe_products[slot].empty())
                    r.oe_products[b][slot] = oe_products[slot];
            for (unsigned group = 0; group < d.cells; ++group)
                r.product_group_owner[b * d.cells + group] =
                    products.owner[group] < 0 ? -1 : int(b * d.cells) + products.owner[group];
        };
        struct Previous
        {
            bool pin = false, cell = false;
            int location = -1, pin_number = -1, slot = -1;
        };
        std::map<std::size_t, Previous> previous;
        for (auto i : r.output_order[b]) {
            search_operations[i].candidates = [&, i]() {
                const bool fixed = signals[i].order.fixed_pin >= 0 || signals[i].order.fixed_node >= 0;
                std::vector<unsigned> candidates;
                bool physically_available = false;
                for (unsigned candidate = 0; candidate < d.cells; ++candidate) {
                    const unsigned cell = d.cells * b + (spread_enabled ? spread[candidate] : candidate);
                    if (cell % cell_step && (!signals[i].buried || d.device == "MACH215"))
                        continue;
                    const unsigned pin = d.io_pins[cell / cell_step];
                    if (placed[cell % d.cells])
                        continue;
                    if (fixed) {
                        if (cell != static_cast<unsigned>(r.cells[i]))
                            continue;
                    } else if (occupied_cells[cell] || (mach2 && !signals[i].buried && occupied_pins[pin]))
                        continue;
                    // Test every candidate before selecting one. Remove only the trial
                    // signal: successful displaced-owner assignments persist.
                    if (products.allocate(static_cast<int>(cell % d.cells), signals[i].order.product_terms)) {
                        physically_available = true;
                        // Check output-enable capacity after the product allocation trial. Even
                        // a rejected candidate can retain successful product-owner displacements.
                        if (d.device == "MACH215" || signals[i].oe_product.empty() ||
                            oe_slot(cell, signals[i].oe_product) >= 0)
                            candidates.push_back(cell);
                        products.remove(static_cast<int>(cell % d.cells));
                    }
                }
                // After ten exhausted spread retries, generate new candidate lists in
                // ascending cell order. Existing lists retain their previous order.
                if (candidates.empty() && physically_available) {
                    if (spread_retries)
                        --spread_retries;
                    else {
                        spread_enabled         = false;
                        r.sequential_search[b] = true;
                    }
                }
                return candidates;
            };
            search_operations[i].commit = [&, i](unsigned cell) {
                if (!products.allocate(int(cell % d.cells), signals[i].order.product_terms))
                    return false;
                const unsigned pin = d.io_pins[cell / cell_step];
                Previous old{occupied_pins[pin], occupied_cells[cell], r.cells[i], r.pins[i], -1};
                if (d.device != "MACH215" && !signals[i].oe_product.empty()) {
                    const int slot = oe_slot(cell, signals[i].oe_product);
                    if (slot < 0) {
                        products.remove(int(cell % d.cells));
                        return false;
                    }
                    oe_products[slot] = signals[i].oe_product;
                    r.oe_slots[i]     = slot;
                    if (signals[i].order.fixed_pin < 0) {
                        ++oe_references[slot];
                        old.slot = slot;
                    }
                }
                previous[i]            = old;
                placed[cell % d.cells] = true;
                occupied_cells[cell]   = true;
                if (!signals[i].buried) {
                    occupied_pins[pin] = true;
                    r.pins[i]          = int(pin);
                }
                r.cells[i] = int(cell);
                return true;
            };
            search_operations[i].remove = [&, i]() {
                const auto cell = unsigned(r.cells[i]), pin = d.io_pins[cell / cell_step];
                const auto old = previous.at(i);
                products.remove(int(cell % d.cells));
                placed[cell % d.cells] = false;
                occupied_cells[cell]   = old.cell;
                occupied_pins[pin]     = old.pin;
                r.cells[i]             = old.location;
                r.pins[i]              = old.pin_number;
                if (old.slot >= 0) {
                    if (--oe_references[unsigned(old.slot)] == 0)
                        oe_products[unsigned(old.slot)].clear();
                    r.oe_slots[i] = -1;
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
        throw PhysicalPlacementFailure("MACH1/2 placement: output candidate search exhausted");
    }
    return r;
}
}  // namespace machxl
