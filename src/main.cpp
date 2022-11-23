#include "core/core.hpp"
Core* core = nullptr;
SDL_Window* window;

Console console;

bool Core::corerunning;
bool Core::devMode;

int width = _WINDOW_WIDTH;
int height = _WINDOW_HEIGHT;

static int resizingEventWatcher(void* data, SDL_Event* event) {
    if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_RESIZED) {
        SDL_Window* window = SDL_GetWindowFromID(event->window.windowID);
        if (window == (SDL_Window*)data) {
            // Resizing...
        }
    }
    return 0;
}

void CoreLoop() {
    float elapsed = 0.0f;
    float elapsedMS = 0.0f;
    Uint64 startF = SDL_GetPerformanceCounter();
    Uint32 engineticka = SDL_GetTicks();
    Uint32 enginetickb;
    
    if ((engineticka - enginetickb) > 1000/60.0f) {
        //cout << "fps: " << 1000 / (engineticka - enginetickb) << endl;
        core->Event(window);
        
        core->Update();
        core->Render(1000/(engineticka - enginetickb));
        
        enginetickb = engineticka;
    }
    
    Uint64 endF = SDL_GetPerformanceCounter();
    elapsed = (endF-startF) / (float)SDL_GetPerformanceFrequency();
    elapsedMS = (endF-startF) / (float)SDL_GetPerformanceFrequency() * 1000.0f;

    SDL_GL_SwapWindow(window);
}

void CoreInit() {
    console.Println("Starting...");
    SDL_Delay(1000);
    core->StartInit();
    
#ifndef EMSCRIPTEN
    while (Core::corerunning) {
        CoreLoop();
    }
#else
    console.Println("Incogine Running in Web platform");
    emscripten_set_main_loop(CoreLoop, 0, 1);
#endif
}

int main(int argc, char* argv[]) {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    system("cls");
#elif __APPLE__
    system("clear");
#endif

    console.Println("Incogine by leafstudiosDot");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 8 );
    
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        //console.Println("Init Core Error " + SDL_GetError());
        cout << "Init Core Error " << SDL_GetError() << endl;
        return -1;
    }
    
    if (IMG_Init(IMG_INIT_PNG) < 0) {
        cout << "Init Image Error " << IMG_GetError() << endl;
        return -1;
    }
    
    if(TTF_Init() < -1) {
        cout << "TTF Error " << TTF_GetError() << endl;
        return -1;
    }

    int windowWidth;
    int windowHeight;
    
    console.Println("Initializing Core...");
    core = new Core();
        
    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
        windowWidth = width*1.5;
        windowHeight = height*1.5;
    #else
        windowWidth = width;
        windowHeight = height;
    #endif

    int flags = SDL_WINDOW_ALLOW_HIGHDPI + SDL_WINDOW_SHOWN + SDL_WINDOW_RESIZABLE;
    #if __APPLE__
        //flags = flags + SDL_WINDOW_METAL; // uncomment later when Apple is accepting OpenGL and Metal is friendship.
        flags = flags + SDL_WINDOW_OPENGL; // uncomment later when Apple is accepting OpenGL and Metal is friendship.
    #else
        flags = flags + SDL_WINDOW_OPENGL;
    #endif

    window = SDL_CreateWindow(_WINDOW_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, flags);
    SDL_AddEventWatch(resizingEventWatcher, window);
    
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "-dev") == 0) {
            Core::devMode = true;
            cout << "Development Mode enabled" << endl;
        } else {
            Core::devMode = false;
        }
    }
    
    Core::corerunning = true;

    SDL_GLContext context;
    context = SDL_GL_CreateContext(window);
    if (SDL_GL_SetSwapInterval(1) < 0)
    {
        printf("Warning: Unable to enable VSync: %s\n", SDL_GetError());
    }

    #ifndef __EMSCRIPTEN__
    glewExperimental = GL_TRUE;
    #endif
    //GLenum glew_init_error = glewInit();

    //gladLoadGLLoader(SDL_GL_GetProcAddress);
    CoreInit();
    
    core->Destroy();
    
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    delete core;

    return 0;
}
