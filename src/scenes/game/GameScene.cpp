#include "GameScene.h"
#include "../../core/engine/engine.h"

GameScene::GameScene() : Scene("Game Scene") {
    // Scene constructor
	//pauseMenu = new PauseMenu(Engine::Instance(0, nullptr)->GetRenderer());
    //steamfont.Init(Engine::Instance(0, nullptr)->GetRenderer());

}

GameScene::~GameScene() {
    // Scene destructor
}

void GameScene::Start() {
    // Scene initialized, calls at the initialization of the scene
    /*steamfont.setColor(255, 255, 255, 255);

    steamfont.setFont(reinterpret_cast<const char*>(_jpsup_font_data), _jpsup_font_size);
    steamfont.setTextContent(L"めいさん");

    steamfont.setFontSize(32);
    if (!steamfont.GetFont()) {
        std::cerr << "Failed to load font in GameScene::Start" << std::endl;
    }*/
}

void GameScene::Update() {
    // Scene update, calls every frame
	pauseMenu->Update();
}

void GameScene::Render() {
    // Scene render

    //steamfont.renderUI((Engine::Instance(0, nullptr)->GetWindowSize().width / 4), (Engine::Instance(0, nullptr)->GetWindowSize().height / 2));
    int scaledFontSize = ((Engine::Instance(0, nullptr)->GetWindowSize().width / 2) / (float)720) * 48;
    //steamfont.setFontSize(scaledFontSize);

	pauseMenu->Render();
}

void GameScene::Events(const SDL_Event& event) {
    // Scene events
    if (pauseMenu->getPaused()) {
        pauseMenu->Events(event);
    } else {
        if (event.type == SDL_EVENT_KEY_DOWN) {
            SDL_Keycode keycode = event.key.key;
            switch (keycode) {
            case SDLK_ESCAPE:
                if (!pauseMenu->getPaused()) {
                    pauseMenu->setPaused(true);
                }
                break;
            default:
                break;
            }
        }
    }
}
