#include "jedec.hpp"
#include "device_family.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace machxl {
namespace {
void validate_fuse_bits(const std::vector<std::uint8_t>& bits)
{
    if (bits.size() > 16000000)
        throw std::invalid_argument("Fuse image exceeds 16 million bits");
    if (std::any_of(bits.begin(), bits.end(), [](auto bit) { return bit > 1; }))
        throw std::invalid_argument("A fuse value must be zero or one");
}
void validate_jedec_text_field(const std::string& text)
{
    for (unsigned char c : text)
        if (c < 32 || c > 126 || c == '*')
            throw std::invalid_argument("Invalid JEDEC text field");
}
std::string format_hex_word(unsigned value)
{
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << (value & 0xffff);
    return out.str();
}

void write_fuse_record(std::ostream& out, const std::vector<std::uint8_t>& fuses, std::size_t start, std::size_t count)
{
    out << 'L' << std::setw(5) << std::setfill('0') << start << ' ';
    for (auto n = start; n < start + count; ++n)
        out << char('0' + fuses[n]);
    out << "*\r\n";
}

void write_multiline_fuse_record(std::ostream& out, const FuseImage& image, std::size_t start, unsigned rows,
                                 unsigned width)
{
    out << 'L' << std::setw(5) << std::setfill('0') << start;
    for (unsigned row = 0; row < rows; ++row) {
        out << "\r\n       ";
        for (unsigned bit = 0; bit < width; ++bit)
            out << char('0' + image.fuses.at(start + row * width + bit));
    }
    out << "*\r\n";
}
void write_device_family_records(std::ostream& out, const FuseImage& image)
{
    const auto& d = mach_device_layout(image.device);
    if (image.pin_count != d.pins || image.fuses.size() != d.fuse_count ||
        image.electrical.size() != d.electrical_count)
        throw std::invalid_argument("JEDEC image differs from physical device dimensions");
    auto note = [&](const std::string& text, unsigned block) {
        out << "NOTE  " << text << " " << char('A' + block) << " *\r\n";
    };
    auto csm = [&](unsigned b) {
        note("Central Switch Matrix fuses for BLOCK", b);
        const auto count = d.legacy110 ? 44u : d.muxes * unsigned(d.mux_planes.size());
        const auto width = d.legacy110 ? count : count / 2;
        for (unsigned offset = 0; offset < count; offset += width)
            write_fuse_record(out, image.fuses, d.block_bases[b] + offset, width);
    };
    auto products = [&](unsigned b) {
        note("Product Term Array for BLOCK", b);
        const unsigned first = d.legacy110 || d.device == "MACH111" ? 3 : 2;
        for (unsigned row = 0; row < d.rows; ++row) {
            if (d.device == "MACH215") {
                if (row % 8 == 0)
                    out << "NOTE  Individual PTs for relative macrocell " << row / 8 << " *\r\n";
                if (row % 8 == 4)
                    out << "NOTE  pt cluster " << row / 8 << " *\r\n";
            } else {
                if (row >= first && row < first + d.cells * 4 && (row - first) % 4 == 0)
                    out << "NOTE  pt cluster " << (row - first) / 4 << " *\r\n";
                if (row == first + d.cells * 4)
                    out << "NOTE  End of clustered pts *\r\n";
            }
            write_fuse_record(out, image.fuses, d.product_address(b, row, 0), d.muxes * 2);
        }
    };
    auto macros = [&](unsigned b) {
        note("Macrocell fuses for BLOCK", b);
        for (unsigned cell = 0; cell < d.cells; cell += 4)
            write_fuse_record(out, image.fuses, d.macrocell_address(b, cell),
                              (d.macrocell_width(0) + d.macrocell_width(1)) * 2);
    };
    auto modern_block = [&](unsigned b) {
        const unsigned cluster = d.device == "MACH355" ? 6 : 11;
        for (unsigned row = 0; row < d.rows - 2; ++row) {
            if (row % cluster == 0)
                write_multiline_fuse_record(out, image, d.product_address(b, row, 0), 1, d.muxes * 2);
            else
                write_fuse_record(out, image.fuses, d.product_address(b, row, 0), d.muxes * 2);
        }
        write_multiline_fuse_record(out, image, d.product_address(b, d.rows - 2, 0), 2, d.muxes * 2);
        write_fuse_record(out, image.fuses, d.block_control_address(b), 4);
        for (unsigned cell = 0; cell < d.cells; ++cell)
            write_fuse_record(out, image.fuses, d.macrocell_address(b, cell), d.macrocell_width(cell));
    };
    if (d.legacy110) {
        for (unsigned b = 0; b < d.blocks; ++b)
            products(b);
        for (unsigned b = 0; b < d.blocks; ++b)
            macros(b);
        for (unsigned b = 0; b < d.blocks; ++b)
            csm(b);
    } else if (!d.one_hot) {
        for (unsigned b = 0; b < d.blocks; ++b) {
            csm(b);
            products(b);
            macros(b);
        }
    } else if (d.subblock_stride) {
        for (unsigned pair = 0; pair < d.blocks; pair += 2) {
            const auto display = pair % 4 == 2 ? pair + 1 : pair;
            out << "NOTE  Interleaved Central Switch Matrices for BLOCKS " << char('A' + display) << " and "
                << char('A' + (display ^ 1)) << " *\r\n";
            write_multiline_fuse_record(out, image, d.block_bases[pair], d.slots, d.muxes * 2);
            const auto first = d.block_offsets[pair] ? pair + 1 : pair;
            for (unsigned half = 0; half < 2; ++half) {
                // DOS compatibility uses the pair index to label block offsets;
                // fuse addresses still follow the physical block order.
                note("Non-CSM fuses for BLOCK",
                     image.dos_compatibility ? pair + (half ^ d.block_offsets[pair / 2]) : first ^ half);
                modern_block(first ^ half);
            }
        }
    } else {
        for (unsigned b = 0; b < d.blocks; ++b) {
            note("BLOCK", b);
            write_multiline_fuse_record(out, image, d.block_bases[b], d.slots / 2, d.muxes * 2);
            modern_block(b);
        }
    }
    if (d.electrical_count > d.electrical_prefix_count) {
        out << "NOTE  Power Down bits descending by block *\r\n";
        out << (d.one_hot ? "NOTE    and by product term cluster within blocks.*\r\n"
                          : "NOTE    and by macrocell within blocks.*\r\n");
    }
}

// MACHFITR's MACH111 records follow the physical CSM, product-term and
// macrocell sections. This is a layout rule, independent of circuit data.
void write_mach111_records(std::ostream& out, const FuseImage& image)
{
    if (image.device != "MACH111" || image.pin_count != 44 || image.fuses.size() != 7724 ||
        image.electrical.size() != 32)
        throw std::invalid_argument("MACH111 JEDEC layout requires 7724 main and 32 electrical bits");
    for (unsigned block = 0; block < 2; ++block) {
        const auto base  = 3862 * block;
        const auto label = char('A' + block);
        out << "NOTE  Central Switch Matrix fuses for BLOCK " << label << " *\r\n";
        write_fuse_record(out, image.fuses, base, 39);
        write_fuse_record(out, image.fuses, base + 39, 39);
        out << "NOTE  Product Term Array for BLOCK " << label << " *\r\n";
        for (unsigned row = 0; row < 70; ++row) {
            if (row >= 3 && row < 67 && (row - 3) % 4 == 0)
                out << "NOTE  pt cluster " << (row - 3) / 4 << " *\r\n";
            if (row == 67)
                out << "NOTE  End of clustered pts *\r\n";
            write_fuse_record(out, image.fuses, base + 78 + 52 * row, 52);
        }
        out << "NOTE  Macrocell fuses for BLOCK " << label << " *\r\n";
        for (unsigned row = 0; row < 4; ++row)
            write_fuse_record(out, image.fuses, base + 3718 + 36 * row, 36);
    }
    out << "NOTE  Power Down bits descending by block *\r\n"
           "NOTE    and by macrocell within blocks.*\r\n";
}
}  // namespace

std::uint16_t fuse_checksum(const FuseImage& image)
{
    validate_fuse_bits(image.fuses);
    validate_fuse_bits(image.electrical);
    std::uint32_t checksum = 0;
    std::size_t bit_number = 0;
    for (const auto* field : {&image.fuses, &image.electrical})
        for (auto bit : *field)
            checksum += static_cast<unsigned>(bit) << (bit_number++ % 8);
    return static_cast<std::uint16_t>(checksum);
}

std::string serialize_jedec(const FuseImage& image, const std::string& preamble)
{
    if (image.fuses.empty())
        throw std::invalid_argument("Empty fuse image");
    auto checksum = fuse_checksum(image);
    if (!image.pin_count || image.pin_count > 10000)
        throw std::invalid_argument("Invalid JEDEC package pin count");
    if (image.device.empty() || image.design.empty())
        throw std::invalid_argument("JEDEC device and design identifiers are required");
    validate_jedec_text_field(image.device);
    validate_jedec_text_field(image.design);
    if (!image.has_test_vectors && !image.test_vectors.empty())
        throw std::invalid_argument("JEDEC vectors supplied without simulation enabled");
    if (image.test_vectors.size() > 100000)
        throw std::invalid_argument("JEDEC test vector limit exceeded");
    for (const auto& vector : image.test_vectors)
        if (vector.size() != image.pin_count || vector.find_first_not_of("01HLXZCNKPUD") != std::string::npos)
            throw std::invalid_argument("Invalid JEDEC test vector width or pin code");
    std::string heading;
    for (unsigned char c : preamble) {
        if (c == '\r')
            continue;
        if (c == '\n')
            heading += "\r\n";
        else if (c >= 32 && c <= 126)
            heading += c;
        else
            throw std::invalid_argument("Invalid character in JEDEC preamble");
    }
    if (!heading.empty() && heading.back() != '\n')
        heading += "\r\n";
    std::ostringstream frame;
    frame << '\x02' << image.device << "\r\n" << image.design << "*\r\n";
    if (image.has_test_vectors)
        frame << "QV" << std::setw(4) << std::setfill('0') << image.test_vectors.size() << "*\r\n";
    frame << "QP" << image.pin_count << "*\r\nQF" << image.fuses.size() << "*\r\n"
          << "G" << unsigned(image.security) << "*F0*\r\n";
    for (const auto& note : image.notes) {
        validate_jedec_text_field(note);
        frame << "NOTE " << note << "*\r\n";
    }
    if (image.layout == JedecLayout::MachFamily)
        write_device_family_records(frame, image);
    else if (image.layout == JedecLayout::Mach111)
        write_mach111_records(frame, image);
    else
        for (std::size_t start = 0; start < image.fuses.size(); start += 64)
            write_fuse_record(frame, image.fuses, start, std::min<std::size_t>(64, image.fuses.size() - start));
    if (!image.electrical.empty()) {
        frame << 'E';
        const auto row_width = image.layout == JedecLayout::MachFamily ? mach_device_layout(image.device).cells
                               : image.layout == JedecLayout::Mach111  ? 16u
                                                                       : 64u;
        const auto prefix =
            image.layout == JedecLayout::MachFamily ? mach_device_layout(image.device).electrical_prefix_count : 0u;
        for (std::size_t n = 0; n < image.electrical.size(); ++n) {
            if (n && (n - prefix) % row_width == 0)
                frame << "\r\n ";
            frame << char('0' + image.electrical[n]);
        }
        frame << "*\r\n";
    }
    auto write_user_code = [&]() {
        if (image.user_code.empty())
            return;
        validate_fuse_bits(image.user_code);
        frame << 'U';
        for (auto bit : image.user_code)
            frame << char('0' + bit);
        frame << "*\r\n";
    };
    if (image.has_test_vectors) {
        // MACHSIMU moves the existing user signature ahead of its vectors.
        write_user_code();
        frame << "X0*\r\n";
        for (std::size_t i = 0; i < image.test_vectors.size(); ++i)
            frame << 'V' << std::setw(4) << std::setfill('0') << i + 1 << ' ' << image.test_vectors[i] << "*\r\n";
    }
    frame << 'C' << format_hex_word(checksum) << "*\r\n";
    if (!image.has_test_vectors)
        write_user_code();
    frame << '\x03';
    auto text                  = frame.str();
    std::uint32_t transmission = 0;
    for (unsigned char c : text)
        transmission += c;
    return heading + text + format_hex_word(transmission) + "\r\n";
}
}  // namespace machxl
