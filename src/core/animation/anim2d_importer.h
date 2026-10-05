// Incogine — `.incoanim` importer (IAssetImporter implementation).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// This is the seam a future FBX / OBJ / glTF / `.blend` importer would follow
// for 3D animation: register against an extension, parse bytes, hand back the
// module's own document type. Nothing above the importer knows how a
// `.incoanim` is stored.

#pragma once

#include <string>

#include "../assets/assetimport.h"
#include "anim_document.h"

namespace icg {
namespace anim {

// Parses `.incoanim` bytes into an AnimDocument. The document lives on the
// importer, so callers read it through `document()` — the importer owns the
// parsed result rather than returning an untyped blob.
class Anim2DImporter : public assets::IAssetImporter {
public:
    assets::AssetKind kind() const override { return assets::AssetKind::Animation2D; }
    const char* extension() const override { return "incoanim"; }
    const char* displayName() const override { return "2D Animation"; }

    bool ImportBytes(const void* bytes, size_t size, std::string& error) override;

    const AnimDocument& document() const { return document_; }
    AnimDocument& document() { return document_; }

private:
    AnimDocument document_;
};

} // namespace anim
} // namespace icg