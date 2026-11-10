#include "device_family.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
using namespace machxl;
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static FuseImage read_reference(const std::string& path) {
    std::ifstream in(path);check(bool(in),"missing device reference");
    std::string text{std::istreambuf_iterator<char>(in),{}};
    FuseImage image; std::smatch m;
    check(std::regex_search(text,m,std::regex("\\x02(MACH[0-9]+)")),"missing device header");image.device=m[1];
    check(std::regex_search(text,m,std::regex("QP([0-9]+)")),"missing QP");image.pin_count=std::stoul(m[1]);
    check(std::regex_search(text,m,std::regex("QF([0-9]+)")),"missing QF");image.fuses.resize(std::stoul(m[1]));
    std::regex rows("L([0-9]+)[ \r\n]+([01 \r\n]+)\\*");
    for(auto it=std::sregex_iterator(text.begin(),text.end(),rows);it!=std::sregex_iterator();++it) {
        auto address=std::stoul((*it)[1]);for(char bit:(*it)[2].str()) if(bit=='0'||bit=='1') image.fuses.at(address++)=bit-'0';
    }
    if(std::regex_search(text,m,std::regex("[\\r\\n]E([01\\r\\n ]+)\\*")))
        for(char bit:m[1].str())if(bit=='0'||bit=='1')image.electrical.push_back(bit-'0');
    image.design = "FAMILY";
    const auto first_physical = std::min(text.find("NOTE  Central"), text.find("NOTE  Product"));
    auto physical = std::min(first_physical, text.find("NOTE  BLOCK"));
    physical = std::min(physical, text.find("NOTE  Interleaved"));
    std::regex note("NOTE ([^*]+)\\*");
    const auto prefix = text.substr(0, physical);
    for(auto it=std::sregex_iterator(prefix.begin(),prefix.end(),note);it!=std::sregex_iterator();++it) image.notes.push_back((*it)[1]);
    if(std::regex_search(text,m,std::regex("[\\r\\n]U([01]+)\\*")))
        for(char bit:m[1].str()) image.user_code.push_back(bit-'0');
    return image;
}
int main(int argc,char**argv) {
    check(mach_device_layout("MACH465").clock_pins==std::vector<unsigned>({187,74,83,178}),"MACH465 hardware clock order");
    check(argc==2,"expected fixture directory");
    for(const auto& name:mach_device_codec_names()) {
        const auto& d=mach_device_layout(name);
        if(d.oe_constraint_count>0) {
            check(d.oe_constraint_indices.size()==d.blocks,"missing DEV constraint selector");
            for(auto index:d.oe_constraint_indices)
                check(index>=0&&index<d.oe_constraint_count,"DEV constraint selector outside table");
        } else check(d.oe_constraint_indices.empty(),"disabled DEV constraints have selectors");
        std::vector<unsigned> visits(d.fuse_count);
        auto mark=[&](std::size_t at){check(at<visits.size(),"physical address outside device");++visits[at];};
        for(unsigned b=0;b<d.blocks;++b) {
            for(unsigned m=0;m<d.muxes;++m)for(auto a:d.mux_addresses(b,m))mark(a);
            for(unsigned r=0;r<d.rows;++r)for(unsigned m=0;m<d.muxes;++m){auto a=d.product_address(b,r,m);mark(a);mark(a+1);}
            if(d.one_hot)for(unsigned bit=0;bit<4;++bit)mark(d.block_control_address(b)+bit);
            for(unsigned c=0;c<d.cells;++c)for(unsigned bit=0;bit<d.macrocell_width(c);++bit)mark(d.macrocell_address(b,c)+bit);
        }
        check(std::all_of(visits.begin(),visits.end(),[](auto n){return n==1;}),"physical map has overlaps or holes");
        auto original=read_reference(std::string(argv[1])+"/M"+name.substr(4)+".JED");
        auto decoded=decode_mach_device(original);
        auto encoded=encode_mach_device(d,decoded,"FAMILY");
        check(encoded.fuses==original.fuses&&encoded.electrical==original.electrical,"original physical codec round trip differs");
        encoded.dos_compatibility=true;
        encoded.notes=original.notes;
        encoded.user_code=original.user_code;
        std::ifstream reference(std::string(argv[1])+"/M"+name.substr(4)+".JED",std::ios::binary);
        std::string reference_text{std::istreambuf_iterator<char>(reference),{}};
        const auto expected=reference_text.substr(reference_text.find('\x02'));
        const auto actual=serialize_jedec(encoded);
        if(actual!=expected) {
            std::ofstream("/private/tmp/"+name+"-actual.jed",std::ios::binary)<<actual;
            std::ofstream("/private/tmp/"+name+"-expected.jed",std::ios::binary)<<expected;
            throw std::runtime_error(name+" original JEDEC record formatting differs");
        }
        // Every encoded bit is exercised independently of the original's defaults.
        for(std::size_t i=0;i<original.fuses.size();++i)original.fuses[i]=((i*0x9e37u)>>(i%13))&1;
        for(std::size_t i=0;i<original.electrical.size();++i)original.electrical[i]=i%3==1;
        encoded=encode_mach_device(d,decode_mach_device(original));
        check(encoded.fuses==original.fuses&&encoded.electrical==original.electrical,"arbitrary physical bits did not round trip");
        std::cout<<name<<" "<<d.fuse_count<<" main / "<<d.electrical_count<<" electrical bits\n";
    }
    const auto& a=mach_device_layout("MACH110");
    check(a.mux_addresses(0,8)==std::vector<std::size_t>({6438,6439}),"MACH110 mux8 shuffle");
    check(a.mux_addresses(0,18)==std::vector<std::size_t>({6432,6433,6434}),"MACH110 mux18 shuffle");
    check(a.mux_addresses(0,16).empty(),"MACH110 fixed input has fuses");
    check(a.mux_code(18,4)==7,"MACH110 slot4 encoding");
    check(mach_device_layout("MACH211").mux_code(0,11)==11,"MACH211 slot encoding");
    check(mach_device_layout("MACH210").mux_code(0,6)==8,"12-way slot encoding");
}
