#include "fitter_order.hpp"
#include <algorithm>
#include <numeric>
#include <stdexcept>

using namespace machxl;
#define CHECK(condition) do { \
    if (!(condition)) throw std::runtime_error("check failed: " #condition); \
} while (false)

int main() {
    // Partition keys use unsigned comparisons, unlike signed signal keys.
    std::vector<PartitionOrderKey> clusters{
        {0,0,0}, {0xffff,0xffff,0}, {0xffff,0,0xffff},
        {0xffff,0,0}, {0x8000,0,0}, {0x7fff,0,0}};
    CHECK((partition_signal_order(clusters) == std::vector<std::size_t>{3,2,1,4,5,0}));
    CHECK((partition_signal_order(std::vector<PartitionOrderKey>(3)) ==
           std::vector<std::size_t>{1,0,2}));
    FitterOrderKey a,b;
    a.serial=1; b.serial=2;
    CHECK(compare_fitter_signals(a,b,false) == 1);
    a.fanout_count=3; b.fanout_count=1;
    CHECK(compare_fitter_signals(a,b,false) == -2);
    a.fanout_count=b.fanout_count=1;
    a.product_terms=5; b.product_terms=3;
    CHECK(compare_fitter_signals(a,b,false) == -2);
    b.fixed_node=4;
    CHECK(compare_fitter_signals(a,b,false) == 1);
    a.fixed_pin=3;
    a.has_group=true;
    CHECK(compare_fitter_signals(a,b,true) == -1);
    CHECK(compare_fitter_signals(a,b,false) == 1);
    a={}; b={};
    a.type=1; b.type=0;
    CHECK(compare_fitter_signals(a,b,false) == 1);
    b.type=1; b.block=0;
    CHECK(compare_fitter_signals(a,b,false) == 1);
    a.block=0; a.fixed_pin=3;
    CHECK(compare_fitter_signals(a,b,false) == -1);
    b.fixed_pin=4; b.type=9;
    CHECK(compare_fitter_signals(a,b,false) == 1);
    a.type=9;
    a.serial=32767; b.serial=-32768;
    CHECK(compare_fitter_signals(a,b,false) == 1);
    // Original three-item median shortcut swaps equal keys 0 and 1.
    std::vector<FitterOrderKey> equal(3);
    CHECK((fitter_signal_order(equal,false) == std::vector<std::size_t>{1,0,2}));
    equal.resize(4);
    CHECK((fitter_signal_order(equal,false) == std::vector<std::size_t>{0,1,2,3}));
    CHECK((original_unsigned_order({2,2,2}) == std::vector<std::size_t>{1,0,2}));
    CHECK((original_unsigned_order({2,2,2,2}) == std::vector<std::size_t>{0,1,2,3}));
    CHECK((original_unsigned_order({3,1,2,0}) == std::vector<std::size_t>{3,1,2,0}));
    // The signal keys reproduce the 3COUNT_D ordering in the saved OUT file.
    // Each key is specified explicitly to keep the comparator test focused.
    std::vector<FitterOrderKey> counter(9);
    for (unsigned i=0;i<9;++i) counter[i].serial=static_cast<std::int16_t>(77+i);
    counter[0].type=9; counter[0].fixed_pin=34;
    counter[1].type=counter[2].type=1;
    counter[1].fanout_count=counter[2].fanout_count=2;
    for (unsigned i=3;i<6;++i) {
        counter[i].type=5;
        counter[i].block=i==4 ? 0 : 1;
        counter[i].product_terms=static_cast<std::int16_t>(i==3 ? 4 : i==4 ? 3 : 1);
    }
    for (unsigned i=6;i<9;++i) {
        counter[i]=counter[i-3];
        counter[i].serial=static_cast<std::int16_t>(77+i);
        counter[i].type=3;
        counter[i].fanout_count=i==6 ? 1 : 2;
    }
    CHECK((fitter_signal_order(counter,false) == std::vector<std::size_t>{3,4,5,7,8,6,0,2,1}));
    // All permutations exercise both partition directions and tail-recursion
    // branches; serial tie-breaks must produce descending original IDs.
    std::vector<int> permutation(8);
    std::iota(permutation.begin(),permutation.end(),0);
    do {
        std::vector<FitterOrderKey> keys(8);
        for (unsigned i=0;i<8;++i) keys[i].serial=static_cast<std::int16_t>(permutation[i]);
        const auto order=fitter_signal_order(keys,false);
        for (unsigned i=0;i<8;++i) CHECK(keys[order[i]].serial == 7-static_cast<int>(i));
    } while (std::next_permutation(permutation.begin(),permutation.end()));
}
