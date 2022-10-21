#include "core.hpp"

Game *game;

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
    
    game->Start();
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
        
        game->Event(event);
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

void Core::Render() {
    /*  Render Codes. cleaner codes. */
    
    /*glShadeModel(GL_SMOOTH);
    glClearColor(1.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0f);
    glDepthFunc(GL_LEQUAL);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);*/
    
    // Display
    /*glViewport(0, 0, _windowWidth*2, _windowHeight*2);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, _windowWidth, _windowHeight, 0, -10, 10);*/
    
    glViewport(0, 0, _windowWidth*2, _windowHeight*2);

    glClearColor(0.0f, 0.0f, 1.0f, 0.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Game
    game->Render();
}

void Core::Destroy() {
    game->Destroy();
}
