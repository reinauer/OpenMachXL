#include "fitter_modern_place.hpp"
#include <algorithm>
#include <stdexcept>
using namespace machxl;
#define CHECK(x) do {if(!(x))throw std::runtime_error("check failed: " #x);} while(false)
#include "fitter_modern_ir.inc"
#include "fitter_modern_input_order.inc"
#include "fitter_modern_borrowing.inc"
#include "fitter_modern_mixed_borrowing.inc"
#include "fitter_modern_stress_borrowing.inc"
#include "fitter_modern_async_borrowing.inc"
#include "fitter_modern_async_stress.inc"
std::vector<Mach111PlaceSignal> design(unsigned count,bool group=false,
                                     const std::vector<unsigned>& pins={}) {
    std::vector<Mach111PlaceSignal> s(2+count);
    for(unsigned i=0;i<2;++i) {s[i].name=i?"B":"A";s[i].order.type=1;s[i].order.serial=std::int16_t(2+i);s[i].order.fixed_pin=std::int16_t(2+i);}
    for(unsigned i=0;i<count;++i) {
        auto& o=s[2+i];o.name="Q"+std::to_string(i);o.output=true;o.order.product_terms=1;
        o.order.serial=std::int16_t(277+i);o.fanins={std::uint16_t(2+i%2)};
        if(group) o.order.block=0;
        if(!pins.empty()) {o.order.fixed_pin=std::int16_t(pins[i]-1);o.order.serial=o.order.fixed_pin;}
    }
    return s;
}
int main() {
    const auto& d=mach_device_layout("MACH435");
    check_mixed_input_order(d);
    // FP0: both feedback paths at the first floating IPAIR pad belong
    // to fixed live nodes. DOS advances the pad3->4, keeping all nodes.
    {
        std::vector<Mach111PlaceSignal> s(8);
        const char* names[]={"B","C","CLK","A","N","Q","X","Y"};
        const int ids[]={11,12,19,277,278,7,85,86};
        for(unsigned i=0;i<s.size();++i){s[i].name=names[i];s[i].order.serial=std::int16_t(ids[i]);}
        for(unsigned i=0;i<4;++i)s[i].order.type=i==2?9:1;
        for(unsigned i=0;i<3;++i)s[i].order.fixed_pin=std::int16_t(ids[i]);
        s[3].raw_input_live=true;s[3].order.block=0;
        s[4].order.type=8;s[4].input_register=true;s[4].paired_input=3;
        s[4].order.block=0;s[4].global_clock=0;s[4].feedback_live=true;
        s[5].output=true;s[5].order.fixed_pin=7;s[5].order.product_terms=1;
        s[5].fanins={277,278,85,86};
        for(unsigned i=6;i<8;++i) {
            s[i].output=s[i].buried=s[i].feedback_live=true;s[i].order.type=3;
            s[i].fixed_cell=int(i-6);s[i].order.fixed_node=std::int16_t(ids[i]);
            s[i].order.product_terms=1;s[i].fanins={11,12};
        }
        const auto p=place_mach_modern(d,s);
        CHECK(p.pins[3]==4 && p.input_registers[4]==1);
        CHECK(p.cells[5]==12 && p.cells[6]==0 && p.cells[7]==1);
    }
    check_async_stress(d);check_async_borrowing(d);check_stress_borrowing(d);check_mixed_borrowing(d);check_test2(d);check_p7_src(d);check_p8_src(d);
    // RPM814 first places X0 at cell 11; input-source feasibility then
    // advances its existing cursor to cell 15. The saved ordinary and
    // debug outputs have identical complete fuse maps.
    {
        std::vector<Mach111PlaceSignal> raw(20);
        const char* names[]={"B","C","A","CLK","N","Q"};
        const int serials[]={3,4,7,19,218,8};
        for(unsigned i=0;i<6;++i) {
            raw[i].name=names[i];raw[i].order.serial=std::int16_t(serials[i]);
            raw[i].order.type=i==3?9:i==4?8:i==5?0:1;
            if(i!=4)raw[i].order.fixed_pin=std::int16_t(serials[i]);
        }
        raw[4].input_register=true;raw[4].paired_input=2;
        raw[4].global_clock=0;raw[4].feedback_live=true;
        raw[5].output=true;raw[5].order.product_terms=2;
        raw[5].fanins={7,218};
        for(unsigned n=0;n<14;++n) {
            auto& x=raw[6+n];x.name="X"+std::to_string(n);
            x.order.serial=std::int16_t(277+n);x.order.type=3;
            x.order.block=0;x.order.product_terms=1;x.output=x.buried=true;
            x.feedback_live=true;x.fanins={3,4};
            raw[5].fanins.push_back(std::uint16_t(277+n));
        }
        CHECK(place_mach_modern(d,raw).cells[6]==11);
        raw[2].raw_input_live=true;
        const auto retried=place_mach_modern(d,raw);
        CHECK((std::vector<int>(retried.cells.begin()+6,retried.cells.end())==
            std::vector<int>{15,7,3,14,10,6,2,13,9,5,1,12,8,4}));
        CHECK(retried.cells[5]==0 && retried.input_registers[4]==5);
        // RPM815 fills every macrocell. Q is the only nonfeedback
        // signal, but its fixed pin cannot select either raw IPAIR source.
        // Original search retains the final full trial before its chosen
        // IMX owner's exhausted candidate list terminates the attempt.
        auto impossible=raw;
        auto fifteenth=raw.back();fifteenth.name="X14";fifteenth.order.serial=291;
        impossible.push_back(fifteenth);impossible[5].fanins.push_back(291);
        bool exhausted=false;
        try {(void)place_mach_modern(d,impossible);}
        catch(const std::runtime_error& e) {exhausted=std::string(e.what()).find("node/input-source placement retry exhausted")!=std::string::npos;}
        CHECK(exhausted);
        // An occupied but unused feedback source must not trigger movement.
        raw[6].feedback_live=false;
        CHECK(place_mach_modern(d,raw).cells[6]==11);
        // RPM714 puts the conflict earlier in the signal order. The fitter
        // chooses X8, discards only the suffix, and regenerates it;
        // ordinary Cartesian backtracking reaches the attempt limit.
        raw[6].feedback_live=true;
        raw[2].order.fixed_pin=raw[2].order.serial=6;
        raw[4].order.serial=217;raw[5].fanins[0]=6;raw[5].fanins[1]=217;
        const auto earlier=place_mach_modern(d,raw);
        CHECK((std::vector<int>(earlier.cells.begin()+6,earlier.cells.end())==
            std::vector<int>{15,11,7,3,14,10,6,2,13,5,1,12,8,4}));
        // Dense RPC4: the conflicted owner itself needs five products.
        // Releasing it clears both its main and auxiliary references.
        raw[0].order.fixed_pin=raw[0].order.serial=11;
        raw[1].order.fixed_pin=raw[1].order.serial=12;
        raw[2].order.fixed_pin=raw[2].order.serial=7;
        raw[4].order.serial=218;raw[5].fanins[0]=7;raw[5].fanins[1]=218;
        raw[5].order.product_terms=1;
        for(unsigned i=0;i<14;++i) {
            raw[6+i].order.product_terms=5;raw[6+i].fanins={11,12,13,14,15};
        }
        for(unsigned i=0;i<3;++i) {
            Mach111PlaceSignal input;input.name=std::string(1,char('D'+i));
            input.order.type=1;input.order.serial=input.order.fixed_pin=std::int16_t(13+i);
            raw.push_back(input);
        }
        const auto complex=place_mach_modern(d,raw);
        CHECK((std::vector<int>(complex.cells.begin()+6,complex.cells.begin()+20)==
            std::vector<int>{15,7,3,14,10,6,2,13,9,5,1,12,8,4}));
        for(unsigned cell=0;cell<16;++cell) {
            CHECK(complex.product_group_owner[cell]==(cell==0 || cell==11?-1:int(cell)));
            CHECK(complex.product_aux_owner[cell]==(cell==11?-1:int(cell)));
        }
    }
    // AP7: seven asynchronous five-term XOR nodes. A displaced child
    // resumes at the NEXT candidate, leaving N6 on group3 plus its aux.
    {
        std::vector<Mach111PlaceSignal> a(15);
        a[0].name="Q";a[0].output=true;a[0].order.fixed_pin=a[0].order.serial=7;
        a[0].order.product_terms=1;a[0].order.block=0;
        for(unsigned i=0;i<7;++i) {
            a[1+i].name=i==6?"X":"A"+std::to_string(i);
            a[1+i].order.type=1;a[1+i].order.serial=std::int16_t(277+i);
            auto& n=a[8+i];n.name="N"+std::to_string(i);n.output=n.buried=true;
            n.order.type=3;n.order.serial=std::int16_t(284+i);n.order.block=0;
            n.order.product_terms=5;n.xor_product=true;n.local_clock=true;
            n.feedback_live=true;n.clocked_storage=true;
            for(unsigned k=0;k<7;++k)n.fanins.push_back(std::uint16_t(277+k));
            a[0].fanins.push_back(std::uint16_t(284+i));
        }
        const auto p=place_mach_modern(d,a);
        CHECK((std::vector<int>(p.cells.begin()+8,p.cells.end())==
               std::vector<int>{13,9,5,1,12,8,4}));
        CHECK(p.product_group_owner[3]==4 && p.product_aux_owner[3]==4 && p.product_aux_data[3]);
        CHECK(p.product_group_owner[4]==-1 && p.product_aux_owner[4]==4 && !p.product_aux_data[4]);
    }
    // AX1/3/6 exercise reciprocal exchange, failed-child XOR preservation,
    // and the difference between claiming a fresh data auxiliary and
    // retaining an XOR auxiliary.
    {
        struct Case {std::vector<int> terms,cells,main,aux,synchronous;};
        const Case cases[]={
            {{4,9,9,5,5,1,8},{9,8,4,5,1,13,12},
             {1,1,-1,4,4,4,5,8,9,9,8,12,12,13,12,-1},
             {0,1,-1,4,4,5,5,-1,8,9,8,-1,12,13,-1,-1}},
            {{9,8,4,8,5,8},{4,1,9,12,5,8},
             {1,1,1,4,-1,-1,4,5,8,8,8,9,12,12,12,-1},
             {0,1,-1,-1,4,5,4,5,8,9,-1,-1,12,-1,-1,-1}},
            {{9,5,1,2,4,4},{4,8,9,5,1,12},
             {-1,1,1,4,-1,5,4,-1,8,9,8,-1,12,12,-1,-1},
             {0,1,-1,-1,4,5,4,-1,8,9,-1,-1,12,-1,-1,-1}},
            // AY14 requires whole-block validation and rebuild.
            {{5,9,8,5,1,4,9,1},{5,8,12,1,2,9,4,13},
             {1,1,2,4,4,5,5,8,9,9,8,12,-1,12,13,-1},
             {0,1,2,4,4,5,-1,-1,8,9,8,-1,12,13,-1,-1},{4,6,7}}
        };
        for(const auto& c:cases) {
            std::vector<Mach111PlaceSignal> a(8+c.terms.size());
            a[0].name="Q";a[0].output=true;a[0].order.fixed_pin=a[0].order.serial=7;
            a[0].order.product_terms=1;a[0].order.block=0;
            for(unsigned i=0;i<7;++i) {
                auto& input=a[1+i];input.name=i==6?"X":"A"+std::to_string(i);
                input.order.type=1;input.order.serial=input.order.fixed_pin=std::int16_t(11+i);
                input.order.block=1;
            }
            for(unsigned i=0;i<c.terms.size();++i) {
                auto& n=a[8+i];n.name="N"+std::to_string(i);n.output=n.buried=true;
                n.order.type=3;n.order.serial=std::int16_t(277+i);n.order.block=0;
                n.order.product_terms=c.terms[i];n.xor_product=true;
                n.local_clock=std::find(c.synchronous.begin(),c.synchronous.end(),int(i))==c.synchronous.end();
                if(!n.local_clock)n.global_clock=0;
                n.feedback_live=true;n.clocked_storage=true;
                for(unsigned k=0;k<7;++k)n.fanins.push_back(std::uint16_t(11+k));
                a[0].fanins.push_back(std::uint16_t(277+i));
            }
            const auto p=place_mach_modern(d,a);
            CHECK(std::vector<int>(p.cells.begin()+8,p.cells.end())==c.cells);
            CHECK(std::vector<int>(p.product_group_owner.begin(),p.product_group_owner.begin()+16)==c.main);
            CHECK(std::vector<int>(p.product_aux_owner.begin(),p.product_aux_owner.begin()+16)==c.aux);
        }
    }
    // Saved PLC placements for MB1..MB20 preserve the requested PT counts.
    for(int terms=1;terms<=20;++terms) {
        auto input=design(1,false,{8});
        input[2].order.product_terms=std::int16_t(terms);
        const auto placed=place_mach_modern(d,input);
        const int cell=terms<=15?0:12;
        CHECK(placed.cells[2]==cell);
        std::vector<int> expected_main(128,-1),expected_aux(128,-1);
        std::copy(borrowed_main[terms-1],borrowed_main[terms-1]+16,expected_main.begin());
        std::copy(borrowed_aux[terms-1],borrowed_aux[terms-1]+16,expected_aux.begin());
        CHECK(placed.product_group_owner==expected_main);
        CHECK(placed.product_aux_owner==expected_aux);
    }
    // Original MI435DF/DA/LF/LA and MI465DA/LA pairs use IO resources
    // without occupying an output macrocell; latch/register share placement.
    for(bool larger:{false,true})for(bool fixed:{false,true}) {
        if(larger && fixed)continue;
        const auto& layout=mach_device_layout(larger?"MACH465":"MACH435");
        std::vector<Mach111PlaceSignal> pair(4);
        const int first=larger?593:277;
        pair[0].name="A";pair[0].order.serial=std::int16_t(fixed?7:first);pair[0].order.type=1;
        if(fixed)pair[0].order.fixed_pin=7;
        pair[1].name="Q";pair[1].output=true;pair[1].order.serial=std::int16_t(first+!fixed);
        pair[1].order.product_terms=1;pair[1].fanins={std::uint16_t(fixed?218:first+2)};
        pair[2].name="N";pair[2].order.type=8;pair[2].order.serial=std::int16_t(fixed?218:first+2);
        pair[2].input_register=true;pair[2].paired_input=0;
        pair[3].name="CLK";pair[3].order.type=9;pair[3].order.serial=larger?186:19;
        pair[3].order.fixed_pin=pair[3].order.serial;
        const auto placed=place_mach_modern(layout,pair);
        CHECK(placed.cells[1]==0 && placed.cells[2]==-1);
        CHECK(placed.pins[0]==(larger?200:fixed?8:12));
        CHECK(placed.pins[1]==(larger?195:fixed?9:8));
        CHECK(placed.input_registers[2]==(larger?8:fixed?5:15));
    }
    // LC20/23/62/65 exercise clock policies: three distinct resets and
    // explicit GND presets force two outputs asynchronous.
    for(unsigned clock=0;clock<4;++clock)for(bool allow:{false,true}) {
        std::vector<Mach111PlaceSignal> policy(8);
        for(unsigned i=0;i<4;++i) {
            policy[i].name=std::string(1,char('A'+i));policy[i].order.type=1;
            policy[i].order.serial=policy[i].order.fixed_pin=std::int16_t(11+i);
        }
        policy[4].name="CLK";policy[4].order.type=9;
        policy[4].order.serial=policy[4].order.fixed_pin=std::int16_t(d.clock_pins[clock]-1);
        for(unsigned i=0;i<3;++i) {
            auto& out=policy[5+i];out.name=std::string(1,char('Q'+i));out.output=true;
            out.order.serial=out.order.fixed_pin=std::int16_t(4+i);out.order.product_terms=1;
            out.fanins={std::uint16_t(11+i),14};out.reset=out.global_reset=std::uint16_t(i+1);
            out.preset=out.global_preset=0;out.global_clock=int(clock);out.clock_fanin=std::uint16_t(policy[4].order.serial);
        }
        Mach111PlaceOptions options;options.allow_global_product_clocks=allow;
        if(clock>=2 && !allow) {
            bool rejected=false;try{place_mach_modern(d,policy,options);}catch(const std::runtime_error&){rejected=true;}
            CHECK(rejected);continue;
        }
        const auto placed=place_mach_modern(d,policy,options);
        CHECK(placed.cells[5]==4 && placed.cells[6]==12 && placed.cells[7]==8);
        CHECK(!placed.asynchronous_cells[4] && placed.asynchronous_cells[12] && placed.asynchronous_cells[8]);
        CHECK(!placed.product_clock_cells[4]);
        CHECK(placed.product_clock_cells[12]==(clock>=2));
        CHECK(placed.product_clock_cells[8]==(clock>=2));
    }
    // OM0: greedy OMX assignment leaves Q7 unmatched. The original
    // augmenting path moves seven earlier floating pad assignments.
    {
        auto om=design(8);
        om[0].order.serial=om[0].order.fixed_pin=11;
        om[1].order.serial=om[1].order.fixed_pin=12;
        const int nodes[]={0,1,2,3,4,6,14,15};
        for(unsigned i=0;i<8;++i) {
            om[2+i].paired_output=true;om[2+i].fixed_cell=nodes[i];
            om[2+i].order.fixed_node=std::int16_t(85+nodes[i]);
            om[2+i].fanins={11,12};
        }
        const auto placed=place_mach_modern(d,om);
        CHECK((std::vector<int>(placed.pins.begin()+2,placed.pins.end())==
               std::vector<int>{9,10,3,4,5,6,8,7}));
        // ON0-v2: all eight pins cannot serve the first node set. The
        // floating paired node retains its cursor and advances4->8.
        const int fixed_nodes[]={0,1,2,3,5,6,7};
        for(unsigned i=0;i<7;++i) {
            om[2+i].fixed_cell=fixed_nodes[i];
            om[2+i].order.fixed_node=std::int16_t(85+fixed_nodes[i]);
        }
        om[9].fixed_cell=-1;om[9].order.fixed_node=-1;om[9].order.block=0;
        const auto moved=place_mach_modern(d,om);
        CHECK(moved.cells[9]==8);
        CHECK((std::vector<int>(moved.pins.begin()+2,moved.pins.end())==
               std::vector<int>{8,9,10,3,4,5,6,7}));
    }
    auto one=place_mach_modern(d,design(1));
    CHECK((one.pins==std::vector<int>{3,4,8}));CHECK(one.cells[2]==0);CHECK(one.output_muxes[5]==6);
    CHECK((std::vector<int>(one.output_muxes.begin(),one.output_muxes.begin()+8)==std::vector<int>{1,0,0,0,0,6,0,0}));
    CHECK(one.output_muxes[8]==7);
    auto two=place_mach_modern(d,design(2));
    CHECK((two.pins==std::vector<int>{3,4,14,8}));CHECK((two.cells==std::vector<int>{-1,-1,16,0}));
    unsigned route_attempts=0;
    const auto continued=place_mach_modern(d,design(2,true),{},
        [&](const ModernPlacement& attempt,const std::vector<bool>& movable) {
            ++route_attempts;
            if(route_attempts==1) {
                CHECK(attempt.cells[3]==0 && movable[3]);
                return PlacementRouteFeedback{false,3};
            }
            return PlacementRouteFeedback{};
        });
    CHECK(route_attempts==2 && continued.cells[3]==4 && continued.cells[2]==0);
    auto nine=place_mach_modern(d,design(9));
    CHECK((nine.pins==std::vector<int>{3,4,52,77,29,71,14,56,8,35,50}));
    CHECK((nine.cells==std::vector<int>{-1,-1,68,112,32,96,16,80,0,48,64}));
    auto dense=place_mach_modern(d,design(6,true));
    CHECK((dense.pins==std::vector<int>{3,4,5,9,7,6,10,8}));
    CHECK((dense.cells==std::vector<int>{-1,-1,5,1,12,8,4,0}));
    CHECK((std::vector<int>(dense.output_muxes.begin(),dense.output_muxes.begin()+8)==std::vector<int>{3,0,1,2,4,6,5,6}));
    CHECK((place_mach_modern(d,design(2,false,{8,9})).cells==std::vector<int>{-1,-1,12,0}));
    CHECK((place_mach_modern(d,design(3,false,{7,8,9})).cells==std::vector<int>{-1,-1,8,12,0}));
    CHECK((place_mach_modern(d,design(4,false,{6,7,8,9})).cells==std::vector<int>{-1,-1,9,8,12,0}));
    CHECK((place_mach_modern(d,design(6,false,{5,6,7,8,9,10})).cells==std::vector<int>{-1,-1,4,9,8,1,12,0}));
    auto clocked=design(1);Mach111PlaceSignal k;k.name="K";k.order.type=9;k.order.serial=278;
    clocked.push_back(k);CHECK(place_mach_modern(d,clocked).pins.back()==20);
    k.name="L";k.order.serial=279;clocked.push_back(k);
    auto ck=place_mach_modern(d,clocked);CHECK(ck.pins[3]==20 && ck.pins[4]==23);
    auto controls=design(2);controls[2].reset=3;controls[2].fanins={2,3};
    CHECK((place_mach_modern(d,controls).cells==std::vector<int>{-1,-1,16,0}));
    controls[2].preset=controls[2].reset;controls[2].reset.reset();
    CHECK((place_mach_modern(d,controls).cells==std::vector<int>{-1,-1,16,0}));
    controls[2].order.block=controls[3].order.block=0;controls[3].preset=2;controls[3].fanins={2,3};
    CHECK((place_mach_modern(d,controls).cells==std::vector<int>{-1,-1,4,0}));
    controls[2].reset=3;controls[3].reset=2;controls[2].preset.reset();controls[3].preset.reset();
    CHECK((place_mach_modern(d,controls).cells==std::vector<int>{-1,-1,4,0}));
    // Explicit GND consumes a global resource despite a false fuse row.
    // Both presence flags are set for every non-absent equation ID.
    auto grounded=design(2,true);
    grounded[2].reset=grounded[2].preset=0;
    grounded[2].global_reset=grounded[2].global_preset=0;
    grounded[3].reset=grounded[3].global_reset=1;
    const auto ground_rows=place_mach_modern(d,grounded);
    CHECK(ground_rows.global_controls[0][0]==0 && ground_rows.global_controls[0][1]==0);
    CHECK(!ground_rows.asynchronous_cells[unsigned(ground_rows.cells[2])]);
    CHECK(ground_rows.asynchronous_cells[unsigned(ground_rows.cells[3])]);
    auto constant=design(2);constant[2].fanins.clear();constant[2].order.product_terms=0;constant[3].fanins={2};
    CHECK((place_mach_modern(d,constant).cells==std::vector<int>{-1,-1,0,16}));
    auto buried=design(2,true,{8,9});
    buried[2].name="Q";buried[2].fanins={3,277};buried[2].order.product_terms=2;
    buried[3].name="N";buried[3].buried=true;buried[3].order.type=3;buried[3].order.serial=277;
    buried[3].order.fixed_pin=-1;buried[3].fanins={2,3};
    auto b=place_mach_modern(d,buried);CHECK((b.cells==std::vector<int>{-1,-1,0,4}));CHECK(b.pins[3]==-1);
    buried[3].fixed_cell=0;buried[3].order.fixed_node=85;buried[3].order.serial=85;
    buried[2].fanins={3,85};buried[2].order.product_terms=1;
    CHECK((place_mach_modern(d,buried).cells==std::vector<int>{-1,-1,12,0}));
    for(const std::string device:{"MACH355","MACH445","MACH446","MACH465"}) {
        const auto& layout=mach_device_layout(device);
        auto family=design(9);
        for(unsigned i=0;i<2;++i) {family[i].order.fixed_pin=std::int16_t(layout.io_pins[i]-1);family[i].order.serial=family[i].order.fixed_pin;}
        for(unsigned i=0;i<9;++i) family[2+i].fanins={std::uint16_t(layout.io_pins[i%2]-1)};
        const auto f=place_mach_modern(layout,family);
        if(device=="MACH355") {
            CHECK((f.pins==std::vector<int>{129,130,123,20,57,132,88,131,89,52,124}));
            CHECK((f.cells==std::vector<int>{-1,-1,84,16,48,4,68,0,64,32,80}));
        } else if(device=="MACH465") {
            CHECK((f.pins==std::vector<int>{190,191,37,91,15,66,5,56,205,47,195}));
            CHECK((f.cells==std::vector<int>{-1,-1,64,128,48,112,32,96,16,80,0}));
        } else {
            CHECK((f.pins==std::vector<int>{93,94,50,83,24,74,7,57,98,33,48}));
            CHECK(f.cells==nine.cells);
        }
        family.resize(8);for(auto& signal:family)if(signal.output)signal.order.block=0;
        const auto fd=place_mach_modern(layout,family);CHECK(fd.cells==dense.cells);
        CHECK(fd.pins==(device=="MACH355"?std::vector<int>{129,130,134,132,140,139,133,131}:
                       device=="MACH465"?std::vector<int>{190,191,192,196,194,193,197,195}:
                       std::vector<int>{93,94,95,99,97,96,100,98}));
    }
}
