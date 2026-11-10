#include "settings.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace machxl {
namespace {
std::string trim(const std::string& text)
{
    auto first = text.find_first_not_of(" \t\r\n");
    return first == std::string::npos ? "" : text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}
std::string upper(std::string text)
{
    for (char& c : text)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return text;
}

struct SerializedSetting
{
    const char* key;
    unsigned buffer, offset, width;
};
const SerializedSetting serialized_settings[] = {
    // Compatibility storage layout: shared buffers, byte offsets and copy widths.
    {"VERSION", 21, 0, 0},       {"EXE_DIR", 0, 0, 0},        {"WORK_DIR", 1, 0, 0},
    {"ORCAD_EXE", 2, 0, 0},      {"MIN_DISK", 3, 0, 3},       {"EDIT_FILE", 3, 3, 50},
    {"MOUSE_TYPE", 4, 0, 1},     {"MOUSE_SPEED", 4, 1, 1},    {"RAM_SIZE", 4, 2, 4},
    {"EQN_1_BPP", 5, 0, 1},      {"EQN_1_EXP", 5, 1, 1},      {"EQN_1_MIN", 5, 2, 1},
    {"EQN_1_PALXPT", 5, 3, 1},   {"EQN_1_PLSASM", 5, 4, 1},   {"EQN_1_PLDSIM", 5, 5, 1},
    {"EQN_1_TREPL2", 5, 6, 1},   {"DESIGN_FILE", 6, 0, 0},    {"EDITOR_NAME", 7, 0, 0},
    {"RUN_LOG", 10, 0, 15},      {"COMP_OPT", 11, 0, 1},      {"SIM_OPT", 11, 1, 1},
    {"DESIGN_OPT", 11, 2, 1},    {"BELL_OPT", 11, 3, 1},      {"NETLIST_OPT", 11, 4, 1},
    {"SIMF_OPT", 11, 5, 1},      {"SILENT_OPT", 11, 6, 1},    {"RUN_MODE", 12, 0, 0},
    {"FILE_FORMAT", 13, 0, 0},   {"P_N", 14, 0, 1},           {"GATE_SPLIT", 14, 1, 1},
    {"THRESHOLD", 14, 2, 3},     {"BUCKET_SIZE", 11, 8, 2},   {"D_T_REG_OPTM", 9, 6, 31},
    {"M_POLAR", 14, 33, 28},     {"BPP_SIGNAL", 17, 26, 11},  {"FAST_MIN", 18, 2, 1},
    {"PTERM_CLK", 15, 13, 1},    {"FITR_PLACE", 16, 0, 26},   {"FITR_DATA", 16, 26, 26},
    {"SIM_PLACE", 17, 0, 26},    {"EQN_22V10", 18, 3, 1},     {"M3XX_MSG", 19, 0, 1},
    {"M3XX_SET_RST", 19, 1, 1},  {"M3XX_RUNTIME", 19, 2, 4},  {"M3XX_PREPLACE", 19, 11, 30},
    {"ZERO_HOLD_TM", 15, 0, 1},  {"SIZE_REDUCE", 18, 7, 1},   {"SIZE_REDUCE_N", 18, 8, 1},
    {"DOWN_CONFIG", 10, 15, 50}, {"ROUTE_PER_PL", 15, 14, 1}, {"JTAG_STATUS", 15, 15, 1},
    {"JTAG_ERROR", 15, 16, 1},   {"JTAG_ERASE", 15, 17, 1},   {"ITERATE_FITR", 15, 18, 1},
    {"SPREAD_PL", 15, 20, 1},    {"JTAG_PORT", 15, 21, 5},    {"SUPPORT_NT", 2, 1, 1},
    {"THRESHOLD_12", 22, 0, 3},  {"THRESHOLD_2X", 22, 3, 3},  {"BUCKET_SIZE0", 22, 6, 3},
    {"BUCKET_SIZE1", 22, 9, 3},  {"PAIR", 15, 19, 1},         {"RPT_LEVEL", 20, 0, 9},
    {"FLOAT_ALL", 20, 9, 1},     {"RE_FIT", 20, 10, 1},       {"UNUSED_IO", 20, 11, 1},
    {"UNUSED_IO_L", 20, 12, 1},  {"BALANCED_PAR", 20, 13, 1},
};

std::map<std::string, std::string> parse_compatibility_settings(const std::string& source)
{
    // Compatibility parsing starts with zeroed project buffers; absent
    // records do not inherit defaults. Read at most 109 bytes per record.
    std::array<std::array<char, 70>, 23> storage{};
    std::string text;
    for (std::size_t i = 0; i < source.size() && source[i] != '\x1a'; ++i) {
        if (source[i] == '\r' && i + 1 < source.size() && source[i + 1] == '\n')
            continue;
        text += source[i];
    }
    for (std::size_t start = 0; start < text.size();) {
        const auto newline = text.find('\n', start);
        const auto end     = std::min(start + 109, newline == std::string::npos ? text.size() : newline + 1);
        const auto line    = text.substr(start, end - start);
        start              = end;
        const auto folded  = upper(line);
        for (const auto& field : serialized_settings) {
            const auto key = "[" + std::string(field.key) + "]";
            if (folded.compare(0, key.size(), key) != 0)
                continue;
            auto& buffer = storage[field.buffer];
            auto value =
                line.size() > key.size() && line[key.size()] == '=' ? line.substr(key.size() + 1) : std::string{};
            value.resize(std::min(value.size(), value.find('\0')));
            if (value.empty()) {
                buffer[field.offset] = 0;
                continue;
            }
            const auto width = field.width ? field.width : unsigned(value.size() + 1);
            for (unsigned i = 0; i < width && field.offset + i < buffer.size(); ++i)
                buffer[field.offset + i] = i < value.size() ? value[i] : 0;
            if (field.width != 1)
                for (unsigned i = field.offset; i < buffer.size() && buffer[i]; ++i)
                    if (buffer[i] == '\n') {
                        buffer[i] = 0;
                        break;
                    }
        }
    }
    std::map<std::string, std::string> result;
    for (const auto& field : serialized_settings) {
        const auto& buffer = storage[field.buffer];
        auto end           = field.offset;
        const auto maximum = field.width == 1 ? field.offset + 1 : unsigned(buffer.size());
        while (end < maximum && buffer[end])
            ++end;
        result[field.key] = std::string(buffer.data() + field.offset, end - field.offset);
    }
    const auto first = [&](const char* key) { return result[key].empty() ? char(0) : result[key].front(); };
    const auto has   = [&](const char* key, const char* value) { return result[key].find(value) != std::string::npos; };
    const auto polarity     = first("M_POLAR");
    result["M_POLAR"]       = polarity == 'H'   ? "High, active high"
                              : polarity == 'L' ? "Low, active low"
                              : polarity == 'B' ? "Best for device"
                                                : "As specified in design file";
    result["D_T_REG_OPTM"]  = has("D_T_REG_OPTM", "RS to B")     ? "D,T As specified. JK,RS to Best"
                              : has("D_T_REG_OPTM", "RS to D")   ? "D,T As specified. JK,RS to D"
                              : has("D_T_REG_OPTM", "RS to T")   ? "D,T As specified. JK,RS to T"
                              : has("D_T_REG_OPTM", "D-type")    ? "Change all to D-type"
                              : has("D_T_REG_OPTM", "T-type")    ? "Change all to T-type"
                              : has("D_T_REG_OPTM", "Best type") ? "Best type for device"
                                                                 : "D,T As specified. JK,RS to D";
    result["M3XX_PREPLACE"] = has("M3XX_PREPLACE", "Input")         ? "Float Non-Input Nodes, Groups"
                              : has("M3XX_PREPLACE", "Float Nodes") ? "Float Nodes, Groups"
                              : has("M3XX_PREPLACE", "Groups")      ? "Float Pins, Nodes, Groups"
                              : has("M3XX_PREPLACE", "Nodes")       ? "Float Pins, Nodes"
                                                                    : "No Change";
    result["BPP_SIGNAL"]    = first("BPP_SIGNAL") == 'O' ? "Off" : "Don't care";
    for (const char* key : {"FITR_PLACE", "SIM_PLACE", "FITR_DATA"}) {
        const auto initial = first(key);
        if (initial == 'D' || (initial == '0' && std::string(key) == "SIM_PLACE"))
            result[key] = "Design file";
        else if (initial == 'L')
            result[key] = "Last successful placement";
        else if (initial == 'S')
            result[key] = "Saved placement";
    }
    return result;
}
}  // namespace

CompilerSettings::CompilerSettings()
    : values_{{"VERSION", "MACHXL 2.0"},
              {"P_N", "Y"},
              {"GATE_SPLIT", "N"},
              {"THRESHOLD", "20"},
              {"BUCKET_SIZE", "20"},
              {"D_T_REG_OPTM", "D,T As specified. JK,RS to D"},
              {"M_POLAR", "As specified in design file"},
              {"BPP_SIGNAL", "Off"},
              {"FAST_MIN", "N"},
              {"PTERM_CLK", "N"},
              {"FITR_PLACE", "Design file"},
              {"FITR_DATA", "Last successful placement"},
              {"SIMF_OPT", "N"},
              {"SIM_OPT", "N"},
              {"SIM_PLACE", "Last successful placement"},
              {"EQN_22V10", "Y"},
              {"M3XX_MSG", "N"},
              {"M3XX_SET_RST", "Y"},
              {"M3XX_RUNTIME", "0"},
              {"M3XX_PREPLACE", "No Change"},
              {"ZERO_HOLD_TM", "N"},
              {"SIZE_REDUCE", "N"},
              {"SIZE_REDUCE_N", "1"},
              {"DOWN_CONFIG", "< None >"},
              {"ROUTE_PER_PL", "N"},
              {"RPT_LEVEL", "Brief"},
              {"FLOAT_ALL", "N"},
              {"RE_FIT", "N"},
              {"UNUSED_IO", "N"},
              {"UNUSED_IO_L", "H"},
              {"ITERATE_FITR", "Y"},
              {"THRESHOLD_12", "12"},
              {"THRESHOLD_2X", "16"},
              {"BUCKET_SIZE0", "12"},
              {"BUCKET_SIZE1", "16"},
              {"PAIR", "Y"},
              {"BALANCED_PAR", "Y"},
              {"SPREAD_PL", "Y"}}
{
}

const std::string& CompilerSettings::get(const std::string& key) const
{
    auto found = values_.find(upper(key));
    if (found == values_.end())
        throw std::invalid_argument("Missing setting: " + key);
    return found->second;
}

bool CompilerSettings::yes_no(const std::string& key) const
{
    if (dos_compatibility_) {
        const auto& value = get(key);
        const char first  = value.empty() ? 0 : value.front();
        const auto name   = upper(key);
        return name == "PTERM_CLK" || name == "M3XX_SET_RST" ? first != 'N' : first == 'Y';
    }
    auto value = upper(get(key));
    if (value == "Y")
        return true;
    if (value == "N")
        return false;
    throw std::invalid_argument("Expected Y/N for setting " + key + ": " + value);
}

bool CompilerSettings::fitter_yes_no(const std::string& key) const
{
    const auto name = upper(key);
    if (dos_compatibility_ && (name == "P_N" || name == "PAIR")) {
        const auto& value = get(key);
        return value.empty() || value.front() != 'N';
    }
    return yes_no(key);
}

unsigned CompilerSettings::integer(const std::string& key) const
{
    const auto& text = get(key);
    if (dos_compatibility_) {
        const bool gate  = upper(key).find("THRESHOLD") == 0 || upper(key).find("BUCKET_SIZE") == 0;
        const auto token = trim(text);
        if (gate) {
            if (token.empty() || token.find_first_not_of("0123456789") != std::string::npos)
                throw std::invalid_argument("L108: gate splitting parameter is not a number: " + text);
            unsigned value = 0;
            for (char c : token)
                value = (value * 10 + unsigned(c - '0')) & 0xffff;
            return value;
        }
        // Fitter numbers are embedded after '=' in one command token.
        // Leading or internal whitespace leaves an extra CLI argument.
        if (!token.empty() && (std::isspace(static_cast<unsigned char>(text.front())) ||
                               token.find_first_of(" \t\r\n") != std::string::npos))
            throw std::invalid_argument("z5008: extra argument in fitter setting " + key);
        std::size_t i = 0;
        while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])))
            ++i;
        bool negative = false;
        if (i < text.size() && (text[i] == '+' || text[i] == '-'))
            negative = text[i++] == '-';
        unsigned result = 0;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9')
            result = (result * 10 + unsigned(text[i++] - '0')) & 0xffff;
        return negative ? (-result) & 0xffff : result;
    }
    if (text.empty())
        throw std::invalid_argument("Empty integer setting: " + key);
    unsigned result = 0;
    for (char c : text) {
        if (c < '0' || c > '9' || result > (std::numeric_limits<unsigned>::max() - unsigned(c - '0')) / 10)
            throw std::invalid_argument("Invalid integer setting: " + key);
        result = result * 10 + unsigned(c - '0');
    }
    return result;
}

void CompilerSettings::parse(const std::string& source, bool dos_compatibility)
{
    if (dos_compatibility) {
        auto next          = parse_compatibility_settings(source);
        values_            = std::move(next);
        dos_compatibility_ = true;
        return;
    }
    // Parsing is atomic: a malformed file must not leave partially applied options.
    auto next = values_;
    std::istringstream stream(source.substr(0, source.find('\x1a')));
    std::string line;
    unsigned line_number = 0;
    while (std::getline(stream, line)) {
        ++line_number;
        const auto raw_line = line;
        line                = trim(line);
        if (line.empty() || line[0] == ';')
            continue;
        auto end = line.find(']');
        if (line[0] != '[' || end == std::string::npos)
            throw std::invalid_argument("Invalid MXL record at line " + std::to_string(line_number));
        auto key = upper(line.substr(1, end - 1));
        if (key.empty() || key.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != std::string::npos)
            throw std::invalid_argument("Invalid MXL setting name");
        auto tail = trim(line.substr(end + 1));
        if (tail.empty() || tail[0] != '=')
            throw std::invalid_argument("Missing '=' after MXL setting " + key);
        // Read records in order; a later occurrence replaces the earlier value,
        // including repeated synthesis policy selections.
        next[key] = trim(tail.substr(1));
    }
    if (next.at("VERSION") != "MACHXL 2.0" && next.at("VERSION") != "MACHXL 2.1")
        throw std::invalid_argument("Unsupported MACHXL settings version: " + next.at("VERSION"));
    values_            = std::move(next);
    dos_compatibility_ = false;
}
}  // namespace machxl
