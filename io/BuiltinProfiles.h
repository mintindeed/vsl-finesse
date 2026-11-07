#pragma once

#include <vslf/Profile.h>

#include <vector>

namespace vslf::io
{

// The instrument profiles in /profiles, compiled into the binary.
// Sorted by family, then name. "generic" is always present.
std::vector<InstrumentProfile> builtinProfiles();

// Finds a profile by id in `profiles`; nullptr if missing.
const InstrumentProfile* findProfile (const std::vector<InstrumentProfile>& profiles, const std::string& id);

} // namespace vslf::io
