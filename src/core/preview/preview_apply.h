// Incogine studio-preview live commands (engine side).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Applies Studio's live-edit commands to runtime objects, addressed by the
// stable Object ids from `src/core/objects/objects.h`. SELECT only records
// the id (picking/highlight UI consumes it later); TRANSFORM moves the
// object immediately so the preview viewport shows the result.
#pragma once

#include <cstdint>

void ApplyPreviewTransform(uint64_t id, const float pos[3], const float rot[3],
                           const float scale[3]);
void SetPreviewSelection(uint64_t id);
uint64_t GetPreviewSelection();
