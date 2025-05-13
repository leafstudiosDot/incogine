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
    /*if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return;
    }*/

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


	if (devmode) {
		cout << "Creating Window..." << endl;
	}

    if (!SDL_CreateWindowAndRenderer(windowName, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "WinAndRenErr: %s", SDL_GetError());
        SDL_Quit();
        return;
    }

    if (devmode) {
        cout << "Linking Renderer..." << endl;
    }

    // VSync always on
    if (!SDL_SetRenderVSync(renderer, 1))
    {
        SDL_Log("Could not enable VSync! SDL error: %s\n", SDL_GetError());
    }

    if (!TTF_Init()) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    mainfont = TTF_OpenFontIO(SDL_IOFromConstMem(_mainfont_data, _mainfont_size), 1, 24);
    if (mainfont == nullptr) {
        std::cerr << "TTF_OpenFont Error: " << SDL_GetError() << std::endl;
        TTF_Quit();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    if (devmode) {
        fpsfont = TTF_OpenFontIO(SDL_IOFromConstMem(_mainfont_data, _mainfont_size), 1, 15);
        if (fpsfont == nullptr) {
            std::cerr << "TTF_OpenFont Error: " << SDL_GetError() << std::endl;
            TTF_Quit();
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return;
        }

        SDL_Color devmode_color = {255, 255, 255, 128};
        devmode_surface = TTF_RenderText_Blended(mainfont, "Development Mode", 0, devmode_color);
        if (!devmode_surface) {
            TTF_CloseFont(mainfont);
            TTF_Quit();
            SDL_DestroyWindow(window);
            SDL_Quit();
            return;
        }

        devmode_texture = SDL_CreateTextureFromSurface(renderer, devmode_surface);
        if (!devmode_texture) {
            SDL_DestroySurface(devmode_surface);
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

    if (devmode) {
        cout << "Scene loading..." << endl;
    }
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
        SDL_DestroySurface(devmode_surface);
        if (dbfps_texture != nullptr) {
            SDL_DestroyTexture(dbfps_texture);
            dbfps_texture = nullptr;
        }
        if (dbfps_surface != nullptr) {
            dbfps_surface = nullptr;
            SDL_DestroySurface(dbfps_surface);
        }
        TTF_CloseFont(fpsfont);
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

    currentTime = SDL_GetTicks();
    frameDelta = currentTime - lastTime;
    deltaTime = frameDelta;
    lastTime = currentTime;

    if (sceneManager != nullptr) {
        sceneManager->UpdateScene();
    }

    frameCount++;
    static double accumTime = 0.0;
    accumTime += frameDelta;
    if (accumTime >= 1000) {
        fps = frameCount / (accumTime / 1000.0f);
        frameCount = 0;
        accumTime = 0.0;
        //cout << "FPS: " << fps << endl;
    }
}

void Engine::Render() {
    //SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    if (sceneManager != nullptr) {
        sceneManager->RenderScene();
    }

    if (devmode) {
        devmode_destRect.x = windowWidth - devmode_destRect.w;
        devmode_destRect.y = windowHeight - devmode_destRect.h;
        SDL_RenderTexture(renderer, devmode_texture, nullptr, &devmode_destRect);

        // FPS UI
        SDL_Color dbfps_color = { 255, 255, 255, 128 };
        dbfps_surface = TTF_RenderText_Blended(fpsfont, (std::string(fpsConvert(getfps())) + "fps").c_str(), 0, dbfps_color);
        if (!dbfps_surface) {
            TTF_CloseFont(fpsfont);
            TTF_Quit();
            SDL_DestroyWindow(window);
            SDL_Quit();
            return;
        }

        dbfps_texture = SDL_CreateTextureFromSurface(renderer, dbfps_surface);
        if (!dbfps_texture) {
            SDL_DestroySurface(dbfps_surface);
            TTF_CloseFont(fpsfont);
            TTF_Quit();
            SDL_DestroyWindow(window);
            SDL_Quit();
            return;
        }
        SDL_SetTextureBlendMode(dbfps_texture, SDL_BLENDMODE_BLEND);

        dbfps_destRect.w = dbfps_surface->w;
        dbfps_destRect.h = dbfps_surface->h;

		dbfps_destRect.x = windowWidth - dbfps_destRect.w - 15;
		dbfps_destRect.y = 15;
		SDL_RenderTexture(renderer, dbfps_texture, nullptr, &dbfps_destRect);
        SDL_DestroyTexture(dbfps_texture);
        SDL_DestroySurface(dbfps_surface);
    }

    SDL_RenderPresent(renderer);
}

void Engine::Events() {
    int winWidth;
    int winHeight;

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            Quit();
        }

        if (sceneManager != nullptr) {
            sceneManager->EventScene(event);
        }

        switch(event.type) {
            case SDL_EVENT_WINDOW_RESIZED:
                winWidth = event.window.data1;
                winHeight = event.window.data2;

                if (winWidth < MIN_WIDTH) winWidth = MIN_WIDTH;
                if (winHeight < MIN_HEIGHT) winHeight = MIN_HEIGHT;

                if (winWidth / 16 != winHeight / 9) {
                    winHeight = winWidth * 9 / 16;
                }

                SDL_SetWindowSize(window, winWidth, winHeight);

                windowWidth = event.window.data1;
                windowHeight = event.window.data2;
                break;

            case SDL_EVENT_KEY_DOWN:
				if (event.key.key == SDLK_F11 && winFocused) { // F11 key
                    ToggleFullscreen();
                }
                break;

			// Core SDL events
            case SDL_EVENT_WILL_ENTER_BACKGROUND:
                // Incogine is going to background (Android/iOS)
                inBackground = true;
                break;
            case SDL_EVENT_DID_ENTER_FOREGROUND:
                // Incogine is coming to foreground (Android/iOS)
                inBackground = false;
                break;
            case SDL_EVENT_TERMINATING:
                // Incogine is about to terminate (Android/iOS)
                Quit();
                break;
            case SDL_EVENT_LOW_MEMORY:
                // Low memory warning (Android/iOS)
                break;

            // Core Window SDL events
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                // Window focused at Incogine (Windows/macOS/Linux)
                winFocused = true;
                break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                // Window isn't focused at Incogine (Windows/macOS/Linux)
                winFocused = false;
                break;
            case SDL_EVENT_WINDOW_MINIMIZED:
                // Incogine is minimized (Windows/macOS/Linux)
                inBackground = true;
                break;
            case SDL_EVENT_WINDOW_MAXIMIZED:
                // Incogine is restored (Windows/macOS/Linux)
                inBackground = false;
                break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                // Incogine is about to terminate (Windows/macOS/Linux)
                Quit();
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
        SDL_SetWindowFullscreen(window, true);
    } else {
        //SDL_SetWindowSize(window, windowedWidth, windowedHeight);
        SDL_SetWindowFullscreen(window, false);
    }
}

char* Engine::fpsConvert(float fps) {
    static char buffer[32];
    if (fps == static_cast<int>(fps)) {
        snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(fps));
    } else {
        snprintf(buffer, sizeof(buffer), "%.2f", fps);
    }
    return buffer;
}