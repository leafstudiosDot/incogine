#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <SDL2/SDL.h>
#if defined(__APPLE__) && defined(__IPHONEOS__)
    #include <SDL2/SDL_main.h>
#endif
#include <SDL_ttf.h>
#include <SDL_image.h>
//#include <fbxsdk.h>
//#include "console/console.h"
#include "version.h"

#include "../components/components.h"

#include "../../fonts/main_font.h"

#include "../scenes/scenes.h"

#include "../../scenes/splash/splash.h"
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

        void SetScene(Scene* scene);

        inline bool inDebugMode() { return debugMode; }
        inline bool inDevMode() { return devmode; }
        inline bool running() { return isRunning; }
        inline bool checkInBackground() { return inBackground; }

        inline SDL_Window* GetWindow() { return window; }
        inline SDL_Renderer* GetRenderer() { return renderer; }
        inline TTF_Font* GetMainFont() { return mainfont; }
        
        float getfps() { return 60.0f; }

        inline static Engine* Instance(int argc, char* argv[]) { return instance = (instance != nullptr) ? instance : new Engine(argc, argv); }

    protected:
        bool debugMode;
        bool devmode;
        bool isRunning;
        bool inBackground;
        SDL_Window* window;
        SDL_Renderer* renderer;
        TTF_Font* mainfont;

        SDL_Surface* devmode_surface;
        SDL_Texture* devmode_texture;
        SDL_Rect devmode_destRect;

        int windowWidth = SCREEN_WIDTH;
        int windowHeight = SCREEN_HEIGHT;
        SceneManager sceneManager;

    private:
        Engine(int argc, char* argv[]);

        static Engine* instance;

};

#endif