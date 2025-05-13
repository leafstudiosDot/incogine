#include <iostream>
#include "../../core/engine/engine.h"
#include "../../core/components/components.h"
#include "../../core/objects/objects.h"
#include "../../core/objects/square/square.h"
#include "../../core/fonts/fonts.h"
#include "../../fonts/jpsup_font.h"

#ifndef PAUSEMENU_H
#define PAUSEMENU_H

class PauseMenu {
    private:
        bool paused = false;
        SDL_Renderer* renderer;
        Square* square = nullptr;
        Font pausedFont;

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