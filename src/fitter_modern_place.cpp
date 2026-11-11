#include "fitter_modern_place.hpp"
#include "fitter_budget.hpp"
#include "fitter_family_place.hpp"
#include "fitter_iteration.hpp"
#include "fitter_output_search.hpp"
#include <algorithm>
#include <functional>
#include <set>
#include <stdexcept>

namespace machxl {
namespace {
[[noreturn]] void fail(const std::string& reason)
{
    throw std::runtime_error("MACH3/4 placement: " + reason);
}
// Allocate synchronous four-term groups and their single auxiliary product.
// Working and committed ownership differ: a successful displaced frame
// remains committed if its parent fails.
struct ModernProducts
{
    struct Group
    {
        int main = -1, aux = -1;
        bool data = false, coupled = false;
    };
    std::array<Group, 16> groups, saved;
    std::array<int, 16> demand{}, xor_demand{};
    std::array<bool, 16> active{};
    std::array<int, 16> capacity;
    ModernProducts() { capacity.fill(4); }
    static std::vector<int> candidates(int cell)
    {
        std::vector<int> result;
        for (int delta : {0, 1, 2, -1})
            if (cell + delta >= 0 && cell + delta < 16)
                result.push_back(cell + delta);
        return result;
    }
    std::pair<int, int> remaining(int cell, std::size_t before = 16) const
    {
        int data = demand[cell], xr = xor_demand[cell];
        auto list = candidates(cell);
        for (std::size_t n = 0; n < std::min(before, list.size()); ++n) {
            const int g   = list[n];
            const auto& r = groups[g];
            if (r.main == cell)
                data -= capacity[g];
            if (r.aux == cell) {
                if (r.data)
                    data -= 1;
                else if (g == cell)
                    xr -= 1;
            }
        }
        return {data, xr};
    }
    void clear_aux(Group& g)
    {
        g.aux     = -1;
        g.data    = false;
        g.coupled = false;
    }
    void commit(int cell)
    {
        int data = demand[cell], xr = xor_demand[cell];
        const auto list = candidates(cell);
        for (int g : list) {
            auto& r   = groups[g];
            auto& old = saved[g];
            if (g == cell && r.aux == cell && xor_demand[cell] == 0 && demand[cell] <= 1) {
                --data;
                r.data    = true;
                r.coupled = false;
                if (r.main == cell)
                    r.main = old.main = -1;
                old.aux     = r.aux;
                old.data    = true;
                old.coupled = false;
                continue;
            }
            if (r.main == cell) {
                if (data < 1)
                    r.main = old.main = -1;
                else {
                    old.main = cell;
                    data -= capacity[g];
                }
            } else if (old.main == cell)
                old.main = -1;
            if (r.aux == cell) {
                if (xr < 1) {
                    if (data < 1) {
                        clear_aux(r);
                        clear_aux(old);
                    } else if (r.data) {
                        old.aux     = r.aux;
                        old.data    = r.data;
                        old.coupled = r.coupled;
                        --data;
                    }
                } else if (g == cell) {
                    old.aux     = r.aux;
                    old.data    = r.data;
                    old.coupled = r.coupled;
                    --xr;
                } else if (data < 1) {
                    clear_aux(r);
                    clear_aux(old);
                } else {
                    old.aux     = r.aux;
                    old.data    = r.data;
                    old.coupled = r.coupled;
                    --data;
                }
            } else if (old.aux == cell)
                clear_aux(old);
        }
        // Original second pass releases unnecessary borrowed auxiliary
        // products, but deliberately retains the cell's own product.
        data = demand[cell];
        for (int g : list)
            if (groups[g].main == cell)
                data -= capacity[g];
        for (int g : list)
            if (groups[g].aux == cell) {
                if (g == cell) {
                    if (groups[g].data && data > 0)
                        --data;
                } else if (data < 1) {
                    clear_aux(groups[g]);
                    clear_aux(saved[g]);
                } else
                    --data;
            }
    }
    // Check and claim resources outside the contested group.
    bool reserve_except(int cell, int excluded, bool check_only, int incoming_need)
    {
        int left = demand[cell], last = -1;
        const auto list = candidates(cell);
        for (int g : list) {
            if (left <= 0)
                break;
            if (g == excluded)
                continue;
            last          = g;
            const auto& r = groups[g];
            if (r.main == cell)
                left -= capacity[g];
            if (left > 0 && r.aux == cell && r.data)
                --left;
            if (left > 0 && r.main < 0)
                left -= capacity[g];
            if (left > 0 && r.aux < 0 && (r.main < 0 || r.main == cell))
                --left;
        }
        if (left > 0)
            return false;
        if (check_only)
            return true;
        // Use the contested cell mode with the last candidate's capacity.
        // Even a free last candidate is limited to two products for this
        // asynchronous capacity check.
        if (last < 0 || capacity[excluded] < incoming_need)
            return false;
        left = demand[cell];
        for (int g : list) {
            if (left <= 0)
                break;
            if (g == excluded)
                continue;
            auto& r = groups[g];
            if (r.main == cell)
                left -= capacity[g];
            if (left > 0 && r.aux == cell && r.data)
                --left;
            if (left > 0 && r.main < 0) {
                left -= capacity[g];
                r.main = cell;
            }
            if (left > 0 && r.aux < 0 && (r.main < 0 || r.main == cell)) {
                --left;
                r.aux     = cell;
                r.data    = true;
                r.coupled = demand[cell] > 1;
            }
        }
        return true;
    }
    // After preferring free resources, transfer a main group from an ancestor
    // frame. The ancestor recalculates its remaining demand when it resumes.
    void exchange_ancestor(int cell, int g, int needed)
    {
        const int ancestor = groups[g].main;
        if (capacity[ancestor] == capacity[cell]) {
            if (!reserve_except(ancestor, g, false, needed))
                return;
        } else if (capacity[ancestor] == 4 && capacity[cell] == 2) {
            if (reserve_except(cell, g, true, needed))
                return;
        }
        auto& r = groups[g];
        r.main  = cell;
        if (r.aux == ancestor && r.data && r.coupled)
            clear_aux(r);
        needed -= capacity[g];
        if (needed > 0 && r.aux < 0) {
            r.aux     = cell;
            r.data    = true;
            r.coupled = true;
        }
    }
    // A satisfied neighbour can exchange a smaller group for one of this
    // frame's groups. Free or coupled auxiliaries join both sides only when
    // both groups permit exchange. Otherwise charge a one-sided auxiliary
    // against the other owner's demand.
    void exchange_satisfied(int cell)
    {
        int needed = remaining(cell).first;
        if (needed <= 0)
            return;
        for (int g : candidates(cell)) {
            const int other = groups[g].main;
            if (other < 0 || other == cell)
                continue;
            const auto balance = remaining(other);
            if (balance.first > 0 || balance.second > 0)
                continue;
            int other_needed = balance.first;
            for (int h : candidates(other)) {
                if (groups[h].main != cell)
                    continue;
                const bool ga = groups[g].aux < 0 || (groups[g].data && groups[g].coupled);
                const bool ha = groups[h].aux < 0 || (groups[h].data && groups[h].coupled);
                int gain = capacity[g], loss = capacity[h];
                if (ga && ha) {
                    ++gain;
                    ++loss;
                } else {
                    if (ga)
                        ++other_needed;
                    if (ha)
                        ++needed;
                }
                if (needed + loss > gain || other_needed + gain > loss)
                    continue;
                std::swap(groups[g].main, groups[h].main);
                if (ga && ha) {
                    clear_aux(groups[g]);
                    clear_aux(groups[h]);
                    const auto a = remaining(cell), b = remaining(other);
                    if (a.first > 0 || a.second > 0) {
                        groups[g].aux  = cell;
                        groups[g].data = groups[g].coupled = true;
                    }
                    if (b.first > 0 || b.second > 0) {
                        groups[h].aux  = other;
                        groups[h].data = groups[h].coupled = true;
                    }
                } else {
                    if (groups[g].coupled)
                        clear_aux(groups[g]);
                    if (groups[h].coupled)
                        clear_aux(groups[h]);
                }
                return;
            }
        }
    }
    // On failed-child unwind, restore this candidate from the latest committed
    // owners while retaining an uncoupled XOR. A successful descendant may
    // have changed that snapshot before its parent failed.
    void restore_after_child(int g)
    {
        auto& r   = groups[g];
        auto& old = saved[g];
        r.main    = old.main;
        if (r.aux < 0 || r.data || r.coupled) {
            r.aux     = old.aux;
            r.data    = old.data;
            r.coupled = old.coupled;
        }
        if (r.aux >= 0 && r.data && r.coupled && r.main != r.aux) {
            const bool main_is_ancestor = r.main >= 0 && active[r.main];
            old.main                    = r.aux;
            if (main_is_ancestor)
                clear_aux(r);
            else
                r.main = r.aux;
        }
    }
    bool frame(int cell)
    {
        check_fitting_budget();
        active[cell]        = true;
        bool forced_failure = false;
        const auto list     = candidates(cell);
        for (std::size_t cursor = 0; cursor < list.size(); ++cursor) {
            // Initially count all owned groups; on continuation count only preceding
            // groups. An owner already satisfied by later groups must not acquire
            // earlier auxiliaries again.
            const auto all = remaining(cell);
            if (all.first <= 0 && all.second <= 0)
                break;
            auto need = remaining(cell, cursor);
            if (need.first <= 0 && need.second <= 0)
                break;
            const int g               = list[cursor];
            auto& r                   = groups[g];
            bool claimed_own_data_aux = false;
            if (g == cell && (xor_demand[cell] > 0 || demand[cell] <= 1)) {
                if (r.aux == cell) {
                    if (r.data)
                        --need.first;
                    else
                        --need.second;
                } else if (r.aux < 0 || !active[r.aux]) {
                    const int displaced  = r.aux;
                    r.aux                = cell;
                    r.data               = xor_demand[cell] == 0;
                    r.coupled            = false;
                    claimed_own_data_aux = displaced < 0 && r.data;
                    if (r.data)
                        --need.first;
                    else
                        --need.second;
                    if (displaced >= 0 && !frame(displaced)) {
                        restore_after_child(g);
                    }
                    // After a displaced frame returns, advance the parent cursor without
                    // revisiting its main group after auxiliary displacement.
                    if (displaced >= 0)
                        continue;
                }
            }
            // When a local one-term cell claims its free data auxiliary, its main
            // capacity falls from four to two. Revisit the main owner even without a
            // main-group transfer. Existing and XOR auxiliaries do not trigger this
            // continuation. Returning from the child advances the parent candidate.
            if (claimed_own_data_aux && capacity[cell] == 2 && r.main >= 0 && r.main != cell && !active[r.main]) {
                if (!frame(r.main)) {
                    forced_failure = true;
                    break;
                }
                continue;
            }
            need = remaining(cell, cursor);
            // Include any own auxiliary assigned in this iteration.
            if (r.aux == cell) {
                if (r.data)
                    --need.first;
                else
                    --need.second;
            }
            if (need.first > 0) {
                if (r.main == cell)
                    need.first -= capacity[g];
                else if (r.main < 0 || !active[r.main]) {
                    const int displaced = r.main;
                    if (r.aux == displaced && r.coupled)
                        clear_aux(r);
                    r.main = cell;
                    need.first -= capacity[g];
                    if (need.first > 0 && r.aux < 0) {
                        r.aux     = cell;
                        r.data    = true;
                        r.coupled = true;
                        --need.first;
                    }
                    if (displaced >= 0 && !frame(displaced)) {
                        // Unwinding a failed child preserves an uncoupled XOR auxiliary even
                        // if it was absent from the committed snapshot. The parent may still
                        // satisfy its data demand on a later group.
                        restore_after_child(g);
                    }
                    // The same continuation applies to main-group
                    // displacement; the child already settled this group.
                    if (displaced >= 0)
                        continue;
                }
                // Skip data-only synchronous ancestor frames.
                else if (demand[g] > 0 && (xor_demand[g] > 0 || capacity[g] == 2)) {
                    exchange_ancestor(cell, g, need.first);
                    need = remaining(cell, cursor + 1);
                }
                if (need.first > 0 && r.aux < 0 && r.main == cell) {
                    r.aux     = cell;
                    r.data    = true;
                    r.coupled = true;
                    --need.first;
                }
            }
        }
        if (!forced_failure && remaining(cell).first > 0)
            exchange_satisfied(cell);
        const auto need = remaining(cell);
        const bool ok   = !forced_failure && need.first <= 0 && need.second <= 0;
        if (ok)
            commit(cell);
        else
            for (int g : list) {
                if (groups[g].main == cell)
                    groups[g].main = saved[g].main;
                if (groups[g].aux == cell) {
                    groups[g].aux     = saved[g].aux;
                    groups[g].data    = saved[g].data;
                    groups[g].coupled = saved[g].coupled;
                }
            }
        active[cell] = false;
        return ok;
    }
    void validate() const
    {
        for (int cell = 0; cell < 16; ++cell)
            if (demand[cell] > 0 || xor_demand[cell] > 0) {
                const auto need = remaining(cell);
                if (need.first > 0 || need.second > 0)
                    fail("product ownership invariant after displacement");
            }
    }
    // Validate every occupied owner before committing the block snapshot.
    // A failed child can leave an earlier owner short even when the new cell
    // has enough products. Roll back and rebuild instead of accepting that
    // partial allocation.
    bool block_consistent() const
    {
        for (int cell = 0; cell < 16; ++cell)
            if (demand[cell] > 0 || xor_demand[cell] > 0) {
                int data = demand[cell], xr = xor_demand[cell];
                for (int g : candidates(cell)) {
                    const auto& r = groups[g];
                    if (r.aux == cell) {
                        if (xr < 1) {
                            if (data > 0) {
                                if (!(r.data && ((!r.coupled && g == cell) || (r.coupled && r.main == cell))))
                                    return false;
                                --data;
                            }
                        } else if (g == cell)
                            --xr;
                        else if (data > 0)
                            --data;
                    }
                    if (r.main == cell && data > 0)
                        data -= capacity[g];
                }
                if (data > 0 || xr > 0)
                    return false;
            }
        return true;
    }
    void remove(int cell)
    {
        for (unsigned g = 0; g < 16; ++g) {
            if (groups[g].main == cell)
                groups[g].main = -1;
            if (saved[g].main == cell)
                saved[g].main = -1;
            if (groups[g].aux == cell)
                clear_aux(groups[g]);
            if (saved[g].aux == cell)
                clear_aux(saved[g]);
        }
        demand[cell] = xor_demand[cell] = 0;
        capacity[cell]                  = 4;
        validate();
    }
    // Rebuild ownership in physical-cell order after allocation failure.
    // Retain successful rearrangements during candidate discovery; removing
    // the trial cell does not undo them.
    bool rebuild()
    {
        groups.fill(Group{});
        saved.fill(Group{});
        std::array<int, 16> left = demand;
        for (int c = 0; c < 16; ++c)
            if (xor_demand[c])
                groups[c].aux = c;
        const auto amount   = [&](int g) { return capacity[g] + int(groups[g].coupled); };
        const auto transfer = [&](int g, int c) {
            groups[g].main = c;
            if (groups[g].coupled)
                groups[g].aux = c;
        };
        // Swap two owners to make room for an additional withdrawal.
        const auto swap_room = [&](int c, int needed, int excluded) {
            for (int g : candidates(c)) {
                const int other = groups[g].main;
                if (other < 0 || other == c || g == excluded)
                    continue;
                for (int h : candidates(other)) {
                    if (h == g || h == excluded || groups[h].main != c)
                        continue;
                    const int a = amount(g), b = amount(h);
                    if (left[c] + b - a + needed <= 0 && left[other] - b + a <= 0) {
                        left[c] += b - a;
                        left[other] += a - b;
                        transfer(g, c);
                        transfer(h, other);
                        return true;
                    }
                }
            }
            return false;
        };
        // Replace a separately held auxiliary with an earlier main group,
        // subtracting its capacity from the donor.
        const auto replace_aux = [&](int c, int incoming) {
            for (int g : candidates(c))
                if (g < c && g < incoming && groups[g].main != c) {
                    if (groups[g].main < 0) {
                        groups[g].main = c;
                        left[c] -= capacity[g];
                        return true;
                    }
                    const int other = groups[g].main, units = amount(g);
                    if (left[other] + units <= 0) {
                        left[other] -= units;
                        if (groups[g].coupled)
                            clear_aux(groups[g]);
                        groups[g].main = c;
                        left[c] -= capacity[g];
                        return true;
                    }
                }
            return false;
        };
        for (int c = 0; c < 16; ++c)
            if (demand[c] > 0 || xor_demand[c] > 0) {
                if (demand[c] <= 1 && groups[c].aux < 0) {
                    groups[c].aux  = c;
                    groups[c].data = true;
                    --left[c];
                    saved[c] = groups[c];
                    continue;
                }
                const auto list = candidates(c);
                for (int g : list) {
                    auto& r = groups[g];
                    if (r.main < 0) {
                        r.main = c;
                        left[c] -= capacity[g];
                    }
                    if (r.aux < 0) {
                        if (g == c) {
                            r.aux     = c;
                            r.data    = true;
                            r.coupled = false;
                            --left[c];
                        }
                        if (r.main == c) {
                            if (r.aux < 0)
                                --left[c];
                            r.aux  = c;
                            r.data = r.coupled = true;
                        }
                    }
                    saved[g] = r;
                }
                bool placed = false;
                for (std::size_t start = 0; start < list.size(); ++start) {
                    for (std::size_t n = start; n < list.size(); ++n) {
                        const int g = list[n], other = groups[g].main;
                        if (other == c)
                            continue;
                        if (other < 0)
                            fail("unowned group during allocator rebuilding");
                        auto& r       = groups[g];
                        int units     = capacity[g];
                        bool move_aux = false;
                        if (other == g && xor_demand[other] == 0) {
                            if (demand[other] > 1) {
                                ++units;
                                move_aux = true;
                            } else {
                                r.data    = true;
                                r.coupled = false;
                            }
                        } else if (r.aux >= 0 && r.data && r.coupled) {
                            ++units;
                            move_aux = true;
                        }
                        if (left[other] + units <= 0 || swap_room(other, units, g)) {
                            left[other] += units;
                            left[c] -= units;
                            r.main = c;
                            if (move_aux) {
                                r.aux  = c;
                                r.data = r.coupled = true;
                            }
                        } else {
                            if (r.aux == other && r.data && !r.coupled && r.main == other)
                                r.coupled = true;
                            if (g == c && xor_demand[c] == 0 && left[other] + 1 <= 0 && r.data && r.coupled &&
                                demand[c] <= 1) {
                                ++left[other];
                                --left[c];
                                r.aux     = c;
                                r.data    = true;
                                r.coupled = false;
                            }
                        }
                    }
                    if (left[c] > 0 && left[c] <= 1)
                        for (int g : list) {
                            auto& r = groups[g];
                            if (r.aux != c && r.aux >= 0 && r.data && !r.coupled && replace_aux(r.aux, c)) {
                                --left[c];
                                r.aux  = c;
                                r.data = r.coupled = true;
                            }
                        }
                    if (left[c] > 0) {
                        for (int g : list)
                            groups[g] = saved[g];
                        for (int earlier = 0; earlier <= c; ++earlier)
                            left[earlier] = remaining(earlier).first;
                        continue;
                    }
                    // Prefer the larger exchanged group when both owners fit.
                    bool improved = false;
                    for (int g : list) {
                        const int other = groups[g].main;
                        if (other < 0 || other == c)
                            continue;
                        for (int h : candidates(other))
                            if (h != g && groups[h].main == c) {
                                const int a = amount(g), b = amount(h);
                                if (left[c] + b <= a && left[other] - b + a <= 0 && b < a) {
                                    left[c] += b - a;
                                    left[other] += a - b;
                                    transfer(g, c);
                                    transfer(h, other);
                                    improved = true;
                                    break;
                                }
                            }
                        if (improved)
                            break;
                    }
                    for (int g : list)
                        saved[g] = groups[g];
                    for (int g : list)
                        if (g > c && (demand[g] > 0 || xor_demand[g] > 0) && demand[g] <= 1 && groups[g].aux == c &&
                            groups[g].data && groups[g].coupled && left[c] + 1 <= 0) {
                            ++left[c];
                            clear_aux(groups[g]);
                            clear_aux(saved[g]);
                        }
                    placed = true;
                    break;
                }
                if (!placed)
                    return false;
            }
        for (int c = 0; c < 16; ++c)
            if (demand[c] > 0 || xor_demand[c] > 0)
                commit(c);
        for (int c = 0; c < 16; ++c)
            if (demand[c] > 0 || xor_demand[c] > 0) {
                const auto need = remaining(c);
                if (need.first > 0 || need.second > 0)
                    return false;
            }
        return true;
    }
    bool allocate(int cell, int terms, bool split, bool asynchronous)
    {
        const auto previous = groups, previous_saved = saved;
        capacity[cell]   = asynchronous ? 2 : 4;
        demand[cell]     = terms;
        xor_demand[cell] = split ? 1 : 0;
        if (frame(cell)) {
            if (block_consistent()) {
                validate();
                return true;
            }
        }
        groups = previous;
        saved  = previous_saved;
        if (rebuild()) {
            validate();
            return true;
        }
        groups         = previous;
        saved          = previous_saved;
        capacity[cell] = 4;
        demand[cell] = xor_demand[cell] = 0;
        return false;
    }
};
}  // namespace
static std::vector<int> complete_output_muxes(const std::vector<int>& used, unsigned ios)
{
    const auto node = [&](unsigned io, unsigned slot) {
        return ios == 16 ? (io / 8) * 8 + (io + slot) % 8 : (2 * io + slot) % 16;
    };
    const auto selection = [&](unsigned io, unsigned cell) {
        return ios == 16 ? int((cell + 8 - io) % 8) : int((cell + 16 - 2 * io) % 16);
    };
    if (used.size() % ios)
        fail("invalid output multiplexer block count");
    auto result = used;
    for (std::size_t base = 0; base < used.size(); base += ios) {
        // Set all output-mux bits when the block has no physical output,
        // including blocks containing only buried nodes.
        if (std::all_of(used.begin() + base, used.begin() + base + ios, [](int value) { return value < 0; })) {
            std::fill(result.begin() + base, result.begin() + base + ios, 7);
            continue;
        }
        std::vector<int> pins, owner(16, -1), selected(16, -1);
        std::vector<bool> fixed(16, false);
        for (unsigned io = 0; io < ios; ++io) {
            if (used[base + io] < 0) {
                pins.push_back(int(io));
                continue;
            }
            if (used[base + io] > 7)
                fail("invalid used output mux selection");
            unsigned cell = node(io, unsigned(used[base + io]));
            if (fixed[cell])
                fail("duplicate used output mux node");
            fixed[cell] = true;
            owner[cell] = int(cell);
        }
        // Try augmentation immediately for each unused pin. An occupied candidate
        // can displace an older unused pin before later free candidates are inspected.
        for (unsigned start = 0; start < pins.size(); ++start) {
            const auto old = selected;
            std::vector<int> predecessor(16, -1);
            std::vector<unsigned> cursor(ios, 0);
            unsigned signal = start;
            bool found      = false;
            for (;;) {
                check_fitting_budget();
                const unsigned pin = unsigned(pins[signal]);
                if (cursor[pin] < 8) {
                    const unsigned cell = node(pin, cursor[pin]++);
                    if (owner[cell] < 0) {
                        predecessor[cell] = int(signal);
                        int path          = int(cell);
                        do {
                            int next                 = predecessor[unsigned(path)];
                            owner[unsigned(path)]    = next;
                            selected[unsigned(next)] = path;
                            path                     = old[unsigned(next)];
                        } while (path >= 0);
                        found = true;
                        break;
                    }
                    if (!fixed[cell] && predecessor[cell] < 0) {
                        predecessor[cell]              = int(signal);
                        signal                         = unsigned(owner[cell]);
                        cursor[unsigned(pins[signal])] = 0;
                    }
                } else {
                    const int previous = old[signal];
                    if (previous < 0)
                        break;
                    signal = unsigned(predecessor[unsigned(previous)]);
                }
            }
            if (!found)
                fail("unused output multiplexer matching failed");
        }
        for (unsigned i = 0; i < pins.size(); ++i) {
            const unsigned pin = unsigned(pins[i]);
            result[base + pin] = selection(pin, unsigned(selected[i]));
        }
    }
    return result;
}
std::vector<int> complete_mach435_output_muxes(const std::vector<int>& used)
{
    return complete_output_muxes(used, 8);
}
std::vector<int> complete_modern_output_muxes(const MachDeviceLayout& d, const std::vector<int>& used)
{
    return complete_output_muxes(used, d.device == "MACH355" ? 16 : 8);
}
ModernPlacement place_mach_modern(const MachDeviceLayout& d, const std::vector<Mach111PlaceSignal>& signals,
                                  Mach111PlaceOptions options, const ModernPlacementCallback& route_callback,
                                  const ModernPlacementFailureCallback& failure_callback)
{
    if (!options.iteration) {
        PartitionIterationState iteration(options.iterate_partition);
        options.iteration = &iteration;
        for (;;) {
            FittingIterationCounters counters(d.blocks, options.iterate_partition);
            const auto previous = iteration.completed;
            try {
                return place_mach_modern(d, signals, options, route_callback, failure_callback);
            } catch (const PhysicalPlacementFailure&) {
                if (!iteration.remaining || iteration.completed == previous)
                    throw;
            } catch (const RepartitionRequested&) {
                if (!iteration.remaining || iteration.completed == previous)
                    throw PhysicalPlacementFailure("z7502: physical fitting unsuccessful after partition retries");
            }
        }
    }
    const unsigned ios = d.device == "MACH355" ? 16 : 8;
    if ((d.device != "MACH355" && d.device != "MACH435" && d.device != "MACH445" && d.device != "MACH446" &&
         d.device != "MACH465") ||
        d.cells != 16 || d.io_pins.size() != d.blocks * ios)
        fail("modern output-multiplexer geometry required");
    ModernPlacement r;
    r.cells.assign(signals.size(), -1);
    r.pins.assign(signals.size(), -1);
    r.input_registers.assign(signals.size(), -1);
    std::vector<int> input_blocks(signals.size(), -1);
    r.output_order.resize(d.blocks);
    r.product_group_owner.assign(d.blocks * d.cells, -1);
    r.output_muxes.assign(d.io_pins.size(), -1);
    r.product_aux_owner.assign(d.blocks * d.cells, -1);
    r.product_aux_data.assign(d.blocks * d.cells, false);
    r.asynchronous_cells.assign(d.blocks * d.cells, false);
    r.product_clock_cells.assign(d.blocks * d.cells, false);
    auto io_for_pin = [&](unsigned pin) {
        auto at = std::find(d.io_pins.begin(), d.io_pins.end(), pin);
        return at == d.io_pins.end() ? -1 : int(at - d.io_pins.begin());
    };
    auto clock_pin = [&](unsigned pin) {
        return std::find(d.clock_pins.begin(), d.clock_pins.end(), pin) != d.clock_pins.end();
    };
    auto data_pin = [&](unsigned pin) {
        return io_for_pin(pin) >= 0 ||
               std::find(d.dedicated_pins.begin(), d.dedicated_pins.end(), pin) != d.dedicated_pins.end();
    };
    std::vector<bool> occupied_pins(d.pins + 1), reserved_cells(d.blocks * d.cells), placed(d.blocks * d.cells);
    std::vector<FitterOrderKey> keys;
    std::vector<std::size_t> outputs, clocks, cluster_signals;
    std::set<std::size_t> paired_inputs;
    for (const auto& s : signals)
        if (s.input_register) {
            if (s.paired_input < 0 || unsigned(s.paired_input) >= signals.size())
                fail("invalid IPAIR input index");
            paired_inputs.insert(std::size_t(s.paired_input));
        }
    std::vector<Mach111PartitionCluster> clusters;
    for (std::size_t i = 0; i < signals.size(); ++i) {
        const auto& s = signals[i];
        keys.push_back(s.order);
        if (s.order.fixed_pin >= 0) {
            const unsigned pin = unsigned(s.order.fixed_pin) + 1;
            if (pin > d.pins || (!data_pin(pin) && !(s.order.type == 9 && clock_pin(pin))))
                fail("invalid fixed pin: " + s.name);
            r.pins[i]          = int(pin);
            occupied_pins[pin] = true;
            if (io_for_pin(pin) >= 0)
                keys[i].block = std::int16_t(io_for_pin(pin) / int(ios));
        }
        if (s.input_register) {
            if (ios == 16)
                fail("MACH355 input pairs use the ordinary macrocell path");
            const auto& input = signals[std::size_t(s.paired_input)];
            keys[i].fixed_pin = input.order.fixed_pin;
            Mach111PartitionCluster c;
            c.order.signal_id = std::uint16_t(input.order.serial);
            c.output          = true;
            c.product_groups  = 0;
            c.macrocell       = false;
            c.data_terms      = 0;
            c.global_clock    = s.global_clock;
            c.clock_positive  = s.clock_positive;
            c.clock_fanin     = s.clock_fanin;
            if (input.order.fixed_pin >= 0) {
                const auto io = io_for_pin(unsigned(input.order.fixed_pin) + 1);
                if (io < 0)
                    fail("input register requires an IO pin");
                c.fixed_block = io / int(ios);
            }
            for (int constrained : {int(s.order.block), int(input.order.block)})
                if (constrained >= 0) {
                    if (unsigned(constrained) >= d.blocks || (c.fixed_block >= 0 && c.fixed_block != constrained))
                        fail("conflicting IPAIR block");
                    c.fixed_block = constrained;
                }
            clusters.push_back(c);
            cluster_signals.push_back(i);
            continue;
        }
        if (!s.output) {
            if (s.order.type == 9)
                clocks.push_back(i);
            continue;
        }
        if (s.order.product_terms < 0 || s.order.product_terms > 20)
            fail("product count exceeds physical steering capacity: " + s.name);
        outputs.push_back(i);
        cluster_signals.push_back(i);
        // Resource rank uses separate data and XOR capacity tables. A single
        // ordinary term has zero weight; two through five terms have weight one.
        // This differs from rounding the term count up to four-term groups.
        static constexpr unsigned rank_groups[21] = {0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4};
        static constexpr unsigned xor_groups[20]  = {0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4};
        if (s.xor_product && s.order.product_terms > 19)
            fail("z5517: separate XOR product exceeds steering capacity: " + s.name);
        static constexpr unsigned async_groups[19]     = {0, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4};
        static constexpr unsigned async_xor_groups[18] = {0, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4};
        const bool forced_async                        = s.force_async || s.local_clock;
        if (forced_async && s.order.product_terms > (s.xor_product ? 17 : 18))
            fail("asynchronous product count exceeds physical steering capacity");
        const unsigned terms    = unsigned(s.order.product_terms);
        const unsigned resource = forced_async    ? (s.xor_product ? async_xor_groups[terms] : async_groups[terms])
                                  : s.xor_product ? xor_groups[terms]
                                                  : rank_groups[terms];
        Mach111PartitionCluster cluster;
        cluster.order.signal_id = std::uint16_t(s.order.serial);
        cluster.fanins          = s.fanins;
        cluster.reset           = s.reset;
        cluster.output          = !s.buried;
        cluster.product_groups  = resource;
        cluster.oe_product      = s.oe_product;
        cluster.preset          = s.preset;
        clusters.push_back(cluster);
        auto& c = clusters.back();
        if (terms <= (s.xor_product ? 17u : 18u))
            c.asynchronous_product_groups = s.xor_product ? async_xor_groups[terms] : async_groups[terms];
        c.data_terms                  = std::max(1u, unsigned(s.order.product_terms));
        c.global_reset                = s.global_reset;
        c.global_preset               = s.global_preset;
        c.global_clock                = s.global_clock;
        c.clock_positive              = s.clock_positive;
        c.clock_fanin                 = s.clock_fanin;
        c.local_clock                 = s.local_clock;
        c.allow_global_product_clocks = options.allow_global_product_clocks;
        c.force_async                 = s.force_async;
        c.force_sync                  = s.force_sync;
        c.clocked_storage             = s.clocked_storage;
        // Mark compatible partial groups before seeding fixed clusters.
        if (terms == 2 || terms == 6 || (terms == 7 && s.xor_product))
            c.sync_product_flags = 0x100;
        if (!s.xor_product && terms == 1)
            c.async_product_flags = 1;
        if (terms == 4 || terms == 9 || terms == 14)
            c.async_product_flags = 0x101;
        if (terms == 5 || terms == 10 || terms == 15)
            c.async_product_flags = s.xor_product ? 0x1000 : 0x101;
        if (r.pins[i] >= 0) {
            const int io = io_for_pin(unsigned(r.pins[i]));
            if (io < 0 || s.buried)
                fail("invalid output pin: " + s.name);
            c.fixed_block = io / int(ios);
        }
        if (s.fixed_cell >= 0) {
            if ((!s.buried && !s.paired_output) || unsigned(s.fixed_cell) >= d.blocks * d.cells ||
                reserved_cells[unsigned(s.fixed_cell)])
                fail("invalid fixed node: " + s.name);
            reserved_cells[unsigned(s.fixed_cell)] = true;
            c.fixed_block                          = s.fixed_cell / int(d.cells);
        }
        if (s.order.block >= 0) {
            if (unsigned(s.order.block) >= d.blocks || (c.fixed_block >= 0 && c.fixed_block != s.order.block))
                fail("conflicting block constraint: " + s.name);
            c.fixed_block = s.order.block;
        }
    }
    if (outputs.empty())
        fail("no output macrocells");
    std::vector<unsigned> capacity(d.blocks, ios);
    std::set<unsigned> output_pins, counted;
    for (auto i : outputs)
        if (r.pins[i] >= 0)
            output_pins.insert(unsigned(r.pins[i]));
    for (std::size_t i = 0; i < signals.size(); ++i)
        if (!signals[i].output && r.pins[i] >= 0) {
            const unsigned pin = unsigned(r.pins[i]);
            const int io       = io_for_pin(pin);
            if (io >= 0 && !paired_inputs.count(i) && !output_pins.count(pin) && counted.insert(pin).second)
                --capacity[unsigned(io) / ios];
        }
    std::vector<std::size_t> cluster_order;
    std::vector<bool> product_clocks;
    std::vector<bool> converted_clocks(signals.size());
    std::vector<unsigned> implicit_controls;
    r.implicit_controls.assign(signals.size(), 0);
    PartitionObserver observe_partition;
    if (options.partition_observer)
        observe_partition = [&](const PartitionObservation& frame) {
            options.partition_observer(signals, cluster_signals, frame);
        };
    const auto blocks = partition_mach_family_first_success(
        d, clusters, capacity, options.balanced_partition, &r.global_controls, &cluster_order, &product_clocks,
        &implicit_controls, options.iteration, nullptr, &r.partition_product_groups, observe_partition);
    for (unsigned b = 0; b < d.blocks; ++b)
        for (auto c : blocks[b]) {
            const auto i           = cluster_signals[c];
            converted_clocks[i]    = product_clocks[c];
            r.implicit_controls[i] = implicit_controls[c];
            if (signals[i].input_register) {
                input_blocks[i] = input_blocks[std::size_t(signals[i].paired_input)] = int(b);
                keys[i].block = keys[std::size_t(signals[i].paired_input)].block = std::int16_t(b);
            } else
                r.cells[i] = int(b * d.cells);
        }
    // Modern devices assign automatic clocks in ascending device order.
    // MACH1 instead preassigns a sole clock to its default slot.
    if ((options.reserve_default_clock || !clocks.empty()) && d.clock_pins.empty())
        fail("dedicated clock geometry is not implemented for " + d.device);
    if (options.reserve_default_clock)
        occupied_pins[d.clock_pins.front()] = true;
    std::sort(clocks.begin(), clocks.end(), [&](auto a, auto b) { return keys[a].serial < keys[b].serial; });
    for (auto i : clocks)
        if (r.pins[i] < 0 && signals[i].assigned_clock_pin) {
            const auto pin = signals[i].assigned_clock_pin;
            if (!clock_pin(pin) || occupied_pins[pin])
                fail("invalid normalized clock pin");
            r.pins[i]          = int(pin);
            occupied_pins[pin] = true;
        }
    for (auto i : clocks) {
        if (r.pins[i] < 0)
            for (auto pin : d.clock_pins)
                if (!occupied_pins[pin]) {
                    r.pins[i]          = int(pin);
                    occupied_pins[pin] = true;
                    break;
                }
        if (r.pins[i] < 0)
            fail("no free dedicated clock");
        keys[i].fixed_pin = std::int16_t(r.pins[i] - 1);
    }
    for (std::size_t i = 0; i < signals.size(); ++i)
        if (keys[i].type != 9) {
            std::set<unsigned> destinations;
            for (auto out : outputs)
                if (std::find(signals[out].fanins.begin(), signals[out].fanins.end(), std::uint16_t(keys[i].serial)) !=
                        signals[out].fanins.end() ||
                    (converted_clocks[out] && signals[out].clock_fanin == std::uint16_t(keys[i].serial)))
                    destinations.insert(unsigned(r.cells[out]) / d.cells);
            keys[i].fanout_count = std::int16_t(destinations.size());
        }
    // Import logical records in partition-cluster order, with each paired pad
    // immediately before its input register. The mixed type-1/type-8 comparator
    // is not antisymmetric, so initial array order matters even with unique IDs.
    std::vector<std::size_t> imported;
    std::vector<bool> emitted(signals.size());
    auto emit = [&](std::size_t i) {
        if (!emitted[i]) {
            imported.push_back(i);
            emitted[i] = true;
        }
    };
    for (auto c : cluster_order) {
        const auto i = cluster_signals[c];
        if (signals[i].input_register)
            emit(std::size_t(signals[i].paired_input));
        emit(i);
    }
    for (std::size_t i = 0; i < signals.size(); ++i)
        emit(i);
    std::vector<FitterOrderKey> imported_keys;
    for (auto i : imported)
        imported_keys.push_back(keys[i]);
    for (auto i : fitter_signal_order(imported_keys, false))
        r.global_order.push_back(imported[i]);
    for (auto i : r.global_order)
        if (signals[i].output)
            r.output_order[unsigned(r.cells[i]) / d.cells].push_back(i);
    std::vector<ModernProducts> products(d.blocks);
    std::vector<unsigned> output_block(signals.size(), d.blocks);
    std::vector<std::size_t> global_outputs;
    for (unsigned b = 0; b < d.blocks; ++b)
        for (auto i : r.output_order[b])
            output_block[i] = b;
    for (auto i : r.global_order)
        if (signals[i].output)
            global_outputs.push_back(i);
    auto assign_outputs = [&](unsigned b) {
        // Scan nodes greedily, then augment each unmatched output. Fixed outputs
        // can displace an earlier floating choice; fixed pins and input pads
        // cannot be displaced.
        std::vector<std::size_t> io_outputs;
        for (unsigned local = 0; local < d.cells; ++local)
            for (auto i : r.output_order[b])
                if (r.cells[i] == int(b * d.cells + local) && !signals[i].buried)
                    io_outputs.push_back(i);
        std::vector<int> owner(ios, -1), selected(io_outputs.size(), -1);
        std::vector<bool> fixed(ios);
        for (unsigned io = 0; io < ios; ++io)
            fixed[io] = occupied_pins[d.io_pins[b * ios + io]];
        for (auto i : io_outputs)
            if (r.pins[i] >= 0)
                fixed[unsigned(io_for_pin(unsigned(r.pins[i]))) % ios] = false;
        auto pin_candidates = [&](unsigned index) {
            std::vector<unsigned> result;
            const unsigned local = unsigned(r.cells[io_outputs[index]]) % d.cells;
            for (unsigned k = 0; k < (ios == 16 ? 8u : 4u); ++k)
                result.push_back(ios == 16 ? (local / 8) * 8 + k : (local / 2 + 5 + k) % 8);
            return result;
        };
        for (unsigned index = 0; index < io_outputs.size(); ++index) {
            const auto i = io_outputs[index];
            if (r.pins[i] >= 0) {
                const unsigned io = unsigned(io_for_pin(unsigned(r.pins[i]))) % ios;
                if (fixed[io])
                    fail("conflicting fixed output pin");
                if (owner[io] >= 0)
                    selected[unsigned(owner[io])] = -1;
                owner[io]       = int(index);
                selected[index] = int(io);
                fixed[io]       = true;
            } else
                for (unsigned io : pin_candidates(index))
                    if (!fixed[io] && owner[io] < 0) {
                        owner[io]       = int(index);
                        selected[index] = int(io);
                        break;
                    }
        }
        for (unsigned start = 0; start < io_outputs.size(); ++start)
            if (selected[start] < 0) {
                const auto old = selected;
                std::vector<int> predecessor(ios, -1);
                std::vector<unsigned> cursor(io_outputs.size());
                unsigned index = start;
                bool matched   = false;
                for (;;) {
                    check_fitting_budget();
                    const auto candidates = pin_candidates(index);
                    if (cursor[index] < candidates.size()) {
                        const unsigned io = candidates[cursor[index]++];
                        if (fixed[io])
                            continue;
                        if (owner[io] < 0) {
                            predecessor[io] = int(index);
                            int pin         = int(io);
                            do {
                                const int next           = predecessor[unsigned(pin)];
                                owner[unsigned(pin)]     = next;
                                selected[unsigned(next)] = pin;
                                pin                      = old[unsigned(next)];
                            } while (pin >= 0);
                            matched = true;
                            break;
                        }
                        if (predecessor[io] < 0) {
                            predecessor[io] = int(index);
                            index           = unsigned(owner[io]);
                            cursor[index]   = 0;
                        }
                    } else {
                        const int previous = old[index];
                        if (previous < 0)
                            break;
                        index = unsigned(predecessor[unsigned(previous)]);
                    }
                }
                if (!matched) {
                    return false;
                }
            }
        for (unsigned index = 0; index < io_outputs.size(); ++index) {
            const auto i         = io_outputs[index];
            const unsigned local = unsigned(r.cells[i]) % d.cells;
            const unsigned io = b * ios + unsigned(selected[index]), pin = d.io_pins[io];
            occupied_pins[pin]  = true;
            r.pins[i]           = int(pin);
            const unsigned slot = ios == 16 ? (local + 8 - io % ios) % 8 : (local + 16 - 2 * (io % ios)) % 16;
            if (slot >= 8 || r.output_muxes[io] >= 0)
                fail("conflicting output multiplexer");
            r.output_muxes[io] = int(slot);
        }
        return true;
    };

    const auto asynchronous = [&](std::size_t i) {
        const auto& s = signals[i];
        const auto b  = output_block[i];
        auto matches  = [&](const std::optional<std::uint16_t>& control, unsigned slot) {
            if (!control)
                return true;
            return *control == r.global_controls[b][slot].value_or(0);
        };
        const auto reset  = (r.implicit_controls[i] & 1) ? std::optional<std::uint16_t>(0) : s.reset;
        const auto preset = (r.implicit_controls[i] & 2) ? std::optional<std::uint16_t>(0) : s.preset;
        const bool global = (matches(reset, 0) && matches(preset, 1)) || (matches(reset, 1) && matches(preset, 0));
        const bool result = s.local_clock || s.force_async || converted_clocks[i] || !global;
        return result;
    };
    std::vector<OutputSearchCallbacks> operations(signals.size());
    for (auto i : global_outputs) {
        operations[i].candidates = [&, i]() {
            const auto& s    = signals[i];
            const auto b     = output_block[i];
            const bool async = asynchronous(i);
            std::vector<unsigned> candidates;
            for (unsigned n = 0; n < d.cells; ++n) {
                const unsigned local = options.spread_placement ? (n % 4) * 4 + n / 4 : n;
                const unsigned cell  = b * d.cells + local;
                if (placed[cell])
                    continue;
                if (s.fixed_cell >= 0) {
                    if (cell != unsigned(s.fixed_cell))
                        continue;
                } else if (reserved_cells[cell])
                    continue;
                if (s.order.fixed_pin >= 0) {
                    const unsigned io = unsigned(io_for_pin(unsigned(r.pins[i]))) % ios;
                    if (ios == 16 ? local / 8 != io / 8 : (local + 16 - 2 * io) % 16 >= 8)
                        continue;
                }
                if (products[b].allocate(int(local), s.order.product_terms, s.xor_product, async)) {
                    candidates.push_back(cell);
                    products[b].remove(int(local));
                }
            }
            return candidates;
        };
        operations[i].commit = [&, i](unsigned cell) {
            const auto& s    = signals[i];
            const auto b     = output_block[i];
            const bool async = asynchronous(i);
            if (!products[b].allocate(int(cell % d.cells), s.order.product_terms, s.xor_product, async))
                return false;
            r.cells[i]                  = int(cell);
            placed[cell]                = true;
            r.asynchronous_cells[cell]  = async;
            r.product_clock_cells[cell] = converted_clocks[i];
            return true;
        };
        operations[i].remove = [&, i]() {
            const auto cell = unsigned(r.cells[i]), b = output_block[i];
            products[b].remove(int(cell % d.cells));
            placed[cell]                = false;
            r.asynchronous_cells[cell]  = false;
            r.product_clock_cells[cell] = false;
            r.cells[i]                  = -1;
        };
    }
    const auto complete = [&](const std::vector<bool>& can_move) -> PlacementRouteFeedback {
        for (unsigned b = 0; b < d.blocks; ++b)
            if (!assign_outputs(b)) {
                for (auto at = r.output_order[b].rbegin(); at != r.output_order[b].rend(); ++at)
                    if (can_move[*at])
                        return {false, int(*at)};
                return {false, int(r.output_order[b].back())};
            }
        for (unsigned b = 0; b < d.blocks; ++b)
            for (unsigned local = 0; local < d.cells; ++local) {
                const auto& group                          = products[b].groups[local];
                r.product_group_owner[b * d.cells + local] = group.main < 0 ? -1 : int(b * d.cells) + group.main;
                r.product_aux_owner[b * d.cells + local]   = group.aux < 0 ? -1 : int(b * d.cells) + group.aux;
                r.product_aux_data[b * d.cells + local]    = group.data;
            }
        // Input-register pads retain ascending physical candidate lists separately
        // from macrocell candidates. A source conflict between fixed feedback cells
        // must advance the paired pad.
        std::vector<std::size_t> pair_order;
        std::vector<int> pair_register(signals.size(), -1), assigned = r.pins;
        std::vector<std::vector<unsigned>> pair_lists(signals.size());
        std::vector<unsigned> pair_cursor(signals.size());
        auto pair_occupied = occupied_pins;
        for (std::size_t reg = 0; reg < signals.size(); ++reg)
            if (signals[reg].input_register)
                pair_register[std::size_t(signals[reg].paired_input)] = int(reg);
        for (auto paired : r.global_order)
            if (paired_inputs.count(paired) && r.pins[paired] < 0)
                pair_order.push_back(paired);
        const auto fill_pairs = [&](std::size_t start) {
            for (std::size_t position = start; position < pair_order.size(); ++position) {
                const auto paired = pair_order[position];
                const auto reg    = unsigned(pair_register[paired]);
                auto& list        = pair_lists[paired];
                list.clear();
                pair_cursor[paired] = 0;
                for (unsigned pin = 1; pin <= d.pins; ++pin) {
                    const int io = io_for_pin(pin);
                    if (io >= 0 && io / int(ios) == input_blocks[reg] && !pair_occupied[pin])
                        list.push_back(pin);
                }
                if (list.empty())
                    return false;
                assigned[paired]                          = int(list[pair_cursor[paired]++]);
                pair_occupied[unsigned(assigned[paired])] = true;
            }
            return true;
        };
        if (!fill_pairs(0))
            throw PhysicalPlacementFailure("input-register pin placement retry");
        for (;;) {
            check_fitting_budget();
            auto trial          = r;
            auto trial_occupied = pair_occupied;
            auto movable        = can_move;
            for (auto paired : r.global_order)
                if (paired_inputs.count(paired)) {
                    trial.pins[paired]         = assigned[paired];
                    const auto reg             = unsigned(pair_register[paired]);
                    trial.input_registers[reg] = io_for_pin(unsigned(assigned[paired]));
                    if (trial.input_registers[reg] < 0)
                        fail("input-register pad is not on an I/O pin");
                    movable[paired] = movable[reg] = pair_cursor[paired] < pair_lists[paired].size();
                }
            for (auto i : trial.global_order)
                if (!signals[i].output && !signals[i].input_register && trial.pins[i] < 0) {
                    for (unsigned pin = 1; pin <= d.pins; ++pin)
                        if (!trial_occupied[pin] && data_pin(pin)) {
                            const int io = io_for_pin(pin);
                            if (signals[i].order.block >= 0 && (io < 0 || io / int(ios) != signals[i].order.block))
                                continue;
                            if (signals[i].order.type == 9 && !clock_pin(pin))
                                continue;
                            trial_occupied[pin] = true;
                            trial.pins[i]       = int(pin);
                            break;
                        }
                    if (trial.pins[i] < 0)
                        fail("input placement retry: " + signals[i].name);
                }
            trial.output_muxes = complete_modern_output_muxes(d, trial.output_muxes);

            PlacementRouteFeedback result;
            // Select a live feedback owner at the congested raw/registered input pair
            // using retained candidate availability.
            for (std::size_t reg = 0; reg < signals.size(); ++reg)
                if (signals[reg].input_register && signals[reg].feedback_live) {
                    const auto paired = std::size_t(signals[reg].paired_input);
                    if (!signals[paired].raw_input_live || trial.pins[paired] < 0)
                        continue;
                    const int io = io_for_pin(unsigned(trial.pins[paired]));
                    if (io < 0)
                        continue;
                    const unsigned b = unsigned(io) / ios, even = b * d.cells + 2 * (unsigned(io) % ios);
                    int owners[2] = {-1, -1};
                    for (auto i : r.output_order[b])
                        if (signals[i].feedback_live)
                            for (unsigned side = 0; side < 2; ++side)
                                if (r.cells[i] == int(even + side))
                                    owners[side] = int(i);
                    if (owners[0] >= 0 && owners[1] >= 0) {
                        int selected = -1;
                        if (signals[unsigned(owners[0])].fixed_cell >= 0 &&
                            signals[unsigned(owners[1])].fixed_cell >= 0 && movable[paired])
                            selected = int(paired);
                        else {
                            // Walk input-mux groups backwards. Congested groups can borrow free
                            // feedback capacity from an earlier group; advance that boundary owner
                            // rather than necessarily an owner of the congested pair.
                            int available = 0, feedback = 0;
                            bool congested = false;
                            for (unsigned group = ios; group-- > 0;) {
                                unsigned pad_members = 0, live_feedback = 0;
                                int candidate = -1;
                                for (std::size_t node = 0; node < signals.size(); ++node)
                                    if (signals[node].input_register) {
                                        const auto pad = std::size_t(signals[node].paired_input);
                                        if (trial.pins[pad] != int(d.io_pins[b * ios + group]))
                                            continue;
                                        pad_members += unsigned(signals[node].feedback_live) +
                                                       unsigned(signals[pad].raw_input_live);
                                    }
                                for (unsigned side = 0; side < 2; ++side)
                                    for (auto id : r.output_order[b])
                                        if (r.cells[id] == int(b * d.cells + 2 * group + side)) {
                                            live_feedback += unsigned(signals[id].feedback_live);
                                            if (candidate < 0 || (compare_fitter_signals(
                                                                      keys[id], keys[unsigned(candidate)], false) > 0 &&
                                                                  movable[id]))
                                                candidate = int(id);
                                        }
                                congested |= live_feedback + pad_members > 3;
                                available += 3 - int(std::max(1u, pad_members));
                                feedback += int(live_feedback);
                                if (congested && available >= feedback && candidate >= 0) {
                                    selected = candidate;
                                    break;
                                }
                            }
                        }
                        if (selected >= 0 && signals[unsigned(selected)].fixed_cell < 0 &&
                            !movable[unsigned(selected)]) {
                            // An exhausted input-mux owner can release capacity by moving a
                            // nonfeedback owner before the first congested group. Scan forward,
                            // unlike the reverse boundary-owner traversal.
                            int alternate = -1, alternate_group = -1, total_live = 0;
                            bool before_congestion = true;
                            const auto limit =
                                std::find(global_outputs.begin(), global_outputs.end(), std::size_t(selected));
                            for (unsigned group = 0; group < ios; ++group) {
                                unsigned live = 0;
                                for (unsigned side = 0; side < 2; ++side)
                                    for (auto id : r.output_order[b])
                                        if (r.cells[id] == int(b * d.cells + 2 * group + side)) {
                                            live += unsigned(signals[id].feedback_live);
                                            if (before_congestion && signals[id].fixed_cell < 0 &&
                                                (!signals[id].buried || !signals[id].feedback_live) &&
                                                (alternate < 0 ||
                                                 compare_fitter_signals(keys[id], keys[unsigned(alternate)], false) >
                                                     0)) {
                                                alternate       = int(id);
                                                alternate_group = int(group);
                                            }
                                        }
                                for (std::size_t node = 0; node < signals.size(); ++node)
                                    if (signals[node].input_register) {
                                        const auto pad = std::size_t(signals[node].paired_input);
                                        if (trial.pins[pad] == int(d.io_pins[b * ios + group]))
                                            live += unsigned(signals[node].feedback_live) +
                                                    unsigned(signals[pad].raw_input_live);
                                    }
                                total_live += int(live);
                                if (live > 3)
                                    before_congestion = false;
                                int exhausted = -1;
                                for (auto at = limit; at != global_outputs.begin();) {
                                    const auto id = *--at;
                                    if (output_block[id] != b)
                                        continue;
                                    if (alternate < 0) {
                                        if (exhausted >= 0 &&
                                            compare_fitter_signals(keys[id], keys[unsigned(exhausted)], false) < 0)
                                            alternate = int(id);
                                    } else if (!movable[unsigned(alternate)]) {
                                        exhausted       = alternate;
                                        alternate       = -1;
                                        alternate_group = -1;
                                    }
                                    if (alternate >= 0 && alternate_group >= 0)
                                        break;
                                }
                            }
                            if (total_live <= int(3 * ios) && alternate >= 0)
                                selected = alternate;
                        }
                        result = {false, selected, true};
                        // When an explicit input-mux retry target has no retained candidate,
                        // scan the complete logical list without selecting other fallback owners.
                        // Record the walk before capturing the final consistent failed placement.
                        if (selected >= 0 && signals[unsigned(selected)].fixed_cell < 0 && !movable[unsigned(selected)])
                            record_fitting_complete_logical_walk();
                        break;
                    }
                }
            if (!result.success && failure_callback)
                failure_callback(trial, movable);
            if (result.success && route_callback)
                result = route_callback(trial, movable);
            if (result.success) {
                r = std::move(trial);
                return {};
            }
            int paired = result.retry_signal;
            if (paired >= 0 && signals[unsigned(paired)].input_register)
                paired = signals[unsigned(paired)].paired_input;
            const auto target =
                paired < 0 ? pair_order.end() : std::find(pair_order.begin(), pair_order.end(), std::size_t(paired));
            if (target == pair_order.end() || pair_cursor[unsigned(paired)] >= pair_lists[unsigned(paired)].size())
                return result;
            for (auto it = target; it != pair_order.end(); ++it)
                pair_occupied[unsigned(assigned[*it])] = false;
            if (!r.reverse_visit_costs.empty())
                record_fitting_logical_revisit(r.reverse_visit_costs.at(unsigned(paired)));
            assigned[unsigned(paired)] = int(pair_lists[unsigned(paired)][pair_cursor[unsigned(paired)]++]);
            pair_occupied[unsigned(assigned[unsigned(paired)])] = true;
            if (!fill_pairs(std::size_t(target - pair_order.begin()) + 1))
                throw PhysicalPlacementFailure("input-register suffix pin placement retry");
        }
    };
    const auto finish = [&](const std::vector<bool>& can_move) {
        const auto pins = r.pins, registers = r.input_registers, muxes = r.output_muxes;
        const auto occupied = occupied_pins;
        const auto result   = complete(can_move);
        if (!result.success) {
            r.pins            = pins;
            r.input_registers = registers;
            r.output_muxes    = muxes;
            occupied_pins     = occupied;
        }
        return result;
    };
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
    if (!search_output_candidates(r.output_order, global_outputs, operations, finish, r.reverse_visit_costs))
        throw PhysicalPlacementFailure("MACH3/4 placement: node/input-source placement retry exhausted");
    return r;
}
}  // namespace machxl
