#include <iostream>
#include "../../core/scenes/scenes.h"
#include "../../core/fonts/fonts.h"
#include "../../fonts/main_font.h"

#ifndef SPLASH_H
#define SPLASH_H

class Splash : public Scene {
    private:
        Font font;
        int splashopacity;
        float deltaTime;
        float elapsedTime = 0.0f;

        float LogoOne_startFadeIn = 100.0f / 60.0f;  // ~1.67 seconds
        float LogoOne_endFadeIn = 300.0f / 60.0f;  // 5 seconds
        float LogoOne_startFadeOut = 400.0f / 60.0f;  // ~6.67 seconds
        float LogoOne_endFadeOut = 520.0f / 60.0f;  // ~8.67 seconds
        float LogoOne_switchSceneTime = 540.0f / 60.0f; // 9 seconds
    public:
        Splash();
        ~Splash();

        void Start() override;
        void Update() override;
        void Render() override;
};

#endif