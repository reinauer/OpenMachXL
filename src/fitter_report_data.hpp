#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace machxl {
struct Design;
struct FuseImage;
// Report observations are captured from accepted or retained failed search
// frames. They are not inferred by fitting the source a second time.
struct FitterReportNode
{
    std::string name;
    int block              = -1;
    int cell               = -1;  // Local cell; modern input registers use a separate IO index.
    int number             = -1;  // One-based device NODE number.
    int pin                = -1;  // Associated pad, if any.
    bool input_register    = false;
    bool implied           = false;
    bool paired            = false;
    char logic             = 'C';  // C, D, T, L.
    bool asynchronous      = false;
    bool negative          = false;
    bool low_power         = false;
    unsigned product_terms = 0;
    unsigned xor_terms     = 0;
    std::optional<unsigned> available_terms;
    std::string clock, preset, reset;
    std::string oe_expression;
    std::vector<std::string> fanins;
    // Fan-ins that are only the node's product-term clock and come from a
    // dedicated clock pin. They are routed like logic but reported as clock
    // use, not as equations where the signal is used.
    std::vector<std::string> clock_pin_fanins;
    // The reset or preset was taken from the global node. Such controls are
    // numbered after those the design gives a register directly.
    bool global_reset = false, global_preset = false;
    // Data/XOR excitation only; excludes OE, clock and set/reset controls.
    std::vector<std::string> data_fanins;
    std::vector<unsigned> fanout_blocks;
    bool fixed = false;
    int oe_key = -1, oe_slot = -1;
    std::string power_annotation;
    std::optional<char> xor_mode;  // Actual selected auxiliary input: P or G.
    bool explicit_output_pair = false;
    std::optional<unsigned> traversal_order;
};
struct FitterReportPin
{
    std::string name;
    // Logical association retained before an output pad has a location.
    std::string output_signal;
    unsigned number = 0;
    int block       = -1;
    bool input = false, output = false;
    int output_node = -1, input_node = -1;
    std::string enable;
    std::vector<unsigned> fanout_blocks, clock_blocks;
    bool fixed           = false;
    bool dedicated_clock = false;  // Retained partition signal-record type9.
    // Fixed clock pin whose only register was removed as unused. The original
    // still reports the pin as a clock input, with no fanout.
    bool retained_clock = false;
    std::optional<unsigned> traversal_order;
};
struct FitterReportRoute
{
    std::string signal;
    int mux        = -1;
    int source     = -1;  // Encoded CSM source from the accepted route.
    int source_pin = -1, source_block = -1, source_cell = -1;
    bool source_input_register = false;
    // Logical record order for failed RPT; source_pin denotes a NODE
    // number here rather than a package pin.
    std::optional<unsigned> logical_id;
};
struct FitterReportClock
{
    std::string signal;
    int slot = -1, pin = -1;
    bool negative = false;
};
struct FitterReportBlock
{
    bool sequential_search = false;  // PRD dot: retained search mode1.
    unsigned fanin = 0, product_terms = 0, io_pins = 0, input_registers = 0;
    unsigned macrocells = 0, clusters = 0;
    std::optional<unsigned> required_macrocells;
    // Original resource summary uses retained partition charges, not steering owners.
    std::optional<unsigned> partition_clusters;
    // Unknown after an incomplete placement must remain absent.
    std::optional<unsigned> unusable, free_macrocells, free_single_pt, free_clusters;
    std::vector<FitterReportRoute> routes;
    std::vector<FitterReportClock> clocks;
    // Physical macrocell-indexed capacity after the final steering state.
    std::vector<unsigned> available_terms;
};
struct FitterReportFailure
{
    std::string signal;
    std::vector<std::vector<std::string>> block_reasons;
    std::vector<std::string> remaining_signals;
    int fixed_block = -1;
};
struct FitterReportCellFailure
{
    std::string signal;
    unsigned block = 0, product_terms = 0;
};
struct FitterReportData
{
    bool complete = false;
    // The corrected coefficient model estimates current; it is not a
    // device measurement. Compatibility mode omits the estimate.
    std::optional<float> estimated_static_icc_milliamps;
    // Actual optimizer output, retained before partitioning for timing and
    // failure reports. This never re-runs optimization or reads a fixture.
    std::string optimized_pla;
    std::shared_ptr<const Design> optimized_design;
    std::vector<FitterReportNode> nodes;
    std::vector<FitterReportPin> pins;
    std::vector<FitterReportBlock> blocks;
    // Global physical group/cell coordinates, retained from placement.
    std::vector<int> group_owner, auxiliary_owner;
    std::vector<bool> auxiliary_data;
    std::optional<FitterReportFailure> partition_failure;
    // Numerator/denominator pairs from completed work, not wall time.
    unsigned partitioned = 0, partition_total = 0;
    unsigned placed = 0, placement_total = 0;
    unsigned routed = 0, routing_total = 0;
    bool successful = false;
    // Failed physical trial, never a programmable fuse image.
    std::shared_ptr<const FuseImage> retained_image;
    std::vector<unsigned> imx_congested_blocks;
    std::vector<unsigned> placement_failed_blocks;
    std::optional<FitterReportCellFailure> cell_placement_failure;
    std::vector<unsigned> route_failed_blocks;
    std::optional<unsigned long long> placement_attempts, routing_attempts;
    // Last legacy physical OE table operation: DEV selector and table count.
    std::optional<std::array<int, 2>> last_oe_constraint;
};
struct Design;
struct FuseImage;
void finalize_fitter_report_data(const Design& design, const FuseImage& image, FitterReportData& data);
}  // namespace machxl
