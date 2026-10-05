#include "anim_channel.h"

#include <cstdint>
#include <cstdio>

namespace icg {
namespace studio {

std::string AnimChannelNameForPath(const std::string& path) {
    // FNV-1a 64-bit. Deterministic across processes and platforms (unlike
    // std::hash, whose output is implementation-defined), which matters because
    // two separate processes must derive the same socket name.
    uint64_t hash = 1469598103934665603ull;
    for (char c : path) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= 1099511628211ull;
    }
    // Hex without padding, prefixed so the name is recognizable in a socket
    // listing.
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "incoanim_%016llx",
                  static_cast<unsigned long long>(hash));
    return std::string(buffer);
}

} // namespace studio
} // namespace icg