#include "engine.h"
using namespace std;

Engine* Engine::instance = nullptr;

void Engine::Init() {
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return;
    }

    char projectName[256];

    if (devmode) {
        snprintf(projectName, sizeof(projectName), "%s [DEV]", PROJECT_NAME);
    } else if (debugMode) {
        snprintf(projectName, sizeof(projectName), "%s [DEBUG]", PROJECT_NAME);
    } else {
        snprintf(projectName, sizeof(projectName), "%s", PROJECT_NAME);
    }

    window = SDL_CreateWindow(projectName, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT,
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
    sceneManager.SetScene(new Splash());
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
    sceneManager.UpdateScene();
}

void Engine::Render() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    if (devmode) {
        devmode_destRect.x = windowWidth - devmode_destRect.w;
        devmode_destRect.y = windowHeight - devmode_destRect.h;
        SDL_RenderCopy(renderer, devmode_texture, nullptr, &devmode_destRect);
    }

    sceneManager.RenderScene();

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
        }
    }
}

void Engine::SetScene(Scene* scene) {
    sceneManager.SetScene(scene);
}