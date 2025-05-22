#include "splash.h"
#include "../../core/engine/engine.h"
#include "../../scenes/MainScene.h"

Splash::Splash() : Scene("Splash") {
    // Scene constructor
    //font.Init(Engine::Instance(0, nullptr)->GetRenderer());
    splashopacity = 0;
}

Splash::~Splash() {
    // Scene destructor
}

void Splash::Start() {
    // Scene initialized, calls at the initialization of the scene
    /*font.setColor(0, 0, 0, 0);

    font.setFont(reinterpret_cast<const char*>(_mainfont_data), _mainfont_size);
    font.setTextContent("Powered by Incogine");

    font.setFontSize(32);
    if (!font.GetFont()) {
        std::cerr << "Failed to load font in Splash::Start" << std::endl;
    }*/

    //Engine::Instance(0, nullptr)->SetScene(new MainScene());
}

void Splash::Update() {
    // Scene update, calls every frame
    /*deltaTime = Engine::Instance(0, nullptr)->getDeltaTime() / 1000.0f;

    elapsedTime += deltaTime;

    LogoOne_startY = static_cast<float>(Engine::Instance(0, nullptr)->GetWindowSize().height); // Logo starts at the bottom
    LogoOne_centerY = static_cast<float>(Engine::Instance(0, nullptr)->GetWindowSize().height / 2); // Logo at center

	// Powered by Incogine logo
    if (elapsedTime >= LogoOne_startFadeIn && elapsedTime < LogoOne_endFadeIn) {
        splashopacity = min(splashopacity + 4, 255);
        float t = (elapsedTime - LogoOne_startFadeIn) / (LogoOne_endFadeIn - LogoOne_startFadeIn);
        float easedT = cubicBezier(t, 0.0f, 0.5f, 0.0f, 1.0f);
        LogoOne_newY = lerp(LogoOne_startY, LogoOne_centerY, easedT);
    } else if (elapsedTime >= LogoOne_endFadeIn && elapsedTime < LogoOne_startFadeOut) {
        LogoOne_newY = LogoOne_centerY;
    } else if (elapsedTime >= LogoOne_startFadeOut && elapsedTime <= LogoOne_endFadeOut) {
        splashopacity = max(splashopacity - 2, 0);
        float t = (elapsedTime - LogoOne_startFadeOut) / (LogoOne_endFadeOut - LogoOne_startFadeOut);
        float easedT = cubicBezier(t, 1.0f, 0.0f, 1.0f, 0.5f);
        LogoOne_newY = lerp(LogoOne_centerY, LogoOne_endY, easedT);
    } else if (elapsedTime >= LogoOne_switchSceneTime) {
        Engine::Instance(0, nullptr)->SetScene(new MainScene());
    }*/
}

void Splash::Render() {
    // Scene render
    //font.renderUI((Engine::Instance(0, nullptr)->GetWindowSize().width/2) - (font.getFontWidth().width/2), (Engine::Instance(0, nullptr)->GetWindowSize().height / 2));
    /*font.renderUI((Engine::Instance(0, nullptr)->GetWindowSize().width / 2.8), (LogoOne_newY));
    int scaledFontSize = ((Engine::Instance(0, nullptr)->GetWindowSize().width / 2) / (float)720) * 48;
    font.setFontSize(scaledFontSize);

    font.setColor(splashopacity, splashopacity, splashopacity, splashopacity);*/
}
