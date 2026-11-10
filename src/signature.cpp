#include "signature.hpp"
#include <cctype>

namespace machxl {
void apply_signature(FuseImage& image, const std::string& signature)
{
    if (image.device != "MACH355" && image.device != "MACH445" && image.device != "MACH446" &&
        image.device != "MACH465") {
        image.user_code.clear();
        return;
    }
    std::uint32_t value = 0;
    if (!signature.empty()) {
        unsigned base = 10, limit = 9;
        std::string digits = signature;
        if (signature.size() > 1 && signature[0] == '#') {
            switch (std::tolower(static_cast<unsigned char>(signature[1]))) {
            case 'b':
                base  = 2;
                limit = 32;
                break;
            case 'o':
                base  = 8;
                limit = 10;
                break;
            case 'd':
                break;
            case 'h':
                base  = 16;
                limit = 8;
                break;
            default:
                base = 0;
                break;
            }
            if (base)
                digits = signature.substr(2);
        }
        // MACHXL truncates before checking the digits. Even an invalid
        // suffix beyond this radix's limit does not turn it into text.
        digits              = digits.substr(0, limit);
        std::size_t at      = 0;
        const bool negative = !digits.empty() && digits[0] == '-';
        if (!digits.empty() && (digits[0] == '-' || digits[0] == '+'))
            ++at;
        bool numeric = base && at < digits.size();
        for (; numeric && at < digits.size(); ++at) {
            const auto c         = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(digits[at])));
            const unsigned digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : 16;
            if (digit >= base)
                numeric = false;
            else
                value = value * base + digit;
        }
        if (numeric) {
            if (negative)
                value = 0u - value;
        } else {
            // Preserve compiler compatibility: take four leading characters
            // and pad on the left, despite the manual's right-padding rule.
            auto text = signature.substr(0, 4);
            text.insert(0, 4 - text.size(), ' ');
            value = 0;
            for (unsigned char c : text)
                value = (value << 8) | c;
        }
    }
    image.user_code.resize(32);
    for (unsigned bit = 0; bit < 32; ++bit)
        image.user_code[bit] = static_cast<std::uint8_t>((value >> (31 - bit)) & 1u);
}
}  // namespace machxl
