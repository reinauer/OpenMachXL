#include "power_reports.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <regex>
#include <stdexcept>

using namespace machxl;
static void check(bool value,const char* message) {
    if(!value)throw std::runtime_error(message);
}
static FuseImage read_reference(const std::string& path) {
    std::ifstream input(path,std::ios::binary);check(bool(input),"Missing power reference");
    const std::string text{std::istreambuf_iterator<char>(input),{}};
    FuseImage image;image.device="MACH446";image.pin_count=mach_device_layout(image.device).pins;std::smatch match;
    check(std::regex_search(text,match,std::regex("QF([0-9]+)")),"Missing QF");
    image.fuses.resize(std::stoul(match[1]));
    const std::regex rows("L([0-9]+)[ \\r\\n]+([01 \\r\\n]+)\\*");
    for(auto row=std::sregex_iterator(text.begin(),text.end(),rows);row!=std::sregex_iterator();++row) {
        auto address=std::stoul((*row)[1]);
        for(char bit:(*row)[2].str())if(bit=='0'||bit=='1')image.fuses.at(address++)=bit-'0';
    }
    check(std::regex_search(text,match,std::regex("[\\r\\n]E([01\\r\\n ]+)\\*")),"Missing power fuses");
    for(char bit:match[1].str())if(bit=='0'||bit=='1')image.electrical.push_back(bit-'0');
    return image;
}
int main(int argc,char** argv) {
    if(argc==2) {
        std::cout<<std::setprecision(10)<<corrected_mach446_icc(decode_mach_device(read_reference(argv[1])))<<'\n';
        return 0;
    }
    const auto& layout=mach_device_layout("MACH446");
    auto physical=empty_mach_device(layout);
    std::fill(physical.low_power.begin(),physical.low_power.end(),1);
    for(auto& block:physical.blocks)for(auto& row:block.products)std::fill(row.begin(),row.end(),0);
    const auto baseline=corrected_mach446_icc(physical);
    check(std::abs(baseline-50.2f)<0.002f,"All-disabled low-power baseline");
    // Each of the720 physical rows must contribute exactly once. Cover the
    // last OE row, both global controls and all high-index logic cells.
    for(unsigned block=0;block<layout.blocks;++block)for(unsigned row=0;row<layout.rows;++row) {
        const bool shared=row>=88||row%11==0;
        auto& product=physical.blocks[block].products[row];
        std::fill(product.begin(),product.end(),3);
        check(std::abs(corrected_mach446_icc(physical)-baseline-(shared?0.115f:0.07f))<0.0001f,"True-row coefficient/count");
        product.front()=2;
        check(std::abs(corrected_mach446_icc(physical)-baseline-(shared?0.06f:0.05f))<0.0001f,"Ordinary-row coefficient/count");
        std::fill(product.begin(),product.end(),0);
    }
    std::vector<unsigned> owners(128);std::iota(owners.begin(),owners.end(),0);
    for(bool low:{false,true}) {
        const auto request=Mach446PowerRequest::source(low);
        check(request.low==low && request.high!=low,"Canonical source request flags");
    }
    auto& mode=physical.blocks[0].macrocell_words[2];mode|=512;
    check(mach446_power_annotation(2,Mach446PowerRequest::source(false),physical,owners)=="H","High requested power");
    check(mach446_power_annotation(2,Mach446PowerRequest{true,true},physical,owners)=="#H","Conflicting raw requests decode defensively");
    check(mach446_power_annotation(2,Mach446PowerRequest::source(true),physical,owners)=="L","Low-power signal");
    owners[3]=2;physical.low_power[3]=0;
    check(mach446_power_annotation(2,Mach446PowerRequest::source(true),physical,owners)=="LHd","Borrowed high-power data");
    mode&=~512;physical.low_power[2]=0;owners[2]=1;
    check(mach446_power_annotation(2,Mach446PowerRequest::source(true),physical,owners)=="LHdc","High-power data and controls");
    physical.low_power[3]=1;
    check(mach446_power_annotation(2,Mach446PowerRequest::source(true),physical,owners)=="LHc","High-power controls only");
    physical.blocks[0].products.pop_back();
    try { (void)corrected_mach446_icc(physical);throw std::runtime_error("Invalid geometry accepted"); }
    catch(const std::invalid_argument&) {}
    std::cout<<"MACH446 corrected row accounting and power annotations passed\n";
}
