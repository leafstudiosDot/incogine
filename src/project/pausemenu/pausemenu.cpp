#include "pausemenu.h"
#include <iostream>

PauseMenu::PauseMenu() {
	square = new Square();
	square->setName("PauseMenuContainer");

	for (int i = 0; i < PauseMenuItemCount; ++i) {
		if (!pauseMenuFonts[i].setFont(_jpsup_font_data, _jpsup_font_size, 30)) {
			if (Engine::Instance(0, nullptr)->inDevMode()) {
				std::cerr << "Failed to load menu index: " << PauseMenuItemNames[i] << " font in PauseMenu::PauseMenu" << std::endl;
			}
		}
	}

	if (!pausedFont.setFont(_jpsup_font_data, _jpsup_font_size, 48)) {
		if (Engine::Instance(0, nullptr)->inDevMode()) {
			std::cerr << "Failed to load \"Paused\" font in PauseMenu::PauseMenu" << std::endl;
		}
	}
}

PauseMenu::~PauseMenu() {
	delete square;
}

void PauseMenu::Update() {
	square->setPosition(Position(0, 0, 0));
	square->setScale(Scale(Engine::Instance(0, nullptr)->GetWindowSize().width, Engine::Instance(0, nullptr)->GetWindowSize().height, 0));
	square->setColor(Color(0, 0, 0, 128));

	pausedFont.setTextContent("Paused");

	for (int i = 0; i < PauseMenuItemCount; ++i) {
		pauseMenuFonts[i].setTextContent(PauseMenuItemNames[i]);
	}
}

void PauseMenu::Render() {
	if (!this->paused) {
		return;
	}

	square->Render();

	int windowHeight = Engine::Instance(0, nullptr)->GetWindowSize().height;
	float scale = static_cast<float>(windowHeight) / 720; // 720 is base height
	
	pausedFont.renderUI(50, 50);
	pausedFont.setColor(255, 255, 255, 255);
	pausedFont.setFontScale(scale);

	for (int i = 0; i < PauseMenuItemCount; ++i) {
		int yindex = ((Engine::Instance(0, nullptr)->GetWindowSize().width / 2) / (float)720) * (100 + (i * 50));
		pauseMenuFonts[i].renderUI(50, 30 + yindex);
		pauseMenuFonts[i].setFontScale(scale);

		if (i == pauseMenuSelItem) {
			pauseMenuFonts[i].setColor(255, 255, 255, 255);
		} else {
			pauseMenuFonts[i].setColor(255, 255, 255, 100);
		}
	}

}

void PauseMenu::Events(const SDL_Event& event) {
	if (event.type == SDL_EVENT_KEY_DOWN) {
		SDL_Keycode keycode = event.key.key;
		if (this->paused) {
			switch (keycode) {
				case SDLK_ESCAPE:
					this->setPaused(false);
					break;
				case SDLK_UP:
				case SDLK_W:
					pauseMenuSelItem--;
					if (pauseMenuSelItem < 0) {
						pauseMenuSelItem = PauseMenuItemCount - 1;  // to Bottom
					}
					break;
				case SDLK_DOWN:
				case SDLK_S:
					pauseMenuSelItem++;
					if (pauseMenuSelItem >= PauseMenuItemCount) {
						pauseMenuSelItem = 0; // to Top
					}
					break;
				case SDLK_RETURN:
					switch (pauseMenuSelItem) {
						case RESUME:
							this->setPaused(false);
							break;
						case LEAVEGAME:
							Engine::Instance(0, nullptr)->SetScene(new MainScene());
							break;
					}
				default:
					break;
			}
		}
	}
}