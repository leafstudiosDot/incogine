#include <iostream>
#include "../../core/engine/engine.h"
#include "../../core/components/components.h"
#include "../../core/objects/objects.h"
#include "../../core/objects/square/square.h"
#include "../../core/fonts/fonts.h"
#include <jpsup_font.h>

#ifndef PAUSEMENU_H
#define PAUSEMENU_H

enum PauseMenuItems {
    RESUME,
    LEAVEGAME
};

static constexpr const char* PauseMenuItemNames[] = {
    "Resume",
    "Leave Game"
};
static constexpr int PauseMenuItemCount = sizeof(PauseMenuItemNames) / sizeof(PauseMenuItemNames[0]);

class PauseMenu {
    private:
        bool paused = false;
        SDL_Renderer* renderer;
        Square* square = nullptr;
        Font pausedFont;
        Font pauseMenuFonts[PauseMenuItemCount];
        int pauseMenuSelItem = 0;
    public:
        PauseMenu(SDL_Renderer* renderer);
        ~PauseMenu();

		inline bool getPaused() const {
			return paused;
		}

		inline void setPaused(bool p) {
			paused = p;
		}

        void Update();
        void Render();
		void Events(const SDL_Event& event);
};

#endif