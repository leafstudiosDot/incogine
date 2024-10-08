#include "splash.h"
#include "../../core/engine/engine.h"
#include "../../scenes/MainScene.h"

Splash::Splash() {
    // Scene constructor
    font.Init(Engine::Instance(0, nullptr)->GetRenderer());
    frames = 0;
    splashopacity = 0;
}

Splash::~Splash() {
    // Scene destructor
}

void Splash::Start() {
    // Scene initialized, calls at the initialization of the scene
    font.setColor(0, 0, 0, 0);

    font.setFont(reinterpret_cast<const char*>(_mainfont_data), _mainfont_size);
    if (!font.GetFont()) {
        std::cerr << "Failed to load font in Splash::Start" << std::endl;
    }

    //Engine::Instance(0, nullptr)->SetScene(new MainScene());
}

void Splash::Update() {
    // Scene update, calls every frame
    frames++;
    if (frames >= 100 && frames < 300) {
        if (splashopacity >= 254) {
            splashopacity = 255;
        } else {
            splashopacity += 2;
        }
    }

    if (frames >= 300) {
        if (splashopacity <= 0) {
            splashopacity = 0;
        } else {
            splashopacity -= 2;
        }
    }

    if (frames >= 440) {
        Engine::Instance(0, nullptr)->SetScene(new MainScene());
    }
}

void Splash::Render() {
    // Scene render
    font.renderUI("Splash Scene", 10, 10, 24);
    font.setColor(splashopacity, splashopacity, splashopacity, splashopacity);
}
