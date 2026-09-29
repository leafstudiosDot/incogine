// Puroko — studio-preview scene registrations (reference game layer).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Registers every concrete scene with the engine's PreviewSceneRegistry so
// `--studio-preview=<ClassName>` boots it directly. A game built on
// Incogine registers its own scenes the same way.
#include "core/preview/preview_scenes.h"

#include "scenes/credits/Credits.h"
#include "scenes/game/GameScene.h"
#include "scenes/MainScene.h"
#include "scenes/settings/SettingsScene.h"
#include "scenes/splash/splash.h"

namespace {

bool RegisterPreviewScenes() {
    PreviewSceneRegistry::Register("Splash", []() -> Scene* { return new Splash(); });
    PreviewSceneRegistry::Register("MainScene",
                                   []() -> Scene* { return new MainScene(); });
    PreviewSceneRegistry::Register("GameScene",
                                   []() -> Scene* { return new GameScene(); });
    PreviewSceneRegistry::Register("SettingsScene",
                                   []() -> Scene* { return new SettingsScene(); });
    PreviewSceneRegistry::Register("CreditsScene",
                                   []() -> Scene* { return new CreditsScene(); });
    return true;
}

const bool kPreviewScenesRegistered = RegisterPreviewScenes();

} // namespace

// External linkage: Engine::Init calls this (which is also what forces
// this TU — and every scene it names — to link).
int PreviewSceneRegistrationCount() {
    return static_cast<int>(PreviewSceneRegistry::Names().size());
}
