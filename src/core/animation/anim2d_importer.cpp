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
    const std::string text(static_cast<const char*>(bytes), size);
    // A previous successful import must not leak through when the next one
    // fails, or callers would silently read stale artwork.
    document_ = AnimDocument();
    return Deserialize(text, document_, error);
}

} // namespace anim
} // namespace icg