#include "core.hpp"

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
}

float playerx = 0, playery = 0, speed = 5;
const Uint8 *_Pkeyboard = SDL_GetKeyboardState(0);

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
    }
    
    //Raw KeyDown (Hold Key)
    if (_Pkeyboard[SDL_SCANCODE_D]) {
        // Pressed
        playerx += speed;
    } else {
        // Released
    }
    
    if (_Pkeyboard[SDL_SCANCODE_A]) {
       // Pressed
        playerx -= speed;
    } else {
        // Released
    }

    if (_Pkeyboard[SDL_SCANCODE_W]) {
        // Pressed
        playery -= speed;
    } else {
        // Released
    }
    
    if (_Pkeyboard[SDL_SCANCODE_S]) {
        // Pressed
        playery += speed;
    } else {
        // Released
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
    glPushMatrix();
    glTranslated(playerx, playery, 0);
    glBegin(GL_QUADS);
        glColor3ub(255, 0, 0);
        glVertex2f(0, 0);
        glVertex2f(100, 0);
        glVertex2f(100, 100);
        glVertex2f(0, 100);
    glEnd();
    glPopMatrix();
}
