#pragma once
#include "jedec.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace machxl {
// Physical wiring and fuse geometry from device descriptions.
// This layer encodes already placed/routed hardware; it chooses no placement.
struct MachDeviceLayout
{
    std::string device;
    unsigned pins, blocks, muxes, slots, rows, cells, macrocell_bits;
    unsigned fuse_count, electrical_count;
    std::vector<unsigned> io_pins;
    std::vector<std::vector<unsigned>> mux_sources;
    std::vector<unsigned> block_bases, macrocell_bases, mux_planes;
    bool legacy110 = false;
    bool one_hot   = false;
    std::vector<unsigned> block_offsets;
    unsigned subblock_stride         = 0;
    unsigned electrical_prefix_count = 0;
    std::vector<unsigned> dedicated_pins, dedicated_sources, clock_pins, oe_product_rows;
    unsigned alternate_macrocell_bits = 0;
    // Device constraint count and per-block constraint record index.
    // A negative count disables the legacy physical OE constraint table.
    int oe_constraint_count = -1;
    std::vector<int> oe_constraint_indices;
    unsigned macrocell_width(unsigned cell) const;
    unsigned source(unsigned block, unsigned mux, unsigned slot) const;
    std::uint64_t mux_code(unsigned mux, unsigned slot) const;
    std::vector<std::size_t> mux_addresses(unsigned block, unsigned mux) const;
    std::size_t product_address(unsigned block, unsigned row, unsigned mux) const;
    std::size_t macrocell_address(unsigned block, unsigned cell) const;
    std::size_t block_control_address(unsigned block) const;
};
const MachDeviceLayout& mach_device_layout(const std::string& device);
// Highest NODE location number. Macrocell nodes are numbered from 2; MACH4
// devices follow them with one input-register node per I/O pin.
unsigned mach_node_limit(const MachDeviceLayout& layout);
std::vector<std::string> mach_device_codec_names();
struct MachDevicePhysical
{
    struct Block
    {
        // Encoded selection bits; source-slot conversion is layout.mux_code.
        // Fixed MACH110 muxes16/17 have zero bits and require code zero.
        std::vector<std::uint64_t> mux_codes;
        // Two-bit AND literal:0 contradiction,1 negative,2 positive,3 absent.
        std::vector<std::vector<std::uint8_t>> products;
        std::vector<std::uint16_t> macrocell_words;
        std::uint8_t control_word = 0;  // Four bits on MACH3/4; absent on1/2.
    };
    std::vector<Block> blocks;
    // Physical cell order, ascending block and macrocell. MACHXL E reverses it.
    std::vector<std::uint8_t> low_power;
    std::vector<std::uint8_t> global_configuration;
};
MachDevicePhysical empty_mach_device(const MachDeviceLayout& layout);
FuseImage encode_mach_device(const MachDeviceLayout& layout, const MachDevicePhysical& physical,
                             const std::string& design = {});
MachDevicePhysical decode_mach_device(const FuseImage& image);
}  // namespace machxl
