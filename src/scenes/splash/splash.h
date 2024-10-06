#include <iostream>
#include "../../core/scenes/scenes.h"
#include "../../core/fonts/fonts.h"
#include "../../fonts/main_font.h"

#ifndef SPLASH_H
#define SPLASH_H

class Splash : public Scene {
    private:
        Font font;
    public:
        Splash();
        ~Splash();

        void Start() override;
        void Update() override;
        void Render() override;
};

#endif