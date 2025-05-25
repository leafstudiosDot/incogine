#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <locale>
#include <codecvt>
#include <GL/glew.h>
#include <SDL/SDL.h>
#if defined(__APPLE__) && defined(__IPHONEOS__)
    #include <SDL2/SDL_main.h>
#endif
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <SDL_opengl.h>
#include <SDL_mixer.h>
//#include <fbxsdk.h>
//#include "console/console.h"
#include "version.h"
#include "math.h"

#include "../components/components.h"

#include "../fonts/fonts.h"
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

struct WindowSize {
    int width;
    int height;
};

class Engine {
    public:
        void Init();
        void Quit();
        void Cleanup();

        void Update();
        void Render();
        void Events();

        void SetScene(Scene* scene);

        void ToggleFullscreen();

        inline bool inDebugMode() { return debugMode; }
        inline bool inDevMode() { return devmode; }
        inline bool running() { return isRunning; }
        inline bool checkInBackground() { return inBackground; }
		inline bool inFullScreen() { return fullScreenMode; }
        inline WindowSize GetWindowSize() { return windowSize; };

        inline SDL_Window* GetWindow() { return window; }
		inline SDL_GLContext GetGLContext() { return glcontext; }
        inline SDL_Event GetEventProvider() { return event; };
        
		inline double getDeltaTime() { return deltaTime; }
        float getfps() { return fps; }

        inline static Engine* Instance(int argc, char* argv[]) { return instance = (instance != nullptr) ? instance : new Engine(argc, argv); }

    protected:
        bool debugMode;
        bool devmode;
        bool isRunning = false;
        bool inBackground;
        bool skipSplash;
        bool winFocused;
        bool fullScreenMode = false;
        SDL_Window* window;
        SDL_Event event;
        SDL_GLContext glcontext;
        float fps = 0.0f;
        Font devmode_font;
        Font fpstext_font;

        // FPS
        Uint64 currentTime;
        double deltaTime = 0.0f;
		double frameDelta = 0.0f;
        Uint64 lastTime = SDL_GetTicks();
        Uint64 frameCount = 0;

        int windowWidth = SCREEN_WIDTH;
        int windowHeight = SCREEN_HEIGHT;

        // After full screen toggle
		int windowedWidth = SCREEN_WIDTH;
		int windowedHeight = SCREEN_HEIGHT;

        WindowSize windowSize = {windowWidth, windowHeight};
        SceneManager* sceneManager;

    private:
        Engine(int argc, char* argv[]);

        static Engine* instance;

        char* fpsConvert(float fps);

};

#endif