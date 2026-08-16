#include "SettingsScene.h"
#include "../../core/engine/engine.h"
#include "../../scenes/MainScene.h"

SettingsScene::SettingsScene() : Scene("Settings") {
    // Scene constructor
    menuFonts.resize(SettingsMenu.size());

    if (!headerFont.setFont(_jpsup_font_data, _jpsup_font_size, 48)) {
        if (Engine::Instance(0, nullptr)->inDevMode()) {
            std::cerr << "Failed to load \"Settings\" font in SettingsScene::SettingsScene" << std::endl;
        }
    }

    for (int i = 0; i < SettingsMenu.size(); ++i) {
        if (!menuFonts[i].setFont(_jpsup_font_data, _jpsup_font_size, 30)) {
            if (Engine::Instance(0, nullptr)->inDevMode()) {
                std::cerr << "Failed to load menu index: " << SettingsMenu[i].name << " font in SettingsScene::SettingsScene" << std::endl;
            }
        }
    }
}

SettingsScene::~SettingsScene() {
    // Scene destructor
}

void SettingsScene::Start() {
    // Scene initialized, calls at the initialization of the scene
}

void SettingsScene::Update() {
    // Scene update, calls every frame
    headerFont.setTextContent("Settings");

    for (int i = 0; i < SettingsMenu.size(); ++i) {
        menuFonts[i].setTextContent(SettingsMenu[i].name);
    }
}

void SettingsScene::Render() {
    // Scene render
    int windowWidth = Engine::Instance(0, nullptr)->GetWindowSize().width;
    int windowHeight = Engine::Instance(0, nullptr)->GetWindowSize().height;
	float scale = static_cast<float>(windowHeight) / 720; // 720 is base height
    float widthScale = static_cast<float>(windowWidth) / 1280.0f; // 1280 is base width
    int subMenuX = static_cast<int>(200.0f * widthScale);

    if (subMenuX < 200) {
        subMenuX = 200;
    }

    headerFont.renderUI(50, 25);
    headerFont.setColor(255, 255, 255, 255);
    headerFont.setFontScale(scale);

    for (int i = 0; i < SettingsMenu.size(); ++i) {
        int yindex = ((windowWidth / 2) / (float)720) * (100 + (i * 50));

		menuFonts[i].setFontScale(scale);

		if (i == menuSelected) {
			menuFonts[i].setColor(255, 255, 255, 255);
		} else {
            menuFonts[i].setColor(255, 255, 255, 100);
        }

		menuFonts[i].renderUI(50, yindex);
    }

    if (inSubMenu) {
        const auto& subItems = SettingsMenu[menuSelected].subItems;

        // Prevent out-of-range on subMenuFonts access.
        if (subMenuFonts.size() != subItems.size()) {
            std::size_t oldSize = subMenuFonts.size();
            subMenuFonts.resize(subItems.size());

            for (std::size_t i = oldSize; i < subMenuFonts.size(); ++i) {
                if (!subMenuFonts[i].setFont(_jpsup_font_data, _jpsup_font_size, 30)) {
                    if (Engine::Instance(0, nullptr)->inDevMode()) {
                        std::cerr << "Failed to load submenu index: " << i << " font in SettingsScene::Render" << std::endl;
                    }
                }
            }
        }

        if (subItems.empty()) {
            subMenuSelected = 0;
            return;
        }

        if (subMenuSelected < 0 || subMenuSelected >= static_cast<int>(subItems.size())) {
            subMenuSelected = 0;
        }

        for (std::size_t i = 0; i < subItems.size(); ++i) {
            int yindex = ((windowWidth / 2) / 720.0f) * (100 + (static_cast<int>(i) * 50));

            subMenuFonts[i].setFontScale(scale);

            if (static_cast<int>(i) == subMenuSelected) {
                subMenuFonts[i].setColor(255, 255, 255, 255);
            }
            else {
                subMenuFonts[i].setColor(255, 255, 255, 100);
            }

            subMenuFonts[i].setTextContent(subItems[i].name);
            subMenuFonts[i].renderUI(subMenuX, yindex);
        }
    }
}

void SettingsScene::Events(const SDL_Event& event) {
    // Handle events specific to the settings scene
    if (event.type == SDL_EVENT_KEY_DOWN) {
		SDL_Keycode keycode = event.key.key;
        const std::size_t menuCount = SettingsMenu.size();
        const std::size_t subCount = SettingsMenu[menuSelected].subItems.size();

        switch (keycode) {
            case SDLK_UP:
            case SDLK_W:
                if (inSubMenu) {
                    if (subCount > 0) {
                        if (subMenuSelected == 0) {
                            subMenuSelected = subCount - 1;
                        } else {
                            --subMenuSelected;
                        }
                    }
                } else {
                    if (menuCount > 0)
                    {
                        if (menuSelected == 0) {
                            menuSelected = menuCount - 1;
                        } else {
                            --menuSelected;
                        }
                    }
                }
				// std::cout << "Menu selected: " << menuSelected << ", Submenu selected: " << subMenuSelected << std::endl;
                break;
			case SDLK_DOWN:
            case SDLK_S:
                if (inSubMenu) {
                    subMenuSelected++;
                    if (subMenuSelected >= subCount) {
                        subMenuSelected = 0;
                    }
                } else {
                    menuSelected++;
                    if (menuSelected >= menuCount) {
                        menuSelected = 0;
                    }
				}
                break;
            case SDLK_RETURN:
                if (!inSubMenu) {
                    switch (menuSelected) {
                        case 0: // Video
                        case 1: // Graphics
                        case 2: // Audio
                        case 3: // Controls
                            inSubMenu = true;
                            subMenuSelected = 0;
                            break;
                        case 4: // Back
                            Engine::Instance(0, nullptr)->SetScene(new MainScene());
                            break;
                        default:
                            break;
					}
                } else {
                    if (subMenuSelected < SettingsMenu[menuSelected].subItems.size())
                    {
                        if (SettingsMenu[menuSelected].subItems[subMenuSelected].action) {
                            SettingsMenu[menuSelected].subItems[subMenuSelected].action();
                        }
                    }
                }
                break;
            case SDLK_ESCAPE:
                if (inSubMenu) {
                    inSubMenu = false;
                } else {
                    Engine::Instance(0, nullptr)->SetScene(new MainScene());
				}
                break;
        }
    }
}
