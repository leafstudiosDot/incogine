#include "pausemenu.h"
#include <iostream>

PauseMenu::PauseMenu(SDL_Renderer* renderer) {
	square = new Square(renderer);
	square->setName("PauseMenuContainer");

	pausedFont.Init(renderer);
	pausedFont.setFont(reinterpret_cast<const char*>(_jpsup_font_data), _jpsup_font_size);
	pausedFont.setTextContent("Paused");

	for (int i = 0; i < PauseMenuItemCount; ++i) {
		pauseMenuFonts[i].Init(renderer);
		pauseMenuFonts[i].setFont(reinterpret_cast<const char*>(_jpsup_font_data), _jpsup_font_size);
		pauseMenuFonts[i].setTextContent(PauseMenuItemNames[i]);
	}
}

PauseMenu::~PauseMenu() {
	delete square;
}

void PauseMenu::Update() {
	square->setPosition(Position(0, 0, 0));
	square->setScale(Scale(Engine::Instance(0, nullptr)->GetWindowSize().width, Engine::Instance(0, nullptr)->GetWindowSize().height, 0));
	square->setColor(Color(0, 0, 0, 128));

	for (int i = 0; i < PauseMenuItemCount; ++i) {
		pauseMenuFonts[i].setFontSize(((Engine::Instance(0, nullptr)->GetWindowSize().width / 2) / (float)720) * 32);
	}
}

void PauseMenu::Render() {
	if (this->paused) {
		square->Render();

		int scaledFontSize = ((Engine::Instance(0, nullptr)->GetWindowSize().width / 2) / (float)720) * 64;
		pausedFont.renderUI(50, ((Engine::Instance(0, nullptr)->GetWindowSize().width / 2) / (float)720));
		pausedFont.setFontSize(scaledFontSize);

		for (int i = 0; i < MainMenuItemCount; ++i) {
			int x = 50;
			int y = (50 + ((Engine::Instance(0, nullptr)->GetWindowSize().width / 2) / (float)720) * (100 + (i * 50)));

			pauseMenuFonts[i].renderUI(x, y);

			if (i == pauseMenuSelItem) {
				pauseMenuFonts[i].setColor(255, 255, 255, 255);
			} else {
				pauseMenuFonts[i].setColor(255, 255, 255, 100);
			}
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