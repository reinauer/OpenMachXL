#include "fitter_legacy_failure_report.hpp"
#include "frontend.hpp"
#include "fitter_place_report.hpp"
#include <cassert>
#include <memory>

using namespace machxl;

int main() {
    const auto design=parse_design("CHIP FRAME MACH111\nPIN ? A\nPIN ? Q REG\nPIN ? R\nEQUATIONS\nQ:=A\nR=Q\n");
    std::vector<Mach111PlaceSignal> signals(3);
    signals[0].name="A";
    signals[1].name="Q";signals[1].output=true;signals[1].clocked_storage=true;
    signals[1].fanins={1};signals[1].order.product_terms=1;
    signals[2].name="R";signals[2].output=true;signals[2].order.product_terms=1;
    // Separate raw-pad and register-feedback IDs have the same logical spelling.
    signals[2].fanins={2,4};
    Mach111Placement frame;frame.cells={-1,14,11};frame.pins={5,20,17};frame.failed_pad_assignment=true;
    frame.product_group_owner.fill(-1);
    frame.product_group_owner[14]=14;frame.product_group_owner[11]=11;
    std::vector<bool> assigned{false,true,true};unsigned failed_block=1;
    const auto capture=[&] {
        FitterReportData report;
        report.optimized_design=std::make_shared<Design>(design);
        report.optimized_pla=emit_pla(design,"frame.pds");
        capture_legacy_placement_failure(report,signals,frame,failed_block,assigned,
            {{"A",1},{"Q",2},{"R",3}},{{"Q",4}});
        return report;
    };
    auto report=capture();
    const auto& routes=report.blocks[0].routes;
    assert(routes.size()==3);
    assert(routes[0].source_pin==5); // Actual automatic placement, not declaration0.
    assert(routes[1].source_pin==20&&routes[1].source_cell==-1);
    assert(routes[2].source_pin==-1&&routes[2].source_block==0&&routes[2].source_cell==14);
    // A colliding pad is unknown while its already allocated macrocell survives.
    frame.pins[1]=5;
    report=capture();
    assert(report.blocks[0].routes[1].source_pin==-1);
    assert(report.blocks[0].routes[1].source_cell==-1);
    assert(report.blocks[0].routes[2].source_cell==14);
    for(const auto& pin:report.pins)if(pin.name=="Q") {
        assert(pin.number==0&&pin.output_node==16);
    }
    assert(report.placed==report.placement_total&&report.placed==3);
    frame.last_oe_constraint=std::array<int,2>{1,2};
    report=capture();
    assert(report.last_oe_constraint==frame.last_oe_constraint);
    const auto prd=render_fitter_place_report(*report.retained_image,report,
        CompilerSettings{},"frame.pds","start","end","00:00:00");
    assert(prd.find("Warning: {OErc[1:2]}\r\n")!=std::string::npos);
    signals[1].clock_fanin=1;
    frame.pins[0]=13;frame.pins[1]=20;
    report=capture();
    assert(report.blocks[0].clocks.size()==1&&report.blocks[0].clocks[0].pin==13);
    for(const auto& pin:report.pins)if(pin.name=="A")assert(pin.clock_blocks==std::vector<unsigned>{0});
    // Failed IO assignment publishes no floating pads from that block.
    failed_block=0;report=capture();
    for(const auto& pin:report.pins)if(pin.output)assert(pin.number==0);
    // A source-fixed pad remains known before the cell/pad-link phase.
    signals[1].order.fixed_pin=19;assigned[1]=false;
    frame.failed_pad_assignment=false;frame.failed_signal=1;
    report=capture();
    for(const auto& pin:report.pins)if(pin.name=="Q")assert(pin.number==20);
    for(const auto& node:report.nodes)if(node.name=="Q")assert(node.cell==-1&&node.pin==-1);
    assert(report.placed==2&&report.placement_total==3);
    {
        const auto paired=parse_design("CHIP FRAME MACH211\nPIN ? A PAIR N\nPIN 13 CLK\nPIN 2 Q\nNODE ? N REG\nEQUATIONS\nN=A\nN.CLKF=CLK\nQ=N\n");
        std::vector<Mach111PlaceSignal> ir(4);
        ir[0].name="A";ir[1].name="N";ir[1].input_register=true;ir[1].buried=true;ir[1].paired_input=0;
        ir[2].name="CLK";ir[2].order.type=9;
        ir[3].name="Q";ir[3].output=true;ir[3].order.fixed_pin=1;ir[3].order.product_terms=1;ir[3].fanins={101};
        FamilyPlacement pending;pending.cells={-1,32,-1,0};pending.pins={-1,-1,13,2};
        pending.product_group_owner.assign(64,-1);pending.sequential_search.assign(4,false);
        FitterReportData state;state.optimized_design=std::make_shared<Design>(paired);state.optimized_pla=emit_pla(paired,"pair.pds");
        capture_legacy_placement_failure(state,ir,pending,0,{false,false,false,true},
            {{"A",100},{"N",101},{"CLK",12},{"Q",1}},{});
        // Partition block B's placeholder cell32 is not an assigned input register.
        for(const auto& node:state.nodes)if(node.name=="N")assert(node.cell==-1&&node.number==-1&&node.pin==-1);
        for(const auto& route:state.blocks[0].routes)if(route.signal=="N")assert(route.source_block==-1&&route.source_cell==-1);
    }

}
