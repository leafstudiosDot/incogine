#include "splash.h"
#include "../../core/engine/engine.h"
#include "../../scenes/MainScene.h"

Splash::Splash() {
    // Scene constructor
    font.Init(Engine::Instance(0, nullptr)->GetRenderer());
}

Splash::~Splash() {
    // Scene destructor
}

void Splash::Start() {
    // Scene initialized, calls at the initialization of the scene

    font.setFont(reinterpret_cast<const char*>(_mainfont_data), _mainfont_size);
    if (!font.GetFont()) {
        std::cerr << "Failed to load font in Splash::Start" << std::endl;
    }

    //Engine::Instance(0, nullptr)->SetScene(new MainScene());
}

void Splash::Update() {
    // Scene update, calls every frame
}

void Splash::Render() {
    // Scene render
    font.renderUI("Splash Scene", 10, 10, 24);
}
