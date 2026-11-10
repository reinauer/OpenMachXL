#include "fitter_partition.hpp"
#include "fitter_family_place.hpp"
#include "fitter_iteration.hpp"
#include <algorithm>
#include <numeric>
#include <stdexcept>

using namespace machxl;
#define CHECK(condition) do { \
    if (!(condition)) throw std::runtime_error("check failed: " #condition); \
} while (false)

using Clusters=std::vector<Mach111PartitionCluster>;
#include "fitter_u14_partition_ir.inc"
using Blocks=std::array<std::vector<std::size_t>,2>;
void membership(const Clusters& clusters, const Blocks& expected) {
    auto ranked=clusters;
    rank_mach111_single_group(ranked);
    for (std::size_t i=0;i<clusters.size();++i) {
        CHECK(ranked[i].order.rank==clusters[i].order.rank);
        CHECK(ranked[i].order.fanout_sum==clusters[i].order.fanout_sum);
        CHECK(ranked[i].order.signal_id==clusters[i].order.signal_id);
    }
    auto actual=partition_mach111_single_group(clusters,true);
    for (auto& block:actual) std::sort(block.begin(),block.end());
    CHECK(actual==expected);
}
int main() {
    // Success advances the next rank before the main loop decrements
    // the four-attempt budget. The last two physical attempts restrict
    // subsequent advancement to phase zero.
    PartitionIterationState iteration;
    for(unsigned phase:{0u,1u,2u,0u}) {
        CHECK(iteration.phase==phase);
        iteration.succeeded(phase);
    }
    CHECK(iteration.completed==4 && iteration.remaining==0);
    PartitionIterationState balanced_iteration;
    bool balanced=true;
    for(unsigned phase:{0u,0u,1u,0u}) {
        CHECK(balanced_iteration.phase==phase);
        balanced_iteration.succeeded(phase,balanced);
        balanced=balanced_iteration.balanced_phase;
    }
    CHECK(!balanced && balanced_iteration.remaining==0);
    // A failed physical cell allocation reserves one cluster on the
    // next partition, independent of the cluster's nominal data charge.
    {
        const auto& layout=mach_device_layout("MACH111");
        Clusters fixed(2);
        for(unsigned i=0;i<2;++i) {
            fixed[i].order.signal_id=std::uint16_t(i);
            fixed[i].fixed_block=int(i);fixed[i].output=true;
            fixed[i].product_groups=i?3:1;fixed[i].data_terms=i?9:1;
        }
        PartitionIterationState retry;
        std::vector<unsigned> used;
        partition_mach_family_first_success(layout,fixed,{16,16},true,
            nullptr,nullptr,nullptr,nullptr,&retry,nullptr,&used);
        CHECK((used==std::vector<unsigned>{1,3}));
        retry.reserve_product_cluster(1);
        retry.reserve_product_cluster(1); // status bit, not an accumulating count
        partition_mach_family_first_success(layout,fixed,{16,16},true,
            nullptr,nullptr,nullptr,nullptr,&retry,nullptr,&used);
        CHECK((used==std::vector<unsigned>{1,4}));
    }
    // Use the asynchronous resource descriptor when the fixed block's
    // global control rows cannot hold the incoming RE. Its one-term data
    // consumes a group despite a zero synchronous rank.
    {
        auto layout=mach_device_layout("MACH435");layout.cells=2;
        Clusters fallback(2);
        for(unsigned i=0;i<2;++i) {
            fallback[i].order.signal_id=std::uint16_t(i);
            fallback[i].fixed_block=0;fallback[i].output=false;
        }
        fallback[0].reset=fallback[0].global_reset=1;
        fallback[0].preset=fallback[0].global_preset=2;
        fallback[1].reset=fallback[1].global_reset=3;
        fallback[1].global_clock=0;
        fallback[1].product_groups=0;
        fallback[1].asynchronous_product_groups=1;
        std::vector<unsigned> used;
        const std::vector<unsigned> capacities(layout.blocks,2);
        CHECK(partition_mach_family_first_success(layout,fallback,capacities,true,
            nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&used)[0].size()==2);
        CHECK(used[0]==2);
        fallback[1].asynchronous_product_groups=2;
        bool failed=false;
        try {partition_mach_family_first_success(layout,fallback,capacities,true);}
        catch(const std::runtime_error&) {failed=true;}
        CHECK(failed);
    }
    // Fixed descriptors can share a partially occupied group. Floating
    // descriptors must pay the complete group count.
    {
        auto layout=mach_device_layout("MACH435");layout.cells=3;
        Clusters sharing(2);
        for(unsigned i=0;i<2;++i) {
            sharing[i].order.signal_id=std::uint16_t(i);
            sharing[i].fixed_block=0;sharing[i].output=false;
            sharing[i].product_groups=2;sharing[i].data_terms=5;
        }
        sharing[0].local_clock=true;sharing[0].async_product_flags=1;
        sharing[1].sync_product_flags=0x100;
        const std::vector<unsigned> capacities(layout.blocks,3);
        CHECK(partition_mach_family_first_success(layout,sharing,capacities,true)[0].size()==2);
        sharing[0].async_product_flags=0;
        bool failed=false;
        try {partition_mach_family_first_success(layout,sharing,capacities,true);}
        catch(const std::runtime_error&) {failed=true;}
        CHECK(failed);
        //Deferred asynchronous flags require both implicit-control
        //retry and a fixed-seed product failure before becoming usable.
        sharing[0].async_product_flags=0x1000;
        sharing[0].clocked_storage=sharing[1].clocked_storage=true;
        std::vector<unsigned> implicit;
        CHECK(partition_mach_family_first_success(layout,sharing,capacities,true,
            nullptr,nullptr,nullptr,&implicit)[0].size()==2);
        CHECK((implicit==std::vector<unsigned>{0,0}));
        //The retry restores absent controls before the successful pass.
        sharing[0].reset=sharing[0].global_reset=7;
        CHECK(partition_mach_family_first_success(layout,sharing,capacities,true,
            nullptr,nullptr,nullptr,&implicit)[0].size()==2);
        CHECK((implicit==std::vector<unsigned>{0,0}));
    }

    Clusters implicit(5);
    for(auto& c:implicit)c.clocked_storage=true;
    implicit[1].reset=7;implicit[1].global_reset=7;
    implicit[2].reset=0;implicit[2].preset=9;
    implicit[3].clocked_storage=false; //COM may still carry default clock metadata.
    implicit[3].global_clock=0;
    implicit[4].macrocell=false; //IPAIR is an input descriptor.
    CHECK(ground_implicit_partition_controls(implicit)==std::vector<unsigned>({3,2,0,0,0}));
    CHECK(implicit[0].reset==0 && implicit[0].preset==0);
    CHECK(implicit[0].global_reset==0 && implicit[0].global_preset==0);
    CHECK(implicit[1].reset==7 && implicit[1].global_reset==7 && implicit[1].global_preset==0);
    CHECK(!implicit[3].reset && !implicit[4].preset);
    CHECK(ground_implicit_partition_controls(implicit)==std::vector<unsigned>(5));
    // Modern fitting uses five distinct ranking formulas. In phases two
    // and three, GND controls lose their 10000 weight; phase four removes
    // the pad bonus.
    Clusters phase(1);
    phase[0].fanins={1,2,3};phase[0].reset=0;phase[0].preset=7;
    phase[0].product_groups=2;phase[0].data_terms=8;
    const unsigned ranks[5]={22532,22770,13500,13500,21000};
    for(unsigned mode=0;mode<5;++mode) {
        rank_mach_modern_phase(phase,mode);
        CHECK(phase[0].order.rank==ranks[mode]);
        CHECK(phase[0].order.fanout_sum==3);
    }
    rank_mach_modern_phase(phase,1,true);
    CHECK(phase[0].order.rank==23170);
    rank_mach_modern_phase(phase,2,false,true);
    CHECK(phase[0].order.rank==23500); // The policy adds a rank bonus.
    phase[0].reset.reset();phase[0].preset.reset();phase[0].output=false;
    phase[0].fanins.resize(25);std::iota(phase[0].fanins.begin(),phase[0].fanins.end(),0);
    rank_mach_modern_phase(phase,2);
    CHECK(phase[0].order.rank==22600);
    phase[0].fanins.resize(13);
    rank_mach_modern_phase(phase,2);CHECK(phase[0].order.rank==5950);
    rank_mach_modern_phase(phase,3);CHECK(phase[0].order.rank==11800);
    // MACH215 z5128 materializes zero RE/PR before initial ranking,
    // independently of the configurable set/reset don't-care policy.
    // SW002 and AP2150 isolate the rank effect and zero-table swap guard.
    {
        Clusters storage(3);
        storage[0].order.signal_id=111;storage[0].fanins={112};
        storage[1].order.signal_id=112;storage[1].fanins={110,113};storage[1].output=false;
        storage[2].order.signal_id=113;storage[2].fanins={110};storage[2].output=false;
        storage[2].clocked_storage=true;
        const auto layout=mach_device_layout("MACH215");
        auto blocks=partition_mach_family_first_success(layout,storage,{8,8,8,8},true);
        for(auto& block:blocks)std::sort(block.begin(),block.end());
        CHECK((blocks==std::vector<std::vector<std::size_t>>{{0,2},{1},{},{}}));
        storage[0].clocked_storage=true;
        blocks=partition_mach_family_first_success(layout,storage,{8,8,8,8},true);
        for(auto& block:blocks)std::sort(block.begin(),block.end());
        CHECK((blocks==std::vector<std::vector<std::size_t>>{{0},{1,2},{},{}}));
        storage[0].implicit_storage_controls=true;
        rank_mach111_single_group(storage);
        CHECK(storage[0].order.rank==22511);
        CHECK(!storage[0].reset&&!storage[0].preset);
        rank_mach_modern_phase(storage,2);
        CHECK(storage[0].order.rank==2850);
        rank_mach_modern_phase(storage,2,false,true);
        CHECK(storage[0].order.rank==22850);
    }
    // U14 requires move/swap rescue and repeated failed-cluster priority
    // promotion. The original emits eleven intermediate DEB maps.
    const auto u14=partition_mach111_single_group(u14_partition_ir(),true,{8,9});
    auto u14a=u14[0];std::sort(u14a.begin(),u14a.end());
    CHECK((u14a==std::vector<std::size_t>{0,1,3,5,11,13,14,17,20,21,22,24,26,27,28,29}));
    // Saved partition cases supply both inputs and expected membership:
    // EQN2.PLA contains signal keys and DEB1.PLA contains block assignments.
    Clusters decoder;
    for (unsigned i=0;i<8;++i)
        decoder.push_back({{2541,25,static_cast<std::uint16_t>(88+i)},
                           {static_cast<std::uint16_t>(77+i),85,86,87},{}});
    membership(decoder,Blocks{{{2,5,6,7},{0,1,3,4}}});
    // Also catch replacing mutable reverse-index swaps with static pairs.
    CHECK((partition_mach111_single_group(decoder,true)==Blocks{{{5,6,7,2},{0,3,1,4}}}));
    membership({{{12551,12,80},{78,79,83,84,85},79},
                {{12541,11,81},{78,79,84,85},79},
                {{12531,9,82},{78,79,85},79}},Blocks{{{1},{0,2}}});
    membership({{{2511,1,79},{77},{}},{{2511,1,80},{78},{}}},Blocks{{{1},{0}}});
    membership({{{2521,4,80},{77,78},{}},{{2521,4,81},{77,79},{}},
                {{2521,4,82},{78,79},{}}},Blocks{{{1,2},{0}}});
    membership({{{2521,4,82},{77,78},{}},{{2521,4,83},{79,80},{}},
                {{2521,4,84},{77,79},{}},{{2521,4,85},{78,80},{}},
                {{2511,1,86},{81},{}}},Blocks{{{1,2,4},{0,3}}});

    // Reject incompatible reset keys before changing block membership.
    Clusters controls={{{3,0,0},{1},10},{{2,0,1},{2},20},{{1,0,2},{1},10}};
    CHECK((partition_mach111_single_group(controls,true)==Blocks{{{0,2},{1}}}));

    // The reset-key check does not check fanin union. A separate exchange
    // check removes an outgoing cluster, checks the combined fanins, and
    // appends the cluster again after a rejected trial. That changes order.
    Clusters capacity(3);
    for (unsigned i=0;i<3;++i) capacity[i].order.rank=static_cast<std::uint16_t>(3-i);
    capacity[0].fanins.resize(26);capacity[1].fanins.resize(26);capacity[2].fanins.resize(25);
    std::iota(capacity[0].fanins.begin(),capacity[0].fanins.end(),0);
    std::iota(capacity[1].fanins.begin(),capacity[1].fanins.end(),8);
    std::iota(capacity[2].fanins.begin(),capacity[2].fanins.end(),0);
    CHECK((partition_mach111_single_group(capacity,true)==Blocks{{{2,0},{1}}}));

    // The first destination can fit while the second exceeds 26. For
    // pair (2,3), A gains only input35, but B would gain input0 as its
    // 27th input. Both tentative memberships must be rolled back.
    Clusters second_capacity(4);
    for (unsigned i=0;i<4;++i)
        second_capacity[i].order.rank=static_cast<std::uint16_t>(4-i);
    second_capacity[0].fanins.resize(25);
    second_capacity[1].fanins.resize(26);
    std::iota(second_capacity[0].fanins.begin(),second_capacity[0].fanins.end(),0);
    std::iota(second_capacity[1].fanins.begin(),second_capacity[1].fanins.end(),10);
    second_capacity[2].fanins={0};second_capacity[3].fanins={35};
    CHECK((partition_mach111_single_group(second_capacity,true)==Blocks{{{0,2},{1,3}}}));

    // Full device capacity is independent of fanin sharing.
    Clusters full(32);
    for (unsigned i=0;i<32;++i) {
        full[i].order.signal_id=static_cast<std::uint16_t>(i);
        full[i].fanins={77};
    }
    const auto full_blocks=partition_mach111_single_group(full,true);
    CHECK(full_blocks[0].size()==16 && full_blocks[1].size()==16);
    full.push_back({{0,0,32},{77},{}});
    bool full_rejected=false;
    try { partition_mach111_single_group(full,true); }
    catch (const std::runtime_error&) { full_rejected=true; }
    CHECK(full_rejected);
    capacity[0].fanins.push_back(50);
    bool rejected=false;
    try { partition_mach111_single_group(capacity,true); }
    catch (const std::runtime_error&) { rejected=true; }
    CHECK(rejected);
    // Original U7 LEAB_N has no fanins and still consumes one output,
    // one macrocell and one group: EQN2 rank2501, fanout sum0.
    Clusters constant(1);
    constant[0].order.signal_id=19;constant[0].fixed_block=0;
    rank_mach111_single_group(constant);
    CHECK(constant[0].order.rank==2501 && constant[0].order.fanout_sum==0);
    CHECK((partition_mach111_single_group(constant,true)==Blocks{{{0},{}}}));
    rejected=false;
    try { partition_mach111_single_group({{{1,0,0},{1,1},{}}},true); }
    catch (const std::invalid_argument&) { rejected=true; }
    CHECK(rejected);
}
