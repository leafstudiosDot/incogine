// Incogine Studio - SHA-256 (Qt-free, no third-party dependencies).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Standard FIPS 180-4 implementation used to bind Studio launches to the
// development build: the game executable's hash must match its CMake-
// generated sidecar before the preview channel opens.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace icg {
namespace studio {
namespace sha256 {

// Lowercase hex digest of raw bytes / a byte vector / a whole file.
std::string HexOf(const void* data, size_t size);
std::string HexOf(const std::vector<uint8_t>& data);
bool HexOfFile(const std::string& path, std::string& outHex, std::string& error);

} // namespace sha256
} // namespace studio
} // namespace icg

