#include "MainScene.h"
#include "core/engine/engine.h"
#include "scenes/game/GameScene.h"

MainScene::MainScene() : Scene("Main Scene") {
    // Scene constructor
    for (int i = 0; i < MainMenuItemCount; ++i) {
        if (!menuFonts[i].setFont(_jpsup_font_data, _jpsup_font_size, 30)) {
            if (Engine::Instance(0, nullptr)->inDevMode()) {
                std::cerr << "Failed to load menu index: " << MainMenuItemNames[i] << " font in MainScene::MainScene" << std::endl;
            }
        }
    }

    versionFont.setFont(_jpsup_font_data, _jpsup_font_size, 14);
}

MainScene::~MainScene() {
    // Scene destructor
}

void MainScene::Start() {
    // Scene initialized, calls at the initialization of the scene
    // Engine::Instance(0, nullptr)->SetScene(new GameScene());
}

void MainScene::Update() {
    // Scene update, calls every frame
    for (int i = 0; i < MainMenuItemCount; ++i) {
		menuFonts[i].setTextContent(MainMenuItemNames[i]);
    }

    // Version info
    versionFont.setTextContent("v" + std::string(PROJECT_VERSION));
}

void MainScene::Render() {
    // Scene render
    int windowHeight = Engine::Instance(0, nullptr)->GetWindowSize().height;
    float scale = static_cast<float>(windowHeight) / 720; // 720 is base height
    float scaleR = versionFont.getSize().width + (static_cast<float>(windowHeight) / 720); // 720 is base height
    float scaleRH = versionFont.getSize().height + (static_cast<float>(windowHeight) / 720); // 720 is base height

    for (int i = 0; i < MainMenuItemCount; ++i) {
        // Example: render each menu item at different Y position
        int yindex = ((Engine::Instance(0, nullptr)->GetWindowSize().width / 2) / (float)720) * (100 + (i * 50));

        menuFonts[i].setFontScale(scale);

		if (i == menuSelItem) {
			menuFonts[i].setColor(255, 255, 255, 255);
		} else {
			menuFonts[i].setColor(255, 255, 255, 100);
		}

		menuFonts[i].renderUI(50, yindex);
    }

    // Render version info
    versionFont.renderUI(Engine::Instance(0, nullptr)->GetWindowSize().width - scaleR - 5, Engine::Instance(0, nullptr)->GetWindowSize().height - scaleRH - 5);
    versionFont.setColor(255, 255, 255, 255);
	versionFont.setFontScale(scale);
}

void MainScene::Events(const SDL_Event& event) {
	// Scene events
    if (event.type == SDL_EVENT_KEY_DOWN) {
        SDL_Keycode keycode = event.key.key;
        switch (keycode) {
            case SDLK_UP:
            case SDLK_W:
                menuSelItem--;
                if (menuSelItem < 0) {
                    menuSelItem = MainMenuItemCount - 1;  // to Bottom
                }
                break;
            case SDLK_DOWN:
            case SDLK_S:
                menuSelItem++;
                if (menuSelItem >= MainMenuItemCount) {
                    menuSelItem = 0; // to Top
                }
                break;
            case SDLK_RETURN:
				switch (menuSelItem) {
				case NEWGAME:
					Engine::Instance(0, nullptr)->SetScene(new GameScene());
					break;
				case LOADGAME:

					break;
				case SETTINGS:

					break;
                case CREDITS:

                    break;
				case EXIT:
					Engine::Instance(0, nullptr)->Quit();
					break;
				}
                break;
            default:
                break;
        }
    }
}
