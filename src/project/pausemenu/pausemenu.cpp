#include "pausemenu.h"
#include <iostream>

PauseMenu::PauseMenu(SDL_Renderer* renderer) {
	square = new Square(renderer);
	square->setName("PauseMenuContainer");
	pausedFont.Init(renderer);
	pausedFont.setFont(reinterpret_cast<const char*>(_jpsup_font_data), _jpsup_font_size);
	pausedFont.setTextContent("Paused");
}

PauseMenu::~PauseMenu() {
	delete square;
}

void PauseMenu::Update() {
	square->setPosition(Position(0, 0, 0));
	square->setScale(Scale(Engine::Instance(0, nullptr)->GetWindowSize().width, Engine::Instance(0, nullptr)->GetWindowSize().height, 0));
	square->setColor(Color(0, 0, 0, 128));
}

void PauseMenu::Render() {
	if (this->paused) {
		square->Render();

		int x = 50;
		int y = ((Engine::Instance(0, nullptr)->GetWindowSize().width / 2) / (float)720);

		pausedFont.renderUI(x, y);
		int scaledFontSize = ((Engine::Instance(0, nullptr)->GetWindowSize().width / 2) / (float)720) * 64;
		pausedFont.setFontSize(scaledFontSize);
	}
}

void PauseMenu::Events(const SDL_Event& event) {
	if (event.type == SDL_EVENT_KEY_DOWN) {
		SDL_Keycode keycode = event.key.key;
		if (this->paused) {

		}
	}
}