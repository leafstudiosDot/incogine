#include <iostream>
#include <stdint.h>
#include <stdio.h>

#include "misc/console/console.h"
Console console;

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_image.h>

#if __APPLE__
    #include <OpenGL/gl.h>
    #include <OpenGL/glu.h>
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    #include <GL/GL.h>
    #include <GL/GLU.h>
    //#include <GL/glut.h>
#else
    #include <GL/gl.h>
    #include <GL/glu.h>
#endif

#define _WINDOW_NAME "leafstudiosDot"

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
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 8 );
    SDL_Window* window = nullptr;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        //console.Println("Init Core Error " + SDL_GetError());
        cout << "Init Core Error " << SDL_GetError() << endl;
        return -1;
    }

    if (IMG_Init(IMG_INIT_PNG) < 0) {
        cout << "Init Image Error " << IMG_GetError() << endl;
        return -1;
    }

    int width = 1280;
    int height = 720;
    int windowWidth;
    int windowHeight;
        
    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
        windowWidth = width*1.5;
        windowHeight = height*1.5;
    #else
        windowWidth = width;
        windowHeight = height;
    #endif

    int flags = SDL_WINDOW_ALLOW_HIGHDPI + SDL_WINDOW_RESIZABLE + SDL_WINDOW_SHOWN;
    #if __APPLE__
        //flags = flags + SDL_WINDOW_METAL; // uncomment later when Apple is accepting OpenGL and Metal is friendship.
        flags = flags + SDL_WINDOW_OPENGL; // uncomment later when Apple is accepting OpenGL and Metal is friendship.
    #else
        flags = flags + SDL_WINDOW_OPENGL;
    #endif

    window = SDL_CreateWindow(_WINDOW_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, flags);
    bool running = true;

    SDL_GLContext context;
    context = SDL_GL_CreateContext(window);

    //gladLoadGLLoader(SDL_GL_GetProcAddress);

    /* Incogine Logo */


    while (running) {
        glViewport(0, 0, windowWidth, windowHeight);
        SDL_GL_SetSwapInterval(-1);

        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            if(event.type == SDL_QUIT) {
                running = false;
            }

            const Uint8* state = SDL_GetKeyboardState(NULL);
            if (state[SDL_SCANCODE_RIGHT]) {
                console.Println("Right key pressed!");
            }
        }

        glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

        /* Render GL Here */

        SDL_GL_SwapWindow(window);
    }
    
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);

    IMG_Quit();
    SDL_Quit();

    return 0;
}
