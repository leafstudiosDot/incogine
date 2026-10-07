#include "anim2d_importer.h"

#include "anim_io.h"

namespace icg {
namespace anim {

bool Anim2DImporter::ImportBytes(const void* bytes, size_t size,
                                 std::string& error) {
    if (bytes == nullptr && size != 0) {
        error = "ImportBytes called with a null buffer";
        return false;
    }
    // A previous successful import must not leak through when the next one
    // fails, or callers would silently read stale artwork.
    document_ = AnimDocument();
    // Format-detecting: v2 container or v1 JSON text both load, so shipped
    // games read old and new animation files with no asset changes.
    return DeserializeBytes(bytes, size, document_, error);
}

} // namespace anim
} // namespace icg