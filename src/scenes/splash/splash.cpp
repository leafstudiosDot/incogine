#include "splash.h"
#include "../../core/engine/engine.h"
#include "../../scenes/MainScene.h"

Splash::Splash() : Scene("Splash") {
    // Scene constructor
    font.Init(Engine::Instance(0, nullptr)->GetRenderer());
    splashopacity = 0;
}

Splash::~Splash() {
    // Scene destructor
}

void Splash::Start() {
    // Scene initialized, calls at the initialization of the scene
    font.setColor(0, 0, 0, 0);

    font.setFont(reinterpret_cast<const char*>(_mainfont_data), _mainfont_size);
    font.setTextContent("Powered by Incogine");

    font.setFontSize(32);
    if (!font.GetFont()) {
        std::cerr << "Failed to load font in Splash::Start" << std::endl;
    }

    //Engine::Instance(0, nullptr)->SetScene(new MainScene());
}

void Splash::Update() {
    // Scene update, calls every frame
    deltaTime = Engine::Instance(0, nullptr)->getDeltaTime() / 1000.0f;

    elapsedTime += deltaTime;

    if (elapsedTime >= LogoOne_startFadeIn && elapsedTime < LogoOne_endFadeIn) {
        splashopacity = std::min(splashopacity + 4, 255);
    } else if (elapsedTime >= LogoOne_startFadeOut && elapsedTime <= LogoOne_endFadeOut) {
        splashopacity = std::max(splashopacity - 2, 0);
    } else if (elapsedTime >= LogoOne_switchSceneTime) {
        Engine::Instance(0, nullptr)->SetScene(new MainScene());
    }
}

void Splash::Render() {
    // Scene render
    //font.renderUI((Engine::Instance(0, nullptr)->GetWindowSize().width/2) - (font.getFontWidth().width/2), (Engine::Instance(0, nullptr)->GetWindowSize().height / 2));
    font.renderUI((Engine::Instance(0, nullptr)->GetWindowSize().width/2.8), (Engine::Instance(0, nullptr)->GetWindowSize().height / 2));
    int scaledFontSize = ((Engine::Instance(0, nullptr)->GetWindowSize().width / 2) / (float)720) * 48;
    font.setFontSize(scaledFontSize);

    font.setColor(splashopacity, splashopacity, splashopacity, splashopacity);
}
