// Incogine studio-preview argv parsing implementation.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "preview_args.h"

namespace {

constexpr char kPreviewFlag[] = "--studio-preview";
constexpr char kScenePrefix[] = "--studio-preview=";
constexpr char kTokenPrefix[] = "--studio-token=";

bool HasPrefix(const std::string& arg, const char* prefix, size_t len) {
    return arg.compare(0, len, prefix, len) == 0;
}

} // namespace

PreviewArgs ParsePreviewArgs(int argc, char** argv) {
    PreviewArgs out;
    for (int i = 0; i < argc; ++i) {
        const std::string arg = argv[i] ? argv[i] : "";
        if (arg == kPreviewFlag) {
            out.enabled = true;
        } else if (HasPrefix(arg, kScenePrefix, sizeof(kScenePrefix) - 1)) {
            out.enabled = true;
            out.scene = arg.substr(sizeof(kScenePrefix) - 1);
        } else if (HasPrefix(arg, kTokenPrefix, sizeof(kTokenPrefix) - 1)) {
            try {
                out.token = std::stoull(arg.substr(sizeof(kTokenPrefix) - 1),
                                        nullptr, 16);
            } catch (...) {
                out.token = 0;
            }
        }
    }
    return out;
}
