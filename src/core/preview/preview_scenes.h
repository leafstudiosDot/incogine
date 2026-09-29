// Incogine studio-preview scene registry (engine side).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Maps scene class names to factories so `--studio-preview=<Name>` can boot
// a chosen scene. The engine core owns the registry; the game layer
// (src/project/preview_scenes.cpp) registers its scenes. Unknown names
// boot the normal flow.
#pragma once

#include <string>
#include <vector>

#include "../scenes/scenes.h"

typedef Scene* (*PreviewSceneFactory)();

class PreviewSceneRegistry {
public:
    static void Register(const char* name, PreviewSceneFactory factory);
    static Scene* Create(const std::string& name); // nullptr when unknown
    static std::vector<std::string> Names();

private:
    struct Entry {
        std::string name;
        PreviewSceneFactory factory;
    };
    static std::vector<Entry>& Entries();
};

// Implemented by the game layer (src/project/preview_scenes.cpp): returns
// the registered count. Engine::Init calls it in preview mode, which also
// forces the registration TU (and every scene it names) to link — static
// initializers alone would let the linker drop unreferenced scenes.
int PreviewSceneRegistrationCount();
