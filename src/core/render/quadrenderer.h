#ifndef QUAD_RENDERER_H
#define QUAD_RENDERER_H

#include "glloader.h"

// Minimal shader-based 2D quad renderer used by the engine's UI elements.
// Replaces the old fixed-function (glBegin/glEnd) pipeline so the engine
// runs on both desktop OpenGL 3.3 core and OpenGL ES 3.0 (Android/iOS/Web).
class QuadRenderer {
    public:
        static bool Init(SDL_Window* window, SDL_GLContext context);
        static void Shutdown();

        static void SetViewport(int width, int height);

        // Draws a quad in window pixel coordinates (origin top-left).
        // Pass a valid GL texture to draw it textured (white 1x1 texture
        // otherwise). UV coordinates default to the full texture.
        static void DrawQuad(float x, float y, float w, float h,
                             float r, float g, float b, float a,
                             GLuint texture = 0,
                             float uvx0 = 0.0f, float uvy0 = 0.0f,
                             float uvx1 = 1.0f, float uvy1 = 1.0f);

    private:
        static bool CreateShaders();
        static bool CreateWhiteTexture();

        static bool sInitialized;
        static bool sIsES;
        static GLuint sProgram;
        static GLuint sVAO;
        static GLuint sVBO;
        static GLuint sWhiteTexture;
        static GLint sUniformProj;
        static GLint sUniformColor;
        static GLint sUniformTexture;
        static float sProj[16];
};

#endif
