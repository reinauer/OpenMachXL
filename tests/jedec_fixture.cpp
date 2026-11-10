#include "jedec.hpp"
#include "device_mach111.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

static std::vector<std::uint8_t> read_bits(const char* path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open test bit vector");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

int main(int argc, char** argv) {
    if (argc != 5) return 2;
    try {
        machxl::FuseImage image;
        image.device = argv[3];
        image.design = "ROUNDTRIP";
        image.pin_count = std::stoul(argv[4]);
        image.fuses = read_bits(argv[1]);
        image.electrical = read_bits(argv[2]);
        if (image.device == "MACH111")
            image = machxl::encode_mach111(machxl::decode_mach111(image), image.design);
        std::cout << machxl::serialize_jedec(image);
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
