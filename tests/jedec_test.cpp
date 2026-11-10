#include "jedec.hpp"
#include <iostream>
#include <stdexcept>

using namespace machxl;
namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class F> void rejects(F operation) {
    try { operation(); } catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("invalid JEDEC image accepted");
}
}
int main() {
    try {
        FuseImage image{"MACH111", "CHECK", 44, {1,0,1}, {1,1}, false, {"checksum test"}};
        require(fuse_checksum(image) == 29, "E bits must immediately follow unaligned QF bits");
        auto text = serialize_jedec(image, "native\n");
        require(text.find("QF3*\r\n") != std::string::npos, "QF record");
        require(text.find("L00000 101*\r\nE11*\r\nC001D*") != std::string::npos, "data records");
        auto first = text.find('\x02'), last = text.find('\x03');
        unsigned sum = 0;
        for (auto n = first; n <= last; ++n) sum += static_cast<unsigned char>(text[n]);
        require(std::stoul(text.substr(last + 1, 4), nullptr, 16) == (sum & 65535), "transmission checksum");
        auto variant = image;
        variant.fuses[0] = 2;
        rejects([&] { serialize_jedec(variant); });
        variant = image; variant.device += "*F1";
        rejects([&] { serialize_jedec(variant); });
        variant = image; variant.fuses.clear();
        rejects([&] { serialize_jedec(variant); });
        variant = image; variant.pin_count = 0;
        rejects([&] { serialize_jedec(variant); });
        variant = image; variant.layout = JedecLayout::Mach111;
        rejects([&] { serialize_jedec(variant); });
        rejects([&] { serialize_jedec(image, "illegal\x02" "frame"); });
        variant=image;variant.has_test_vectors=true;variant.test_vectors={std::string(44,'X')};
        auto simulated=serialize_jedec(variant);
        require(simulated.find("QV0001*\r\n")!=std::string::npos,"simulation vector count");
        require(simulated.find("X0*\r\nV0001 "+std::string(44,'X')+"*\r\nC001D*")!=std::string::npos,"simulation records and unchanged fuse checksum");
        first=simulated.find('\x02');last=simulated.find('\x03');sum=0;
        for(auto n=first;n<=last;++n)sum+=static_cast<unsigned char>(simulated[n]);
        require(std::stoul(simulated.substr(last+1,4),nullptr,16)==(sum&65535),"transmission checksum includes vectors");
        variant.test_vectors[0][3]='*';rejects([&]{serialize_jedec(variant);});
        variant.test_vectors={"0"};rejects([&]{serialize_jedec(variant);});
        variant.test_vectors={std::string(44,'X')};variant.has_test_vectors=false;rejects([&]{serialize_jedec(variant);});
        std::cout << "JEDEC writer tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
