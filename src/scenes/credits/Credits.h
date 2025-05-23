#include <iostream>
#include "../../core/scenes/scenes.h"
#include "core/components/components.h"
#include "core/components/transform/transform.h"

#include "../../core/fonts/fonts.h"
#include <jpsup_font.h>

#ifndef CREDITSSCENE_H
#define CREDITSSCENE_H

class CreditsScene : public Scene {
    private:

    public:
        CreditsScene();
        ~CreditsScene();

        void Start() override;
        void Update() override;
        void Render() override;
        void Events(const SDL_Event& event) override;

};

#endif