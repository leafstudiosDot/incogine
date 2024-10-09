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
        if (splashopacity >= 245) {
            splashopacity = 255;
        } else {
            splashopacity += 4;
        }
    }

    if (frames >= 400 && frames <= 500) {
        if (splashopacity <= 0) {
            splashopacity = 0;
        } else {
            splashopacity -= 2;
        }
    }

    if (frames >= 540) {
        Engine::Instance(0, nullptr)->SetScene(new MainScene());
    }
}

void Splash::Render() {
    // Scene render
    font.renderUI("Powered by Incogine", (Engine::Instance(0, nullptr)->GetWindowSize().width/2) - 120, (Engine::Instance(0, nullptr)->GetWindowSize().width/4), 24);
    font.setColor(splashopacity, splashopacity, splashopacity, splashopacity);
}
