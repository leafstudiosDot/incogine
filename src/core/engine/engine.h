#include <iostream>
#include <string>
#include <SDL2/SDL.h>

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
        
        float getfps() { return 60.0f; }

        inline static Engine* Instance() { return instance = (instance != nullptr) ? instance : new Engine(); }

    private:
        Engine() {}
        bool debugMode;
        bool devmode;
        bool isRunning;
        SDL_Window* window;
        SDL_Renderer* renderer;
        static Engine* instance;

};

#endif