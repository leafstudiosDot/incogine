#include <iostream>
#include "../core/scenes/scenes.h"

#include "../core/fonts/fonts.h"
#include <jpsup_font.h>

#ifndef MAINSCENE_H
#define MAINSCENE_H

enum MainMenuItems {
    NEWGAME,
    LOADGAME,
    SETTINGS,
    EXIT
};

static constexpr const char* MainMenuItemNames[] = {
    "New Game",
    "Load Game",
    "Settings",
    "Exit Game"
};
static constexpr int MainMenuItemCount = sizeof(MainMenuItemNames) / sizeof(MainMenuItemNames[0]);

class MainScene : public Scene {
    private:
        int menuSelItem = 0;
        Font menuFonts[MainMenuItemCount];

    public:
        MainScene();
        ~MainScene();

        void Start() override;
        void Update() override;
        void Render() override;
        void Events(const SDL_Event& event) override;
};

#endif