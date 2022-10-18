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
    _windowWidth = _WINDOW_WIDTH;
    _windowHeight = _WINDOW_HEIGHT;
}

float playerx = 0, playery = 0, speed = 5;
const Uint8 *_Pkeyboard = SDL_GetKeyboardState(0);

void Core::Event(SDL_Window* window) {
    Console console;
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        // Quit
        if(event.type == SDL_QUIT) {
            corerunning = false;
        }
        
        if(event.type == SDL_WINDOWEVENT) {
            switch(event.window.event)
            {
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    _windowWidth = event.window.data1;
                    _windowHeight = event.window.data2;
                    break;
            }
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
    
    // Display
    glViewport(0, 0, _windowWidth*2, _windowHeight*2);
    glOrtho(0, _windowWidth, _windowHeight, 0, -10, 10);
    
    // Game
    glClearColor(0.0f, 0.2f, 0.1f, 1.0f);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
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
