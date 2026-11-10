#include "fitter_family.hpp"
#include "fitter_family_place.hpp"
#include <algorithm>
#include <map>
#include <fstream>
#include <iostream>
#include <regex>
#include <stdexcept>
using namespace machxl;
static void check(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
static std::string read(const std::string& path){std::ifstream file(path,std::ios::binary);check(bool(file),"missing "+path);return {std::istreambuf_iterator<char>(file),{}};}
static std::vector<std::uint8_t> bits(const std::string& text){
    std::smatch m;check(std::regex_search(text,m,std::regex("QF([0-9]+)")),"missing QF");
    std::vector<std::uint8_t> data(std::stoul(m[1]));
    std::regex rows("L([0-9]+)[ \\r\\n]+([01 \\r\\n]+)\\*");
    for(auto it=std::sregex_iterator(text.begin(),text.end(),rows);it!=std::sregex_iterator();++it){auto at=std::stoul((*it)[1]);for(char bit:(*it)[2].str())if(bit=='0'||bit=='1')data.at(at++)=bit-'0';}
    if(std::regex_search(text,m,std::regex("[\\r\\n]E([01\\r\\n ]+)\\*")))for(char bit:m[1].str())if(bit=='0'||bit=='1')data.push_back(bit-'0');
    return data;
}
int main(int argc,char**argv){
    check(argc==2,"expected reference directory");
    for(const auto device:{110,120,121,130,131})for(unsigned variant=0;variant<10;++variant){
        if(device==110&&variant==3)continue;
        const auto name="F"+std::to_string(device)+std::to_string(variant),base=std::string(argv[1])+"/"+name;
        const auto design=parse_design(read(base+".PDS"));
        const auto image=fit_mach_family_fixed(design);
        check(bits(serialize_jedec(image))==bits(read(base+".JED")),name+" differs from original main/electrical fuses");
        std::cout<<name<<" exact\n";
    }
    for(const auto device:{210,211,215,220,221,231})for(unsigned variant:{0,4,5,6,7,8,9}){
        if(variant==8&&device!=215)continue;
        const auto name="G"+std::to_string(device)+std::to_string(variant),base=std::string(argv[1])+"/"+name;
        const auto image=fit_mach_family_fixed(parse_design(read(base+".PDS")));
        check(bits(serialize_jedec(image))==bits(read(base+".JED")),name+" differs from original main/electrical fuses");
        std::cout<<name<<" exact\n";
    }
    for(const auto device:{210,211,215,220,221,231}){
        const auto name="A"+std::to_string(device),base=std::string(argv[1])+"/"+name;
        const auto image=fit_mach_family_fixed(parse_design(read(base+".PDS")));
        check(bits(serialize_jedec(image))==bits(read(base+".JED")),name+" differs from original automatic fuses");
        const auto plc=read(base+".PLC");
        const std::regex assignment("Pin[ \t]+([0-9]+)[ \t]+([^ \t\r\n]+)");
        for(auto it=std::sregex_iterator(plc.begin(),plc.end(),assignment);it!=std::sregex_iterator();++it)
            check(image.pin_signals.at(std::stoul((*it)[1]))==(*it)[2],name+" differs from original pin assignments");
        std::cout<<name<<" exact automatic placement\n";
    }
    for(const auto device:{110,120,130,210,211,215,220,231})for(unsigned variant=0;variant<5;++variant){
        const auto name="K"+std::to_string(device)+std::to_string(variant),base=std::string(argv[1])+"/"+name;
        const auto d=parse_design(read(base+".PDS"));
        if(variant>=3){
            const auto corrected=fit_mach_family_fixed(d);
            check(!corrected.fuses.empty(),name+" did not fit corrected constant design");
            auto compatibility=d;compatibility.dos_compatibility=true;
            bool rejected=false;try{fit_mach_family_fixed(compatibility);}catch(const std::exception&){rejected=true;}
            check(rejected,name+" accepted the original malformed single-constant PLA");continue;}
        const auto image=fit_mach_family_fixed(d);
        check(bits(serialize_jedec(image))==bits(read(base+".JED")),name+" differs from original default clock/constant control fuses");
        std::cout<<name<<" exact default controls\n";
    }
    for(const auto device:{110,120,131,210,211,215,220,231})for(unsigned variant=0;variant<6;++variant){
        const auto name="N"+std::to_string(device)+std::to_string(variant),base=std::string(argv[1])+"/"+name;
        const auto d=parse_design(read(base+".PDS"));
        if(device==215&&variant<2){bool rejected=false;try{fit_mach_family_fixed(d);}catch(const std::exception&){rejected=true;}
            check(rejected,name+" accepted an odd MACH215 logic node rejected by DOS");continue;}
        const auto image=fit_mach_family_fixed(d);
        check(bits(serialize_jedec(image))==bits(read(base+".JED")),name+" differs from original buried-node fuses");
        std::cout<<name<<" exact buried node\n";
    }
    for(const auto device:{110,120,131,210,211,215,220,231})for(unsigned variant=0;variant<3;++variant){
        const auto name="B"+std::to_string(device)+std::to_string(variant),base=std::string(argv[1])+"/"+name;
        const auto image=fit_mach_family_fixed(parse_design(read(base+".PDS")));
        check(bits(serialize_jedec(image))==bits(read(base+".JED")),name+" differs from original borrowed product groups");
        std::cout<<name<<" exact product borrowing\n";
    }
    for(const auto device:{120,211,215})for(unsigned variant=0;variant<3;++variant){
        const auto name="O"+std::to_string(device)+std::to_string(variant),base=std::string(argv[1])+"/"+name;
        const auto image=fit_mach_family_fixed(parse_design(read(base+".PDS")),{}, {variant==1,variant==0});
        check(bits(serialize_jedec(image))==bits(read(base+".JED")),name+" differs from original placement options");
        std::cout<<name<<" exact placement options\n";
    }
    for(const auto device:{210,211,215,220,231})for(unsigned variant=0;variant<5;++variant) {
        const auto name="I"+std::to_string(device)+std::to_string(variant),base=std::string(argv[1])+"/"+name;
        const auto image=fit_mach_family_fixed(parse_design(read(base+".PDS")));
        check(bits(serialize_jedec(image))==bits(read(base+".JED")),name+" differs from original dedicated input register");
        const auto& storage=image.simulation_model.registers.at("N");
        check(storage.power_up==(variant==1?SimulationValue::High:SimulationValue::Low),name+" wrong input register startup phase");
        std::cout<<name<<" exact dedicated input register\n";
    }
    for(const auto device:{211,215})for(unsigned variant=0;variant<14;++variant) {
        const auto name="IC"+std::to_string(device)+std::to_string(variant),base=std::string(argv[1])+"/"+name;
        const auto design=parse_design(read(base+".PDS"));
        if(variant==6||variant==12||(device==211&&(variant==0||variant==3))) {
            bool rejected=false;try{fit_mach_family_fixed(design);}catch(const std::exception&){rejected=true;}
            check(rejected,name+" accepted an input register rejected by DOS");continue;
        }
        const auto image=fit_mach_family_fixed(design);
        check(bits(serialize_jedec(image))==bits(read(base+".JED")),name+" differs from original input register controls");
        std::cout<<name<<" exact input register controls\n";
    }
    for(const auto device:{120,131,211,215,231})for(unsigned variant=0;variant<4;++variant) {
        const auto name="V"+std::to_string(device)+std::to_string(variant),base=std::string(argv[1])+"/"+name;
        const auto image=fit_mach_family_fixed(parse_design(read(base+".PDS")));
        check(bits(serialize_jedec(image))==bits(read(base+".JED")),name+" differs from original multi-control placement");
        check(image.simulation_model.registers.size()==4,name+" missing fitted register metadata");
        for(const auto& entry:image.simulation_model.registers) {
            const auto& model=entry.second;
            check(model.power_up.has_value()&&model.reset_value.has_value()&&model.preset_value.has_value(),name+" incomplete fitted values");
            check(bool(model.next_state)&&bool(model.clock)&&bool(model.reset)&&bool(model.preset),name+" incomplete fitted equations");
        }
        std::cout<<name<<" exact multiple reset/preset controls\n";
    }
    for(const auto device:{210,211,215,220,221,231})for(unsigned variant=0;variant<4;++variant) {
        const auto name="L"+std::to_string(device)+std::to_string(variant),base=std::string(argv[1])+"/"+name;
        const auto d=parse_design(read(base+".PDS"));
        if(device!=215&&(variant==1||variant==3)) {
            bool rejected=false;try{fit_mach_family_fixed(d);}catch(const std::exception&){rejected=true;}
            check(rejected,name+" accepted a clock polarity rejected by DOS");continue;
        }
        const auto image=fit_mach_family_fixed(d);
        check(bits(serialize_jedec(image))==bits(read(base+".JED")),name+" differs from original latch/clock mode");
        std::cout<<name<<" exact latch/clock mode\n";
    }
    for(unsigned variant=0;variant<23;++variant) {
        const auto name=variant<12?"E215"+std::to_string(variant):variant<17?"P215"+std::to_string(variant-12):"P215X"+std::to_string(variant-17);
        const auto base=std::string(argv[1])+"/"+name;
        const auto image=fit_mach_family_fixed(parse_design(read(base+".PDS")));
        check(bits(serialize_jedec(image))==bits(read(base+".JED")),name+" differs from original MACH215 clock routing");
        std::cout<<name<<" exact direct/product clock\n";
    }
    for(const auto device:{120,131,211,215,231})for(unsigned variant=0;variant<4;++variant) {
        const auto name="H"+std::to_string(device)+std::to_string(variant),base=std::string(argv[1])+"/"+name;
        const auto image=fit_mach_family_fixed(parse_design(read(base+".PDS")));
        check(bits(serialize_jedec(image))==bits(read(base+".JED")),name+" differs from original GROUP handling");
        std::cout<<name<<" exact vector/block/power group\n";
    }
    for(const auto device:{120,211,215})for(unsigned variant=0;variant<2;++variant) {
        const auto name="Z"+std::to_string(device)+std::to_string(variant),base=std::string(argv[1])+"/"+name;
        const auto image=fit_mach_family_fixed(parse_design(read(base+".PDS")));
        check(bits(serialize_jedec(image))==bits(read(base+".JED")),name+" differs with unused floating declaration");
        check(std::find(image.pin_signals.begin(),image.pin_signals.end(),"UNUSED")==image.pin_signals.end(),name+" assigned an unused pin");
        std::cout<<name<<" exact unused declaration\n";
    }
    for(const auto device:{120,131,211,215,231})for(unsigned variant=0;variant<4;++variant) {
        const auto name="J"+std::to_string(device)+std::to_string(variant),base=std::string(argv[1])+"/"+name;
        const auto image=fit_mach_family_fixed(parse_design(read(base+".PDS")));
        check(bits(serialize_jedec(image))==bits(read(base+".JED")),name+" differs from original output pairing");
        if(variant)check(image.simulation_model.registers.count("N")&&image.simulation_model.registers.count("Q"),name+" missing paired storage model");
        std::cout<<name<<" exact output pairing\n";
    }
    for(const auto device:{120,211,215})for(unsigned variant=0;variant<6;++variant) {
        const auto name="JX"+std::to_string(device)+std::to_string(variant),base=std::string(argv[1])+"/"+name;
        const auto image=fit_mach_family_fixed(parse_design(read(base+".PDS")));
        check(bits(serialize_jedec(image))==bits(read(base+".JED")),name+" differs from original pair polarity/constraints");
        std::cout<<name<<" exact pair polarity/constraints\n";
    }
    for(const auto device:{120,211,215})for(unsigned variant=0;variant<4;++variant) {
        const auto name="JY"+std::to_string(device)+std::to_string(variant),base=std::string(argv[1])+"/"+name;
        const auto d=parse_design(read(base+".PDS"));
        if(variant==2||(variant==3&&device!=211)) {
            bool rejected=false;try{fit_mach_family_fixed(d);}catch(const std::exception&){rejected=true;}
            check(rejected,name+" accepted an output pair rejected by DOS");continue;
        }
        const auto image=fit_mach_family_fixed(d);
        check(bits(serialize_jedec(image))==bits(read(base+".JED")),name+" differs from original logical pair phases");
        if(variant<2)check(image.simulation_model.registers.at("N").power_up!=image.simulation_model.registers.at("Q").power_up,name+" lost complementary logical storage phases");
        std::cout<<name<<" exact logical pair phases\n";
    }
    for(const auto device:{120,131})for(unsigned variant:{0,1,3,4}) {
        const auto name="I"+std::to_string(device)+std::to_string(variant),base=std::string(argv[1])+"/"+name;
        const auto image=fit_mach_family_fixed(parse_design(read(base+".PDS")));
        check(bits(serialize_jedec(image))==bits(read(base+".JED")),name+" differs from original ignored MACH1 input pairing");
    }
    for(const auto device:{211,215})for(unsigned variant:{0,1}) {
        const auto name="JT"+std::to_string(device)+std::to_string(variant),base=std::string(argv[1])+"/"+name;
        const auto image=fit_mach_family_fixed(parse_design(read(base+".PDS")));
        check(bits(serialize_jedec(image))==bits(read(base+".JED")),name+" differs from original paired T/latch mode");
    }
    for(const auto device:{120,211,215})for(unsigned variant=0;variant<5;++variant) {
        const auto name="JC"+std::to_string(device)+std::to_string(variant),base=std::string(argv[1])+"/"+name;
        const auto design=parse_design(read(base+".PDS"));
        if(variant==0||variant==1||variant==3) {
            bool rejected=false;try{fit_mach_family_fixed(design);}catch(const std::exception&){rejected=true;}
            check(rejected,name+" accepted conflicting output-pair controls");continue;
        }
        const auto image=fit_mach_family_fixed(design);
        check(bits(serialize_jedec(image))==bits(read(base+".JED")),name+" lost node-only output-pair control");
    }
    for(const auto device:{120,131,211,215})for(unsigned variant=0;variant<4;++variant) {
        const auto name="AO"+std::to_string(device)+std::to_string(variant),base=std::string(argv[1])+"/"+name;
        const auto image=fit_mach_family_fixed(parse_design(read(base+".PDS")));
        check(bits(serialize_jedec(image))==bits(read(base+".JED")),name+" differs from original distinct automatic output enables");
    }
    // These dense reference cases require both partition rescue and
    // floating input retries. Check output placement separately as well.
    for(unsigned variant=0;variant<2;++variant) {
        const auto name="R131"+std::to_string(variant),base=std::string(argv[1])+"/"+name;
        const auto d=parse_design(read(base+".PDS"));
        const auto image=fit_mach_family_fixed(d);
        check(bits(serialize_jedec(image))==bits(read(base+".JED")),name+" differs from original rescued/rerouted image");
        const auto& layout=mach_device_layout(d.device);
        std::map<std::string,unsigned> ids;
        std::vector<Mach111PlaceSignal> signals;
        const unsigned first=layout.pins+layout.blocks*layout.cells+1;
        for(const auto& pin:d.pins) {
            Mach111PlaceSignal signal;signal.name=pin.name;
            ids[pin.name]=first+unsigned(signals.size());
            signal.order.serial=std::int16_t(ids.at(pin.name));
            signal.order.type=1;signals.push_back(signal);
        }
        for(const auto& equation:d.equations) {
            auto& signal=signals.at(ids.at(equation.signal)-first);
            signal.output=true;signal.order.type=0;signal.order.product_terms=1;
            const auto sop=sum_of_products(equation.expression);
            check(sop.size()==1,"rescue fixture requires one product");
            for(const auto& literal:sop[0])signal.fanins.push_back(std::uint16_t(ids.at(literal.first)));
            std::sort(signal.fanins.begin(),signal.fanins.end());
        }
        const auto placed=place_mach_family(layout,signals);
        const auto plc=read(base+".PLC");
        const std::regex assignment("Pin[ \t]+([0-9]+)[ \t]+(Q[0-9]+)");
        unsigned checked=0;
        for(auto it=std::sregex_iterator(plc.begin(),plc.end(),assignment);it!=std::sregex_iterator();++it) {
            check(placed.pins.at(ids.at((*it)[2])-first)==int(std::stoul((*it)[1])),name+" differs from original rescue output assignment");
            ++checked;
        }
        check(checked==28,"missing rescue output assignments");
        std::cout<<name<<" exact rescued placement and full routed image\n";
    }
    auto design=parse_design("CHIP T MACH120\nPIN 4 A\nPIN 5 B\nPIN 2 Y REG\nEQUATIONS\nY=A*B\nY.CLKF=A+B\n");
    bool rejected=false;try{fit_mach_family_fixed(design);}catch(const std::exception&){rejected=true;}check(rejected,"complex clock was accepted");
}
