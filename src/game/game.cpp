//
//  game.cpp
//  Incogine
//
//  Created by moritz on 10/17/22.
//

#include "game.hpp"

TTF_Font *logofont;
TTF_Font *font;
int p_windowWidth;
int p_windowHeight;

Game::Game() {
    // Initialize Game
    Console console;
    console.Println("Game initialized successfully");
}

Game::~Game() {
    // Purge Game
    Console console;
    console.Println("Game purged successfully");
}

bool is3D = false;

const Uint8 *_Pkeyboard = SDL_GetKeyboardState(0);

float hudsx = 2.9f, hudsy = 0.5f;

void Game::RawEvent(SDL_Event event, int _windowWidth, int _windowHeight) {
    // Events (Keyboard, Mouse, etc.)
    Console console;
    if (!is3D) {
        // 2D Controls
        // Type-Style KeyDown (Hold Key)
        /*if (_Pkeyboard[SDL_SCANCODE_UP]) {
            hudy -= 5;
        } else {
            
        }*/
        if (_Pkeyboard[SDL_SCANCODE_W]) {
            hudsx += 0.1f;
        } else {
            
        }
        if (_Pkeyboard[SDL_SCANCODE_S]) {
            hudsx -= 0.1f;
        } else {
            
        }
        if (_Pkeyboard[SDL_SCANCODE_A]) {
            hudsy -= 0.1f;
        } else {
            
        }
        if (_Pkeyboard[SDL_SCANCODE_D]) {
            hudsy += 0.1f;
        } else {
            
        }
    } else {
        // Disable if using 2D
        /*if (_Pkeyboard[SDL_SCANCODE_UP]) {
            hudy -= 5;
        } else {
            
        }*/
    }
}

void Game::Event(SDL_Event event, int _windowWidth, int _windowHeight) {
    Console console;
    
    // Quit
    if(event.type == SDL_QUIT) {
        Core::corerunning = false;
    }
    
    // KeyUp and KeyDown
    // Pressed
    if(event.type == SDL_KEYDOWN) {
        switch(event.key.keysym.sym){
        /*case SDLK_LEFT:
            //console.Println("Left Key Pressed");
            break;
        case SDLK_r:
            //console.Println("Engine reset successfully");
            break;*/
            case SDLK_c:
                cout << "HUDSX: " << hudsx << endl;
                cout << "HUDSY: " << hudsy << endl;
                break;
        }
    }
    // Released
    if(event.type == SDL_KEYUP) {
        switch(event.key.keysym.sym) {
            case SDLK_LEFT:
                //console.Println("Left Key Lifted");
                break;
        }
    }
    
    // Mouse
    if (event.type == SDL_MOUSEMOTION) {
        int mouseposx = event.motion.xrel, mouseposy = event.motion.yrel;
        if (is3D) {
            /*if (mouseposx < 1) {
                angle -= mouseposx * (camsensitivity - 0.04f);
                camx = sin(angle);
                camy = -cos(angle);
            } else {
                angle += (mouseposx * (camsensitivity - 0.04f))*(-1);
                camx = sin(angle);
                camy = -cos(angle);
            }*/
        }
    }
}

void Game::Start(int _windowWidth, int _windowHeight) {
    // Executes as game launches
    Console console;
    #if __APPLE__
    const char fontFile1[] = "../Resources/fonts/def_font.ttf";
    const char fontFile2[] = "../Resources/fonts/arlrdbd.ttf";
    #elif EMSCRIPTEN
    const char fontFile1[] = "/assets/fonts/def_font.ttf";
    const char fontFile2[] = "/assets/fonts/arlrdbd.ttf";
    #endif
    
    if(!(font = TTF_OpenFont(fontFile1, 100))) {
        printf("Error loading font: %s", TTF_GetError());
    }
    if(!(logofont = TTF_OpenFont(fontFile2, 100))) {
        printf("Error loading font: %s", TTF_GetError());
    }
    
    glDepthFunc(GL_LESS);
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    
    //camx = sin(angle);
    //camy = -cos(angle);
    
    if (is3D) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }
}

void Game::Update(int _windowWidth, int _windowHeight) {
    // Executes every frame at game
    p_windowWidth = _windowWidth;
    p_windowHeight = _windowHeight;
}

Fonts *lsDotLogo;
SDL_Color _lsDotLogo_color;

void Game::Render(int _windowWidth, int _windowHeight) {
    // Render Game
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    if (is3D) {
        // Use in 3D Camera Movement (First Person)
        //gluLookAt(playerx, 0.0f, (playery*(-1)), playerx+camx, 0.0f, (playery*(-1))+camy, 0, 1, 0);
    } else {
        // Use in 2D Camera Movement (Top Down)
        gluLookAt((0.0f*(-1)), (0.0f*(-1)), 0.0f, (0.0f*(-1)), (0.0f*(-1)), -100, 0, 1, 0);
    }
    
    glPushMatrix();
    glTranslated(-5.1f, 0.0f, -10.0f);
    _lsDotLogo_color.r = 255;
    _lsDotLogo_color.g = 255;
    _lsDotLogo_color.b = 255;
    _lsDotLogo_color.a = 255;
    lsDotLogo->RenderFont(logofont, "leafstudiosDot", 5.0f, 0, 0, _lsDotLogo_color, 3.0f, 0.5f);
    glPopMatrix();
}

void Game::RenderCanvas(int _windowWidth, int _windowHeight, bool devMode) {
    // Render HUD
    /*glPushMatrix();
    glTranslatef(hudx, hudy, 0);
    glBegin(GL_QUADS);
        glColor3f(1.0f, 0.0f, 0.0);
        glVertex2f(0.0, 0.0);
        glVertex2f(10.0, 0.0);
        glVertex2f(10.0, 10.0);
        glVertex2f(0.0, 10.0);
    glEnd();
    glPopMatrix();
    
    glPushMatrix();
    glTranslated(hudx, hudy, 0);
    _whatsoeva_hud_color.r = 255;
    _whatsoeva_hud_color.g = 255;
    _whatsoeva_hud_color.b = 255;
    _whatsoeva_hud_color.a = 100;
    whatsoeva_hud->RenderFontHUD(font, "Hud Font", 40.0f, 90.0f, 0.0f, _whatsoeva_hud_color, 230.0f, -70.0f, hudx);
    glPopMatrix();*/
}

void Game::Destroy() {
    // Destroy Game
    TTF_CloseFont(font);
}
