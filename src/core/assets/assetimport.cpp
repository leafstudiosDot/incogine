#include "assetimport.h"

#include <algorithm>
#include <cctype>

#include "../animation/anim2d_importer.h"

namespace icg {
namespace assets {
namespace {

// True when `ch` separates path segments. Both separators are accepted because
// Studio hands out native paths and the engine hands out canonical '/'-paths.
bool IsSeparator(char ch) {
    return ch == '/' || ch == '\\';
}

} // namespace

AssetImporterRegistry& AssetImporterRegistry::Instance() {
    static AssetImporterRegistry registry;
    return registry;
}

std::string AssetImporterRegistry::NormalizeExtension(const char* extension) {
    std::string out;
    if (!extension) {
        return out;
    }
    size_t begin = 0;
    if (extension[0] == '.') {
        begin = 1;
    }
    for (size_t i = begin; extension[i] != '\0'; ++i) {
        out.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(extension[i]))));
    }
    return out;
}

std::string AssetImporterRegistry::ExtensionOf(const char* path) {
    if (!path) {
        return std::string();
    }
    // Last dot after the last separator, so "assets/v1.2/file" has no
    // extension while "assets/file.incoanim" does.
    size_t lastSep = 0;
    size_t lastDot = std::string::npos;
    for (size_t i = 0; path[i] != '\0'; ++i) {
        if (IsSeparator(path[i])) {
            lastSep = i + 1;
            lastDot = std::string::npos;
        } else if (path[i] == '.') {
            lastDot = i;
        }
    }
    if (lastDot == std::string::npos || lastDot < lastSep) {
        return std::string();
    }
    // A dot that starts the filename is part of the name, not an extension
    // separator: ".gitignore" is a dotfile with no extension, not a file with
    // extension "gitignore". Without this, the asset browser would try to
    // dispatch dotfiles on their bare name.
    if (lastDot == lastSep) {
        return std::string();
    }
    return NormalizeExtension(path + lastDot + 1);
}

bool AssetImporterRegistry::Register(std::unique_ptr<IAssetImporter> importer) {
    if (!importer) {
        return false;
    }
    const std::string ext = NormalizeExtension(importer->extension());
    if (ext.empty()) {
        return false;
    }
    if (FindByExtension(ext.c_str()) != nullptr) {
        return false;
    }
    importers_.push_back(std::move(importer));
    return true;
}

void AssetImporterRegistry::RegisterBuiltins() {
    // Single switch point for formats compiled into this binary. Later formats
    // (FBX / OBJ / glTF / .blend for 3D animation) register here too.
    Register(std::unique_ptr<IAssetImporter>(new anim::Anim2DImporter()));
}

IAssetImporter* AssetImporterRegistry::FindByExtension(
    const char* extension) const {
    const std::string ext = NormalizeExtension(extension);
    if (ext.empty()) {
        return nullptr;
    }
    for (const auto& importer : importers_) {
        if (NormalizeExtension(importer->extension()) == ext) {
            return importer.get();
        }
    }
    return nullptr;
}

IAssetImporter* AssetImporterRegistry::Find(const char* path) const {
    return FindByExtension(ExtensionOf(path).c_str());
}

std::vector<IAssetImporter*> AssetImporterRegistry::All() const {
    std::vector<IAssetImporter*> out;
    out.reserve(importers_.size());
    for (const auto& importer : importers_) {
        out.push_back(importer.get());
    }
    return out;
}

} // namespace assets
} // namespace icg