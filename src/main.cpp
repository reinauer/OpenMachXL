#include "fitter.hpp"
#include "machxl_version.hpp"
#include "logic.hpp"
#include "fitter_family.hpp"
#include "fitter_metadata.hpp"
#include "fitter_modern.hpp"
#include "fitter_place_report.hpp"
#include "fitter_reports.hpp"
#include "frontend.hpp"
#include "signature.hpp"
#include "simulation_placement.hpp"
#include "simulation_reports.hpp"
#include "simulator.hpp"
#include "synthesis.hpp"
#include "timing_report.hpp"
#include <cctype>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>

namespace {
std::string read_file(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw machxl::LogicError("Cannot open file: " + path);
    return {(std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>()};
}
std::string report_timestamp(bool weekday = false)
{
    const auto now    = std::time(nullptr);
    const auto* local = std::localtime(&now);
    if (!local)
        throw machxl::LogicError("Cannot obtain report timestamp");
    std::ostringstream stamp;
    stamp.imbue(std::locale::classic());
    stamp << std::put_time(local, weekday ? "%a %b %d %H:%M:%S %Y" : "%b %d %H:%M:%S %Y");
    return stamp.str();
}
void reject_report_input_overwrites(const std::string& destination, const std::string& source,
                                    const std::vector<std::string>& extensions,
                                    const std::vector<std::filesystem::path>& inputs)
{
    if (destination.empty())
        return;
    const auto stem = std::filesystem::path(source).stem().string();
    for (const auto& extension : extensions) {
        const auto output   = std::filesystem::path(destination) / (stem + extension);
        const auto resolved = std::filesystem::weakly_canonical(output);
        for (const auto& input : inputs) {
            if (input.empty())
                continue;
            if (resolved == std::filesystem::weakly_canonical(input) ||
                (std::filesystem::exists(output) && std::filesystem::exists(input) &&
                 std::filesystem::equivalent(output, input)))
                throw machxl::LogicError("Report output would overwrite input: " + output.string());
        }
    }
}
void clear_previous_reports(const std::string& destination, const std::string& source,
                            const std::vector<std::string>& extensions)
{
    if (destination.empty())
        return;
    const std::filesystem::path directory(destination);
    std::filesystem::create_directories(directory);
    const auto stem = std::filesystem::path(source).stem().string();
    // Only the requested design's known outputs are replaced. Otherwise a
    // rejected rerun can leave an old successful placement beside a new TAL.
    for (const auto& extension : extensions) {
        const auto path = directory / (stem + extension);
        if (std::filesystem::is_directory(path))
            throw machxl::LogicError("Report output is a directory: " + path.string());
        std::filesystem::remove(path);
    }
}
void write_reports(const std::string& destination, const std::string& source,
                   const std::vector<std::pair<std::string, const std::string*>>& reports)
{
    const std::filesystem::path directory(destination);
    std::filesystem::create_directories(directory);
    const auto stem = std::filesystem::path(source).stem().string();
    for (const auto& report : reports) {
        if (report.second->empty())
            continue;
        const auto path = directory / (stem + report.first);
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << *report.second;
        file.close();
        if (!file)
            throw machxl::LogicError("Cannot write report: " + path.string());
    }
}
std::filesystem::path find_sibling_file(const std::string& source, const std::string& extension)
{
    auto path = std::filesystem::path(source);
    path.replace_extension(extension);
    if (!std::filesystem::exists(path)) {
        auto lower = extension;
        for (auto& c : lower)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        path.replace_extension(lower);
    }
    return path;
}
}  // namespace

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--version") {
        std::cout << "OpenMachXL " MACHXL_VERSION " (" MACHXL_RELEASE_DATE ")\n";
        return 0;
    }
    std::string source_path, settings_path, placement_path, reports_path, fitter_reports_path;
    bool compatibility = false, simulate = false;
    try {
        enum class Output
        {
            None,
            Pla,
            Jedec
        };
        Output mode = Output::None;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--emit-pla" && mode == Output::None)
                mode = Output::Pla;
            else if (arg == "--emit-jedec" && mode == Output::None)
                mode = Output::Jedec;
            else if (arg == "--simulate" && !simulate)
                simulate = true;
            else if (arg == "--compatibility" && !compatibility)
                compatibility = true;
            else if (arg == "--settings" && settings_path.empty() && i + 1 < argc)
                settings_path = argv[++i];
            else if (arg == "--placement" && placement_path.empty() && i + 1 < argc)
                placement_path = argv[++i];
            else if (arg == "--simulation-reports" && reports_path.empty() && i + 1 < argc)
                reports_path = argv[++i];
            else if (arg == "--fitter-reports" && fitter_reports_path.empty() && i + 1 < argc)
                fitter_reports_path = argv[++i];
            else if (!arg.empty() && arg.front() != '-' && source_path.empty())
                source_path = arg;
            else
                throw machxl::LogicError("Unknown or repeated argument: " + arg);
        }
        if (mode == Output::None || source_path.empty()) {
            std::cerr << "Usage: machxl --emit-pla input.pds [--settings input.mxl] [--compatibility]\n"
                      << "       machxl --emit-jedec input.pds [--settings input.mxl] [--compatibility] [--simulate] "
                         "[--placement input.plc]\n"
                      << "--simulate runs source test commands and adds JEDEC vectors.\n"
                      << "--simulation-reports DIR writes DOS-format HST/TRF files; requires --simulate.\n"
                      << "--fitter-reports DIR writes RPT/PRD/TAL files; requires --emit-jedec.\n"
                      << "--compatibility reproduces confirmed original DOS defects.\n"
                      << "JEDEC: unsupported device resources fail explicitly.\n";
            return 2;
        }
        machxl::CompilerSettings settings;
        if (!settings_path.empty())
            settings.parse(read_file(settings_path), compatibility);
        if (simulate && mode != Output::Jedec)
            throw machxl::LogicError("--simulate requires --emit-jedec");
        if (!reports_path.empty() && !simulate)
            throw machxl::LogicError("--simulation-reports requires --simulate");
        if (!fitter_reports_path.empty() && mode != Output::Jedec)
            throw machxl::LogicError("--fitter-reports requires --emit-jedec");
        if (simulate && !compatibility && settings.get("SIM_PLACE") != "Last successful placement" &&
            settings.get("SIM_PLACE") != "Design file" && settings.get("SIM_PLACE") != "Saved placement")
            throw machxl::LogicError("Unknown simulation placement: " + settings.get("SIM_PLACE"));
        std::string auxiliary_simulation;
        const bool use_auxiliary = simulate && settings.yes_no("SIMF_OPT");
        if (use_auxiliary) {
            auto path = std::filesystem::path(source_path);
            path.replace_extension(".SIM");
            if (!std::filesystem::exists(path))
                path.replace_extension(".sim");
            auxiliary_simulation = read_file(path.string());
            auto table           = std::filesystem::path(source_path);
            table.replace_extension(".TBL");
            if (!std::filesystem::exists(table))
                table.replace_extension(".tbl");
            if (std::filesystem::exists(table))
                auxiliary_simulation = read_file(table.string()) + "\n" + auxiliary_simulation;
        }
        auto options                       = machxl::synthesis_options(settings);
        options.frontend.dos_compatibility = compatibility;
        if (!placement_path.empty() && mode != Output::Jedec)
            throw machxl::LogicError("--placement requires --emit-jedec");
        // MACHSP 2.1 omits the filename after +P for FITR_PLACE. Explicit
        // --placement exposes the working standalone MACHFITR operation.
        if (mode == Output::Jedec && placement_path.empty() && !compatibility &&
            settings.get("FITR_PLACE") != "Design file") {
            const auto extension = settings.get("FITR_PLACE") == "Saved placement" ? ".BLC" : ".PLC";
            auto path            = std::filesystem::path(source_path);
            path.replace_extension(extension);
            if (!std::filesystem::exists(path)) {
                std::string lower = extension;
                for (auto& c : lower)
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                path.replace_extension(lower);
            }
            placement_path = path.string();
        }
        const auto saved_placement         = placement_path.empty() ? std::string{} : read_file(placement_path);
        auto source_design                 = machxl::parse_design(read_file(source_path), options.frontend,
                                                                  use_auxiliary ? &auxiliary_simulation : nullptr);
        options                            = machxl::synthesis_options(settings, source_design.device);
        options.frontend.dos_compatibility = compatibility;
        for (const auto& diagnostic : source_design.diagnostics)
            std::cerr << source_path << ": warning: " << diagnostic << '\n';
        auto design       = machxl::transform_synthesis(source_design, options);
        const bool modern = design.device == "MACH355" || design.device == "MACH435" || design.device == "MACH445" ||
                            design.device == "MACH446" || design.device == "MACH465";
        // Build the complete result before writing anything on a failed compilation.
        std::string output, history_report, trace_report;
        if (mode == Output::Pla)
            output =
                machxl::emit_pla(design, std::filesystem::path(source_path).filename().string(), options.optimizer);
        else {
            machxl::FitterOptions fitting{settings.yes_no("BALANCED_PAR"), settings.yes_no("SPREAD_PL")};
            if (!fitter_reports_path.empty())
                fitting.report = std::make_shared<machxl::FitterReportData>();
            fitting.warnings                 = std::make_shared<std::vector<std::string>>();
            const auto print_fitter_warnings = [&] {
                // The original prints its messages phase by phase: the global
                // node, the device note, the signal walk, unreferenced
                // pins, unreferenced nodes and last the clock rules.
                const auto phase = [](const std::string& message) {
                    const auto code = message.substr(message.find(": ") + 2, 5);
                    return code == "z5037" ? 0 : code == "z5109" ? 1
                           : code == "z5029" || code == "z5064" || code == "z5065" || code == "z5068" ||
                                     code == "z5070" || code == "z5128"
                               ? 2
                           : code == "z5073"                    ? 3
                           : code == "z5071"                    ? 4
                           : code == "z5121" || code == "z5056" ? 6
                                                                : 5;
                };
                std::stable_sort(fitting.warnings->begin(), fitting.warnings->end(),
                                 [&](const auto& a, const auto& b) { return phase(a) < phase(b); });
                for (const auto& warning : *fitting.warnings)
                    std::cerr << source_path << ": " << warning << '\n';
                fitting.warnings->clear();
            };
            fitting.source_route_limit             = settings.yes_no("ROUTE_PER_PL") ? 2 : 4096;
            fitting.iterate_partition              = settings.yes_no("ITERATE_FITR");
            fitting.runtime_quarters               = settings.integer("M3XX_RUNTIME");
            fitting.automatic_input_pairing        = settings.fitter_yes_no("P_N");
            fitting.automatic_legacy_input_pairing = settings.fitter_yes_no("PAIR");
            fitting.use_saved_placement            = !placement_path.empty();
            fitting.saved_placement                = saved_placement;
            fitting.preplacement_policy            = settings.get("M3XX_PREPLACE");
            fitting.ground_implicit_controls       = !settings.yes_no("M3XX_SET_RST");
            fitting.zero_hold_time                 = settings.yes_no("ZERO_HOLD_TM");
            fitting.product_term_clocks            = settings.yes_no("PTERM_CLK");
            fitting.reduce_global_clocks = settings.yes_no("SIZE_REDUCE") ? settings.integer("SIZE_REDUCE_N") : 0;
            std::vector<std::filesystem::path> report_inputs{source_path, settings_path, placement_path};
            if (use_auxiliary) {
                report_inputs.push_back(find_sibling_file(source_path, ".SIM"));
                report_inputs.push_back(find_sibling_file(source_path, ".TBL"));
            }
            if (simulate && settings.get("SIM_PLACE") == "Saved placement")
                report_inputs.push_back(find_sibling_file(source_path, ".BLC"));
            reject_report_input_overwrites(fitter_reports_path, source_path, {".RPT", ".PRD", ".TAL"}, report_inputs);
            reject_report_input_overwrites(reports_path, source_path, {".HST", ".TRF"}, report_inputs);
            clear_previous_reports(fitter_reports_path, source_path, {".RPT", ".PRD", ".TAL"});
            clear_previous_reports(reports_path, source_path, {".HST", ".TRF"});
            const auto start_time  = std::chrono::steady_clock::now();
            const auto start_stamp = fitter_reports_path.empty() ? std::string{} : report_timestamp(true);
            const auto filename    = std::filesystem::path(source_path).filename().string();
            machxl::FuseImage image;
            try {
                image = design.device == "MACH111" ? machxl::fit_mach111_fixed(design, options.optimizer, fitting)
                        : modern                   ? machxl::fit_mach_modern(design, options.optimizer, fitting)
                                                   : machxl::fit_mach_family_fixed(design, options.optimizer, fitting);
            } catch (...) {
                print_fitter_warnings();
                // Preserve the actual retained trial while keeping failure status
                // and withholding all programming output.
                if (fitting.report && !fitting.report->optimized_pla.empty()) {
                    const auto& captured = *fitting.report;
                    const auto timing =
                        machxl::render_timing_report(captured.optimized_design ? *captured.optimized_design : design,
                                                     captured.optimized_pla, nullptr, filename);
                    std::string resource, placement;
                    std::vector<std::pair<std::string, const std::string*>> reports{{".TAL", &timing}};
                    if (captured.complete) {
                        auto display = filename;
                        for (auto& c : display)
                            c = char(std::tolower(static_cast<unsigned char>(c)));
                        auto resource_filename = display;
                        if (!resource_filename.empty())
                            resource_filename.front() =
                                char(std::toupper(static_cast<unsigned char>(resource_filename.front())));
                        const auto end_stamp = report_timestamp(true);
                        resource = machxl::render_fitter_report(design.device, captured, settings, resource_filename,
                                                                end_stamp.substr(4));
                        reports.push_back({".RPT", &resource});
                        if (captured.retained_image) {
                            const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                                                     std::chrono::steady_clock::now() - start_time)
                                                     .count();
                            std::ostringstream elapsed;
                            elapsed << std::setfill('0') << std::setw(2) << seconds / 3600 << ':' << std::setw(2)
                                    << (seconds / 60) % 60 << ':' << std::setw(2) << seconds % 60;
                            placement =
                                machxl::render_fitter_place_report(*captured.retained_image, captured, settings,
                                                                   display, start_stamp, end_stamp, elapsed.str());
                            reports.push_back({".PRD", &placement});
                        }
                    }
                    write_reports(fitter_reports_path, source_path, reports);
                }
                throw;
            }
            print_fitter_warnings();
            image.dos_compatibility = compatibility;
            machxl::apply_signature(image, design.signature);
            if (!fitter_reports_path.empty()) {
                if (!image.fitter_report)
                    throw machxl::LogicError("Missing fitting report observations");
                const auto end_stamp = report_timestamp(true);
                const auto seconds =
                    std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time)
                        .count();
                std::ostringstream elapsed;
                elapsed << std::setfill('0') << std::setw(2) << seconds / 3600 << ':' << std::setw(2)
                        << (seconds / 60) % 60 << ':' << std::setw(2) << seconds % 60;
                const auto& captured    = *image.fitter_report;
                auto placement_filename = filename;
                for (auto& c : placement_filename)
                    c = char(std::tolower(static_cast<unsigned char>(c)));
                auto resource_filename = placement_filename;
                if (!resource_filename.empty())
                    resource_filename.front() =
                        char(std::toupper(static_cast<unsigned char>(resource_filename.front())));
                const auto resource = machxl::render_fitter_report(design.device, captured, settings, resource_filename,
                                                                   end_stamp.substr(4));
                const auto placement = machxl::render_fitter_place_report(image, captured, settings, placement_filename,
                                                                          start_stamp, end_stamp, elapsed.str());
                const auto timing =
                    machxl::render_timing_report(captured.optimized_design ? *captured.optimized_design : design,
                                                 captured.optimized_pla, &captured, filename);
                write_reports(fitter_reports_path, source_path,
                              {{".RPT", &resource}, {".PRD", &placement}, {".TAL", &timing}});
            }
            if (simulate) {
                machxl::SimulationOptions simulation_options;
                simulation_options.collect_reports = !reports_path.empty();
                machxl::SimulationPinMap pin_map{image.pin_signals, image.pin_inverted};
                pin_map.nodes          = image.node_signals;
                auto simulation_design = source_design;
                machxl::SimulationFitModel saved_simulation_model;
                bool generate_vectors                       = true;
                simulation_options.placement                = &pin_map;
                simulation_options.fitted_model             = &image.simulation_model;
                simulation_options.require_fitted_registers = true;
                if (settings.get("SIM_PLACE") != "Last successful placement") {
                    simulation_options.fitted_model             = nullptr;
                    simulation_options.require_fitted_registers = false;
                    const bool source_as_placement = compatibility && settings.get("SIM_PLACE") != "Saved placement" &&
                                                     settings.get("SIM_PLACE") != "Design file";
                    const bool saved = settings.get("SIM_PLACE") == "Saved placement" || source_as_placement;
                    const auto text =
                        saved ? read_file(source_as_placement ? source_path
                                                              : find_sibling_file(source_path, ".BLC").string())
                              : std::string{};
                    auto placement = machxl::prepare_simulation_placement(simulation_design, saved ? &text : nullptr);
                    pin_map        = std::move(placement.pins);
                    simulation_options.register_power_up = std::move(placement.register_power_up);
                    saved_simulation_model               = std::move(placement.model);
                    if (saved)
                        simulation_options.fitted_model = &saved_simulation_model;
                    generate_vectors = !placement.floating_pins;
                    if (!generate_vectors)
                        std::cerr << source_path
                                  << ": warning: D2200: floating simulation pins disable JEDEC test vectors\n";
                    else if (modern && !saved) {
                        generate_vectors = false;
                        std::cerr << source_path
                                  << ": warning: D2290: this device requires a placement file for JEDEC test vectors\n";
                    }
                }
                simulation_options.legacy_set_reset = settings.yes_no("EQN_22V10");
                simulation_options.max_vectors      = 30000;
                auto result                         = machxl::simulate_design(simulation_design, simulation_options);
                if (!reports_path.empty()) {
                    history_report = machxl::render_simulation_history(simulation_design, result, pin_map);
                    trace_report   = machxl::render_simulation_trace(simulation_design, result, pin_map);
                }
                image.has_test_vectors = generate_vectors;
                if (generate_vectors)
                    image.test_vectors = machxl::encode_simulation_vectors(result, pin_map);
                for (const auto& mismatch : result.mismatches)
                    std::cerr << source_path << ": warning: simulation CHECK mismatch at vector " << mismatch.vector
                              << " for " << mismatch.signal << '\n';
                if (result.vectors_disabled_by_preload)
                    std::cerr << source_path << ": simulation PRELOAD disabled subsequent JEDEC vectors\n";
            }
            output = machxl::serialize_jedec(image);
        }
        if (!reports_path.empty())
            write_reports(reports_path, source_path, {{".HST", &history_report}, {".TRF", &trace_report}});
        std::cout << output;
        return 0;
    } catch (const std::runtime_error& error) {
        // LogicError and the placers' failures: the input cannot be compiled.
        std::cerr << (source_path.empty() ? "machxl" : source_path) << ": " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        // Anything else is a violated internal assumption, not a statement
        // about the design; a bare library text such as "vector" says nothing.
        std::cerr << (source_path.empty() ? "machxl" : source_path) << ": internal error: " << error.what() << '\n';
        return 1;
    }
}
