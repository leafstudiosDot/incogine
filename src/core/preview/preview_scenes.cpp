// Incogine studio-preview scene registry implementation.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "preview_scenes.h"

std::vector<PreviewSceneRegistry::Entry>& PreviewSceneRegistry::Entries() {
    static std::vector<Entry> entries;
    return entries;
}

void PreviewSceneRegistry::Register(const char* name, PreviewSceneFactory factory) {
    if (!name || !factory) {
        return;
    }
    Entries().push_back({name, factory});
}

Scene* PreviewSceneRegistry::Create(const std::string& name) {
    for (const Entry& entry : Entries()) {
        if (entry.name == name && entry.factory) {
            return entry.factory();
        }
    }
    return nullptr;
}

std::vector<std::string> PreviewSceneRegistry::Names() {
    std::vector<std::string> names;
    for (const Entry& entry : Entries()) {
        names.push_back(entry.name);
    }
    return names;
}
