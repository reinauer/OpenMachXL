#include "device_family.hpp"
#include <algorithm>
#include <stdexcept>

namespace machxl {
namespace {
void require_valid_encoding(bool valid, const char* message)
{
    if (!valid)
        throw std::invalid_argument(message);
}
const std::vector<MachDeviceLayout>& device_layout_table()
{
    static const auto result = [] {
        std::vector<MachDeviceLayout> data = {
#include "device_family_tables.inc"
        };
        for (const auto alias : {121u, 131u, 221u, 446u}) {
            const auto source = "MACH" + std::to_string(alias - 1);
            auto item   = *std::find_if(data.begin(), data.end(), [&](const auto& d) { return d.device == source; });
            item.device = "MACH" + std::to_string(alias);
            item.electrical_count = item.electrical_prefix_count + item.blocks * item.cells;
            data.push_back(std::move(item));
        }
        return data;
    }();
    return result;
}
void validate_block_index(const MachDeviceLayout& d, unsigned block)
{
    require_valid_encoding(block < d.blocks, "device block out of range");
}
}  // namespace
const MachDeviceLayout& mach_device_layout(const std::string& device)
{
    for (const auto& layout : device_layout_table())
        if (layout.device == device)
            return layout;
    throw std::invalid_argument("physical codec is not implemented for " + device);
}
unsigned mach_node_limit(const MachDeviceLayout& layout)
{
    const bool input_registers = layout.device.compare(0, 5, "MACH4") == 0;
    return layout.blocks * layout.cells + 1 + (input_registers ? unsigned(layout.io_pins.size()) : 0u);
}
std::vector<std::string> mach_device_codec_names()
{
    std::vector<std::string> names;
    for (const auto& layout : device_layout_table())
        names.push_back(layout.device);
    std::sort(names.begin(), names.end());
    return names;
}
unsigned MachDeviceLayout::source(unsigned block, unsigned mux, unsigned slot) const
{
    validate_block_index(*this, block);
    require_valid_encoding(mux < muxes && slot < slots, "device mux/slot out of range");
    auto value = mux_sources[legacy110 ? block : 0][mux * slots + slot];
    require_valid_encoding(value != 1000, "device mux slot is unconnected");
    return value;
}
std::uint64_t MachDeviceLayout::mux_code(unsigned mux, unsigned slot) const
{
    require_valid_encoding(mux < muxes && slot < slots, "device mux/slot out of range");
    if (legacy110) {
        if (mux == 16 || mux == 17) {
            require_valid_encoding(slot == 0, "fixed MACH110 mux requires slot zero");
            return 0;
        }
        if (mux < 16) {
            require_valid_encoding(slot < 4, "MACH110 mux slot is unconnected");
            return slot;
        }
        return slot > 1 ? 2 * slot - 1 : slot;
    }
    if (one_hot)
        return ((std::uint64_t(1) << slots) - 1) ^ (std::uint64_t(1) << slot);
    if (device == "MACH211")
        return (slot & 1) * 8 + ((slot & 3) >> 1) + (slot & ~3u) - (slot >> 3) * 6;
    return slots == 12 && slot > 5 ? slot + 2 : slot;
}
std::vector<std::size_t> MachDeviceLayout::mux_addresses(unsigned block, unsigned mux) const
{
    validate_block_index(*this, block);
    require_valid_encoding(mux < muxes, "device mux out of range");
    std::vector<std::size_t> addresses;
    if (legacy110) {
        if (mux == 16 || mux == 17)
            return addresses;
        unsigned count = mux < 16 ? 2 : 3;
        int base       = static_cast<int>(block_bases[block] + (mux < 16 ? mux * 2 : 32 + (mux - 18) * 3));
        if (mux >= 8 && mux < 16)
            base += 6;
        if (mux >= 18 && mux < 20)
            base -= 16;
        for (unsigned bit = 0; bit < count; ++bit)
            addresses.push_back(base + mux_planes[bit]);
    } else {
        for (unsigned bit = 0; bit < mux_planes.size(); ++bit) {
            auto stride = device == "MACH111" && bit == 0 ? 1u : 2u;
            addresses.push_back(block_bases[block] + mux * stride + mux_planes[bit] +
                                (one_hot ? block_offsets[block] : 0));
        }
    }
    return addresses;
}
std::size_t MachDeviceLayout::product_address(unsigned block, unsigned row, unsigned mux) const
{
    validate_block_index(*this, block);
    require_valid_encoding(row < rows && mux < muxes, "device product address out of range");
    // MACH110 stores products in a separate global AND array.
    auto base = legacy110 ? block * rows * muxes * 2
                          : block_bases[block] + muxes * mux_planes.size() * (subblock_stride ? 2 : 1) +
                                subblock_stride * block_offsets[block];
    return base + row * muxes * 2 + mux * 2;
}
unsigned MachDeviceLayout::macrocell_width(unsigned cell) const
{
    require_valid_encoding(cell < cells, "device macrocell out of range");
    return alternate_macrocell_bits && (cell & 1) ? alternate_macrocell_bits : macrocell_bits;
}
std::size_t MachDeviceLayout::macrocell_address(unsigned block, unsigned cell) const
{
    validate_block_index(*this, block);
    require_valid_encoding(cell < cells, "device macrocell out of range");
    return (legacy110 ? macrocell_bases[block]
                      : block_bases[block] + macrocell_bases[0] + subblock_stride * block_offsets[block]) +
           (alternate_macrocell_bits
                ? (cell / 2) * (macrocell_bits + alternate_macrocell_bits) + (cell & 1) * macrocell_bits
                : cell * macrocell_bits);
}
std::size_t MachDeviceLayout::block_control_address(unsigned block) const
{
    require_valid_encoding(one_hot, "device has no block control field");
    return macrocell_address(block, 0) - 4;
}
MachDevicePhysical empty_mach_device(const MachDeviceLayout& d)
{
    MachDevicePhysical p;
    p.blocks.resize(d.blocks);
    for (auto& b : p.blocks) {
        b.mux_codes.resize(d.muxes);
        b.products.resize(d.rows, std::vector<std::uint8_t>(d.muxes));
        b.macrocell_words.resize(d.cells);
    }
    p.low_power.resize(d.electrical_count - d.electrical_prefix_count);
    p.global_configuration.resize(d.electrical_prefix_count);
    return p;
}
FuseImage encode_mach_device(const MachDeviceLayout& d, const MachDevicePhysical& p, const std::string& design)
{
    require_valid_encoding(p.blocks.size() == d.blocks &&
                               p.low_power.size() == d.electrical_count - d.electrical_prefix_count &&
                               p.global_configuration.size() == d.electrical_prefix_count,
                           "physical device dimensions differ");
    FuseImage image;
    image.layout = JedecLayout::MachFamily;
    if (d.device == "MACH355" || d.device == "MACH445" || d.device == "MACH446" || d.device == "MACH465")
        image.user_code.resize(32);
    image.device    = d.device;
    image.design    = design;
    image.pin_count = d.pins;
    image.fuses.resize(d.fuse_count);
    for (unsigned b = 0; b < d.blocks; ++b) {
        const auto& block = p.blocks[b];
        require_valid_encoding(block.mux_codes.size() == d.muxes && block.products.size() == d.rows &&
                                   block.macrocell_words.size() == d.cells,
                               "physical block dimensions differ");
        for (unsigned mux = 0; mux < d.muxes; ++mux) {
            auto addresses = d.mux_addresses(b, mux);
            require_valid_encoding(block.mux_codes[mux] < (std::uint64_t(1) << addresses.size()),
                                   "encoded mux exceeds physical width");
            for (unsigned bit = 0; bit < addresses.size(); ++bit)
                image.fuses.at(addresses[bit]) = (block.mux_codes[mux] >> bit) & 1;
        }
        for (unsigned row = 0; row < d.rows; ++row) {
            require_valid_encoding(block.products[row].size() == d.muxes, "physical row dimensions differ");
            for (unsigned mux = 0; mux < d.muxes; ++mux) {
                auto literal = block.products[row][mux];
                require_valid_encoding(literal < 4, "invalid two-bit AND literal");
                auto address                = d.product_address(b, row, mux);
                image.fuses.at(address)     = literal & 1;
                image.fuses.at(address + 1) = literal >> 1;
            }
        }
        if (d.one_hot) {
            require_valid_encoding(block.control_word < 16, "block control exceeds physical width");
            for (unsigned bit = 0; bit < 4; ++bit)
                image.fuses.at(d.block_control_address(b) + bit) = (block.control_word >> bit) & 1;
        }
        for (unsigned cell = 0; cell < d.cells; ++cell) {
            auto word = block.macrocell_words[cell];
            require_valid_encoding(word < (1u << d.macrocell_width(cell)), "macrocell word exceeds physical width");
            for (unsigned bit = 0; bit < d.macrocell_width(cell); ++bit)
                image.fuses.at(d.macrocell_address(b, cell) + bit) = (word >> bit) & 1;
        }
    }
    image.electrical = p.global_configuration;
    image.electrical.insert(image.electrical.end(), p.low_power.rbegin(), p.low_power.rend());
    for (auto bit : image.electrical)
        require_valid_encoding(bit < 2, "electrical fuse must be a bit");
    return image;
}
MachDevicePhysical decode_mach_device(const FuseImage& image)
{
    const auto& d = mach_device_layout(image.device);
    require_valid_encoding(image.pin_count == d.pins && image.fuses.size() == d.fuse_count &&
                               image.electrical.size() == d.electrical_count,
                           "fuse image dimensions differ from device");
    for (const auto* data : {&image.fuses, &image.electrical})
        for (auto bit : *data)
            require_valid_encoding(bit < 2, "fuse must be a bit");
    auto p = empty_mach_device(d);
    for (unsigned b = 0; b < d.blocks; ++b) {
        auto& block = p.blocks[b];
        for (unsigned mux = 0; mux < d.muxes; ++mux) {
            auto addresses = d.mux_addresses(b, mux);
            for (unsigned bit = 0; bit < addresses.size(); ++bit)
                block.mux_codes[mux] |= std::uint64_t(image.fuses.at(addresses[bit])) << bit;
        }
        for (unsigned row = 0; row < d.rows; ++row)
            for (unsigned mux = 0; mux < d.muxes; ++mux) {
                auto address             = d.product_address(b, row, mux);
                block.products[row][mux] = image.fuses.at(address) | (image.fuses.at(address + 1) << 1);
            }
        if (d.one_hot)
            for (unsigned bit = 0; bit < 4; ++bit)
                block.control_word |= image.fuses.at(d.block_control_address(b) + bit) << bit;
        for (unsigned cell = 0; cell < d.cells; ++cell)
            for (unsigned bit = 0; bit < d.macrocell_width(cell); ++bit)
                block.macrocell_words[cell] |= image.fuses.at(d.macrocell_address(b, cell) + bit) << bit;
    }
    p.global_configuration.assign(image.electrical.begin(), image.electrical.begin() + d.electrical_prefix_count);
    p.low_power.assign(image.electrical.rbegin(), image.electrical.rend() - d.electrical_prefix_count);
    return p;
}
}  // namespace machxl
