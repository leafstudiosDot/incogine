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
#include <sstream>
#include <vector>
#include <cmath>

#include "misc/console/console.h"
#include "objects/fonts.hpp"
#include "objects/entity.hpp"
#include "objects/components/component.hpp"

#include "../game/game.hpp"

#define _WINDOW_WIDTH 1280
#define _WINDOW_HEIGHT 720

#define _COMPANY_NAME "leafstudiosDot"
#define _PROJ_NAME "Incogine"

#define _WINDOW_NAME "Incogine"

class Core {
public:
    Core();
    ~Core();
    
    void StartInit();
    void Event(SDL_Window* window);
    void Update();
    void Render(float fps);
    void Destroy();
    
    static bool corerunning;
    static bool devMode;
    char *prefpath;
    void showPerfs(bool enabled, float fps);
private:
    int SceneIndex;
    int Frame;
    int timeSeconds;
    int _windowWidth;
    int _windowHeight;
    void InitSysPrefPath();
    int FramePerSecondClock;
    float FramePerSecond;
};

#endif
