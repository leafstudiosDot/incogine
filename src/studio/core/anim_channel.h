// Incogine Studio — shared single-instance channel naming for .incoanim files.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Incogine Animator is a separate process, so opening an animation is a process
// launch. To avoid spawning a second editor on a file that is already open, the
// Animator listens on a local socket named after the file and Studio hands the
// path to it instead of launching.
//
// Both sides must compute the SAME name, so the mapping lives here rather than
// being duplicated. Qt-free (it links into IncogineStudioCore, which the Animator
// links too), and deliberately dependency-free: FNV-1a is a few lines, so there
// is no reason to pull in a crypto or hash helper for a socket name.
#pragma once

#include <string>

namespace icg {
namespace studio {

// Deterministic per-path channel name for the local socket. Platform socket
// names are length-limited, so the path is hashed rather than embedded — which
// also keeps paths with separators or spaces out of the name entirely.
std::string AnimChannelNameForPath(const std::string& path);

// Channel used by an Animator with no document yet.
inline constexpr const char* kAnimSessionChannel = "incoanim_session";

} // namespace studio
} // namespace icg