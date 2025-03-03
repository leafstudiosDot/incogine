#include "engine.h"
using namespace std;

Engine::Engine(int argc, char* argv[]) : sceneManager(nullptr), isRunning(true) {
    devmode = std::find(argv, argv + argc, std::string("-dev")) != argv + argc;
    debugMode = std::find(argv, argv + argc, std::string("-debug")) != argv + argc;
    skipSplash = std::find(argv, argv + argc, std::string("--skipSplash")) != argv + argc;
    sceneManager = new SceneManager(*renderer);
}

Engine* Engine::instance = nullptr;

void Engine::Init() {
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return;
    }

    char projectName[256];
    char windowName[1024];

    if (devmode) {
        //snprintf(projectName, sizeof(projectName), "%s [DEV]", PROJECT_NAME);
        snprintf(windowName, sizeof(windowName), "%s [DEV]", WINDOW_NAME);
    } else if (debugMode) {
        //snprintf(projectName, sizeof(projectName), "%s [DEBUG]", PROJECT_NAME);
        snprintf(windowName, sizeof(windowName), "%s [DEBUG]", WINDOW_NAME);
    } else {
        //snprintf(projectName, sizeof(projectName), "%s", PROJECT_NAME);
        snprintf(windowName, sizeof(windowName), "%s", WINDOW_NAME);
    }

    window = SDL_CreateWindow(windowName, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
    if (window == nullptr) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    if (TTF_Init() == -1) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    mainfont = TTF_OpenFontRW(SDL_RWFromConstMem(_mainfont_data, _mainfont_size), 1, 24);
    if (mainfont == nullptr) {
        std::cerr << "TTF_OpenFont Error: " << TTF_GetError() << std::endl;
        TTF_Quit();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    if (devmode) {
        SDL_Color devmode_color = {255, 255, 255, 128};
        devmode_surface = TTF_RenderText_Blended(mainfont, "Development Mode", devmode_color);
        if (!devmode_surface) {
            TTF_CloseFont(mainfont);
            TTF_Quit();
            SDL_DestroyWindow(window);
            SDL_Quit();
            return;
        }

        devmode_texture = SDL_CreateTextureFromSurface(renderer, devmode_surface);
        if (!devmode_texture) {
            SDL_FreeSurface(devmode_surface);
            TTF_CloseFont(mainfont);
            TTF_Quit();
            SDL_DestroyWindow(window);
            SDL_Quit();
            return;
        }
        SDL_SetTextureBlendMode(devmode_texture, SDL_BLENDMODE_BLEND);

        devmode_destRect.w = devmode_surface->w;
        devmode_destRect.h = devmode_surface->h;
    }

    isRunning = true;
    if (sceneManager != nullptr) {
        if (skipSplash) {
            sceneManager->SetScene(new MainScene());
        } else {
            sceneManager->SetScene(new Splash());
        }
    }
}

void Engine::Quit() {
    isRunning = false;
}

void Engine::Cleanup() {
    if (devmode) {
        SDL_DestroyTexture(devmode_texture);
        SDL_FreeSurface(devmode_surface);
    }
    TTF_CloseFont(mainfont);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void Engine::Update() {
    windowSize.width = windowWidth;
    windowSize.height = windowHeight;

    if (sceneManager != nullptr) {
        sceneManager->UpdateScene();
    }
}

void Engine::Render() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    if (devmode) {
        devmode_destRect.x = windowWidth - devmode_destRect.w;
        devmode_destRect.y = windowHeight - devmode_destRect.h;
        SDL_RenderCopy(renderer, devmode_texture, nullptr, &devmode_destRect);
    }

    if (sceneManager != nullptr) {
        sceneManager->RenderScene();
    }

    SDL_RenderPresent(renderer);
}

void Engine::Events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            Quit();
        }

        if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                int width = event.window.data1;
                int height = event.window.data2;

                if (width < MIN_WIDTH) width = MIN_WIDTH;
                if (height < MIN_HEIGHT) height = MIN_HEIGHT;

                if (width / 16 != height / 9) {
                    height = width * 9 / 16;
                }

                SDL_SetWindowSize(window, width, height);

                windowWidth = event.window.data1;
                windowHeight = event.window.data2;
            }

            switch (event.window.event) {
                // Core Window SDL events
                case SDL_WINDOWEVENT_FOCUS_GAINED:
                    // Window focused at Incogine (Windows/macOS/Linux)
                    winFocused = true;
                    break;
                case SDL_WINDOWEVENT_FOCUS_LOST:
                    // Window isn't focused at Incogine (Windows/macOS/Linux)
                    winFocused = false;
                    break;
                case SDL_WINDOWEVENT_MINIMIZED:
                    // Incogine is minimized (Windows/macOS/Linux)
                    inBackground = true;
                    break;
                case SDL_WINDOWEVENT_MAXIMIZED:
                    // Incogine is restored (Windows/macOS/Linux)
                    inBackground = false;
                    break;
                case SDL_WINDOWEVENT_CLOSE:
                    // Incogine is about to terminate (Windows/macOS/Linux)
                    Quit();
                    break;
            }
        }

        switch(event.type) {
            case SDL_KEYDOWN:
				if (event.key.keysym.sym == SDLK_F12 && winFocused) { // F12 key
                    ToggleFullscreen();
                }
                break;

			// Core SDL events
            case SDL_APP_WILLENTERBACKGROUND:
                // Incogine is going to background (Android/iOS)
                inBackground = true;
                break;
            case SDL_APP_DIDENTERFOREGROUND:
                // Incogine is coming to foreground (Android/iOS)
                inBackground = false;
                break;
            case SDL_APP_TERMINATING:
                // Incogine is about to terminate (Android/iOS)
                Quit();
                break;
            case SDL_APP_LOWMEMORY:
                // Low memory warning (Android/iOS)
                break;
        }
    }
}

void Engine::SetScene(Scene* scene) {
    sceneManager->SetScene(scene);
}

void Engine::ToggleFullscreen() {
    fullScreenMode = !fullScreenMode;
    if (fullScreenMode) {
		windowedHeight = windowHeight;
		windowedWidth = windowWidth;
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    } else {
        SDL_SetWindowSize(window, windowedWidth, windowedHeight);
        SDL_SetWindowFullscreen(window, 0);
    }
}