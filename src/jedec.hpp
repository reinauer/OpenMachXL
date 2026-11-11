#pragma once

#include "fitter_report_data.hpp"
#include "simulation_model.hpp"
#include <optional>
#include <cstdint>
#include <string>
#include <vector>

namespace machxl {
enum class JedecLayout
{
    Linear,
    Mach111,
    MachFamily
};
struct FuseImage
{
    std::string device;
    std::string design;
    unsigned pin_count = 0;
    std::vector<std::uint8_t> fuses;
    std::vector<std::uint8_t> electrical;
    bool security = false;
    std::vector<std::string> notes;
    JedecLayout layout = JedecLayout::Linear;
    std::vector<std::uint8_t> user_code;
    bool dos_compatibility = false;
    // Final physical placement for simulation vectors, indexed by pin
    // number (entry0 unused). This metadata is not a JEDEC record.
    std::vector<std::string> pin_signals;
    std::vector<bool> pin_inverted;
    // Declared buried nodes, indexed by final physical node number.
    std::vector<std::string> node_signals;
    SimulationFitModel simulation_model;
    bool has_test_vectors = false;
    std::vector<std::string> test_vectors;
    std::optional<FitterReportData> fitter_report;
};

// MACHXL includes electrical (E) bits immediately after QF bits in the C
// checksum. There is no byte alignment padding at the QF/E boundary.
std::uint16_t fuse_checksum(const FuseImage& image);
std::string serialize_jedec(const FuseImage& image, const std::string& preamble = {});
}  // namespace machxl
