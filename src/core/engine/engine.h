#include <iostream>
#include <string>
#include <SDL2/SDL.h>
#include <SDL_ttf.h>
//#include <fbxsdk.h>

#include "../components/components.h"

#include "../../fonts/main_font.h"

#include "../scenes/scenes.h"
#include "../../scenes/MainScene.h"

#define MIN_WIDTH 1280
#define MIN_HEIGHT 720
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

#ifndef ENGINE_H
#define ENGINE_H

class Engine {
    public:
        void Init();
        void Quit();
        void Cleanup();

        void Update();
        void Render();
        void Events();

        inline bool inDebugMode() { return debugMode; }
        inline bool inDevMode() { return devmode; }
        inline bool running() { return isRunning; }

        inline SDL_Window* GetWindow() { return window; }
        inline SDL_Renderer* GetRenderer() { return renderer; }
        inline TTF_Font* GetMainFont() { return mainfont; }
        
        float getfps() { return 60.0f; }

        inline static Engine* Instance(int argc, char* argv[]) { return instance = (instance != nullptr) ? instance : new Engine(argc, argv); }

    private:
        Engine(int argc, char* argv[]) {
            devmode = std::find(argv, argv + argc, std::string("-dev")) != argv + argc;
            debugMode = std::find(argv, argv + argc, std::string("-debug")) != argv + argc;
        }
        bool debugMode;
        bool devmode;
        bool isRunning;
        SDL_Window* window;
        SDL_Renderer* renderer;
        TTF_Font* mainfont;
        static Engine* instance;

        SDL_Surface* devmode_surface;
        SDL_Texture* devmode_texture;
        SDL_Rect devmode_destRect;

        int windowWidth = SCREEN_WIDTH;
        int windowHeight = SCREEN_HEIGHT;
        SceneManager sceneManager;

};

#endif