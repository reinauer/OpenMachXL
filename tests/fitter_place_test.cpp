#include "fitter_place.hpp"
#include "fitter_family_place.hpp"
#include "fitter_output_search.hpp"
#include "fitter_iteration.hpp"
#include <stdexcept>
using namespace machxl;
#include "fitter_u1_ir.inc"
#define CHECK(x) do { if(!(x)) throw std::runtime_error("check failed: " #x); } while(false)
Mach111PlaceSignal input(const std::string& name,int serial,int fanout) {
    Mach111PlaceSignal s;s.name=name;s.order.type=1;s.order.serial=serial;
    s.order.fanout_count=fanout;return s;
}
Mach111PlaceSignal output(const std::string& name,int serial,
                          std::vector<std::uint16_t> fanins) {
    Mach111PlaceSignal s;s.name=name;s.order.serial=serial;s.output=true;
    s.order.product_terms=1;s.fanins=std::move(fanins);return s;
}
#include "fitter_oe_retry.inc"
int main() {
    {
        // All blocks get their initial cell attempt before the pad stage.
        // MP111 fails its later block: the earlier colliding cell stays put.
        FittingIterationCounters counters(2);
        begin_fitting_logical_walk(2);
        std::vector<OutputSearchCallbacks> operations(2);
        operations[0]={[]{return std::vector<unsigned>{0};},[](unsigned){return true;},[]{}};
        operations[1]={[]{return std::vector<unsigned>{};},[](unsigned){return false;},[]{}};
        unsigned pad_checks=0;
        CHECK(!search_output_candidates({{0},{1}},{0,1},operations,
            [](const auto&){return PlacementRouteFeedback{};},{},
            [&](unsigned){++pad_checks;return false;}));
        CHECK(pad_checks==0 && fitting_logical_visits()==4);
        // Mirroring the control makes the first block fail. The later
        // block is still attempted, and is present in the failure frame.
        std::swap(operations[0],operations[1]);
        std::vector<bool> last;
        CHECK(!search_output_candidates({{0},{1}},{0,1},operations,
            [](const auto&){return PlacementRouteFeedback{};},{},{},
            [&](unsigned block,const auto& assigned,int signal){
                CHECK(block==0 && signal==0);last=assigned;
            }));
        CHECK((last==std::vector<bool>{false,true}));
    }
    {
        // Controlled DEV experiment duplicates the same constraint table:
        // the physical search is unchanged, but its observation is1:2.
        auto layout=mach_device_layout("MACH120");
        layout.oe_constraint_count=2;
        layout.oe_constraint_indices.assign(layout.blocks,1);
        auto q=output("Q",78,{77});q.oe_product="A";
        const auto placed=place_mach_family(layout,{input("A",77,1),q});
        CHECK((placed.last_oe_constraint==std::array<int,2>{1,2}));
        q.oe_product.clear();
        CHECK(!place_mach_family(layout,{input("A",77,1),q}).last_oe_constraint);
    }
    {
        // An exhausted explicit IMX owner is not a request to advance
        // an earlier owner which happens to have candidates remaining.
        FittingIterationCounters counters(1);
        begin_fitting_logical_walk(5);
        std::vector<OutputSearchCallbacks> operations(2);
        unsigned commits=0;
        operations[0]={[]{return std::vector<unsigned>{0,1};},
            [&](unsigned){++commits;return true;},[]{}};
        operations[1]={[]{return std::vector<unsigned>{0};},
            [&](unsigned){++commits;return true;},[]{}};
        CHECK(!search_output_candidates({{0,1}},{0,1},operations,
            [&](const auto& remaining){
                CHECK(remaining[0]&&!remaining[1]);
                record_fitting_complete_logical_walk();
                return PlacementRouteFeedback{false,1,true};
            }));
        CHECK(commits==2 && fitting_logical_visits()==10);
    }
    {
        FittingIterationCounters counters(1);
        counters.failed_placements[0]=400000;
        std::vector<OutputSearchCallbacks> operations(2);
        unsigned rejected=0,removed=0;
        operations[0]={[] {return std::vector<unsigned>{0,1};},
            [](unsigned){return true;},[&] {++removed;}};
        operations[1]={[] {return std::vector<unsigned>{0,1,2};},
            [&](unsigned){++rejected;return false;},[] {}};
        bool repartition=false;
        try {
            search_output_candidates({{0,1}},{0,1},operations,
                [](const auto&) {return PlacementRouteFeedback{};});
        } catch(const RepartitionRequested&) {repartition=true;}
        CHECK(repartition && rejected==3 && removed==0);
        CHECK(counters.failed_placements[0]==400001);
    }
    {
        // Cell allocation finishes before the fixed-input pad collision
        // is discovered. Keep the candidate cursor while retrying that
        // block, and count the actual reverse logical walks.
        FittingIterationCounters counters(2);
        begin_fitting_logical_walk(3);
        std::vector<OutputSearchCallbacks> operations(3);
        std::array<unsigned,3> chosen{},created{};
        for(unsigned i=0;i<3;++i)operations[i]={
            [&,i] {++created[i];return std::vector<unsigned>{0,1};},
            [&,i](unsigned cell){chosen[i]=cell;return true;},[] {}};
        unsigned complete=0;
        CHECK(search_output_candidates({{0,1},{2}},{0,2,1},operations,
            [](const auto&) {return PlacementRouteFeedback{};}, {3,1,2},
            [&](unsigned block) {if(block)return true;++complete;return chosen[0]==1;}));
        CHECK(complete==3);
        CHECK((created==std::array<unsigned,3>{1,2,1}));
        CHECK(fitting_logical_visits()==7);
        CHECK(counters.failed_placements[0]==0);
    }
    {
        std::vector<OutputSearchCallbacks> operations(2);
        bool live=false;
        operations[0]={[] {return std::vector<unsigned>{0};},
            [&](unsigned){live=true;return true;},[&]{live=false;}};
        operations[1]={[] {return std::vector<unsigned>{};},
            [](unsigned){return false;},[] {}};
        std::vector<std::vector<bool>> observations;
        CHECK(!search_output_candidates({{0,1}},{0,1},operations,
            [](const auto&){return PlacementRouteFeedback{};},{},{},
            [&](unsigned block,const auto& assigned,int){
                CHECK(block==0 && assigned[0]==live);
                observations.push_back(assigned);
            }));
        CHECK((observations==std::vector<std::vector<bool>>{{true,false},{false,false}}));
    }
    {
        std::vector<OutputSearchCallbacks> operations(4);
        std::array<unsigned,4> commits{},removals{};
        for(unsigned i=0;i<4;++i) {
            operations[i].candidates=[] {return std::vector<unsigned>{0,1};};
            operations[i].commit=[&,i](unsigned){++commits[i];return true;};
            operations[i].remove=[&,i] {++removals[i];};
        }
        unsigned attempts=0;
        CHECK(search_output_candidates({{0,2},{1,3}},{0,1,2,3},operations,
            [&](const auto& remaining) {
                if(++attempts==1){CHECK(remaining[0]);return PlacementRouteFeedback{false,0};}
                return PlacementRouteFeedback{};
            }));
        CHECK((commits==std::array<unsigned,4>{2,1,2,1}));
        CHECK((removals==std::array<unsigned,4>{1,0,1,0}));
    }

    check_split_enable_retry();
    // The three-output placement matches the saved PLC and OUT results.
    std::vector<Mach111PlaceSignal> three={input("A",77,2),input("B",78,2),input("C",79,1),
        output("Q",80,{77,78}),output("R",81,{77,79}),output("S",82,{78,79})};
    const auto p=place_mach111(three);
    CHECK((p.pins==std::vector<int>{4,3,6,43,2,5}));
    CHECK((p.cells==std::vector<int>{-1,-1,-1,16,0,3}));
    CHECK((p.global_order==std::vector<std::size_t>{4,5,3,1,0,2}));
    // Allow four partition attempts after physical-fit failures. With
    // iteration disabled, allow only one; never retry a source error.
    for(bool enabled:{false,true}) {
        Mach111PlaceOptions options;options.iterate_partition=enabled;
        unsigned attempts=0;
        bool exhausted=false;
        try {
            place_mach111(three,options,[&](const auto&,const auto&) -> PlacementRouteFeedback {
                ++attempts;throw PhysicalPlacementFailure("injected completed physical failure");
            });
        } catch(const PhysicalPlacementFailure&) {exhausted=true;}
        CHECK(exhausted && attempts==(enabled?4u:1u));
    }
    {
        unsigned attempts=0;
        bool exhausted=false;
        try {
            place_mach111(three,{},[&](const auto&,const auto&) -> PlacementRouteFeedback {
                ++attempts;throw RepartitionRequested{};
            });
        } catch(const PhysicalPlacementFailure& failure) {
            exhausted=std::string(failure.what()).find("z7502")!=std::string::npos;
        }
        CHECK(exhausted && attempts==4);
    }
    {
        unsigned attempts=0;
        bool invalid=false;
        try {
            place_mach111(three,{},[&](const auto&,const auto&) -> PlacementRouteFeedback {
                ++attempts;throw std::invalid_argument("invalid source constraint");
            });
        } catch(const std::invalid_argument&) {invalid=true;}
        CHECK(invalid && attempts==1);
    }
    //A routing failure selects an earlier owner, discards only its
    //suffix, and resumes the retained candidate cursor across blocks.
    unsigned route_attempts=0;
    const auto continued=place_mach111(three,{},
        [&](const auto& candidate,const auto& remaining) {
            ++route_attempts;
            if(route_attempts==1) {
                CHECK(candidate.cells==p.cells && remaining[4]);
                return PlacementRouteFeedback{false,4};
            }
            return PlacementRouteFeedback{};
        });
    CHECK(route_attempts==2);
    CHECK((continued.cells==std::vector<int>{-1,-1,-1,16,3,0}));
    route_attempts=0;
    const auto family_continued=place_mach_family(mach_device_layout("MACH110"),three,{},
        [&](const auto&,const auto& remaining) {
            if(++route_attempts==1) {CHECK(remaining[4]);return PlacementRouteFeedback{false,4};}
            return PlacementRouteFeedback{};
        });
    CHECK(route_attempts==2 && family_continued.cells==continued.cells);
    std::vector<Mach111PlaceSignal> paired={input("A",109,0),input("B",3,1),
        output("Q",1,{3,110}),input("N",110,1)};
    paired[1].order.fixed_pin=3;paired[2].order.fixed_pin=1;
    paired[3].input_register=paired[3].buried=true;
    paired[3].order.type=8;paired[3].paired_input=0;
    route_attempts=0;int first_pair=-1;
    const auto pair_retried=place_mach_family(mach_device_layout("MACH211"),paired,{},
        [&](const auto& candidate,const auto& remaining) {
            if(++route_attempts==1) {
                first_pair=candidate.cells[3];CHECK(remaining[3]&&remaining[0]);
                return PlacementRouteFeedback{false,3};
            }
            return PlacementRouteFeedback{};
        });
    CHECK(route_attempts==2 && pair_retried.cells[3]!=first_pair);
    CHECK(pair_retried.pins[2]==2 && pair_retried.pins[1]==4);
    // Import the logic cluster first, followed by the pair's pad and
    // input register together, then the rank-zero ordinary inputs.
    CHECK((pair_retried.import_order==std::vector<std::size_t>{2,0,3,1}));
    paired[0].order.fixed_pin=5;
    CHECK((place_mach_family(mach_device_layout("MACH211"),paired).import_order==
        std::vector<std::size_t>{2,0,3,1}));


    // Original DECODE's full output and automatic physical input map.
    std::vector<Mach111PlaceSignal> decoder;
    for(unsigned i=0;i<8;++i) decoder.push_back(input("DATA["+std::to_string(i)+"]",77+i,1));
    for(unsigned i=0;i<3;++i) decoder.push_back(input("SEL["+std::to_string(i+1)+"]",85+i,8));
    for(unsigned i=0;i<8;++i)
        decoder.push_back(output("OUT["+std::to_string(i)+"]",88+i,
            {static_cast<std::uint16_t>(77+i),85,86,87}));
    const auto d=place_mach111(decoder);
    CHECK((d.pins==std::vector<int>{17,16,14,13,11,10,9,7,6,4,3,43,40,2,37,30,5,8,15}));
    // Comparator hierarchy independently exercises the name tie-break
    // and each earlier key; alphabetic order must not override these.
    std::vector<PhysicalOutputOrderKey> k={
        {"A",false,false,0,1,0,false},{"Z",false,false,0,1,0,false},
        {"Z",false,false,0,1,1,false},{"Z",false,false,0,1,0,true},
        {"Z",false,false,0,2,0,false},{"Z",false,false,1,1,0,false},
        {"Z",false,false,2,1,0,false},{"Z",false,true,0,1,0,false},
        {"Z",true,false,0,1,0,false}};
    CHECK((physical_output_order(k)==std::vector<std::size_t>{8,7,6,5,4,3,2,0,1}));
    // Check mixed, fixed, and NODE placement against saved physical maps.
    auto mixed=three;
    mixed[0].order.fixed_pin=9;mixed[1].order.fixed_pin=31;mixed[2].order.fixed_pin=34;
    mixed[0].order.serial=9;mixed[1].order.serial=31;mixed[2].order.serial=34;
    mixed[3].order.fixed_pin=1;mixed[3].order.serial=1;
    mixed[4].order.serial=77;mixed[5].order.serial=78;
    mixed[3].fanins={9,31};mixed[4].fanins={9,34};mixed[5].fanins={31,34};
    CHECK((place_mach111(mixed).pins==std::vector<int>{10,32,35,2,5,43}));
    std::vector<Mach111PlaceSignal> nodes={input("A",9,1),input("B",31,2),
        output("Q",1,{31,77}),output("N",77,{9,31})};
    nodes[0].order.fixed_pin=9;nodes[1].order.fixed_pin=31;
    nodes[2].order.fixed_pin=1;nodes[2].order.product_terms=2;
    nodes[3].buried=true;nodes[3].order.type=3;nodes[3].order.fanout_count=1;
    CHECK((place_mach111(nodes).cells==std::vector<int>{-1,-1,0,16}));
    CHECK((place_mach111(nodes).pins==std::vector<int>{10,32,2,-1}));
    // NODE18 is B0 and may coexist with a physical input on pin43.
    nodes[0].order.fixed_pin=42;nodes[0].order.serial=42;nodes[3].fanins={42,31};
    CHECK((place_mach111(nodes).cells==std::vector<int>{-1,-1,0,16}));
    nodes[0].order.fixed_pin=9;nodes[0].order.serial=9;nodes[3].fanins={9,31};
    nodes[3].order.fixed_node=61;nodes[3].fixed_cell=16;
    CHECK((place_mach111(nodes).cells==std::vector<int>{-1,-1,0,16}));
    nodes[3].order.fixed_node=-1;nodes[3].fixed_cell=-1;
    auto m=nodes[3];m.name="M";m.order.serial=78;m.order.product_terms=2;
    nodes.push_back(m);nodes[2].fanins={77,78};
    CHECK((place_mach111(nodes).cells==std::vector<int>{-1,-1,0,19,16}));
    std::vector<Mach111PlaceSignal> clocks={input("A",9,1),input("K",77,0),output("Q",1,{9})};
    clocks[0].order.fixed_pin=9;clocks[1].order.type=9;clocks[2].order.fixed_pin=1;
    CHECK(place_mach111(clocks).pins[1]==35);
    clocks[0].order.fixed_pin=34;
    CHECK(place_mach111(clocks).pins[1]==11);
    clocks[0].order.fixed_pin=9;
    auto clock2=input("C",78,0);clock2.order.type=9;clocks.push_back(clock2);
    const auto clock_map=place_mach111(clocks);
    CHECK(clock_map.pins[1]==11 && clock_map.pins[3]==13);
    // U1's buried candidates perturb a previously placed 5-product
    // output. MAPROM's trial at B13 pushes TCI left, and WE left again;
    // removing MAPROM's trial must retain both displacement side effects.
    const auto u1=place_mach111(u1_placement_ir());
    CHECK(u1.cells[16]==31 && u1.cells[17]==22);
    CHECK(u1.product_group_owner[26]==27);
    CHECK(u1.product_group_owner[27]==28);
    CHECK(u1.product_group_owner[28]==28);
    CHECK(u1.product_group_owner[29]==-1);
    // Three independent enables in one constrained block require the
    // third output to skip the first half's third candidate (A6).
    std::vector<Mach111PlaceSignal> oe_inputs={input("A",9,1),input("B",31,1),
        input("E",10,1),input("F",12,1),input("G",32,1)};
    for(auto& s:oe_inputs) s.order.fixed_pin=s.order.serial;
    for(unsigned n=0;n<3;++n) {
        auto s=output(std::string(1,static_cast<char>('P'+n)),77+n,
            {static_cast<std::uint16_t>(n%2==0 ? 9 : 31),
             static_cast<std::uint16_t>(n==0 ? 10 : n==1 ? 12 : 32)});
        s.order.block=0;s.oe_members=1;s.oe_product=std::string(1,static_cast<char>('E'+n));
        oe_inputs.push_back(s);
    }
    const auto oe3=place_mach111(oe_inputs);
    CHECK(oe3.cells[5]==0 && oe3.cells[6]==3 && oe3.cells[7]==9);
    CHECK(oe3.oe_slots[5]==0 && oe3.oe_slots[6]==1 && oe3.oe_slots[7]==2);
    // Original OE3A also programs the unused fourth enable row with G.
    // This is retained partition state, not a fourth output assignment.
    CHECK((oe3.oe_products[0]==std::array<std::string,4>{"E","F","G","G"}));
}
