#include "core.hpp"

Game game;

Core::Core() {
    Console console;
    console.Println("Core initialized successfully");
}

Core::~Core() {
    Console console;
    console.Println("Core purged successfully");
}

void Core::StartInit() {
    /* Start at Run once. */
    _windowWidth = _WINDOW_WIDTH;
    _windowHeight = _WINDOW_HEIGHT;
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
        
        game.Event(event);
    }
    
    game.RawEvent(event, _windowWidth, _windowHeight);
}

void Core::Update() {
    /* Update at Run every frame. */
    Frame++;
    
    // Calculating Seconds
    if ((Frame % 60) == 0) {
        timeSeconds++;
        //std::cout << Frame << "\r" << std::endl;
    }
    game.Update();
}

void Core::Render() {
    /*  Render Codes. cleaner codes. */
    
    // Display
    glViewport(0, 0, _windowWidth*2, _windowHeight*2);
    glOrtho(0, _windowWidth, _windowHeight, 0, -10, 10);
    
    // Game
    game.Render();
}
