#ifndef CORE_hpp
#define CORE_hpp

#define GLEW_STATIC
#include <glew/glew.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_image.h>
#include <SDL2_ttf/SDL_ttf.h>

#if __APPLE__
    #include <OpenGL/gl.h>
    #include <OpenGL/glu.h>
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    #include <gl/GL.h>
    #include <gl/GLU.h>
    //#include <GL/glut.h>
#elif __EMSCRIPTEN__
    #include <OpenGL/gl.h>
    #include <OpenGL/glu.h>
    #include <emscripten.h>
#else
    #include <GL/gl.h>
    #include <GL/glu.h>
#endif

#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <string>

#include "misc/console/console.h"
#include "objects/fonts.hpp"

#include "../game/game.hpp"

#define _WINDOW_WIDTH 1280
#define _WINDOW_HEIGHT 720

#define _COMPANY_NAME "leafstudiosDot"
#define _PROJ_NAME "Incogine"

#define _WINDOW_NAME "leafstudiosDot"

class Core {
public:
    Core();
    ~Core();
    
    void StartInit();
    void Event(SDL_Window* window);
    void Update();
    void Render();
    void Destroy();
    
    static bool corerunning;
    char *prefpath;
private:
    int SceneIndex;
    int Frame;
    int timeSeconds;
    int _windowWidth;
    int _windowHeight;
    void InitSysPrefPath();
};

#endif
