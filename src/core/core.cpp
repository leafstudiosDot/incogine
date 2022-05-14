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
        
        switch(event.type) {
            
        }

        const Uint8* state = SDL_GetKeyboardState(NULL);
        if (state[SDL_SCANCODE_RIGHT]) {
            console.Println("Right key pressed!");
        }
    }
}

void Core::Update() {
    /* Update at Run every frame. */
    Frame++;
    
    // Calculating Seconds
    /*if ((Frame % 60) == 0) {
        std::cout << Frame << "\r" << std::endl;
    }*/
}

void Core::Render() {
    /*  Render Codes. cleaner codes. */
}
