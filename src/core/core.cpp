#include "core.hpp"

Game *game;

const char* __fontFile1;
Fonts *__lsDotLogo;
SDL_Color white;
Fonts *perfWidget;
SDL_Color perffpscolor;
Fonts *__lsDotLogo2;

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
    __lsDotLogo2 = new Fonts(__fontFile1);
    perfWidget = new Fonts(__fontFile1);
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

void Core::Render(float fps) {
    /*  Render Codes. cleaner codes. */
    
    white.r = 255;
    white.g = 255;
    white.b = 255;
    white.a = 255;
    
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
        __lsDotLogo->RenderFont("leafstudiosDot", 5.0f, 0, 0, white, 3.0f, 0.5f);
        glPopMatrix();
    } else if (Frame >= 130 && Frame < 280) {
        gluLookAt((0.0f*(-1)), (0.0f*(-1)), 0.0f, (0.0f*(-1)), (0.0f*(-1)), -100, 0, 1, 0);
        glPushMatrix();
        glTranslated(-5.1f, 0.0f, -10.0f);
        __lsDotLogo->RenderFont("Powered by Incogine", 5.0f, 0, 0, white, 2.6f, 0.3f);
        glPopMatrix();
        
        glPushMatrix();
        //cout << _windowHeight << endl;
        glTranslated(-5.1f, ((-1)*(_windowHeight)/_windowWidth)-5.5f, -10.0f);
        char copyright = 0xA9;
        string notice1 = "";
        
        std::stringstream ss;
        ss << copyright;
        ss >> notice1;
        string fullnotice = ss.str() + " 2022 leafstudiosDot";
        
        __lsDotLogo2->RenderFont(fullnotice.c_str(), 5.0f, 0, 0, white, 2.7f, 0.3f);
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
    showPerfs(true, fps);
    if (Frame >= 280) {
        // New Game Render Canvas
        game->RenderCanvas(_windowWidth, _windowHeight, Core::devMode);
    }
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void Core::Destroy() {
    delete __lsDotLogo;
    delete __lsDotLogo2;
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

void Core::showPerfs(bool enabled, float fps) {
    if (enabled) {
        if (FramePerSecondClock >= 15) {
            //cout << "FPS Changed: " << fps << endl;
            FramePerSecond = fps;
            FramePerSecondClock = 0;
        } else {
            FramePerSecondClock++;
        }
        
        std::stringstream ss;
        ss << FramePerSecond;
        std::string str = ss.str();
        
        char* fps_char_arr;
        string fps_str_obj(str + " fps");
        fps_char_arr = &fps_str_obj[0];
        
        perffpscolor.r = 255;
        perffpscolor.g = 255;
        perffpscolor.b = 255;
        perffpscolor.a = 50;
        perfWidget->RenderFontHUD(fps_char_arr, _windowWidth - 45, 15, 0, perffpscolor, 27.0f, -10.0f, 0);
    }
}
