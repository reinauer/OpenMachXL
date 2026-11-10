#include "fitter_order.hpp"
#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace machxl {
std::vector<unsigned> placement_reverse_visit_costs(const std::vector<FitterOrderKey>& physical,
                                                    const std::vector<FitterOrderKey>& extra,
                                                    const std::vector<std::vector<std::size_t>>& outputs)
{
    auto keys = physical;
    keys.insert(keys.end(), extra.begin(), extra.end());
    std::vector<std::size_t> imported(keys.size());
    std::iota(imported.begin(), imported.end(), 0);
    std::sort(imported.begin(), imported.end(), [&](auto a, auto b) { return keys[a].serial < keys[b].serial; });
    std::vector<FitterOrderKey> sorted_keys;
    for (auto i : imported)
        sorted_keys.push_back(keys[i]);
    const auto sorted = fitter_signal_order(sorted_keys, false);
    std::vector<std::size_t> order;
    for (auto i : sorted)
        order.push_back(imported[i]);
    std::vector<int> output_block(keys.size(), -1);
    for (unsigned b = 0; b < outputs.size(); ++b)
        for (auto i : outputs[b])
            output_block.at(i) = int(b);
    std::vector<unsigned> cursor(outputs.size());
    for (auto& i : order)
        if (output_block[i] >= 0) {
            const auto b = unsigned(output_block[i]);
            i            = outputs[b][cursor[b]++];
        }
    std::vector<unsigned> costs(physical.size());
    for (unsigned p = 0; p < order.size(); ++p)
        if (order[p] < physical.size())
            costs[order[p]] = unsigned(order.size()) - p;
    return costs;
}
namespace {
bool is_input_class(const FitterOrderKey& key)
{
    // Ordinary, clock and registered inputs share the input sorting class.
    return key.type == 1 || key.type == 2 || key.type == 8 || key.type == 9;
}
bool is_pin_related_signal(const FitterOrderKey& key)
{
    switch (key.type) {
    case 0:
    case 1:
    case 2:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
        return true;
    default:
        return false;
    }
}
int signed_word_difference(std::int16_t a, std::int16_t b)
{
    // DOS comparisons wrap in 16 bits; avoid relying on native int size.
    const auto value = static_cast<std::uint16_t>(static_cast<int>(a) - b);
    return value < 0x8000 ? value : static_cast<int>(value) - 0x10000;
}

// Preserve comparison direction, equal-key swaps and partition traversal.
// These details affect placement when the comparator contains equal keys.
template <class Compare>
void compatibility_quicksort(std::vector<std::size_t>& order, std::size_t first, std::size_t count, const Compare& cmp)
{
    while (count > 2) {
        std::size_t last         = first + count - 1;
        const std::size_t middle = first + count / 2;
        if (cmp(order[middle], order[last]) > 0)
            std::swap(order[last], order[middle]);
        if (cmp(order[middle], order[first]) > 0)
            std::swap(order[first], order[middle]);
        else if (cmp(order[first], order[last]) > 0)
            std::swap(order[last], order[first]);
        if (count == 3) {
            std::swap(order[middle], order[first]);
            return;
        }
        std::size_t equal_end = first + 1, scan = equal_end;
        bool reached_last = false;
        do {
            int comparison;
            while ((comparison = cmp(order[scan], order[first])) <= 0) {
                if (comparison == 0)
                    std::swap(order[equal_end++], order[scan]);
                if (scan >= last) {
                    reached_last = true;
                    break;
                }
                ++scan;
            }
            if (reached_last)
                break;
            for (; scan < last; --last) {
                comparison = cmp(order[first], order[last]);
                if (comparison >= 0) {
                    std::swap(order[last], order[scan]);
                    if (comparison != 0) {
                        ++scan;
                        --last;
                    }
                    break;
                }
            }
        } while (scan < last);
        if (cmp(order[scan], order[first]) <= 0)
            ++scan;
        for (std::size_t left = first, right = scan - 1; left < equal_end && equal_end <= right; ++left, --right)
            std::swap(order[right], order[left]);
        const std::size_t left_count  = scan - equal_end;
        const std::size_t right_count = first + count - scan;
        if (right_count < left_count) {
            compatibility_quicksort(order, scan, right_count, cmp);
            count = left_count;
        } else {
            compatibility_quicksort(order, first, left_count, cmp);
            first = scan;
            count = right_count;
        }
    }
    if (count == 2 && cmp(order[first], order[first + 1]) > 0)
        std::swap(order[first + 1], order[first]);
}
}  // namespace

std::vector<std::size_t> original_unsigned_order(const std::vector<unsigned>& keys)
{
    std::vector<std::size_t> order(keys.size());
    std::iota(order.begin(), order.end(), 0);
    compatibility_quicksort(order, 0, order.size(), [&](std::size_t a, std::size_t b) {
        return keys[a] == keys[b] ? 0 : keys[a] < keys[b] ? -1 : 1;
    });
    return order;
}

int compare_fitter_signals(const FitterOrderKey& a, const FitterOrderKey& b, bool prioritize_constraints)
{
    const bool ai = is_input_class(a), bi = is_input_class(b);
    if (!ai && !bi) {
        if (prioritize_constraints) {
            const bool ac = a.fixed_pin != -1 || a.fixed_node != -1;
            const bool bc = b.fixed_pin != -1 || b.fixed_node != -1;
            if (ac != bc)
                return ac ? -1 : 1;
            if (a.has_group != b.has_group)
                return a.has_group ? -1 : 1;
        }
        const bool ap = is_pin_related_signal(a), bp = is_pin_related_signal(b);
        if (ap && !bp)
            return b.fixed_node == -1 ? -1 : 1;
        if (!ap && bp)
            return a.fixed_node == -1 ? 1 : -1;
        if ((a.fixed_node == -1) != (b.fixed_node == -1))
            return a.fixed_node == -1 ? 1 : -1;
        if (a.fanout_count != b.fanout_count)
            return signed_word_difference(b.fanout_count, a.fanout_count);
        if (a.product_terms != b.product_terms)
            return signed_word_difference(b.product_terms, a.product_terms);
        return signed_word_difference(b.serial, a.serial);
    }
    if (ai != bi)
        return ai ? 1 : -1;
    if ((a.block < 0) != (b.block < 0))
        return a.block < 0 ? 1 : -1;
    if ((a.fixed_pin == -1) != (b.fixed_pin == -1))
        return a.fixed_pin == -1 ? 1 : -1;
    if (a.type == b.type) {
        if (a.fanout_count != b.fanout_count)
            return signed_word_difference(b.fanout_count, a.fanout_count);
        return signed_word_difference(b.serial, a.serial);
    }
    if (a.type == 9)
        return -1;
    if (b.type == 9)
        return 1;
    return a.type == 2 ? -1 : 1;
}

std::vector<std::size_t> fitter_signal_order(const std::vector<FitterOrderKey>& keys, bool prioritize_constraints)
{
    // Preserve the DOS signal-array capacity limit.
    if (keys.size() > 16383)
        throw std::invalid_argument("original fitter signal array capacity exceeded");
    std::vector<std::size_t> order(keys.size());
    std::iota(order.begin(), order.end(), 0);
    compatibility_quicksort(order, 0, order.size(), [&](std::size_t a, std::size_t b) {
        return compare_fitter_signals(keys[a], keys[b], prioritize_constraints);
    });
    return order;
}

std::vector<std::size_t> partition_signal_order(const std::vector<PartitionOrderKey>& keys)
{
    // Preserve the DOS partition-array capacity limit.
    if (keys.size() > 1365)
        throw std::invalid_argument("original fitter cluster array capacity exceeded");
    std::vector<std::size_t> order(keys.size());
    std::iota(order.begin(), order.end(), 0);
    compatibility_quicksort(order, 0, order.size(), [&](std::size_t a, std::size_t b) {
        const auto& left  = keys[a];
        const auto& right = keys[b];
        if (left.rank != right.rank)
            return left.rank > right.rank ? -1 : 1;
        if (left.fanout_sum != right.fanout_sum)
            return left.fanout_sum < right.fanout_sum ? -1 : 1;
        if (left.signal_id != right.signal_id)
            return left.signal_id < right.signal_id ? -1 : 1;
        return 0;
    });
    return order;
}

std::vector<std::size_t> physical_output_order(const std::vector<PhysicalOutputOrderKey>& keys)
{
    if (keys.size() > 16383)
        throw std::invalid_argument("original output array capacity exceeded");
    std::vector<std::size_t> order(keys.size());
    std::iota(order.begin(), order.end(), 0);
    compatibility_quicksort(order, 0, order.size(), [&](std::size_t a, std::size_t b) {
        const auto& x = keys[a];
        const auto& y = keys[b];
        if (x.fixed_node != y.fixed_node)
            return x.fixed_node ? -1 : 1;
        if (x.fixed_pin != y.fixed_pin)
            return x.fixed_pin ? -1 : 1;
        if (x.oe_members != y.oe_members)
            return x.oe_members > y.oe_members ? -1 : 1;
        if (x.product_terms != y.product_terms)
            return x.product_terms > y.product_terms ? -1 : 1;
        if (x.xor_product != y.xor_product)
            return x.xor_product ? -1 : 1;
        if (x.fanout_count != y.fanout_count)
            return x.fanout_count > y.fanout_count ? -1 : 1;
        return x.name.compare(y.name);
    });
    return order;
}
}  // namespace machxl
