// Incogine studio-preview live commands implementation.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "preview_apply.h"

#include "../objects/objects.h"

namespace {

uint64_t g_selectedId = 0;

} // namespace

void ApplyPreviewTransform(uint64_t id, const float pos[3], const float rot[3],
                           const float scale[3]) {
    Object* obj = Object::FindById(id);
    if (!obj) {
        return;
    }
    obj->setPosition(Position(pos[0], pos[1], pos[2]));
    obj->setRotation(Rotation(rot[0], rot[1], rot[2]));
    obj->setScale(Scale(scale[0], scale[1], scale[2]));
}

void SetPreviewSelection(uint64_t id) {
    g_selectedId = id;
}

uint64_t GetPreviewSelection() {
    return g_selectedId;
}
