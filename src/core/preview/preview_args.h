// Incogine studio-preview argv parsing (engine side, stdlib only).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Centralizes the --studio-preview[=<SceneClass>] / --studio-token=<hex>
// parsing (prefix lengths computed, never hardcoded) so Studio and the
// engine agree on values exactly.
#pragma once

#include <cstdint>
#include <string>

struct PreviewArgs {
    bool enabled = false;
    std::string scene;
    uint64_t token = 0; // 0 = unbound
};

PreviewArgs ParsePreviewArgs(int argc, char** argv);
