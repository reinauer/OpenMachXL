#pragma once

#include "jedec.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace machxl {

// Physical representation, independent of placement and routing policy. Every
// field must be supplied by the caller; zero initialization is not a fitted
// device's default configuration. In particular, unused macrocells have
// placement-dependent configuration in the original tools.
struct Mach111Physical
{
    static constexpr std::size_t block_count     = 2;
    static constexpr std::size_t mux_count       = 26;
    static constexpr std::size_t row_count       = 70;
    static constexpr std::size_t cells_per_block = 16;
    static constexpr std::size_t block_fuses     = 3862;
    static constexpr std::size_t fuse_count      = block_count * block_fuses;

    enum class Literal : std::uint8_t
    {
        Contradiction = 0,
        Negative      = 1,
        Positive      = 2,
        DontCare      = 3
    };
    using Product = std::array<Literal, mux_count>;
    struct Block
    {
        std::array<std::uint8_t, mux_count> mux_slots{};
        std::array<Product, row_count> products{};
        // Bit n is the bit at increasing physical address n (n = 0..8).
        std::array<std::uint16_t, cells_per_block> macrocell_words{};
    };
    std::array<Block, block_count> blocks{};
    // Global cell order A0..A15,B0..B15; true encodes E=1.
    std::array<bool, 32> low_power{};
};

struct Mach111MuxChoice
{
    unsigned mux;
    unsigned slot;
};

unsigned mach111_io_pin(unsigned global_cell);
unsigned mach111_pin_source(unsigned physical_pin);
unsigned mach111_feedback_source(unsigned global_cell);
unsigned mach111_mux_source(unsigned mux, unsigned slot);
std::vector<Mach111MuxChoice> mach111_mux_choices(unsigned source);
std::size_t mach111_product_address(unsigned block, unsigned row, unsigned mux);
std::size_t mach111_macrocell_address(unsigned global_cell);

// Balance clock loads in physical cell order. Values
// 0..3 are explicitly selected clocks (pins 11,13,33,35); -1 is unspecified.
// Count explicit clock loads first, then choose the lowest-index least-used
// clock for each unspecified cell. Invoke independently for each block.
std::array<unsigned, 16> mach111_balance_clocks(const std::array<int, 16>& clocks);

// Initialize unused CSM muxes by preferring the first free
// feedback macrocell, otherwise the first free physical pin. The fully
// occupied fallback also needs original source loads and is not guessed.
unsigned mach111_unused_mux_slot(unsigned mux, const std::array<bool, 32>& allocated_cells,
                                 const std::array<bool, 45>& allocated_pins);
// The original initializes these counters to zero for the whole device,
// then increments only an occupied source chosen for an unused mux.
unsigned mach111_unused_mux_slot(unsigned mux, const std::array<bool, 32>& allocated_cells,
                                 const std::array<bool, 45>& allocated_pins,
                                 std::array<unsigned, 70>& unused_source_loads);

FuseImage encode_mach111(const Mach111Physical& physical, const std::string& design = {});
Mach111Physical decode_mach111(const FuseImage& image);

}  // namespace machxl
