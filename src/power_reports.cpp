#include "power_reports.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace machxl {
namespace {
constexpr unsigned blocks = 8, cells = 16, rows = 90, muxes = 33;
void validate_power_geometry(const MachDevicePhysical& physical)
{
    if (physical.blocks.size() != blocks || physical.low_power.size() != blocks * cells)
        throw std::invalid_argument("Invalid MACH446 power-report geometry");
    for (const auto& block : physical.blocks) {
        if (block.products.size() != rows || block.macrocell_words.size() != cells)
            throw std::invalid_argument("Invalid MACH446 power-report block");
        for (const auto& row : block.products) {
            if (row.size() != muxes || std::any_of(row.begin(), row.end(), [](auto v) { return v > 3; }))
                throw std::invalid_argument("Invalid MACH446 power-report product");
        }
    }
}
unsigned classify_product_row(const std::vector<std::uint8_t>& row)
{
    if (std::all_of(row.begin(), row.end(), [](auto value) { return value == 0; }))
        return 1;
    if (std::all_of(row.begin(), row.end(), [](auto value) { return value == 3; }))
        return 0;
    return 2;
}
}  // namespace

std::string mach446_power_annotation(unsigned cell, Mach446PowerRequest request, const MachDevicePhysical& physical,
                                     const std::vector<unsigned>& steering_owners)
{
    validate_power_geometry(physical);
    if (cell >= blocks * cells || steering_owners.size() != blocks * cells)
        throw std::invalid_argument("Invalid MACH446 power-report cell");
    if (request.high)
        return request.low ? "#H" : "H";
    const unsigned block     = cell / cells;
    const bool asynchronous  = !(physical.blocks[block].macrocell_words[cell % cells] & 512);
    const bool high_controls = asynchronous && !physical.low_power[cell];
    bool high_data           = false;
    for (unsigned group = block * cells; group < (block + 1) * cells; ++group) {
        if (steering_owners[group] >= blocks * cells)
            throw std::invalid_argument("Invalid MACH446 power steering owner");
        if (steering_owners[group] == cell && !physical.low_power[group])
            high_data = true;
    }
    return high_data ? (high_controls ? "LHdc" : "LHd") : (high_controls ? "LHc" : "L");
}

float corrected_mach446_icc(const MachDevicePhysical& physical)
{
    validate_power_geometry(physical);
    // Original coefficient triples: disabled, true, ordinary product.
    constexpr std::array<float, 3> shared{{0.12f, 0.235f, 0.18f}};
    constexpr std::array<float, 3> normal{{0.195f, 0.31f, 0.275f}};
    constexpr std::array<float, 3> low{{0.04f, 0.11f, 0.09f}};
    float total = 15.0f;
    for (unsigned block = 0; block < blocks; ++block) {
        const auto& products = physical.blocks[block].products;
        const auto add       = [&](unsigned row, const std::array<float, 3>& coefficients) {
            const auto classification = classify_product_row(products.at(row));
            const auto index          = classification == 1 ? 0 : classification == 0 ? 1 : 2;
            // MACHXL stores binary32 after every addition. The volatile store
            // also prevents an implementation from retaining excess precision.
            volatile float rounded = total + coefficients[index];
            total                  = rounded;
        };
        add(88, shared);
        add(89, shared);
        for (unsigned cell = 0; cell < cells; ++cell) {
            // The eight OE rows are indexed by pad, not by sixteen logic
            // cells. Keep the original loop order for the bounded accesses.
            if (cell < 8)
                add(cell * 11, shared);
            const auto& coefficients = physical.low_power[block * cells + cell] ? low : normal;
            const auto first         = cell * 5 + cell / 2 + 1;
            for (unsigned term = 0; term < 5; ++term)
                add(first + term, coefficients);
        }
    }
    return total;
}
}  // namespace machxl
