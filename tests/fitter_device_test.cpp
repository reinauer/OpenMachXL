#include "device_mach111.hpp"

#include <iostream>
#include <stdexcept>

using namespace machxl;

#define CHECK(condition) do { \
    if (!(condition)) throw std::runtime_error("check failed: " #condition); \
} while (false)

template<class F> void rejects(F f) {
    bool rejected = false;
    try { f(); } catch (const std::invalid_argument&) { rejected = true; }
    CHECK(rejected);
}

int main() {
    // Two explicit clock-35 cells in block B produce the non-periodic unused
    // clock sequence recorded in the saved 3COUNT_D JEDEC.
    std::array<int,16> clock_assignments;
    clock_assignments.fill(-1);
    clock_assignments[0] = clock_assignments[3] = 3;
    const std::array<unsigned,16> balanced = {3,0,1,3,2,0,1,2,0,1,2,3,0,1,2,3};
    CHECK(mach111_balance_clocks(clock_assignments) == balanced);
    // Check the physical pin and source mappings against PHY_111.DAT.
    CHECK(mach111_io_pin(0) == 2);
    CHECK(mach111_io_pin(16) == 43);
    CHECK(mach111_io_pin(31) == 24);
    CHECK(mach111_pin_source(35) == 69);
    CHECK(mach111_pin_source(3) == 3);
    CHECK(mach111_feedback_source(19) == 38);
    CHECK(mach111_mux_source(2,6) == 38);
    CHECK(mach111_mux_source(17,1) == 0);
    std::array<bool,32> allocated_cells{};
    std::array<bool,45> allocated_pins{};
    // Mux 12 starts with free dedicated pin 35, but feedback cell 7 in
    // slot 1 wins. With all feedback occupied, the first free pin wins.
    CHECK(mach111_unused_mux_slot(12,allocated_cells,allocated_pins)==1);
    allocated_cells.fill(true);
    CHECK(mach111_unused_mux_slot(12,allocated_cells,allocated_pins)==0);
    allocated_pins[35]=true;
    CHECK(mach111_unused_mux_slot(12,allocated_cells,allocated_pins)==2);
    allocated_pins.fill(true);
    rejects([&] { mach111_unused_mux_slot(12,allocated_cells,allocated_pins); });
    for (unsigned source = 0; source < 70; ++source) {
        auto choices = mach111_mux_choices(source);
        CHECK(!choices.empty());
        for (auto choice : choices)
            CHECK(mach111_mux_source(choice.mux,choice.slot) == source);
    }
    rejects([] { mach111_pin_source(22); });
    rejects([] { mach111_mux_source(26,0); });
    rejects([] { mach111_feedback_source(32); });

    // Addresses observed in 3COUNT_D: A0 data rows start at 234, B3 at
    // 4720, and A0's normal registered clock-35 word is 100011011.
    CHECK(mach111_product_address(0,3,0) == 234);
    CHECK(mach111_product_address(1,15,0) == 4720);
    Mach111Physical p;
    p.blocks[0].mux_slots[8] = 4;
    p.blocks[0].products[2].fill(Mach111Physical::Literal::DontCare);
    p.blocks[0].products[2][8] = Mach111Physical::Literal::Positive;
    p.blocks[0].macrocell_words[0] = 0x1b1;
    p.low_power.fill(true);
    p.low_power[0] = false;
    auto image = encode_mach111(p,"probe");
    CHECK(image.fuses[43] == 1); // Mux 8, slot bit 2.
    CHECK(image.fuses[198] == 0 && image.fuses[199] == 1); // row 2, mux 8.
    const unsigned expected[] = {1,0,0,0,1,1,0,1,1};
    for (unsigned i = 0; i < 9; ++i) CHECK(image.fuses[3718+i] == expected[i]);
    CHECK(image.electrical[31] == 0 && image.electrical[0] == 1);

    // Exercise every physical address, including non-contiguous CSM planes,
    // both blocks and the reversed power bit order. A one-bit input must
    // round-trip to the same single address without aliases or missing bits.
    image.fuses.assign(7724,0);
    image.electrical.assign(32,0);
    for (unsigned n = 0; n < 7756; ++n) {
        auto& bits = n < 7724 ? image.fuses : image.electrical;
        const unsigned index = n < 7724 ? n : n-7724;
        bits[index] = 1;
        auto encoded = encode_mach111(decode_mach111(image));
        CHECK(encoded.fuses == image.fuses);
        CHECK(encoded.electrical == image.electrical);
        bits[index] = 0;
    }
    p.blocks[0].mux_slots[0] = 8;
    rejects([&] { encode_mach111(p); });
    p.blocks[0].mux_slots[0] = 0;
    p.blocks[1].macrocell_words[4] = 512;
    rejects([&] { encode_mach111(p); });
    image.fuses[123] = 2;
    rejects([&] { decode_mach111(image); });
}
