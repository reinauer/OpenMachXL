#pragma once
#include "jedec.hpp"

namespace machxl {
// Encode MACHXL's 32-bit JTAG user code. Non-JTAG devices ignore SIGNATURE.
void apply_signature(FuseImage& image, const std::string& signature);
}  // namespace machxl
