#pragma once
#include "device_family.hpp"
#include <string>
#include <vector>

namespace machxl {
// Low and high power request flags retained during physical assembly.
// A placed output node has one canonical request, shared by its pad aliases.
struct Mach446PowerRequest
{
    bool low  = false;
    bool high = false;
    static Mach446PowerRequest source(bool low) { return {low, !low}; }
};
// Describe accepted steering and power modes, not measured current.
// Conflicting raw request flags decode to #H, although canonical source
// fits cannot create that combination.
std::string mach446_power_annotation(unsigned cell, Mach446PowerRequest request, const MachDevicePhysical& physical,
                                     const std::vector<unsigned>& steering_owners);

// Corrected MACHXL coefficient model. Every physical row is
// counted once; the original out-of-bounds OE reads are never emulated.
float corrected_mach446_icc(const MachDevicePhysical& physical);
}  // namespace machxl
