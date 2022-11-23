#include "core.hpp"

Game *game;

const char* __fontFile1;
Fonts *__lsDotLogo;

Core::Core() {
    Console console;
    console.Println("Core initialized successfully");
}

Core::~Core() {
    Console console;
    delete game;
    console.Println("Core purged successfully");
}

void Core::StartInit() {
    /* Start at Run once. */
    _windowWidth = _WINDOW_WIDTH;
    _windowHeight = _WINDOW_HEIGHT;
    
    glDepthFunc(GL_LESS);
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    InitSysPrefPath();
    
    #if __APPLE__
    __fontFile1 = "../Resources/fonts/arlrdbd.ttf";
    #elif EMSCRIPTEN
    __fontFile1 = "/assets/fonts/arlrdbd.ttf";
    #endif

    __lsDotLogo = new Fonts(__fontFile1);
}

void Core::Event(SDL_Window* window) {
    Console console;
    SDL_Event event;
    
    while(SDL_PollEvent(&event)) {
        if(event.type == SDL_WINDOWEVENT) {
            // Window
            switch(event.window.event)
            {
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    _windowWidth = event.window.data1;
                    _windowHeight = event.window.data2;
                    break;
            }
        }
        
        game->Event(event, _windowWidth, _windowHeight);
    }
    
    game->RawEvent(event, _windowWidth, _windowHeight);
}

void Core::Update() {
    /* Update at Run every frame. */
    Frame++;
    
    // Calculating Seconds
    if ((Frame % 60) == 0) {
        timeSeconds++;
        //std::cout << Frame << "\r" << std::endl;
    }
    game->Update(_windowWidth, _windowHeight);
}

SDL_Color __lsDotLogo_color;

void Core::Render() {
    /*  Render Codes. cleaner codes. */
    
    glViewport(0, 0, _windowWidth*2, _windowHeight*2);
    //glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(70.0f, _windowWidth / (float)_windowHeight, 0.1f, 300.0f);
    
    // Game
    if (Frame < 130) {
        gluLookAt((0.0f*(-1)), (0.0f*(-1)), 0.0f, (0.0f*(-1)), (0.0f*(-1)), -100, 0, 1, 0);
        glPushMatrix();
        glTranslated(-5.1f, 0.0f, -10.0f);
        __lsDotLogo_color.r = 255;
        __lsDotLogo_color.g = 255;
        __lsDotLogo_color.b = 255;
        __lsDotLogo_color.a = 255;
        __lsDotLogo->RenderFont("leafstudiosDot", 5.0f, 0, 0, __lsDotLogo_color, 3.0f, 0.5f);
        glPopMatrix();
    } else if (Frame >= 130 && Frame < 280) {
        gluLookAt((0.0f*(-1)), (0.0f*(-1)), 0.0f, (0.0f*(-1)), (0.0f*(-1)), -100, 0, 1, 0);
        glPushMatrix();
        glTranslated(-5.1f, 0.0f, -10.0f);
        __lsDotLogo_color.r = 255;
        __lsDotLogo_color.g = 255;
        __lsDotLogo_color.b = 255;
        __lsDotLogo_color.a = 255;
        __lsDotLogo->RenderFont("Powered by Incogine", 5.0f, 0, 0, __lsDotLogo_color, 2.6f, 0.3f);
        glPopMatrix();
    } else if (Frame >= 280) {
        // New Scene
        if (Frame == 280) {
            game->Start(_windowWidth, _windowHeight);
        }
        
        game->Render(_windowWidth, _windowHeight);
    }
    
    // HUD
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, _windowWidth, _windowHeight, 0, -10, 10);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_CULL_FACE);
    glClear(GL_DEPTH_BUFFER_BIT);
    if (Frame >= 280) {
        // New Game Render Canvas
        game->RenderCanvas(_windowWidth, _windowHeight, Core::devMode);
    }
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void Core::Destroy() {
    if (Frame >= 280) {
        game->Destroy();
    }
}

char *prefpath = NULL;

void Core::InitSysPrefPath() {
    
    /*
        Windows:
        C:\Users\${Username}\AppData\Roaming\${_COMPANY_NAME}\${_PROJ_NAME}\

        Linux:
        /home/${Username}/.local/share/${_PROJ_NAME}/

        macOS:
        /Users/${Username}/Library/Application Support/${_PROJ_NAME}/
    */
    
    char *base_path = SDL_GetPrefPath(_COMPANY_NAME, _PROJ_NAME);
    if (base_path) {
        prefpath = base_path;
    } else {
        cout << "Cannot write in preferences path, Saving and Loading disabled!" << endl;
    }
}
