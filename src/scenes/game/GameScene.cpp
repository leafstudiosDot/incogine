#include "GameScene.h"
#include "../../core/engine/engine.h"

GameScene::GameScene() : Scene("Game Scene") {
    // Scene constructor
	pauseMenu = new PauseMenu();

    if (steamfont.setFont(_jpsup_font_data, _jpsup_font_size, 32)) {
        if (Engine::Instance(0, nullptr)->inDevMode()) {
            std::cerr << "Failed to load menu index: \"Text\" font in GameScene::GameScene" << std::endl;
        }
    }
}

GameScene::~GameScene() {
    // Scene destructor
}

void GameScene::Start() {
    // Scene initialized, calls at the initialization of the scene
    steamfont.setTextContent(u8"めいさん");
	steamfont.setColor(255, 255, 255, 255);
}

void GameScene::Update() {
    // Scene update, calls every frame
	pauseMenu->Update();
}

void GameScene::Render() {
    // Scene render
    int windowHeight = Engine::Instance(0, nullptr)->GetWindowSize().height;
    float scale = static_cast<float>(windowHeight) / 720; // 720 is base height
    steamfont.setFontScale(scale);

    steamfont.renderUI((Engine::Instance(0, nullptr)->GetWindowSize().width / 4), (Engine::Instance(0, nullptr)->GetWindowSize().height / 2));

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
