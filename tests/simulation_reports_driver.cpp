#include "simulator.hpp"
#include "simulation_reports.hpp"
#include "synthesis.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <sstream>
#include <regex>
#include <filesystem>

int main(int argc,char** argv) {
    try {
        if(argc<3)throw std::invalid_argument("expected simulation inputs and HST/TRF");
        const std::string report=argv[--argc];
        if(argc<2||argc>4)throw std::invalid_argument("expected PDS [original PLC [MXL]]");
        std::ifstream input(argv[1]);
        if(!input)throw std::invalid_argument("cannot open fixture");
        machxl::CompilerSettings settings;
        if(argc==4){std::ifstream mxl(argv[3]);settings.parse(std::string(std::istreambuf_iterator<char>(mxl),{}));}
        auto options=machxl::synthesis_options(settings).frontend;options.dos_compatibility=true;
        std::string auxiliary;
        if(settings.yes_no("SIMF_OPT")) {
            auto path=std::filesystem::path(argv[1]);path.replace_extension(".SIM");
            std::ifstream sim(path);if(!sim)throw std::invalid_argument("cannot open auxiliary simulation fixture");
            auxiliary.assign(std::istreambuf_iterator<char>(sim),{});
            path.replace_extension(".TBL");std::ifstream table(path);
            if(table)auxiliary=std::string(std::istreambuf_iterator<char>(table),{})+"\n"+auxiliary;
        }
        auto design=machxl::parse_design(std::string(std::istreambuf_iterator<char>(input),{}),options,settings.yes_no("SIMF_OPT")?&auxiliary:nullptr);
        const std::map<std::string,std::pair<unsigned,std::string>> devices={
#include "frontend_devices.inc"
        };
        auto device=devices.at(design.device);
        machxl::SimulationPinMap map;
        map.signals.resize(device.first+1);map.inverted.resize(device.first+1);
        std::stringstream supplies(device.second);std::string supply;
        while(supplies>>supply){auto colon=supply.find(':');map.signals[std::stoul(supply.substr(colon+1))]=supply.substr(0,colon);}
        auto aliases=machxl::pla_signal_aliases(design);std::map<std::string,unsigned> placement;machxl::SimulationOptions simulation_options;simulation_options.collect_reports=true;simulation_options.legacy_set_reset=settings.yes_no("EQN_22V10");std::map<std::string,std::string> source_names;for(const auto& alias:aliases)source_names[alias.second]=alias.first;
        machxl::SimulationFitModel original_model;
        std::map<unsigned,std::string> input_pairs,node_names;
        if(argc>=3) {
            std::ifstream plc(argv[2]);std::string line;
            while(std::getline(plc,line)) {
                std::stringstream row(line);std::string kind,name;unsigned number;
                if(!(row>>kind>>number>>name))continue;
                if(!name.empty()&&name.front()=='/')name.erase(0,1);
                if(kind=="Node"){node_names[number]=name;if(map.nodes.size()<=number)map.nodes.resize(number+1);map.nodes[number]=source_names.count(name)?source_names.at(name):name;continue;}
                if(kind!="Pin")continue;
                placement[name]=number;std::smatch match;
                if(std::regex_search(line,match,std::regex("IPair ([0-9]+)")))input_pairs[std::stoul(match[1])]=name;
                if((design.device[4]=='3'||design.device[4]=='4')&&source_names.count(name)&&line.find(" Reg ")!=std::string::npos&&
                   std::regex_search(line,match,std::regex("S6=([01])")))
                    simulation_options.register_power_up[source_names.at(name)]=match[1]=="1"?machxl::SimulationValue::Low:machxl::SimulationValue::High;
            }
        }
        // Original PLC explicitly identifies dedicated input registers.
        // Their polarity is the NODE declaration, independently of the
        // primary equation's external phase; they have no async set/reset.
        for(const auto& pair:input_pairs) {
            if(!node_names.count(pair.first)||!source_names.count(node_names.at(pair.first))||!source_names.count(pair.second))
                throw std::invalid_argument("original input-register placement has unknown signals");
            const auto node=source_names.at(node_names.at(pair.first)),input_name=source_names.at(pair.second);
            bool node_negative=false,input_negative=false;
            for(const auto& pin:design.pins) {
                if(pin.name==node)node_negative=pin.inverted;
                if(pin.name==input_name)input_negative=pin.inverted;
            }
            for(const auto& equation:design.equations)if(equation.signal==node&&(equation.function=="REG"||equation.function=="G")) {
                auto& model=original_model.registers[node];model.input_register=true;model.next_state=equation.expression;
                if(node_negative)model.next_state=machxl::make_not(model.next_state);
                bool phase=machxl::evaluate(model.next_state,{{input_name,input_negative}});
                model.power_up=model.reset_value=phase?machxl::SimulationValue::High:machxl::SimulationValue::Low;
                model.preset_value=phase?machxl::SimulationValue::Low:machxl::SimulationValue::High;
                model.reset=model.preset=machxl::make_constant(false);
            }
        }
        simulation_options.fitted_model=&original_model;
        for(const auto& pin:design.pins)if(!pin.node) {
            auto number=argc>=3?placement[aliases.at(pin.name)]:pin.number;
            if(!number&&argc>=3)continue; // Original fitter left this source pin unused.
            if(!number||number>device.first)throw std::invalid_argument("fixture has an unplaced pin");
            map.signals[number]=pin.name;map.inverted[number]=pin.inverted;
        }
        simulation_options.placement=&map;
        auto simulation=machxl::simulate_design(design,simulation_options);
        std::cout<<(report=="HST"?machxl::render_simulation_history(design,simulation,map):machxl::render_simulation_trace(design,simulation,map));
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
