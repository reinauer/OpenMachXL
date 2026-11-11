#include "device_mach111.hpp"

#include <stdexcept>

namespace machxl {
namespace {
// Original PHY_111.DAT entries 4..35 and 36..243. These are device wiring
// tables, not design-specific placements or captured compiler outputs.
constexpr std::array<unsigned, 32> pins = {2,  3,  4,  5,  6,  7,  8,  9,  14, 15, 16, 17, 18, 19, 20, 21,
                                           43, 42, 41, 40, 39, 38, 37, 36, 31, 30, 29, 28, 27, 26, 25, 24};
constexpr unsigned mux_sources[26][8]   = {
    {18, 2, 49, 35, 5, 27, 40, 50},   {26, 8, 61, 35, 64, 21, 34, 54}, {16, 65, 49, 45, 9, 21, 38, 48},
    {16, 2, 51, 39, 13, 25, 32, 58},  {30, 12, 59, 41, 5, 21, 68, 58}, {22, 14, 61, 67, 1, 17, 38, 50},
    {30, 8, 55, 43, 13, 19, 40, 60},  {20, 10, 51, 41, 7, 23, 40, 48}, {16, 4, 55, 47, 3, 29, 34, 52},
    {24, 4, 61, 41, 15, 25, 42, 60},  {28, 14, 51, 43, 9, 31, 36, 52}, {18, 8, 59, 67, 11, 23, 46, 56},
    {69, 14, 49, 37, 7, 29, 44, 56},  {30, 2, 57, 37, 3, 23, 36, 62},  {24, 6, 53, 39, 9, 29, 68, 54},
    {22, 12, 63, 33, 3, 31, 42, 56},  {24, 65, 57, 35, 7, 17, 32, 52}, {20, 0, 59, 45, 1, 27, 42, 52},
    {22, 6, 55, 45, 64, 66, 46, 58},  {69, 10, 57, 47, 5, 19, 38, 54}, {18, 0, 55, 33, 15, 17, 36, 48},
    {28, 4, 53, 33, 13, 27, 46, 62},  {28, 0, 63, 39, 11, 19, 34, 50}, {26, 6, 63, 43, 1, 25, 44, 62},
    {26, 10, 53, 37, 11, 31, 32, 60}, {20, 12, 53, 47, 15, 66, 44, 50}};
constexpr unsigned dedicated_pins[6] = {10, 11, 13, 32, 33, 35};

void require_valid_encoding(bool condition, const char* message)
{
    if (!condition)
        throw std::invalid_argument(message);
}
}  // namespace

unsigned mach111_io_pin(unsigned cell)
{
    require_valid_encoding(cell < pins.size(), "MACH111 macrocell must be in 0..31");
    return pins[cell];
}

unsigned mach111_pin_source(unsigned pin)
{
    for (unsigned c = 0; c < pins.size(); ++c)
        if (pins[c] == pin)
            return 2 * c + 1;
    for (unsigned n = 0; n < 6; ++n)
        if (dedicated_pins[n] == pin)
            return 64 + n;
    throw std::invalid_argument("MACH111 pin is not a signal input");
}

unsigned mach111_feedback_source(unsigned cell)
{
    require_valid_encoding(cell < pins.size(), "MACH111 macrocell must be in 0..31");
    return 2 * cell;
}

unsigned mach111_mux_source(unsigned mux, unsigned slot)
{
    require_valid_encoding(mux < 26 && slot < 8, "MACH111 mux or slot out of range");
    return mux_sources[mux][slot];
}

std::vector<Mach111MuxChoice> mach111_mux_choices(unsigned source)
{
    require_valid_encoding(source < 70, "MACH111 source must be in 0..69");
    std::vector<Mach111MuxChoice> choices;
    for (unsigned mux = 0; mux < 26; ++mux)
        for (unsigned slot = 0; slot < 8; ++slot)
            if (mux_sources[mux][slot] == source)
                choices.push_back({mux, slot});
    return choices;
}

std::size_t mach111_product_address(unsigned block, unsigned row, unsigned mux)
{
    require_valid_encoding(block < 2 && row < 70 && mux < 26, "MACH111 product address out of range");
    return block * 3862 + 78 + row * 52 + mux * 2;
}

std::size_t mach111_macrocell_address(unsigned cell)
{
    require_valid_encoding(cell < 32, "MACH111 macrocell must be in 0..31");
    return (cell / 16) * 3862 + 3718 + (cell % 16) * 9;
}

std::array<unsigned, 16> mach111_balance_clocks(const std::array<int, 16>& clocks)
{
    std::array<unsigned, 4> loads{};
    for (const auto clock : clocks) {
        require_valid_encoding(clock >= -1 && clock < 4, "MACH111 clock must be unspecified or in 0..3");
        if (clock != -1)
            ++loads[clock];
    }
    std::array<unsigned, 16> assigned{};
    for (unsigned cell = 0; cell < 16; ++cell) {
        if (clocks[cell] != -1) {
            assigned[cell] = static_cast<unsigned>(clocks[cell]);
        } else {
            unsigned selected = 0;
            for (unsigned clock = 1; clock < 4; ++clock)
                if (loads[clock] < loads[selected])
                    selected = clock;
            ++loads[selected];
            assigned[cell] = selected;
        }
    }
    return assigned;
}

unsigned mach111_unused_mux_slot(unsigned mux, const std::array<bool, 32>& allocated_cells,
                                 const std::array<bool, 45>& allocated_pins)
{
    require_valid_encoding(mux < 26, "MACH111 mux out of range");
    int first_free_pin = -1;
    for (unsigned slot = 0; slot < 8; ++slot) {
        const auto source = mux_sources[mux][slot];
        if (source < 64 && (source & 1) == 0) {
            if (!allocated_cells[source / 2])
                return slot;
        } else {
            const auto pin = source < 64 ? pins[source / 2] : dedicated_pins[source - 64];
            if (!allocated_pins[pin] && first_free_pin == -1)
                first_free_pin = static_cast<int>(slot);
        }
    }
    require_valid_encoding(first_free_pin != -1, "MACH111 unused mux needs original occupied-source load ranking");
    return static_cast<unsigned>(first_free_pin);
}

unsigned mach111_unused_mux_slot(unsigned mux, const std::array<bool, 32>& allocated_cells,
                                 const std::array<bool, 45>& allocated_pins, std::array<unsigned, 70>& loads)
{
    require_valid_encoding(mux < 26, "MACH111 mux out of range");
    int free_pin = -1;
    for (unsigned slot = 0; slot < 8; ++slot) {
        const auto source = mux_sources[mux][slot];
        if (source < 64 && !(source & 1)) {
            if (!allocated_cells[source / 2])
                return slot;
        } else {
            const auto pin = source < 64 ? pins[source / 2] : dedicated_pins[source - 64];
            if (!allocated_pins[pin] && free_pin < 0)
                free_pin = static_cast<int>(slot);
        }
    }
    if (free_pin >= 0)
        return static_cast<unsigned>(free_pin);
    // Initialize source loads to zero. The occupied-source fallback chooses
    // the first minimum and increments only that source's load.
    unsigned selected = 0;
    for (unsigned slot = 1; slot < 8; ++slot)
        if (loads[mux_sources[mux][slot]] < loads[mux_sources[mux][selected]])
            selected = slot;
    ++loads[mux_sources[mux][selected]];
    return selected;
}

FuseImage encode_mach111(const Mach111Physical& physical, const std::string& design)
{
    FuseImage image;
    image.layout    = JedecLayout::Mach111;
    image.device    = "MACH111";
    image.design    = design;
    image.pin_count = 44;
    image.fuses.resize(Mach111Physical::fuse_count);
    image.electrical.resize(32);
    for (unsigned b = 0; b < 2; ++b) {
        const auto& block = physical.blocks[b];
        const auto base   = b * 3862;
        for (unsigned m = 0; m < 26; ++m) {
            const auto slot = block.mux_slots[m];
            require_valid_encoding(slot < 8, "MACH111 mux slot must be in 0..7");
            image.fuses[base + m]          = slot & 1;
            image.fuses[base + 26 + 2 * m] = (slot >> 1) & 1;
            image.fuses[base + 27 + 2 * m] = (slot >> 2) & 1;
        }
        for (unsigned r = 0; r < 70; ++r)
            for (unsigned m = 0; m < 26; ++m) {
                const auto literal = static_cast<unsigned>(block.products[r][m]);
                require_valid_encoding(literal < 4, "MACH111 product literal out of range");
                const auto address       = mach111_product_address(b, r, m);
                image.fuses[address]     = literal & 1;
                image.fuses[address + 1] = (literal >> 1) & 1;
            }
        for (unsigned c = 0; c < 16; ++c) {
            const auto word = block.macrocell_words[c];
            require_valid_encoding(word < 512, "MACH111 macrocell word exceeds nine bits");
            for (unsigned bit = 0; bit < 9; ++bit)
                image.fuses[mach111_macrocell_address(16 * b + c) + bit] = (word >> bit) & 1;
        }
    }
    for (unsigned cell = 0; cell < 32; ++cell)
        image.electrical[31 - cell] = physical.low_power[cell];
    return image;
}

Mach111Physical decode_mach111(const FuseImage& image)
{
    require_valid_encoding(image.device == "MACH111", "physical decoder requires MACH111");
    require_valid_encoding(image.pin_count == 44, "MACH111 requires 44 pins");
    require_valid_encoding(image.fuses.size() == Mach111Physical::fuse_count,
                           "MACH111 requires exactly 7724 main fuses");
    require_valid_encoding(image.electrical.size() == 32, "MACH111 requires exactly 32 power bits");
    for (auto bit : image.fuses)
        require_valid_encoding(bit < 2, "main fuse must be a bit");
    for (auto bit : image.electrical)
        require_valid_encoding(bit < 2, "power fuse must be a bit");
    Mach111Physical physical;
    for (unsigned b = 0; b < 2; ++b) {
        auto& block     = physical.blocks[b];
        const auto base = b * 3862;
        for (unsigned m = 0; m < 26; ++m)
            block.mux_slots[m] = std::uint8_t(image.fuses[base + m] | (image.fuses[base + 26 + 2 * m] << 1) |
                                              (image.fuses[base + 27 + 2 * m] << 2));
        for (unsigned r = 0; r < 70; ++r)
            for (unsigned m = 0; m < 26; ++m) {
                const auto address = mach111_product_address(b, r, m);
                block.products[r][m] =
                    static_cast<Mach111Physical::Literal>(image.fuses[address] | (image.fuses[address + 1] << 1));
            }
        for (unsigned c = 0; c < 16; ++c)
            for (unsigned bit = 0; bit < 9; ++bit)
                block.macrocell_words[c] |= image.fuses[mach111_macrocell_address(16 * b + c) + bit] << bit;
    }
    for (unsigned cell = 0; cell < 32; ++cell)
        physical.low_power[cell] = image.electrical[31 - cell] != 0;
    return physical;
}

}  // namespace machxl
