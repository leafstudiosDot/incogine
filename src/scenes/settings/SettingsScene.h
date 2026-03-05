#include <iostream>
#include <cstddef>
#include <vector>
#include <string>
#include <functional>
#include "../../core/scenes/scenes.h"
#include "../../core/fonts/fonts.h"
#include <jpsup_font.h>

#ifndef SETTINGS_SCENE_H
#define SETTINGS_SCENE_H


struct SubMenuItem {
    std::string name;
    std::function<void()> action;
};

struct MenuItem {
    std::string name;
    std::vector<SubMenuItem> subItems;
};

static const std::vector<MenuItem> SettingsMenu = {

    { "Video", {
        { "Resolution", []() { /*Function()*/ } },
        { "Fullscreen", []() { /*Function()*/ } }
    }},

    { "Graphics", {
        { "VSync", []() { /*Function()*/ } },
        { "Anti-Aliasing", []() { /*Function()*/ } }
    }},

    { "Audio", {
        { "Master Volume", []() { /*Function()*/ } },
        { "Music Volume", []() { /*Function()*/ } },
	    { "Sound Effects Volume", []() { /*Function()*/ } }
    }},

    { "Controls", {
        { "Key Bindings", []() { /*Function()*/ } },
        { "Mouse Sensitivity", []() { /*Function()*/ } }
    }},

    { "Back", {} }
};

class SettingsScene : public Scene {
    private:
        std::size_t menuSelected = 0;
		std::size_t subMenuSelected = 0;
		bool inSubMenu = false;
        Font headerFont;
		std::vector<Font> menuFonts;
		std::vector<Font> subMenuFonts;
    public:
        SettingsScene();
        ~SettingsScene();

        void Start() override;
        void Update() override;
        void Render() override;
        void Events(const SDL_Event& event) override;
};

#endif