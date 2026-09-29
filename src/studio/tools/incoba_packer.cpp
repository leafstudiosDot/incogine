// Incogine Studio — `incoba_packer` CLI (Qt-free, headless).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Usage:
//   incoba_packer <assetDir> <out.incoba>   pack a directory (single bundle)
//   incoba_packer --split <assetDir> <outDir> [--stem <name>] [--max-mb <N>]
//       split into payload-capped bundles + index.incobai (default 128 MiB,
//       default stem "a" -> a.incoba or a_00.incoba, ...)
//   incoba_packer --list <bundle.incoba>     list bundle entries
//   incoba_packer --list-index <index.incobai>  list indexed entries
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include "../core/incoba/incoba.h"

namespace {

int ListBundle(const char* path) {
    icg::studio::incoba::Reader reader;
    std::string error;
    if (!reader.Open(path, error)) {
        std::cerr << "incoba_packer: " << error << "\n";
        return 1;
    }
    for (const auto& e : reader.entries()) {
        std::cout << e.size << "\t" << e.path << "\n";
    }
    return 0;
}

int ListIndex(const char* path) {
    icg::studio::incoba::Index index;
    std::string error;
    if (!icg::studio::incoba::ReadIndex(path, index, error)) {
        std::cerr << "incoba_packer: " << error << "\n";
        return 1;
    }
    for (const auto& b : index.bundles) {
        std::cout << "bundle:\t" << b << "\n";
    }
    for (const auto& e : index.entries) {
        std::cout << e.size << "\t" << index.bundles[e.bundle] << "\t" << e.path << "\n";
    }
    return 0;
}

void Usage() {
    std::cerr << "Usage:\n"
              << "  incoba_packer <assetDir> <out.incoba>\n"
              << "  incoba_packer --split <assetDir> <outDir> [--stem <name>] [--max-mb <N>]\n"
              << "  incoba_packer --list <bundle.incoba>\n"
              << "  incoba_packer --list-index <index.incobai>\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "--list") {
        return ListBundle(argv[2]);
    }
    if (argc == 3 && std::string(argv[1]) == "--list-index") {
        return ListIndex(argv[2]);
    }
    if (argc >= 4 && std::string(argv[1]) == "--split") {
        std::string stem = "a";
        uint64_t maxBytes = icg::studio::incoba::kDefaultMaxBundleBytes;
        for (int i = 4; i < argc; ++i) {
            const std::string flag = argv[i];
            if (flag == "--stem" && i + 1 < argc) {
                stem = argv[++i];
            } else if (flag == "--max-mb" && i + 1 < argc) {
                const unsigned long mb = std::strtoul(argv[++i], nullptr, 10);
                if (mb == 0) {
                    std::cerr << "incoba_packer: --max-mb must be > 0\n";
                    return 2;
                }
                maxBytes = static_cast<uint64_t>(mb) * 1024ull * 1024ull;
            } else {
                Usage();
                return 2;
            }
        }
        std::string error;
        if (!icg::studio::incoba::PackSplit(argv[2], argv[3], stem, maxBytes, error)) {
            std::cerr << "incoba_packer: " << error << "\n";
            return 1;
        }
        std::cout << "Packed " << argv[2] << " -> " << argv[3] << "/ (stem=" << stem << ")\n";
        return 0;
    }
    if (argc != 3) {
        Usage();
        return 2;
    }
    std::string error;
    if (!icg::studio::incoba::PackDirectory(argv[1], argv[2], error)) {
        std::cerr << "incoba_packer: " << error << "\n";
        return 1;
    }
    std::cout << "Packed " << argv[1] << " -> " << argv[2] << "\n";
    return 0;
}
