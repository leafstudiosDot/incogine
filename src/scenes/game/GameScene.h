#include <iostream>
#include "../../core/scenes/scenes.h"
#include "core/components/components.h"
#include "core/components/transform/transform.h"

#include "../../core/fonts/fonts.h"
#include "../../fonts/jpsup_font.h"

#ifndef GAMESCENE_H
#define GAMESCENE_H

class GameScene : public Scene {
    private:
        Font steamfont;

    public:
        GameScene();
        ~GameScene();

        void Start() override;
        void Update() override;
        void Render() override;
        void Events(const SDL_Event& event) override;

};

#endif