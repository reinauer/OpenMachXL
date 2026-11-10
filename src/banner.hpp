#pragma once
#include "machxl_version.hpp"
#include <string>

namespace machxl {
// The two lines at the top of a report: the program with its version, the
// tool that wrote the report (MACHFITR, MACHSIMU) and the release date, then
// the copyright line. `newline` separates the two lines; none follows.
inline std::string program_banner(const char* tool, const char* newline)
{
    return std::string("OpenMachXL v" MACHXL_VERSION " ") + tool + " (" MACHXL_RELEASE_DATE ")" + newline +
           " Copyright (C) " MACHXL_RELEASE_YEAR " by the OpenMachXL team";
}
}  // namespace machxl
