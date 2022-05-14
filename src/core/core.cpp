#include "core.hpp"

Core::Core() {
    Console console;
    console.Println("Core initialized successfully");
}

Core::~Core() {
    
}

void Core::StartInit() {
    /* Start at Run once. */
}

void Core::Event() {
    Console console;
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        if(event.type == SDL_QUIT) {
            corerunning = false;
        }
        
        // KeyUp and KeyDown
        // Pressed
        if(event.type == SDL_KEYDOWN) {
            switch(event.key.keysym.sym){
            case SDLK_LEFT:
                console.Println("Left Key Pressed");
                break;
            }
        }
        // Released
        if(event.type == SDL_KEYUP) {
            switch(event.key.keysym.sym) {
                case SDLK_LEFT:
                    console.Println("Left Key Lifted");
                    break;
            }
        }
        
        // Raw KeyDown (Hold Key)
        /*const Uint8* stateinp = SDL_GetKeyboardState(NULL);
        if (stateinp[SDL_SCANCODE_RIGHT]) {
            // Pressed
            console.Println("Right key pressed!");
        } else {
            // Released
        }*/
    }
}

void Core::Update() {
    /* Update at Run every frame. */
    Frame++;
    
    // Calculating Seconds
    if ((Frame % 60) == 0) {
        timeSeconds++;
        //std::cout << Frame << "\r" << std::endl;
    }
}

void Core::Render() {
    /*  Render Codes. cleaner codes. */
}
