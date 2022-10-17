#ifndef CORE_hpp
#define CORE_hpp

#define GLEW_STATIC
#include <glew/glew.h>

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

#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <string>

#include "misc/console/console.h"

#define _WINDOW_WIDTH 1280
#define _WINDOW_HEIGHT 720

class Core {
public:
    Core();
    ~Core();
    
    void StartInit();
    void Event(SDL_Window* window);
    void Update();
    void Render();
    
    static bool corerunning;
    
private:
    int SceneIndex;
    int Frame;
    int timeSeconds;
    int _windowWidth;
    int _windowHeight;
};

#endif
