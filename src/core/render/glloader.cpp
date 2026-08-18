#include "glloader.h"
#include <SDL3/SDL.h>

namespace {
    bool gIsES = false;

    GLADapiproc loadGLProc(const char* name) {
        return reinterpret_cast<GLADapiproc>(SDL_GL_GetProcAddress(name));
    }
}

bool icgGLInit(SDL_Window* window, SDL_GLContext context) {
    (void)window;
    (void)context;

    bool loaded = false;

#if defined(__ANDROID__) || defined(__EMSCRIPTEN__) || (defined(__APPLE__) && defined(__IPHONEOS__))
    loaded = gladLoadGLES2(loadGLProc) != 0;
#else
    loaded = gladLoadGL(loadGLProc) != 0;
    if (loaded) {
        const char* version = reinterpret_cast<const char*>(glad_glGetString(GL_VERSION));
        if (version && SDL_strstr(version, "OpenGL ES") != nullptr) {
            loaded = gladLoadGLES2(loadGLProc) != 0;
        }
    }
#endif

    if (!loaded) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "icgGLInit: failed to load OpenGL functions: %s", SDL_GetError());
        return false;
    }

    const char* version = reinterpret_cast<const char*>(glad_glGetString(GL_VERSION));
    gIsES = version != nullptr && SDL_strstr(version, "OpenGL ES") != nullptr;

    return true;
}

void icgGLShutdown() {
    gIsES = false;
}

bool icgGLIsES() {
    return gIsES;
}
