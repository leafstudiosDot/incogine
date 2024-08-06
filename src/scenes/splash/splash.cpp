#include "splash.h"
#include "../../core/engine/engine.h"
#include "../../scenes/MainScene.h"

void Splash::Start() {
    // Scene initialized, calls at the initialization of the scene
    Engine::Instance(0, nullptr)->SetScene(new MainScene());
}

void Splash::Update() {
    // Scene update, calls every frame
}

void Splash::Render() {
    // Scene render
}
