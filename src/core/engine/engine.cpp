#include "engine.h"
using namespace std;

Engine::Engine(int argc, char* argv[]) : sceneManager(nullptr), isRunning(true) {
    devmode = std::find(argv, argv + argc, std::string("-dev")) != argv + argc;
    debugMode = std::find(argv, argv + argc, std::string("-debug")) != argv + argc;
    skipSplash = std::find(argv, argv + argc, std::string("--skipSplash")) != argv + argc;
    sceneManager = new SceneManager();
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

    window = SDL_CreateWindow(windowName, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "WinErr: %s", SDL_GetError());
        SDL_Quit();
        return;
    }

    glcontext = SDL_GL_CreateContext(window);
    if (!glcontext) {
        std::cerr << "Failed to initialize OpenGL loader" << std::endl;
        SDL_GL_DestroyContext(glcontext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    if (!TTF_Init()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "FontInitErr: %s", SDL_GetError());
        SDL_GL_DestroyContext(glcontext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    int flags = MIX_INIT_OGG;
    if ((Mix_Init(flags) & flags) != flags) {
        SDL_Log("Mix_Init failed: %s", SDL_GetError());
        // fall through if you don't need OGG, but if you do, bail out
    }

    if (devmode) {
        cout << "Linking Renderer..." << endl;
    }

    // VSync always on
    SDL_GL_SetSwapInterval(1);

    if (devmode) {
        if (!devmode_font.setFont(_mainfont_data, _mainfont_size, 24)) {
            std::cerr << "Failed to load \"Development Mode\" font in Engine::Init" << std::endl;
        }
        devmode_font.setColor(255, 255, 255, 128);
        devmode_font.setTextContent("Development Mode");

        // FPS UI
		if (!fpstext_font.setFont(_mainfont_data, _mainfont_size, 14)) {
			std::cerr << "Failed to load \"FPS\" font in Engine::Init" << std::endl;
		}
		fpstext_font.setColor(255, 255, 255, 128);
		fpstext_font.setTextContent("");
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
    TTF_Quit();
	Mix_Quit();
    SDL_GL_DestroyContext(glcontext);
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

    if (sceneManager) {
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
    glViewport(0, 0, windowSize.width, windowSize.height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (sceneManager != nullptr) {
        sceneManager->RenderScene();
    }

    if (devmode) {
        FontSize devmode_font_sz = devmode_font.getSize();
        devmode_font.renderUI((windowWidth - devmode_font_sz.width), (windowHeight - devmode_font_sz.height));

		// FPS UI
		fpstext_font.setTextContent((std::string(fpsConvert(getfps())) + " fps").c_str());
		FontSize devfps_font_sz = fpstext_font.getSize();
		fpstext_font.renderUI((windowWidth - devfps_font_sz.width - 15), 15);
    }

    SDL_GL_SwapWindow(window);
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