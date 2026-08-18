#ifndef GLLOADER_H
#define GLLOADER_H

#include "glad/include/glad/gl.h"
#include <SDL3/SDL_video.h>

// Loads all OpenGL functions for the current context (desktop GL 3.3 core
// or OpenGL ES 3.0) and reports which one is active.
bool icgGLInit(SDL_Window* window, SDL_GLContext context);
void icgGLShutdown();
bool icgGLIsES();

#endif
