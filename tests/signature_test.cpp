#include "frontend.hpp"
#include "signature.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <stdexcept>
using namespace machxl;
static std::string read(const std::filesystem::path& path) {
    std::ifstream stream(path,std::ios::binary);
    if(!stream) throw std::runtime_error("Cannot read "+path.string());
    return {std::istreambuf_iterator<char>(stream),{}};
}
int main(int argc,char** argv) {
    if(argc!=2) throw std::runtime_error("Expected reference directory");
    unsigned count=0;
    for(const auto& entry:std::filesystem::directory_iterator(argv[1])) {
        if(entry.path().extension()!=".PDS") continue;
        const auto design=parse_design(read(entry.path()));
        FuseImage image;image.device=design.device;image.design=design.chip;image.pin_count=1;image.fuses={0};
        apply_signature(image,design.signature);
        auto reference=entry.path();reference.replace_extension(".JED");
        const auto original=read(reference),native=serialize_jedec(image);
        std::smatch a,b;
        const std::regex record("[\\r\\n]U[01]+\\*");
        const auto got=std::regex_search(native,a,record),want=std::regex_search(original,b,record);
        if(got!=want || (got && a[0]!=b[0]))
            throw std::runtime_error(entry.path().stem().string()+": user code differs");
        ++count;
    }
    if(count!=31) throw std::runtime_error("Expected 31 signature reference cases");
    std::cout<<count<<" original JTAG signature records match\n";
}
