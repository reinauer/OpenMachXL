#include "fitter_partition.hpp"
#include "fitter_budget.hpp"
#include "fitter_family_place.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

namespace machxl {
namespace {
using Clusters = std::vector<Mach111PartitionCluster>;
struct Block : std::vector<std::size_t>
{
    const Clusters* clusters = nullptr;
    std::vector<std::string> oe_order;
    struct OeSlot
    {
        std::string key;
        unsigned count = 0;
    };
    std::array<OeSlot, 4> oe_resources;
    void push_back(std::size_t id)
    {
        std::vector<std::size_t>::push_back(id);
        const auto& c = clusters->at(id);
        // Stop at the first empty or matching resource row. A hole before a later
        // match intentionally creates duplicate keys.
        if (!c.oe_product.empty())
            for (auto& resource : oe_resources)
                if (resource.key.empty() || resource.key == c.oe_product) {
                    resource.key = c.oe_product;
                    ++resource.count;
                    break;
                }
        if (c.fixed_half < 0 && !c.oe_product.empty() &&
            std::find(oe_order.begin(), oe_order.end(), c.oe_product) == oe_order.end())
            oe_order.push_back(c.oe_product);
    }
};

std::set<std::uint16_t> collect_block_fanins(const Block& block, const Clusters& clusters)
{
    std::set<std::uint16_t> result;
    for (auto id : block)
        result.insert(clusters[id].fanins.begin(), clusters[id].fanins.end());
    return result;
}
bool reset_compatible(const Block& block, const Clusters& clusters, std::size_t incoming,
                      std::optional<std::size_t> outgoing = {})
{
    for (auto id : block)
        if (id != outgoing) {
            if (clusters[incoming].reset && clusters[id].reset && clusters[id].reset != clusters[incoming].reset)
                return false;
            if (clusters[incoming].preset && clusters[id].preset && clusters[id].preset != clusters[incoming].preset)
                return false;
        }
    return true;
}
bool oe_compatible(const Block& block, const Clusters& clusters, std::size_t incoming,
                   std::optional<std::size_t> outgoing = {})
{
    const auto& key = clusters[incoming].oe_product;
    if (key.empty())
        return true;
    if (!outgoing) {
        for (const auto& resource : block.oe_resources)
            if (resource.key.empty() || resource.key == key)
                return true;
        return false;
    }
    // Count occupied resource records after a proposed swap without
    // merging duplicate keys.
    unsigned used = 0;
    for (const auto& resource : block.oe_resources) {
        if (resource.key.empty() || resource.key == key)
            continue;
        if (resource.key == clusters[*outgoing].oe_product)
            used += resource.count != 1;
        else
            used += resource.count != 0;
    }
    return used < 4;
}
bool resource_limits_allow(const Block& block, const Clusters& clusters, std::size_t incoming)
{
    // All three resource limits are 16 for one-group MACH111 output clusters.
    if (block.size() >= 16 || !reset_compatible(block, clusters, incoming) || !oe_compatible(block, clusters, incoming))
        return false;
    unsigned groups = clusters[incoming].product_groups;
    for (auto id : block)
        groups += clusters[id].product_groups;
    if (groups > 16)
        return false;
    auto inputs = collect_block_fanins(block, clusters);
    inputs.insert(clusters[incoming].fanins.begin(), clusters[incoming].fanins.end());
    return inputs.size() <= 26;
}
unsigned count_output_clusters(const Block& block, const Clusters& clusters)
{
    unsigned count = 0;
    for (auto id : block)
        count += clusters[id].output;
    return count;
}
void remove_cluster_from_block(Block& block, std::size_t id)
{
    // Removal shifts subsequent entries; insertion appends.
    const auto it = std::find(block.begin(), block.end(), id);
    if (it == block.end())
        throw std::logic_error("partition list membership invariant");
    block.erase(it);
    // Compact unique output-enable records only when the last member leaves.
    // Their order survives ordinary list swaps but changes when a whole group
    // leaves and re-enters.
    const auto& c = block.clusters->at(id);
    // Release only the first matching physical row.
    if (!c.oe_product.empty())
        for (auto& resource : block.oe_resources)
            if (resource.key == c.oe_product) {
                if (--resource.count == 0)
                    resource.key.clear();
                break;
            }
    if (c.fixed_half < 0 && !c.oe_product.empty() &&
        std::none_of(
            block.begin(), block.end(),
            [&](auto member) {
                const auto& other = block.clusters->at(member);
                return other.fixed_half < 0 && other.oe_product == c.oe_product;
            })) {
        auto group = std::find(block.oe_order.begin(), block.oe_order.end(), c.oe_product);
        if (group != block.oe_order.end())
            block.oe_order.erase(group);
    }
}
unsigned maximum_block_fanin_count(const std::array<Block, 2>& blocks, const Clusters& clusters)
{
    // Compare maximum block fanin counts as unsigned values.
    return static_cast<unsigned>(
        std::max(collect_block_fanins(blocks[0], clusters).size(), collect_block_fanins(blocks[1], clusters).size()));
}
unsigned total_block_fanin_count(const std::array<Block, 2>& blocks, const Clusters& clusters)
{
    // Rescue swaps compare total fanin counts; the later optimization pass
    // compares the maximum.
    return static_cast<unsigned>(collect_block_fanins(blocks[0], clusters).size() +
                                 collect_block_fanins(blocks[1], clusters).size());
}
bool block_accepts_cluster(const Block& block, const Clusters& clusters, std::size_t incoming, unsigned capacity)
{
    return resource_limits_allow(block, clusters, incoming) &&
           count_output_clusters(block, clusters) + clusters[incoming].output <= capacity;
}
unsigned count_resource_violations(const Block& block, const Clusters& clusters, std::size_t incoming,
                                   unsigned capacity)
{
    unsigned count  = block.size() >= 16;
    unsigned groups = clusters[incoming].product_groups;
    for (auto id : block)
        groups += clusters[id].product_groups;
    count += groups > 16;
    count += count_output_clusters(block, clusters) + clusters[incoming].output > capacity;
    count += !reset_compatible(block, clusters, incoming);
    count += !oe_compatible(block, clusters, incoming);
    auto inputs = collect_block_fanins(block, clusters);
    inputs.insert(clusters[incoming].fanins.begin(), clusters[incoming].fanins.end());
    count += inputs.size() > 26;
    return count;
}
bool swap_resources(const std::array<Block, 2>& blocks, const Clusters& clusters, unsigned x, std::size_t a, unsigned y,
                    std::size_t b, const std::array<unsigned, 2>& capacity)
{
    if (!reset_compatible(blocks[x], clusters, b, a) || !reset_compatible(blocks[y], clusters, a, b))
        return false;
    if (!oe_compatible(blocks[x], clusters, b, a) || !oe_compatible(blocks[y], clusters, a, b))
        return false;
    if (count_output_clusters(blocks[x], clusters) - clusters[a].output + clusters[b].output > capacity[x] ||
        count_output_clusters(blocks[y], clusters) - clusters[b].output + clusters[a].output > capacity[y])
        return false;
    unsigned ga = 0, gb = 0;
    for (auto id : blocks[x])
        ga += clusters[id].product_groups;
    for (auto id : blocks[y])
        gb += clusters[id].product_groups;
    return ga - clusters[a].product_groups + clusters[b].product_groups <= 16 &&
           gb - clusters[b].product_groups + clusters[a].product_groups <= 16;
}
int select(const std::array<Block, 2>& blocks, const Clusters& clusters, std::size_t incoming, bool spread,
           const std::array<unsigned, 2>& capacity)
{
    int chosen          = -1;
    unsigned best_score = 0, best_inputs = 0;
    for (int b = 0; b < 2; ++b) {
        if (!resource_limits_allow(blocks[b], clusters, incoming))
            continue;
        if (count_output_clusters(blocks[b], clusters) + clusters[incoming].output > capacity[b])
            continue;
        const auto inputs = collect_block_fanins(blocks[b], clusters);
        unsigned score    = 0;
        for (auto input : clusters[incoming].fanins)
            score += inputs.count(input) != 0;
        bool better = chosen == -1;
        if (!better) {
            // Balanced partitioning prefers more remaining cells, then more shared
            // fanins, then fewer existing fanins unless the current choice is full.
            if (spread && blocks[b].size() != blocks[chosen].size())
                better = blocks[b].size() < blocks[chosen].size();
            else if (score != best_score)
                better = score > best_score;
            else
                better = best_inputs == 26 || best_inputs > inputs.size();
        }
        if (better) {
            chosen      = b;
            best_score  = score;
            best_inputs = static_cast<unsigned>(inputs.size());
        }
    }
    return chosen;
}
bool rescue(std::array<Block, 2>& blocks, const Clusters& clusters, std::size_t incoming, bool spread,
            const std::array<unsigned, 2>& capacity)
{
    // Save the incoming cluster's failure masks and order blocks by the
    // number of failed resource checks.
    std::array<unsigned, 2> order = {0, 1};
    if (count_resource_violations(blocks[1], clusters, incoming, capacity[1]) <
        count_resource_violations(blocks[0], clusters, incoming, capacity[0]))
        std::swap(order[0], order[1]);
    for (unsigned pass = 0; pass < 2; ++pass) {
        // Move a nonfixed cluster and retain the move even if it does not yet
        // make room for the incoming cluster.
        for (auto x : order)
            for (std::size_t p = blocks[x].size(); p > 0; --p) {
                const auto a = blocks[x][p - 1];
                if (clusters[a].fixed_block >= 0)
                    continue;
                const auto y = 1 - x;
                if (!block_accepts_cluster(blocks[y], clusters, a, capacity[y]))
                    continue;
                blocks[y].push_back(a);
                remove_cluster_from_block(blocks[x], a);
                const int destination = select(blocks, clusters, incoming, spread, capacity);
                if (destination >= 0) {
                    blocks[destination].push_back(incoming);
                    return true;
                }
            }
        // Read live pairs in reverse order. Failed destination trials append
        // restored members, so rollback order affects later trials.
        const auto x = order[0], y = order[1];
        for (std::size_t ai = blocks[x].size(); ai > 0; --ai) {
            if (clusters[blocks[x][ai - 1]].fixed_block >= 0)
                break;
            for (std::size_t bi = blocks[y].size(); bi > 0; --bi) {
                const auto a = blocks[x][ai - 1], b = blocks[y][bi - 1];
                if (clusters[b].fixed_block >= 0)
                    break;
                if (!swap_resources(blocks, clusters, x, a, y, b, capacity))
                    continue;
                const auto before = total_block_fanin_count(blocks, clusters);
                remove_cluster_from_block(blocks[x], a);
                if (!block_accepts_cluster(blocks[x], clusters, b, capacity[x])) {
                    blocks[x].push_back(a);
                    continue;
                }
                blocks[x].push_back(b);
                remove_cluster_from_block(blocks[y], b);
                if (!block_accepts_cluster(blocks[y], clusters, a, capacity[y])) {
                    remove_cluster_from_block(blocks[x], b);
                    blocks[x].push_back(a);
                    blocks[y].push_back(b);
                    continue;
                }
                blocks[y].push_back(a);
                // Try the two swapped blocks in traversal order, bypassing the
                // balanced/common-input selector.
                if (block_accepts_cluster(blocks[x], clusters, incoming, capacity[x])) {
                    blocks[x].push_back(incoming);
                    return true;
                }
                if (block_accepts_cluster(blocks[y], clusters, incoming, capacity[y])) {
                    blocks[y].push_back(incoming);
                    return true;
                }
                if (before < total_block_fanin_count(blocks, clusters)) {
                    remove_cluster_from_block(blocks[y], a);
                    remove_cluster_from_block(blocks[x], b);
                    blocks[x].push_back(a);
                    blocks[y].push_back(b);
                }
            }
        }
    }
    return false;
}
}  // namespace

void rank_mach111_single_group(std::vector<Mach111PartitionCluster>& clusters)
{
    std::map<std::uint16_t, unsigned> fanout;
    for (const auto& cluster : clusters) {
        const std::set<std::uint16_t> unique(cluster.fanins.begin(), cluster.fanins.end());
        if (unique.size() != cluster.fanins.size())
            throw std::invalid_argument("duplicate partition fanin");
        for (auto input : cluster.fanins)
            ++fanout[input];
    }
    for (auto& cluster : clusters) {
        // Rank clusters using reset weight 10000, output weight 2500, group
        // count and fanin weight 10. Sum global fanout for the second unsigned
        // 16-bit sorting key.
        cluster.order.rank = static_cast<std::uint16_t>(
            ((cluster.reset || cluster.implicit_storage_controls) ? 10000 : 0) +
            ((cluster.preset || cluster.implicit_storage_controls) ? 10000 : 0) + (cluster.output ? 2500 : 0) +
            cluster.product_groups + 10 * cluster.fanins.size());
        unsigned sum = 0;
        for (auto input : cluster.fanins)
            sum += fanout.at(input);
        cluster.order.fanout_sum = static_cast<std::uint16_t>(sum);
    }
}

void rank_mach_modern_phase(std::vector<Mach111PartitionCluster>& clusters, unsigned phase, bool product_pressure,
                            bool retain_ground_controls)
{
    if (phase > 4)
        throw std::invalid_argument("invalid modern partition phase");
    rank_mach111_single_group(clusters);
    if (!phase)
        return;
    for (auto& c : clusters) {
        const bool omit_zero = !retain_ground_controls && (phase == 2 || phase == 3);
        unsigned rank =
            ((c.reset && (!omit_zero || *c.reset != 0)) || (!omit_zero && c.implicit_storage_controls) ? 10000 : 0) +
            ((c.preset && (!omit_zero || *c.preset != 0)) || (!omit_zero && c.implicit_storage_controls) ? 10000 : 0);
        rank += phase == 1 ? c.data_terms * (product_pressure ? 80 : 30) : c.product_groups * 50;
        if (c.output && phase != 4)
            rank += 2500;
        const unsigned inputs = static_cast<unsigned>(c.fanins.size());
        if (phase == 1)
            rank += inputs * 10;
        else {
            const unsigned threshold = phase == 2 ? 12 : phase == 3 ? 6 : 9;
            rank += inputs * (inputs > 2 * threshold ? 900 : inputs > threshold ? 450 : 300);
        }
        c.order.rank = static_cast<std::uint16_t>(rank);
    }
}

std::vector<unsigned> ground_implicit_partition_controls(std::vector<Mach111PartitionCluster>& clusters)
{
    std::vector<unsigned> changed(clusters.size());
    for (std::size_t i = 0; i < clusters.size(); ++i) {
        auto& c = clusters[i];
        if (!c.macrocell || !c.clocked_storage)
            continue;
        if (!c.reset) {
            c.reset = c.global_reset = 0;
            changed[i] |= 1;
        }
        if (!c.preset) {
            c.preset = c.global_preset = 0;
            changed[i] |= 2;
        }
    }
    return changed;
}

std::array<std::vector<std::size_t>, 2> partition_mach111_single_group(
    const std::vector<Mach111PartitionCluster>& clusters, bool spread, const std::array<unsigned, 2>& output_capacity,
    std::array<std::vector<std::string>, 2>* oe_order)
{
    std::vector<PartitionOrderKey> keys;
    for (const auto& cluster : clusters) {
        const std::set<std::uint16_t> unique(cluster.fanins.begin(), cluster.fanins.end());
        if (unique.size() != cluster.fanins.size())
            throw std::invalid_argument("duplicate partition fanin");
        keys.push_back(cluster.order);
        if (cluster.fixed_block < -1 || cluster.fixed_block > 1)
            throw std::invalid_argument("invalid fixed partition block");
        if (cluster.product_groups < 1 || cluster.product_groups > 3)
            throw std::invalid_argument("invalid MACH111 partition product group count");
    }
    std::array<Block, 2> blocks;
    for (auto& block : blocks)
        block.clusters = &clusters;
    auto sorted = partition_signal_order(keys);
    // Seed constrained clusters in sorted order and mark the appended
    // entries fixed. They contribute to resources and fanin counts but
    // never participate in swaps.
    for (auto id : sorted)
        if (clusters[id].fixed_block >= 0) {
            const unsigned b = static_cast<unsigned>(clusters[id].fixed_block);
            if (!resource_limits_allow(blocks[b], clusters, id) ||
                count_output_clusters(blocks[b], clusters) + clusters[id].output > output_capacity[b])
                throw std::runtime_error("fixed partition resources exceeded");
            blocks[b].push_back(id);
        }
    const auto seeded = blocks;
    std::vector<bool> promoted_predecessor(clusters.size());
    for (;;) {
        check_fitting_budget();
        bool complete = true;
        for (std::size_t p = 0; p < sorted.size(); ++p) {
            const auto id = sorted[p];
            if (clusters[id].fixed_block >= 0)
                continue;
            const int b = select(blocks, clusters, id, spread, output_capacity);
            if (b >= 0) {
                blocks[b].push_back(id);
                continue;
            }
            if (rescue(blocks, clusters, id, spread, output_capacity))
                continue;
            // Promote the failed cluster ahead of the latest predecessor not yet
            // used for promotion. Re-sort and restore fixed seeds before retrying.
            bool promoted = false;
            for (std::size_t previous = p; previous > 0; --previous) {
                const auto earlier = sorted[previous - 1];
                if (promoted_predecessor[earlier])
                    continue;
                if (keys[earlier].rank == keys[id].rank)
                    keys[id].fanout_sum = static_cast<std::uint16_t>(keys[earlier].fanout_sum - 1);
                else
                    keys[id].rank = static_cast<std::uint16_t>(keys[earlier].rank + 1);
                promoted_predecessor[earlier] = true;
                promoted                      = true;
                break;
            }
            if (!promoted) {
                // Production placement shares the complete generic
                // kernel. Retain the pre-ranked API's first strategy,
                // then use that same kernel for all remaining ranks.
                std::vector<std::vector<std::string>> ordered_oe;
                const auto result = partition_mach_family_first_success(
                    mach_device_layout("MACH111"), clusters, {output_capacity[0], output_capacity[1]}, spread, nullptr,
                    nullptr, nullptr, nullptr, nullptr, &ordered_oe);
                if (oe_order)
                    for (unsigned b = 0; b < 2; ++b)
                        (*oe_order)[b] = std::move(ordered_oe[b]);
                return {result[0], result[1]};
            }
            sorted   = partition_signal_order(keys);
            blocks   = seeded;
            complete = false;
            break;
        }
        if (complete)
            break;
    }
    // Visit each list backwards. Adding a cluster to a second block cannot
    // lower maximum fanin before removing the old membership, so undo each
    // strict-improvement trial. Preserve its output-enable row effects: an
    // insertion may use an earlier hole than an existing key, and removal
    // releases only the newly encountered first match.
    for (unsigned x = 0; x < 2; ++x)
        for (std::size_t pos = blocks[x].size(); pos > 0; --pos) {
            const auto id = blocks[x][pos - 1];
            if (clusters[id].fixed_block >= 0)
                continue;
            const unsigned target = 1 - x;
            if (block_accepts_cluster(blocks[target], clusters, id, output_capacity[target])) {
                blocks[target].push_back(id);
                remove_cluster_from_block(blocks[target], id);
            }
        }

    // Read live lists at every index: successful swaps append entries and
    // change subsequent pairs. Retain equal-cost swaps.
    for (std::size_t ai = blocks[0].size(); ai > 0; --ai) {
        if (clusters[blocks[0][ai - 1]].fixed_block >= 0)
            break;
        for (std::size_t bi = blocks[1].size(); bi > 0; --bi) {
            const auto a = blocks[0][ai - 1], b = blocks[1][bi - 1];
            if (clusters[b].fixed_block >= 0)
                break;
            if (!reset_compatible(blocks[0], clusters, b, a) || !reset_compatible(blocks[1], clusters, a, b))
                continue;
            if (!oe_compatible(blocks[0], clusters, b, a) || !oe_compatible(blocks[1], clusters, a, b))
                continue;
            // Check output resources before removing either member.
            if (count_output_clusters(blocks[0], clusters) - clusters[a].output + clusters[b].output >
                    output_capacity[0] ||
                count_output_clusters(blocks[1], clusters) - clusters[b].output + clusters[a].output >
                    output_capacity[1])
                continue;
            unsigned groups_a = 0, groups_b = 0;
            for (auto id : blocks[0])
                groups_a += clusters[id].product_groups;
            for (auto id : blocks[1])
                groups_b += clusters[id].product_groups;
            if (groups_a - clusters[a].product_groups + clusters[b].product_groups > 16 ||
                groups_b - clusters[b].product_groups + clusters[a].product_groups > 16)
                continue;
            const auto before = maximum_block_fanin_count(blocks, clusters);
            remove_cluster_from_block(blocks[0], a);
            if (!resource_limits_allow(blocks[0], clusters, b)) {
                blocks[0].push_back(a);
                continue;
            }
            blocks[0].push_back(b);
            remove_cluster_from_block(blocks[1], b);
            if (!resource_limits_allow(blocks[1], clusters, a)) {
                remove_cluster_from_block(blocks[0], b);
                blocks[0].push_back(a);
                blocks[1].push_back(b);
                continue;
            }
            blocks[1].push_back(a);
            if (before < maximum_block_fanin_count(blocks, clusters)) {
                remove_cluster_from_block(blocks[1], a);
                remove_cluster_from_block(blocks[0], b);
                blocks[0].push_back(a);
                blocks[1].push_back(b);
            }
        }
    }
    if (oe_order)
        for (unsigned b = 0; b < 2; ++b)
            (*oe_order)[b] = blocks[b].oe_order;
    return {static_cast<std::vector<std::size_t>>(blocks[0]), static_cast<std::vector<std::size_t>>(blocks[1])};
}
}  // namespace machxl
