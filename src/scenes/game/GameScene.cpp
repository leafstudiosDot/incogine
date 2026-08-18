#include "GameScene.h"
#include "../../core/engine/engine.h"

GameScene::GameScene() : Scene("Game Scene") {
    // Scene constructor
	pauseMenu = new PauseMenu();

    if (steamfont.setFontFile("fonts/jpsup_font.ttf", 32)) {
        if (Engine::Instance(0, nullptr)->inDevMode()) {
            std::cerr << "Failed to load menu index: \"Text\" font in GameScene::GameScene" << std::endl;
        }
    }
    
	audio = new Audio("audio/testbgm.ogg"); // resolved via AssetManager (disk first, embedded fallback)
}

GameScene::~GameScene() {
    // Scene destructor
	if (audio) {
		audio->stop();
		delete audio;
		audio = nullptr;
	}

	if (pauseMenu) {
		delete pauseMenu;
		pauseMenu = nullptr;
	}
}

void GameScene::Start() {
    // Scene initialized, calls at the initialization of the scene
    steamfont.setTextContent(u8"めいさん");
	steamfont.setColor(255, 255, 255, 255);
    audio->play(-1);
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
